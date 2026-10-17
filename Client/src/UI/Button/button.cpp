#include "UI/Button/button.hpp"

#include <raylib.h>

#include "Input/input.hpp"
#include "Raylib/drawText.hpp"

static constexpr float BORDER = 4.0f;
static constexpr float SHADOW = 8.0f;  // resting depth of the drop shadow
static constexpr float LIFT = 4.0f;    // how far the face rises on hover

Button::Button(std::function<void(void)> click, std::string text, float yPos,
               ButtonStyle style)
    : text(text), handler(click), style(style) {
  rec.height = BUTTON_HEIGHT;
  rec.width = BUTTON_WIDTH;
  rec.y = yPos;
  rec.x = GetScreenWidth() / 2.0f - BUTTON_WIDTH / 2.0f;
}

Button::Button(std::function<void(void)> click, std::string text, Rectangle rec,
               int fontSize, ButtonStyle style)
    : text(text),
      handler(click),
      rec(rec),
      fontSize(fontSize),
      centred(false),
      style(style) {}

void Button::frame(Input& input, bool clicked) {
  Vector2 mousePos = input.getPointer();
  if (centred && IsWindowResized())
    rec.x = GetScreenWidth() / 2.0f - BUTTON_WIDTH / 2.0f;

  const bool hovered = CheckCollisionPointRec(mousePos, rec);
  const bool down = hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT);

  if (hovered) {
    input.setCursorStyle(MOUSE_CURSOR_POINTING_HAND);
  }

  const Rectangle face =
      Theme::DrawBlock(rec, hovered ? style.fillHover : style.fill,
                       style.border, style.shadow, hovered, down);

  const Color textColor = hovered ? style.textHover : style.textColor;
  const float textW = (float)MeasureTextFont(text.c_str(), fontSize);
  DrawTextFont(text.c_str(), face.x + face.width / 2 - textW / 2,
               face.y + face.height / 2 - (float)fontSize / 2, fontSize,
               textColor);

  if (hovered && clicked) handler();
}