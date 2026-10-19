#include "accountMenu.hpp"

#include <raylib.h>

#include "GameState/gameState.hpp"
#include "Input/inputState.hpp"
#include "Raylib/drawText.hpp"
#include "UI/Theme/theme.hpp"

void AccountMenu::frame() {
  BeginDrawing();
  ClearBackground(Theme::Ground);
  background.frame(input);
  if (accountScreen == AccountScreen::Login) {
    frameLogin();
  } else if (accountScreen == AccountScreen::Signup) {
    frameSignup();
  }
  exitButton.frame(input, input.down(Action::Click));
  EndDrawing();
}

void AccountMenu::frameSignup() {}

AccountMenu::AccountMenu(Input &input)
    : exitButton(
          Button([]() { GameState::shared().setMenuState(MenuState::Home); },
                 "<", Rectangle{10, 10, 50, 50}, 40, ButtonStyles::Primary)),
      // std::string& content, float yPos, bool masked, ButtonStyle style,
      // Input& input
      usernameBox(username, (float)GetScreenWidth() / 2, false, ButtonStyles::Primary, input),
      passwordBox(password, (float)GetScreenWidth() / 2 + 100, true, ButtonStyles::Primary, input),
      input(input){};

void AccountMenu::frameLogin() {
  DrawTextFont("Login", GetScreenWidth() / 2 - MeasureTextFont("Login", 50) / 2,
               100, 50, WHITE);

  usernameBox.frame();
  passwordBox.frame();
}