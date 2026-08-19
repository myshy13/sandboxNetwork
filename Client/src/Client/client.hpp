#pragma once

#include <algorithm>
#include <enet/enet.h>
#include <raylib.h>
#include <vector>

#ifndef SERVER_IP
#define SERVER_IP "192.168.10.111"
#endif

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
  void deletePlayer(int id) {
    auto it = std::find_if(players.begin(), players.end(),
                           [id](const OnlinePlayer &p) { return p.id == id; });
    if (it != players.end()) {
      players.erase(it);
    }
  }

public:
  void sendPlayerPosition(const Transform &transform);
  void poll();
  std::vector<OnlinePlayer> getPlayers() {
    return players;
  };
  Client();
};