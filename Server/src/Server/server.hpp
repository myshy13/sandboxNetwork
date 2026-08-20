#pragma once

#include <enet/enet.h>
#include <optional>
#include <raylib.h>
#include <vector>

struct Bullet {
  Vector3 pos;
  Vector3 vel;
  int playerId;
  int bulletId;

  float deathCountdown = 3;
};

struct Player {
  int id;
  Vector3 pos;
  float pitch{0.0f};
  float yaw{0.0f};
};

class Server {
  int port{9798};
  ENetHost *host;

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
  void deleteBullet(int id);

  void handleConnect(ENetEvent &event);
  void handleDisconnect(ENetEvent &event);
  void handleReceive(ENetEvent &event);
  void tick(float dt);

public:
  void poll();
  Server();
};
