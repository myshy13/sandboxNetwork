#include "Player/player.hpp"
#include <raylib.h>

int main() {
  const int screenWidth  = 1280;
  const int screenHeight = 720;

  InitWindow(screenWidth, screenHeight, "game-project");
  SetTargetFPS(60);

  SetWindowSize(GetMonitorWidth(0), GetMonitorHeight(0));
  SetWindowPosition(0, 0);

  Camera3D camera;
  camera.position   = {10.0f, 10.0f, 10.0f};
  camera.target     = {0.0f, 0.0f, 0.0f};
  camera.up         = {0.0f, 1.0f, 0.0f};
  camera.fovy       = 70.0f;
  camera.projection = CAMERA_PERSPECTIVE;

  Player player;

  while (!WindowShouldClose()) {
    // ==== Update ====
    float dt = GetFrameTime();
    player.Update(dt, camera);
    // ==== Draw ====
    BeginDrawing();
    ClearBackground({5, 5, 5, 255});

    BeginMode3D(camera);
    player.Draw();
    DrawGrid(40, 10);
    EndMode3D();

    EndDrawing();
  }

  CloseWindow();
  return 0;
}