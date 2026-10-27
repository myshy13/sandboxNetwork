#pragma once

#include "Auth/auth.hpp"
#include "GameState/gameState.hpp"
#include "UI/Background/background.hpp"
#include "UI/Button/button.hpp"
#include "UI/TextBox/textbox.hpp"

enum class AccountScreen { Login, Signup };

class AccountMenu {
 private:
  GameState &gameState = GameState::shared();
  AccountScreen accountScreen = AccountScreen::Login;

  Background &background = gameState.getBackground();

  std::string username;
  std::string password;

  Button exitButton;
  
  bool waiting{false};
  double errorShownAt{-1.0};  // GetTime() when the current error first showed; < 0 = not showing

  TextBox usernameBox;
  TextBox passwordBox;
  Button loginButton;
  Button signupButton;

  void submitLogin();
  void submitSignup();

  Input &input;

  Auth& auth;

 public:
  AccountMenu(Input& input, Auth& auth);
  void frame();
};
