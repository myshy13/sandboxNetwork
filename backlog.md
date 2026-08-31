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
- **World bounds.** `spawnPos` picks random x/z in [-100, 100] but nothing
  stops a player (or a bullet) from wandering or falling forever outside
  that range. A void/out-of-bounds check that triggers a respawn (reusing
  the existing `Respawn` message) would tidy this up.
- **Ammo/reload.** Firing is currently free and instant - hold left click,
  bullets spawn every click with no cooldown enforced server-side (see the
  anti-cheat note below). A magazine size + reload delay would also
  double as the fix for the missing fire-rate limit.
- **Scoreboard.** Kills are already implied by `PlayerHit.shooterId` but
  nothing counts them. A `std::unordered_map<int, int> kills` on the
  server, broadcast periodically or on change, plus a Tab-to-show overlay
  client-side.

## Server hardening

Carried over from an earlier conversation about how easy the current
protocol is to cheat on - noting it here so it doesn't get lost:

- Validate `PlayerUpdate`: clamp per-tick movement distance, reject
  positions that imply speedhacking/teleporting instead of trusting
  whatever the client reports.
- Rate-limit `CreateBullet` server-side (nothing currently stops a
  modified client from firing far faster than the game intends).
- Fix the two `ChatMessage` issues from the chat commit: the handler
  falls through into the "Invalid request" `default` case (missing
  `break`), and it broadcasts the client's raw payload unchecked rather
  than re-stamping the sender id from the connection - so a client can
  currently claim to be a different player in chat.

## Chat

- Slash commands (`/respawn`, `/help`, maybe `/tp` for testing) handled
  client- or server-side before falling through to a normal broadcast
  message.
- Server-side length/rate limiting on `ChatMessage` (the client already
  clamps to 200 chars, but nothing stops a modified client from spamming
  short messages rapidly).
- ~~Show display names instead of ids~~ Done with the display-name feature.

## Web build

- Auto-reconnect on WebSocket drop instead of leaving the client stuck
  showing "Failed to connect to server" forever.
- Basic touch controls (movement stick + fire button) so the web build is
  playable on a phone browser, not just desktop Chrome.

## Tooling

- A small integration test: spin up a `Server`, connect two fake ENet
  clients, and assert their state converges (player positions, a bullet
  hit, a chat message) - closer to an end-to-end check than
  `Shared/Protocol/test_protocol.cpp`'s pure serialization round-trips.
- Move server config (map bounds, tick rate, max players) out of hardcoded
  constants and into either more `--flags` or a small config file, now
  that `--ws-port` has established the CLI-flag pattern.


## human stuff

- names above players
- better player things
- hold to shoot
- fix kills menu
- a gun
- better crosshair
- a bit bigger player (or a stickman instead of an uncooked frenc hfry).
- building color blocks (changed by player, each player has their own color)
- block durability
