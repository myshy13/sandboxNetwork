# Plans

## Plan: water flow (written Sun 27 Sep 2026)

### Why

Water blocks exist (`BlockType::Water`), the player can swim in them, and they render transparent. Right
now every water cell placed at world-gen is permanent and still — the next step is for water to spread and
settle on its own, the way a broken dam or a placed source block would.

### Status

Built: `level` on `Object`, `UpdateWaterLevel`, `FluidSim` (`Server/src/Fluid/`, tested in
`Server/test_fluid.cpp`), ticked from `Server::tick` every `env::FLOW_INTERVAL`, and water drawn by level.
Earlier groundwork (water `BlockType`, swim physics, two-pass translucent rendering, underwater tint) is in git history.

### Requirements

Each rule below should have a test in `Server/test_fluid.cpp` (flow) or be visible in game (rendering).

**Levels.** `level` 0 is a source; 1..`SHARED_WATER_MAX_LEVEL` (`Shared/sharedEnv.hpp`, now 3) is flowing water,
weaker as the number grows. Past the max, water is removed. The server owns every level; clients only draw them.

**Flow, one pass every `FLOW_INTERVAL` (only cells in `activeCells` are looked at):**

1. *Fall:* water with an empty cell below it (and y >= 0) fills that cell, at its own level but never 0.
   Falling copies strength down; it never creates a source.
2. *Feed:* a non-source cell's level becomes the strongest level a neighbour gives it: side water at level n gives
   n + 1, water directly above gives its own level (at least 1). Feeding can strengthen as well as weaken a cell.
3. *Drain:* a non-source cell with no feeder weakens by 1 per pass and is removed past the max. It does not spread.
4. *Spread:* a source, or fed flowing water standing on something solid (or the world floor), fills each empty
   side neighbour at its level + 1, if that is within the max. Flowing water over air or water doesn't spread
   sideways, so a waterfall falls straight down and spreads where it lands.
5. *Stop:* water never replaces or passes a solid block, never goes below y = 0, and never enters a chunk the
   server hasn't generated yet.
6. *Pacing:* a cell woken during a pass is processed next pass, never in the same one, so flow moves exactly one
   cell per pass in every direction.

**What wakes a cell:** a change to its own level, a level change or removal of the water that feeds it, a block
broken beside or below it, and loading its chunk. On load only surface water (no water above it) is woken;
water under a surface is already settled.

**Generation:** the top water block of each generated column is a source; the water under it is level 1.

**Networking:** every level change is sent as `UpdateWaterLevel`, every new cell as `NewObject`, every removal as
`RemoveObject`, all reliable and only to players holding that chunk. Clients apply world events in arrival order.

**Rendering (client):**

- A water surface sits lower the weaker it is (a source sits just under a full block). Water with water above
  it is drawn full height, so a falling column reads as one stream.
- A water face touching the same body (above, below, or a side neighbour at the same height or higher) is not
  drawn. A side face next to lower water is drawn only above that water's surface, so there are no gaps and
  no doubled-up translucent faces.
- A level change re-meshes the chunks around it.

### Pitfalls

- **Flood fill cost.** An unbounded pass over "every water cell, every tick" gets expensive fast on a big
  lake. Keep a dirty/active set (cells that changed last pass, plus their neighbours) instead of scanning
  every water block in the world each tick.
- **Infinite water.** Every source cell spreads forever unless spread has a max level/distance — cap it
  (the Minecraft `0..7` convention exists for exactly this).
- **Orphaned objects.** `Server::indexBlock` overwrites `occupiedCells[key]` without checking for an existing
  occupant there (`chunkBlocks` isn't cleaned up either) — this bit the water-table floor placement earlier
  in this session (see git history). Flow edits that reuse a cell must remove the old block first, not just
  add a new one on top.

---

## Plan: real world streaming (written Sun 20 Sep 2026)

### Why

`WORLD_SIZE` 200 -> 500 made the world 1000x1000 columns: 2.2M blocks at first, 4.6M once the heights became
`rand() % 20` (was 160,000+ at size 200).
Everything that touches "all blocks" got ~6x worse, and it grows with the square of the world size:

- **Join:** the server sends every block to every joining client, and the client keeps all of them in RAM.
- **Server save:** copies and writes every block (now on a background thread, but still the whole world).
- **Client scans:** `damageObject` / `removeObject` search all blocks by id; the renderer walks every chunk.
- **Startup:** `generateWorld` builds the whole map (random + 7 smoothing passes over all of it).

Streaming means cost follows what is near each player, not how big the world is.

### Target behaviour

- Each client only holds chunks within its view radius; the server only sends those.
- The world can be effectively unbounded (chunks are generated the first time someone needs them).
- Join time and RAM stay flat however big the world is.
- The render-distance slider finally means something for bandwidth and memory, not just drawing.

### Decisions to make first (recommendations in bold)

1. **Address blocks by cell (x, y, z), not by id.** Ids come from a counter that has to be saved,
   and terrain that is generated per chunk has no natural id. Cell addressing also removes the client's
   O(N) `removeObject` / `damageObject` scans. **Do this. It is a protocol change: bump `PROTOCOL_VERSION`.**
2. **Chunk = 16x16 cells wide (80x80 world units), full height.** This is separate from the renderer's
   3-cell chunks, which stay as they are (they live inside these).
3. **(Later, steps 7-8)** **Terrain from a deterministic noise function of (seed, x, z), not the global heightMap + smoothing.**
   The smoothing needs the whole map, so it can't be evaluated per chunk. Use integer-hash value noise
   (2-3 octaves), server-side only (`Server/src/Server/terrain.hpp`, tested like `chunk.hpp`). Store the seed in the save,
   or a restart would generate different terrain next to saved chunks.
4. **(Later, steps 7-8)** **Persist only changed chunks** (dirty flag per chunk). A chunk loads as: saved file if it exists,
   otherwise generate it. Reuse the background-save idea from `Server::saveWorldAsync`.
5. **Bullets:** speed 500 u/s and 20 s lifetime means up to 10 km of travel. **Bullets die on leaving the
   loaded area** for now; generating chunks on demand for bullets can come later.

### Steps (each one should leave the game working)

0. **Done:** background save + dirty flag (`saveWorldAsync`), `placeBlock` uses `occupiedCells`,
   and the `reserve()` fix in `World::addObjects` (see the baseline below).
1. **Baseline (done).** All at `WORLD_SIZE = 500`:
   - **Before the fix:** 2,246,270 blocks, 80.9 MB on the wire (~36 bytes/block). Server generate 0.21 s, server
     pack + queue 216 ms, but the client's last chunk arrived at **45 s**, the client froze, and its memory
     climbed past 7.5 GB. Server: ~300 MB.
   - **Cause:** `World::addObjects` called `objects.reserve(size + n)` for every chunk. `reserve` allocates exactly
     that much, so every chunk copied the whole vector (quadratic), all on the game thread. My first guess
     (ENet window) was wrong: the server side was fast all along.
   - **After removing the `reserve`:** 4,639,364 blocks (the world has more blocks now: `rand() % 20`),
     last chunk at **11.31 s**, client memory under 1 GB. Froze during load: not recorded yet.
   - So the world still costs O(world size) per join (RAM, time, freeze); that is what streaming removes.
   - **Target after streaming** (radius 6 chunks ~ 169 chunks ~ 3.5 MB): a couple of seconds, flat as the world grows.
2. **Server chunk index (done, no behaviour change).** Keep `objects` and `occupiedCells` as they are. Add
   `unordered_map<int64_t, vector<int>>` from chunk key to indices into `objects`, updated in `addBlock` and
   `removeBlock` (including the swap-and-pop fix-up; mirror `World::indexObject` / `removeObject` on the client).
   Chunk = 16x16 cells (80 units). Check: print chunk count and average blocks per chunk at startup.
3. **Protocol (done: 3a messages + tests, 3b position-addressed edits; `PROTOCOL_VERSION` 4).** (`Shared/Protocol/protocol.hpp`): `ChunkData` (chunk key + blocks), `ChunkUnload` (key),
   `SetViewRadius` (client -> server, in chunks), and cell-addressed Place/Remove/Damage. Bump `PROTOCOL_VERSION`.
   Keep the old `initBlocks` path working until step 4 replaces it.
   Edits are addressed by the block's **centre position** (`Vector3`), not cell integers: the server floors and the
   client rounds, so each side turns the position into its own key and they never have to agree.
4. **Server: interest management.** Done in two halves around step 5 so the game works after every commit:
   **4a (done)** = views, load/unload, `SetViewRadius`, `sendChunk` (keep `initBlocks`, edits still global `broadcast`);
   **4b (done)** = edits via `broadcastToChunk`, `initBlocks` removed, TEMP calls and logs removed. A `PlaceObject`
   is also refused unless the sender holds that chunk (the server never trusts the client's position).
   - Per client: the set of loaded chunks. Player chunk = floor(pos / 80).
   - At join and whenever the player's chunk changes: compute wanted chunks within radius R, send new ones
     **nearest first, at most K per tick** (a bandwidth budget), and unload ones beyond **R + 1**
     (the extra ring stops load/unload flicker at chunk borders).
   - No per-chunk subscriber list: an edit is sent to every player whose loaded set contains that chunk
     (a loop over players). One source of truth, nothing to keep in sync. Block edits go **only to those players**.
   - Clamp the radius the client asks for to [2, MAX].
5. **Client: chunk-aware `World`** (5a-5d, each testable):
   - **5a** `Client`: `ChunkData` / `ChunkUnload` go into ONE ordered queue (load and unload events, in arrival order).
   - **5b** `World`: a stream-chunk index (key -> set of cell keys, not indices into `objects`: swap-and-pop then
     never has to fix it up and removing one block is O(1); the renderer's 15-unit chunks can't be used, 80 isn't a multiple of 15) plus a `loadedStreamChunks` set (an empty
     chunk is still "loaded"). `addChunk`, `unloadChunk`, `isChunkLoaded`. `addObject` ignores unloaded chunks.
   - **5c** `Game::applyNetworkUpdates` drains chunk events in order, then edits; remove the client's `initBlocks` handling.
   - **5d** Loading gate: **freeze the player and show "Loading..." until the chunk under the player has arrived.**
   - **No per-frame apply limit (changed):** the server already meters K chunks per tick, and a client-side queue
     is where load/edit ordering goes wrong. Add a limit only if the F3 numbers show a frame spike.
6. **Wire the slider (done):** `Game::syncViewRadius` sends `ceil(renderDistance / 80)` chunks whenever it changes (and after each
   connect). The slider is capped at `MAX_RENDER_DISTANCE` (640 = server max 8 chunks x 80). Tune R, K and N.
   Steps 2-6 fix join time, client RAM and the freeze on the existing world. The rest is only needed
   for worlds that don't fit in server memory.
7. **Per-chunk generation** from the noise function (decision 3), lazily, replacing `generateWorld`.
8. **Persistence per chunk:** save only dirty chunks, on the background thread; drop `save.bin`'s single blob.
   Layout: `save/meta.bin` (seed, save format version, terrain generator version) plus `save/chunks/c.<cx>.<cz>.bin`,
   one file per *edited* chunk (file count follows edits, not world size; region files only if that ever hurts).
   - **8a** `SavedChunk` struct (format version, block count, cell-addressed blocks) with cereal `serialize`, in a header
     next to `chunk.hpp`, plus a round-trip test in the `Server/Makefile` `test` target.
   - **8b** Load path in `ensureChunk`: chunk file exists -> load it, else `generateChunk`. Load `meta.bin` before the
     first `ensureChunk` (wrong seed otherwise); write it once on first creation.
   - **8c** Save path: game thread snapshots each dirty chunk's blocks and clears its dirty flag *at snapshot time*
     (clearing after the write loses edits made during it); the background thread writes `.tmp` then `rename()`s.
   - Pitfalls: an empty chunk file is valid (fully mined-out chunk must not regenerate); mismatched generator
     version in `meta.bin` -> refuse or warn, since unedited chunks would regenerate differently next to saved ones.

Step 9 (client-side terrain from the seed) is dropped: the server always sends the chunks.

### Pitfalls

- **Startup cost:** until step 7 the server builds the whole world in its constructor, before `poll()` runs, so it
  accepts nobody and the client just retries "Joining server". `WORLD_SIZE = 10000` (~1.8 billion blocks) never
  finished and passed 16 GB. Stay at 200-500 until per-chunk generation exists.
- **Ctrl+C:** the SIGINT handler is registered only after the `Server` is constructed, so Ctrl+C during a long startup
  kills the process at once instead of being swallowed.
- **Ordering:** register a client as a chunk subscriber at the same moment its `ChunkData` snapshot is taken,
  and send both reliably on the same channel. Then an edit can never arrive before the chunk it belongs to.
- **Spawn:** a deterministic `heightAt(x, z)` lets the server put the spawn on the ground instead of `y = 10`,
  even for a chunk that isn't generated yet.
- **Edits to unloaded chunks:** the server ignores them; the client never sends them.
- **Structures** (later, e.g. trees): a chunk is a pure function of (seed, cx, cz), and so is every structure, so a chunk
  builds itself without asking a neighbour. Structure origins come from a hash of (seed, origin chunk); to build chunk C,
  take the structures of C and its neighbours within `MAX_STRUCTURE_RADIUS`, build each one from its origin (base
  height = `heightAt(origin)`), and keep only the blocks whose cell lies inside C. Don't push overflow blocks into a
  neighbour: it may already be generated, edited or saved. Process structures in a fixed order and keep the first block
  per cell (overlapping leaves), and skip leaves that fall inside terrain (`y <= heightAt(x, z)`).
- **Block format:** each block is an `Object` (id, position, scale, colour, durability) plus a hash entry.
  A chunk of 16x16 columns is ~500 of them. A compact per-column format (height + colour) is a later diet.

### How to measure

- **Join time:** press Play to first frame drawn.
- **Server:** the `tick:` line (objects / bullets / players) and how long startup takes.
- **Client:** F3 overlay (Objects, drawObjects ms) and RAM in Activity Monitor.
- **Network:** bytes sent per join (log it in `handleConnect`).

### Still to check (not confirmed yet)

Build the server and client, then check:

- Place blocks on the big world: it should be instant now (`placeBlock` no longer scans every block).
- Server: stop with Ctrl+C and confirm `save.bin` exists and there is **no leftover `save.bin.tmp`**.
- Server: place a block, wait one save period (30 s), then stop and restart; the block should still be there.

### Still open from before

- `SERVER_IP` is hard-coded in `env.hpp`; a join-server field on the menu would fit.
- Other asset types (sounds, fonts, models) aren't in `AssetManager`.
- Bigger ideas: inventory, structures (fits streaming), more textures (needs a texture atlas in the renderer).
- Client `World::damageObject` / `World::removeObject` are no longer O(N): 3b looks the block up in `occupiedCells`.
