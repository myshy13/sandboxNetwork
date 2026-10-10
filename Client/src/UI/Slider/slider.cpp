#include "UI/Slider/slider.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <utility>

#include "Raylib/drawText.hpp"

constexpr float KNOB_WIDTH  = 20.0f;
constexpr float KNOB_HEIGHT = 40.0f;
constexpr int LABEL_SIZE    = 30;

Slider::Slider(std::string label, int minValue, int maxValue, std::function<int(void)> get,
               std::function<void(int)> set, std::function<std::string(int)> format)
    : label(std::move(label)), minValue(minValue), maxValue(maxValue), get(std::move(get)), set(std::move(set)),
      format(std::move(format)) {}

void Slider::frame(Vector2 mousePos, bool pressed, bool down) {
  // Pressing anywhere along the bar picks the knob up and jumps it there.
  const Rectangle grab = {bar.x, bar.y - (KNOB_HEIGHT - bar.height) / 2, bar.width, KNOB_HEIGHT};
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
  const float t   = static_cast<float>(value - minValue) / static_cast<float>(maxValue - minValue);
  const Rectangle knob = {bar.x + t * bar.width - KNOB_WIDTH / 2, bar.y - (KNOB_HEIGHT - bar.height) / 2, KNOB_WIDTH,
                          KNOB_HEIGHT};

  DrawTextFont((label + format(value)).c_str(), bar.x, bar.y - 50, LABEL_SIZE,
               WHITE);
  DrawRectangleRec(bar, GRAY);
  DrawRectangleRec(knob, WHITE);
}
