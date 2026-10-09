#include <raylib.h>
#include <raymath.h>

#include <cstddef>
#include <memory>
#include <unordered_map>
#include <vector>

#include "AssetManager/manager.hpp"
#include "Input/inputSource.hpp"
#include "Input/inputState.hpp"

constexpr int STICK_RADIUS = 100;
constexpr float DEAD_ZONE = 0.2f;
constexpr float LOOK_SCALE = 2;

constexpr float SCROLL_SENSITIVITY = 0.6f;

struct Touch {
  Vector2 pos;
};

// Not "Button": UI/Button/button.hpp already defines a class of that name.
struct TouchButton {
  Rectangle rect;
  Tex icon;

  Action action;
  float rotation{0.0f};

  int held{-1};  // id of the finger holding it, -1 when free
};

class InputTouch : public InputSource {
 private:
  std::unordered_map<int, Touch> touchPoints;
  int stickId{-1};
  int lookId{-1};
  Vector2 stickOrigin{0, 0};
  Vector2 lookLast{0, 0};

  std::vector<TouchButton> buttons = {};
  int layoutWidth{0};
  int layoutHeight{0};

  // ==== buttons ==== //

  // Right-thumb grid (Jump/Fire/Place/Sneak), Pause and Zoom along the top
  // right, Scores top left.
  void layout() {
    layoutWidth = GetScreenWidth();
    layoutHeight = GetScreenHeight();

    buttons = {
        {{(float)layoutWidth - 60, (float)layoutHeight - 60, 50, 50},
         Tex::JumpIcon,
         Action::Jump},
        {{(float)layoutWidth - 60, (float)layoutHeight - 120, 50, 50},
         Tex::ShootIcon,
         Action::Shoot},
        {{(float)layoutWidth - 60, (float)layoutHeight - 180, 50, 50},
         Tex::PlaceIcon,
         Action::Place},
        {
            {10, (float)layoutHeight - 60, 50, 50},
            Tex::JumpIcon,
            Action::Sneak,
            180.0f,
        },
        {{static_cast<float>(layoutWidth) / 2 - 25, 10, 50, 50},
         Tex::PauseIcon,
         Action::Pause},
        // no zoom
        // no scores
    };
  }

  bool onButton(Vector2 p) const {
    for (const TouchButton& b : buttons) {
      if (CheckCollisionPointRec(p, b.rect)) return true;
    }
    return false;
  }

 public:
  void poll(InputState& state) override {
    // Rebuilt on resize or rotation, so the rectangles always match the screen.
    if (GetScreenWidth() != layoutWidth || GetScreenHeight() != layoutHeight) {
      layout();
    }

    // raylib handles pointer from touch well, so menus read the mapped mouse
    state.pointer = GetMousePosition();
    state.actions.set(static_cast<size_t>(Action::Click),
                      IsMouseButtonDown(MOUSE_BUTTON_LEFT));
    touchPoints.clear();
    for (int i = 0; i < GetTouchPointCount(); i++) {
      touchPoints[GetTouchPointId(i)] = Touch{GetTouchPosition(i)};
    }

    if (stickId != -1 && !touchPoints.contains(stickId)) stickId = -1;
    if (lookId != -1 && !touchPoints.contains(lookId)) lookId = -1;

    for (const auto& [id, t] : touchPoints) {
      if (id == stickId || id == lookId) continue;
      if (onButton(t.pos)) continue;
      if (stickId == -1 && t.pos.x < static_cast<float>(GetScreenWidth()) / 2) {
        stickId = id;
        stickOrigin = t.pos;
      } else if (lookId == -1 &&
                 t.pos.x >= static_cast<float>(GetScreenWidth()) / 2) {
        lookId = id;
        lookLast = t.pos;
      }
    }

    if (stickId != -1) {
      const Touch& t = touchPoints[stickId];
      Vector2 v = Vector2Scale(Vector2Subtract(t.pos, stickOrigin),
                               1.0f / STICK_RADIUS);
      if (Vector2Length(v) > 1) {
        v = Vector2Normalize(v);
      }
      if (Vector2Length(v) < DEAD_ZONE) {
        v = {0, 0};
      }
      state.move = {v.x, -v.y};
    }

    if (lookId != -1) {
      const Touch& t = touchPoints[lookId];
      state.look = Vector2Subtract(t.pos, lookLast) * LOOK_SCALE;
      state.scroll = state.look.y * SCROLL_SENSITIVITY;
      lookLast = t.pos;
    }

    for (TouchButton& b : buttons) {
      b.held = -1;
      for (const auto& [id, t] : touchPoints) {
        if (id == stickId || id == lookId)
          continue;  // the stick finger can't press a button
        if (CheckCollisionPointRec(t.pos, b.rect)) {
          b.held = id;
          state.actions.set(static_cast<size_t>(b.action));
        }
      }
    }
  }

  void setMouseLook(bool) override {}

  void draw(const AssetManager& assets) override {
    for (const TouchButton& b : buttons) {
      DrawRectangleRec(b.rect, b.held != -1 ? Color{200, 200, 200, 150}
                                            : Color{185, 185, 185, 130});
      const Texture2D& texture = assets.get(b.icon);
      DrawTexturePro(texture,
                     {0, 0, static_cast<float>(texture.width),
                      static_cast<float>(texture.height)},
                     {b.rect.x + b.rect.width / 2, b.rect.y + b.rect.height / 2,
                      b.rect.width, b.rect.height},
                     {b.rect.width / 2, b.rect.height / 2}, b.rotation, WHITE);
    }
  }
};

std::unique_ptr<InputSource> makeInputSource() {
  return std::make_unique<InputTouch>();
}
