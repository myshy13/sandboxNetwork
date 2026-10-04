#include <raylib.h>

#include <iostream>

#include "AssetManager/manager.hpp"
#include "Game/game.hpp"
#include "GameState/gameState.hpp"
#include "Home/home.hpp"
#include "Protocol/protocol.hpp"
#include "Settings/settings.hpp"
#include "env.hpp"

int main() {
#ifndef __EMSCRIPTEN__
  // Native: asset paths are relative to the binary, not the shell.
  ChangeDirectory(GetApplicationDirectory());
#endif

  GameState& gameState = GameState::shared();

  // Suppress raylib's unnecessary logging levels.
  SetTraceLogLevel(LOG_WARNING);

  std::cout << "Game version: " << env::VERSION << "\n";
  std::cout << "protocol version: " << proto::PROTOCOL_VERSION << "\n";

#ifndef __EMSCRIPTEN__

  // -------------------------------------------------------------------------
  // Native
  //
  // Preserve the existing native behaviour:
  // - Resizable
  // - HiDPI
  // - VSync
  // - Resize to the current monitor
  // -------------------------------------------------------------------------
  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI | FLAG_VSYNC_HINT);

#else

  // -------------------------------------------------------------------------
  // Web / Emscripten
  //
  // Let the browser/HTML canvas control the display size.
  // Avoid FLAG_WINDOW_HIGHDPI and FLAG_VSYNC_HINT here because browser
  // compositors already control presentation timing, and HiDPI can result
  // in a much larger WebGL framebuffer than the visible canvas.
  // -------------------------------------------------------------------------
  SetConfigFlags(FLAG_WINDOW_RESIZABLE);

#endif

  std::cout << "Create window\n";

  InitWindow(1280, 720, ("Sandbox Network - " + env::VERSION).c_str());

#ifndef __EMSCRIPTEN__

  // Native: preserve the existing fullscreen-ish monitor-sized behaviour.
  int mw = GetMonitorWidth(GetCurrentMonitor());
  int mh = GetMonitorHeight(GetCurrentMonitor());

  SetWindowSize(mw, mh);
  SetWindowPosition(0, 0);

#endif

  SetExitKey(KEY_F12);

#ifdef __EMSCRIPTEN__

  // Do not let raylib introduce an additional frame-rate cap.
  // The browser's requestAnimationFrame/main loop controls presentation.
  SetTargetFPS(0);

#else

  // Native behaviour is unchanged; FLAG_VSYNC_HINT controls presentation.
  // No explicit target FPS is necessary.

#endif

  // Own GPU resources, so this scope ends (and they unload) before
  // CloseWindow() destroys the GL context.
  {
    AssetManager assets;
    Game game(assets);
    Home home;
    Settings settings;

    EnableCursor();

    while (!WindowShouldClose()) {
      if (gameState.getMenu() == MenuState::PLAYING) {
        game.frame();
      } else if (gameState.getMenu() == MenuState::HOME) {
        home.frame();
      } else {
        settings.frame();
      }
    }
  }

  CloseWindow();
  return 0;
}

#ifdef __EMSCRIPTEN__

extern "C" {

void resize(int w, int h) { SetWindowSize(w, h); }
}

#endif
