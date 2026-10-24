#pragma once

#include <raylib.h>

#include <string>

#include "Input/input.hpp"
#include "UI/Button/button.hpp"

constexpr int TEXTBOX_FONTSIZE = 30;

class TextBox {
 private:
  Rectangle rec;
  std::string& content;
  const std::string placeholder{""};

  bool focused;
  bool masked;

  Input& input;

  int fontSize{TEXTBOX_FONTSIZE};
  ButtonStyle style;

  void update();
  void draw();

 public:
  TextBox(std::string& content, Rectangle rec, int fontSize, bool masked,
          ButtonStyle style, Input& input, const std::string& placeholder = "");
  TextBox(std::string& content, float yPos, bool masked, ButtonStyle style,
          Input& input, const std::string& placeholder = "");

  void setYPos(float yPos) {
    rec = Rectangle{GetScreenWidth() / 2.0f - BUTTON_WIDTH / 2.0f, yPos,
                    BUTTON_WIDTH, BUTTON_HEIGHT};
  };

  void frame();
};