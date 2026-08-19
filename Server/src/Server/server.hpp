#pragma once

#include <enet/enet.h>
#include <vector>

#include "models.hpp"

struct Player {
  int id;
  Vector3 pos;
};

class Server {
  int port{9798};
  ENetHost *host;

  int nextClientId{1};
  std::vector<Player> players{};

  Player *findPlayer(int id) {
    for (auto &p : players) {
      if (p.id == id)
        return &p;
    }
    return nullptr;
  }
  void deletePlayer(int id);

  void handleConnect(ENetEvent &event);
  void handleDisconnect(ENetEvent &event);
  void handleReceive(ENetEvent &event);

public:
  void poll();
  Server();
};
