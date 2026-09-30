#pragma once

#include "raylib.h"
#include "sharedEnv.hpp"
#include <cmath>
constexpr Vector3 blockSize = SHARED_BLOCK_SIZE;
constexpr float BLOCK_SIZE = blockSize.x; // cubic, see sharedEnv.hpp

static constexpr float CHUNK_SIZE =
    16 * BLOCK_SIZE; // must match World::STREAM_CHUNK_SIZE
                     // (Client/src/World/world.hpp)

// Packs a grid cell's (x, y, z) into one hashable key, offset so negative cells
// don't collide.
static int64_t cellKey(int x, int y, int z) {
  constexpr int64_t OFFSET = 1 << 20;
  return ((x + OFFSET) << 42) | ((y + OFFSET) << 21) | (z + OFFSET);
}

static int64_t blockKey(Vector3 pos) {
  return cellKey((int)floorf(pos.x / BLOCK_SIZE),
                 (int)floorf(pos.y / BLOCK_SIZE),
                 (int)floorf(pos.z / BLOCK_SIZE));
}