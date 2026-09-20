#include "Game/game.hpp"
#include "AssetManager/manager.hpp"
#include "GameState/gameState.hpp"
#include "env.hpp"

#include <algorithm>
#include <iostream>
#include <raylib.h>
#include <raymath.h>
#include <sstream>
#include <utility>
#include <vector>

// ==== setup / teardown ==== //
Game::Game(const AssetManager &a) : assets(a) {
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
}

Game::~Game() {
  client.disconnect();
}

// ==== one frame ==== //
void Game::frame() {
  float dt = GetFrameTime();

  applyNetworkUpdates();
  handlePause();
  handleChatInput();
  updatePlayer(dt);
  sendPosition(dt);
  handleActions(dt);
  world.update();

  BeginDrawing();
  drawScene(dt);
  world.drawHud();
  drawHealthBar();
  drawChat();
  drawScoreboard();
  drawOverlays(dt);
  drawDebug();
  EndDrawing();
}

// ==== update ==== //
void Game::applyNetworkUpdates() {
  if (client.isConnected()) {
    client.poll();
    if (auto pos = client.takeRespawn()) {
      player.setPosition(*pos);
      client.sendPlayerPosition(player.getTransform(), player.getPitch(), player.getYaw());
      player.UpdateCamera(camera);
    }
    for (const std::vector<Object> &chunk : client.takeInitChunks()) {
      world.addObjects(chunk);
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
  } else if (client.connect()) {
    world.clear(); // the server re-streams every block on join
  }
}

void Game::handlePause() {
  if (!IsKeyPressed(KEY_ESCAPE))
    return;

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

void Game::handleChatInput() {
#ifdef CHAT
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
}

void Game::updatePlayer(float dt) {
  if (paused)
    return;

  // Chat freezes input, not the world: the player keeps falling/sliding
  // while you type, and other clients keep seeing you move.
  player.inputEnabled = !inChat;
#ifdef DEBUG
  double t0 = GetTime();
#endif
  player.Update(dt, camera, world);
#ifdef DEBUG
  playerUpdateMs = (GetTime() - t0) * 1000.0;
#endif
}

void Game::sendPosition(float dt) {
  playerPosCooldown -= dt;
  if (playerPosCooldown <= 0) {
    client.sendPlayerPosition(player.getTransform(), player.getPitch(), player.getYaw());
    playerPosCooldown = Client::POS_UPDATE_INTERVAL;
  }
}

void Game::handleActions(float dt) {
  if (paused || inChat)
    return;

  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    client.createBullet(camera);
  } else if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
    bulletCooldown -= dt;
    if (bulletCooldown <= 0) {
      client.createBullet(camera);
#ifdef CHEATS
      bulletCooldown = 0.0f;
#else
      bulletCooldown = 0.05f;
#endif
    }
  }
#ifdef CHEATS
  constexpr float placeCooldownTime = 0;
#else
  constexpr float placeCooldownTime = 0.2f;
#endif
  if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
    Vector2 centre = {GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f};
    if (world.placeBlock(GetScreenToWorldRay(centre, camera), client, player.getTransform().translation)) {
      placeCooldown = placeCooldownTime;
    }
  } else if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
    placeCooldown -= dt;
    if (placeCooldown <= 0) {
      Vector2 centre = {GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f};
      if (world.placeBlock(GetScreenToWorldRay(centre, camera), client, player.getTransform().translation)) {
        placeCooldown = placeCooldownTime;
      }
    }
  }
}

// ==== draw ==== //
void Game::drawScene(float dt) {
  ClearBackground({5, 5, 5, 255});
  BeginMode3D(camera);
  lighting.begin();
  lighting.setViewPos(camera.position);
  client.updateBullets(dt);
  client.updatePlayers();
  Object *targeted = nullptr;
  {
    Vector2 centre = {GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f};
#ifdef DEBUG
    double t1 = GetTime();
#endif
    targeted = renderer.drawObjects(world.getObjects(), world, GetScreenToWorldRay(centre, camera), lighting, camera);
#ifdef DEBUG
    drawObjectsMs = (GetTime() - t1) * 1000.0;
#endif
  }
  lighting.end();

  // Everything below is drawn without the lighting shader: it multiplies every vertex by a
  // per-instance matrix that only the block renderer supplies, so anything else would collapse to 0,0,0.
  if (targeted != nullptr) {
    ObjectTransform t = targeted->getTransform();
    DrawCubeWiresV(t.pos, t.scale, BLACK);
  }
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
  for (auto &b : client.getBullets()) {
    DrawSphere(b.pos, 0.35f, Color{89, 255, 241, 255});
    DrawCylinderEx(b.pos, Vector3Subtract(b.pos, Vector3Scale(b.vel, 0.02f)), 0.35f, 0, 16, Color{89, 255, 241, 255});
  }

  EndMode3D();
}

void Game::drawHealthBar() {
  Texture2D heart = assets.get(Tex::Heart);
  for (int i = 0; i < env::MAX_HEALTH; i++) {
    Rectangle outRec = {static_cast<float>(16 + i * 19), static_cast<float>(GetScreenHeight() - 32), 16, 16};
    DrawTexturePro(heart, {0, 0, static_cast<float>(heart.width), static_cast<float>(heart.height)}, outRec, {0, 0}, 0, i < client.getHealth() ? WHITE : DARKGRAY);
  }
}

void Game::drawChat() {
#ifdef CHAT
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
#endif
}

// Hold Tab for the scoreboard, sorted by kills.
void Game::drawScoreboard() {
  if (!IsKeyDown(KEY_TAB))
    return;

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

// Pause / damage flashes, the connection status line and the crosshair.
void Game::drawOverlays(float dt) {
  GameState &gameState = GameState::shared();

  if (paused) {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {50, 50, 50, 50});

    DrawText("Paused", GetScreenWidth() / 2 - MeasureText("Paused", 50) / 2, GetScreenHeight() / 2 - 25, 50, WHITE);

    Rectangle exitButton = {10, 10, 60, 60};
    if (CheckCollisionPointRec(GetMousePosition(), exitButton)) {
      DrawRectangleRec(exitButton, LIGHTGRAY);
      if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        client.disconnect();
        paused = false;
        gameState.setMenuState(MenuState::HOME);
      }
    } else {
      DrawRectangleRec(exitButton, GRAY);
    }
    DrawText(" <", 10, 15, 50, WHITE);

  } else if (gameState.damageFlashTimer > 0) {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {100, 10, 10, 50});
    gameState.damageFlashTimer -= dt;
  } else if (gameState.greenFlashTimer > 0) {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {10, 100, 10, 50});
    gameState.greenFlashTimer -= dt;
  }
  if (!client.isConnected()) {
    // Client::connect() retries on its own, so there's no separate "failed" state to show.
    const char *msg = client.getKickReason() ? client.getKickReason()->c_str() : "Connecting...";
    Color col       = client.getKickReason() ? RED : WHITE;
    DrawText(msg, GetScreenWidth() / 2 - MeasureText(msg, 30) / 2, 60, 30, col);
  } else if (!paused) {
    // ==== draw crosshair ==== //
    Vector2 centre = {(float)GetScreenWidth() / 2, (float)GetScreenHeight() / 2};
    DrawCircleV(centre, (float)GetScreenHeight() / 1080, WHITE);
  }
}

void Game::drawDebug() {
#ifdef DEBUG
  if (IsKeyPressed(KEY_F3)) {
    showDebug = !showDebug;
  }
  if (IsKeyPressed(KEY_R)) {
    client.disconnect(); // applyNetworkUpdates starts a fresh session next frame
  }

  constexpr int ROWSIZE  = 30;
  constexpr int FONTSIZE = 20;

  if (showDebug) {
    int rowPos = 10;

    const char *fps = TextFormat("FPS: %d", GetFPS());
    DrawText(fps, 11, rowPos + 1, FONTSIZE, BLACK);
    DrawText(fps, 10, rowPos, FONTSIZE, LIME);
    rowPos += ROWSIZE;

    Vector3 pos = player.getTransform().translation;

    DrawText("Player pos:", 11, rowPos + 1, FONTSIZE, BLACK);
    DrawText("Player pos:", 10, rowPos, FONTSIZE, LIME);
    rowPos += ROWSIZE;

    DrawText(TextFormat("X: %f", pos.x), 11, rowPos + 1, FONTSIZE, BLACK);
    DrawText(TextFormat("X: %f", pos.x), 10, rowPos, FONTSIZE, LIME);
    rowPos += ROWSIZE;

    DrawText(TextFormat("Y: %f", pos.y), 11, rowPos + 1, FONTSIZE, BLACK);
    DrawText(TextFormat("Y: %f", pos.y), 10, rowPos, FONTSIZE, LIME);
    rowPos += ROWSIZE;
    DrawText(TextFormat("Z: %f", pos.z), 11, rowPos + 1, FONTSIZE, BLACK);
    DrawText(TextFormat("Z: %f", pos.z), 10, rowPos, FONTSIZE, LIME);
    rowPos += ROWSIZE;

    rowPos += ROWSIZE / 2; // small gap before the next section

    DrawText("World:", 10, rowPos, FONTSIZE, RED);
    rowPos += ROWSIZE;

    DrawText(TextFormat("Objects: %zu", world.getObjects().size()), 10, rowPos, FONTSIZE, RED);
    rowPos += ROWSIZE;

    DrawText(TextFormat("Shown (post-cull): %zu", renderer.getLastDrawnCount()), 10, rowPos, FONTSIZE, RED);
    rowPos += ROWSIZE;

    DrawText(TextFormat("Player update: %.2f ms", playerUpdateMs), 10, rowPos, FONTSIZE, RED);
    rowPos += ROWSIZE;

    DrawText(TextFormat("drawObjects: %.2f ms", drawObjectsMs), 10, rowPos, FONTSIZE, RED);
    rowPos += ROWSIZE;

    DrawText(TextFormat("cull/build: %.2f ms", renderer.getLastCullMs()), 10, rowPos, FONTSIZE, RED);
    rowPos += ROWSIZE;

    DrawText(TextFormat("draw: %.2f ms", renderer.getLastGpuMs()), 10, rowPos, FONTSIZE, RED);
    rowPos += ROWSIZE;

    rowPos += ROWSIZE / 2;

    DrawText("Network:", 11, rowPos + 1, FONTSIZE, BLACK);
    DrawText("Network:", 10, rowPos, FONTSIZE, YELLOW);
    rowPos += ROWSIZE;

    DrawText(TextFormat("Connected: %s", client.isConnected() ? "yes" : "no"), 11, rowPos + 1, FONTSIZE, BLACK);
    DrawText(TextFormat("Connected: %s", client.isConnected() ? "yes" : "no"), 10, rowPos, FONTSIZE, YELLOW);
    rowPos += ROWSIZE;

    DrawText(TextFormat("Player ID: %d", client.getPlayerId()), 11, rowPos + 1, FONTSIZE, BLACK);
    DrawText(TextFormat("Player ID: %d", client.getPlayerId()), 10, rowPos, FONTSIZE, YELLOW);
    rowPos += ROWSIZE;

    DrawText(TextFormat("Online players: %zu", client.getPlayers().size()), 11, rowPos + 1, FONTSIZE, BLACK);
    DrawText(TextFormat("Online players: %zu", client.getPlayers().size()), 10, rowPos, FONTSIZE, YELLOW);
    rowPos += ROWSIZE;

    DrawText(TextFormat("Bullets: %zu", client.getBullets().size()), 11, rowPos + 1, FONTSIZE, BLACK);
    DrawText(TextFormat("Bullets: %zu", client.getBullets().size()), 10, rowPos, FONTSIZE, YELLOW);
    rowPos += ROWSIZE;

    DrawText(TextFormat("World load: %zu blocks, last chunk at %.2f s", client.getBlocksReceived(), client.secondsToLastChunk()), 11, rowPos + 1, FONTSIZE, BLACK);
    DrawText(TextFormat("World load: %zu blocks, last chunk at %.2f s", client.getBlocksReceived(), client.secondsToLastChunk()), 10, rowPos, FONTSIZE, YELLOW);
    rowPos += ROWSIZE;
  }
#endif
}
