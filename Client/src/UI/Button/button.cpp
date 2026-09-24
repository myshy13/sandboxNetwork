#include "UI/Button/button.hpp"
#include <raylib.h>

Button::Button(std::function<void(void)> click, std::string text, float yPos) : text(text), handler(click) {
  rec.height = BUTTON_HEIGHT;
  rec.width  = BUTTON_WIDTH;
  rec.y      = yPos;
  rec.x      = GetScreenWidth() / 2.0f - BUTTON_WIDTH / 2.0f;
}

void Button::frame(Vector2 mousePos, bool clicked) {
  if (IsWindowResized())
    rec.x = GetScreenWidth() / 2.0f - BUTTON_WIDTH / 2.0f;
  bool hovered = CheckCollisionPointRec(mousePos, rec);
  DrawRectangleRec(rec, hovered ? LIGHTGRAY : Color(150, 150, 150, 255));
  DrawText(text.c_str(), GetScreenWidth() / 2 - MeasureText(text.c_str(), FONT_SIZE) / 2, rec.y + (rec.height / 2) - (float)FONT_SIZE / 2, FONT_SIZE, WHITE);
  if (hovered && clicked) {
    handler();
  }
};