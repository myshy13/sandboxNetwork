#pragma once

#include "Net/transport.hpp"
#include "structs.hpp"
#include <algorithm>
#include <optional>
#include <raylib.h>
#include <raymath.h>
#include <string>
#include <utility>
#include <vector>

#if __has_include("env.hpp")
#include "env.hpp"
#endif

struct OnlinePlayer {
  int id{-1};
  Vector3 pos{0, 0, 0};
  float pitch{0.0f};
  float yaw{0.0f};
};

struct ChatEntry {
  int id;
  std::string text;
  double receivedAt = GetTime();
};

class Client {
  int port{SERVER_PORT};
  std::unique_ptr<Transport> transport;

  std::vector<OnlinePlayer> players{};
  std::vector<Bullet> bullets{};
  std::vector<ChatEntry> chat{};

  int health{3};
  int playerId{-1};
  std::optional<Vector3> respawnTo{};

  OnlinePlayer *findPlayer(int id) {
    for (auto &p : players) {
      if (p.id == id)
        return &p;
    }
    return nullptr;
  }
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
  void createBullet(Vector3 pos);
  void sendChatMessage(const std::string &msg);
  // Connecting is asynchronous, so the game starts before this goes true.
  bool isConnected() const {
    return transport->isConnected();
  }
  const std::vector<OnlinePlayer> &getPlayers() const {
    return players;
  };
  const std::vector<Bullet> &getBullets() const {
    return bullets;
  };
  const std::vector<ChatEntry> &getChat() const {
    return chat;
  };
  const int &getHealth() const {
    return health;
  };
  // Returns a position exactly once per respawn, nullopt otherwise.
  std::optional<Vector3> takeRespawn() {
    return std::exchange(respawnTo, std::nullopt);
  }
  void updateBullets(float dt) {
    for (Bullet &b : bullets) {
      b.pos = Vector3Add(b.pos, Vector3Scale(b.vel, dt));
    }
  }
  Client();
};
