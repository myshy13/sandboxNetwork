#pragma once

#include "Models/Object.hpp"
struct TreeBlock {
  int dx, dy, dz;
  BlockType type;
};
// Trunk (dy 0-3), a wide canopy layer with its 4 corners trimmed (dy 4), then
// a full narrow cap on top (dy 5) - matches a 5-wide octagon under a 3x3 cap.
inline constexpr TreeBlock TREE_SHAPE[] = {
    {0, 0, 0, BlockType::Wood},     {0, 1, 0, BlockType::Wood},
    {0, 2, 0, BlockType::Wood},     {0, 3, 0, BlockType::Wood},

    {-1, 4, -2, BlockType::Leaves}, {0, 4, -2, BlockType::Leaves},
    {1, 4, -2, BlockType::Leaves},  {-2, 4, -1, BlockType::Leaves},
    {-1, 4, -1, BlockType::Leaves}, {0, 4, -1, BlockType::Leaves},
    {1, 4, -1, BlockType::Leaves},  {2, 4, -1, BlockType::Leaves},
    {-2, 4, 0, BlockType::Leaves},  {-1, 4, 0, BlockType::Leaves},
    {0, 4, 0, BlockType::Leaves},   {1, 4, 0, BlockType::Leaves},
    {2, 4, 0, BlockType::Leaves},   {-2, 4, 1, BlockType::Leaves},
    {-1, 4, 1, BlockType::Leaves},  {0, 4, 1, BlockType::Leaves},
    {1, 4, 1, BlockType::Leaves},   {2, 4, 1, BlockType::Leaves},
    {-1, 4, 2, BlockType::Leaves},  {0, 4, 2, BlockType::Leaves},
    {1, 4, 2, BlockType::Leaves},

    {-1, 5, -1, BlockType::Leaves}, {0, 5, -1, BlockType::Leaves},
    {1, 5, -1, BlockType::Leaves},  {-1, 5, 0, BlockType::Leaves},
    {0, 5, 0, BlockType::Leaves},   {1, 5, 0, BlockType::Leaves},
    {-1, 5, 1, BlockType::Leaves},  {0, 5, 1, BlockType::Leaves},
    {1, 5, 1, BlockType::Leaves},
};