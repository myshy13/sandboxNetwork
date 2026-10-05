#pragma once

#include <cstddef>
#include <cstdint>

// ==== server tunables ==== //
// Gameplay/config constants in one place. Compile-time only, like the client's
// env.hpp.

namespace env {

constexpr int PORT = 9798;                // default UDP port
constexpr float TICK_RATE = 1.0f / 60.0f; // 60 tps

constexpr int DEFAULT_MAX_PLAYERS = 24;

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

constexpr uint32_t saveFormatVersion = 5; // Added timeOfDay to SaveMeta
constexpr uint32_t terrainVersion = 5; // block damage now rolls from a 3D hash

constexpr float FLOW_INTERVAL = 0.2f;
constexpr float WATER_HEIGHT = 6;

// #define DEBUG_SHORT_DAY // easy toggle here
#ifdef DEBUG_SHORT_DAY
constexpr float DAY_LENGTH_SECONDS = 10.0f; // for debugging shadows
#else
constexpr float DAY_LENGTH_SECONDS =
    60.0f * 20.0f; // 20 minutes // 1200 seconds
#endif
constexpr float DAY_DEFAULT_TIME = 0.5f;
constexpr float TIME_BROADCAST_INTERVAL = 30.0f; // seconds between resyncs

} // namespace env
