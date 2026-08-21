#include "Server/server.hpp"
#include "Protocol/protocol.hpp"
#include "enet/enet.h"
#include "raylib.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <raymath.h>
#include <string>

namespace {

// An ENet client. The WebSocket equivalent lives in Net/ws_proxy.cpp.
class EnetConnection final : public Connection {
public:
  explicit EnetConnection(ENetPeer *peer) : peer(peer) {}

  void send(const std::string &bytes, bool reliable) override {
    if (peer == nullptr) {
      return;
    }
    ENetPacket *packet =
        enet_packet_create(bytes.data(), bytes.size(),
                           reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
    enet_peer_send(peer, 0, packet);
  }

private:
  ENetPeer *peer;
};

} // namespace

// ==== connection setup ==== //

Server::Server(int wsPort) {
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

  if (wsPort > 0) {
    wsProxy = std::make_unique<WsProxy>(wsPort);
    if (!wsProxy->isListening()) {
      wsProxy.reset(); // it already explained itself on stderr
    }
  }
}

Server::~Server() {
  connections.clear(); // before the proxy and host they point into
  if (host != nullptr) {
    enet_host_destroy(host);
  }
  enet_deinitialize();
}

// ==== sending ==== //

void Server::sendTo(int playerId, const std::string &bytes, bool reliable) {
  auto it = connections.find(playerId);
  if (it != connections.end()) {
    it->second->send(bytes, reliable);
  }
}

void Server::broadcast(const std::string &bytes, bool reliable) {
  for (auto &entry : connections) {
    entry.second->send(bytes, reliable);
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

  Quaternion aim  = QuaternionFromEuler(player->pitch, player->yaw, 0.0f);
  Vector3 forward = Vector3RotateByQuaternion({0.0f, 0.0f, -1.0f}, aim);

  Bullet bullet;
  bullet.bulletId = nextBulletId;
  nextBulletId++;
  bullet.playerId = player->id;
  // player->pos is the player's feet, so lift to the head before pushing the
  // spawn forward - otherwise the bullet renders point-blank on the camera.
  constexpr Vector3 HEAD_OFFSET   = {0.0f, 10.0f, 0.0f};
  constexpr float MUZZLE_DISTANCE = 3.0f;
  Vector3 headPos = Vector3Add(player->pos, HEAD_OFFSET);
  bullet.pos      = Vector3Add(headPos, Vector3Scale(forward, MUZZLE_DISTANCE));
  bullet.vel      = Vector3Scale(forward, BULLET_SPEED);

  bullets.push_back(bullet);
  return bullet;
};

// ==== event handlers ==== //

int Server::handleConnect(std::unique_ptr<Connection> connection) {
  const int id = nextClientId;
  nextClientId++;
  std::printf("Client %d connected\n", id);

  connections[id] = std::move(connection);

  Player newPlayer;
  newPlayer.id  = id;
  newPlayer.pos = {0, 0, 0};
  players.push_back(newPlayer);

  sendTo(id, proto::pack(proto::Type::GivenId, proto::GivenId{id}), true);
  return id;
}

void Server::handleDisconnect(int playerId) {
  std::printf("Client %d disconnected\n", playerId);
  deletePlayer(playerId);
  connections.erase(playerId);

  broadcast(proto::pack(proto::Type::DeletePlayer, proto::DeletePlayer{playerId}),
            true);
}

void Server::handleReceive(int playerId, const std::string &data) {
  if (data.empty()) {
    return;
  }

  switch (proto::peekType(data)) {
  // ==== pos update handler ==== //
  case proto::Type::PlayerUpdate: {
    auto msg = proto::unpack<proto::PlayerUpdate>(data);
    // Trust the connection, not the payload - a client only speaks for itself.
    msg.id = playerId;

    Player *player = findPlayer(playerId);
    if (player != nullptr) {
      player->pos   = msg.pos;
      player->pitch = msg.pitch;
      player->yaw   = msg.yaw;
    }
    // ==== notify all peers ==== //
    // Unreliable on purpose: a dropped position is superseded a frame later.
    broadcast(proto::pack(proto::Type::PlayerUpdate, msg), false);
    break;
  }

  case proto::Type::CreateBullet: {
    std::optional<Bullet> bullet = createBullet(playerId);
    if (bullet.has_value()) {
      proto::NewBullet packetMsg;
      packetMsg.bulletId = bullet->bulletId;
      packetMsg.playerId = bullet->playerId;
      packetMsg.pos      = bullet->pos;
      packetMsg.vel      = bullet->vel;
      broadcast(proto::pack(proto::Type::NewBullet, packetMsg), true);
    }
    break;
  }

  default:
    std::printf("Invalid request\n");
    break;
  }
}

// ==== fixed-rate tick (bullet lifetime etc.) ==== //

constexpr float TICK_RATE = 1.0f / 60.0f;

// same as the client's default Player scale (Client/src/Player/player.cpp)
constexpr Vector3 PLAYER_SCALE       = {0.7f, 5.0f, 0.7f};
constexpr Vector3 BULLET_HALF_EXTENT = {0.7f, 0.7f, 0.7f};

void Server::tick(float dt) {
  // ==== hit detection ==== //
  for (auto &b : bullets) {
    b.deathCountdown -= dt;

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
      playerBox.max = {p.pos.x + PLAYER_SCALE.x * 0.5f, p.pos.y + PLAYER_SCALE.y,
                       p.pos.z + PLAYER_SCALE.z * 0.5f};

      if (CheckCollisionBoxes(bulletBox, playerBox)) {
        std::printf("[hit] bullet %d (from player %d) hit player %d\n",
                    b.bulletId, b.playerId, p.id);
        b.deathCountdown = 0.0f; // TODO: broadcast a hit/damage message
      }
    }
  }

  for (const auto &b : bullets) {
    if (b.deathCountdown <= 0.0f) {
      broadcast(proto::pack(proto::Type::DeleteBullet,
                            proto::DeleteBullet{b.bulletId}),
                true);
    }
  }

  bullets.erase(
      std::remove_if(bullets.begin(), bullets.end(),
                     [](const Bullet &b) { return b.deathCountdown <= 0.0f; }),
      bullets.end());
}

// ==== transport plumbing ==== //

void Server::pumpEnet() {
  ENetEvent event;
  // Doubles as the loop's pacing: blocks up to ~1 frame waiting for traffic.
  if (enet_host_service(host, &event, 16) > 0) {
    switch (event.type) {
    case ENET_EVENT_TYPE_CONNECT: {
      const int id = handleConnect(std::make_unique<EnetConnection>(event.peer));
      event.peer->data = reinterpret_cast<void *>(static_cast<intptr_t>(id));
      break;
    }

    case ENET_EVENT_TYPE_DISCONNECT:
      handleDisconnect(
          static_cast<int>(reinterpret_cast<intptr_t>(event.peer->data)));
      break;

    case ENET_EVENT_TYPE_RECEIVE: {
      const int id =
          static_cast<int>(reinterpret_cast<intptr_t>(event.peer->data));
      std::string data(reinterpret_cast<char *>(event.packet->data),
                       event.packet->dataLength);
      enet_packet_destroy(event.packet);
      handleReceive(id, data);
      break;
    }

    case ENET_EVENT_TYPE_NONE:
      break;
    }
  }
}

void Server::pumpWebSockets() {
  if (!wsProxy) {
    return;
  }

  for (auto &event : wsProxy->drain()) {
    switch (event.kind) {
    case WsEvent::Kind::Connect:
      wsPlayerIds[event.socketId] = handleConnect(std::move(event.connection));
      break;

    case WsEvent::Kind::Message: {
      auto it = wsPlayerIds.find(event.socketId);
      if (it != wsPlayerIds.end()) {
        handleReceive(it->second, event.data);
      }
      break;
    }

    case WsEvent::Kind::Disconnect: {
      auto it = wsPlayerIds.find(event.socketId);
      if (it != wsPlayerIds.end()) {
        handleDisconnect(it->second);
        wsPlayerIds.erase(it);
      }
      break;
    }
    }
  }
}

// ==== main loop ==== //

void Server::poll() {
  static auto lastTick = std::chrono::steady_clock::now();

  pumpEnet();
  pumpWebSockets();

  auto now = std::chrono::steady_clock::now();
  float dt = std::chrono::duration<float>(now - lastTick).count();
  if (dt >= TICK_RATE) {
    tick(dt);
    lastTick = now;
  }
}
