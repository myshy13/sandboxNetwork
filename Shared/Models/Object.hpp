#pragma once
#include <cstdint>
#include <raylib.h>

struct ObjectTransform {
  Vector3 pos{0, 0, 0};
  Vector3 scale{0, 0, 0};
};

enum class BlockType : uint8_t { Solid, Water };

class Object {
private:
  int id{-1}; // -1 means an unassigned id
  ObjectTransform transform{};
  Color color{WHITE};
  int durability{3};
  BlockType type{BlockType::Solid};
  uint8_t level{0}; // source block

public:
  const ObjectTransform &getTransform() const { return transform; }
  const Color &getColor() const { return color; }
  int getDurability() const { return durability; }
  int getId() const { return id; }
  BlockType getType() const { return type; }
  void damage() {
    durability--;
    color = ColorBrightness(color, -0.25f); // darken toward black, keep hue
  }
  void setLevel(uint8_t newLevel) { level = newLevel; };
  uint8_t getLevel() const { return level; };

  void setId(int newId) { id = newId; }

  Object() = default;
  Object(int objectId, Vector3 pos) {
    id = objectId;
    transform.pos = pos;
  }
  Object(int objectId, ObjectTransform t) : id(objectId), transform(t) {}
  Object(int objectId, ObjectTransform t, Color c)
      : id(objectId), transform(t), color(c) {}
  Object(int objectId, ObjectTransform form, Color c, BlockType t)
      : id(objectId), transform(form), color(c), type(t) {}

  // cereal: send the fields the other side needs to draw + identify the block.
  // Vector3's serializer is the free function in Shared/Protocol/protocol.hpp.
  template <class Archive> void serialize(Archive &ar) {
    ar(id, transform.pos, transform.scale, color.r, color.g, color.b, color.a,
       durability, type, level);
  }
};