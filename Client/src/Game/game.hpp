#pragma once

#include <raylib.h>

#include <iterator>
#include <string>

#include "AssetManager/manager.hpp"
#include "Client/client.hpp"
#include "Player/player.hpp"
#include "Renderer/renderer.hpp"
#include "Shaders/lighting.hpp"
#include "World/world.hpp"

constexpr float lightUpdateCooldownTime = 0.1f;

class Game {
 public:
  Game(const AssetManager& a);
  ~Game();
  Game(const Game&) = delete;
  Game& operator=(const Game&) = delete;

  void frame();

 private:
  // Hotbar slots, selected with the number keys.
  static constexpr BlockType blockTypes[] = {
      BlockType::Dirt, BlockType::Grass,  BlockType::Leaves,
      BlockType::Wood, BlockType::Planks, BlockType::Water};
  static constexpr int blockTypesSize = std::size(blockTypes);
  int activeBlockType = 0;

  // Vertical field of view in degrees; holding C narrows it to zoom.
  static constexpr float BASE_FOV = 70.0f;
  static constexpr float ZOOM_FOV = 25.0f;

  const AssetManager& assets;
  Camera3D camera{{10.0f, 10.0f, 10.0f},
                  {0.0f, 0.0f, 0.0f},
                  {0.0f, 1.0f, 0.0f},
                  BASE_FOV,
                  CAMERA_PERSPECTIVE};
  Client client;
  Lighting lighting;
  Player player;
  World world;
  Renderer renderer;

  int sunLights[4] = {-1, -1, -1, -1};
  float lightUpdateCooldown{lightUpdateCooldownTime};

  // The scene renders into a texture RENDER_SCALE times the window size, then
  // is scaled down to the window (supersampling: smoother edges, more cost).
  static constexpr float RENDER_SCALE = 2.0f;
  static RenderTexture makeTarget();
  RenderTexture target = makeTarget();

  // ==== per-frame state ==== //
  bool paused = false;
  bool inChat = false;
  std::string chatInput;
  float playerPosCooldown = Client::POS_UPDATE_INTERVAL;
  float bulletCooldown = 0.0f;
  int sentViewRadius =
      -1;  // chunks the server was last told; -1 = nothing sent this session
  float placeCooldown = 0.0f;
#ifdef DEBUG
  bool showDebug = false;
  bool showChunkBorders = false;    // F4
  bool showCollisionDebug = false;  // F5
  int nearPlaneStep = 0;  // F7: cycles NEAR_PLANES, for depth precision
  static constexpr double NEAR_PLANES[5] = {0.01, 0.5, 1.0, 2.0, 4.0};
  double playerUpdateMs =
      0.0;  // Player::Update, incl. the per-block collision scan
  double drawObjectsMs =
      0.0;  // Renderer::drawObjects, incl. frustum cull + instancing
#endif

  // ==== update ==== //
  void applyNetworkUpdates();
  void syncViewRadius();
  void handlePause();
  void handleChatInput();
  void updatePlayer(float dt);
  void sendPosition(float dt);
  void handleActions(float dt);

  // ==== draw ==== //
  void drawScene(float dt);
  void drawChunkBorders();
  void drawCollisionDebug();
  void drawHealthBar();
  void drawChat();
  void drawScoreboard();
  void drawOverlays(float dt);
  void drawDebug();
  bool chunkUnderPlayerLoaded() const;
  void updateLighting(float dt);
};
