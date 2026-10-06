#pragma once
#include "GameState/gameState.hpp"
#include "UI/Button/button.hpp"
#include "UI/Slider/slider.hpp"

class Settings {
  private:
    GameState &gameState = GameState::shared();
    Button exitButton;
    Button vsyncButton;
    Button interpolationButton;
    Button shadowsButton;
    Slider renderDistanceSlider;
    Slider shadowRadiusSlider;
    Slider targetFpsSlider;

   public:
    Settings();
    void frame();
};
