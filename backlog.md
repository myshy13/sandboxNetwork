# Backlog

Working notes for where the game goes next. Checked items are done (some
still want polish — noted inline). The three **Now** items are the current
focus, in order.

---

> **Note:** [/] means decided not to.

## Now

### 1. World saving / loading

The server is currently in-memory only — every block vanishes on restart.
Goal: the built world survives a server reboot.

- [x] Server serialises `objects` to a file on a timer + on clean shutdown
      (`world.bin` via cereal, same as the wire format — reuse
      `Object::serialize`).
- [x] Server loads that file on startup (restores `objects` + `nextObjectId`),
      broadcasts nothing — clients get the world via `NewObject` on connect.
- [x] `--world <path>` CLI arg (default `world.bin`), `--no-save` to opt out.
- [x] Decide the save cadence: every N seconds *and* debounced after the last
      edit, so an idle server isn't rewriting the file forever.

### 2. Block colours (player picks their own)

`Object` already has a `color` field and it's already on the wire — this is
mostly client wiring.

- [x] Colour palette UI (number keys 1–8, or a small swatch bar).
- [x] `placeBlock` stamps the selected colour onto the `Object` before
      `client.placeObject` (right now it always sends default white).
- [/] Server: trust the colour from `PlaceObject` (it's cosmetic, no reason
      to validate) but clamp to the palette so it can't be abused for
      messages / eye-strain colours.
- [x] Durability shading (`Object::damage`) currently overwrites colour with
      grey — make it *tint toward* grey instead so a damaged red block still
      reads as red.

### Pre-3. Rendering optimisations

Each block currently costs two immediate-mode draw calls
(`DrawCubeV` + `DrawCubeWiresV` in `World::draw`), so 30 blocks is 60 calls
before players/bullets — the web build (WebGL via Emscripten) pays far more
per-draw-call overhead than native, hence it lagging first.

- [ ] Batch the wireframe: draw one `DrawCubeWiresV` outline only for the
      block under the crosshair / recently placed, not every block every
      frame — the outlines are the cheapest thing to cut and add nothing
      once the scene is dense.
- [ ] Instance the cubes: `DrawMeshInstanced` with one cube mesh and a
      per-instance transform/color, instead of one `DrawCubeV` call per
      `Object` — collapses N draw calls into 1 regardless of block count.
- [ ] Frustum-cull `objects` before drawing (raylib has
      `GetCameraFrustum` / bounding-box checks) so blocks behind the camera
      aren't submitted at all.
- [ ] Re-check block count where it stops being smooth after each change
      above (30, 100, 300) so this list can stop once it's fast enough
      rather than chasing a perfect renderer.

### 3. A real starting world instead of the bare grid

Right now `World::draw` just draws `DrawGrid`. Spawn into something.

- [ ] A flat floor of blocks (generated once on the server at startup if the
      save file is empty) so bullets/placement have something to land on.
- [ ] Keep it server-authoritative — the floor is just `objects` like any
      placed block, not special-cased client geometry.
- [ ] Optional: a couple of pre-built structures / cover so early playtests
      aren't on an empty plain.
- [x] Lit ground: `World::draw` lays a `DrawPlane` under the grid so the
      floor isn't near-black away from origin (the grid lines themselves
      still render dark through the shader — a real block floor supersedes
      this anyway).

---

## Building & world

- [x] right-click block placement
- [x] block-vs-player placement collision check
- [x] block durability (server-side hit damage + client colour feedback)
- [ ] block removal (dedicated action, not just shooting it — a "break" key
      or left-click with a tool selected)
- [ ] undo last placed block (client asks server to remove your most recent)
- [ ] larger builds: place a 2×2×2 or drag a line of blocks
- [ ] snap-to-grid preview: ghost block at the target cell before you commit
- [ ] block types beyond the plain cube (ramp, half-slab) — needs a `kind`
      enum on `Object` and matching draw + hitbox

## Lighting & rendering

- [x] per-fragment lighting (`Lighting` class, raylib `lighting.fs` + rlights)
- [x] call `setViewPos` each frame so specular highlights track the camera
- [x] a second light — two warm directional lights crossing over the scene
      (a true warm *point* light is still todo if you want local falloff)
- [ ] day/night: rotate the directional light over time, server broadcasts
      the time-of-day so everyone matches
- [ ] simple shadows (shadow-mapping is a big lift — maybe just a dark blob
      decal under each player first)
- [ ] skybox / gradient background instead of near-black clear

## Combat & players

- [x] names above players *(needs improvement — scale, occlusion, distance fade)*
- [ ] hold to shoot *(partly there — `IsMouseButtonDown` path exists, tune it)*
- [ ] an actual gun model in first person + muzzle flash
- [ ] fix the kills menu
- [ ] better crosshair (hit-marker feedback on a confirmed hit)
- [ ] bigger / better player model — a stickman instead of the current
      "uncooked french fry"
- [ ] respawn timer + spawn-point selection instead of instant respawn
- [ ] health regen or pickups
- [ ] hit direction indicator (which way did that shot come from)
- [ ] kill feed (top-right, "A killed B")

## Netcode

- [x] version handshake on connect — `proto::PROTOCOL_VERSION`, client sends
      `clientHandshake{ver}` first, server sends `kick` on mismatch.
      (Polish: server still lets the connection linger after sending the
      kick — could hard-close it.)
- [ ] client-side interpolation of remote players (buffer 2–3 updates, render
      ~100ms in the past — kills the current teleport-on-packet look)
- [ ] client-side prediction + reconciliation for the local player
- [ ] lag compensation on the server for hit detection (rewind targets to the
      shooter's view time)
- [ ] send rate / tick rate as a shared constant, not a magic `0.1667` on
      the client (server tick rate now lives in `Server/src/env.hpp`)
- [ ] basic anti-cheat: server rejects impossible position deltas

## Infra & ops

- [ ] deploy the server to an always-on host (Oracle free-tier Arm box or GCP
      e2-micro) under systemd with `Restart=always`
- [ ] server logs to a file, not just stdout
- [ ] a `--max-players` cap with a polite "server full" reject
- [ ] health/status endpoint or console command (player count, uptime,
      object count)
- [ ] web client: verify the WS proxy path works against the deployed server
      over `wss://`

## Housekeeping

- [ ] delete the tracked `.mov` screen recording from the repo
- [ ] the `CHAT` / `CHEATS` / `CHEATS`-nested `#ifdef`s in `main.cpp` — decide
      if chat is a real feature and either commit to it or cut it
- [ ] `PLAYER_SCALE` is duplicated in three places now (server, player,
      world.cpp placement check) — pull into one shared header (the two
      `env.hpp` files are the obvious home, but it must stay in sync across
      the client/server split)
