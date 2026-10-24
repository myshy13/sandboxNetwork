#pragma once

#include <raylib.h>

#include <functional>
#include <optional>
#include <string>

#include "Input/input.hpp"
#include "UI/Theme/theme.hpp"

// constants
constexpr int BUTTON_WIDTH = 450;
constexpr int BUTTON_HEIGHT = 80;
constexpr int FONT_SIZE = 50;

struct ButtonStyle {
  Color fill;
  Color fillHover;
  Color border;
  Color shadow;
  Color textColor;
  Color mutedColor;
  Color textHover;
};

namespace ButtonStyles {
inline const ButtonStyle Primary = {Theme::Blue,     Theme::BlueHi, Theme::Text,
                                    Theme::BlueDark, Theme::Text,   Theme::Text,
                                    Theme::TextMuted};
inline const ButtonStyle Multi = {Theme::Red,      Theme::RedHi, Theme::Text,
                                  Theme::RedDark,  Theme::Text,  Theme::Text,
                                  Theme::TextMuted};
inline const ButtonStyle Secondary = {
    Theme::Panel, Theme::PanelHi, Theme::PanelBorder, Theme::PanelShadow,
    Theme::Text,  Theme::Text,    Theme::TextMuted};
inline const ButtonStyle Ghost = {
    BLANK,       BLANK,           BLANK, BLANK, Theme::TextMuted,
    Theme::Text, Theme::TextMuted};
}  // namespace ButtonStyles

class Button {
  std::string text;
  std::function<void(void)> handler;
  Rectangle rec;
  int fontSize{FONT_SIZE};
  bool centred{true};  // a menu button stays centred when the window resizes
  ButtonStyle style;
  std::optional<float> initialY;

 public:
  // A menu button: fixed size, centred on the screen at yPos.
  Button(std::function<void(void)> click, std::string text, float yPos,
         ButtonStyle style);
  // A button of any size and place, e.g. a toggle on a settings row.
  Button(std::function<void(void)> click, std::string text, Rectangle rec,
         int fontSize, ButtonStyle style = ButtonStyles::Primary);

  void setText(const std::string& t) { text = t; }

  // A button given its own rectangle is no longer a centred menu button.
  void setRec(Rectangle r) {
    rec = r;
    centred = false;
  }

  void setY(float yPos) {
    rec.height = BUTTON_HEIGHT;
    rec.width = BUTTON_WIDTH;
    rec.y = yPos;
    rec.x = GetScreenWidth() / 2.0f - BUTTON_WIDTH / 2.0f;
  }

  void frame(Input& input, bool clicked);
};
