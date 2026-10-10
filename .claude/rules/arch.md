# Architecture / folder layout

```
Client/src/
  Client/      network-facing game client (Client class: send/receive, player+bullet lists; connect() starts a fresh session)
  Net/         transport interface + one file per backend (transport_enet.cpp / transport_ws.cpp)
  Player/      local player movement, camera, drawing
  Game/Entity/ shared drawable/entity helpers
  Game/        Game class: owns every subsystem + per-frame state, frame() = update then draw
  World/       client-side blocks: occupied-cell collision lookup, per-chunk index, dirty chunks, and the world clock
               (timeOfDay 0-1, advanced per frame, reset by each SetTime)
  Renderer/    chunked frustum culling + instanced block drawing (only dirty chunks rebuild), and the sun's shadow-map
               pass (Renderer::shadowMap); faceMatrix is the one place a face's instance matrix is built
  Shaders/     Lighting: owns the lighting shader, its lights and ambient; timeToLight/skyColor turn the clock into the
               sun and sky; setShadow hands the sun's view-projection matrix + depth map to the shader
  Raylib/      small helpers around raylib's own API (shadowMap.hpp: depth-only render target)
  Input/       Input (polled once per frame in main.cpp's update(); owns an InputSource) + InputState (Action bitset, move,
               look, scroll, pointer, hotbar slot). InputSources/: one backend per platform, CMake compiles exactly one
               (input_kbm.cpp desktop + web, input_touch.cpp iOS: stick, look drag, on-screen buttons). Game code never
               calls raylib's input functions, except chat typing
  Home/        home menu screen (Home class: owns its Buttons, per-frame draw)
  UI/          reusable widgets, one folder each (Button/: hover, click, draw, runs its handler; menu or any-rectangle
               buttons. Slider/: int range tied to a value by a getter and a setter)
  Settings/    settings screen (reached from the menu; Buttons and Sliders that write values into GameState)
  AssetManager/ owns loaded assets (textures now); enum-indexed get(); built in main.cpp, destroyed before CloseWindow()
  GameState/   singleton shared by menu/settings/game: current MenuState + user settings (render distance, shadows
               on/off, shadow radius, interpolation)
main.cpp       opens nothing itself: constructs Game and calls Game::frame() in a loop

Server/src/
  Server/      game loop (Server::tick), hit detection, bullet lifetime, player bookkeeping, per-client chunk views
               (updateView / sendChunk / broadcastToChunk); blockHelpers.hpp = block size, cell keys
  Terrain/     chunk.hpp = chunk keys + chunk save files (tested), terrain.cpp = seeded terrain (tested)
  Fluid/       FluidSim: server-owned water flow, reaches the world only through FluidWorld (tested)
  Net/         Connection interface + WsProxy (browser WebSocket bridge)
main.cpp       CLI args (--ws-port, --save-path, --save-time, --seed, --time, --freeze-time, --max-players), owns the Server

Shared/
  sharedEnv.hpp  constants both sides must agree on (SHARED_PLAYER_SCALE)
  Protocol/
    protocol.hpp   cereal message structs (PlayerUpdate, NewBullet, DeleteBullet, PlayerHit, ChunkData, ChunkUnload, SetViewRadius, SetTime, ...)
    protocol.cpp   pack/unpack, compiled directly into both Client and Server
```

## Block types

`BlockType` and `BLOCK_INFO` (`Shared/Models/blocks.hpp`) are the one place a block's behaviour is defined: solid,
opaque (hides the faces behind it), translucent, placeable, fluid and base colour. A row names only the fields that
differ from the defaults, and a compile-time check fails the build if a row is missing or out of enum order. An `Object`
(`Shared/Models/Object.hpp`) stores no colour: `getColor()` is the type's colour, darkened a quarter per point of
damage, so server and client agree without sending it. It does store `state`, one byte each block type reads its own way
(water: flow level, 0 = source). The client's type-to-texture table is `BLOCK_TEX` in
`Client/src/AssetManager/blockTex.hpp`. Changing what is sent or saved for an `Object` bumps `PROTOCOL_VERSION` and
`env::saveFormatVersion`.

Adding a block:

1. Add the enum value before `Count` in `BlockType`.
2. Add its `BLOCK_INFO` row, in the same position.
3. Add a `Tex` entry, its path row and a `BLOCK_TEX` row, if it has a texture.
4. Bump `PROTOCOL_VERSION` (an old client would draw the new type as an invalid magenta block).
5. Optional: add it to `blockTypes` (`Client/src/Game/game.hpp`) if it should be on the hotbar. `placeable` only says the
   server accepts a place request; the hotbar is a separate choice of slots and order.

## State ownership

- **Server** is authoritative for: player position/pitch/yaw (as received,
  broadcast unreliably), bullet spawn/position/expiry, and hit detection.
  Server-side state lives on `Server` in `Server/src/Server/server.hpp`
  (`players`, `bullets`, `connections`).
- **Client** is authoritative for: its own local player's movement/camera
  (`Player` in `Client/src/Player/`), and purely cosmetic simulation of
  remote bullets between server updates (`Client::updateBullets`).
- **Time of day**: the server owns the clock (`Server::timeOfDay`, a 0-1 fraction of the day, saved in `meta.bin`).
  It sends `SetTime` (time, day length, frozen flag) on connect and every `env::TIME_BROADCAST_INTERVAL`; in between
  each client runs its own clock (`World::update`), so the sun is smooth and no per-frame traffic is needed. The day
  length travels in the message, so there is no client copy to keep in sync.
- Don't move authoritative game logic (hit detection, bullet lifetime,
  spawning) into the client — it renders and predicts, it doesn't decide.

## Sun and shadows (client only, cosmetic)

`Lighting::timeToLight(time)` is the one function that turns the clock into the sun (midday 0.5, sunrise 0.25, sunset
0.75); the lights, the sky colour and the shadow camera all derive from it. Shadows are one 2048x2048 depth map drawn from
the sun by `Renderer::shadowMap` (before `BeginTextureMode(target)`, since passes can't nest), centred on the player in
the same camera-relative space as everything else. The lighting shaders sample it and scale the sun's light, never the
ambient. `GameState::getShadows()` turns the pass and the shader lookup off; its default comes from `SHADOWS_DEFAULT`
in `Client/CMakeLists.txt`. The server never sees any of this.

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
