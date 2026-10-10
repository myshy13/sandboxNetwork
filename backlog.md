# Backlog

Working notes for where the game goes next. Only open work remains; finished
and skipped items were cleared (see git history and `plan.md`).

---

## Now

- [ ] Block removal as a dedicated action (see Building).
- [ ] Pre-built structures / cover: trees exist (`structures.hpp`), add a
      second structure (hut, ruin) so playtests have cover as well as scenery.
- [ ] Update `.claude/rules/*` for trees and water if not already covered.

## Maintenance

- [ ] Fix the narrowing warnings MSVC reported in the server (`int` -> `float`
      at `server.hpp:72`, `server.cpp` chunk/terrain code, `unsigned` seed in
      `main.cpp`) with explicit casts so `-Wall -Wextra` stays clean.
- [ ] Remaining duplicated-by-hand constants (`STREAM_CHUNK_SIZE` vs
      `CHUNK_SIZE`, `MAX_RENDER_DISTANCE` vs `MAX_VIEW_RADIUS`): move into
      `sharedEnv.hpp` like `SHARED_PLAYER_HEALTH`, update `arch.md`.
- [ ] `static constexpr TREE_SHAPE` in a header gives every includer its own
      copy: make it `inline constexpr` (formatting rule).
- [ ] Server: hard-close the connection after sending `kick` on a protocol
      version mismatch instead of letting it linger.
- [ ] CI: run the two test suites (`Shared/Protocol`, `Server`) before the
      builds in `build.yml` (`test.yml` exists, make releases depend on it).
- [ ] Seed the CI build cache from `main` so tag releases aren't cold builds.
- [ ] Windows build parked: needs `NOGDI NOUSER NOMINMAX` on the server target.
- [ ] Bump `GAME_VERSION` and tag `vX.Y.Z` per release.

## Building & world

- [ ] block removal (dedicated "break" key or tool, not just shooting it)
- [ ] undo last placed block (client asks server to remove your most recent)
- [ ] block types beyond the cube (ramp, half-slab): `kind` enum on `Object`,
      matching draw + hitbox
- [ ] terrain variety: biomes (sand/snow colours), caves, or hills from a
      second noise layer
- [ ] tree variety: random height / canopy size per tree from the seed
- [ ] tree interaction: leaves breakable, trunk gives cover (check bullets
      stop on tree blocks like any other block)
- [ ] water: bucket / place-water block so players can shape flow
- [ ] water: swimming (slower move, buoyancy) and a screen tint underwater
- [ ] water: cap or test worst-case flow cost per tick on big open drops
- [ ] sound effects (shoot, hit, splash)
- [ ] Solid clouds (you can walk on them) **Note:** possibly maybe build the game goal about that

## Blocks to add

Each is an enum value + `BLOCK_INFO` row (see "Adding a block" in `arch.md`). Grouped by what they need beyond that.

Just a row and a texture (flags only):

- [ ] glass (see `plan.md`, "Then: glass"): solid, translucent, not opaque
- [ ] stone, gravel, snow, brick: plain solid cubes, for terrain variety and building
- [ ] ice: solid + translucent, a good cold-biome block
- [ ] cactus / bush: solid, could hurt on touch later (needs a damage hook, so wait)

Needs the per-block `state` byte (already in `Object`, no wire change):

- [ ] door / trapdoor: `state` = open or closed; the `solid` flag has to follow the state, so physics asks the state too
- [ ] crops / farmland: `state` = growth stage; needs a server tick that advances it
- [ ] torch / lantern: a light source; needs `Lighting` to take lights from blocks (only the sun exists now)

Needs stage 2 of the registry (behaviour hooks or a non-cube shape):

- [ ] sand (falls when unsupported): the first block with a server tick, so it is one of the two cases stage 2 is designed from
- [ ] lava: a second fluid, which is what lets `FluidSim` stop being water-only
- [ ] slab / stairs / ramp: a client-side shape per block, plus a matching hitbox (see "block types beyond the cube")
- [ ] ladder: climb instead of fall, a player-movement rule keyed on the block under you

## Entities & animals

Cows, sheep, chickens. This is a new system, not a block: expect it to be bigger than anything so far (a rough guess:
more work than bullets, about the size of chunk streaming). Do it in slices, each one playable:

- [ ] 1. One entity type (a cow) that the **server** owns and moves: a `std::vector<Entity>` next to `players` and
      `bullets` in `Server`, ticked in `Server::tick`. Wander AI: pick a direction, walk a few seconds, stand, repeat.
      Server-side gravity and block collision (players do this on the client; an animal can't, nobody owns it)
- [ ] 2. Sync it: `SpawnEntity` / `EntityUpdate` (unreliable, like `PlayerUpdate`) / `DespawnEntity` in `protocol.hpp`,
      plus a client list and a drawn box (a cube cow first, a model later). Interpolate between updates on the client
- [ ] 3. Interest: only send entities in chunks the client can see (reuse the per-client chunk view, `updateView` /
      `broadcastToChunk`), or every animal on the map goes to every player
- [ ] 4. Shootable: reuse the bullet sweep in `Server::tick` against entity boxes; entity health, death, despawn
- [ ] 5. Spawning: a few per chunk from the seed when it generates, a cap per chunk, despawn when far from all players
- [ ] 6. More species and drops (meat, wool), once one species is solid
- [ ] unresolved: persistence. The product rules say no persistence, but chunks are saved, so decide whether animals are
      saved with their chunk or respawn fresh

## Structures

Each is a block-offset table like `TREE_SHAPE`, placed by terrain from the seed.

- [ ] hut: 5x5 walls with a door gap and window, flat roof (cover + shelter)
- [ ] watchtower: 3x3 tall shaft with a platform and rail (sniper perch)
- [ ] ruined wall: broken line of stacked blocks (cheap cover, no interior)
- [ ] boulder cluster: a few stone-coloured blobs, harmless scatter cover
- [ ] bridge: plank span for gaps/water, needs a river or ravine to matter
- [ ] ruins / arena: ring of low walls with gaps, a natural fight spot
- [ ] well or pond: water source block in a stone ring (feeds `FluidSim`)
- [ ] bunker: half-buried box with one entrance (needs terrain-height carve)
- [ ] pine / dead tree: variants of `TREE_SHAPE` for biome variety
- [ ] spawn camp: small fenced pad so new players start with cover
- [ ] placement rules: min spacing, flat-ground check, never overlap water or
      chunk borders unless the table is clipped per chunk

## Lighting & rendering

- [ ] water look: transparency / animated surface instead of a flat colour
- [ ] day/night: rotate the directional light, server broadcasts time-of-day
- [ ] blob shadow under each player (shadow mapping is a big lift)
- [ ] skybox / gradient background instead of near-black clear
- [ ] fog at the render-distance edge so chunk pop-in is hidden
- [ ] weaker shadows from translucent blocks (glass, water): a second depth map holding only translucent faces, drawn in
      its own pass in `Renderer::shadowMap`; the lighting shader looks it up beside the opaque map and scales the sun by a
      constant (about 0.6) where it is blocked. One strength for every translucent block (a depth map can't carry a
      per-block value). A face must not shadow itself (reuse the bias), and `glsl100` needs the same change. Until
      then glass and water cast no shadow

## Combat & players

- [ ] names above players: scale, occlusion, distance fade
- [ ] hold-to-shoot: tune the rate
- [ ] gun model in first person + muzzle flash
- [ ] respawn timer + spawn-point selection instead of instant respawn
- [ ] health regen or pickups
- [ ] hit direction indicator (which way did that shot come from)
- [x] kill feed (top-right, "A killed B") **Revision:** In the chat, not top right. displayed as red
- [ ] fall damage / drowning

## Netcode

- [ ] client-side prediction + reconciliation for the local player
- [ ] lag compensation for hit detection (rewind targets to shooter's time)
- [ ] send rate / tick rate as a shared constant, not a magic `0.1667`
- [ ] basic anti-cheat: server rejects impossible position deltas
- [ ] bullets crossing into unloaded chunks: die or generate on demand
- [ ] measure bandwidth per player at the default render distance, including
      water level updates while a flow is spreading
- [ ] chunk save format versioning (terrain/tree changes vs old saves)

## Infra & ops

- [ ] deploy the server to an always-on host under systemd, `Restart=always`
- [ ] `--max-players` cap with a polite "server full" reject
- [ ] status console command (player count, uptime, chunk count)
- [ ] web client: verify the WS proxy path against the deployed server over
      `wss://`
- [ ] iOS client: planned in `plan.md` ("iOS client"), setup in `iOS.md`. Original questions (unchecked: what it supports, whether ENet/UDP works on iOS or it needs
      the WebSocket transport, touch controls for the camera, WASD and shooting, and the shadow/render cost on a phone)
