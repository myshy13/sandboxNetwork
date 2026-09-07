# Tech stack

- **Language**: C++20, both Client and Server.
- **Build**: CMake 3.20+, dependencies fetched via `FetchContent` at
  configure time (not vendored) — first configure needs network access.
- **Rendering/windowing**: raylib 5.5.
- **Native transport**: ENet 1.3.18 (reliable UDP).
- **Web transport**: Emscripten + WebSocket, proxied server-side through
  IXWebSocket 11.4.5.
- **Wire format**: cereal 1.3.2 (header-only binary serialisation) —
  message structs live in `Shared/Protocol/protocol.hpp`.

## Syntax / style rules

- Warnings are errors in intent, not enforced by the build: `-Wall -Wextra`
  on GCC/Clang, `/W4` on MSVC (`Client/CMakeLists.txt`). Don't introduce new
  warnings.
- raylib headers are included as system headers — don't "fix" raylib's own
  warnings, they're not ours to fix.
- No `#ifdef` platform splits in game code. The *only* native/web switch is
  which transport file gets compiled (`transport_enet.cpp` vs
  `transport_ws.cpp`, chosen in `Client/CMakeLists.txt` by whether
  `EMSCRIPTEN` is defined). If a change needs a platform branch anywhere
  else, that's a sign it belongs in the transport layer instead.
- Both Client and Server compile `Shared/Protocol/protocol.cpp` directly
  (not a linked library) — the two sides can never drift out of sync on
  the wire format. Keep it that way; don't split it into a separate
  build target.
- Prefer raylib/raymath vector helpers (`Vector3Add`, `Vector3Scale`, ...)
  over manual component math.
- Server position updates trust the connection, not the payload — a
  client can only speak for itself (see `PlayerUpdate` handling in
  `Server/src/Server/server.cpp`). Keep that pattern for any new
  client->server message that carries an id.
- `Client/src/env.hpp` holds connection config: `env::SERVER_IP` /
  `env::SERVER_PORT` as `constexpr` in `namespace env`, and the feature
  toggles (`SERVER_WSS`, `DEBUG`, `CHEATS`, `CHAT`) as `#define` because
  `#ifdef` needs them. `SERVER_IP` is a bare hostname (no scheme);
  `SERVER_WSS` toggles `wss://` vs `ws://` in `transport_ws.cpp` — define
  it when the page is served over https (e.g. a dev tunnel), since
  browsers block plain `ws://` from an https page as mixed content.
- `Server/src/env.hpp` mirrors this for the server: `namespace env` of
  `constexpr` tunables (tick rate, bullet speed/lifetime, save period).

## Testing

- `Shared/Protocol` has a standalone round-trip test
  (`cd Shared/Protocol && make test`) that needs no client or server
  running. It reuses headers already fetched into `Client/build`, so
  build the native client at least once first.
