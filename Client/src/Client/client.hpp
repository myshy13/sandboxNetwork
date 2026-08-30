#pragma once

#include "Models/Object.hpp"
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
  std::optional<std::string> name;
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
  std::optional<std::string> playerName;

  int health{3};
  int playerId{-1};
  std::optional<Vector3> respawnTo{};

  std::vector<Object> pendingObjects{};
  std::vector<int> pendingRemovals{};

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
#ifdef CHEATS
  void createBullet(Vector3 pos);
#else
  void createBullet();
#endif
  void sendChatMessage(const std::string &msg);
  void setName(const std::string &msg);
  void placeObject(const Object &object);
  OnlinePlayer *findPlayer(int id) {
    for (auto &p : players) {
      if (p.id == id)
        return &p;
    }
    return nullptr;
  }
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
  const std::optional<std::string> &getName() const {
    return playerName;
  };
  const std::vector<ChatEntry> &getChat() const {
    return chat;
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
  // Returns a position exactly once per respawn, nullopt otherwise.
  std::optional<Vector3> takeRespawn() {
    return std::exchange(respawnTo, std::nullopt);
  }
  std::vector<Object> takeNewObjects() {
    return std::exchange(pendingObjects, {});
  }
  std::vector<int> takeRemovedObjects() {
    return std::exchange(pendingRemovals, {});
  }
  void updateBullets(float dt) {
    for (Bullet &b : bullets) {
      b.pos = Vector3Add(b.pos, Vector3Scale(b.vel, dt));
    }
  }
  Client();
};
