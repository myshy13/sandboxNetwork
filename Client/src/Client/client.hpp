#pragma once

#include <raylib.h>
#include "raymath.h"
#include "Net/transport.hpp"
#include "structs.hpp"
#include <algorithm>
#include <string>
#include <vector>

#ifndef SERVER_IP
#define SERVER_IP "192.168.10.111"
#endif

struct OnlinePlayer {
  int id{-1};
  Vector3 pos{0, 0, 0};
  float pitch{0.0f};
  float yaw{0.0f};
};

class Client {
  int port{9798};
  std::unique_ptr<Transport> transport;

  std::vector<OnlinePlayer> players{};
  std::vector<Bullet> bullets{};

  int playerId{-1};

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
  void createBullet();
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
  void updateBullets(float dt) {
    for (Bullet &b : bullets) {
      b.pos = Vector3Add(b.pos, Vector3Scale(b.vel, dt));
    }
  }
  Client();
};
