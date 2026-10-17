#pragma once

#include <raylib.h>

#include <vector>

#include "Input/input.hpp"
#include "UI/Background/background.hpp"
#include "UI/Button/button.hpp"

class Home {
 private:
  std::vector<Button> buttons;
  Input& input;

  std::vector<Particle> particles;

  Background& background;

 public:
  Home(Input& input);
  void frame();
};
