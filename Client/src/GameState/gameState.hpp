#pragma once

class GameState {
public:
  static GameState &shared() {
    static GameState instance;
    return instance;
  }

  float damageFlashTimer          = 0.0f;
  const float flashDuration       = 0.1f;

  float greenFlashTimer = 0.0f;

  void TriggerDamageFlash() {
    damageFlashTimer = flashDuration;
  }
  void TriggerGreenFlash() {
    greenFlashTimer = flashDuration;
  }

private:
  GameState() = default;
};