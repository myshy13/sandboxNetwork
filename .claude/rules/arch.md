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
               (updateView / sendChunk / broadcastToChunk); blockHelpers.hpp = block size, cell keys
  Terrain/     chunk.hpp = chunk keys + chunk save files (tested), terrain.cpp = seeded terrain (tested)
  Fluid/       FluidSim: server-owned water flow, reaches the world only through FluidWorld (tested)
  Net/         Connection interface + WsProxy (browser WebSocket bridge)
main.cpp       CLI args (--ws-port), owns the Server instance

Shared/
  sharedEnv.hpp  constants both sides must agree on (SHARED_PLAYER_SCALE)
  Protocol/
    protocol.hpp   cereal message structs (PlayerUpdate, NewBullet, DeleteBullet, PlayerHit, ChunkData, ChunkUnload, SetViewRadius, ...)
    protocol.cpp   pack/unpack, compiled directly into both Client and Server
```

## Block types

`BlockType` and `BLOCK_INFO` (`Shared/Models/Object.hpp`) are the one place a block's behaviour is defined: solid,
placeable, fluid and base colour. A new block is an enum value plus a row (one row per value, in enum order: a
misplaced row compiles but swaps two blocks' properties). An `Object` stores no colour: `getColor()` is the type's
colour, darkened a quarter per point of damage, so server and client agree without sending it. The client's
type-to-texture table is `BLOCK_TEX` in `Client/src/AssetManager/blockTex.hpp`. Changing what is sent or saved for an
`Object` bumps `PROTOCOL_VERSION` and `env::saveFormatVersion`.

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
   (`Client/src/Client/client.cpp`). Anything that changes blocks or chunks goes into the one `WorldEvent`
   queue, which `Game::applyNetworkUpdates` applies in arrival order. Don't add a queue per message type:
   draining them one after another reorders edits and leaves ghost blocks.
4. If it needs constant per-tick simulation, that's `Server::tick` on the
   server side and the per-frame loop in `Client/src/main.cpp` on the
   client side — don't invent a second update loop.

## Constants that must stay in sync

The player's hitbox scale is defined once, as `SHARED_PLAYER_SCALE` in `Shared/sharedEnv.hpp`. The server's
`PLAYER_SCALE` (`Server/src/Server/server.cpp`) and the client's `env::PLAYER_SCALE` (`Client/src/env.hpp`, template in
`env.example.hpp`; `env.hpp` is git-ignored, so it must `#include "sharedEnv.hpp"` too) both alias it, so server hit
detection and the client's body, hitbox and placement check can't disagree. Change it in `sharedEnv.hpp` only.

`World::STREAM_CHUNK_SIZE` (`Client/src/World/world.hpp`) and the server's `CHUNK_SIZE`
(`Server/src/Server/blockHelpers.hpp`) are the same streaming-chunk size (16 cells = 80 units), duplicated by hand;
the client's debug chunk borders and (later) chunk loading rely on them agreeing.

`GameState::MAX_RENDER_DISTANCE` (`Client/src/GameState/gameState.hpp`) must equal the server's `env::MAX_VIEW_RADIUS`
(`Server/src/env.hpp`) times `World::STREAM_CHUNK_SIZE`: the slider can't ask for more chunks than the server will send.

Likewise `env::MAX_HEALTH` (`Client/src/env.hpp`, template in
`env.example.hpp`) and the server's `env::PLAYER_MAX_HEALTH`
(`Server/src/env.hpp`): the client sizes its health bar from its copy while the
server decides when you die from its own.
