#include "Game/game.hpp"
#include "GameState/gameState.hpp"
#include "Menu/menu.hpp"
#include "Protocol/protocol.hpp"
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
  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI);
  std::cout << "Create window\n";
  InitWindow(1280, 720, std::string("Sandbox Network - " + std::string(VERSION)).c_str());
  SetExitKey(KEY_NULL);

  Game game;
  Menu menu;
  EnableCursor(); // Player's constructor captured it; the menu needs a pointer

  while (!WindowShouldClose()) {
    if (gameState.getMenu() == MenuState::PLAYING) {
      game.frame();
    } else {
      menu.draw();
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
