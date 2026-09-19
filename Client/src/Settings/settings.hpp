#pragma once
#include "GameState/gameState.hpp"
class Settings {
  private:
    GameState &gameState = GameState::shared();
  public:
    void frame();
};