#include "Server/server.hpp"
#include "Protocol/protocol.hpp"
#include "enet/enet.h"
#include "raylib.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <raymath.h>
#include <string>

// ==== connection setup ==== //

Server::Server() {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
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

// ==== Bullet handling ==== //
constexpr float BULLET_SPEED = 100.0f;

std::optional<Bullet> Server::createBullet(int playerId) {
  Player *player = findPlayer(playerId);
  if (player == nullptr) {
    return std::nullopt;
  }

  Quaternion aim = QuaternionFromEuler(player->pitch, player->yaw, 0.0f);
  Vector3 forward = Vector3RotateByQuaternion({0.0f, 0.0f, -1.0f}, aim);

  Bullet bullet;
  bullet.bulletId = nextBulletId;
  nextBulletId++;
  bullet.playerId = player->id;
  // player->pos is the client's eye/camera position (see sendPlayerPosition),
  // so this is already head height - HEAD_OFFSET nudges it, MUZZLE_DISTANCE
  // then pushes it forward so it doesn't spawn point-blank on the camera.
  constexpr Vector3 HEAD_OFFSET = {0.0f, 10.0f, 0.0f};
  constexpr float MUZZLE_DISTANCE = 3.0f;
  Vector3 headPos = Vector3Add(player->pos, HEAD_OFFSET);
  bullet.pos = Vector3Add(headPos, Vector3Scale(forward, MUZZLE_DISTANCE));
  bullet.vel = Vector3Scale(forward, BULLET_SPEED);

  bullets.push_back(bullet);
  return bullet;
};

// ==== event handlers ==== //

void Server::handleConnect(ENetEvent &event) {
  std::printf("Client connected\n");

  auto bytes = proto::pack(proto::Type::GivenId, proto::GivenId{nextClientId});
  ENetPacket *outPacket =
      enet_packet_create(bytes.data(), bytes.size(), ENET_PACKET_FLAG_RELIABLE);
  enet_peer_send(event.peer, 0, outPacket);

  Player newPlayer;
  newPlayer.id = nextClientId;
  newPlayer.pos = {0, 0, 0};
  players.push_back(newPlayer);

  event.peer->data =
      reinterpret_cast<void *>(static_cast<intptr_t>(nextClientId));
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
  std::string data(reinterpret_cast<char *>(event.packet->data),
                   event.packet->dataLength);

  // ==== pos update handler ==== //
  if (!data.empty() && proto::peekType(data) == proto::Type::PlayerUpdate) {
    auto msg = proto::unpack<proto::PlayerUpdate>(data);

    Player *player = findPlayer(msg.id);
    if (player != nullptr) {
      player->pos = msg.pos;
      player->pitch = msg.pitch;
      player->yaw = msg.yaw;
    }
    // ==== notify all peers ==== //
    auto bytes = proto::pack(proto::Type::PlayerUpdate, msg);
    ENetPacket *packet = enet_packet_create(bytes.data(), bytes.size(), 0);
    enet_host_broadcast(host, 0, packet);
  } else if (!data.empty() &&
             proto::peekType(data) == proto::Type::CreateBullet) {
    auto msg = proto::unpack<proto::CreateBullet>(data);
    std::optional<Bullet> bullet = createBullet(msg.playerId);
    if (bullet.has_value()) {
      proto::NewBullet packetMsg;
      packetMsg.bulletId = bullet->bulletId;
      packetMsg.playerId = bullet->playerId;
      packetMsg.pos = bullet->pos;
      packetMsg.vel = bullet->vel;
      auto bytes = proto::pack(proto::Type::NewBullet, packetMsg);
      ENetPacket *packet = enet_packet_create(bytes.data(), bytes.size(), 0);
      enet_host_broadcast(host, 0, packet);
    }
  } else {
    std::printf("Invalid request\n");
  }

  enet_packet_destroy(event.packet);
}

// ==== fixed-rate tick (bullet lifetime etc.) ==== //

constexpr float TICK_RATE = 1.0f / 60.0f;

// same as the client's default Player scale (Client/src/Player/player.cpp)
constexpr Vector3 PLAYER_SCALE = {0.7f, 5.0f, 0.7f};
constexpr Vector3 BULLET_HALF_EXTENT = {0.7f, 0.7f, 0.7f};

void Server::tick(float dt) {
  for (auto &b : bullets) {
    b.deathCountdown -= dt;
  }

  // ==== hit detection ==== //
  for (auto &b : bullets) {
    BoundingBox bulletBox;
    bulletBox.min = Vector3Subtract(b.pos, BULLET_HALF_EXTENT);
    bulletBox.max = Vector3Add(b.pos, BULLET_HALF_EXTENT);

    for (const auto &p : players) {
      if (p.id == b.playerId) {
        continue; // don't hit the shooter
      }

      // p.pos is the player's feet/base position, so the box extends
      // upward from there rather than being centered on it.
      BoundingBox playerBox;
      playerBox.min = {p.pos.x - PLAYER_SCALE.x * 0.5f, p.pos.y,
                       p.pos.z - PLAYER_SCALE.z * 0.5f};
      playerBox.max = {p.pos.x + PLAYER_SCALE.x * 0.5f,
                       p.pos.y + PLAYER_SCALE.y,
                       p.pos.z + PLAYER_SCALE.z * 0.5f};

      if (CheckCollisionBoxes(bulletBox, playerBox)) {
        std::printf("[hit] bullet %d (from player %d) hit player %d\n",
                    b.bulletId, b.playerId, p.id);
        std::cout << "AAAAH!\n";
        b.deathCountdown = 0.0f; // TODO: broadcast a hit/damage message
      }
    }
  }

  for (const auto &b : bullets) {
    if (b.deathCountdown <= 0.0f) {
      auto bytes = proto::pack(proto::Type::DeleteBullet,
                               proto::DeleteBullet{b.bulletId});
      ENetPacket *packet = enet_packet_create(bytes.data(), bytes.size(), 0);
      enet_host_broadcast(host, 0, packet);
    }
  }

  bullets.erase(
      std::remove_if(bullets.begin(), bullets.end(),
                     [](const Bullet &b) { return b.deathCountdown <= 0.0f; }),
      bullets.end());
}

// ==== main loop ==== //

void Server::poll() {
  static auto lastTick = std::chrono::steady_clock::now();

  ENetEvent event;
  if (enet_host_service(host, &event, 16) > 0) {
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

  auto now = std::chrono::steady_clock::now();
  float dt = std::chrono::duration<float>(now - lastTick).count();
  if (dt >= TICK_RATE) {
    tick(dt);
    lastTick = now;
  }
}
