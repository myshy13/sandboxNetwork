#include "client.hpp"
#include "GameState/gameState.hpp"
#include "Models/Object.hpp"
#include "Protocol/protocol.hpp"

#include "structs.hpp"
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
  // Send the exact ray the camera has this frame. The server can't derive it
  // itself: our PlayerUpdate aim is unreliable and a few frames stale, so
  // reconstructing the ray there missed slightly - worse the further the target.
  Vector3 dir = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
  auto bytes = proto::pack(proto::Type::CreateBullet,
                           proto::CreateBullet{camera.position, dir});
  transport->send(bytes, true);
}

// ==== connection setup ==== //
Client::Client() {
  transport = makeTransport();
  transport->connect(SERVER_IP, port);
}

// ==== incoming message handling ==== //
void Client::poll() {
  while (auto data = transport->receive()) {
    if (!data->empty()) {
      handleMessage(*data);
    }
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
        newPlayer.id    = msg.id;
        newPlayer.pos   = msg.pos;
        newPlayer.pitch = msg.pitch;
        newPlayer.yaw   = msg.yaw;
        players.push_back(newPlayer);
      } else {
        player->pos   = msg.pos;
        player->pitch = msg.pitch;
        player->yaw   = msg.yaw;
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
    OnlinePlayer *p    = findPlayer(msg.id);
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
    pendingRemovals.push_back(proto::unpack<proto::RemoveObject>(data).id);
    break;
  }
  case proto::Type::DamageObject: {
    pendingDamage.push_back(proto::unpack<proto::DamageObject>(data).id);
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