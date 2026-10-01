#pragma once

#include "raylib.h"
struct TreeBlock {
  int dx, dy, dz;
  Color color;
};
// Trunk (dy 0-3), a wide canopy layer with its 4 corners trimmed (dy 4), then
// a full narrow cap on top (dy 5) - matches a 5-wide octagon under a 3x3 cap.
static constexpr TreeBlock TREE_SHAPE[] = {
    {0, 0, 0, BROWN},
    {0, 1, 0, BROWN},
    {0, 2, 0, BROWN},
    {0, 3, 0, BROWN},

    {-1, 4, -2, DARKGREEN}, {0, 4, -2, DARKGREEN},  {1, 4, -2, DARKGREEN},
    {-2, 4, -1, DARKGREEN}, {-1, 4, -1, DARKGREEN}, {0, 4, -1, DARKGREEN},
    {1, 4, -1, DARKGREEN},  {2, 4, -1, DARKGREEN},  {-2, 4, 0, DARKGREEN},
    {-1, 4, 0, DARKGREEN},  {0, 4, 0, DARKGREEN},   {1, 4, 0, DARKGREEN},
    {2, 4, 0, DARKGREEN},   {-2, 4, 1, DARKGREEN},  {-1, 4, 1, DARKGREEN},
    {0, 4, 1, DARKGREEN},   {1, 4, 1, DARKGREEN},   {2, 4, 1, DARKGREEN},
    {-1, 4, 2, DARKGREEN},  {0, 4, 2, DARKGREEN},   {1, 4, 2, DARKGREEN},

    {-1, 5, -1, DARKGREEN}, {0, 5, -1, DARKGREEN},  {1, 5, -1, DARKGREEN},
    {-1, 5, 0, DARKGREEN},  {0, 5, 0, DARKGREEN},   {1, 5, 0, DARKGREEN},
    {-1, 5, 1, DARKGREEN},  {0, 5, 1, DARKGREEN},   {1, 5, 1, DARKGREEN},
};