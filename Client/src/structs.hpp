#pragma once
#include <raylib.h>

struct Bullet {
  int playerId;
  int bulletId;
  Vector3 pos;
  Vector3 vel;
};