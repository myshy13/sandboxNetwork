#pragma once
#include <raylib.h>

struct ObjectTransform {
  Vector3 pos{0, 0, 0};
  Vector3 scale{0, 0, 0};
};

class Object {
private:
  int id{-1}; // -1 means an unassigned id
  ObjectTransform transform{};
  Color color{WHITE};
  int durability{3};

public:
  const ObjectTransform &getTransform() const { return transform; }
  const Color &getColor() const { return color; }
  int getDurability() const { return durability; }
  int getId() const { return id; }
  void damage() {
    durability--;
    switch (durability) {
    case 2:
      color = LIGHTGRAY;
      break;
    case 1:
      color = GRAY;
      break;
    case 0:
      color = DARKGRAY;
      break;
    }
  }

  void setId(int newId) { id = newId; }

  Object() = default;
  Object(int objectId, Vector3 pos) {
    id = objectId;
    transform.pos = pos;
  }
  Object(int objectId, ObjectTransform t) : id(objectId), transform(t) {}

  // cereal: send the fields the other side needs to draw + identify the block.
  // Vector3's serializer is the free function in Shared/Protocol/protocol.hpp.
  template <class Archive> void serialize(Archive &ar) {
    ar(id, transform.pos, transform.scale, color.r, color.g, color.b, color.a,
       durability);
  }
};