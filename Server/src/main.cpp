#include "models.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <enet/enet.h>
#include <sstream>
#include <string>
#include <vector>

int nextClientId{1};

struct Player {
  int id{nextClientId};
  Vector3 pos;
};

std::vector<Player> players;

Player *findPlayer(int id) {
  for (auto &p : players) {
    if (p.id == id)
      return &p;
  }
  return nullptr;
}

void deletePlayer(int id) {

  auto it = std::find_if(
      players.begin(), players.end(),
      [id](const Player &p) { return p.id == id; });

  if (it != players.end()) {
    players.erase(it);
  }
}

int main() {
  if (enet_initialize() != 0) {
    std::fprintf(stderr, "Failed to initialize ENet\n");
    return EXIT_FAILURE;
  }

  std::printf("ENet initialized\n");
  ENetAddress address;
  address.host = ENET_HOST_ANY;
  address.port = 9798;

  ENetHost *server = enet_host_create(&address, 32, 2, 0, 0);
  if (server == nullptr) {
    std::fprintf(stderr, "Failed to create ENet server host\n");
    return 1;
  }

  while (true) {
    ENetEvent event;
    int result = enet_host_service(server, &event, 1000);
    if (result > 0) {
      switch (event.type) {
      case ENET_EVENT_TYPE_CONNECT: {
        std::printf("Client connected\n");
        std::string reply = "givenId " + std::to_string(nextClientId);
        std::printf("%s\n", reply.c_str());
        ENetPacket *outPacket =
            enet_packet_create(reply.c_str(), reply.size(), 0);
        enet_peer_send(event.peer, 0, outPacket);
        Player newPlayer;
        newPlayer.id = nextClientId;
        newPlayer.pos = {0, 0, 0};
        event.peer->data =
            reinterpret_cast<void *>(static_cast<intptr_t>(nextClientId));
        players.push_back(newPlayer);
        nextClientId++;

        break;
      }
      case ENET_EVENT_TYPE_DISCONNECT: {

        std::printf("Client disconnected\n");
        int id = static_cast<int>(reinterpret_cast<intptr_t>(event.peer->data));
        deletePlayer(id);

        std::string message = "deletePlayer " + std::to_string(id);
        ENetPacket *packet =
            enet_packet_create(message.c_str(), message.size(), 0);

        enet_host_broadcast(server, 0,
                            packet); // sends to every connected peer
        break;
      }
      case ENET_EVENT_TYPE_RECEIVE: {
        std::string data(reinterpret_cast<char *>(event.packet->data),
                         event.packet->dataLength);
        // ==== pos update handler ====
        if (data.rfind("playerUpdate ", 0) == 0) {
          if (data.length() >= 13) {
            data.erase(0, 13);
          }
          std::stringstream ss(data);
          float x, y, z;
          int id;
          if (ss >> id >> x >> y >> z) {
            Player *player = findPlayer(id);
            if (player != nullptr) {
              player->pos = {x, y, z};
            }
            // ==== notify all peers ====
            std::string message = "updatePlayer " + std::to_string(id) + " " +
                                  std::to_string(x) + " " + std::to_string(y) +
                                  " " + std::to_string(z);
            ENetPacket *packet =
                enet_packet_create(message.c_str(), message.size(), 0);

            enet_host_broadcast(server, 0,
                                packet); // sends to every connected peer
          } else {
            std::fprintf(stderr, "Malformed playerUpdate from client\n");
          }
        } else {
          std::printf("Invalid request\n");
        }

        enet_packet_destroy(event.packet);
        break;
      }
      case ENET_EVENT_TYPE_NONE:
        break;
      }
    }
  }

  enet_host_destroy(server);
  enet_deinitialize();
  std::printf("ENet Deinitialised\n");

  return 0;
}
