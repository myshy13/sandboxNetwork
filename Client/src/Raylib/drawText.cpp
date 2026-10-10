#include "GameState/gameState.hpp"
#include "raylib.h"

void DrawTextFont(const char* text, int x, int y, int fontSize, Color color) {
  DrawTextEx(GameState::shared().getMainFont(), text,
             (Vector2){(float)x, (float)y}, (float)fontSize, 1.0f, color);
}

int MeasureTextFont(const char* text, int fontSize) {
  return static_cast<int>(MeasureTextEx(GameState::shared().getMainFont(), text,
                                        (float)fontSize, 1.0f)
                              .x);
}
