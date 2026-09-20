# Plan: real world streaming (written Sun 20 Sep 2026)

## Why

`WORLD_SIZE` 200 -> 500 made the world 1000x1000 columns: 2.2M blocks at first, 4.6M once the heights became
`rand() % 20` (was 160,000+ at size 200).
Everything that touches "all blocks" got ~6x worse, and it grows with the square of the world size:

- **Join:** the server sends every block to every joining client, and the client keeps all of them in RAM.
- **Server save:** copies and writes every block (now on a background thread, but still the whole world).
- **Client scans:** `damageObject` / `removeObject` search all blocks by id; the renderer walks every chunk.
- **Startup:** `generateWorld` builds the whole map (random + 7 smoothing passes over all of it).

Streaming means cost follows what is near each player, not how big the world is.

## Target behaviour

- Each client only holds chunks within its view radius; the server only sends those.
- The world can be effectively unbounded (chunks are generated the first time someone needs them).
- Join time and RAM stay flat however big the world is.
- The render-distance slider finally means something for bandwidth and memory, not just drawing.

## Decisions to make first (recommendations in bold)

1. **Address blocks by cell (x, y, z), not by id.** Ids come from a counter that has to be saved,
   and terrain that is generated per chunk has no natural id. Cell addressing also removes the client's
   O(N) `removeObject` / `damageObject` scans. **Do this. It is a protocol change: bump `PROTOCOL_VERSION`.**
2. **Chunk = 16x16 cells wide (80x80 world units), full height.** This is separate from the renderer's
   3-cell chunks, which stay as they are (they live inside these).
3. **(Later, steps 7-9)** **Terrain from a deterministic noise function of (seed, x, z), not the global heightMap + smoothing.**
   The smoothing needs the whole map, so it can't be evaluated per chunk. Use integer-hash value noise
   (2-3 octaves) so every machine (arm, x86, web) gets identical results. Store the seed in the save.
4. **(Later, steps 7-9)** **Persist only changed chunks** (dirty flag per chunk). A chunk loads as: saved file if it exists,
   otherwise generate it. Reuse the background-save idea from `Server::saveWorldAsync`.
5. **Bullets:** speed 500 u/s and 20 s lifetime means up to 10 km of travel. **Bullets die on leaving the
   loaded area** for now; generating chunks on demand for bullets can come later.

## Steps (each one should leave the game working)

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
9. **Optional, big win:** send the seed once and let the client generate the terrain itself
   (shared code in `Shared/`). The server then only sends **edits** per chunk. Needs the noise to be
   bit-identical on every platform, hence the integer-hash noise in decision 3.

## Pitfalls

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
- **Structures** (later): placed per chunk from a hash of (seed, chunk x, chunk z). A structure that crosses
  a chunk border needs a rule (generate from the neighbours' overlap) or you get cut-off buildings.
- **Block format:** each block is an `Object` (id, position, scale, colour, durability) plus a hash entry.
  A chunk of 16x16 columns is ~500 of them. A compact per-column format (height + colour) is a later diet.

## How to measure

- **Join time:** press Play to first frame drawn.
- **Server:** the `tick:` line (objects / bullets / players) and how long startup takes.
- **Client:** F3 overlay (Objects, drawObjects ms) and RAM in Activity Monitor.
- **Network:** bytes sent per join (log it in `handleConnect`).

## Still to check (not confirmed yet)

Build the server and client, then check:

- Place blocks on the big world: it should be instant now (`placeBlock` no longer scans every block).
- Server: stop with Ctrl+C and confirm `save.bin` exists and there is **no leftover `save.bin.tmp`**.
- Server: place a block, wait one save period (30 s), then stop and restart; the block should still be there.

## Still open from before

- `SERVER_IP` is hard-coded in `env.hpp`; a join-server field on the menu would fit.
- Other asset types (sounds, fonts, models) aren't in `AssetManager`.
- Bigger ideas: inventory, structures (fits streaming), more textures (needs a texture atlas in the renderer).
- Check the "Loading..." overlay in `game.cpp` (an `else if` chain made it show only while paused).
- Client `World::damageObject` / `World::removeObject` are no longer O(N): 3b looks the block up in `occupiedCells`.
