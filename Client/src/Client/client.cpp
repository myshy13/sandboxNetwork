#include "client.hpp"
#include "Protocol/protocol.hpp"
#include <cstdio>
#include <cstdlib>
#include <enet/enet.h>
#include <iostream>
#include <raylib.h>
#include <stdexcept>
#include <string>

// ==== outgoing messages ==== //

void Client::sendPlayerPosition(const Transform &transform) {
  if (playerId == -1)
    return;
  auto bytes = proto::pack(proto::Type::PlayerUpdate, proto::PlayerUpdate{playerId, transform.translation});
  ENetPacket *packet = enet_packet_create(bytes.data(), bytes.size(), ENET_PACKET_FLAG_RELIABLE);

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
  enet_address_set_host(&address, SERVER_IP);
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
      if (data.empty()) {
        break;
      }

      switch (proto::peekType(data)) {
      // ==== id assignment ==== //
      case proto::Type::GivenId: {
        playerId = proto::unpack<proto::GivenId>(data).id;
        break;
      }
      // ==== other player position updates ==== //
      case proto::Type::PlayerUpdate: {
        auto msg = proto::unpack<proto::PlayerUpdate>(data);
        if (msg.id != playerId) {
          OnlinePlayer *player = findPlayer(msg.id);
          if (player == nullptr) {
            OnlinePlayer newPlayer;
            newPlayer.id  = msg.id;
            newPlayer.pos = msg.pos;
            players.push_back(newPlayer);
          } else {
            player->pos = msg.pos;
          }
        }
        break;
      }
      // ==== other player disconnected ==== //
      case proto::Type::DeletePlayer: {
        auto msg = proto::unpack<proto::DeletePlayer>(data);
        if (msg.id != playerId) {
          deletePlayer(msg.id);
        }
        break;
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