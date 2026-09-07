#pragma once

// ==== connection config ==== //
namespace env {
constexpr const char *SERVER_IP = "127.0.0.1";
constexpr int SERVER_PORT       = 9798;
} // namespace env

// Compile-time toggles live as #define because #ifdef needs them.
// #define SERVER_WSS  // wss:// instead of ws:// (define when the page is https)
#define DEBUG

#define CHEATS // debug stuff (for testing)
#define CHAT
