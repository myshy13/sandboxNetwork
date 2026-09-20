#include "Server/chunk.hpp"
#include <cassert>
#include <cstdio>

int main() {
  // ==== round trip, including every sign combination ==== //
  const int values[] = {-100000, -3, -1, 0, 1, 4, 100000};
  for (int cx : values) {
    for (int cz : values) {
      auto [rx, rz] = chunkCoords(chunkKey(cx, cz));
      assert(rx == cx && rz == cz);
    }
  }

  // ==== a negative z must not leak into the x half ==== //
  assert(chunkCoords(chunkKey(5, -1)).first == 5);

  // ==== different chunks never share a key ==== //
  assert(chunkKey(1, 0) != chunkKey(0, 1));
  assert(chunkKey(-1, 0) != chunkKey(0, -1));
  assert(chunkKey(-1, -1) != chunkKey(0, 0));

  std::printf("chunk tests passed\n");
  return 0;
}
