#include "home.hpp"
#include "GameState/gameState.hpp"
#include "UI/Button/button.hpp"

#include <raylib.h>

Home::Home() {
  // play button
  Button playButton = Button([](void) {
    GameState::shared().setMenuState(MenuState::PLAYING);
    DisableCursor(); // mouselook takes over
  },
                             "Play", (float)GetScreenHeight() / 2);
  buttons.push_back(playButton);

  // settings button

  Button settingsButton = Button([](void) {
    GameState::shared().setMenuState(MenuState::SETTINGS);
    EnableCursor(); // just in case :)
  },
                                 "Settings", (float)GetScreenHeight() / 2 + 100);
  buttons.push_back(settingsButton);
}

void Home::frame() {
  // Everything, including "Loading...", has to be drawn between Begin/EndDrawing to reach the screen.
  BeginDrawing();
  ClearBackground({5, 5, 5, 255});

  DrawText("Sandbox", (GetScreenWidth() - MeasureText("Sandbox", 60)) / 2, 100, 60, RED);
  DrawText("Network", (GetScreenWidth() - MeasureText("Network", 50)) / 2, 170, 50, BLUE);

  Vector2 mouse = GetMousePosition();
  bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
  for (auto &b : buttons) {
    b.frame(mouse, clicked);
  }

  EndDrawing();
}
