#include "home.hpp"

#include <raylib.h>
#include <raymath.h>

#include "GameState/gameState.hpp"
#include "Input/input.hpp"
#include "Input/inputState.hpp"
#include "Raylib/drawText.hpp"
#include "UI/Button/button.hpp"
#include "UI/TextBox/textbox.hpp"
#include "UI/Theme/theme.hpp"
#include "env.hpp"

Home::Home(Input& input, Auth& auth)
    : input(input),
      auth(auth),
      background(GameState::shared().getBackground()),
      loginButton(
          [&input](void) {
            GameState::shared().setMenuState(MenuState::Login);
            input.setMouseLook(false);
          },
          "Login", (float)GetScreenHeight() - 100, ButtonStyles::Secondary),
      logoutButton([&auth](void) { auth.logout(); }, "Sign out",
                   (float)GetScreenHeight() - 100, ButtonStyles::Secondary) {
  // play button
  Button playButton = Button(
      [&input](void) {
        GameState::shared().setMenuState(MenuState::Playing);
        input.setMouseLook(true);
      },
      "Play", (float)GetScreenHeight() / 2, ButtonStyles::Primary);
  buttons.push_back(playButton);

  // settings button

  Button settingsButton = Button(
      [&input](void) {
        GameState::shared().setMenuState(MenuState::Settings);
        input.setMouseLook(false);
      },
      "Settings", (float)GetScreenHeight() / 2 + 100, ButtonStyles::Secondary);
  buttons.push_back(settingsButton);

  // only desktop builds support a clean quit
#ifdef DESKTOP
  // quit button
  Button quitButton =
      Button([](void) { GameState::shared().closeWindow(); }, "Quit",
             (float)GetScreenHeight() / 2 + 200, ButtonStyles::Ghost);
  buttons.push_back(quitButton);
#endif

  // ==== init particles ==== //
  int particleCount = 10 + rand() % 10;
  const int sizes[3] = {16, 24, 32};
  for (int i = 0; i < particleCount; i++) {
    Particle p;
    p.home = {(float)GetRandomValue(0, GetScreenWidth()),
              (float)GetRandomValue(0, GetScreenHeight())};
    p.pos = p.home;
    p.size = (float)sizes[GetRandomValue(0, 2)];
    p.depth = 0.3f + (p.size - 16.0f) / 16.0f * 0.7f;  // bigger = reacts more
    p.phase = GetRandomValue(0, 628) / 100.0f;
    p.color = (i % 2 == 0) ? Theme::Blue : Theme::LogoRed;
    p.color.a = 110;  // see-through, so it stays behind the UI
    particles.push_back(p);
  }
}

void DrawLogo(const Font& font, float centerX, float top) {
  constexpr float SIZE = 80.0f;
  constexpr float LINE = 88.0f;  // line height
  constexpr float SHADOW = 6.0f;

  struct Word {
    const char* text;
    Color color;
    Color shadow;
  };

  const Word words[2] = {
      {"SANDBOX", Theme::Blue, Theme::BlueDark},
      {"NETWORK", Theme::LogoRed, Theme::RedDark},
  };

  for (int i = 0; i < 2; i++) {
    const Vector2 size = MeasureTextEx(font, words[i].text, SIZE, 0);
    // Round positions to whole pixels so the glyphs stay sharp
    const Vector2 pos = {std::roundf(centerX - size.x / 2),
                         std::roundf(top + i * LINE + (LINE - SIZE) / 2)};

    DrawTextEx(font, words[i].text, {pos.x + SHADOW, pos.y + SHADOW}, SIZE, 0,
               words[i].shadow);
    DrawTextEx(font, words[i].text, pos, SIZE, 0, words[i].color);
  }
}

void Home::frame() {
  // Everything, including "Loading...", has to be drawn between
  // Begin/EndDrawing to reach the screen.
  BeginDrawing();
  ClearBackground(Theme::Ground);

  background.frame(input);

  DrawLogo(GameState::shared().getMainFont(), (float)GetScreenWidth() / 2, 100);

  bool clicked = input.pressed(Action::Click);
  for (auto& b : buttons) {
    b.frame(input, clicked);
  }
  if (auth.getState() == State::LoggedIn) {
    logoutButton.frame(input, clicked);
  } else {
    loginButton.frame(input, clicked);
  }

  input.drawCursor();

  DrawTextFont(("v" + env::VERSION).c_str(), 20, (float)GetScreenHeight() - 40,
               20, Theme::TextMuted);

  EndDrawing();
}
