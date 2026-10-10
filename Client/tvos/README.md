# tvOS raylib backend

Makes a stock [ghera/raylib-ios](https://github.com/ghera/raylib-ios) checkout build for tvOS: native OpenGLES, no ANGLE.
You don't run anything by hand: with `-DCMAKE_SYSTEM_NAME=tvOS`, `Client/CMakeLists.txt` copies the fork's `src` into the
build directory and runs `apply.sh` on the copy. Your checkout is never modified, and a re-run of CMake re-patches only when
a file here is newer than the build's `.patched` stamp.

- `rcore_tvos.c`: EAGLContext + CAEAGLLayer backend; extended gamepads are polled from GameController into raylib's gamepad
  state (the Siri Remote is not read). The view controller is a `GCEventViewController` so Menu reaches the game.
- `tvos.patch`: `rcore.c` (backend select) and `rlgl.h` (OpenGLES include, `rlSetDefaultFramebuffer`). tvOS has no
  framebuffer 0, so rlgl's "back to the screen" binds the id the backend registered.
- `apply.sh <raylib src dir>`: copies the backend in and applies the patch; safe to re-run.

The patch is made against `d1aedbd6` ("Support iOS27"). If the fork moves and `apply.sh` stops applying cleanly, regenerate
`tvos.patch` against the new `rcore.c`/`rlgl.h` (the edits are small and described above).
