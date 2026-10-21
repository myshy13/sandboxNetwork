#include <raylib.h>
#include <raymath.h>

#include <cstddef>
#include <memory>

#include "Input/inputSource.hpp"
#include "Input/inputState.hpp"

// Per second, so the feel doesn't change with the frame rate.
constexpr float CURSOR_SPEED = 600.0f;  // pixels
constexpr float LOOK_SPEED = 1200.0f;   // mouse-delta units
constexpr float DRIFT_MAXIMUM = 0.1f;

// SDL_GameControllerDB (zlib), copied next to the other assets
constexpr const char* MAPPINGS_FILE = "assets/gamecontrollerdb.txt";

namespace {
struct Bind {
  Action action;
  GamepadButton button;
};

// Draft layout (Xbox names). An action can sit on several buttons, a button on
// several actions. Unbound: chat, commands, debug, chunk borders, collision,
// clipping, reconnect (no keyboard on the TV).
constexpr Bind BINDS[] = {
    {Action::Shoot, GAMEPAD_BUTTON_RIGHT_TRIGGER_2},          // RT
    {Action::Place, GAMEPAD_BUTTON_LEFT_TRIGGER_2},           // LT
    {Action::Jump, GAMEPAD_BUTTON_RIGHT_FACE_DOWN},           // A
    {Action::Sneak, GAMEPAD_BUTTON_RIGHT_FACE_LEFT},          // X
    {Action::Pause, GAMEPAD_BUTTON_MIDDLE_RIGHT},             // Menu
    {Action::TabKills, GAMEPAD_BUTTON_MIDDLE_LEFT},           // View
    {Action::Zoom, GAMEPAD_BUTTON_RIGHT_THUMB},               // R3
    {Action::Click, GAMEPAD_BUTTON_RIGHT_FACE_DOWN},          // A: menus only
    {Action::DownHotbarSlot, GAMEPAD_BUTTON_LEFT_TRIGGER_1},  // LB
    {Action::UpHotbarSlot, GAMEPAD_BUTTON_RIGHT_TRIGGER_1},   // RB
    {Action::OpenDebug, GAMEPAD_BUTTON_LEFT_FACE_DOWN},       // Left DPad down
    {Action::ShowCollision, GAMEPAD_BUTTON_LEFT_FACE_LEFT},   // Left DPad Left
    {Action::Openchat, GAMEPAD_BUTTON_LEFT_FACE_RIGHT},  // Left DPad  Right
    {Action::Reconnect, GAMEPAD_BUTTON_LEFT_FACE_UP},    // Left DPad Up
    {Action::Exit, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT},     // B
};
}  // namespace

class InputGamepad : public InputSource {
 private:
  Vector2 cursor{0, 0};
  bool cursorVisible{true};
  bool windowResized{true};

  MouseCursor pendingCursor = MOUSE_CURSOR_DEFAULT;

 public:
  InputGamepad() {
    // GLFW's built-in table lacks newer firmware GUIDs, so such a pad reads as
    // all zeros without this
    if (char* db = LoadFileText(MAPPINGS_FILE)) {
      SetGamepadMappings(db);
      UnloadFileText(db);
    }
  }

  void poll(InputState& state) override {
    if (!IsGamepadAvailable(0)) return;

    if (IsWindowResized()) windowResized = true;
    if (windowResized) {
      cursor = {(float)GetScreenWidth() / 2, (float)GetScreenHeight() / 2};
      windowResized = false;
    }
    state = {};

    for (const Bind& b : BINDS) {
      // Click is menu-only: A also jumps, and must not click while playing
      if (b.action == Action::Click && !cursorVisible) continue;
      if (IsGamepadButtonDown(0, b.button)) {
        state.actions.set(static_cast<size_t>(b.action));
      }
    }

    Vector2 leftMovement = {GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X),
                            GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y)};
    Vector2 rightMovement = {GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_X),
                             GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_Y)};

    if (leftMovement.x < DRIFT_MAXIMUM && leftMovement.x > -DRIFT_MAXIMUM)
      leftMovement.x = 0.0f;
    if (leftMovement.y < DRIFT_MAXIMUM && leftMovement.y > -DRIFT_MAXIMUM)
      leftMovement.y = 0.0f;
    if (rightMovement.x < DRIFT_MAXIMUM && rightMovement.x > -DRIFT_MAXIMUM)
      rightMovement.x = 0.0f;
    if (rightMovement.y < DRIFT_MAXIMUM && rightMovement.y > -DRIFT_MAXIMUM)
      rightMovement.y = 0.0f;

    // The cursor only moves in menus; while playing the stick walks instead
    float dt = GetFrameTime();
    if (cursorVisible) {
      cursor += leftMovement * (CURSOR_SPEED * dt);
      cursor = Vector2Clamp(
          cursor, {0, 0}, {(float)GetScreenWidth(), (float)GetScreenHeight()});
    }

    state.pointer = cursor;
    state.look = rightMovement * (LOOK_SPEED * dt);
    state.move = {leftMovement.x, -leftMovement.y};
  };

  void setCursorStyle(MouseCursor style) override { pendingCursor = style; }

  void drawCursor(const AssetManager& assets) override {
    static const Texture2D& cursorHoverIcon = assets.get(Tex::HoveredIcon);
    if (cursorVisible) {
      switch (pendingCursor) {
        case MOUSE_CURSOR_POINTING_HAND:
          DrawTexturePro(cursorHoverIcon,
                         {0, 0, (float)cursorHoverIcon.width,
                          (float)cursorHoverIcon.height},
                         {cursor.x - ((float)cursorHoverIcon.width / 2 * 2),
                          cursor.y - ((float)cursorHoverIcon.height / 2 * 2),
                          (float)cursorHoverIcon.width * 2,
                          (float)cursorHoverIcon.height * 2},
                         {0, 0}, 0.0f, WHITE);
          break;
        default:
          DrawRectangle(cursor.x - 5, cursor.y - 5, 10, 10, WHITE);
          break;
      }
    }
    pendingCursor = MOUSE_CURSOR_DEFAULT;
  }

  void setMouseLook(bool mouseLook) override {
    cursorVisible = !mouseLook;
    cursor = {(float)GetScreenWidth() / 2, (float)GetScreenHeight() / 2};
  }
};

std::unique_ptr<InputSource> makeInputSource() {
  return std::make_unique<InputGamepad>();
}