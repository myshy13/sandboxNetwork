#include "UI/TextBox/textbox.hpp"

#include <raylib.h>

#include "Input/inputState.hpp"
#include "Raylib/drawText.hpp"
#include "UI/Theme/theme.hpp"

static constexpr float PADDING = 12.0f;
static constexpr double CARET_BLINK = 0.5;  // seconds per on/off half

TextBox::TextBox(std::string& content, Rectangle rec, int fontSize, bool masked,
                 ButtonStyle style, Input& input,
                 const std::string& placeHolder)
    : rec(rec),
      content(content),
      placeholder(placeHolder),
      focused(false),
      masked(masked),
      input(input),
      fontSize(fontSize),
      style(style) {}

TextBox::TextBox(std::string& content, float yPos, bool masked,
                 ButtonStyle style, Input& input,
                 const std::string& placeholder)
    : TextBox(content,
              Rectangle{GetScreenWidth() / 2.0f - BUTTON_WIDTH / 2.0f, yPos,
                        BUTTON_WIDTH, BUTTON_HEIGHT},
              TEXTBOX_FONTSIZE, masked, style, input, placeholder) {}

void TextBox::update() {
  const bool hovered = CheckCollisionPointRec(input.getPointer(), rec);
  if (hovered) input.setCursorStyle(MOUSE_CURSOR_IBEAM);

  if (input.pressed(Action::Click)) {
    const bool wasFocused = focused;
    focused = hovered;
    // Drop keys typed before focus, so they don't land in this box.
    if (focused && !wasFocused)
      while (GetCharPressed() != 0) {
      }
  }
  if (!focused) return;

  // Several characters can queue up in one frame; plain ASCII only.
  int ch;
  while ((ch = GetCharPressed()) != 0)
    if (ch >= 32 && ch <= 126) content.push_back((char)ch);

  if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) &&
      !content.empty())
    content.pop_back();
}

void TextBox::draw() {
  const Rectangle inner = Theme::DrawTrack(rec);
  if (focused) DrawRectangleLinesEx(rec, Theme::Border, Theme::Focus);

  bool muted{false};
  std::string shown = masked ? std::string(content.size(), '*') : content;
  // Keep the end of the text (where you type) in view.
  while (!shown.empty() &&
         MeasureTextFont(shown.c_str(), fontSize) > inner.width - PADDING * 2)
    shown.erase(0, 1);

  if (shown.size() == 0 && placeholder.size() == 0) {
    shown = placeholder;
    muted = true;
  }

  const int x = (int)(inner.x + PADDING);
  const int y = (int)(inner.y + inner.height / 2 - fontSize / 2.0f);
  DrawTextFont(shown.c_str(), x, y, fontSize,
               muted ? style.mutedColor : style.textColor);

  if (focused && (int)(GetTime() / CARET_BLINK) % 2 == 0)
    DrawRectangle(x + MeasureTextFont(shown.c_str(), fontSize) + 2, y, 2,
                  fontSize, Theme::Text);
}

void TextBox::frame() {
  if (IsWindowResized()) rec.x = GetScreenWidth() / 2.0f - rec.width / 2.0f;
  update();
  draw();
}
