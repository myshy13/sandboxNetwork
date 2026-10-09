#pragma once

#include <raylib.h>

#include <memory>
#include <optional>

#include "Input/inputSource.hpp"
#include "Input/inputState.hpp"

class Input {
 private:
  std::unique_ptr<InputSource> source;

  InputState current;
  InputState previous;

 public:
  Input(std::unique_ptr<InputSource> source);

  void update();
  void draw();

  // ==== getters ==== //
  bool down(Action) const;
  bool pressed(Action) const;
  Vector2 getMove() const;
  Vector2 getLook() const;
  Vector2 getPointer() const;
  float getScroll() const;
  std::optional<int> getHotbarSlot() const;

  // ==== setters ==== //
  void setMouseLook(bool mouseLook);
};