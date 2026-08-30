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

  bool active{false}; // active means that the object will not update, it will still be visible.
  bool visible{true}; // visisble defines whether the object is rendered, it will still update.

public:
  const ObjectTransform &getTransform() const { return transform; }
  const Color &getColor() const { return color; }
  int getId() const { return id; }
  bool isActive() const { return active; }
  bool isVisible() const { return visible; }

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
    ar(id, transform.pos, transform.scale,
       color.r, color.g, color.b, color.a, visible);
  }
};