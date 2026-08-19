#pragma once
#include <cstdint>
#include <sstream>
#include <string>
#include <cereal/archives/binary.hpp>

// Vector3 must already be defined before this header is included
// (via <raylib.h> on the Client, or "models.hpp" on the Server) -
// both define a compatible {x, y, z} float struct.
template <class Archive> void serialize(Archive &ar, Vector3 &v) {
  ar(v.x, v.y, v.z);
}

namespace proto {
enum class Type : uint8_t { PlayerUpdate, GivenId, DeletePlayer };

struct PlayerUpdate {
  int id;
  Vector3 pos;
  template <class A> void serialize(A &ar) { ar(id, pos); }
};
struct GivenId {
  int id;
  template <class A> void serialize(A &ar) { ar(id); }
};
struct DeletePlayer {
  int id;
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
