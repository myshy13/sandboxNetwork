#pragma once

#include <cstddef>

// ==== server tunables ==== //
// Gameplay/config constants in one place. Compile-time only, like the client's
// env.hpp.

namespace env {

constexpr int PORT = 9798;                // default UDP port
constexpr float TICK_RATE = 1.0f / 60.0f; // 60 tps

constexpr float BULLET_SPEED = 500.0f; // world units per tick, scaled by dt
constexpr float BULLET_LIFETIME =
    20.0f; // seconds before a bullet expires on its own
constexpr float MUZZLE_DISTANCE =
    1.0f; // spawn offset ahead of the shooter's eye

constexpr int PLAYER_MAX_HEALTH =
    20; // must match the client's env::MAX_HEALTH (Client/src/env.hpp)

// Blocks per initBlocks message on connect - keeps one big world sync from
// blocking the client on a single huge collision-grid rebuild.
constexpr size_t WORLD_SYNC_CHUNK_SIZE = 500;

constexpr int MAX_VIEW_RADIUS = 8;     // absolute maximum (to stop cheating)
constexpr int DEFAULT_VIEW_RADIUS = 4; // default
constexpr int CHUNKS_PER_TICK = 2;     // bandwidth budget per player per tick

} // namespace env
