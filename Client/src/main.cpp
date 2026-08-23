#include "Client/client.hpp"
#include "GameState/gameState.hpp"
#include "Player/player.hpp"
#include <raylib.h>
#include <raymath.h>
#include <string>
#include <vector>

bool paused = false;

int main() {
  int screenWidth  = 1280;
  int screenHeight = 720;
  bool inChat      = false;
  std::string chatInput;

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
      if (!inChat) {
        paused = !paused;
        if (paused) {
          EnableCursor();
        } else {
          DisableCursor();
        }
      } else {
        inChat = false;
        chatInput.clear();
      }
    }

    // ==== chat input ==== //
    // Only capture text once already open - otherwise the T that opens the
    // box would also land inside it as the first character typed.
    if (inChat) {
      int ch;
      while ((ch = GetCharPressed()) != 0) {
        // Printable ASCII only - matches Client::sendChatMessage's clamp.
        if (ch >= 32 && ch < 127 && chatInput.size() < 200) {
          chatInput += static_cast<char>(ch);
        }
      }
      if (IsKeyPressed(KEY_BACKSPACE) && !chatInput.empty()) {
        chatInput.pop_back();
      }
      if (IsKeyPressed(KEY_ENTER)) {
        if (!chatInput.empty()) {
          client.sendChatMessage(chatInput);
          chatInput.clear();
        }
        inChat = false;
      }
    } else if (IsKeyPressed(KEY_T)) {
      inChat = true;
      paused = false;
    }

    if (!paused && !inChat) {
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
    if (!paused && !inChat) {
      player.Update(dt, camera);
    }
    // ==== Draw ====
    {
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
    }

    // ==== health bar ==== //
    {
      constexpr int MAX_HEALTH = 3; // matches PLAYER_MAX_HEALTH on the server
      for (int i = 0; i < MAX_HEALTH; i++) {
        Color fill = i < client.getHealth() ? RED : Color{60, 60, 60, 255};
        DrawRectangle(20 + i * 44, 20, 40, 16, fill);
      }
    }

    if (!client.isConnected()) {
      const char *msg = GetTime() < 5.0 ? "Connecting..." : "Failed to connect to server";
      Color col       = GetTime() < 5.0 ? WHITE : RED;
      DrawText(msg, GetScreenWidth() / 2 - MeasureText(msg, 30) / 2, 60, 30, col);
    }

    // ==== chat (bottom left) ==== //
    {
      constexpr int VISIBLE_CHAT_MESSAGES    = 8;
      constexpr double CHAT_MESSAGE_LIFETIME = 10.0; // seconds
      constexpr int LINE_HEIGHT              = 22;
      constexpr int FONT_SIZE                = 18;
      const auto &chat                       = client.getChat();

      // Walk backward from the newest message. receivedAt is stamped with
      // GetTime() as each message arrives, so it only ever increases -
      // the first stale entry we hit means everything before it is stale too.
      std::vector<int> visible;
      for (int i = (int)chat.size() - 1; i >= 0 && (int)visible.size() < VISIBLE_CHAT_MESSAGES; i--) {
        if (GetTime() - chat[i].receivedAt > CHAT_MESSAGE_LIFETIME) {
          break;
        }
        visible.push_back(i);
      }

      // History sits above the input box, so leave room for it when open.
      int inputHeight = inChat ? LINE_HEIGHT + 6 : 0;
      int y           = GetScreenHeight() - 20 - inputHeight - LINE_HEIGHT * (int)visible.size();
      for (auto it = visible.rbegin(); it != visible.rend(); ++it) {
        const ChatEntry &entry = chat[*it];
        std::string line       = "Player " + std::to_string(entry.id) + ": " + entry.text;
        DrawRectangle(16, y - 2, MeasureText(line.c_str(), FONT_SIZE) + 8, LINE_HEIGHT, {0, 0, 0, 120});
        DrawText(line.c_str(), 20, y, FONT_SIZE, WHITE);
        y += LINE_HEIGHT;
      }

      if (inChat) {
        // ~2Hz blink, Minecraft/terminal-style.
        const char *cursor = (int)(GetTime() * 2) % 2 == 0 ? "_" : "";
        std::string prompt = "> " + chatInput + cursor;
        int boxY           = GetScreenHeight() - 20 - LINE_HEIGHT;
        DrawRectangle(16, boxY - 2, GetScreenWidth() - 32, LINE_HEIGHT + 4, {0, 0, 0, 160});
        DrawText(prompt.c_str(), 20, boxY, FONT_SIZE, WHITE);
      }
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

#ifdef __EMSCRIPTEN__
extern "C" {
void resize(int w, int h) {
  SetWindowSize(w, h);
}
}
#endif
