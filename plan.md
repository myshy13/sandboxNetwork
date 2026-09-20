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
2. **Server chunk index (no behaviour change).** Keep `objects` and `occupiedCells` as they are. Add
   `unordered_map<int64_t, vector<int>>` from chunk key to indices into `objects`, updated in `addBlock` and
   `removeBlock` (including the swap-and-pop fix-up; mirror `World::indexObject` / `removeObject` on the client).
   Chunk = 16x16 cells (80 units). Check: print chunk count and average blocks per chunk at startup.
3. **Protocol** (`Shared/Protocol/protocol.hpp`): `ChunkData` (chunk key + blocks), `ChunkUnload` (key),
   `SetViewRadius` (client -> server, in chunks), and cell-addressed Place/Remove/Damage. Bump `PROTOCOL_VERSION`.
   Keep the old `initBlocks` path working until step 4 replaces it.
4. **Server: interest management.**
   - Per client: the set of loaded chunks. Player chunk = floor(pos / 80).
   - At join and whenever the player's chunk changes: compute wanted chunks within radius R, send new ones
     **nearest first, at most K per tick** (a bandwidth budget), and unload ones beyond **R + 1**
     (the extra ring stops load/unload flicker at chunk borders).
   - Per chunk: the list of subscribed players. Block edits are broadcast **only to subscribers**.
   - Clamp the radius the client asks for to [2, MAX].
5. **Client: chunk-aware `World`.** `addChunk` / `unloadChunk`. Simplest first version: keep the
   `objects` vector with swap-and-pop and unload a chunk by removing its blocks one by one
   (cost is per chunk, not per world). Apply **at most N chunks per frame** (the freeze fix). Mark the
   neighbouring render chunks dirty when a chunk arrives, so border faces are drawn correctly.
   Loading gate: **freeze the player and show "Loading..." until the chunk under the player has arrived.**
6. **Wire the slider:** render distance / 80 -> `SetViewRadius`. Tune R, K and N.
   Steps 2-6 fix join time, client RAM and the freeze on the existing world. The rest is only needed
   for worlds that don't fit in server memory.
7. **Per-chunk generation** from the noise function (decision 3), lazily, replacing `generateWorld`.
8. **Persistence per chunk:** save only dirty chunks, on the background thread; drop `save.bin`'s single blob.
9. **Optional, big win:** send the seed once and let the client generate the terrain itself
   (shared code in `Shared/`). The server then only sends **edits** per chunk. Needs the noise to be
   bit-identical on every platform, hence the integer-hash noise in decision 3.

## Pitfalls

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
- Remaining O(N) client scans: `World::damageObject` and `World::removeObject` (fixed properly by cell
  addressing in decision 1).
