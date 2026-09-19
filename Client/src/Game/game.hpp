#pragma once

#include "Client/client.hpp"
#include "Player/player.hpp"
#include "Renderer/renderer.hpp"
#include "Shaders/lighting.hpp"
#include "World/world.hpp"
#include <raylib.h>
#include <string>

// Owns every subsystem and the per-frame state; main() just calls frame() in a loop.
// The window must already be open when a Game is constructed (Lighting/Renderer need a GL context).
class Game {
public:
  Game();
  ~Game();
  Game(const Game &)            = delete;
  Game &operator=(const Game &) = delete;

  void frame();

private:
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
  float playerPosCooldown = 0.1667f;
  float bulletCooldown    = 0.0f;
  float placeCooldown     = 0.0f;
#ifdef DEBUG
  bool showDebug        = false;
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
  void drawHealthBar();
  void drawChat();
  void drawScoreboard();
  void drawOverlays(float dt);
  void drawDebug();
};
