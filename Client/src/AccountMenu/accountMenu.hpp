#pragma once

#include "Auth/auth.hpp"
#include "Auth/tokenStore.hpp"
#include "GameState/gameState.hpp"
#include "Net/httpClient.hpp"
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
