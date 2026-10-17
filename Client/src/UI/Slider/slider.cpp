#include "UI/Slider/slider.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <utility>

#include "Input/input.hpp"
#include "Raylib/drawText.hpp"
#include "UI/Theme/theme.hpp"

constexpr float KNOB_WIDTH = 20.0f;
constexpr float KNOB_HEIGHT = 40.0f;
constexpr int LABEL_SIZE = 30;

Slider::Slider(std::string label, int minValue, int maxValue,
               std::function<int(void)> get, std::function<void(int)> set,
               std::function<std::string(int)> format)
    : label(std::move(label)),
      minValue(minValue),
      maxValue(maxValue),
      get(std::move(get)),
      set(std::move(set)),
      format(std::move(format)) {}

void Slider::frame(Input& input, bool pressed, bool down) {
  Vector2 mousePos = input.getPointer();
  // Pressing anywhere along the bar picks the knob up and jumps it there.
  const Rectangle grab = {bar.x, bar.y - (KNOB_HEIGHT - bar.height) / 2,
                          bar.width, KNOB_HEIGHT};
  if (!dragging && pressed && CheckCollisionPointRec(mousePos, grab)) {
    dragging = true;
  }
  if (dragging && !down) {
    dragging = false;
  }
  if (dragging) {
    const float t = std::clamp((mousePos.x - bar.x) / bar.width, 0.0f, 1.0f);
    set(minValue + static_cast<int>(std::lround(t * (maxValue - minValue))));
  }

  const int value = get();
  const float t = static_cast<float>(value - minValue) /
                  static_cast<float>(maxValue - minValue);
  const Rectangle knob = {bar.x + t * bar.width - KNOB_WIDTH / 2,
                          bar.y - (KNOB_HEIGHT - bar.height) / 2, KNOB_WIDTH,
                          KNOB_HEIGHT};

  const bool hovered = dragging || CheckCollisionPointRec(mousePos, grab);

  if (hovered) input.setCursorStyle(MOUSE_CURSOR_RESIZE_EW);

  if (!dragging && CheckCollisionPointRec(mousePos, knob))
    input.setCursorStyle(MOUSE_CURSOR_POINTING_HAND);

  DrawTextFont((label + format(value)).c_str(), bar.x, bar.y - 50, LABEL_SIZE,
               Theme::Text);

  // Track with the filled part in blue, up to the knob
  Rectangle inner = Theme::DrawTrack(bar);
  inner.width =
      std::clamp(knob.x + KNOB_WIDTH / 2 - inner.x, 0.0f, inner.width);
  DrawRectangleRec(inner, Theme::Blue);

  // Knob: a small raised block, like the buttons
  Theme::DrawBlock(knob, hovered ? Theme::BlueHi : Theme::Blue, Theme::Text,
                   Theme::BlueDark, hovered, dragging, 4.0f);
}
