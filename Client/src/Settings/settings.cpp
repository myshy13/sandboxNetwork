#include "settings.hpp"
#include "GameState/gameState.hpp"
#include <raylib.h>

void Settings::frame() {
  BeginDrawing();
  ClearBackground(BLACK);
  Rectangle exitButtonRec = {10, 10, 60, 60};
  if (CheckCollisionPointRec(GetMousePosition(), exitButtonRec)) {
    DrawRectangleRec(exitButtonRec, LIGHTGRAY);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      gameState.setMenuState(MenuState::HOME);
    }
  } else {
    DrawRectangleRec(exitButtonRec, GRAY);
  };
  DrawText("<", 37 - MeasureText("<", 50) / 2, 15, 50, WHITE);
  EndDrawing();
}