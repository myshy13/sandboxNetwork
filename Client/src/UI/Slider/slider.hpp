#pragma once

#include <functional>
#include <raylib.h>
#include <string>

// A horizontal slider over an int range, tied to its value by a getter and a setter.
class Slider {
  std::string label;
  int minValue;
  int maxValue;
  std::function<int(void)> get;
  std::function<void(int)> set;
  std::function<std::string(int)> format; // how the value is shown after the label
  Rectangle bar{0, 0, 0, 0};
  bool dragging{false};

public:
  Slider(std::string label, int minValue, int maxValue, std::function<int(void)> get, std::function<void(int)> set,
         std::function<std::string(int)> format);

  // Where the bar sits; set it each frame if the layout follows the window size.
  void setBar(Rectangle r) { bar = r; }

  // pressed: the button went down this frame; down: it is held.
  void frame(Vector2 mousePos, bool pressed, bool down);
};
