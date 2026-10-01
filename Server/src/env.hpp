#pragma once

#include <cstddef>
#include <cstdint>

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

// ==== what a client may ask for ==== //
constexpr float SHOT_INTERVAL = 0.05f; // fastest fire rate; must not be slower
                                       // than the client's bullet cooldown
constexpr float SHOT_BURST =
    3.0f; // shots allowed to arrive bunched together by the network
constexpr float MAX_MUZZLE_OFFSET =
    20.0f; // a bullet must start this close to its shooter (eye height + lag)
constexpr float PLACE_REACH = 65.0f; // client REACH (50) + eye height + lag;
                                     // CHEATS' longer reach isn't honoured
constexpr float WORLD_LIMIT = 1000000.0f; // |coordinate| a player may claim;
                                          // cell keys overflow past ~5.2M
constexpr float MAX_TICK_DT =
    0.1f; // a stalled tick simulates at most this much time

constexpr int PLAYER_MAX_HEALTH =
    20; // must match the client's env::MAX_HEALTH (Client/src/env.hpp)

constexpr int MAX_VIEW_RADIUS = 8;     // absolute maximum (to stop cheating)
constexpr int DEFAULT_VIEW_RADIUS = 4; // default
constexpr int CHUNKS_PER_TICK = 2;     // bandwidth budget per player per tick

constexpr uint32_t saveFormatVersion = 2; // Object gained `level`
constexpr uint32_t terrainVersion =
    3; // deeper water, only its top layer is a source

constexpr float FLOW_INTERVAL = 0.2f;
constexpr float WATER_HEIGHT = 6;

} // namespace env
