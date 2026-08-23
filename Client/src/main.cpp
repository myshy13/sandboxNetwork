#include "Client/client.hpp"
#include "GameState/gameState.hpp"
#include "Player/player.hpp"
#include <raylib.h>
#include <raymath.h>

bool paused = false;

int main() {
  const int screenWidth  = 1280;
  const int screenHeight = 720;

  GameState &gameState = GameState::shared();

  int playerPosCooldown = 1;

  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI);
  InitWindow(screenWidth, screenHeight, "Sandbox Network");
  SetTargetFPS(60);

  SetExitKey(KEY_NULL);

  Camera3D camera;
  camera.position   = {10.0f, 10.0f, 10.0f};
  camera.target     = {0.0f, 0.0f, 0.0f};
  camera.up         = {0.0f, 1.0f, 0.0f};
  camera.fovy       = 70.0f;
  camera.projection = CAMERA_PERSPECTIVE;

  Player player;
  Client client;

  while (!WindowShouldClose()) {
    float dt = GetFrameTime();
    // ==== Server communication
    client.poll();
    if (auto pos = client.takeRespawn()) {
      player.setPosition(*pos);
      client.sendPlayerPosition(player.getTransform(), player.getPitch(), player.getYaw());
      player.UpdateCamera(camera);
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
      paused = !paused;
      if (paused) {
        EnableCursor();
      } else {
        DisableCursor();
      }
    }

    if (!paused) {
      playerPosCooldown--;
      if (playerPosCooldown <= 0) {
        client.sendPlayerPosition(player.getTransform(), player.getPitch(), player.getYaw());
        playerPosCooldown = 3;
      }

      if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        client.createBullet();
      }
    }

    // ==== Update ====
    if (!paused) {
      player.Update(dt, camera);
    }
    // ==== Draw ====
    BeginDrawing();
    ClearBackground({5, 5, 5, 255});
    BeginMode3D(camera);
    // ==== draw online players
    for (auto &p : client.getPlayers()) {
      Transform transform;
      transform.rotation    = QuaternionFromEuler(0, p.yaw, 0);
      transform.scale       = {1, 10, 1};
      transform.translation = p.pos;
      player.DrawPlayer(transform);
    }
    client.updateBullets(dt);
    for (auto &b : client.getBullets()) {
      DrawCubeV(b.pos, {0.7f, 0.7f, 0.7f}, RED);
      DrawCubeWiresV(b.pos, {0.7f, 0.7f, 0.7f}, BLACK);
    }
    player.Draw();
    DrawGrid(40, 10);
    EndMode3D();

    // ==== health bar ==== //
    constexpr int MAX_HEALTH = 3; // matches PLAYER_MAX_HEALTH on the server
    for (int i = 0; i < MAX_HEALTH; i++) {
      Color fill = i < client.getHealth() ? RED : Color{60, 60, 60, 255};
      DrawRectangle(20 + i * 44, 20, 40, 16, fill);
    }

    if (!client.isConnected()) {
      const char *msg = GetTime() < 5.0 ? "Connecting..." : "Failed to connect to server";
      Color col       = GetTime() < 5.0 ? WHITE : RED;
      DrawText(msg, GetScreenWidth() / 2 - MeasureText(msg, 30) / 2, 60, 30, col);
    }

    if (paused) {
      DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {50, 50, 50, 50});

      DrawText("Paused", GetScreenWidth() / 2 - MeasureText("Paused", 50) / 2, GetScreenHeight() / 2 - 25, 50, WHITE);
    } else if (gameState.damageFlashTimer > 0) {
      DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {100, 10, 10, 50});
      gameState.damageFlashTimer -= dt;
    } else if (gameState.greenFlashTimer > 0) {
      DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {10, 100, 10, 50});
      gameState.greenFlashTimer -= dt;
    } else {
      // ==== draw crosshair ==== //
      Vector2 centre = {(float)GetScreenWidth() / 2, (float)GetScreenHeight() / 2};
      DrawCircleV(centre, (float)GetScreenHeight() / 1080, WHITE);
    }

    EndDrawing();
  }

  CloseWindow();
  return 0;
}