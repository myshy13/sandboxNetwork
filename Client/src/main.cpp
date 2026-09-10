#include "Client/client.hpp"
#include "GameState/gameState.hpp"
#include "Player/player.hpp"
#include "Protocol/protocol.hpp"
#include "Shaders/lighting.hpp"
#include "World/world.hpp"

#include <iostream>
#include <optional>
#include <raylib.h>
#include <raymath.h>
#include <string>
#include <unordered_map>
#include <vector>

bool paused          = false;
float bulletCooldown = 0.0f;
float placeCooldown  = 0.0f;

#define VERSION "0.1.1"

#ifdef DEBUG
bool showDebug = false;
#endif

int main() {
  // suppresses raylib's unnecessary logging levels
  SetTraceLogLevel(LOG_WARNING);
  std::cout << "Game version: " << VERSION << "\n";
  std::cout << "protocol version: " << proto::PROTOCOL_VERSION << "\n";

  int screenWidth  = 1280;
  int screenHeight = 720;
  bool inChat      = false;
  std::string chatInput;

  GameState &gameState = GameState::shared();

  float playerPosCooldown = 0.1667;

  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI);
  std::cout << "Create window\n";
  InitWindow(screenWidth, screenHeight, std::string("Sandbox Network - " + std::string(VERSION)).c_str());

  SetTargetFPS(144);

  SetExitKey(KEY_NULL);

  Camera3D camera;
  camera.position   = {10.0f, 10.0f, 10.0f};
  camera.target     = {0.0f, 0.0f, 0.0f};
  camera.up         = {0.0f, 1.0f, 0.0f};
  camera.fovy       = 70.0f;
  camera.projection = CAMERA_PERSPECTIVE;

  Client client;
  // shaders
  Lighting lighting;
  Player player;
  World world;

  // ==== lighting ==== //
  lighting.addDirectional(
      {50.0f, 100.0f, 40.0f},
      {0.0f, 0.0f, 0.0f},
      {255, 245, 225, 255});
  lighting.addDirectional(
      {-50.0f, 100.0f, -40.0f},
      {0.0f, 0.0f, 0.0f},
      {255, 245, 225, 255});

  // TODO: Sunrise and sunset

  while (!WindowShouldClose()) {
    float dt = GetFrameTime();
    // ==== Server communication ==== //
    client.poll();
    if (auto pos = client.takeRespawn()) {
      player.setPosition(*pos);
      client.sendPlayerPosition(player.getTransform(), player.getPitch(), player.getYaw());
      player.UpdateCamera(camera);
    }
    for (const Object &o : client.takeNewObjects()) {
      world.addObject(o);
    }
    for (int id : client.takeRemovedObjects()) {
      world.removeObject(id);
    }
    for (int id : client.takeDamagedObjects()) {
      world.damageObject(id);
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
#ifdef CHAT
    // ==== chat input ==== //
    if (inChat) {
      int ch;
      while ((ch = GetCharPressed()) != 0) {
        if (ch >= 32 && ch < 127 && chatInput.size() < 200) {
          chatInput += static_cast<char>(ch);
        }
      }
      if (IsKeyPressed(KEY_BACKSPACE) && !chatInput.empty()) {
        chatInput.pop_back();
      }
      if (IsKeyPressed(KEY_ENTER)) {
        if (!chatInput.empty()) {
          if (chatInput.starts_with("/")) {
            chatInput.erase(0, 1);
            if (chatInput.starts_with("setname ")) {
              chatInput.erase(0, 8);
              client.setName(chatInput);
              std::cout << "Set name to " << chatInput << "\n";
            } else if (chatInput.starts_with("clear")) {
              client.clearChat();
            }
#ifdef CHEATS
            else if (chatInput.starts_with("tp ")) {
              chatInput.erase(0, 3);
              std::stringstream pos(chatInput);
              Vector3 p;
              if (pos >> p.x >> p.y >> p.z) {
                player.setPosition(p);
              }
            } else {
              client.addLocalChat("Command not found");
            }
#endif
          } else {
            client.sendChatMessage(chatInput);
          }
          chatInput.clear();
        }
        inChat = false;
      }
    } else if (IsKeyPressed(KEY_T)) {
      inChat = true;
      paused = false;
    } else if (IsKeyPressed(KEY_SLASH)) {
      inChat    = true;
      paused    = false;
      chatInput = "/";
    }
#endif

    if (!paused) {
      // Chat freezes input, not the world: the player keeps falling/sliding
      // while you type, and other clients keep seeing you move.
      player.inputEnabled = !inChat;
      player.Update(dt, camera, world.getObjects());
    }

    playerPosCooldown -= dt;
    if (playerPosCooldown <= 0) {
      client.sendPlayerPosition(player.getTransform(), player.getPitch(), player.getYaw());
      playerPosCooldown = 0.1667;
    }

    if (!paused && !inChat) {
      if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        client.createBullet(camera);
      } else if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        bulletCooldown -= dt;
        if (bulletCooldown <= 0) {
          client.createBullet(camera);
#ifdef CHEATS
          bulletCooldown = 0.0f;
#else
          bulletCooldown = 0.1f;
#endif
        }
      }
      if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        Vector2 centre = {GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f};
        if (world.placeBlock(GetScreenToWorldRay(centre, camera), client, player.getTransform().translation)) {
          placeCooldown = 0.2f;
        }
      } else if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        placeCooldown -= dt;
        if (placeCooldown <= 0) {
          Vector2 centre = {GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f};
          if (world.placeBlock(GetScreenToWorldRay(centre, camera), client, player.getTransform().translation)) {
            placeCooldown = 0.2f;
          }
        }
      }
    }

    // ==== Draw ====
    {
      BeginDrawing();
      ClearBackground({5, 5, 5, 255});
      BeginMode3D(camera);
      lighting.begin();
      lighting.setViewPos(camera.position);
      // ==== draw online players ====
      for (const auto &p : client.getPlayers()) {
        Transform transform;
        transform.rotation    = QuaternionFromEuler(0, p.yaw, 0);
        transform.scale       = {1, 10, 1};
        transform.translation = p.pos;
        if (p.name.has_value()) {
          player.DrawPlayer(transform, p.name.value(), player.getTransform().translation);
        } else {
          player.DrawPlayer(transform, "Player " + std::to_string(p.id), player.getTransform().translation);
        }
      }
      client.updateBullets(dt);
      world.draw();
      lighting.end();
      for (auto &b : client.getBullets()) {
        DrawSphere(b.pos, 0.35f, Color{89, 255, 241, 255});
        DrawCylinderEx(b.pos, Vector3Subtract(b.pos, Vector3Scale(b.vel, 0.02f)), 0.35f, 0, 16, Color{89, 255, 241, 255});
      }
      EndMode3D();
    }

    // ==== health bar ==== //
    {
      constexpr int MAX_HEALTH = 3;
      for (int i = 0; i < MAX_HEALTH; i++) {
        Color fill = i < client.getHealth() ? RED : Color{60, 60, 60, 255};
        DrawRectangle(20 + i * 44, GetScreenHeight() - 32, 40, 16, fill);
      }
    }

#ifdef CHAT
    // ==== chat (bottom left) ==== //
    {
      constexpr int VISIBLE_CHAT_MESSAGES    = 8;
      constexpr double CHAT_MESSAGE_LIFETIME = 10.0; // seconds
      constexpr int LINE_HEIGHT              = 22;
      constexpr int FONT_SIZE                = 18;
      const auto &chat                       = client.getChat();

      // Walk newest-first and stop once messages age out, so visible ends up oldest-first-capped-at-8.
      std::vector<int> visible;
      for (int i = (int)chat.size() - 1; i >= 0 && (int)visible.size() < VISIBLE_CHAT_MESSAGES; i--) {
        if (inChat) {
          visible.push_back(i);
        } else {
          if (GetTime() - chat[i].receivedAt > CHAT_MESSAGE_LIFETIME) {
            break; // older entries are older still, nothing left to show
          }
          visible.push_back(i);
        }
      }

      // History sits above the input box, so leave room for it when open.
      int inputHeight = 0;
      if (inChat) {
        inputHeight = LINE_HEIGHT + 6;
      }
      int y = GetScreenHeight() - 20 - inputHeight - LINE_HEIGHT * (int)visible.size();
      for (auto it = visible.rbegin(); it != visible.rend(); ++it) { // reverse again to draw oldest-to-newest top-to-bottom
        const ChatEntry &entry = chat[*it];
        bool isSystemMessage   = entry.id == -1; // server-generated messages (joins/leaves/etc) use id -1
        std::string senderName = "Player " + std::to_string(entry.id);
        if (entry.id == client.getPlayerId()) {
          senderName = client.getName().value_or(senderName); // fall back to "Player N" if we haven't set a name
        } else if (OnlinePlayer *p = client.findPlayer(entry.id); p && p->name.has_value()) {
          senderName = p->name.value();
        }
        std::string line = entry.text;
        if (!isSystemMessage) {
          line = senderName + ": " + entry.text;
        }
        Color textColor = WHITE;
        if (isSystemMessage) {
          textColor = Color{200, 74, 64, 255}; // red so system messages stand out from chat
        }
        DrawRectangle(16, y - 2, MeasureText(line.c_str(), FONT_SIZE) + 8, LINE_HEIGHT, {0, 0, 0, 120}); // background behind the text for readability
        DrawText(line.c_str(), 20, y, FONT_SIZE, textColor);
        y += LINE_HEIGHT * ((int)std::count(line.begin(), line.end(), '\n') + 1); // multi-line messages push the next line down further
      }

      if (inChat) {
        const char *cursor = "";
        if ((int)(GetTime() * 2) % 2 == 0) {
          cursor = "_"; // blink at 1Hz
        }
        std::string prompt = "> " + chatInput + cursor;
        int boxY           = GetScreenHeight() - 20 - LINE_HEIGHT;
        DrawRectangle(16, boxY - 2, GetScreenWidth() - 32, LINE_HEIGHT + 4, {0, 0, 0, 160});
        DrawText(prompt.c_str(), 20, boxY, FONT_SIZE, WHITE);
      }
    }
#endif

    if (IsKeyDown(KEY_TAB)) {
      // Tab Kills UI - hold Tab for the scoreboard, sorted by kills.
      constexpr int ROW_HEIGHT = 28;
      constexpr int FONT_SIZE  = 20;
      const int rowWidth       = (int)(GetScreenWidth() * 0.6f);
      const int x              = GetScreenWidth() / 2 - rowWidth / 2;

      DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {0, 0, 0, 120});

      std::vector<std::pair<int, int>> rows(client.getKills().begin(), client.getKills().end());
      std::sort(rows.begin(), rows.end(), [](const auto &a, const auto &b) { return a.second > b.second; });

      int y = 80;
      DrawRectangle(x, y, rowWidth, ROW_HEIGHT * ((int)rows.size() + 1), {0, 0, 0, 160});
      DrawText("Kills", x + 10, y + 4, FONT_SIZE, {200, 200, 200, 255});
      y += ROW_HEIGHT;
      for (const auto &[id, kills] : rows) {
        std::string name = "Player " + std::to_string(id);
        if (id == client.getPlayerId()) {
          if (const auto &n = client.getName(); n.has_value()) {
            name = n.value();
          }
        } else if (OnlinePlayer *p = client.findPlayer(id); p && p->name.has_value()) {
          name = p->name.value();
        }
        DrawText(name.c_str(), x + 10, y + 4, FONT_SIZE, WHITE);
        std::string k = std::to_string(kills);
        DrawText(k.c_str(), x + rowWidth - 10 - MeasureText(k.c_str(), FONT_SIZE), y + 4, FONT_SIZE, WHITE);
        y += ROW_HEIGHT;
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
    }
    if (!client.isConnected()) {
      const char *msg =
          client.getKickReason() ? client.getKickReason()->c_str()
          : GetTime() < 5.0      ? "Connecting..."
                                 : "Failed to connect to server";
      Color col = GetTime() < 5.0 && !client.getKickReason() ? WHITE : RED;
      DrawText(msg, GetScreenWidth() / 2 - MeasureText(msg, 30) / 2, 60, 30, col);
      if (GetTime() >= 5.0f) {
        std::cout << "Failed to connect to server\n";
      }
    } else if (!paused) {
      // ==== draw crosshair ==== //
      Vector2 centre = {(float)GetScreenWidth() / 2, (float)GetScreenHeight() / 2};
      DrawCircleV(centre, (float)GetScreenHeight() / 1080, WHITE);
    }

#ifdef DEBUG
    if (IsKeyPressed(KEY_F3)) {
      showDebug = !showDebug;
    }

    constexpr int ROWSIZE  = 30;
    constexpr int FONTSIZE = 20;

    if (showDebug) {
      int rowPos = 10;

      const char *fps = TextFormat("FPS: %d", GetFPS());
      DrawText(fps, 10, rowPos, FONTSIZE, LIME);

      rowPos += ROWSIZE;
      Vector3 pos = player.getTransform().translation;

      DrawText("Player pos:", 10, rowPos, FONTSIZE, LIME);
      rowPos += ROWSIZE;

      DrawText(TextFormat("X: %f", pos.x), 10, rowPos, FONTSIZE, LIME);
      rowPos += ROWSIZE;

      DrawText(TextFormat("Y: %f", pos.y), 10, rowPos, FONTSIZE, LIME);
      rowPos += ROWSIZE;
      DrawText(TextFormat("Z: %f", pos.z), 10, rowPos, FONTSIZE, LIME);
      rowPos += ROWSIZE;
    }
#endif

    EndDrawing();
  }
  client.disconnect();

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
