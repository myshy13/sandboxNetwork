#pragma once

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

  TextBox usernameBox;
  TextBox passwordBox;
  Button loginButton;
  Button signupButton;

  void submitLogin();
  void submitSignup();

  Input &input;

 public:
  AccountMenu(Input &input);
  void frame();
};
