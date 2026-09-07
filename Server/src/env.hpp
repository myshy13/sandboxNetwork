#pragma once

// ==== server tunables ==== //
// Gameplay/config constants in one place. Compile-time only, like the client's env.hpp.

namespace env {

constexpr int   PORT              = 9798;   // default UDP port
constexpr float TICK_RATE         = 1.0f / 60.0f;

constexpr float BULLET_SPEED      = 500.0f; // world units per tick, scaled by dt
constexpr float BULLET_LIFETIME   = 20.0f; // seconds before a bullet expires on its own
constexpr float MUZZLE_DISTANCE   = 5.0f;   // spawn offset ahead of the shooter's eye

constexpr float WORLD_SAVE_PERIOD = 30.0f;  // seconds between world autosaves

constexpr int PLAYER_MAX_HEALTH = 20;

} // namespace env
