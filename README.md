# sandboxNetwork

A small multiplayer 3D shooter: a raylib client, an authoritative server, and a
shared binary protocol between them.

Native clients talk to the server over **ENet/UDP**. Browsers can't open raw UDP
sockets, so the web build talks **WebSocket** to an optional proxy running inside
the same server process. Both kinds of client land in the same game world.

```
   native client  ──UDP/ENet────────►┐
                                     ├──►  server (players, bullets, hit detection)
   web client     ──TCP/WebSocket───►┘
                     (--ws-port)
```

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

## Quick start

```bash
# terminal 1 - server, with browser support on
cmake -S Server -B Server/build && cmake --build Server/build
./Server/build/sandboxNetworkServer --ws-port 9798

# terminal 2 - native client
cmake -S Client -B Client/build && cmake --build Client/build
./Client/build/sandboxNetwork
```

---

## Server

```bash
cmake -S Server -B Server/build
cmake --build Server/build
```

Binary: `Server/build/sandboxNetworkServer`

```bash
./Server/build/sandboxNetworkServer                  # ENet only
./Server/build/sandboxNetworkServer --ws-port 9798   # also accept browsers
./Server/build/sandboxNetworkServer --help
```

Without `--ws-port` the WebSocket proxy never starts, and browser clients can't
connect. The server always listens for ENet on **UDP 9798**.

> **Use `--ws-port 9798`.** It looks like a clash with the ENet port, but TCP and
> UDP port numbers are separate namespaces, so they coexist. The web client
> currently dials the same port number it uses for ENet, so matching them means
> no code change.

The server binds `ENET_HOST_ANY`, so other devices on your network can reach it.
macOS may prompt to allow incoming connections the first time.

## Native client

```bash
cmake -S Client -B Client/build
cmake --build Client/build
```

Binary: `Client/build/sandboxNetwork`

The server address is compiled in. To point it somewhere else, edit `SERVER_IP`
in [`Client/src/Client/client.hpp`](Client/src/Client/client.hpp):

```cpp
#define SERVER_IP "192.168.10.111"
```

## Web client (Emscripten)

```bash
emcmake cmake -S Client -B Client/build-web -DBUILD_WEB=ON
cmake --build Client/build-web
```

Output: `Client/build-web/sandboxNetwork.{html,js,wasm,data}`

`emcmake` sets `EMSCRIPTEN`, which turns on `BUILD_WEB`, which makes CMake skip
ENet and compile `transport_ws.cpp` in place of `transport_enet.cpp`. That build
switch is the *only* place the native/web split exists — there are no platform
`#ifdef`s in the source.

Browsers refuse to load wasm over `file://`, so serve the directory:

```bash
cd Client/build-web && python3 -m http.server 8080
```

Then open <http://localhost:8080/sandboxNetwork.html>, with the server running
as `--ws-port 9798`.

> Serve over plain `http://`. The client connects with `ws://`, and browsers
> block that from an `https://` page as mixed content — TLS would need `wss://`
> and a certificate.

## Protocol tests

The wire format has a standalone round-trip check that needs neither client nor
server:

```bash
cd Shared/Protocol && make test
```

It reuses headers already fetched into `Client/build`, so build the native client
at least once first.

## Layout

```
Client/src/
  Client/      network-facing game client
  Net/         transport interface + one file per backend
  Player/      movement, camera, drawing
Server/src/
  Server/      game loop, hit detection, bullet lifetime
  Net/         Connection interface + the WebSocket proxy
Shared/Protocol/
  protocol.hpp cereal message structs, pack/unpack
```

Both `Client` and `Server` compile `Shared/Protocol/protocol.cpp` directly, so
the two sides can never drift out of sync on the wire format.

## Troubleshooting

**`Failed to create ENet server host`** — port 9798 is already taken, usually by
a server you forgot to stop:

```bash
lsof -i :9798 -t | xargs kill
```

**Web configure fails with "does not match the source ... used to generate
cache"** — a stale `CMakeCache.txt` from an older layout. Delete the build
directory and configure again; it only holds build output.

**Server dies when you close its terminal** — zsh sends `SIGHUP` to background
jobs. Run it in its own window, or `nohup ./Server/build/sandboxNetworkServer &`.

**Browser client connects but nothing happens** — the server almost certainly
isn't running `--ws-port`. Without it there is no WebSocket listener at all.
