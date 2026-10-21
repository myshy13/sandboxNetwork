#pragma once

#include <raylib.h>

#include <vector>

#include "Auth/auth.hpp"
#include "Input/input.hpp"
#include "UI/Background/background.hpp"
#include "UI/Button/button.hpp"

class Home {
 private:
  std::vector<Button> buttons;
  Input& input;
  Auth& auth;

  std::vector<Particle> particles;

  Background& background;
  Button loginButton;
  Button logoutButton;

 public:
  Home(Input& input, Auth& auth);
  void frame();
};
