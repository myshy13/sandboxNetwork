#pragma once
#include "GameState/gameState.hpp"
#include "Input/input.hpp"
#include "UI/Button/button.hpp"
#include "UI/Slider/slider.hpp"
#include "UI/TextBox/textbox.hpp"

class Settings {
 private:
  GameState& gameState = GameState::shared();

  Slider renderDistanceSlider;
  Slider shadowRadiusSlider;
  Slider targetFpsSlider;

  Button exitButton;
  Button vsyncButton;
  Button interpolationButton;
  Button shadowsButton;
  Button autoJumpButton;

  TextBox serverPort;
  TextBox serverIp;

  Button setServerButton;

  Input& input;

  std::string ip;
  std::string port;

  float scroll{0.0f};

 public:
  Settings(Input& input);
  void frame();
};
