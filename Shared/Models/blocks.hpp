#pragma once

// Count is a sentinel, never a real block: it sizes BLOCK_INFO and
// bounds-checks.
#include <cstdint>
#include <iterator>
#include <raylib.h>
enum class BlockType : uint8_t {
  Grass,
  Dirt,
  Water,
  Leaves,
  Wood,
  Planks,
  Count
};

struct BlockInfo {
  bool solid;       // players collide with it
  bool placeable;   // a client may ask the server to place it
  bool fluid;       // placing over it replaces it, and players swim in it
  Color color;      // the block's look until it has a texture; alpha is opacity
  bool translucent; // the player can see through it
};

// One row per BlockType, in enum order: a new block is a new enum value + a
// row.
inline constexpr BlockInfo BLOCK_INFO[] = {
    {true, true, false, WHITE, false}, // Grass
    {true, true, false, WHITE, false}, // Dirt
    {false, true, true, WHITE, true},  // Water
    {true, true, false, WHITE, false}, // Leaves
    {true, true, false, WHITE, false}, // Wood
    {true, true, false, WHITE, false}, // Planks
};

static_assert(std::size(BLOCK_INFO) == static_cast<size_t>(BlockType::Count),
              "BLOCK_INFO needs exactly one row per BlockType");

// A type read off the wire is untrusted: check it before indexing BLOCK_INFO.
inline bool isValid(BlockType t) { return t < BlockType::Count; }
inline bool isSolid(BlockType t) {
  return isValid(t) && BLOCK_INFO[static_cast<size_t>(t)].solid;
}
inline bool isPlaceable(BlockType t) {
  return isValid(t) && BLOCK_INFO[static_cast<size_t>(t)].placeable;
}
inline bool isFluid(BlockType t) {
  return isValid(t) && BLOCK_INFO[static_cast<size_t>(t)].fluid;
}
inline bool isTranslucent(BlockType t) {
  return isValid(t) && BLOCK_INFO[static_cast<size_t>(t)].translucent;
}
// Magenta marks an invalid type so it's obvious rather than invisible.
inline Color blockColor(BlockType t) {
  return isValid(t) ? BLOCK_INFO[static_cast<size_t>(t)].color : MAGENTA;
}