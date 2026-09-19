#include "settings.hpp"
#include "GameState/gameState.hpp"
#include "env.hpp"
#include <algorithm>
#include <raylib.h>
#include <string>
template <typename T>
T map(T x, T in_min, T in_max, T out_min, T out_max) {
  return out_min + (x - in_min) * (out_max - out_min) / (in_max - in_min);
}

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

  // ==== render distance slider ==== //
  // slider base
  const int currentRenderDistanceValue = gameState.getRenderDistance();
  const int screenDistance5th          = GetScreenWidth() / 5;
  int sliderX                          = map(currentRenderDistanceValue, 200, 1000, screenDistance5th, screenDistance5th * 4);
  if (changingRenderDistance) {
    sliderX = std::clamp(GetMouseX(), screenDistance5th, screenDistance5th * 4); // pixels
    gameState.setRenderDistance(map(sliderX, screenDistance5th, screenDistance5th * 4, 200, 1000));
  }
  Rectangle sliderRec = {static_cast<float>(sliderX - 10), 190, 20, 40};
  if (changingRenderDistance && IsMouseButtonUp(MOUSE_BUTTON_LEFT)) {
    changingRenderDistance = false;
  } else if (!changingRenderDistance && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec({sliderRec.x, static_cast<float>(GetMouseY())}, sliderRec)) {
    changingRenderDistance = true;
  }
  DrawText(std::string("Render distance:" + std::to_string(gameState.getRenderDistance() / 5)).c_str(), screenDistance5th, 150, 30, WHITE); // divided by 5 to match the block size
  DrawRectangle(screenDistance5th, 200, screenDistance5th * 3, 20, GRAY);
  DrawRectangleRec(sliderRec, WHITE);

  // ==== vsync toggle ==== //
  // raylib owns the flag, so ask it instead of mirroring the state in GameState
  const Rectangle vsyncRec = {static_cast<float>(screenDistance5th * 4) - 100, 280, 100, 50};
  const bool vsyncHovered  = CheckCollisionPointRec(GetMousePosition(), vsyncRec);
  if (vsyncHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    if (IsWindowState(FLAG_VSYNC_HINT)) {
      ClearWindowState(FLAG_VSYNC_HINT);
    } else {
      SetWindowState(FLAG_VSYNC_HINT);
    }
  }
  DrawRectangleRec(vsyncRec, vsyncHovered ? LIGHTGRAY : GRAY);
  DrawText("VSync:", screenDistance5th, 295, 30, WHITE);
  DrawText(IsWindowState(FLAG_VSYNC_HINT) ? "On" : "Off", screenDistance5th * 4 - vsyncRec.width * 0.7, 295, 30, WHITE);

  EndDrawing();
}