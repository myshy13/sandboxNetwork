#include "menu.hpp"
#include "GameState/gameState.hpp"

#include <raylib.h>

void Menu::draw() {
  constexpr int BUTTON_WIDTH  = 400;
  constexpr int BUTTON_HEIGHT = 80;
  constexpr int FONT_SIZE     = 50;

  DrawText("Sandbox", (GetScreenWidth() - MeasureText("Sandbox", 60)) / 2, 100, 60, RED);
  DrawText("Network", (GetScreenWidth() - MeasureText("Network", 50)) / 2, 170, 50, BLUE);

  Rectangle button = {GetScreenWidth() / 2.0f - BUTTON_WIDTH / 2.0f, (float)GetScreenHeight() / 2, BUTTON_WIDTH, BUTTON_HEIGHT};
  bool hovered     = CheckCollisionPointRec(GetMousePosition(), button);

  BeginDrawing();
  ClearBackground({5, 5, 5, 255});
  DrawRectangleRec(button, hovered ? LIGHTGRAY : GRAY);
  DrawText("Play",
           (int)button.x + (BUTTON_WIDTH - MeasureText("Play", FONT_SIZE)) / 2,
           (int)button.y + (BUTTON_HEIGHT - FONT_SIZE) / 2,
           FONT_SIZE, WHITE);
  EndDrawing();

  if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    DrawText("Loading...", GetScreenWidth() / 2 - MeasureText("Loading...", 50), GetScreenHeight() / 2 - 25, 50, WHITE);
    GameState::shared().setMenuState(MenuState::PLAYING);
    DisableCursor(); // mouselook takes over
  }
}
