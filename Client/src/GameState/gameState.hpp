#pragma once

#include <raylib.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <string>
#include <utility>

#include "UI/Background/background.hpp"
#include "env.hpp"
#include "sharedEnv.hpp"

enum class MenuState { Home, Settings, Playing, Login };

// A bare hostname or IPv4 (no scheme, no port); the transport's lookup decides
// if it exists.
inline bool validateHost(const std::string& host) {
  if (host.empty() || host.size() > 253) return false;
  if (host.front() == '.' || host.front() == '-' || host.back() == '.' ||
      host.back() == '-')
    return false;
  return std::all_of(host.begin(), host.end(), [](unsigned char c) {
    return std::isalnum(c) || c == '.' || c == '-';
  });
}

// The whole string must be a number in 1-65535 ("80abc" is rejected).
inline bool parsePort(const std::string& text, int& port) {
  const char* end = text.data() + text.size();
  auto [ptr, ec] = std::from_chars(text.data(), end, port);
  return ec == std::errc{} && ptr == end && port > 0 && port <= 65535;
}

class GameState {
 private:
  MenuState menu{MenuState::Home};

  Background background;
  bool windowCloseRequested{false};

  // ==== settings ==== //
  int renderDistance{(int)(NET_CHUNK_SIZE / 2 * MAX_CHUNK_DISTANCE)};
  bool interpolation{true};
  bool shadows{SHADOWS_DEFAULT != 0};     // set per platform in CMakeLists.txt
  bool autoJump{AUTO_JUMP_DEFAULT != 0};  // set per platform in CMakeLists.txt
  int shadowRadius{DEFAULT_SHADOW_RADIUS};
  int targetFps{DEFAULT_TARGET_FPS};

  std::string serverIp = env::SERVER_IP;  // bare hostname or IPv4
  int serverPort = env::SERVER_PORT;

 public:
  Font mainFont{};

  // automatically adjusts from shared env
  static constexpr int MIN_RENDER_DISTANCE = env::BLOCKSIZE.x * 8 * 5;
  static constexpr int MAX_RENDER_DISTANCE =
      MAX_CHUNK_DISTANCE * (env::BLOCKSIZE.x) * 16;

  // How far around the player the sun's shadow map reaches (world units).
  // same as render distance
  static constexpr int MIN_SHADOW_RADIUS = MIN_RENDER_DISTANCE;
  static constexpr int MAX_SHADOW_RADIUS = MAX_RENDER_DISTANCE;
  static constexpr int DEFAULT_SHADOW_RADIUS = 200;
  static constexpr int DEFAULT_TARGET_FPS =
      TARGET_FPS_DEFAULT;  // 0 = uncapped, see CMakeLists.txt

  static GameState& shared() {
    static GameState instance;
    return instance;
  }

  float damageFlashTimer = 0.0f;
  const float flashDuration = 0.1f;

  float greenFlashTimer = 0.0f;

  void TriggerDamageFlash() { damageFlashTimer = flashDuration; }

  void TriggerGreenFlash() { greenFlashTimer = flashDuration; }

  const Font& getMainFont() { return mainFont; }

  void setMainFont(const Font& f) { mainFont = std::move(f); }

  // ==== set and get ==== //
  const MenuState& getMenu() const { return menu; }

  void setMenuState(const MenuState& m) { menu = m; }

  void setRenderDistance(int distance) { renderDistance = distance; }

  const int& getRenderDistance() const { return renderDistance; }

  void toggleInterpolation() { interpolation = !interpolation; }

  void toggleShadows() { shadows = !shadows; }

  void toggleAutoJump() { autoJump = !autoJump; }

  bool getInterpolation() const { return interpolation; }

  bool getShadows() const { return shadows; }

  bool getAutoJump() const { return autoJump; }

  void setShadowRadius(int radius) { shadowRadius = radius; }

  int getShadowRadius() const { return shadowRadius; }

  void setTargetFps(int fps) {
    targetFps = fps;
    SetTargetFPS(fps);
  }

  int getTargetFps() const { return targetFps; }

  Background& getBackground() { return background; }

  void closeWindow() { windowCloseRequested = true; }

  bool shouldClose() const { return windowCloseRequested; }

  const std::string& getIp() const { return serverIp; };

  // Both or neither: a good host with a bad port must not change the host.
  bool setServer(const std::string& ip, const std::string& portText) {
    int port = 0;
    if (!validateHost(ip) || !parsePort(portText, port)) return false;
    serverIp   = ip;
    serverPort = port;
    return true;
  }

  int getPort() const { return serverPort; }
};