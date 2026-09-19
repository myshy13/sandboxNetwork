#pragma once
#include "GameState/gameState.hpp"

class Settings {
  private:
    GameState &gameState = GameState::shared();
    bool changingRenderDistance{false};

  public:
    void frame();
};