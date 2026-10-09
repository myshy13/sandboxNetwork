#pragma once

#include <memory>
#include "Input/inputState.hpp"

class InputSource {
 public:
  virtual ~InputSource() = default;
  virtual void poll(InputState&) = 0;
  virtual void setMouseLook(bool) = 0;
  virtual void draw() {};
};

std::unique_ptr<InputSource> makeInputSource();
