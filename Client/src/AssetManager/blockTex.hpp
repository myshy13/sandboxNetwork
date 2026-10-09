#pragma once

#include <iterator>
#include <optional>

#include "AssetManager/manager.hpp"
#include "Models/blocks.hpp"

enum class BlockFace { Top = 0, Side, Bottom };

// One row per BlockType, in enum order; nullopt = no texture yet, drawn as its
// colour.
inline constexpr std::optional<Tex> BLOCK_TEX[][3] = {
    {Tex::Grass, Tex::Grass_Side, Tex::Dirt},  // Grass
    {Tex::Dirt, Tex::Dirt, Tex::Dirt},         // Dirt
    {Tex::Water, Tex::Water, Tex::Water},      // Water
    {Tex::Leaves, Tex::Leaves, Tex::Leaves},   // Leaves
    {Tex::Wood, Tex::Wood, Tex::Wood},         // Wood
    {Tex::Planks, Tex::Planks, Tex::Planks},   // Planks
    {Tex::Glass, Tex::Glass, Tex::Glass},      // Glass
};

static_assert(std::size(BLOCK_TEX) == static_cast<size_t>(BlockType::Count),
              "BLOCK_TEX needs exactly one row per BlockType");

inline std::optional<Tex> blockTex(BlockType t, BlockFace face) {
  return isValid(t) ? BLOCK_TEX[static_cast<size_t>(t)][static_cast<int>(face)]
                    : std::nullopt;
}

// Mirrors FACE_DIR from renderer to a texture
// 2       = Top    (+Y)
// 3       = Bottom (-Y)
// 0, 1, 4 = Sides  (X&Z)
inline BlockFace indexToFace(int faceIndex) {
  switch (faceIndex) {
    case 2:
      return BlockFace::Top;
    case 3:
      return BlockFace::Bottom;
    default:
      return BlockFace::Side;
  }
}