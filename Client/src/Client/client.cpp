#include "client.hpp"
#include <cstdio>
#include <cstdlib>
#include <enet/enet.h>
#include <iostream>
#include <raylib.h>
#include <sstream>
#include <stdexcept>
#include <string>

// ==== outgoing messages ==== //

void Client::sendPlayerPosition(const Transform &transform) {
  if (playerId == -1)
    return;
  std::string message = "playerUpdate " + std::to_string(playerId) + " " + std::to_string(transform.translation.x) + " " + std::to_string(transform.translation.y) + " " + std::to_string(transform.translation.z) + "\n";
  ENetPacket *packet  = enet_packet_create(message.c_str(), message.size(), ENET_PACKET_FLAG_RELIABLE);

  enet_peer_send(peer, 0, packet);
  enet_host_flush(host);
};

// ==== connection setup ==== //

Client::Client() {
  if (enet_initialize() != 0) {
    std::cerr << "Failed to initialise ENet client host\n";
    std::exit(EXIT_FAILURE);
  }

  host = enet_host_create(nullptr, 1, 2, 0, 0);
  if (host == nullptr) {
    std::cerr << "Failed to create ENet client host\n";
    std::exit(EXIT_FAILURE);
  }

  ENetAddress address;
  enet_address_set_host(&address, "127.0.0.1");
  address.port = port;

  peer = enet_host_connect(host, &address, 2, 0);
  if (peer == nullptr) {
    std::cerr << "No available peers for connection attempt\n";
    std::exit(EXIT_FAILURE);
  }

  ENetEvent event;
  if (enet_host_service(host, &event, 5000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT) {
    std::cout << "Connected to server\n";
  } else {
    std::cerr << "Connection to server failed\n";
    enet_peer_reset(peer);
    std::exit(EXIT_FAILURE);
  }
}

// ==== incoming message handling ==== //
void Client::poll() {
  ENetEvent event;
  if (enet_host_service(host, &event, 0) > 0) {
    switch (event.type) {
    case ENET_EVENT_TYPE_RECEIVE: {
      std::string data(reinterpret_cast<char *>(event.packet->data), event.packet->dataLength);

      // ==== id assignment ==== //
      if (data.rfind("givenId ", 0) == 0) {
        data.erase(0, 8);
        try {
          playerId = std::stoi(data);
        } catch (const std::invalid_argument &e) {
          std::exit(EXIT_FAILURE);
        } catch (const std::out_of_range &e) {
          std::exit(EXIT_FAILURE);
        }
      }
      // ==== other player position updates ==== //
      else if (data.rfind("updatePlayer ", 0) == 0) {
        if (data.length() >= 13) {
          data.erase(0, 13);
        }
        std::stringstream ss(data);
        float x, y, z;
        int id;
        if (ss >> id >> x >> y >> z) {
          if (id != playerId) {
            OnlinePlayer *player = findPlayer(id);
            if (player == nullptr) {
              OnlinePlayer player;
              player.id  = id;
              player.pos = {x, y, z};
              players.push_back(player);
            } else {
              player->pos = {x, y, z};
            }
          }
        } else {
          std::cerr << "Malformed playerUpdate from client\n";
        }
      }
      break;
    }
    case ENET_EVENT_TYPE_DISCONNECT: {
      std::cerr << "Error: Client disconnect\n";
      std::exit(EXIT_FAILURE);
      break;
    }
    default:
      break;
    }
  }
}