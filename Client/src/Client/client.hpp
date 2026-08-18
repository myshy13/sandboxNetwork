#pragma once

#include <enet/enet.h>
#include <raylib.h>
#include <vector>

struct OnlinePlayer {
  int id{-1};
  Vector3 pos{0,0,0};
};

class Client {
  int port{9798};
  ENetHost *host;
  ENetPeer *peer;

  std::vector<OnlinePlayer> players{};

  int playerId{-1};

  OnlinePlayer *findPlayer(int id) {
    for (auto &p : players) {
      if (p.id == id)
        return &p;
    }
    return nullptr;
  }

public:
  void sendPlayerPosition(const Transform &transform);
  void poll();
  std::vector<OnlinePlayer> getPlayers() {
    return players;
  };
  Client();
};