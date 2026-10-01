#pragma once

#include "Models/Object.hpp"
#include "Net/transport.hpp"
#include "structs.hpp"
#include <algorithm>
#include <array>
#include <optional>
#include <raylib.h>
#include <raymath.h>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#if __has_include("env.hpp")
#include "env.hpp"
#else
#error env.hpp should be included
#error cp src/env.example.hpp src/env.hpp
#endif

struct OnlinePlayer {
  int id{-1};
  Vector3 pos{0, 0, 0};
  float pitch{0.0f};
  float yaw{0.0f};
  std::optional<std::string> name;
  // interpolation
  std::array<Vector3, 2> last2pos{{{0, 0, 0}, {0, 0, 0}}};
  std::array<float, 2> last2yaw{{0.0f, 0.0f}}; // glide start, glide end
  double updatedAt{0.0};
  double glideTime{0.0}; // seconds the current glide takes, measured from the gap between updates
};

struct ChatEntry {
  int id;
  std::string text;
  double receivedAt = GetTime();
};

// One change to the block world. Applied in arrival order: grouping by kind reorders them and leaves ghost blocks.
struct WorldEvent {
  enum class Kind : uint8_t { LoadChunk, UnloadChunk, Add, Remove, Damage, WaterLevel };
  explicit WorldEvent(Kind k) : kind(k) {}
  Kind kind;
  int cx{0}, cz{0};           // LoadChunk / UnloadChunk
  std::vector<Object> blocks; // LoadChunk
  Object object;              // Add
  Vector3 pos{0, 0, 0};       // Remove / Damage / WaterLevel
  uint8_t level{0};           // WaterLevel
};

class Client {
private:
  int port{env::SERVER_PORT};
  std::unique_ptr<Transport> transport;

  std::vector<OnlinePlayer> players{};
  std::vector<Bullet> bullets{};
  std::optional<std::string> kickReason{};
  std::vector<ChatEntry> chat{};
  std::optional<std::string> playerName;
  std::unordered_map<int, int> kills;
  std::unordered_map<int, std::string> pendingNames; // names that arrived before that player's first position

  int health{env::MAX_HEALTH};
  int playerId{-1};
  std::optional<Vector3> respawnTo{};

  // ==== drain variables ==== //
  std::vector<WorldEvent> pendingWorldEvents{};

  bool handshakeSent{false};
  double connectStartedAt{0.0};
  bool connecting{false};

  void deletePlayer(int id) {
    auto it = std::find_if(players.begin(), players.end(),
                           [id](const OnlinePlayer &p) { return p.id == id; });
    if (it != players.end()) {
      players.erase(it);
    }
  }
  void handleMessage(const std::string &data);

public:
  void sendPlayerPosition(const Transform &transform, float pitch, float yaw);
  void poll();
  bool aimingAtPlayer(Ray facing);
  void createBullet(Vector3 origin, Vector3 dir);
  void sendChatMessage(const std::string &msg);
  void setName(const std::string &msg);
  void placeObject(const Object &object);
  // Asks the server to stream this many chunks around us. False if the handshake hasn't finished yet (call again).
  bool sendViewRadius(int chunks);
  static constexpr double CONNECT_TIMEOUT = 5.0; // seconds before an attempt counts as failed
  static constexpr double POS_UPDATE_INTERVAL = 1.0 / 12.0;
  static constexpr float SNAP_DISTANCE        = 20;
  static constexpr double MIN_GLIDE_TIME      = 0.02; // bounds for the measured gap between updates
  static constexpr double MAX_GLIDE_TIME      = 0.25;

  // Starts a fresh session (old connection and state dropped); true if one started.
  // Safe to call every frame: it waits out an attempt in flight and never retries after a kick.
  bool connect();
  // Leaving on purpose: the next connect() starts clean.
  void disconnect() {
    if (transport)
      transport->disconnect();
    connecting = false;
    kickReason.reset();
  }
  double secondsSinceConnect() const {
    return GetTime() - connectStartedAt;
  }
  OnlinePlayer *findPlayer(int id) {
    for (auto &p : players) {
      if (p.id == id)
        return &p;
    }
    return nullptr;
  }

  std::optional<std::string> idToName(int id) {
    for (auto& p : players) {
      if (p.id == id) {
        return p.name;
      }
    }
    return std::nullopt;
  }
  // Connecting is asynchronous, so the game starts before this goes true.
  bool isConnected() const {
    return transport && transport->isConnected();
  }
  const std::vector<OnlinePlayer> &getPlayers() const {
    return players;
  };
  const std::optional<std::string> &getKickReason() const {
    return kickReason;
  };
  const std::vector<Bullet> &getBullets() const {
    return bullets;
  };
  const std::optional<std::string> &getName() const {
    return playerName;
  };
  const std::vector<ChatEntry> &getChat() const {
    return chat;
  };
  const std::unordered_map<int, int> &getKills() const {
    return kills;
  };
  void addLocalChat(std::string text) {
    ChatEntry c;
    c.id         = -1;
    c.receivedAt = GetTime();
    c.text       = text;
    chat.push_back(c);
  }
  const int &getPlayerId() const {
    return playerId;
  }
  void clearChat() {
    chat = {};
  }
  const int &getHealth() const {
    return health;
  };
  // ==== get respawn pos ====
  std::optional<Vector3> takeRespawn() {
    return std::exchange(respawnTo, std::nullopt);
  }
  // ==== drains ==== //
  std::vector<WorldEvent> takeWorldEvents() {
    return std::exchange(pendingWorldEvents, {});
  }

  void updateBullets(float dt) {
    for (Bullet &b : bullets) {
      b.pos = Vector3Add(b.pos, Vector3Scale(b.vel, dt));
    }
  }
  void updatePlayers();
};
