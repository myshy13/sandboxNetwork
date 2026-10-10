#include "home.hpp"

#include <raylib.h>

#include "GameState/gameState.hpp"
#include "Input/input.hpp"
#include "Input/inputState.hpp"
#include "UI/Button/button.hpp"

Home::Home(Input& input) : input(input) {
  // play button
  Button playButton = Button(
      [&input](void) {
        GameState::shared().setMenuState(MenuState::PLAYING);
        input.setMouseLook(true);
      },
      "Play", (float)GetScreenHeight() / 2);
  buttons.push_back(playButton);

  // settings button

  Button settingsButton = Button(
      [&input](void) {
        GameState::shared().setMenuState(MenuState::SETTINGS);
        input.setMouseLook(false);
      },
      "Settings", (float)GetScreenHeight() / 2 + 100);
  buttons.push_back(settingsButton);
}

void Home::frame() {
  // Everything, including "Loading...", has to be drawn between
  // Begin/EndDrawing to reach the screen.
  BeginDrawing();
  ClearBackground({5, 5, 5, 255});

  DrawTextFont("Sandbox",
               (GetScreenWidth() - MeasureTextFont("Sandbox", 60)) / 2, 100, 60,
               RED);
  DrawTextFont("Network",
               (GetScreenWidth() - MeasureTextFont("Network", 50)) / 2, 170, 50,
               BLUE);

  Vector2 mouse = input.getPointer();
  bool clicked = input.pressed(Action::Click);
  for (auto& b : buttons) {
    b.frame(mouse, clicked);
  }

  EndDrawing();
}
