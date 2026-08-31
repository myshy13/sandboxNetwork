# Backlog

Feature ideas for sandboxNetwork, roughly grouped and ordered by how well
they build on what already exists. Not commitments, just a running list.

## Gameplay

- ~~**Display names.**~~ Done (`458615e`). Client sends a chosen name in
  the connect handshake, server stores it on `Player` and threads it
  through chat / hit / connect log instead of raw ids.
- ~~**Building/placing blocks.**~~ Done (`c6e3929`, `af939dd`). Fixed-size
  blocks, place/break protocol pair, positions broadcast to new joiners,
  player collision against placed blocks.
  - **Note:** Added a maximum block placing distance check in `world.cpp`
    (not currently enforced server-side).
- ~~**World bounds.**~~ Not needed — players and bullets are intentionally
  allowed to leave the normal spawn area.
- **Ammo/reload.** Add a magazine size and reload delay. This could also
  provide a proper fire-rate limit enforced server-side.
- **Scoreboard.** Track kills using `PlayerHit.shooterId` and display them
  in a Tab-to-show scoreboard.

## Server hardening

Carried over from an earlier review of the current protocol. These are
important once the game is being played by people other than yourself:

- **Validate `PlayerUpdate`.** Clamp per-tick movement distance and reject
  positions that imply speedhacking or teleporting instead of trusting
  whatever the client reports.
- **Rate-limit `CreateBullet`.** Nothing should allow a modified client to
  fire faster than the intended fire rate.
- **Fix `ChatMessage` validation.** The handler currently falls through into
  the "Invalid request" default case (missing `break`) and accepts the
  client's sender id instead of re-stamping it from the connection.
- **Validate building requests server-side.** The maximum block placing
  distance currently exists in `world.cpp`, but the server should enforce
  the rule too.

## Chat

- **Slash commands.** `/respawn`, `/help`, and possibly `/tp` for testing.
- **Server-side message limits.** Limit both message length and message
  frequency so modified clients cannot spam the server.
- ~~**Show display names instead of ids.**~~ Done with the display-name
  feature.

## Web build

- **Auto-reconnect.** Reconnect automatically after a WebSocket drop instead
  of leaving the client stuck showing "Failed to connect to server".
- **Touch controls.** Add a movement stick and fire button so the web build
  works properly on phones.

## Tooling

- **Integration test.** Spin up a `Server`, connect two fake ENet clients,
  and assert that their state converges: player positions, a bullet hit,
  and a chat message.
- **Server configuration.** Move map bounds, tick rate, max players, etc.
  out of hardcoded constants and into CLI flags or a small config file.

## Human stuff

- [x] names above players *(needs improvement)*
- [ ] better player things
- [ ] hold to shoot
- [ ] fix kills menu
- [ ] a gun
- [ ] better crosshair
- [ ] slightly bigger player (or a stickman instead of an uncooked french fry)
- [ ] building color blocks — player chooses their own color
- [ ] block durability
