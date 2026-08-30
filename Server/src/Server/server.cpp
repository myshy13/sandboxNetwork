#include "Server/server.hpp"
#include "Models/Object.hpp"
#include "Protocol/protocol.hpp"
#include "enet/enet.h"
#include "raylib.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <optional>
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
    ENetPacket *packet = enet_packet_create(
        bytes.data(), bytes.size(), reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
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
  enet_address_set_host(&address, "192.168.10.111");
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
constexpr float BULLET_SPEED = 150.0f;

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
  // player->pos is the player's feet, so lift to the head before pushing the
  // spawn forward - otherwise the bullet renders point-blank on the camera.
  constexpr Vector3 HEAD_OFFSET = {0.0f, 10.0f, 0.0f};
  constexpr float MUZZLE_DISTANCE = 3.0f;
  Vector3 headPos = Vector3Add(player->pos, HEAD_OFFSET);
  bullet.pos = Vector3Add(headPos, Vector3Scale(forward, MUZZLE_DISTANCE));
  bullet.vel = Vector3Scale(forward, BULLET_SPEED);

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
  newPlayer.id = id;
  Vector3 spawnPos;
  spawnPos.x = rand() % 200 - 100;
  spawnPos.z = rand() % 200 - 100;
  spawnPos.y = 10;
  newPlayer.pos = spawnPos;
  players.push_back(newPlayer);

  sendTo(id, proto::pack(proto::Type::GivenId, proto::GivenId{id}), true);

  // Catch the newcomer up on blocks placed before they joined.
  for (const Object &o : objects) {
    sendTo(id, proto::pack(proto::Type::NewObject, proto::NewObject{o}), true);
  }
  return id;
}

void Server::handleDisconnect(int playerId) {
  std::printf("Client %d disconnected\n", playerId);
  deletePlayer(playerId);
  connections.erase(playerId);

  broadcast(
      proto::pack(proto::Type::DeletePlayer, proto::DeletePlayer{playerId}),
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
      player->pos = msg.pos;
      player->pitch = msg.pitch;
      player->yaw = msg.yaw;
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
      packetMsg.pos = bullet->pos;
      packetMsg.vel = bullet->vel;
      broadcast(proto::pack(proto::Type::NewBullet, packetMsg), true);
    }
    break;
  }

  case proto::Type::ChatMessage: {
    auto msg = proto::unpack<proto::ChatMessage>(data);
    broadcast(data, true);
    break;
  }

  case proto::Type::SetName: {
    auto msg = proto::unpack<proto::SetName>(data);
    if (auto *p = findPlayer(msg.id)) {
      p->displayName = msg.name;
      broadcast(data, true);
    }
    break;
  }

  case proto::Type::PlaceObject: {
    auto msg = proto::unpack<proto::PlaceObject>(data);
    msg.object.setId(nextObjectId++); // server owns ids, clients send -1
    objects.push_back(msg.object);
    broadcast(proto::pack(proto::Type::NewObject, proto::NewObject{msg.object}),
              true); // reliable
    break;
  }
  case proto::Type::RemoveObject: {
    auto msg = proto::unpack<proto::RemoveObject>(data);
    std::erase_if(objects,
                  [&](const Object &o) { return o.getId() == msg.id; });
    broadcast(data, true);
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
constexpr Vector3 PLAYER_SCALE = {1.5f, 10.0f, 1.5f};

bool SegmentIntersectsBox(Vector3 start, Vector3 end, BoundingBox box) {
  Vector3 dir = Vector3Subtract(end, start);
  float tMin = 0.0f;
  float tMax = 1.0f;

  auto clipAxis = [&](float s, float d, float boxMin, float boxMax) {
    if (fabsf(d) < 1e-6f) {
      return s >= boxMin && s <= boxMax;
    }
    float t1 = (boxMin - s) / d;
    float t2 = (boxMax - s) / d;
    if (t1 > t2) {
      std::swap(t1, t2);
    }
    tMin = std::max(tMin, t1);
    tMax = std::min(tMax, t2);
    return tMin <= tMax;
  };

  return clipAxis(start.x, dir.x, box.min.x, box.max.x) &&
         clipAxis(start.y, dir.y, box.min.y, box.max.y) &&
         clipAxis(start.z, dir.z, box.min.z, box.max.z);
}

void Server::tick(float dt) {
  // ==== hit detection ==== //
  for (auto &b : bullets) {
    b.deathCountdown -= dt;

    Vector3 prevPos = b.pos;
    b.pos = Vector3Add(b.pos, Vector3Scale(b.vel, dt));

    std::erase_if(objects, [&](const Object &object) {
      ObjectTransform t = object.getTransform();
      Vector3 half = Vector3Scale(t.scale, 0.5f);
      BoundingBox box{Vector3Subtract(t.pos, half), Vector3Add(t.pos, half)};
      if (!SegmentIntersectsBox(prevPos, b.pos, box)) {
        return false;
      }
      b.deathCountdown = 0.0f;
      broadcast(proto::pack(proto::Type::RemoveObject,
                            proto::RemoveObject{object.getId()}),
                true);
      return true;
    });

    for (auto &p : players) {
      if (b.deathCountdown <= 0.0f) {
        break; // already spent on a block this tick
      }
      if (p.id == b.playerId) {
        continue; // don't hit the shooter
      }
      BoundingBox player;
      player.min = Vector3Subtract(
          p.pos, {PLAYER_SCALE.x * 0.5f, 0.0f, PLAYER_SCALE.z * 0.5f});
      player.max = Vector3Add(player.min, PLAYER_SCALE);

      if (SegmentIntersectsBox(prevPos, b.pos, player)) {
        b.deathCountdown = 0.0f;
        p.health--;

        if (p.health <= 0) {
          Vector3 spawnPos;
          spawnPos.x = rand() % 200 - 100;
          spawnPos.z = rand() % 200 - 100;
          spawnPos.y = 10;
          p.health = PLAYER_MAX_HEALTH;
          p.pos = spawnPos;
          sendTo(p.id,
                 proto::pack(proto::Type::Respawn, proto::Respawn{spawnPos}),
                 true);
          kills[b.playerId] += 1;
        }

        broadcast(proto::pack(proto::Type::PlayerHit,
                              proto::PlayerHit{p.health, p.id, b.playerId}),
                  true);
        break;
      }
    }

    if (b.pos.y <= 0) {
      b.deathCountdown = 0.0f;
    }

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
      const int id =
          handleConnect(std::make_unique<EnetConnection>(event.peer));
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
