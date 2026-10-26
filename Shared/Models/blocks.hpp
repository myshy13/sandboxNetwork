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
  Glass,
  Stone,
  Count
};

struct BlockInfo {
  BlockType type;
  bool opaque{true}; // hides the face of a neighbour behind it
  bool fluid{false}; // placing over it replaces it, and players swim in it
  bool translucent{false}; // the player can see through it
  bool blended{false};  // drawn with alpha blending (water); glass is a cutout
  bool solid{true};     // players collide with it
  bool placeable{true}; // a client may ask the server to place it
  int durability{3};
  Color color{WHITE};   // the block's look until it has a texture
};

// One row per BlockType, in enum order. (it's checked at compile time)
inline constexpr BlockInfo BLOCK_INFO[] = {
    {.type = BlockType::Grass},
    {.type = BlockType::Dirt, .durability = 2},
    {.type = BlockType::Water,
     .opaque = false,
     .fluid = true,
     .translucent = true,
     .blended = true,
     .solid = false},
    {.type = BlockType::Leaves, .durability = 1},
    {.type = BlockType::Wood, .durability = 5},
    {.type = BlockType::Planks, .durability = 5},
    {.type = BlockType::Glass, .opaque = false, .translucent = true},
    {.type = BlockType::Stone, .durability = 6},
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
inline bool isBlended(BlockType t) {
  return isValid(t) && BLOCK_INFO[static_cast<size_t>(t)].blended;
}
inline bool isOpaque(BlockType t) {
  return isValid(t) && BLOCK_INFO[static_cast<size_t>(t)].opaque;
}
// Magenta marks an invalid type so it's obvious rather than invisible.
inline Color blockColor(BlockType t) {
  return isValid(t) ? BLOCK_INFO[static_cast<size_t>(t)].color : MAGENTA;
}