#include "AssetManager/manager.hpp"
#include "Game/game.hpp"
#include "GameState/gameState.hpp"
#include "Menu/menu.hpp"
#include "Protocol/protocol.hpp"
#include "Settings/settings.hpp"
#include "env.hpp"

#include <iostream>
#include <raylib.h>

int main() {
#ifndef __EMSCRIPTEN__
  ChangeDirectory(GetApplicationDirectory()); // asset paths are relative to the binary, not the shell
#endif

  GameState &gameState = GameState::shared();
  // suppresses raylib's unnecessary logging levels
  SetTraceLogLevel(LOG_WARNING);
  std::cout << "Game version: " << VERSION << "\n";
  std::cout << "protocol version: " << proto::PROTOCOL_VERSION << "\n";

  // The window (GL context) must exist before Game/Menu: Lighting and Renderer load GPU resources.
  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI | FLAG_VSYNC_HINT);
  std::cout << "Create window\n";
  // get Monitor
  InitWindow(1280, 720, std::string("Sandbox Network - " + std::string(VERSION)).c_str());

  int mw = GetMonitorWidth(GetCurrentMonitor());
  int mh = GetMonitorHeight(GetCurrentMonitor());
  SetWindowSize(mw, mh);
  SetWindowPosition(0, 0);
  SetExitKey(KEY_F12); // force exit button instead of esc

  // Own GPU resources, so this scope ends (and they unload) before CloseWindow() kills the GL context.
  {
    AssetManager assets; // declared before Game so it outlives it
    Game game(assets);
    Menu menu;
    Settings settings;
    EnableCursor(); // Player's constructor captured it; the menu needs a pointer

    while (!WindowShouldClose()) {
      if (gameState.getMenu() == MenuState::PLAYING) {
        game.frame();
      } else if (gameState.getMenu() == MenuState::HOME) {
        menu.frame();
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
void resize(int w, int h) {
  SetWindowSize(w, h);
}
}
#endif
