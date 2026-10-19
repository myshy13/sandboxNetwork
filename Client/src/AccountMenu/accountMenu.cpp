#include "accountMenu.hpp"

#include <raylib.h>

#include "GameState/gameState.hpp"
#include "Input/inputState.hpp"
#include "Raylib/drawText.hpp"
#include "UI/Button/button.hpp"
#include "UI/Theme/theme.hpp"

void AccountMenu::frame() {
  BeginDrawing();
  ClearBackground(Theme::Ground);
  background.frame(input);
  const std::string titleText =
      accountScreen == AccountScreen::Login ? "Login" : "Signup";
  DrawTextFont(
      titleText.c_str(),
      GetScreenWidth() / 2 - MeasureTextFont(titleText.c_str(), 50) / 2, 100,
      50, WHITE);

  DrawTextFont("Username", GetScreenWidth() / 2 - BUTTON_WIDTH / 2 + 2,
               (float)GetScreenHeight() / 2 - 100 + 2, 40, Theme::TextMuted);
  DrawTextFont("Username", GetScreenWidth() / 2 - BUTTON_WIDTH / 2,
               (float)GetScreenHeight() / 2 - 100, 40, Theme::Text);
  usernameBox.frame();
  DrawTextFont("Password", GetScreenWidth() / 2 - BUTTON_WIDTH / 2 + 2,
               (float)GetScreenHeight() / 2 + 110 - 50 + 2, 40,
               Theme::TextMuted);
  DrawTextFont("Password", GetScreenWidth() / 2 - BUTTON_WIDTH / 2,
               (float)GetScreenHeight() / 2 + 110 - 50, 40, Theme::Text);
  passwordBox.frame();

  const bool clicked = input.pressed(Action::Click);
  loginButton.frame(input, clicked);
  signupButton.frame(input, clicked);

  exitButton.frame(input, clicked);
  EndDrawing();
}

void AccountMenu::submitLogin() {};
void AccountMenu::submitSignup() {};

AccountMenu::AccountMenu(Input& input)
    : exitButton(
          Button([]() { GameState::shared().setMenuState(MenuState::Home); },
                 "<", Rectangle{10, 10, 50, 50}, 40, ButtonStyles::Primary)),
      // std::string& content, float yPos, bool masked, ButtonStyle style,
      // Input& input
      usernameBox(username, (float)GetScreenHeight() / 2 - 50, false,
                  ButtonStyles::Primary, input),
      passwordBox(password, (float)GetScreenHeight() / 2 + 110, true,
                  ButtonStyles::Primary, input),
      loginButton([&]() { submitLogin(); }, "Login",
                  (float)GetScreenHeight() - 100, ButtonStyles::Primary),
      signupButton([&]() { submitSignup(); }, "Signup",
                   (float)GetScreenHeight() - 100, ButtonStyles::Secondary),
      input(input) {
  Rectangle loginButtonRec;
  loginButtonRec.height = BUTTON_HEIGHT;
  loginButtonRec.width = (float)BUTTON_WIDTH;
  loginButtonRec.y = (float)GetScreenHeight() - 100;
  loginButtonRec.x = GetScreenWidth() / 2.0f - BUTTON_WIDTH;
  Rectangle signupButtonRec;
  signupButtonRec.height = BUTTON_HEIGHT;
  signupButtonRec.width = BUTTON_WIDTH;
  signupButtonRec.y = (float)GetScreenHeight() - 100;
  signupButtonRec.x = GetScreenWidth() / 2.0f + 10;

  loginButton.setRec(loginButtonRec);
  signupButton.setRec(signupButtonRec);
};
