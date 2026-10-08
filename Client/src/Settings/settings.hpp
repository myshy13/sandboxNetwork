#pragma once
#include "GameState/gameState.hpp"
#include "Input/input.hpp"
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
    Input& input;

   public:
    Settings(Input& input);
    void frame();
};
