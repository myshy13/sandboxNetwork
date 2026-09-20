#include <raylib.h>
#include "Protocol/protocol.hpp"
#include <cassert>
#include <cstdio>

int main() {
  // ==== PlayerUpdate ==== //
  {
    auto bytes = proto::pack(proto::Type::PlayerUpdate, proto::PlayerUpdate{7, {1.5f, 2.5f, -3.0f}});
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
    auto bytes = proto::pack(proto::Type::ChunkData, proto::ChunkData{-1, -3, {}});
    assert(proto::peekType(bytes) == proto::Type::ChunkData);
    auto msg = proto::unpack<proto::ChunkData>(bytes);
    assert(msg.cx == -1 && msg.cz == -3);
    assert(msg.blocks.empty()); // an empty chunk is still sent, so the client knows it has loaded
  }

  // ==== ChunkData: blocks keep their id, position, scale, colour and durability ==== //
  // (No damage() here: it calls raylib's ColorBrightness, and this test doesn't link raylib.)
  {
    Object first(5, ObjectTransform{{2.5f, 7.5f, -12.5f}, {5, 5, 5}}, GREEN);
    Object second(6, ObjectTransform{{-2.5f, 2.5f, 7.5f}, {5, 5, 5}}, BROWN);

    auto bytes = proto::pack(proto::Type::ChunkData, proto::ChunkData{4, -2, {first, second}});
    auto msg = proto::unpack<proto::ChunkData>(bytes);
    assert(msg.cx == 4 && msg.cz == -2);
    assert(msg.blocks.size() == 2);

    const Object &a = msg.blocks[0];
    assert(a.getId() == 5);
    assert(a.getTransform().pos.x == 2.5f && a.getTransform().pos.y == 7.5f && a.getTransform().pos.z == -12.5f);
    assert(a.getTransform().scale.x == 5.0f);
    assert(a.getColor().r == GREEN.r && a.getColor().g == GREEN.g && a.getColor().b == GREEN.b && a.getColor().a == GREEN.a);
    assert(a.getDurability() == first.getDurability());

    const Object &b = msg.blocks[1];
    assert(b.getId() == 6);
    assert(b.getTransform().pos.x == -2.5f && b.getTransform().pos.z == 7.5f);
    assert(b.getColor().r == BROWN.r && b.getColor().g == BROWN.g && b.getColor().b == BROWN.b);
  }

  // ==== ChunkData: a full-size chunk (~1200 blocks, ~40 KB, far over one 1392 byte datagram) ==== //
  {
    std::vector<Object> blocks;
    for (int i = 0; i < 1200; i++) {
      blocks.emplace_back(i, ObjectTransform{{i * 5.0f + 2.5f, 2.5f, 2.5f}, {5, 5, 5}}, WHITE);
    }
    auto bytes = proto::pack(proto::Type::ChunkData, proto::ChunkData{0, 0, blocks});
    assert(bytes.size() > 1392); // fragmenting is ENet's job, so the message itself just has to round-trip
    auto msg = proto::unpack<proto::ChunkData>(bytes);
    assert(msg.blocks.size() == 1200);
    assert(msg.blocks[0].getId() == 0);
    assert(msg.blocks[1199].getId() == 1199);
    assert(msg.blocks[1199].getTransform().pos.x == 1199 * 5.0f + 2.5f);
  }

  // ==== ChunkUnload ==== //
  {
    auto bytes = proto::pack(proto::Type::ChunkUnload, proto::ChunkUnload{-7, 12});
    assert(proto::peekType(bytes) == proto::Type::ChunkUnload);
    auto msg = proto::unpack<proto::ChunkUnload>(bytes);
    assert(msg.cx == -7 && msg.cz == 12);
  }

  // ==== SetViewRadius ==== //
  {
    auto bytes = proto::pack(proto::Type::SetViewRadius, proto::SetViewRadius{6});
    assert(proto::peekType(bytes) == proto::Type::SetViewRadius);
    auto msg = proto::unpack<proto::SetViewRadius>(bytes);
    assert(msg.radius == 6);
  }

  std::printf("protocol tests passed\n");
  return 0;
}
