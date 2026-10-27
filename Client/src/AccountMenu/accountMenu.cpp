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

constexpr double ERROR_SECONDS =
    2.0;  // how long an ordinary error stays on screen
constexpr double BAN_SECONDS = 6.0;  // a ban with a reason to read stays longer

void drawTextSize(const std::string& message, int initialSize, float yMod = 0) {
  const char* msg = message.c_str();
  int fontSize = initialSize;
  int width = MeasureTextFont(msg, fontSize);
  const float maxWidth = GetScreenWidth() * 0.9;
  while (width > maxWidth) {
    fontSize *= (maxWidth / width);
    width = MeasureTextFont(msg, fontSize);
  }
  DrawTextFont(msg, GetScreenWidth() / 2 - width / 2 + 4,
               GetScreenHeight() / 2.0f - (fontSize / 2.0f) + 4 + yMod,
               fontSize, Theme::RedDark);
  DrawTextFont(msg, GetScreenWidth() / 2 - width / 2,
               GetScreenHeight() / 2.0f - (fontSize / 2.0f) + yMod, fontSize,
               Theme::Red);
}

void AccountMenu::frame() {
  if (auth.getState() == State::LoggedIn) {
    gameState.setMenuState(MenuState::Home);
    return;
  }
  waiting = auth.getState() == State::Working;
  // Any state but Error rearms the timer, so the next error gets its full time.
  if (auth.getState() != State::Error) errorShownAt = -1.0;

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
  usernameBox.frame(GetFrameTime());
  DrawTextFont("Password", GetScreenWidth() / 2 - BUTTON_WIDTH / 2 + 2,
               (float)GetScreenHeight() / 2 + 110 - 50 + 2, 40,
               Theme::TextMuted);
  DrawTextFont("Password", GetScreenWidth() / 2 - BUTTON_WIDTH / 2,
               (float)GetScreenHeight() / 2 + 110 - 50, 40, Theme::Text);
  passwordBox.frame(GetFrameTime());

  const bool clicked = input.pressed(Action::Click);
  loginButton.frame(input, clicked);
  signupButton.frame(input, clicked);

  exitButton.frame(input, clicked);

  if (auth.getState() != State::Working) {
    waiting = false;
    if (auth.getState() == State::Error) {
      // Timed from the first frame the error is seen; the state itself stays
      // Error.
      if (errorShownAt < 0) errorShownAt = GetTime();
      const std::string errorMessage = auth.getErrorMessage();
      const std::string banReason = auth.getBanReason();
      // A ban has a reason to read as well as the message, so it stays up
      // longer.
      const double showFor = banReason.empty() ? ERROR_SECONDS : BAN_SECONDS;
      if (GetTime() - errorShownAt < showFor) {
        drawTextSize(errorMessage, 50);
        drawTextSize("Reason: " + banReason, 20, 50);
      }
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
    password.clear();  // after the call: Auth has its own copy by now
    waiting = true;
  }
};

void AccountMenu::submitSignup() {
  if (!waiting) {
    auth.signup(username, password);
    password.clear();
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
