#include "client.hpp"
#include "GameState/gameState.hpp"
#include "Models/Object.hpp"
#include "Protocol/protocol.hpp"

#include "structs.hpp"
#include <cmath>
#include <cstddef>
#include <raylib.h>
#include <raymath.h>
#include <string>

// ==== outgoing messages ==== //
void Client::sendPlayerPosition(const Transform &transform, float pitch, float yaw) {
  if (playerId == -1)
    return;
#ifdef CHEATS
  pitch = -0.02f;
#endif
  auto bytes = proto::pack(proto::Type::PlayerUpdate, proto::PlayerUpdate{playerId, transform.translation, pitch, yaw});
  transport->send(bytes, false);
};

void Client::createBullet(const Camera3D &camera) {
  if (playerId == -1)
    return;
  Vector3 dir = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
  auto bytes  = proto::pack(proto::Type::CreateBullet,
                            proto::CreateBullet{camera.position, dir});
  transport->send(bytes, true);
}

// ==== connection setup ==== //
bool Client::connect() {
  if (connecting && !isConnected() && (kickReason || secondsSinceConnect() < CONNECT_TIMEOUT))
    return false;

  std::cout << "Joining server: " << env::SERVER_IP << " at port " << port << "\n";
  disconnect();
  transport = makeTransport();
  transport->connect(env::SERVER_IP, port);

  // ==== forget the last session ==== //
  players.clear();
  bullets.clear();
  chat.clear();
  kills.clear();
  kickReason.reset();
  playerName.reset();
  respawnTo.reset();
  pendingObjects.clear();
  pendingInitChunks.clear();
  pendingRemovals.clear();
  pendingDamage.clear();
  health           = env::MAX_HEALTH;
  playerId         = -1;
  handshakeSent    = false;
  connectStartedAt = GetTime();
  connecting       = true;
  waiting          = true;
  blocksReceived   = 0;
  lastChunkAt      = 0.0;
  blocksReceived   = 0;
  lastChunkAt      = 0.0;
  return true;
}

// ==== incoming message handling ==== //
void Client::poll() {
  if (!transport)
    return;
  while (auto data = transport->receive()) {
    if (!data->empty()) {
      handleMessage(*data);
    }
  }
  if (!handshakeSent && transport->isConnected()) {
    transport->send(proto::pack(proto::Type::clientHandshake,
                                proto::clientHandshake{proto::PROTOCOL_VERSION}),
                    true);
    handshakeSent = true;
    std::cout << "Connected to server\n";
  }
}

void Client::handleMessage(const std::string &data) {
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
        newPlayer.id          = msg.id;
        newPlayer.pos         = msg.pos;
        newPlayer.pitch       = msg.pitch;
        newPlayer.yaw         = msg.yaw;
        newPlayer.last2pos[0] = {msg.pos};
        newPlayer.last2pos[1] = {msg.pos};
        newPlayer.last2yaw[0] = msg.yaw;
        newPlayer.last2yaw[1] = msg.yaw;
        newPlayer.updatedAt   = GetTime();
        newPlayer.glideTime   = POS_UPDATE_INTERVAL;
        players.push_back(newPlayer);
      } else {
        const double now = GetTime();
        // glide for as long as the last gap between updates, so a dropped packet stretches it instead of stalling
        player->glideTime = std::clamp(now - player->updatedAt, MIN_GLIDE_TIME, MAX_GLIDE_TIME);
        player->updatedAt = now;
        player->pitch     = msg.pitch;
        // large jump, skip interpolation
        if (Vector3Distance(msg.pos, player->last2pos[1]) > SNAP_DISTANCE) {
          player->last2pos[0] = msg.pos;
          player->last2pos[1] = msg.pos;
          player->last2yaw[0] = msg.yaw;
          player->last2yaw[1] = msg.yaw;
          player->pos         = msg.pos;
          player->yaw         = msg.yaw;
        } else {
          player->last2pos[0] = player->pos;
          player->last2pos[1] = msg.pos;
          player->last2yaw[0] = player->yaw;
          player->last2yaw[1] = msg.yaw;
        }
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
  case proto::Type::NewBullet: {
    auto msg = proto::unpack<proto::NewBullet>(data);
    Bullet b;
    b.playerId = msg.playerId;
    b.vel      = msg.vel;
    b.pos      = msg.pos;
    b.bulletId = msg.bulletId;
    bullets.push_back(b);
    break;
  }
  case proto::Type::DeleteBullet: {
    auto msg = proto::unpack<proto::DeleteBullet>(data);
    std::erase_if(bullets, [msg](Bullet b) {
      return b.bulletId == msg.id;
    });
    break;
  }

  case proto::Type::PlayerHit: {
    auto msg = proto::unpack<proto::PlayerHit>(data);
    if (msg.id == playerId) {
      health = msg.health;
      GameState::shared().TriggerDamageFlash();
    } else if (msg.shooterId == playerId) {
      GameState::shared().TriggerGreenFlash();
    }
    if (msg.health <= 0) {
      ChatEntry deathMessage;
      deathMessage.id         = -1;
      deathMessage.receivedAt = GetTime();
      deathMessage.text       = "Player " + std::to_string(msg.shooterId) + " killed Player " + std::to_string(msg.id);
      chat.push_back(deathMessage);
      kills[msg.shooterId] += 1;
    }
    break;
  }
  case proto::Type::Respawn: {
    respawnTo = proto::unpack<proto::Respawn>(data).pos;
    break;
  }
  case proto::Type::ChatMessage: {
    auto msg = proto::unpack<proto::ChatMessage>(data);
    ChatEntry entry;
    entry.id         = msg.id;
    entry.text       = msg.text;
    entry.receivedAt = GetTime();
    chat.push_back(entry);
    break;
  }
  case proto::Type::SetName: {
    proto::SetName msg = proto::unpack<proto::SetName>(data);
    if (msg.id == playerId) {
      playerName = msg.name;
      return;
    }
    OnlinePlayer *p = findPlayer(msg.id);
    if (p) {
      p->name = msg.name;
    }
    break;
  }
  // Server assigns the id and echoes NewObject to everyone (including us).
  case proto::Type::NewObject: {
    pendingObjects.push_back(proto::unpack<proto::NewObject>(data).object);
    break;
  }
  case proto::Type::RemoveObject: {
    pendingRemovals.push_back(proto::unpack<proto::RemoveObject>(data).pos);
    break;
  }
  case proto::Type::DamageObject: {
    pendingDamage.push_back(proto::unpack<proto::DamageObject>(data).pos);
    break;
  }
  case proto::Type::kick: {
    auto msg = proto::unpack<proto::kick>(data);
    if (msg.playerId == playerId) {
      kickReason = msg.reason;
      transport->disconnect(); // not disconnect(): that would forget the reason we were kicked
    }
    break;
  }
  case proto::Type::initBlocks: {
    auto msg = proto::unpack<proto::initBlocks>(data);
    blocksReceived += msg.objects.size(); // count before the move empties it
    lastChunkAt = GetTime();
    pendingInitChunks.push_back(std::move(msg.objects));
    waiting = false;
    break;
  }
  default:
    break;
  }
}

void Client::sendChatMessage(const std::string &msg) {
  if (playerId == -1)
    return;
  auto bytes = proto::pack(proto::Type::ChatMessage, proto::ChatMessage{msg, playerId});
  transport->send(bytes, true);
}

void Client::setName(const std::string &newname) {
  if (playerId == -1)
    return;
  auto bytes = proto::pack(proto::Type::SetName, proto::SetName{newname, playerId});
  transport->send(bytes, true);
}

void Client::placeObject(const Object &object) {
  if (playerId == -1)
    return;
  auto bytes = proto::pack(proto::Type::PlaceObject, proto::PlaceObject{object});
  transport->send(bytes, true);
};

void Client::updatePlayers() {
  for (OnlinePlayer &p : players) {
    float alpha = Clamp(static_cast<float>((GetTime() - p.updatedAt) / p.glideTime), 0.0f, 1.0f);
    if (!GameState::shared().getInterpolation()) {
      alpha = 1.0f;
    }
    p.pos = Vector3Lerp(p.last2pos[0], p.last2pos[1], alpha);
    // remainder() wraps the difference into -PI..PI, so yaw turns the short way round
    p.yaw = p.last2yaw[0] + std::remainder(p.last2yaw[1] - p.last2yaw[0], 2.0f * PI) * alpha;
  }
};