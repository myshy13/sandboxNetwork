#include "input.hpp"

#include <raylib.h>

#include <cstddef>
#include <optional>
#include <utility>

#include "Input/inputSource.hpp"
#include "Input/inputState.hpp"

Input::Input(std::unique_ptr<InputSource> inputSource)
    : source(std::move(inputSource)) {}

void Input::update() {
  previous = std::move(current);
  current = {};

  source->poll(current);
}

void Input::setMouseLook(bool mouseLook) { source->setMouseLook(mouseLook); }

Vector2 Input::getMove() const { return current.move; }

Vector2 Input::getLook() const { return current.look; }

std::optional<int> Input::getHotbarSlot() const {
  return current.newHotBarSlot;
}

Vector2 Input::getPointer() const { return current.pointer; }

float Input::getScroll() const { return current.scroll; }

bool Input::down(Action action) const {
  size_t actionIndex = static_cast<size_t>(action);
  return current.actions[actionIndex];
}

bool Input::pressed(Action action) const {
  size_t actionIndex = static_cast<size_t>(action);
  return current.actions[actionIndex] && !previous.actions[actionIndex];
}