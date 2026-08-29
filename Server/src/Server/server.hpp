#pragma once

#include "Net/connection.hpp"
#include "Net/ws_proxy.hpp"

#include <enet/enet.h>
#include <memory>
#include <optional>
#include <raylib.h>
#include <string>
#include <unordered_map>
#include <vector>

struct Bullet {
  Vector3 pos;
  Vector3 vel;
  int playerId;
  int bulletId;

  float deathCountdown = 3;
};

constexpr int PLAYER_MAX_HEALTH = 3;

struct Player {
  int id;
  Vector3 pos;
  float pitch{0.0f};
  float yaw{0.0f};
  int health{PLAYER_MAX_HEALTH};
  std::optional<std::string> displayName;
};

class Server {
  int port{9798};
  ENetHost *host;

  std::unique_ptr<WsProxy> wsProxy;

  // Every client, ENet and WebSocket alike, keyed by player id.
  std::unordered_map<int, std::unique_ptr<Connection>> connections;
  std::unordered_map<std::string, int> wsPlayerIds;

  int nextClientId{1};
  int nextBulletId{1};
  std::vector<Player> players{};
  std::vector<Bullet> bullets;

  Player *findPlayer(int id) {
    for (auto &p : players) {
      if (p.id == id)
        return &p;
    }
    return nullptr;
  }
  void deletePlayer(int id);
  std::optional<Bullet> createBullet(int playerId);

  // ==== transport-agnostic game logic ==== //
  int handleConnect(std::unique_ptr<Connection> connection);
  void handleDisconnect(int playerId);
  void handleReceive(int playerId, const std::string &data);
  void tick(float dt);

  void sendTo(int playerId, const std::string &bytes, bool reliable);
  void broadcast(const std::string &bytes, bool reliable);

  // ==== transport plumbing ==== //
  void pumpEnet();
  void pumpWebSockets();

public:
  void poll();
  // wsPort of 0 leaves the browser proxy switched off.
  explicit Server(int wsPort = 0);
  ~Server();
};
