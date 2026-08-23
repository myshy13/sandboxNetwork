#pragma once

class GameState {
public:
  static GameState &shared() {
    static GameState instance;
    return instance;
  }

  float damageFlashTimer          = 0.0f;
  const float damageFlashDuration = 0.5f;

  float greenFlashTimer                     = 0.0f;
  constexpr static float greenFlashDuration = 0.2f;

  void TriggerDamageFlash() {
    damageFlashTimer = damageFlashDuration;
  }
  void TriggerGreenFlash() {
    greenFlashTimer = greenFlashDuration;
  }

private:
  GameState() = default;
};