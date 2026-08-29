# sandboxNetwork

A small multiplayer 3D shooter, built to learn how multiplayer games fit
together: a raylib client (native **and** browser), one authoritative server, and
a shared binary protocol between them.

Native clients talk to the server over **ENet/UDP**. Browsers can't open raw UDP
sockets, so the web build talks **WebSocket** to an optional proxy running inside
the same server process. Both kinds of client land in the same game world.

```
   native client  ──UDP/ENet────────►┐
                                     ├──►  server (players, bullets, hit detection)
   web client     ──TCP/WebSocket───►┘
                     (--ws-port)
```

## Features

- **Movement** — WASD + mouselook, Space to jump, Shift to sprint, `C` toggles
  first/third person.
- **Shooting** — left click fires a bullet along the camera's aim. The *server*
  decides hits: a bullet swept through another player's box kills the bullet and
  damages that player (you can't hit yourself). Bullets expire after a few
  seconds if they hit nothing.
- **Health & respawn** — 3 HP. The screen flashes red when you're hit, green when
  you land a hit; at 0 HP the server respawns you.
- **Chat** — press `T` (or `/`) to open the chat box. The world keeps simulating
  while you type. Commands: `/setname <name>`, `/clear`.
- **Names** — `/setname` broadcasts a display name that shows in chat.
- **Pause** — `Esc` frees the cursor; `Esc` again resumes.

State that matters (positions, bullets, hits, health) is owned by the server.
Position updates are best-effort; connect/disconnect, bullet spawn, hits and chat
are reliable. There's no persistence — restart the server and the world resets.

## Prerequisites

| | |
|---|---|
| CMake | 3.20+ |
| Compiler | anything with C++20 support |
| Git | needed at configure time — dependencies are fetched, not vendored |
| Emscripten | only for the web client (`brew install emscripten`) |

Dependencies download themselves on first configure via CMake `FetchContent`, so
the first build of each target is slow and needs a network connection:

- **raylib 5.5** — rendering, and the shared math/`Vector3` types
- **ENet 1.3.18** — reliable UDP (skipped entirely for the web build)
- **cereal 1.3.2** — binary serialisation, header-only
- **IXWebSocket 11.4.5** — server only, backs the browser proxy

## Configuration

The client reads connection settings from `Client/src/env.hpp`, which is
git-ignored. Copy the template before the first build:

```bash
cp Client/src/env.example.hpp Client/src/env.hpp
```

| define | meaning |
|---|---|
| `SERVER_IP` | server hostname or IP, no scheme (e.g. `"127.0.0.1"`) |
| `SERVER_PORT` | server port (ENet and WebSocket share the number) |
| `SERVER_WSS` | web build only — use `wss://` instead of `ws://` (needed when the page is served over https) |
| `DEBUG` | show an FPS counter |
| `CHEATS` | enable aimbot / rapid-fire (off by default) |

## Build & run

### Server

```bash
cmake -S Server -B Server/build
cmake --build Server/build
./Server/build/sandboxNetworkServer --ws-port 9798   # omit --ws-port for ENet only
```

Without `--ws-port` the WebSocket proxy never starts and browsers can't connect.
The server always listens for ENet on **UDP 9798**, and binds `ENET_HOST_ANY` so
other devices on the network can reach it. Using `9798` for both is fine — TCP
and UDP port numbers are separate namespaces.

### Native client

```bash
cmake -S Client -B Client/build
cmake --build Client/build
./Client/build/sandboxNetwork
```

### Web client (Emscripten)

```bash
emcmake cmake -S Client -B Client/build-web -DBUILD_WEB=ON
cmake --build Client/build-web
cd Client/build-web && python3 -m http.server 8080
```

Then open <http://localhost:8080/sandboxNetwork.html> with the server running
`--ws-port 9798`. `emcmake` sets `EMSCRIPTEN`, which turns on `BUILD_WEB`, which
makes CMake skip ENet and compile `transport_ws.cpp` instead of
`transport_enet.cpp` — that build switch is the *only* native/web split; there
are no platform `#ifdef`s in game code.

Serve over plain `http://`: the client connects with `ws://`, which browsers
block from an `https://` page as mixed content (that would need `wss://` and a
certificate — see `SERVER_WSS`).

### Protocol round-trip test

```bash
cd Shared/Protocol && make test
```

Reuses headers already fetched into `Client/build`, so build the native client at
least once first.

## Layout

```
Client/src/
  main.cpp       window, game loop, HUD (chat, crosshair, damage flash)
  Client/        network-facing client: send/receive, player + bullet lists, chat, health
  Net/           transport interface + one file per backend (transport_enet.cpp / transport_ws.cpp)
  Player/        local player movement, camera, first/third-person drawing
  Entity/        shared drawable/entity base
  GameState/     small global for screen-flash timers
  env.hpp        connection config (git-ignored; copy from env.example.hpp)

Server/src/
  main.cpp       CLI args (--ws-port), owns the Server
  Server/        game loop (Server::tick): hit detection, bullet lifetime, player bookkeeping
  Net/           Connection interface + WsProxy (browser WebSocket bridge)

Shared/Protocol/
  protocol.hpp   cereal message structs (PlayerUpdate, NewBullet, PlayerHit, ChatMessage, ...)
  protocol.cpp   pack/unpack
```

Both `Client` and `Server` compile `Shared/Protocol/protocol.cpp` directly (not a
linked library), so the two sides can never drift out of sync on the wire format.
