#pragma once

#include "UI/Button/button.hpp"
#include <raylib.h>
#include <vector>

class Home {
  std::vector<Button> buttons;

public:
  void frame();
  Home();
};
