#include "Server/server.hpp"
#include "Protocol/protocol.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>

// ==== connection setup ==== //

Server::Server() {
  if (enet_initialize() != 0) {
    std::fprintf(stderr, "Failed to initialize ENet\n");
    std::exit(EXIT_FAILURE);
  }
  std::printf("ENet initialized\n");

  ENetAddress address;
  address.host = ENET_HOST_ANY;
  address.port = port;

  host = enet_host_create(&address, 32, 2, 0, 0);
  if (host == nullptr) {
    std::fprintf(stderr, "Failed to create ENet server host\n");
    std::exit(EXIT_FAILURE);
  }
}

// ==== player bookkeeping ==== //

void Server::deletePlayer(int id) {
  auto it = std::find_if(players.begin(), players.end(),
                          [id](const Player &p) { return p.id == id; });
  if (it != players.end()) {
    players.erase(it);
  }
}

// ==== event handlers ==== //

void Server::handleConnect(ENetEvent &event) {
  std::printf("Client connected\n");

  auto bytes = proto::pack(proto::Type::GivenId, proto::GivenId{nextClientId});
  ENetPacket *outPacket =
      enet_packet_create(bytes.data(), bytes.size(), ENET_PACKET_FLAG_RELIABLE);
  enet_peer_send(event.peer, 0, outPacket);

  Player newPlayer;
  newPlayer.id  = nextClientId;
  newPlayer.pos = {0, 0, 0};
  players.push_back(newPlayer);

  event.peer->data = reinterpret_cast<void *>(static_cast<intptr_t>(nextClientId));
  nextClientId++;
}

void Server::handleDisconnect(ENetEvent &event) {
  int id = static_cast<int>(reinterpret_cast<intptr_t>(event.peer->data));
  std::printf("Client %d disconnected\n", id);
  deletePlayer(id);

  auto bytes = proto::pack(proto::Type::DeletePlayer, proto::DeletePlayer{id});
  ENetPacket *packet = enet_packet_create(bytes.data(), bytes.size(), 0);
  enet_host_broadcast(host, 0, packet);
}

void Server::handleReceive(ENetEvent &event) {
  std::string data(reinterpret_cast<char *>(event.packet->data), event.packet->dataLength);

  // ==== pos update handler ==== //
  if (!data.empty() && proto::peekType(data) == proto::Type::PlayerUpdate) {
    auto msg = proto::unpack<proto::PlayerUpdate>(data);

    Player *player = findPlayer(msg.id);
    if (player != nullptr) {
      player->pos = msg.pos;
    }
    // ==== notify all peers ==== //
    auto bytes = proto::pack(proto::Type::PlayerUpdate, msg);
    ENetPacket *packet = enet_packet_create(bytes.data(), bytes.size(), 0);
    enet_host_broadcast(host, 0, packet);
  } else {
    std::printf("Invalid request\n");
  }

  enet_packet_destroy(event.packet);
}

// ==== main loop ==== //

void Server::poll() {
  ENetEvent event;
  if (enet_host_service(host, &event, 1000) > 0) {
    switch (event.type) {
    case ENET_EVENT_TYPE_CONNECT:
      handleConnect(event);
      break;
    case ENET_EVENT_TYPE_DISCONNECT:
      handleDisconnect(event);
      break;
    case ENET_EVENT_TYPE_RECEIVE:
      handleReceive(event);
      break;
    case ENET_EVENT_TYPE_NONE:
      break;
    }
  }
}
