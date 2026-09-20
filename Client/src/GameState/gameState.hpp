#pragma once

enum class MenuState {
  HOME,
  SETTINGS,
  PLAYING
};

class GameState {
private:
  MenuState menu{MenuState::HOME};

  // ==== settings ==== //
  int renderDistance{500};
  bool interpolation{true};

public:
  static constexpr int MIN_RENDER_DISTANCE = 200;
  // The server holds at most env::MAX_VIEW_RADIUS (8) chunks of World::STREAM_CHUNK_SIZE (80) around you; keep in sync.
  static constexpr int MAX_RENDER_DISTANCE = 640;

  static GameState &shared() {
    static GameState instance;
    return instance;
  }

  float damageFlashTimer    = 0.0f;
  const float flashDuration = 0.1f;

  float greenFlashTimer = 0.0f;

  void TriggerDamageFlash() {
    damageFlashTimer = flashDuration;
  }
  void TriggerGreenFlash() {
    greenFlashTimer = flashDuration;
  }

  // ==== set and get ==== //
  const MenuState &getMenu() const {
    return menu;
  }
  void setMenuState(const MenuState &m) {
    menu = m;
  }
  void setRenderDistance(int distance) {
    renderDistance = distance;
  }
  const int &getRenderDistance() const {
    return renderDistance;
  }
  void toggleInterpolation() {
    interpolation = !interpolation;
  }
  bool getInterpolation() const {
    return interpolation;
  }

private:
  GameState() = default;
};