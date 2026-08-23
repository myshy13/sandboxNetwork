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

  std::printf("protocol tests passed\n");
  return 0;
}
