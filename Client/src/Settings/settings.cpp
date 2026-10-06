#include "settings.hpp"

#include <raylib.h>

#include <string>

#include "GameState/gameState.hpp"

// Distances are stored in world units; show them in blocks (5 units each).
static std::string inBlocks(int units) { return std::to_string(units / 5); }

static std::string inUnits(int units) { return std::to_string(units); }

Settings::Settings()
    : exitButton([] { GameState::shared().setMenuState(MenuState::HOME); }, "<",
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
          "", Rectangle{0, 0, 100, 50}, 30),
      interpolationButton([] { GameState::shared().toggleInterpolation(); }, "",
                          Rectangle{0, 0, 100, 50}, 30),
      shadowsButton([] { GameState::shared().toggleShadows(); }, "",
                    Rectangle{0, 0, 100, 50}, 30),
      renderDistanceSlider(
          "Render distance: ", GameState::MIN_RENDER_DISTANCE,
          GameState::MAX_RENDER_DISTANCE,
          [] { return GameState::shared().getRenderDistance(); },
          [](int v) { GameState::shared().setRenderDistance(v); }, inBlocks),
      shadowRadiusSlider(
          "Shadow distance: ", GameState::MIN_SHADOW_RADIUS,
          GameState::MAX_SHADOW_RADIUS,
          [] { return GameState::shared().getShadowRadius(); },
          [](int v) { GameState::shared().setShadowRadius(v); }, inBlocks),
      targetFpsSlider(
          "Target FPS: ", 10, 512,
          [] { return GameState::shared().getTargetFps(); },
          [](int v) { GameState::shared().setTargetFps(v); }, inUnits) {}

void Settings::frame() {
  const float left = GetScreenWidth() / 5.0f;
  const float width = left * 3;
  const float toggleX = left * 4 - 100;
  const Vector2 mouse = GetMousePosition();
  const bool pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
  const bool down = IsMouseButtonDown(MOUSE_BUTTON_LEFT);

  // ==== layout (follows the window width) ==== //
  renderDistanceSlider.setBar({left, 200, width, 20});
  shadowRadiusSlider.setBar({left, 290, width, 20});
  targetFpsSlider.setBar({left, 380, width, 20});
  vsyncButton.setRec({toggleX, 460, 100, 50});
  interpolationButton.setRec({toggleX, 530, 100, 50});
  shadowsButton.setRec({toggleX, 600, 100, 50});

  vsyncButton.setText(IsWindowState(FLAG_VSYNC_HINT) ? "On" : "Off");
  interpolationButton.setText(gameState.getInterpolation() ? "On" : "Off");
  shadowsButton.setText(gameState.getShadows() ? "On" : "Off");

  BeginDrawing();
  ClearBackground(BLACK);

  exitButton.frame(mouse, pressed);
  renderDistanceSlider.frame(mouse, pressed, down);
  shadowRadiusSlider.frame(mouse, pressed, down);
  // Hidden on web, where the browser paces the loop (default 0 = uncapped).
  if (GameState::DEFAULT_TARGET_FPS > 0)
    targetFpsSlider.frame(mouse, pressed, down);

  // ==== toggles ==== //
  DrawText("VSync:", left, 475, 30, WHITE);
  vsyncButton.frame(mouse, pressed);
  DrawText("Interpolation:", left, 545, 30, WHITE);
  interpolationButton.frame(mouse, pressed);
  DrawText("Shadows:", left, 615, 30, WHITE);
  shadowsButton.frame(mouse, pressed);

  EndDrawing();
}
