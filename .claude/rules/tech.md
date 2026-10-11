# Tech stack

- **Language**: C++20, both Client and Server.
- **Build**: CMake 3.20+, dependencies fetched via `FetchContent` at
  configure time (not vendored) — first configure needs network access.
- **Rendering/windowing**: raylib 5.5.
- **Native transport**: ENet 1.3.18 (reliable UDP).
- **iOS**: the `ghera/raylib-ios` fork (raylib on ANGLE; needs its PR #1 UIScene fix on iOS 27), built with CMake's Xcode
  generator: `cmake -S Client -B Client/build-ios -G Xcode -DCMAKE_SYSTEM_NAME=iOS -DRAYLIB_IOS_DIR=<fork> -DIOS_TEAM=<id>
  -DIOS_BUNDLE_ID=<id>` (see `iOS.md`). Same ENet transport as native; iOS owns the main loop (`ios_*` in `main.cpp`).
- **tvOS** (experimental): same fork and generator with `-DCMAKE_SYSTEM_NAME=tvOS` (`-DCMAKE_OSX_SYSROOT=appletvsimulator` for the
  simulator). `BUILD_TVOS` implies `BUILD_IOS`. Native OpenGLES, no ANGLE (`PLATFORM_TVOS`, never alongside `PLATFORM_IOS`);
  the backend (`Client/tvos/`: backend source, `tvos.patch`, `apply.sh`, see its README) is patched into a copy of the
  fork's `src` in the build dir at configure time, so any stock checkout at `d1aedbd6` works. Input is
  `input_gamepad.cpp` over raylib's gamepad API, which the backend fills from GameController (extended gamepads only; the
  Siri Remote is not read). `-DUSE_GAMEPAD_INPUT=ON` compiles that backend on any platform, for testing; on macOS GLFW
  only treats a pad as a gamepad if its GUID is in its mapping table, so newer firmware reads as all zeros until
  `SetGamepadMappings` is given an up-to-date SDL_GameControllerDB.
  `CMAKE_OSX_DEPLOYMENT_TARGET` is a cache variable: change it with `-D`, editing `CMakeLists.txt` alone won't take.
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
  `constexpr` tunables (tick rate, bullet speed/lifetime, save period, day length).
- `SHADOWS_DEFAULT` (1 native, 0 web) is a compile definition set next to the native/web switch in
  `Client/CMakeLists.txt` and read as a plain value in `GameState`, so no platform `#ifdef` reaches game code.
  `RENDER_SCALE_DEFAULT` (2.0 desktop, 1.0 tvOS) follows the same pattern for the scene render-target scale. On tvOS the
  Xcode Debug configuration is built `-O3` too (`CMAKE_*_FLAGS_DEBUG`), since `-O0` is unusable on an Apple TV HD.
- Shaders live in `Client/assets/shaders/glsl330` and `glsl100` (web); a change to one needs the same change in the other.
- Web raylib is built with `OPENGL_VERSION="ES 3.0"` (`Client/CMakeLists.txt`): the default GLES2 rlgl calls null
  instancing/VAO extension pointers inside the WebGL2 context the link flags ask for. The web shaders are still GLSL ES 1.00.

## Testing

- `Shared/Protocol` has a standalone round-trip test
  (`cd Shared/Protocol && make test`) that needs no client or server
  running. It reuses headers already fetched into `Client/build`, so
  build the native client at least once first.
- `cd Server && make test` runs the chunk-key and `SaveMeta` round-trip (`Server/src/Terrain/chunk.hpp`), terrain and
  `FluidSim` tests.
  They need no ENet and no running server, but reuse cereal/raylib headers from `Client/build`. `FluidSim` is
  tested through a stand-in `FluidWorld` (`Server/test_fluid.cpp`); keep new server logic testable the same way.
