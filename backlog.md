# Backlog

Working notes for where the game goes next. Checked items are done (some
still want polish — noted inline). The three **Now** items are the current
focus, in order.

---

## Now

### 1. World saving / loading

The server is currently in-memory only — every block vanishes on restart.
Goal: the built world survives a server reboot.

- [ ] Server serialises `objects` to a file on a timer + on clean shutdown
      (`world.bin` via cereal, same as the wire format — reuse
      `Object::serialize`).
- [ ] Server loads that file on startup, re-assigns ids, broadcasts nothing
      (clients get the world through the normal `NewObject` path on connect).
- [ ] `--world <path>` CLI arg (default `world.bin`), `--no-save` to opt out.
- [ ] Decide the save cadence: every N seconds *and* debounced after the last
      edit, so an idle server isn't rewriting the file forever.
- [ ] **Rules update**: `product.md` lists "no persistence" as a non-goal.
      That line changes when this lands — persistence of *world geometry*
      only, still no accounts / player state.

### 2. Block colours (player picks their own)

`Object` already has a `color` field and it's already on the wire — this is
mostly client wiring.

- [ ] Colour palette UI (number keys 1–8, or a small swatch bar).
- [ ] `placeBlock` stamps the selected colour onto the `Object` before
      `client.placeObject` (right now it always sends default white).
- [ ] Server: trust the colour from `PlaceObject` (it's cosmetic, no reason
      to validate) but clamp to the palette so it can't be abused for
      messages / eye-strain colours.
- [ ] Durability shading (`Object::damage`) currently overwrites colour with
      grey — make it *tint toward* grey instead so a damaged red block still
      reads as red.

### 3. A real starting world instead of the bare grid

Right now `World::draw` just draws `DrawGrid`. Spawn into something.

- [ ] A flat floor of blocks (generated once on the server at startup if the
      save file is empty) so bullets/placement have something to land on.
- [ ] Keep it server-authoritative — the floor is just `objects` like any
      placed block, not special-cased client geometry.
- [ ] Optional: a couple of pre-built structures / cover so early playtests
      aren't on an empty plain.
- [ ] Pull `DrawGrid` out of `World::draw` and draw it in `main.cpp` before
      `lighting.begin()` (it renders black through the lighting shader).

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
- [ ] call `setViewPos` each frame so specular highlights track the camera
- [ ] a second light (warm point light) to show off the system
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

- [ ] version handshake on connect — `proto::VERSION` constant (bump by hand
      on any `serialize` / `Type` change), client sends a fixed-shape `Hello
      {uint32 version}` first, server disconnects + `Rejected` on mismatch.
      Guards against a stale client silently misparsing the wire. Do this
      once the server is deployed separately from the dev machine.
- [ ] client-side interpolation of remote players (buffer 2–3 updates, render
      ~100ms in the past — kills the current teleport-on-packet look)
- [ ] client-side prediction + reconciliation for the local player
- [ ] lag compensation on the server for hit detection (rewind targets to the
      shooter's view time)
- [ ] send rate / tick rate as a shared constant, not a magic `0.1667`
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
      world.cpp placement check) — pull into one shared header
