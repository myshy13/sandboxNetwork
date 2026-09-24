# Architecture / folder layout

```
Client/src/
  Client/      network-facing game client (Client class: send/receive, player+bullet lists; connect() starts a fresh session)
  Net/         transport interface + one file per backend (transport_enet.cpp / transport_ws.cpp)
  Player/      local player movement, camera, drawing
  Game/Entity/ shared drawable/entity helpers
  Game/        Game class: owns every subsystem + per-frame state, frame() = update then draw
  World/       client-side blocks: occupied-cell collision lookup, per-chunk index, dirty chunks
  Renderer/    chunked frustum culling + instanced block drawing (only dirty chunks rebuild)
  Home/        home menu screen (Home class: owns its Buttons, per-frame draw)
  UI/          reusable widgets, one folder each (Button/: hover, click, draw, runs its handler)
  Settings/    settings screen (reached from the menu; writes values into GameState)
  AssetManager/ owns loaded assets (textures now); enum-indexed get(); built in main.cpp, destroyed before CloseWindow()
  GameState/   singleton shared by menu/settings/game: current MenuState + user settings (e.g. render distance)
main.cpp       opens nothing itself: constructs Game and calls Game::frame() in a loop

Server/src/
  Server/      game loop (Server::tick), hit detection, bullet lifetime, player bookkeeping, per-client chunk views
               (updateView / sendChunk / broadcastToChunk); chunk.hpp = chunk key packing (tested), terrain.hpp = seeded terrain (WIP)
  Net/         Connection interface + WsProxy (browser WebSocket bridge)
main.cpp       CLI args (--ws-port), owns the Server instance

Shared/
  sharedEnv.hpp  constants both sides must agree on (SHARED_PLAYER_SCALE)
  Protocol/
    protocol.hpp   cereal message structs (PlayerUpdate, NewBullet, DeleteBullet, PlayerHit, ChunkData, ChunkUnload, SetViewRadius, ...)
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

The player's hitbox scale is defined once, as `SHARED_PLAYER_SCALE` in `Shared/sharedEnv.hpp`. The server's
`PLAYER_SCALE` (`Server/src/Server/server.cpp`) and the client's `env::PLAYER_SCALE` (`Client/src/env.hpp`, template in
`env.example.hpp`; `env.hpp` is git-ignored, so it must `#include "sharedEnv.hpp"` too) both alias it, so server hit
detection and the client's body, hitbox and placement check can't disagree. Change it in `sharedEnv.hpp` only.

`World::STREAM_CHUNK_SIZE` (`Client/src/World/world.hpp`) and the server's `CHUNK_SIZE`
(`Server/src/Server/server.cpp`) are the same streaming-chunk size (16 cells = 80 units), duplicated by hand;
the client's debug chunk borders and (later) chunk loading rely on them agreeing.

`GameState::MAX_RENDER_DISTANCE` (`Client/src/GameState/gameState.hpp`) must equal the server's `env::MAX_VIEW_RADIUS`
(`Server/src/env.hpp`) times `World::STREAM_CHUNK_SIZE`: the slider can't ask for more chunks than the server will send.

Likewise `env::MAX_HEALTH` (`Client/src/env.hpp`, template in
`env.example.hpp`) and the server's `env::PLAYER_MAX_HEALTH`
(`Server/src/env.hpp`): the client sizes its health bar from its copy while the
server decides when you die from its own.
