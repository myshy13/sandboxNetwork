#include "UI/Button/button.hpp"
#include <raylib.h>

Button::Button(std::function<void(void)> click, std::string text, float yPos) : text(text), handler(click) {
  rec.height = BUTTON_HEIGHT;
  rec.width  = BUTTON_WIDTH;
  rec.y      = yPos;
  rec.x      = GetScreenWidth() / 2.0f - BUTTON_WIDTH / 2.0f;
}

Button::Button(std::function<void(void)> click, std::string text, Rectangle rec, int fontSize)
    : text(text), handler(click), rec(rec), fontSize(fontSize), centred(false) {}

void Button::frame(Vector2 mousePos, bool clicked) {
  if (centred && IsWindowResized())
    rec.x = GetScreenWidth() / 2.0f - BUTTON_WIDTH / 2.0f;
  bool hovered = CheckCollisionPointRec(mousePos, rec);
  DrawRectangleRec(rec, hovered ? LIGHTGRAY : Color(150, 150, 150, 255));
  DrawText(text.c_str(), rec.x + rec.width / 2 - MeasureText(text.c_str(), fontSize) / 2, rec.y + (rec.height / 2) - (float)fontSize / 2, fontSize, WHITE);
  if (hovered && clicked) {
    handler();
  }
};
