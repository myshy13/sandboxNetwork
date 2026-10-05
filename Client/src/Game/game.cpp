#include "Game/game.hpp"

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "AssetManager/blockTex.hpp"
#include "AssetManager/manager.hpp"
#include "Client/client.hpp"
#include "GameState/gameState.hpp"
#include "Shaders/lighting.hpp"
#include "World/world.hpp"
#include "env.hpp"
#ifdef CHEATS
#include <sstream>
#endif

// ==== setup / teardown ==== //
Game::Game(const AssetManager& a) : assets(a), renderer(a) {
  // ==== lighting ==== //
  DirectionalLight light = lighting.timeToLight(world.getTime());
  sunLights[0] = lighting.addDirectional(Vector3Add(light.pos, {0, 0, 0}),
                                         light.tar, light.color);
  sunLights[1] = lighting.addDirectional(Vector3Add(light.pos, {10, 0, 0}),
                                         light.tar, light.color);
  sunLights[2] = lighting.addDirectional(Vector3Add(light.pos, {-10, -10, 10}),
                                         light.tar, light.color);
  sunLights[3] = lighting.addDirectional(Vector3Add(light.pos, {0, 10, -10}),
                                         light.tar, light.color);
}

Game::~Game() {
  client.disconnect();
  UnloadRenderTexture(target);
}

RenderTexture Game::makeTarget() {
  RenderTexture t =
      LoadRenderTexture(static_cast<int>(GetScreenWidth() * RENDER_SCALE),
                        static_cast<int>(GetScreenHeight() * RENDER_SCALE));
  // Bilinear, so scaling down averages neighbouring pixels (the default,
  // nearest, would just drop them and shimmer).
  SetTextureFilter(t.texture, TEXTURE_FILTER_BILINEAR);
  return t;
}

// ==== one frame ==== //
void Game::frame() {
  // Caps how far a single frame can move physics forward - a chunk-rebuild
  // stall would otherwise report a huge dt and tunnel the player through the
  // floor (gravity integrated over the whole stall in one uncollided step).
  constexpr float MAX_DT = 1.0f / 30.0f;
  float dt = std::min(GetFrameTime(), MAX_DT);

  world.update(dt);
  applyNetworkUpdates();
  updateLighting();
  handlePause();
  handleChatInput();
  updatePlayer(dt);
  sendPosition(dt);
  handleActions(dt);

  // Zoom narrows the field of view instead of cropping the texture, so the
  // scene is re-rendered at full resolution rather than enlarged pixels. The
  // frustum culling and the camera-relative view both read camera.fovy too.
  const float wantedFov = (!inChat && IsKeyDown(KEY_C)) ? ZOOM_FOV : BASE_FOV;
  camera.fovy = Lerp(camera.fovy, wantedFov, 1.0f - expf(-15.0f * dt));

  // The setting, and a valid clock (it isn't until the handshake finishes).
  const bool shadowsOn =
      GameState::shared().getShadows() && std::isfinite(world.getTime());
  if (shadowsOn) {
    renderer.shadowMap(
        world.getObjects(), camera, lighting,
        Vector3Normalize(lighting.timeToLight(world.getTime()).pos),
        client.getPlayers(),
        {client.getPlayerId(), player.getTransform().translation,
         player.getPitch(), player.getYaw(), ""});
  }
  lighting.setShadowsEnabled(shadowsOn);

  BeginTextureMode(target);
  if (shadowsOn) {
    lighting.setShadow(renderer.getLightMatrix(), renderer.getShadowDepth().id,
                       renderer.getShadowDepth().width);
  }
  drawScene(dt);
  EndTextureMode();

  BeginDrawing();
  // Negative height: render textures are stored upside down.
  const Rectangle source = {0, 0, static_cast<float>(target.texture.width),
                            -static_cast<float>(target.texture.height)};
  DrawTexturePro(target.texture, source,
                 {0, 0, static_cast<float>(GetScreenWidth()),
                  static_cast<float>(GetScreenHeight())},
                 {0, 0}, 0, WHITE);

  // The UI goes straight to the screen, so zooming or resizing the scene
  // texture never scales it. Debug before the overlays so the pause menu
  // covers it.
  drawHealthBar();
  drawChat();
  drawScoreboard();
  drawDebug();
  drawOverlays(dt);
  EndDrawing();
}

// ==== update ==== //
void Game::applyNetworkUpdates() {
  if (client.isConnected()) {
    client.poll();
    if (auto pos = client.takeRespawn()) {
      player.setPosition(*pos);
      client.sendPlayerPosition(player.getTransform(), player.getPitch(),
                                player.getYaw());
      player.UpdateCamera(camera);
    }
    // In arrival order: the server already sends a place-over-water as Remove
    // then Add.
    for (WorldEvent& e : client.takeWorldEvents()) {
      switch (e.kind) {
        case WorldEvent::Kind::LoadChunk:
          world.addChunk(e.cx, e.cz, e.blocks);
          break;
        case WorldEvent::Kind::UnloadChunk:
          world.unloadChunk(e.cx, e.cz);
          break;
        case WorldEvent::Kind::Add:
          world.addObject(e.object);
          break;
        case WorldEvent::Kind::Remove:
          world.removeObject(e.pos);
          break;
        case WorldEvent::Kind::Damage:
          world.damageObject(e.pos);
          break;
        case WorldEvent::Kind::WaterLevel:
          world.setWaterLevel(e.pos, e.level);
          break;
      }
    }
    syncViewRadius();

    std::optional<TimeSetting> newTime = client.takeTimeSetting();
    if (newTime.has_value()) {
      world.setTimeSettings(newTime.value());
    }
  } else if (client.connect()) {
    world.clear();  // the server re-streams every block on join
    sentViewRadius = -1;
  }
}

// Render distance is in world units; the server counts chunks, and clamps what
// it accepts.
void Game::syncViewRadius() {
  const int chunks = (int)std::ceil(GameState::shared().getRenderDistance() /
                                    World::STREAM_CHUNK_SIZE);
  if (chunks == sentViewRadius) return;
  if (client.sendViewRadius(chunks)) {
    sentViewRadius =
        chunks;  // otherwise the handshake isn't done yet; try again next frame
  }
}

void Game::handlePause() {
  if (!IsKeyPressed(KEY_ESCAPE)) return;

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
    inChat = true;
    paused = false;
    chatInput = "/";
  }
#endif
}

bool Game::chunkUnderPlayerLoaded() const {
  const Vector3 pos = player.getTransform().translation;
  const int cx = World::streamChunkCoord(pos.x);
  const int cz = World::streamChunkCoord(pos.z);
  return world.isChunkLoaded(cx, cz);
}

void Game::updatePlayer(float dt) {
  if (paused) return;

  if (!chunkUnderPlayerLoaded()) return;

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
    client.sendPlayerPosition(player.getTransform(), player.getPitch(),
                              player.getYaw());
    playerPosCooldown = Client::POS_UPDATE_INTERVAL;
  }
}

void Game::updateLighting() {
  // The clock is infinite until the handshake sends the day length.
  if (!std::isfinite(world.getTime())) return;
  auto newLight = lighting.timeToLight(world.getTime());
  // Four lights stack, so each gets a quarter (alpha is ignored by the
  // shader).
  const Color sun = ColorBrightness(newLight.color, -0.75f);
  lighting.updateLight(sunLights[0], Vector3Add(newLight.pos, {0, 0, 0}),
                       newLight.tar, sun);
  lighting.updateLight(sunLights[1], Vector3Add(newLight.pos, {10, 0, 0}),
                       newLight.tar, sun);
  lighting.updateLight(sunLights[2], Vector3Add(newLight.pos, {-10, -10, 10}),
                       newLight.tar, sun);
  lighting.updateLight(sunLights[3], Vector3Add(newLight.pos, {0, 10, -10}),
                       newLight.tar, sun);
  const float (&ambient)[4] = {((float)newLight.color.r / 4 + 10) / 255,
                               ((float)newLight.color.g / 4 + 10) / 255,
                               ((float)newLight.color.b / 4 + 10) / 255, 1.0f};
  lighting.updateAmbient(ambient);
}

void Game::handleActions(float dt) {
  if (IsWindowResized()) {
    UnloadRenderTexture(target);
    target = makeTarget();
  }
  {
    int key = GetKeyPressed();
    if (key >= KEY_ONE && key < KEY_ONE + blockTypesSize) {
      activeBlockType = key - KEY_ONE;
    }
  }
  if (paused || inChat || !chunkUnderPlayerLoaded()) return;

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
    if (world.placeBlock(aim, client, player.getTransform().translation,
                         blockTypes[activeBlockType])) {
      placeCooldown = placeCooldownTime;
    }
  } else if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
    placeCooldown -= dt;
    if (placeCooldown <= 0) {
      if (world.placeBlock(aim, client, player.getTransform().translation,
                           blockTypes[activeBlockType])) {
        placeCooldown = placeCooldownTime;
      }
    }
  }
}

// ==== draw ==== //
void Game::drawScene(float dt) {
  // Black while the handshake is still loading (the clock isn't set yet).
  if (!std::isfinite(world.getTime())) {
    ClearBackground(BLACK);
    return;
  }
  Color bg = lighting.skyColor(world.getTime());
  // ClearBackground({5, 5, 5, 255});
  ClearBackground(bg);

  // Floating origin: the GPU only ever sees coordinates near zero, however far
  // into the world `camera` itself has drifted. Everything drawn below this
  // point must subtract camera.position from its world position to match -
  // miss one and it renders however far from the origin the camera really is.
  Camera3D relCamera = camera;
  relCamera.position = {0, 0, 0};
  // Not camera.target - camera.position: camera.target was already rounded to
  // head's precision when UpdateCamera built it (see there), so subtracting
  // afterward can't recover what that add discarded. getLookForward() never
  // touches the huge position, so it's exact at any distance from the origin.
  relCamera.target = player.getLookForward();

  BeginMode3D(relCamera);
  lighting.begin();
  Vector3 originViewPos = {0, 0, 0};
  lighting.setViewPos(
      originViewPos);  // camera-relative, like everything else in this scene
  client.updateBullets(dt);
  client.updatePlayers();
  Object* targeted = nullptr;
  {
    Vector2 centre = {GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f};
    // Built from relCamera (exact direction) then shifted back to world space
    // for the hit-test, which still runs against world-space block positions -
    // only the ray's origin needs the shift, its direction is already exact.
    Ray pickRay = GetScreenToWorldRay(centre, relCamera);
    pickRay.position = Vector3Add(pickRay.position, camera.position);
#ifdef DEBUG
    double t1 = GetTime();
#endif
    targeted = renderer.drawObjects(world.getObjects(), world, pickRay,
                                    lighting, camera, client.getPlayers());
#ifdef DEBUG
    drawObjectsMs = (GetTime() - t1) * 1000.0;
#endif
  }
  lighting.end();

  // Everything below is drawn without the lighting shader: it multiplies every
  // vertex by a per-instance matrix that only the block renderer supplies, so
  // anything else would collapse to 0,0,0.
  if (targeted != nullptr) {
    const ObjectTransform& t = targeted->getTransform();
    DrawCubeWiresV(Vector3Subtract(t.pos, camera.position), t.scale, BLACK);
  }
  // ==== draw online players ====
  for (const auto& p : client.getPlayers()) {
    Transform transform;
    transform.rotation = QuaternionFromEuler(0, p.yaw, 0);
    transform.scale = env::PLAYER_SCALE;
    transform.translation = Vector3Subtract(p.pos, camera.position);
    Vector3 localPos =
        Vector3Subtract(player.getTransform().translation, camera.position);
    if (p.name.has_value()) {
      player.DrawPlayer(transform, p.name.value(), localPos);
    } else {
      player.DrawPlayer(transform, "Player " + std::to_string(p.id), localPos);
    }
  }
  for (auto& b : client.getBullets()) {
    Vector3 pos = Vector3Subtract(b.pos, camera.position);
    DrawSphere(pos, 0.35f, Color{89, 255, 241, 255});
    DrawCylinderEx(pos, Vector3Subtract(pos, Vector3Scale(b.vel, 0.02f)), 0.35f,
                   0, 16, Color{89, 255, 241, 255});
  }
  drawCollisionDebug();
  drawChunkBorders();

  EndMode3D();

  // Sized to the render texture, not the window: they differ by RENDER_SCALE.
  if (world.isWater(camera.position))
    DrawRectangle(0, 0, target.texture.width, target.texture.height,
                  Fade(SKYBLUE, 0.35f));
}

// F5: red wireframes on every cell the collision grid treats as solid near you,
// green for your hitbox.
void Game::drawCollisionDebug() {
#ifdef DEBUG
  if (IsKeyPressed(KEY_F5)) {
    showCollisionDebug = !showCollisionDebug;
  }
  if (!showCollisionDebug) {
    return;
  }

  constexpr float CELL = 5.0f;  // same as env::BLOCKSIZE in world.cpp
  const Vector3 feet = player.getTransform().translation;
  for (int dx = -3; dx <= 3; dx++) {
    for (int dy = -2; dy <= 3; dy++) {
      for (int dz = -3; dz <= 3; dz++) {
        Vector3 c = {(floorf(feet.x / CELL) + dx + 0.5f) * CELL,
                     (floorf(feet.y / CELL) + dy + 0.5f) * CELL,
                     (floorf(feet.z / CELL) + dz + 0.5f) * CELL};
        if (world.isOccupied(c)) {
          DrawCubeWiresV(Vector3Subtract(c, camera.position),
                         {CELL, CELL, CELL}, RED);
        }
      }
    }
  }
  const Vector3 size = player.getTransform().scale;
  DrawCubeWiresV(
      Vector3Subtract(Vector3Add(feet, {0, size.y * 0.5f, 0}), camera.position),
      size, GREEN);
#endif
}

// F4: draws the server's streaming chunk grid around you, with your own chunk
// in yellow.
void Game::drawChunkBorders() {
#ifdef DEBUG
  if (IsKeyPressed(KEY_F4)) {
    showChunkBorders = !showChunkBorders;
  }
  if (!showChunkBorders) {
    return;
  }

  constexpr int RADIUS = 2;      // chunks drawn on each side of yours
  constexpr float TOP = 200.0f;  // how tall the border lines are
  constexpr float SIZE = World::STREAM_CHUNK_SIZE;
  const Vector3 pos = player.getTransform().translation;
  const int cx = World::streamChunkCoord(pos.x);
  const int cz = World::streamChunkCoord(pos.z);

  // These lines are drawn inside the camera-relative BeginMode3D (see
  // drawScene), so every point needs the same - camera.position offset, or the
  // grid renders far from the blocks.
  const Vector3 cam = camera.position;

  // Grid corner (i, j) is the corner with the smallest x and z of chunk (i, j);
  // yours has four.
  for (int i = cx - RADIUS; i <= cx + RADIUS + 1; i++) {
    for (int j = cz - RADIUS; j <= cz + RADIUS + 1; j++) {
      const bool ownCorner = i >= cx && i <= cx + 1 && j >= cz && j <= cz + 1;
      Vector3 bottom = Vector3Subtract({i * SIZE, 0.0f, j * SIZE}, cam);
      Vector3 top = Vector3Subtract({i * SIZE, TOP, j * SIZE}, cam);
      DrawLine3D(bottom, top, ownCorner ? YELLOW : SKYBLUE);
    }
  }

  // Your chunk's outline at your feet, so you can see where the edge is at
  // ground level.
  const Vector3 c00 = Vector3Subtract({cx * SIZE, pos.y, cz * SIZE}, cam);
  const Vector3 c10 = Vector3Subtract({(cx + 1) * SIZE, pos.y, cz * SIZE}, cam);
  const Vector3 c11 =
      Vector3Subtract({(cx + 1) * SIZE, pos.y, (cz + 1) * SIZE}, cam);
  const Vector3 c01 = Vector3Subtract({cx * SIZE, pos.y, (cz + 1) * SIZE}, cam);
  DrawLine3D(c00, c10, YELLOW);
  DrawLine3D(c10, c11, YELLOW);
  DrawLine3D(c11, c01, YELLOW);
  DrawLine3D(c01, c00, YELLOW);
#endif
}

void Game::drawHealthBar() {
  const Texture2D& heart = assets.get(Tex::Heart);
  for (int i = 0; i < env::MAX_HEALTH; i++) {
    Rectangle outRec = {static_cast<float>(16 + i * 19),
                        static_cast<float>(GetScreenHeight() - 32), 16, 16};
    DrawTexturePro(heart,
                   {0, 0, static_cast<float>(heart.width),
                    static_cast<float>(heart.height)},
                   outRec, {0, 0}, 0,
                   i < client.getHealth() ? WHITE : DARKGRAY);
  }
}

void Game::drawChat() {
#ifdef CHAT
  constexpr int VISIBLE_CHAT_MESSAGES = 8;
  constexpr double CHAT_MESSAGE_LIFETIME = 10.0;  // seconds
  constexpr int LINE_HEIGHT = 22;
  constexpr int FONT_SIZE = 18;
  const auto& chat = client.getChat();

  // Walk newest-first and stop once messages age out, so visible ends up
  // oldest-first-capped-at-8.
  std::vector<int> visible;
  for (int i = (int)chat.size() - 1;
       i >= 0 && (int)visible.size() < VISIBLE_CHAT_MESSAGES; i--) {
    if (inChat) {
      visible.push_back(i);
    } else {
      if (GetTime() - chat[i].receivedAt > CHAT_MESSAGE_LIFETIME) {
        break;  // older entries are older still, nothing left to show
      }
      visible.push_back(i);
    }
  }

  // History sits above the input box, so leave room for it when open.
  int inputHeight = 0;
  if (inChat) {
    inputHeight = LINE_HEIGHT + 6;
  }
  int y =
      GetScreenHeight() - 20 - inputHeight - LINE_HEIGHT * (int)visible.size();
  for (auto it = visible.rbegin(); it != visible.rend();
       ++it) {  // reverse again to draw oldest-to-newest top-to-bottom
    const ChatEntry& entry = chat[*it];
    bool isSystemMessage =
        entry.id ==
        -1;  // server-generated messages (joins/leaves/etc) use id -1
    std::string senderName = "Player " + std::to_string(entry.id);
    if (entry.id == client.getPlayerId()) {
      senderName = client.getName().value_or(
          senderName);  // fall back to "Player N" if we haven't set a name
    } else if (OnlinePlayer* p = client.findPlayer(entry.id);
               p && p->name.has_value()) {
      senderName = p->name.value();
    }
    std::string line = entry.text;
    if (!isSystemMessage) {
      line = senderName + ": " + entry.text;
    }
    Color textColor = WHITE;
    if (isSystemMessage) {
      textColor = Color{200, 74, 64,
                        255};  // red so system messages stand out from chat
    }
    DrawRectangle(
        16, y - 2, MeasureText(line.c_str(), FONT_SIZE) + 8, LINE_HEIGHT,
        {0, 0, 0, 120});  // background behind the text for readability
    DrawText(line.c_str(), 20, y, FONT_SIZE, textColor);
    y += LINE_HEIGHT *
         ((int)std::count(line.begin(), line.end(), '\n') +
          1);  // multi-line messages push the next line down further
  }

  if (inChat) {
    const char* cursor = "";
    if ((int)(GetTime() * 2) % 2 == 0) {
      cursor = "_";  // blink at 1Hz
    }
    std::string prompt = "> " + chatInput + cursor;
    int boxY = GetScreenHeight() - 20 - LINE_HEIGHT;
    DrawRectangle(16, boxY - 2, GetScreenWidth() - 32, LINE_HEIGHT + 4,
                  {0, 0, 0, 160});
    DrawText(prompt.c_str(), 20, boxY, FONT_SIZE, WHITE);
  }
#endif
}

// Hold Tab for the scoreboard, sorted by kills.
void Game::drawScoreboard() {
  if (!IsKeyDown(KEY_TAB)) return;

  constexpr int ROW_HEIGHT = 28;
  constexpr int FONT_SIZE = 20;
  const int rowWidth = (int)(GetScreenWidth() * 0.6f);
  const int x = GetScreenWidth() / 2 - rowWidth / 2;

  DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {0, 0, 0, 120});

  int y = 80;
  DrawRectangle(x, y, rowWidth,
                ROW_HEIGHT * ((int)client.getKills().size() + 1),
                {0, 0, 0, 160});
  DrawText("Kills", x + 10, y + 4, FONT_SIZE, {200, 200, 200, 255});
  y += ROW_HEIGHT;
  for (const auto& [id, kills] : client.getKills()) {
    std::optional<std::string> name = client.idToName(id);
    if (!name.has_value()) {
      name = "Player " + std::to_string(id);
    }
    DrawText(name->c_str(), x + 10, y + 4, FONT_SIZE, WHITE);
    std::string k = std::to_string(kills);
    DrawText(k.c_str(), x + rowWidth - 10 - MeasureText(k.c_str(), FONT_SIZE),
             y + 4, FONT_SIZE, WHITE);
    y += ROW_HEIGHT;
  }
}

// Pause / damage flashes, the connection status line and the crosshair.
void Game::drawOverlays(float dt) {
  GameState& gameState = GameState::shared();

  {
    constexpr float BOXSIZE = 50.0f;  // square
    constexpr float BORDER = 5.0f;
    const float left = GetScreenWidth() - BOXSIZE * blockTypesSize;
    const float top = GetScreenHeight() - BOXSIZE;
    for (int i = 0; i < blockTypesSize; i++) {
      const float x = left + i * BOXSIZE;
      DrawRectangle(x, top, BOXSIZE, BOXSIZE,
                    activeBlockType == i ? WHITE : GRAY);
      const Rectangle inner = {x + BORDER, top + BORDER, BOXSIZE - 2 * BORDER,
                               BOXSIZE - 2 * BORDER};
      // The block's colour, with its texture drawn over it once it has one.
      DrawRectangleRec(inner, blockColor(blockTypes[i]));
      if (std::optional<Tex> tex = blockTex(blockTypes[i], BlockFace::Side)) {
        const Texture2D& texture = assets.get(*tex);
        DrawTexturePro(texture,
                       {0, 0, (float)texture.width, (float)texture.height},
                       inner, {0, 0}, 0, WHITE);
      }
    }
  }

  if (paused) {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {0, 0, 0, 90});

    DrawText("Paused", GetScreenWidth() / 2 - MeasureText("Paused", 50) / 2,
             GetScreenHeight() / 2 - 25, 50, WHITE);

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
    // Client::connect() retries on its own, so there's no separate "failed"
    // state to show.
    const char* msg = client.getKickReason() ? client.getKickReason()->c_str()
                                             : "Connecting...";
    Color col = client.getKickReason() ? RED : WHITE;
    DrawText(msg, GetScreenWidth() / 2 - MeasureText(msg, 30) / 2, 60, 30, col);
  } else if (!paused && !chunkUnderPlayerLoaded()) {
    DrawText("Loading...",
             GetScreenWidth() / 2 - MeasureText("Loading...", 50) / 2,
             GetScreenHeight() / 2 - 25, 50, WHITE);
  } else if (!paused) {
    // ==== draw crosshair ==== //
    Vector2 centre = {(float)GetScreenWidth() / 2,
                      (float)GetScreenHeight() / 2};
    Ray facing = {};

    facing.position = camera.position;
    facing.direction =
        Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    DrawCircleV(centre, (float)GetScreenHeight() / 480,
                client.aimingAtPlayer(facing) ? Color{255, 200, 200, 200}
                                              : Color{255, 255, 255, 255});
    DrawCircleLinesV(centre, (float)GetScreenHeight() / 200,
                     client.aimingAtPlayer(facing) ? Color{255, 50, 50, 100}
                                                   : Color{255, 255, 255, 100});
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
    client
        .disconnect();  // applyNetworkUpdates starts a fresh session next frame
  }
  if (IsKeyPressed(KEY_F7)) {
    nearPlaneStep = (nearPlaneStep + 1) % 5;
    // Far only has to clear the furthest block drawn, so it tracks render
    // distance.
    rlSetClipPlanes(NEAR_PLANES[nearPlaneStep],
                    GameState::shared().getRenderDistance() + 100.0);
  }

  constexpr int ROWSIZE = 30;
  constexpr int FONTSIZE = 20;

  if (showDebug) {
    int rowPos = 10;

    // NOTE: The +1 with black text is for readability (same as a drop shadow)

    const char* fps = TextFormat("FPS: %d", GetFPS());
    DrawText(fps, 11, rowPos + 1, FONTSIZE, BLACK);
    DrawText(fps, 10, rowPos, FONTSIZE, LIME);
    rowPos += ROWSIZE;

    Vector3 pos = player.getTransform().translation;

    DrawText("Player pos:", 11, rowPos + 1, FONTSIZE, BLACK);
    DrawText("Player pos:", 10, rowPos, FONTSIZE, LIME);
    rowPos += ROWSIZE;

    DrawText(TextFormat("X: %i", (int)(pos.x / env::BLOCKSIZE.x)), 11,
             rowPos + 1, FONTSIZE, BLACK);
    DrawText(TextFormat("X: %i", (int)(pos.x / env::BLOCKSIZE.x)), 10, rowPos,
             FONTSIZE, LIME);
    rowPos += ROWSIZE;

    DrawText(TextFormat("Y: %i", (int)(pos.y / env::BLOCKSIZE.y)), 11,
             rowPos + 1, FONTSIZE, BLACK);
    DrawText(TextFormat("Y: %i", (int)(pos.y / env::BLOCKSIZE.y)), 10, rowPos,
             FONTSIZE, LIME);
    rowPos += ROWSIZE;

    DrawText(TextFormat("Z: %i", (int)(pos.z / env::BLOCKSIZE.z)), 11,
             rowPos + 1, FONTSIZE, BLACK);
    DrawText(TextFormat("Z: %i", (int)(pos.z / env::BLOCKSIZE.z)), 10, rowPos,
             FONTSIZE, LIME);
    rowPos += ROWSIZE;

    const char* chunkText =
        TextFormat("Chunk: %d, %d (F4 borders)", World::streamChunkCoord(pos.x),
                   World::streamChunkCoord(pos.z));
    DrawText(chunkText, 11, rowPos + 1, FONTSIZE, BLACK);
    DrawText(chunkText, 10, rowPos, FONTSIZE, LIME);
    rowPos += ROWSIZE;

    const char* nearText =
        TextFormat("F7 near: %.2f", NEAR_PLANES[nearPlaneStep]);
    DrawText(nearText, 11, rowPos + 1, FONTSIZE, BLACK);
    DrawText(nearText, 10, rowPos, FONTSIZE, LIME);
    rowPos += ROWSIZE;

    rowPos += ROWSIZE / 2;  // small gap before the next section

    DrawText("World:", 10, rowPos, FONTSIZE, RED);
    rowPos += ROWSIZE;

    DrawText(TextFormat("World time: %f", world.getTime()), 10, rowPos,
             FONTSIZE, RED);
    rowPos += ROWSIZE;

    DrawText(TextFormat("Objects: %zu", world.getObjects().size()), 10, rowPos,
             FONTSIZE, RED);
    rowPos += ROWSIZE;

    DrawText(TextFormat("Faces drawn: %zu", renderer.getLastDrawnCount()), 10,
             rowPos, FONTSIZE, RED);
    rowPos += ROWSIZE;

    DrawText(TextFormat("Player update: %.2f ms", playerUpdateMs), 10, rowPos,
             FONTSIZE, RED);
    rowPos += ROWSIZE;

    DrawText(TextFormat("drawObjects: %.2f ms", drawObjectsMs), 10, rowPos,
             FONTSIZE, RED);
    rowPos += ROWSIZE;

    DrawText(TextFormat("cull/build: %.2f ms", renderer.getLastCullMs()), 10,
             rowPos, FONTSIZE, RED);
    rowPos += ROWSIZE;

    DrawText(TextFormat("draw: %.2f ms", renderer.getLastGpuMs()), 10, rowPos,
             FONTSIZE, RED);
    rowPos += ROWSIZE;

    if (GameState::shared().getShadows()) {
      DrawText(TextFormat("Shadows: %.2f ms", renderer.getLastShadowMapMs()),
               10, rowPos, FONTSIZE, RED);
      rowPos += ROWSIZE;

      const Texture2D& shadowMap = renderer.getShadowDepth();
      DrawTexturePro(shadowMap,
                     {0, 0, static_cast<float>(shadowMap.width),
                      -static_cast<float>(shadowMap.height)},
                     {static_cast<float>(GetScreenWidth() - 10 -
                                         ((float)GetScreenWidth() / 8)),
                      10, static_cast<float>(GetScreenWidth()) / 8,
                      static_cast<float>(GetScreenWidth()) / 8},
                     {0, 0}, 0, WHITE);
    }

    rowPos += ROWSIZE / 2;

    DrawText("Network:", 11, rowPos + 1, FONTSIZE, BLACK);
    DrawText("Network:", 10, rowPos, FONTSIZE, YELLOW);
    rowPos += ROWSIZE;

    DrawText(TextFormat("Connected: %s", client.isConnected() ? "yes" : "no"),
             11, rowPos + 1, FONTSIZE, BLACK);
    DrawText(TextFormat("Connected: %s", client.isConnected() ? "yes" : "no"),
             10, rowPos, FONTSIZE, YELLOW);
    rowPos += ROWSIZE;

    DrawText(TextFormat("Player ID: %d", client.getPlayerId()), 11, rowPos + 1,
             FONTSIZE, BLACK);
    DrawText(TextFormat("Player ID: %d", client.getPlayerId()), 10, rowPos,
             FONTSIZE, YELLOW);
    rowPos += ROWSIZE;

    DrawText(TextFormat("Online players: %zu", client.getPlayers().size()), 11,
             rowPos + 1, FONTSIZE, BLACK);
    DrawText(TextFormat("Online players: %zu", client.getPlayers().size()), 10,
             rowPos, FONTSIZE, YELLOW);
    rowPos += ROWSIZE;

    DrawText(TextFormat("Bullets: %zu", client.getBullets().size()), 11,
             rowPos + 1, FONTSIZE, BLACK);
    DrawText(TextFormat("Bullets: %zu", client.getBullets().size()), 10, rowPos,
             FONTSIZE, YELLOW);
    rowPos += ROWSIZE;
  }
#endif
}
