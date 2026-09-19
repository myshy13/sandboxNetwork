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