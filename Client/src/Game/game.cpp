#include "Game/game.hpp"
#include "AssetManager/manager.hpp"
#include "Client/client.hpp"
#include "GameState/gameState.hpp"
#include "env.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <utility>
#include <vector>
#ifdef CHEATS
#include <sstream>
#endif

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
    std::vector<ChunkEvent> chunkEvents = client.takeChunkEvents();
    for (ChunkEvent &e : chunkEvents) {
      if (e.load) {
        world.addChunk(e.cx, e.cz, e.blocks);
      } else {
        world.unloadChunk(e.cx, e.cz);
      }
    }
    for (const Object &o : client.takeNewObjects()) {
      world.addObject(o);
    }
    for (Vector3 pos : client.takeRemovedObjects()) {
      world.removeObject(pos);
    }
    for (Vector3 pos : client.takeDamagedObjects()) {
      world.damageObject(pos);
    }
    syncViewRadius();
  } else if (client.connect()) {
    world.clear(); // the server re-streams every block on join
    sentViewRadius = -1;
  }
}

// Render distance is in world units; the server counts chunks, and clamps what it accepts.
void Game::syncViewRadius() {
  const int chunks = (int)std::ceil(GameState::shared().getRenderDistance() / World::STREAM_CHUNK_SIZE);
  if (chunks == sentViewRadius)
    return;
  if (client.sendViewRadius(chunks)) {
    sentViewRadius = chunks; // otherwise the handshake isn't done yet; try again next frame
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

bool Game::chunkUnderPlayerLoaded() const {
  const Vector3 pos = player.getTransform().translation;
  const int cx      = World::streamChunkCoord(pos.x);
  const int cz      = World::streamChunkCoord(pos.z);
  return world.isChunkLoaded(cx, cz);
}

void Game::updatePlayer(float dt) {
  if (paused)
    return;

  if (!chunkUnderPlayerLoaded())
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
  if (paused || inChat || !chunkUnderPlayerLoaded())
    return;

  // Crosshair actions aim straight down the look direction - not
  // GetScreenToWorldRay(centre, camera) or camera.target - camera.position:
  // both read camera.target, which UpdateCamera can only build to head's
  // float precision (see there), so far from the origin they round to a
  // slightly wrong direction. getLookForward() never touches that huge
  // position, so it's exact at any distance.
  Ray aim{camera.position, player.getLookForward()};

  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    client.createBullet(aim.position, aim.direction);
  } else if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
    bulletCooldown -= dt;
    if (bulletCooldown <= 0) {
      client.createBullet(aim.position, aim.direction);
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
    if (world.placeBlock(aim, client, player.getTransform().translation)) {
      placeCooldown = placeCooldownTime;
    }
  } else if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
    placeCooldown -= dt;
    if (placeCooldown <= 0) {
      if (world.placeBlock(aim, client, player.getTransform().translation)) {
        placeCooldown = placeCooldownTime;
      }
    }
  }
}

// ==== draw ==== //
void Game::drawScene(float dt) {
  ClearBackground({5, 5, 5, 255});

  // Floating origin: the GPU only ever sees coordinates near zero, however far
  // into the world `camera` itself has drifted. Everything drawn below this
  // point must subtract camera.position from its world position to match -
  // miss one and it renders however far from the origin the camera really is.
  Camera3D relCamera = camera;
  relCamera.position = {0, 0, 0};
  // Not camera.target - camera.position: camera.target was already rounded to head's
  // precision when UpdateCamera built it (see there), so subtracting afterward can't
  // recover what that add discarded. getLookForward() never touches the huge position,
  // so it's exact at any distance from the origin.
  relCamera.target = player.getLookForward();

  BeginMode3D(relCamera);
  lighting.begin();
  Vector3 originViewPos = {0, 0, 0};
  lighting.setViewPos(originViewPos); // camera-relative, like everything else in this scene
  client.updateBullets(dt);
  client.updatePlayers();
  Object *targeted = nullptr;
  {
    Vector2 centre = {GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f};
    // Built from relCamera (exact direction) then shifted back to world space for the
    // hit-test, which still runs against world-space block positions - only the ray's
    // origin needs the shift, its direction is already exact.
    Ray pickRay      = GetScreenToWorldRay(centre, relCamera);
    pickRay.position = Vector3Add(pickRay.position, camera.position);
#ifdef DEBUG
    double t1 = GetTime();
#endif
    targeted = renderer.drawObjects(world.getObjects(), world, pickRay, lighting, camera);
#ifdef DEBUG
    drawObjectsMs = (GetTime() - t1) * 1000.0;
#endif
  }
  lighting.end();

  // Everything below is drawn without the lighting shader: it multiplies every vertex by a
  // per-instance matrix that only the block renderer supplies, so anything else would collapse to 0,0,0.
  if (targeted != nullptr) {
    ObjectTransform t = targeted->getTransform();
    DrawCubeWiresV(Vector3Subtract(t.pos, camera.position), t.scale, BLACK);
  }
  // ==== draw online players ====
  for (const auto &p : client.getPlayers()) {
    Transform transform;
    transform.rotation    = QuaternionFromEuler(0, p.yaw, 0);
    transform.scale       = {1, 10, 1};
    transform.translation = Vector3Subtract(p.pos, camera.position);
    Vector3 localPos      = Vector3Subtract(player.getTransform().translation, camera.position);
    if (p.name.has_value()) {
      player.DrawPlayer(transform, p.name.value(), localPos);
    } else {
      player.DrawPlayer(transform, "Player " + std::to_string(p.id), localPos);
    }
  }
  for (auto &b : client.getBullets()) {
    Vector3 pos = Vector3Subtract(b.pos, camera.position);
    DrawSphere(pos, 0.35f, Color{89, 255, 241, 255});
    DrawCylinderEx(pos, Vector3Subtract(pos, Vector3Scale(b.vel, 0.02f)), 0.35f, 0, 16, Color{89, 255, 241, 255});
  }
  drawChunkBorders();

  EndMode3D();
}

// F4: draws the server's streaming chunk grid around you, with your own chunk in yellow.
void Game::drawChunkBorders() {
#ifdef DEBUG
  if (IsKeyPressed(KEY_F4)) {
    showChunkBorders = !showChunkBorders;
  }
  if (!showChunkBorders) {
    return;
  }

  constexpr int RADIUS = 2;      // chunks drawn on each side of yours
  constexpr float TOP  = 200.0f; // how tall the border lines are
  constexpr float SIZE = World::STREAM_CHUNK_SIZE;
  const Vector3 pos    = player.getTransform().translation;
  const int cx         = World::streamChunkCoord(pos.x);
  const int cz         = World::streamChunkCoord(pos.z);

  // These lines are drawn inside the camera-relative BeginMode3D (see drawScene), so every
  // point needs the same - camera.position offset, or the grid renders far from the blocks.
  const Vector3 cam = camera.position;

  // Grid corner (i, j) is the corner with the smallest x and z of chunk (i, j); yours has four.
  for (int i = cx - RADIUS; i <= cx + RADIUS + 1; i++) {
    for (int j = cz - RADIUS; j <= cz + RADIUS + 1; j++) {
      const bool ownCorner = i >= cx && i <= cx + 1 && j >= cz && j <= cz + 1;
      Vector3 bottom       = Vector3Subtract({i * SIZE, 0.0f, j * SIZE}, cam);
      Vector3 top          = Vector3Subtract({i * SIZE, TOP, j * SIZE}, cam);
      DrawLine3D(bottom, top, ownCorner ? YELLOW : SKYBLUE);
    }
  }

  // Your chunk's outline at your feet, so you can see where the edge is at ground level.
  const Vector3 c00 = Vector3Subtract({cx * SIZE, pos.y, cz * SIZE}, cam);
  const Vector3 c10 = Vector3Subtract({(cx + 1) * SIZE, pos.y, cz * SIZE}, cam);
  const Vector3 c11 = Vector3Subtract({(cx + 1) * SIZE, pos.y, (cz + 1) * SIZE}, cam);
  const Vector3 c01 = Vector3Subtract({cx * SIZE, pos.y, (cz + 1) * SIZE}, cam);
  DrawLine3D(c00, c10, YELLOW);
  DrawLine3D(c10, c11, YELLOW);
  DrawLine3D(c11, c01, YELLOW);
  DrawLine3D(c01, c00, YELLOW);
#endif
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
  } else if (!paused && !chunkUnderPlayerLoaded()) {
    DrawText("Loading...", GetScreenWidth() / 2 - MeasureText("Loading...", 50) / 2, GetScreenHeight() / 2 - 25, 50, WHITE);
  }
}

void Game::drawDebug() {
#ifdef DEBUG
#ifdef __EMSCRIPTEN__
  if (IsKeyPressed(KEY_K)) {
    showDebug = !showDebug;
  }
#else
  if (IsKeyPressed(KEY_F3)) {
    showDebug = !showDebug;
  }
#endif
  if (IsKeyPressed(KEY_R)) {
    client.disconnect(); // applyNetworkUpdates starts a fresh session next frame
  }
  if (IsKeyPressed(KEY_F7)) {
    nearPlaneStep = (nearPlaneStep + 1) % 5;
    // Far only has to clear the furthest block drawn, so it tracks render distance.
    rlSetClipPlanes(NEAR_PLANES[nearPlaneStep], GameState::shared().getRenderDistance() + 100.0);
  }

  constexpr int ROWSIZE  = 30;
  constexpr int FONTSIZE = 20;

  if (showDebug) {
    int rowPos = 10;

    // NOTE: The +1 with black text is for readability (same as a drop shadow)

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

    const char *chunkText = TextFormat("Chunk: %d, %d (F4 borders)", World::streamChunkCoord(pos.x), World::streamChunkCoord(pos.z));
    DrawText(chunkText, 11, rowPos + 1, FONTSIZE, BLACK);
    DrawText(chunkText, 10, rowPos, FONTSIZE, LIME);
    rowPos += ROWSIZE;

    const char *nearText = TextFormat("F7 near: %.2f", NEAR_PLANES[nearPlaneStep]);
    DrawText(nearText, 11, rowPos + 1, FONTSIZE, BLACK);
    DrawText(nearText, 10, rowPos, FONTSIZE, LIME);
    rowPos += ROWSIZE;

    rowPos += ROWSIZE / 2; // small gap before the next section

    DrawText("World:", 10, rowPos, FONTSIZE, RED);
    rowPos += ROWSIZE;

    DrawText(TextFormat("Objects: %zu", world.getObjects().size()), 10, rowPos, FONTSIZE, RED);
    rowPos += ROWSIZE;

    DrawText(TextFormat("Faces drawn: %zu", renderer.getLastDrawnCount()), 10, rowPos, FONTSIZE, RED);
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
  }
#endif
}
