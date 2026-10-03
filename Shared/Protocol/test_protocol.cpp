#include "Protocol/protocol.hpp"
#include <cassert>
#include <cstdio>
#include <raylib.h>

int main() {
  // ==== PlayerUpdate ==== //
  {
    auto bytes = proto::pack(proto::Type::PlayerUpdate,
                             proto::PlayerUpdate{7, {1.5f, 2.5f, -3.0f}});
    assert(proto::peekType(bytes) == proto::Type::PlayerUpdate);
    auto msg = proto::unpack<proto::PlayerUpdate>(bytes);
    assert(msg.id == 7);
    assert(msg.pos.x == 1.5f && msg.pos.y == 2.5f && msg.pos.z == -3.0f);
  }

  // ==== GivenId ==== //
  {
    auto bytes = proto::pack(proto::Type::GivenId, proto::GivenId{42});
    assert(proto::peekType(bytes) == proto::Type::GivenId);
    auto msg = proto::unpack<proto::GivenId>(bytes);
    assert(msg.id == 42);
  }

  // ==== DeletePlayer ==== //
  {
    auto bytes = proto::pack(proto::Type::DeletePlayer, proto::DeletePlayer{3});
    assert(proto::peekType(bytes) == proto::Type::DeletePlayer);
    auto msg = proto::unpack<proto::DeletePlayer>(bytes);
    assert(msg.id == 3);
  }

  // ==== PlayerHit ==== //
  {
    auto bytes = proto::pack(proto::Type::PlayerHit, proto::PlayerHit{2, 0, 1});
    auto msg = proto::unpack<proto::PlayerHit>(bytes);
    assert(proto::peekType(bytes) == proto::Type::PlayerHit);
    assert(msg.health == 2 && msg.id == 0 && msg.shooterId == 1);
  }

  // ==== Respawn ==== //
  {
    auto bytes =
        proto::pack(proto::Type::Respawn, proto::Respawn{{0.0f, 10.0f, 0.0f}});
    assert(proto::peekType(bytes) == proto::Type::Respawn);
    auto msg = proto::unpack<proto::Respawn>(bytes);
    assert(msg.pos.x == 0.0f && msg.pos.y == 10.0f && msg.pos.z == 0.0f);
  }

  // ==== ChunkData: negative coordinates, no blocks ==== //
  {
    auto bytes =
        proto::pack(proto::Type::ChunkData, proto::ChunkData{-1, -3, {}});
    assert(proto::peekType(bytes) == proto::Type::ChunkData);
    auto msg = proto::unpack<proto::ChunkData>(bytes);
    assert(msg.cx == -1 && msg.cz == -3);
    assert(msg.blocks.empty()); // an empty chunk is still sent, so the client
                                // knows it has loaded
  }

  // ==== ChunkData: blocks keep their id, position, scale, type and
  // durability ==== //
  {
    Object first(5, ObjectTransform{{2.5f, 7.5f, -12.5f}, {5, 5, 5}},
                 BlockType::Grass);
    Object second(6, ObjectTransform{{-2.5f, 2.5f, 7.5f}, {5, 5, 5}},
                  BlockType::Dirt);
    second.damage();

    auto bytes = proto::pack(proto::Type::ChunkData,
                             proto::ChunkData{4, -2, {first, second}});
    auto msg = proto::unpack<proto::ChunkData>(bytes);
    assert(msg.cx == 4 && msg.cz == -2);
    assert(msg.blocks.size() == 2);

    const Object &a = msg.blocks[0];
    assert(a.getId() == 5);
    assert(a.getTransform().pos.x == 2.5f && a.getTransform().pos.y == 7.5f &&
           a.getTransform().pos.z == -12.5f);
    assert(a.getTransform().scale.x == 5.0f);
    assert(a.getType() == BlockType::Grass);
    assert(a.getDurability() == first.getDurability());

    const Object &b = msg.blocks[1];
    assert(b.getId() == 6);
    assert(b.getTransform().pos.x == -2.5f && b.getTransform().pos.z == 7.5f);
    assert(b.getType() == BlockType::Dirt);
    assert(b.getDurability() == MAX_DURABILITY - 1); // damage survives the wire
  }

  // ==== Object colour: from the type, a quarter darker per damage ==== //
  {
    Object fresh(1, ObjectTransform{{0, 0, 0}, {5, 5, 5}}, BlockType::Grass);
    Color c = fresh.getColor();
    assert(c.r == GREEN.r && c.g == GREEN.g && c.b == GREEN.b);
    fresh.damage();
    assert(fresh.getColor().g == GREEN.g * 3 / 4);
    assert(fresh.getColor().a == GREEN.a); // damage never changes opacity
    // a received block's colour matches the sender's without being sent
    auto msg = proto::unpack<proto::ChunkData>(proto::pack(
        proto::Type::ChunkData, proto::ChunkData{0, 0, {fresh}}));
    assert(msg.blocks[0].getColor().g == fresh.getColor().g);
  }

  // ==== ChunkData: a full-size chunk (~1200 blocks, ~40 KB, far over one 1392
  // byte datagram) ==== //
  {
    std::vector<Object> blocks;
    for (int i = 0; i < 1200; i++) {
      blocks.emplace_back(
          i, ObjectTransform{{i * 5.0f + 2.5f, 2.5f, 2.5f}, {5, 5, 5}},
          BlockType::Dirt);
    }
    auto bytes =
        proto::pack(proto::Type::ChunkData, proto::ChunkData{0, 0, blocks});
    assert(bytes.size() > 1392); // fragmenting is ENet's job, so the message
                                 // itself just has to round-trip
    auto msg = proto::unpack<proto::ChunkData>(bytes);
    assert(msg.blocks.size() == 1200);
    assert(msg.blocks[0].getId() == 0);
    assert(msg.blocks[1199].getId() == 1199);
    assert(msg.blocks[1199].getTransform().pos.x == 1199 * 5.0f + 2.5f);
  }

  // ==== ChunkData: water blocks keep their type and flow level ==== //
  {
    Object water(9, ObjectTransform{{2.5f, 2.5f, 2.5f}, {5, 5, 5}},
                 BlockType::Water);
    water.setLevel(3);
    Object solid(10, ObjectTransform{{7.5f, 2.5f, 2.5f}, {5, 5, 5}},
                 BlockType::Dirt);

    auto msg = proto::unpack<proto::ChunkData>(proto::pack(
        proto::Type::ChunkData, proto::ChunkData{0, 0, {water, solid}}));
    assert(msg.blocks[0].getType() == BlockType::Water &&
           msg.blocks[0].getLevel() == 3);
    assert(msg.blocks[1].getType() == BlockType::Dirt &&
           msg.blocks[1].getLevel() == 0); // default: source/unset
  }

  // ==== BLOCK_INFO: properties come from the table, bad wire values fail ====
  // //
  {
    assert(isSolid(BlockType::Grass) && isSolid(BlockType::Dirt));
    assert(isSolid(BlockType::Leaves) && isSolid(BlockType::Wood));
    assert(!isSolid(BlockType::Water) && isPlaceable(BlockType::Water));
    assert(isFluid(BlockType::Water) && !isFluid(BlockType::Dirt));
    assert(!isFluid(BlockType::Count));
    assert(blockColor(BlockType::Water).a < 255); // water is translucent
    assert(!isValid(BlockType::Count) && !isPlaceable(BlockType::Count));
    assert(!isSolid(static_cast<BlockType>(200))); // a hostile client's byte
  }

  // ==== UpdateWaterLevel ==== //
  {
    auto bytes = proto::pack(proto::Type::UpdateWaterLevel,
                             proto::UpdateWaterLevel{{-2.5f, 12.5f, 7.5f}, 7});
    assert(proto::peekType(bytes) == proto::Type::UpdateWaterLevel);
    auto msg = proto::unpack<proto::UpdateWaterLevel>(bytes);
    assert(msg.pos.x == -2.5f && msg.pos.y == 12.5f && msg.pos.z == 7.5f);
    assert(msg.level == 7);
  }

  // ==== ChunkUnload ==== //
  {
    auto bytes =
        proto::pack(proto::Type::ChunkUnload, proto::ChunkUnload{-7, 12});
    assert(proto::peekType(bytes) == proto::Type::ChunkUnload);
    auto msg = proto::unpack<proto::ChunkUnload>(bytes);
    assert(msg.cx == -7 && msg.cz == 12);
  }

  // ==== SetViewRadius ==== //
  {
    auto bytes =
        proto::pack(proto::Type::SetViewRadius, proto::SetViewRadius{6});
    assert(proto::peekType(bytes) == proto::Type::SetViewRadius);
    auto msg = proto::unpack<proto::SetViewRadius>(bytes);
    assert(msg.radius == 6);
  }

  std::printf("protocol tests passed\n");
  return 0;
}
