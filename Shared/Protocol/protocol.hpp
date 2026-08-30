#pragma once
#include "Models/Object.hpp"
#include "raylib.h"
#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cstdint>
#include <sstream>
#include <string>

// Vector3 must already be defined before this header is included
// (via <raylib.h> on the Client, or "models.hpp" on the Server) -
// both define a compatible {x, y, z} float struct.
template <class Archive> void serialize(Archive &ar, Vector3 &v) {
  ar(v.x, v.y, v.z);
}

namespace proto {
enum class Type : uint8_t {
  PlayerUpdate,
  GivenId,
  DeletePlayer,
  CreateBullet,
  NewBullet,
  DeleteBullet,
  PlayerHit,
  Respawn,
  ChatMessage,
  SetName,
  PlaceObject,
  NewObject,
  RemoveObject
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
  int playerId;
  template <class A> void serialize(A &ar) { ar(playerId); }
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
  int id; // both directions
  template <class A> void serialize(A &ar) { ar(id); }
};

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
