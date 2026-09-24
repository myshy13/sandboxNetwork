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

public:
  Button(std::function<void(void)> click, std::string text, float yPos);

  void frame(Vector2 mousePos, bool clicked);
};