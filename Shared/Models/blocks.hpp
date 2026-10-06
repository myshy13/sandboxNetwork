#pragma once

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <raylib.h>

// Count is a sentinel, never a real block: it sizes BLOCK_INFO and
// bounds-checks.
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
  BlockType type;
  bool opaque{true}; // hides the face of a neighbour behind it
  bool fluid{false}; // placing over it replaces it, and players swim in it
  bool translucent{false}; // the player can see through it
  bool solid{true};        // players collide with it
  bool placeable{true};    // a client may ask the server to place it
  Color color{WHITE};      // the block's look until it has a texture
};

// One row per BlockType, in enum order. (it's checked at compile time)
inline constexpr BlockInfo BLOCK_INFO[] = {
    {.type = BlockType::Grass},
    {.type = BlockType::Dirt},
    {.type = BlockType::Water,
     .opaque = false,
     .fluid = true,
     .translucent = true,
     .solid = false},
    {.type = BlockType::Leaves},
    {.type = BlockType::Wood},
    {.type = BlockType::Planks},
};

constexpr bool rowsValid() {
  if (!(std::size(BLOCK_INFO) == static_cast<size_t>(BlockType::Count)))
    return false;
  for (size_t i = 0; i < static_cast<size_t>(BlockType::Count); i++) {
    BlockType infoType = BLOCK_INFO[i].type;
    BlockType expectedType = static_cast<BlockType>(i);
    if (infoType != expectedType)
      return false;
  }
  return true;
}

static_assert(rowsValid(), "BLOCK_INFO is not valid");

// A type read off the wire is untrusted: check it before indexing
// BLOCK_INFO.
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
inline bool isOpaque(BlockType t) {
  return isValid(t) && BLOCK_INFO[static_cast<size_t>(t)].opaque;
}
// Magenta marks an invalid type so it's obvious rather than invisible.
inline Color blockColor(BlockType t) {
  return isValid(t) ? BLOCK_INFO[static_cast<size_t>(t)].color : MAGENTA;
}