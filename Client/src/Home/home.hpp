#pragma once

#include <raylib.h>

#include <vector>

#include "Input/input.hpp"
#include "UI/Button/button.hpp"

class Home {
  std::vector<Button> buttons;
  Input& input;

 public:
  Home(Input& input);
  void frame();
};
