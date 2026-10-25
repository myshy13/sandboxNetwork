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

- [x] glass (see `plan.md`, "Then: glass"): solid, translucent, not opaque
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

## Game mode: last one standing

An arena where the last player alive wins. Two ways to drive it; the round loop is the safer first step.

- [ ] round loop: a server-owned `Round` state machine (Lobby, Countdown, Playing, Ending), `RoundState` message,
      per-player `alive` flag, arena regenerated per round. Watch: simultaneous deaths (draw), disconnects count as dead,
      late joiners spectate, `--save-path` should not persist a per-round arena
- [ ] rising tide (alternative to the phase machine): a `tide` 0-1 clock on the server beside `timeOfDay`, sent as
      `SetTide` on connect and every `env::TIME_BROADCAST_INTERVAL`, clients tick it in between. A shared `levelAt(tide)`
      (in `Shared/`, like `SHARED_PLAYER_SCALE`) turns it into a water height; a player whose head cell is below it takes
      damage through the normal death path. Draw the water as a translucent plane, not real `FluidSim` blocks.
      Needs an arena terrain mode with real high ground (a central mountain in a basin), and a respawn rule
      (e.g. when the tide drops below spawn height)
- [ ] shrinking safe zone: damage outside a closing boundary; pairs with the round loop to force fights

## Different to Minecraft

Same voxel look, different game. Pick a direction before building any of these: they pull different ways.

- [ ] last one standing (see the section above): arena, no crafting or survival loop
- [ ] sky islands: floating terrain with gaps, so falling is the hazard and bridges or jumps are the routes (pairs with
      solid clouds in "Building & world")
- [ ] capture the flag / king of the hill on the voxel map: blocks are cover and routes, not resources
- [ ] destructible cover: bullets damage blocks (`Object` already carries damage), craters open lines of sight and
      `FluidSim` floods them
- [ ] tug-of-war: two bases, a front line that moves; the team that holds more ground drains the other's base
- [ ] hide and seek: one hunter, many props that disguise as blocks; the server checks guesses, so nobody can cheat
- [ ] time attack course: a parkour map with checkpoints and a server-kept best-time board, no combat
- [ ] gravity or tide events: the server flips a rule on a timer (low gravity, rising water, night-only visibility) and
      broadcasts it like `SetTime`
- [ ] one-shot duels: a bracket of 1v1 matches in the arena, spectators watch the live fight
- [ ] build-then-defend: a short build phase to wall in a base, then a siege where the other team shoots it down
- [ ] map vote per match: the seed picks the terrain style (sky islands, canyon, flooded valley)

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

## Input & platforms

- [ ] `input_touch_remote.cpp`: a Siri Remote input source for tvOS (touch surface moves the pointer, click, Play/Pause
      and Menu map to Click / Pause / Exit). Today the remote's touch surface still reaches raylib as a touch and moves
      raylib's mouse (`rcore_tvos.c`), but nothing reads it as an `InputSource`. First make `Background::frame()` take
      its pointer from `input.getPointer()` instead of `GetMousePosition()`, or the particles keep following the remote
      on the TV while the gamepad cursor is somewhere else.
- [ ] Ship gamepad support on every device that supports Bluetooth controllers (iOS, tvOS, desktop; web if the browser
      Gamepad API is enough), not only tvOS or `-DUSE_GAMEPAD_INPUT=ON`, and let the player switch between sources.
      Needs `Input` to own more than one `InputSource` (CMake compiles exactly one today) and a rule for which one is
      active: last device used, or a setting. Hide the touch overlay and the drawn cursor of whichever source is not
      active. Update `arch.md` ("CMake compiles exactly one") and `tech.md` when it lands.

## Admin & moderation

- [-] `AdminUnban`: the admin panel's unban also tells the game server (new message, admin-checked like `AdminKick`),
      so it drops that account from the revoked-tickets list. Until then an unbanned player stays blocked on the game
      server until their entry expires (up to an hour). **Skipped:** They can just log back in, it's not that hard
- [x] Game server revoked-tickets list: `AdminKick` records the account (`revokedUntil`, lifetime = ticket lifetime +
      `EXPIRY_GRACE_WINDOW`), and the `clientHandshake` and `TicketRefresh` cases reject it, so a banned player can't
      rejoin on a ticket that is still valid.

## Infra & ops

- [ ] deploy the server to an always-on host under systemd, `Restart=always`
- [x] `--max-players` cap with a polite "server full" reject
- [ ] status console command (player count, uptime, chunk count)
- [ ] status log: average ping (`roundTripTime` on each ENet peer)
- [ ] web client: verify the WS proxy path against the deployed server over
      `wss://`
- [ ] iOS client: planned in `plan.md` ("iOS client"), setup in `iOS.md`. Original questions (unchecked: what it supports, whether ENet/UDP works on iOS or it needs
      the WebSocket transport, touch controls for the camera, WASD and shooting, and the shadow/render cost on a phone)
