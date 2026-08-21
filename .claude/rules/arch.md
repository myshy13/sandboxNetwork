# Architecture / folder layout

```
Client/src/
  Client/      network-facing game client (Client class: send/receive, player+bullet lists)
  Net/         transport interface + one file per backend (transport_enet.cpp / transport_ws.cpp)
  Player/      local player movement, camera, drawing
  Entity/      shared drawable/entity helpers
main.cpp       InitWindow, game loop, draws online players + bullets

Server/src/
  Server/      game loop (Server::tick), hit detection, bullet lifetime, player bookkeeping
  Net/         Connection interface + WsProxy (browser WebSocket bridge)
main.cpp       CLI args (--ws-port), owns the Server instance

Shared/Protocol/
  protocol.hpp   cereal message structs (PlayerUpdate, NewBullet, DeleteBullet, PlayerHit, ...)
  protocol.cpp   pack/unpack, compiled directly into both Client and Server
```

## State ownership

- **Server** is authoritative for: player position/pitch/yaw (as received,
  broadcast unreliably), bullet spawn/position/expiry, and hit detection.
  Server-side state lives on `Server` in `Server/src/Server/server.hpp`
  (`players`, `bullets`, `connections`).
- **Client** is authoritative for: its own local player's movement/camera
  (`Player` in `Client/src/Player/`), and purely cosmetic simulation of
  remote bullets between server updates (`Client::updateBullets`).
- Don't move authoritative game logic (hit detection, bullet lifetime,
  spawning) into the client — it renders and predicts, it doesn't decide.

## Adding a new networked feature

1. Add the message struct to `Shared/Protocol/protocol.hpp` (and register
   it in `protocol.cpp` if pack/unpack needs it) — this is the one place
   both sides share, so this is where a new message type starts.
2. Server: handle it in `Server::handleReceive` (client -> server) or emit
   it from `Server::tick` / an event handler (server -> client).
3. Client: handle the reply in the client's receive path
   (`Client/src/Client/client.cpp`).
4. If it needs constant per-tick simulation, that's `Server::tick` on the
   server side and the per-frame loop in `Client/src/main.cpp` on the
   client side — don't invent a second update loop.

## Constants that must stay in sync

`PLAYER_SCALE` in `Server/src/Server/server.cpp` and the client's default
`Player` scale (`Client/src/Player/player.cpp`) are duplicated by hand and
must match, or server hit detection and client rendering disagree on the
player's hitbox. There's a comment at each site pointing at the other —
keep both in sync when either changes.
