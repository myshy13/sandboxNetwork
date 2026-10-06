#pragma once
#include <cstddef>
#include <cstdint>
#include <raylib.h>

#include "blocks.hpp"

struct ObjectTransform {
  Vector3 pos{0, 0, 0};
  Vector3 scale{0, 0, 0};
};

constexpr int MAX_DURABILITY = 3;

class Object {
private:
  int id{-1}; // -1 means an unassigned id
  ObjectTransform transform{};
  int durability{MAX_DURABILITY};
  BlockType type{BlockType::Dirt};
  uint8_t state{0}; // source block

public:
  const ObjectTransform &getTransform() const { return transform; }
  // The type's colour, a quarter darker per point of damage taken.
  Color getColor() const {
    Color c = blockColor(type);
    for (int i = durability; i < MAX_DURABILITY; i++) {
      c.r = static_cast<unsigned char>(c.r * 3 / 4);
      c.g = static_cast<unsigned char>(c.g * 3 / 4);
      c.b = static_cast<unsigned char>(c.b * 3 / 4);
    }
    return c;
  }
  int getDurability() const { return durability; }
  int getId() const { return id; }
  BlockType getType() const { return type; }
  void damage() { durability--; }
  void setState(uint8_t newLevel) { state = newLevel; };
  uint8_t getState() const { return state; };

  void setId(int newId) { id = newId; }

  bool isSolid() const { return ::isSolid(type); }
  bool isTranslucent() const { return ::isTranslucent(type); }
  bool isOpaque() const { return ::isOpaque(type); }
  bool isFluid() const { return ::isFluid(type); }

  Object() = default;
  Object(int objectId, ObjectTransform t, BlockType ty)
      : id(objectId), transform(t), type(ty) {}
  // client only: the server assigns the real id
  Object(ObjectTransform t, BlockType ty) : transform(t), type(ty) {}

  // cereal: send the fields the other side needs to draw + identify the
  // block (colour is derived from type + durability, so it isn't sent).
  // Vector3's serializer is the free function in Shared/Protocol/protocol.hpp.
  template <class Archive> void serialize(Archive &ar) {
    ar(id, transform.pos, transform.scale, durability, type, state);
  }
};