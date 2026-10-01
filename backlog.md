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
