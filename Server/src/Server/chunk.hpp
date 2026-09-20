#pragma once

#include <cstdint>
#include <utility>

// Packs a chunk's (x, z) coordinates into one hashable key: x in the high 32
// bits, z in the low 32. A chunk spans every height, so there is no y.
inline int64_t chunkKey(int cx, int cz) {
  // The unsigned cast keeps a negative cz from sign-extending over the x half.
  return (static_cast<int64_t>(cx) << 32) | static_cast<uint32_t>(cz);
}

// Inverse of chunkKey.
inline std::pair<int, int> chunkCoords(int64_t key) {
  return {static_cast<int>(key >> 32), static_cast<int32_t>(key)};
}
