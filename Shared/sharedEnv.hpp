#pragma once
#include <raylib.h>
#include <string>

constexpr Vector3 SHARED_PLAYER_SCALE = {2.3f, 8.0f, 2.3f};
constexpr Vector3 SHARED_BLOCK_SIZE = {5, 5, 5}; // must be cubic
constexpr int SHARED_PLAYER_HEALTH = 20;
constexpr int SHARED_WATER_MAX_LEVEL =
    8; // weakest flowing water; server flow and client water height both use it
constexpr std::string SHARED_VERSION = "0.7.3"; // Fixed bug: Shadow snapping