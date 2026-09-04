#pragma once

#include "Models/Object.hpp"
#include "Net/transport.hpp"
#include "structs.hpp"
#include <algorithm>
#include <optional>
#include <raylib.h>
#include <raymath.h>
#include <string>
#include <unordered_map>
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
private:
  int port{SERVER_PORT};
  std::unique_ptr<Transport> transport;

  std::vector<OnlinePlayer> players{};
  std::vector<Bullet> bullets{};
  std::optional<std::string> kickReason{};
  std::vector<ChatEntry> chat{};
  std::optional<std::string> playerName;
  std::unordered_map<int, int> kills;

  int health{3};
  int playerId{-1};
  std::optional<Vector3> respawnTo{};

  std::vector<Object> pendingObjects{};
  std::vector<int> pendingRemovals{};
  std::vector<int> pendingDamage{};
  bool handshakeSent{false};

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
  void createBullet(const Camera3D &camera);
  void sendChatMessage(const std::string &msg);
  void setName(const std::string &msg);
  void placeObject(const Object &object);
  void disconnect() { transport->disconnect(); }
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
  std::vector<int> takeDamagedObjects() {
    return std::exchange(pendingDamage, {});
  }
  void updateBullets(float dt) {
    for (Bullet &b : bullets) {
      b.pos = Vector3Add(b.pos, Vector3Scale(b.vel, dt));
    }
  }
  Client();
};
