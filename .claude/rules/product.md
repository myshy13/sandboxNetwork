# Product

A small multiplayer 3D shooter: one raylib client (native + web), one
authoritative server, one shared binary protocol.

## Core loop

- Player spawns, moves with WASD + mouselook, jumps with Space.
- Left click fires a bullet along the camera's aim.
- The server owns hit detection: a bullet that sweeps through another
  player's box kills the bullet and notifies the hit player
  (`proto::Type::PlayerHit`). The shooter can't hit themself
  (`Server::tick`, `Server/src/Server/server.cpp`).
- Bullets expire after `deathCountdown` seconds if they hit nothing.
- Position updates are unreliable/best-effort (a dropped frame is
  superseded a frame later); connect/disconnect/create-bullet/hit events
  are reliable.

## Day and night

- A day is `env::DAY_LENGTH_SECONDS` (20 minutes). The server keeps the clock and saves it; clients show a moving sun,
  a sky colour that follows it, dim blue-grey nights and (optionally) sun shadows from blocks.
- Shadows are a setting (settings screen), on by default on native and off on web.

## Clients

- **Native**: connects over ENet/UDP directly to the server.
- **Web**: browsers can't open raw UDP, so it connects over WebSocket to
  the server's optional `--ws-port` proxy. Both client kinds share the
  same world and the same wire protocol.

## Accounts and guests

- Players sign in with a name and password at a separate auth service and join with a signed ticket; the game server
  checks the ticket, never the password. The name shown is the account's name.
- A server started with `--allow-guests` also lets players in without an account, named `"Guest <name>"`. A guest has
  no identity and can't be banned; what a guest may do (build, chat) is a set of bools in `Server/src/env.hpp`.
- Bans sit on the account. A banned account can't log in or refresh, but a ticket already issued works until it
  expires (up to an hour).
- A logged-in player who joins again elsewhere replaces their old session.

## Non-goals (don't add without asking)

- No per-player persistence in the game server — accounts live in the separate auth service. The game server saves the
  world (chunk files + `meta.bin`: seed, time of day, next object id) under `--save-path`, but not players, bullets or
  identities.
- No client-side authority over hits or position — the server is the
  source of truth.
