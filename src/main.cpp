#include "raylib.h"

int main() {
  const int screenWidth = 1280;
  const int screenHeight = 720;

  InitWindow(screenWidth, screenHeight, "game-project");
  SetTargetFPS(60);

  Camera3D camera;
  camera.position = {10.0f, 10.0f, 10.0f};
  camera.target = {0.0f, 0.0f, 0.0f};
  camera.up = {0.0f, 1.0f, 0.0f};
  camera.fovy = 45.0f;
  camera.projection = CAMERA_PERSPECTIVE;

  while (!WindowShouldClose()) {
    BeginDrawing();
    ClearBackground(RAYWHITE);

    BeginMode3D(camera);
    DrawGrid(20, 1.0f);
    EndMode3D();

    DrawFPS(10, 10);
    EndDrawing();
  }

  CloseWindow();
  return 0;
}