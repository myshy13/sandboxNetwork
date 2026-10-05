#pragma once

#include <string>

#include "sharedEnv.hpp"

// ==== connection config ==== //
namespace env {
constexpr const char* SERVER_IP = "127.0.0.1";
constexpr int SERVER_PORT = 9798;
constexpr int MAX_HEALTH = SHARED_PLAYER_HEALTH;
constexpr Vector3 PLAYER_SCALE = SHARED_PLAYER_SCALE;
constexpr std::string VERSION = SHARED_VERSION;
constexpr Vector3 BLOCKSIZE = {5, 5, 5};

}  // namespace env

// Compile-time toggles
// #define SERVER_WSS  // wss:// instead of ws:// (define when the page is
// https)
#define DEBUG

#define CHEATS  // debug stuff (for testing)
#define CHAT

#ifdef CHEATS
#define REACH 500.0f
#else
#define REACH 50.0f
#endif
