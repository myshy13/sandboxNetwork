#include "client.hpp"

#include <raylib.h>
#include <raymath.h>

#include <cmath>
#include <cstddef>
#include <string>
#include <utility>

#include "GameState/gameState.hpp"
#include "Models/Object.hpp"
#include "Protocol/protocol.hpp"
#include "env.hpp"
#include "structs.hpp"

// ==== outgoing messages ==== //
void Client::sendPlayerPosition(const Transform& transform, float pitch,
                                float yaw) {
  if (playerId == -1) return;
#ifdef CHEATS
  pitch = -0.02f;
#endif
  auto bytes = proto::pack(
      proto::Type::PlayerUpdate,
      proto::PlayerUpdate{playerId, transform.translation, pitch, yaw});
  transport->send(bytes, false);
};

void Client::createBullet(Vector3 origin, Vector3 dir) {
  if (playerId == -1) return;
  auto bytes =
      proto::pack(proto::Type::CreateBullet, proto::CreateBullet{origin, dir});
  transport->send(bytes, true);
}

// ==== connection setup ==== //
bool Client::connect() {
  if (connecting && !isConnected() &&
      (kickReason || secondsSinceConnect() < CONNECT_TIMEOUT))
    return false;

  std::cout << "Joining server: " << env::SERVER_IP << " at port " << port
            << "\n";
  disconnect();
  transport = makeTransport();
  transport->connect(env::SERVER_IP, port);

  // ==== forget the last session ==== //
  players.clear();
  bullets.clear();
  chat.clear();
  kills.clear();
  pendingNames.clear();
  kickReason.reset();
  playerName.reset();
  respawnTo.reset();
  pendingWorldEvents.clear();
  health = env::MAX_HEALTH;
  playerId = -1;
  handshakeSent = false;
  connectStartedAt = GetTime();
  connecting = true;
  return true;
}

bool Client::aimingAtPlayer(Ray facing) {
  for (OnlinePlayer& p : players) {
    RayCollision collision = GetRayCollisionBox(
        facing, {p.pos, Vector3Add(p.pos, env::PLAYER_SCALE)});
    if (collision.hit &&
        collision.distance < 200 * env::BLOCKSIZE.x) {  // 200 blocks
      return true;
    }
  }
  return false;
}

// ==== incoming message handling ==== //
void Client::poll() {
  if (!transport) return;
  while (auto data = transport->receive()) {
    if (!data->empty()) {
      handleMessage(*data);
    }
  }
  if (!handshakeSent && transport->isConnected()) {
    transport->send(
        proto::pack(proto::Type::clientHandshake,
                    proto::clientHandshake{proto::PROTOCOL_VERSION}),
        true);
    handshakeSent = true;
    std::cout << "Connected to server\n";
  }
}

void Client::handleMessage(const std::string& data) {
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
        OnlinePlayer* player = findPlayer(msg.id);
        if (player == nullptr) {
          OnlinePlayer newPlayer;
          newPlayer.id = msg.id;
          newPlayer.pos = msg.pos;
          newPlayer.pitch = msg.pitch;
          newPlayer.yaw = msg.yaw;
          newPlayer.last2pos[0] = {msg.pos};
          newPlayer.last2pos[1] = {msg.pos};
          newPlayer.last2yaw[0] = msg.yaw;
          newPlayer.last2yaw[1] = msg.yaw;
          newPlayer.updatedAt = GetTime();
          newPlayer.glideTime = POS_UPDATE_INTERVAL;
          if (auto name = pendingNames.find(msg.id);
              name != pendingNames.end()) {
            newPlayer.name = name->second;
            pendingNames.erase(name);
          }
          players.push_back(newPlayer);
        } else {
          const double now = GetTime();
          // glide for as long as the last gap between updates, so a dropped
          // packet stretches it instead of stalling
          player->glideTime = std::clamp(now - player->updatedAt,
                                         MIN_GLIDE_TIME, MAX_GLIDE_TIME);
          player->updatedAt = now;
          player->pitch = msg.pitch;
          // large jump, skip interpolation
          if (Vector3Distance(msg.pos, player->last2pos[1]) > SNAP_DISTANCE) {
            player->last2pos[0] = msg.pos;
            player->last2pos[1] = msg.pos;
            player->last2yaw[0] = msg.yaw;
            player->last2yaw[1] = msg.yaw;
            player->pos = msg.pos;
            player->yaw = msg.yaw;
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
        pendingNames.erase(msg.id);
      }
      break;
    }
    case proto::Type::NewBullet: {
      auto msg = proto::unpack<proto::NewBullet>(data);
      Bullet b;
      b.playerId = msg.playerId;
      b.vel = msg.vel;
      b.pos = msg.pos;
      b.bulletId = msg.bulletId;
      bullets.push_back(b);
      break;
    }
    case proto::Type::DeleteBullet: {
      auto msg = proto::unpack<proto::DeleteBullet>(data);
      std::erase_if(bullets, [msg](Bullet b) { return b.bulletId == msg.id; });
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
        deathMessage.id = -1;
        deathMessage.receivedAt = GetTime();
        deathMessage.text = "Player " + std::to_string(msg.shooterId) +
                            " killed Player " + std::to_string(msg.id);
        chat.push_back(deathMessage);
        kills[msg.shooterId] += 1;
      }
      break;
    }
    case proto::Type::Respawn: {
      respawnTo = proto::unpack<proto::Respawn>(data).pos;
      health = env::MAX_HEALTH;  // the killing PlayerHit left it at 0
      break;
    }
    case proto::Type::ChatMessage: {
      auto msg = proto::unpack<proto::ChatMessage>(data);
      ChatEntry entry;
      entry.id = msg.id;
      entry.text = msg.text;
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
      OnlinePlayer* p = findPlayer(msg.id);
      if (p) {
        p->name = msg.name;
      } else {
        pendingNames[msg.id] =
            msg.name;  // applied when their first PlayerUpdate creates them
      }
      break;
    }
    // Server assigns the id and echoes NewObject to everyone (including us).
    case proto::Type::NewObject: {
      WorldEvent e{WorldEvent::Kind::Add};
      e.object = proto::unpack<proto::NewObject>(data).object;
      pendingWorldEvents.push_back(std::move(e));
      break;
    }
    case proto::Type::RemoveObject: {
      WorldEvent e{WorldEvent::Kind::Remove};
      e.pos = proto::unpack<proto::RemoveObject>(data).pos;
      pendingWorldEvents.push_back(std::move(e));
      break;
    }
    case proto::Type::DamageObject: {
      WorldEvent e{WorldEvent::Kind::Damage};
      e.pos = proto::unpack<proto::DamageObject>(data).pos;
      pendingWorldEvents.push_back(std::move(e));
      break;
    }
    case proto::Type::UpdateWaterLevel: {
      auto msg = proto::unpack<proto::UpdateWaterLevel>(data);
      WorldEvent e{WorldEvent::Kind::WaterLevel};
      e.pos = msg.pos;
      e.level = msg.level;
      pendingWorldEvents.push_back(std::move(e));
      break;
    }
    case proto::Type::kick: {
      auto msg = proto::unpack<proto::kick>(data);
      if (msg.playerId == playerId) {
        kickReason = msg.reason;
        transport->disconnect();  // not disconnect(): that would forget the
                                  // reason we were kicked
      }
      break;
    }
    case proto::Type::ChunkData: {
      auto msg = proto::unpack<proto::ChunkData>(data);
      WorldEvent e{WorldEvent::Kind::LoadChunk};
      e.cx = msg.cx;
      e.cz = msg.cz;
      e.blocks = std::move(msg.blocks);
      pendingWorldEvents.push_back(std::move(e));
      break;
    }
    case proto::Type::ChunkUnload: {
      auto msg = proto::unpack<proto::ChunkUnload>(data);
      WorldEvent e{WorldEvent::Kind::UnloadChunk};
      e.cx = msg.cx;
      e.cz = msg.cz;
      pendingWorldEvents.push_back(std::move(e));
      break;
    }
    default:
      break;
  }
}

void Client::sendChatMessage(const std::string& msg) {
  if (playerId == -1) return;
  auto bytes =
      proto::pack(proto::Type::ChatMessage, proto::ChatMessage{msg, playerId});
  transport->send(bytes, true);
}

void Client::setName(const std::string& newname) {
  if (playerId == -1) return;
  auto bytes =
      proto::pack(proto::Type::SetName, proto::SetName{newname, playerId});
  transport->send(bytes, true);
}

bool Client::sendViewRadius(int chunks) {
  if (playerId == -1) return false;
  auto bytes =
      proto::pack(proto::Type::SetViewRadius, proto::SetViewRadius{chunks});
  transport->send(bytes, true);
  return true;
}

void Client::placeObject(const Object& object) {
  if (playerId == -1) return;
  auto bytes =
      proto::pack(proto::Type::PlaceObject, proto::PlaceObject{object});
  transport->send(bytes, true);
};

void Client::updatePlayers() {
  for (OnlinePlayer& p : players) {
    float alpha =
        Clamp(static_cast<float>((GetTime() - p.updatedAt) / p.glideTime), 0.0f,
              1.0f);
    if (!GameState::shared().getInterpolation()) {
      alpha = 1.0f;
    }
    p.pos = Vector3Lerp(p.last2pos[0], p.last2pos[1], alpha);
    // remainder() wraps the difference into -PI..PI, so yaw turns the short way
    // round
    p.yaw = p.last2yaw[0] +
            std::remainder(p.last2yaw[1] - p.last2yaw[0], 2.0f * PI) * alpha;
  }
};