#pragma once
#include "Models/Object.hpp"
#include "raylib.h"
#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

template <class Archive> void serialize(Archive &ar, Vector3 &v) {
  ar(v.x, v.y, v.z);
}

namespace proto {

constexpr int PROTOCOL_VERSION = 5; // removed initBlocks

enum class Type : uint8_t {
  // player
  PlayerUpdate,
  DeletePlayer,
  PlayerHit,
  // bullet
  CreateBullet,
  NewBullet,
  DeleteBullet,
  Respawn,
  // handshakes and chat
  ChatMessage,
  PlaceObject,
  GivenId,
  clientHandshake,
  SetName,
  kick,
  // kickPlayer
  // world
  NewObject,
  RemoveObject,
  DamageObject,

  ChunkData,
  ChunkUnload,
  SetViewRadius
};

struct PlayerUpdate {
  int id;
  Vector3 pos;
  float pitch;
  float yaw;
  template <class A> void serialize(A &ar) { ar(id, pos, pitch, yaw); }
};
struct GivenId {
  int id;
  template <class A> void serialize(A &ar) { ar(id); }
};
struct DeletePlayer {
  int id;
  template <class A> void serialize(A &ar) { ar(id); }
};
struct CreateBullet { // clients telling server
  // Aim ray captured client-side at click time; server can't rederive it since
  // PlayerUpdate aim is unreliable and a few frames stale.
  Vector3 origin;
  Vector3 dir;
  template <class A> void serialize(A &ar) { ar(origin, dir); }
};
struct DeleteBullet { // clients telling server
  int id;
  template <class A> void serialize(A &ar) { ar(id); }
};
struct NewBullet { // server telling clients
  int playerId;
  int bulletId;
  Vector3 pos;
  Vector3 vel;
  template <class A> void serialize(A &ar) { ar(playerId, bulletId, pos, vel); }
};
struct PlayerHit {
  int health;
  int id;
  int shooterId;
  template <class A> void serialize(A &ar) { ar(health, id, shooterId); }
};
struct Respawn {
  Vector3 pos;
  template <class A> void serialize(A &ar) { ar(pos); }
};
struct ChatMessage {
  std::string text;
  int id;
  template <class A> void serialize(A &ar) { ar(text, id); }
};
struct SetName {
  std::string name;
  int id;
  template <class A> void serialize(A &ar) { ar(name, id); }
};
struct PlaceObject {
  Object object;
  template <class A> void serialize(A &ar) { ar(object); }
};
struct NewObject {
  Object object; // server -> clients
  template <class A> void serialize(A &ar) { ar(object); }
};
struct RemoveObject {
  Vector3 pos;
  template <class A> void serialize(A &ar) { ar(pos); }
};
struct DamageObject {
  Vector3 pos;
  template <class A> void serialize(A &ar) { ar(pos); }
};
struct clientHandshake {
  int ver; // client version
  template <class A> void serialize(A &ar) { ar(ver); }
};
struct kick { // server -> client
  int playerId;
  std::string reason;
  template <class A> void serialize(A &ar) { ar(playerId, reason); }
};
struct ChunkData {
  int cx, cz;
  std::vector<Object> blocks;
  template <class A> void serialize(A &ar) { ar(cx, cz, blocks); }
};
struct ChunkUnload {
  int cx, cz;
  template <class A> void serialize(A &ar) { ar(cx, cz); }
};
struct SetViewRadius {
  int radius;
  template <class A> void serialize(A &ar) { ar(radius); }
};
// TODO: Implement player permissions
// struct kickPlayer {
//   int playerId; // who to kick
//   std::string reason;
//   template <class A> void serialize(A &ar) { ar(playerId, reason); }
// };

// ==== pack: struct -> bytes, tag prepended ==== //
template <typename T> std::string pack(Type type, const T &msg) {
  std::ostringstream os(std::ios::binary);
  os.put(static_cast<char>(type));
  {
    cereal::BinaryOutputArchive ar(os);
    ar(msg);
  }
  return os.str();
}

// ==== unpack: peek tag, then caller reads the matching type ==== //
Type peekType(const std::string &data);
template <typename T> T unpack(const std::string &data) {
  std::istringstream is(data.substr(1), std::ios::binary);
  cereal::BinaryInputArchive ar(is);
  T msg;
  ar(msg);
  return msg;
}
} // namespace proto
