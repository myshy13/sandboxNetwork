#pragma once

#include "AssetManager/manager.hpp"
#include "Client/client.hpp"
#include "Player/player.hpp"
#include "Renderer/renderer.hpp"
#include "Shaders/lighting.hpp"
#include "World/world.hpp"
#include <raylib.h>
#include <string>

class Game {
public:
  Game(const AssetManager &a);
  ~Game();
  Game(const Game &)            = delete;
  Game &operator=(const Game &) = delete;

  void frame();

private:
  const AssetManager &assets;
  Camera3D camera{{10.0f, 10.0f, 10.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, 70.0f, CAMERA_PERSPECTIVE};
  Client client;
  Lighting lighting;
  Player player;
  World world;
  Renderer renderer;

  // ==== per-frame state ==== //
  bool paused = false;
  bool inChat = false;
  std::string chatInput;
  float playerPosCooldown = Client::POS_UPDATE_INTERVAL;
  float bulletCooldown    = 0.0f;
  float placeCooldown     = 0.0f;
#ifdef DEBUG
  bool showDebug        = false;
  bool showChunkBorders = false; // F4
  double playerUpdateMs = 0.0; // Player::Update, incl. the per-block collision scan
  double drawObjectsMs  = 0.0; // Renderer::drawObjects, incl. frustum cull + instancing
#endif

  // ==== update ==== //
  void applyNetworkUpdates();
  void handlePause();
  void handleChatInput();
  void updatePlayer(float dt);
  void sendPosition(float dt);
  void handleActions(float dt);

  // ==== draw ==== //
  void drawScene(float dt);
  void drawChunkBorders();
  void drawHealthBar();
  void drawChat();
  void drawScoreboard();
  void drawOverlays(float dt);
  void drawDebug();
  bool chunkUnderPlayerLoaded() const;
};
