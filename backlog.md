# Backlog

Working notes for where the game goes next. Finished sections were removed
(world saving, block colours, rendering/collision perf, starting world, chunk
streaming; see git history and `plan.md`). Only open work and a few open
polish notes remain.

> **Note:** [/] means skipped or unnecessary
> **Note:** [-] means removed after it was unnecessary

---

## Now

- [ ] Finish `plan.md`: seeded terrain per chunk (`terrain.hpp`) and persisting
      only changed chunks (steps 7-8).
- [ ] Optional: a couple of pre-built structures / cover so early playtests
      aren't on an empty plain.
- [ ] Block removal as a dedicated action (see Building).

## Maintenance

- [ ] Fix the narrowing warnings MSVC reported in the server (`int` -> `float`
      at `server.hpp:72`, `server.cpp` chunk/terrain code, `unsigned` seed in
      `main.cpp`) with explicit casts so `-Wall -Wextra` stays clean.
- [ ] Duplicated-by-hand constants (`STREAM_CHUNK_SIZE` vs `CHUNK_SIZE`,
      `MAX_RENDER_DISTANCE` vs `MAX_VIEW_RADIUS`, `MAX_HEALTH` vs
      `PLAYER_MAX_HEALTH`): move into `sharedEnv.hpp` like `SHARED_PLAYER_SCALE`
      and update the "must stay in sync" section of `arch.md`.
- [ ] Server: hard-close the connection after sending `kick` on a protocol
      version mismatch instead of letting it linger.
- [ ] Run the two test suites (`Shared/Protocol`, `Server`) in CI before the
      builds, so a broken protocol can't be released.
- [ ] Seed the CI build cache from `main` (tag runs can only restore caches
      from the default branch), so releases aren't always cold builds.
- [ ] Windows build was removed. Parked: bring it back only if someone needs
      it (needs `NOGDI NOUSER NOMINMAX` on the server target, and the
      server `min`/`max` clashes checked on the client too).
- [ ] Keep `.claude/rules/*` and `README.md` current after each feature
      (build/run commands, folder layout, new protocol messages).
- [ ] Bump `GAME_VERSION` and tag `vX.Y.Z` per release so the GitHub Release
      job publishes the Linux/macOS builds.

## Building & world

- [x] right-click block placement
- [x] block-vs-player placement collision check
- [x] block durability (server-side hit damage + client colour feedback)
- [ ] block removal (dedicated action, not just shooting it: a "break" key
      or left-click with a tool selected)
- [ ] undo last placed block (client asks server to remove your most recent)
- [ ] block types beyond the plain cube (ramp, half-slab): needs a `kind`
      enum on `Object` and matching draw + hitbox
- [/] larger builds: 2x2x2 or drag a line of blocks **(other half, no)**
- [/] snap-to-grid ghost block **Reason:** it will look ugly and hide the view
- [/] clamp `PlaceObject` colour to the palette **Reason:** cosmetic only

## Lighting & rendering

- [x] per-fragment lighting, `setViewPos` each frame, two directional lights
- [ ] a true warm point light if local falloff is wanted
- [ ] day/night: rotate the directional light over time, server broadcasts
      the time-of-day so everyone matches
- [ ] simple shadows (a dark blob decal under each player first; shadow
      mapping is a big lift)
- [ ] skybox / gradient background instead of near-black clear

## Combat & players

- [x] names above players *(needs improvement: scale, occlusion, distance fade)*
- [x] hold to shoot *(tune the rate)*
- [x] hit-marker crosshair, stickman player model
- [ ] an actual gun model in first person + muzzle flash
- [ ] fix the kills menu
- [ ] respawn timer + spawn-point selection instead of instant respawn
- [ ] health regen or pickups
- [ ] hit direction indicator (which way did that shot come from)
- [ ] kill feed (top-right, "A killed B")

## Netcode

- [x] version handshake (`proto::PROTOCOL_VERSION`), remote-player interpolation
- [x] interest management: per-client chunk views, edits only to holders
- [ ] client-side prediction + reconciliation for the local player
- [ ] lag compensation on the server for hit detection (rewind targets to the
      shooter's view time)
- [ ] send rate / tick rate as a shared constant, not a magic `0.1667` on
      the client
- [ ] basic anti-cheat: server rejects impossible position deltas
- [ ] bullets crossing into unloaded chunks: decide (die vs generate on demand)
- [ ] measure bandwidth per player at the default render distance

## Infra & ops

- [ ] deploy the server to an always-on host (Oracle free-tier Arm box or GCP
      e2-micro) under systemd with `Restart=always`
- [ ] a `--max-players` cap with a polite "server full" reject
- [ ] health/status console command (player count, uptime, chunk count)
- [ ] web client: verify the WS proxy path works against the deployed server
      over `wss://`
- [x] server logs to `server.log`
