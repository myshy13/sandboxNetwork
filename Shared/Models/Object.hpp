#pragma once
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <raylib.h>

struct ObjectTransform {
  Vector3 pos{0, 0, 0};
  Vector3 scale{0, 0, 0};
};

// Count is a sentinel, never a real block: it sizes BLOCK_INFO and
// bounds-checks.
enum class BlockType : uint8_t { Grass, Dirt, Water, Leaves, Wood, Count };

struct BlockInfo {
  bool solid;       // players collide with it
  bool placeable;   // a client may ask the server to place it
  bool fluid;       // placing over it replaces it, and players swim in it
  Color color;      // the block's look until it has a texture; alpha is opacity
  bool translucent; // the player can see through it
};

// One row per BlockType, in enum order: a new block is a new enum value + a
// row.
inline constexpr BlockInfo BLOCK_INFO[] = {
    {true, true, false, WHITE, false}, // Grass
    {true, true, false, WHITE, false}, // Dirt
    {false, true, true, WHITE, true},  // Water
    {true, true, false, WHITE, false}, // Leaves
    {true, true, false, WHITE, false}, // Wood
};
static_assert(std::size(BLOCK_INFO) == static_cast<size_t>(BlockType::Count),
              "BLOCK_INFO needs exactly one row per BlockType");

// A type read off the wire is untrusted: check it before indexing BLOCK_INFO.
inline bool isValid(BlockType t) { return t < BlockType::Count; }
inline bool isSolid(BlockType t) {
  return isValid(t) && BLOCK_INFO[static_cast<size_t>(t)].solid;
}
inline bool isPlaceable(BlockType t) {
  return isValid(t) && BLOCK_INFO[static_cast<size_t>(t)].placeable;
}
inline bool isFluid(BlockType t) {
  return isValid(t) && BLOCK_INFO[static_cast<size_t>(t)].fluid;
}
inline bool isTranslucent(BlockType t) {
  return isValid(t) && BLOCK_INFO[static_cast<size_t>(t)].translucent;
}
// Magenta marks an invalid type so it's obvious rather than invisible.
inline Color blockColor(BlockType t) {
  return isValid(t) ? BLOCK_INFO[static_cast<size_t>(t)].color : MAGENTA;
}

constexpr int MAX_DURABILITY = 3;

class Object {
private:
  int id{-1}; // -1 means an unassigned id
  ObjectTransform transform{};
  int durability{MAX_DURABILITY};
  BlockType type{BlockType::Dirt};
  uint8_t level{0}; // source block

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
  void setLevel(uint8_t newLevel) { level = newLevel; };
  uint8_t getLevel() const { return level; };

  void setId(int newId) { id = newId; }

  bool isSolid() const { return ::isSolid(type); }
  bool isTranslucent() const { return ::isTranslucent(type); }

  Object() = default;
  Object(int objectId, ObjectTransform t, BlockType ty)
      : id(objectId), transform(t), type(ty) {}
  // client only: the server assigns the real id
  Object(ObjectTransform t, BlockType ty) : transform(t), type(ty) {}

  // cereal: send the fields the other side needs to draw + identify the
  // block (colour is derived from type + durability, so it isn't sent).
  // Vector3's serializer is the free function in Shared/Protocol/protocol.hpp.
  template <class Archive> void serialize(Archive &ar) {
    ar(id, transform.pos, transform.scale, durability, type, level);
  }
};