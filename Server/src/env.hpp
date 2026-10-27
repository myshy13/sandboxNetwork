#pragma once

#include <cstddef>
#include <cstdint>

#include "sharedEnv.hpp"

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
    0.2f; // a stalled tick simulates at most this much time

constexpr int PLAYER_MAX_HEALTH =
    SHARED_PLAYER_HEALTH; // the client's env::MAX_HEALTH aliases it too

constexpr int DEFAULT_VIEW_RADIUS =
    MAX_CHUNK_DISTANCE / 2;            // default, until the client broadcasts
constexpr int CHUNKS_PER_TICK = 2;     // bandwidth budget per player per tick

constexpr uint32_t saveFormatVersion = 6; // Added Stone blocktype
constexpr uint32_t terrainVersion = 6;    // Add stone generation below y=4

// world properties
constexpr float FLOW_INTERVAL = 0.2f;
constexpr float WATER_HEIGHT = 6;
constexpr float STONE_HEIGHT = 2;

// #define DEBUG_SHORT_DAY // easy toggle here
#ifdef DEBUG_SHORT_DAY
constexpr float DAY_LENGTH_SECONDS = 10.0f; // for debugging shadows
#else
constexpr float DAY_LENGTH_SECONDS =
    60.0f * 20.0f; // 20 minutes // 1200 seconds
#endif
constexpr float DAY_DEFAULT_TIME = 0.5f;
constexpr float TIME_BROADCAST_INTERVAL = 30.0f; // seconds between resyncs
constexpr int EXPIRY_CHECK_INTERVAL =
    30;                                 // seconds between ticket expiry checks
constexpr int EXPIRY_GRACE_WINDOW = 30; // grace window for clients

constexpr int REVOKED_UNTIL_LIFETIME =
    3600 + EXPIRY_GRACE_WINDOW; // Must match ticketSigner.ts

constexpr bool debug{false};

constexpr float FALL_DAMAGE_PER_BLOCK = 1.0f;
constexpr int FALL_SAFE_BLOCKS = 3;

constexpr float SERVER_STATUS_INTERVAL = 5.0f;

// which stats to log, so doesn't get messy
namespace stats {
// ==== performance ==== //
constexpr bool PERFORMANCE_STATS = true;
constexpr bool TICK_TIME = false;
constexpr bool WARNINGS = true;
constexpr bool UPTIME = true;
constexpr bool MEMORY = false;

// ==== players ==== //
constexpr bool PLAYER_STATS = false;
constexpr bool PLAYER_COUNT = true;
constexpr bool PLAYER_CAPACITY = true;
constexpr bool ADMIN_COUNT = true;

// ==== World stats ==== //
constexpr bool WORLD_STATS = true;
constexpr bool TIME_OF_DAY = true;
constexpr bool GENERATED_CHUNKS = false;

// ==== Combat ==== //
constexpr bool COMBAT_STATS = false;
constexpr bool BULLET_COUNT = true;
constexpr bool BULLETS_FIRED = true;
constexpr bool RECENT_KILLS = true;

// ==== Network === //
constexpr bool NETWORK_STATS = false;
constexpr bool PACKET_COUNT =
    true; // for each handlereceive, sendto, and broadcast.

}; // namespace stats

namespace guest {
constexpr bool CAN_BUILD = true;
constexpr bool CAN_CHAT = true;
} // namespace guest

} // namespace env
