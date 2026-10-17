#pragma once

#include <raylib.h>

#include <memory>

#include "AssetManager/manager.hpp"
#include "Input/inputState.hpp"

class InputSource {
 public:
  virtual ~InputSource() = default;
  virtual void poll(InputState&) = 0;
  virtual void setMouseLook(bool) = 0;
  virtual void draw(const AssetManager&) {};
  virtual void drawCursor(const AssetManager&) {};
  virtual void setCursorStyle(MouseCursor){};
};

std::unique_ptr<InputSource> makeInputSource();
