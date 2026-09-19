#include "menu.hpp"
#include "GameState/gameState.hpp"

#include <raylib.h>

void Menu::frame() {
  constexpr int BUTTON_WIDTH  = 400;
  constexpr int BUTTON_HEIGHT = 80;
  constexpr int FONT_SIZE     = 50;

  Rectangle button = {GetScreenWidth() / 2.0f - BUTTON_WIDTH / 2.0f, (float)GetScreenHeight() / 2, BUTTON_WIDTH, BUTTON_HEIGHT};
  bool hovered     = CheckCollisionPointRec(GetMousePosition(), button);
  bool clicked     = hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

  Rectangle settingsButton = {GetScreenWidth() / 2.0f - BUTTON_WIDTH / 2.0f, (float)GetScreenHeight() / 2 + 100, BUTTON_WIDTH, BUTTON_HEIGHT};
  bool settingsHovered     = CheckCollisionPointRec(GetMousePosition(), settingsButton);
  bool settingsClicked     = settingsHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

  // Everything, including "Loading...", has to be drawn between Begin/EndDrawing to reach the screen.
  BeginDrawing();
  ClearBackground({5, 5, 5, 255});

  DrawText("Sandbox", (GetScreenWidth() - MeasureText("Sandbox", 60)) / 2, 100, 60, RED);
  DrawText("Network", (GetScreenWidth() - MeasureText("Network", 50)) / 2, 170, 50, BLUE);

  const char *label = clicked ? "Loading..." : "Play";
  if (!clicked) {
    DrawRectangleRec(button, hovered ? LIGHTGRAY : GRAY);
  }
  DrawText(label,
           (int)button.x + (BUTTON_WIDTH - MeasureText(label, FONT_SIZE)) / 2,
           (int)button.y + (BUTTON_HEIGHT - FONT_SIZE) / 2,
           FONT_SIZE, WHITE);

  const char *settingsLabel = settingsClicked ? "Loading..." : "Settings";
  if (!settingsClicked) {
    DrawRectangleRec(settingsButton, settingsHovered ? LIGHTGRAY : GRAY);
  }
  DrawText(settingsLabel,
           (int)settingsButton.x + (BUTTON_WIDTH - MeasureText(settingsLabel, FONT_SIZE)) / 2,
           (int)settingsButton.y + (BUTTON_HEIGHT - FONT_SIZE) / 2,
           FONT_SIZE, WHITE);
  EndDrawing();

  if (clicked) {
    GameState::shared().setMenuState(MenuState::PLAYING);
    DisableCursor(); // mouselook takes over
  } else if (settingsClicked) {
    GameState::shared().setMenuState(MenuState::SETTINGS);
    EnableCursor(); // just in case :)
  }
}
