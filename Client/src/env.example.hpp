#pragma once

// ==== connection config ==== //
namespace env {
constexpr const char *SERVER_IP = "127.0.0.1";
constexpr int SERVER_PORT       = 9798;
constexpr int MAX_HEALTH        = 20; // must match the server's env::PLAYER_MAX_HEALTH
} // namespace env

// Compile-time toggles
// #define SERVER_WSS  // wss:// instead of ws:// (define when the page is https)
#define DEBUG

#define CHEATS // debug stuff (for testing)
#define CHAT

#ifdef CHEATS
#define REACH 500.0f
#else
#define REACH 50.0f
#endif

#define VERSION "0.2.3.001" // added player health
