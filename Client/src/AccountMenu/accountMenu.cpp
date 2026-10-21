#include "accountMenu.hpp"

#include <raylib.h>

#include "Auth/auth.hpp"
#include "GameState/gameState.hpp"
#include "Input/inputState.hpp"
#include "Raylib/drawText.hpp"
#include "UI/Button/button.hpp"
#include "UI/Theme/theme.hpp"

void sizeButtons(Button& loginButton, Button& signupButton) {
  Rectangle loginButtonRec;
  loginButtonRec.height = BUTTON_HEIGHT;
  loginButtonRec.width = BUTTON_WIDTH;
  loginButtonRec.y = GetScreenHeight() - 100;
  loginButtonRec.x = GetScreenWidth() / 2.0f - BUTTON_WIDTH;

  Rectangle signupButtonRec;
  signupButtonRec.height = BUTTON_HEIGHT;
  signupButtonRec.width = BUTTON_WIDTH;
  signupButtonRec.y = GetScreenHeight() - 100;
  signupButtonRec.x = GetScreenWidth() / 2.0f + 10;

  loginButton.setRec(loginButtonRec);
  signupButton.setRec(signupButtonRec);
}

void AccountMenu::frame() {
  if (auth.getState() == State::LoggedIn) {
    gameState.setMenuState(MenuState::Home);
    return;
  }
  waiting = auth.getState() == State::Working;

  BeginDrawing();
  ClearBackground(Theme::Ground);
  background.frame(input);
  if (IsWindowResized()) {
    sizeButtons(loginButton, signupButton);

    usernameBox.setYPos((float)GetScreenHeight() / 2 - 50);
    passwordBox.setYPos((float)GetScreenHeight() / 2 + 110);
  }
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

  if (auth.getState() != State::Working) {
    waiting = false;
    if (auth.getState() == State::Error) {
      const std::string errorMessage = auth.getErrorMesssage();
      const char* msg = errorMessage.c_str();
      int fontSize = 50;
      int width = MeasureTextFont(msg, fontSize);
      const float maxWidth = GetScreenWidth() * 0.9;
      while (width > maxWidth) {
        fontSize *= (maxWidth / width);
        width = MeasureTextFont(msg, fontSize);
      }
      DrawTextFont(msg, GetScreenWidth() / 2 - width / 2 + 4,
                   GetScreenHeight() / 2 - (fontSize / 2) + 4, fontSize,
                   Theme::RedDark);
      DrawTextFont(msg, GetScreenWidth() / 2 - width / 2,
                   GetScreenHeight() / 2 - (fontSize / 2), fontSize,
                   Theme::Red);
    } 
  }
  if (waiting) {
    DrawTextFont(
        "Loading...",
        GetScreenWidth() / 2.0f - MeasureTextFont("Loading...", 50) / 2.0f + 3,
        GetScreenHeight() / 2.0f - 25 + 3, 50, Theme::TextMuted);
    DrawTextFont(
        "Loading...",
        GetScreenWidth() / 2.0f - MeasureTextFont("Loading...", 50) / 2.0f,
        GetScreenHeight() / 2.0f - 25, 50, Theme::Text);
  }

  EndDrawing();
}

void AccountMenu::submitLogin() {
  if (!waiting) {
    auth.login(username, password);
    waiting = true;
  }
};

void AccountMenu::submitSignup() {
  if (!waiting) {
    auth.signup(username, password);
    waiting = true;
  }
};

AccountMenu::AccountMenu(Input& input, Auth& auth)
    : exitButton(
          Button([]() { GameState::shared().setMenuState(MenuState::Home); },
                 "<", Rectangle{10, 10, 50, 50}, 40, ButtonStyles::Primary)),
      usernameBox(username, (float)GetScreenHeight() / 2 - 50, false,
                  ButtonStyles::Primary, input),
      // std::string& content, float yPos, bool masked, ButtonStyle style,
      // Input& input
      passwordBox(password, (float)GetScreenHeight() / 2 + 110, true,
                  ButtonStyles::Primary, input),
      loginButton([&]() { submitLogin(); }, "Login",
                  (float)GetScreenHeight() - 100, ButtonStyles::Primary),
      signupButton([&]() { submitSignup(); }, "Signup",
                   (float)GetScreenHeight() - 100, ButtonStyles::Secondary),
      input(input),
      auth(auth) {
  sizeButtons(loginButton, signupButton);
};
