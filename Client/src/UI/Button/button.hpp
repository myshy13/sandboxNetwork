#pragma once

#include <functional>
#include <raylib.h>
#include <string>

// constants
constexpr int BUTTON_WIDTH  = 400;
constexpr int BUTTON_HEIGHT = 80;
constexpr int FONT_SIZE     = 50;

class Button {
  std::string text;
  std::function<void(void)> handler;
  Rectangle rec;
  int fontSize{FONT_SIZE};
  bool centred{true}; // a menu button stays centred when the window resizes

public:
  // A menu button: fixed size, centred on the screen at yPos.
  Button(std::function<void(void)> click, std::string text, float yPos);
  // A button of any size and place, e.g. a toggle on a settings row.
  Button(std::function<void(void)> click, std::string text, Rectangle rec, int fontSize);

  void setText(const std::string &t) { text = t; }
  void setRec(Rectangle r) { rec = r; }

  void frame(Vector2 mousePos, bool clicked);
};
