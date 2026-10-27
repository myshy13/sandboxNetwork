#include "settings.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <string>

#include "GameState/gameState.hpp"
#include "Input/inputState.hpp"
#include "Raylib/drawText.hpp"
#include "UI/Button/button.hpp"
#include "UI/TextBox/textbox.hpp"
#include "UI/Theme/theme.hpp"

static std::string noFormat(int units) { return std::to_string(units); }

// Render and shadow distance are stored in chunks.
static std::string inChunks(int chunks) {
  return std::to_string(chunks) + " chunks";
}

Settings::Settings(Input& input)
    : renderDistanceSlider(
          "Render distance: ", GameState::MIN_RENDER_DISTANCE,
          GameState::MAX_RENDER_DISTANCE,
          [] { return GameState::shared().getRenderDistance(); },
          [](int v) { GameState::shared().setRenderDistance(v); }, inChunks),
      shadowRadiusSlider(
          "Shadow distance: ", GameState::MIN_SHADOW_RADIUS,
          GameState::MAX_SHADOW_RADIUS,
          [] { return GameState::shared().getShadowRadius(); },
          [](int v) { GameState::shared().setShadowRadius(v); }, inChunks),
      targetFpsSlider(
          "Target FPS: ", 10, 512,
          [] { return GameState::shared().getTargetFps(); },
          [](int v) { GameState::shared().setTargetFps(v); }, noFormat),
      exitButton([] { GameState::shared().setMenuState(MenuState::Home); }, "<",
                 Rectangle{10, 10, 60, 60}, 50),
      vsyncButton(
          [] {
            // raylib owns the flag, so ask it instead of mirroring the state in
            // GameState
            if (IsWindowState(FLAG_VSYNC_HINT)) {
              ClearWindowState(FLAG_VSYNC_HINT);
            } else {
              SetWindowState(FLAG_VSYNC_HINT);
            }
          },
          "", Rectangle{0, 0, 120, 50}, 30),
      interpolationButton([] { GameState::shared().toggleInterpolation(); }, "",
                          Rectangle{0, 0, 120, 50}, 30),
      shadowsButton([] { GameState::shared().toggleShadows(); }, "",
                    Rectangle{0, 0, 120, 50}, 30),
      autoJumpButton([] { GameState::shared().toggleAutoJump(); }, "",
                     Rectangle{0, 0, 120, 50}, 30),
      serverPort(port, 0, false, ButtonStyles::Secondary, input, "Port"),
      serverIp(ip, 0, false, ButtonStyles::Secondary, input, "Server ip"),
      setServerButton(
          [&]() {
            if (gameState.setServer(ip, port)) {
              serverIp.setStatus(TextBoxStatus::Success, 3.0f);
              serverPort.setStatus(TextBoxStatus::Success, 3.0f);
            } else {
              serverIp.setStatus(TextBoxStatus::Error, 3.0f);
              serverPort.setStatus(TextBoxStatus::Error, 3.0f);
            }
          },
          "Set server",
          Rectangle{0, 0, BUTTON_WIDTH * 1.5f, BUTTON_HEIGHT},  // set in frame()
          50, ButtonStyles::Primary),
      input(input) {}

void Settings::frame() {
  if (input.down(Action::Exit)) {
    gameState.setMenuState(MenuState::Home);
    input.setMouseLook(false);  // reset to centre on a gamepad
  }
  // Bottom edge of the last row plus a margin; scrolling stops when it meets
  // the window's bottom.
  constexpr float contentBottom = 1200;
  scroll -= input.getScroll();  // inverted scrolling
  // Recomputed every frame: the window can shrink or rotate, which lowers the
  // limit under the current scroll.
  const float maxScroll = std::fmax(0.0f, contentBottom - GetScreenHeight());
  scroll = std::clamp(scroll, 0.0f, maxScroll);

  const float left = GetScreenWidth() / 5.0f;
  const float width = left * 3;
  const float toggleX = left * 4 - 120;
  const bool pressed = input.pressed(Action::Click);
  const bool down = input.down(Action::Click);

  // ==== layout (follows the window width) ==== //
  renderDistanceSlider.setBar({left, 200 - scroll, width, 20});
  shadowRadiusSlider.setBar({left, 290 - scroll, width, 20});
  targetFpsSlider.setBar({left, 380 - scroll, width, 20});
  vsyncButton.setRec({toggleX, 460 - scroll, 120, 50});
  interpolationButton.setRec({toggleX, 530 - scroll, 120, 50});
  shadowsButton.setRec({toggleX, 600 - scroll, 120, 50});
  autoJumpButton.setRec({toggleX, 670 - scroll, 120, 50});

  serverIp.setYPos(670 + BUTTON_HEIGHT + 20 - scroll);
  serverPort.setYPos(670 + BUTTON_HEIGHT * 2 + 30 - scroll);
  setServerButton.setRec(
      Rectangle{(GetScreenWidth() - BUTTON_WIDTH * 1.5f) / 2.0f,
                670 + BUTTON_HEIGHT * 3 + 40 - scroll, BUTTON_WIDTH * 1.5f,
                BUTTON_HEIGHT});

  vsyncButton.setText(IsWindowState(FLAG_VSYNC_HINT) ? "On" : "Off");
  interpolationButton.setText(gameState.getInterpolation() ? "On" : "Off");
  shadowsButton.setText(gameState.getShadows() ? "On" : "Off");
  autoJumpButton.setText(gameState.getAutoJump() ? "On" : "Off");

  BeginDrawing();
  ClearBackground(Theme::Ground);

  gameState.getBackground().frame(input);

  exitButton.frame(input, pressed);
  renderDistanceSlider.frame(input, pressed, down);
  shadowRadiusSlider.frame(input, pressed, down);
  // Hidden on web, where the browser paces the loop (default 0 = uncapped).
  if (GameState::DEFAULT_TARGET_FPS > 0)
    targetFpsSlider.frame(input, pressed, down);

  // ==== toggles ==== //
  DrawTextFont("VSync:", left, 475 - scroll, 30, Theme::Text);
  vsyncButton.frame(input, pressed);
  DrawTextFont("Interpolation:", left, 545 - scroll, 30, Theme::Text);
  interpolationButton.frame(input, pressed);
  DrawTextFont("Shadows:", left, 615 - scroll, 30, Theme::Text);
  shadowsButton.frame(input, pressed);
  DrawTextFont("Auto jump:", left, 685 - scroll, 30, Theme::Text);
  autoJumpButton.frame(input, pressed);

  serverIp.frame(GetFrameTime());
  serverPort.frame(GetFrameTime());
  setServerButton.frame(input, input.pressed(Action::Click));

  input.drawCursor();

  EndDrawing();
}
