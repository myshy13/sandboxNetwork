#include <raylib.h>

#include <cstddef>
#include <iterator>
#include <memory>

#include "Input/inputSource.hpp"
#include "Input/inputState.hpp"

constexpr float SCROLL_SENSITIVITY = 1.5f;

// ==== binds ==== //
namespace {

struct Bind {
  bool mouse;  // true: a mouse button, false: a key
  int code;
};

// One row per Action, in enum order (the static_assert catches a missing row,
// not a swapped one).
constexpr Bind BINDS[] = {
    {true, MOUSE_BUTTON_LEFT},   // Shoot
    {false, KEY_SPACE},          // Jump
    {false, KEY_LEFT_SHIFT},     // Sneak
    {true, MOUSE_BUTTON_RIGHT},  // Place
    {false, KEY_ESCAPE},         // Pause
    {false, KEY_T},              // Openchat
    {false, KEY_SLASH},          // OpenChatCommands
    {false, KEY_TAB},            // TabKills
    {true, MOUSE_BUTTON_LEFT},   // Click
#ifdef __EMSCRIPTEN__
    {false, KEY_K},  // OpenDebug (browsers keep F3 for find)
#else
    {false, KEY_F3},  // OpenDebug
#endif
    {false, KEY_F4},  // Show chunk borders
    {false, KEY_F5},  // ShowCollision
    {false, KEY_F7},  // ChangeClipping
    {false, KEY_R},   // Reconnect
    {false, KEY_C},   // Zoom
};

static_assert(std::size(BINDS) == static_cast<size_t>(Action::Count),
              "one bind per Action");

}  // namespace

class InputKbm : public InputSource {
  void poll(InputState& inputState) override {
    inputState = {};
    for (size_t i = 0; i < std::size(BINDS); i++) {
      const Bind& bind = BINDS[i];
      inputState.actions.set(
          i, bind.mouse ? IsMouseButtonDown(bind.code) : IsKeyDown(bind.code));
    }

    inputState.look = GetMouseDelta();
    inputState.pointer = GetMousePosition();
    inputState.scroll = GetMouseWheelMove() * SCROLL_SENSITIVITY;

    int key = GetKeyPressed();
    // 1-9 range
    if (key >= KEY_ONE && key < KEY_ONE + 9) {
      inputState.newHotBarSlot = key - KEY_ONE;
    }

    inputState.move = {0, 0};
    // Movement
    if (IsKeyDown(KEY_W)) {
      inputState.move.y += 1.0f;
    }

    if (IsKeyDown(KEY_A)) {
      inputState.move.x -= 1.0f;
    }

    if (IsKeyDown(KEY_S)) {
      inputState.move.y -= 1.0f;
    }

    if (IsKeyDown(KEY_D)) {
      inputState.move.x += 1.0f;
    }
  }

  void setMouseLook(bool mouseLook) override {
    if (mouseLook) {
      DisableCursor();
    } else {
      EnableCursor();
    }
  }
};

std::unique_ptr<InputSource> makeInputSource() {
  return std::make_unique<InputKbm>();
};
