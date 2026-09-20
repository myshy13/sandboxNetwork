#pragma once

#include "Client/client.hpp"
#include "Models/Object.hpp"
#include <cmath>
#include <raylib.h>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#define MAX_COLOURS 6

class World {
private:
  const Color colors[MAX_COLOURS] = {WHITE, BROWN, GREEN, DARKGRAY, RED, BLUE};
  int activeColor                 = 0;
  std::vector<Object> objects{};

public:
  void drawHud();
  bool placeBlock(Ray aim, Client &client, const Vector3 &playerPos);
  void addObject(const Object &object);
  void update();
  void removeObject(Vector3 pos);
  // indexes each object as it's added, no full rebuild.
  void addObjects(const std::vector<Object> &newObjects);
  void damageObject(Vector3 pos);
  // Empties the world (leaving a session); old chunks stay dirty so the renderer drops them.
  void clear();

  // ==== streaming chunks (what the server sends; see STREAM_CHUNK_SIZE) ==== //
  // Adds a chunk's blocks and records it as loaded, even when it has none.
  void addChunk(int cx, int cz, const std::vector<Object> &blocks);
  // Removes every block in the chunk. Safe if the chunk was never loaded.
  void unloadChunk(int cx, int cz);
  bool isChunkLoaded(int cx, int cz) const;

  std::vector<Object> &getObjects();
  bool isOccluded(const Object &o) const;
  // True if box overlaps a placed block. Only tests the handful of grid
  // cells box spans, not every object - see occupiedCells.
  bool boxCollides(BoundingBox box) const;

  static constexpr float CHUNK_SIZE = 15.0f; // world units per chunk (3 blocks)

  // The server's streaming chunk (16x16 cells, every height), not the renderer chunk above.
  static constexpr float STREAM_CHUNK_SIZE = 80.0f; // must match CHUNK_SIZE in Server/src/Server/server.cpp
  // Which streaming chunk a world x or z falls in; floors as a float, since an (int) cast rounds toward zero.
  static int streamChunkCoord(float v) { return (int)floorf(v / STREAM_CHUNK_SIZE); }
  // Indices into getObjects() of every block in a chunk, or null if it's empty.
  const std::vector<int> *getChunk(int64_t key) const;
  // Chunks whose blocks (or neighbours' occlusion) changed since the last call.
  std::unordered_set<int64_t> takeDirtyChunks() { return std::exchange(dirtyChunks, {}); }

private:
  // Indexes objects[objects.size() - 1] (the object just appended) into occupiedCells and chunks.
  void indexObject();
  // Marks pos's chunk and its 6 face-neighbours' chunks dirty (occlusion looks at neighbours).
  void markDirty(Vector3 pos);

  // chunkKey(pos) -> indices into `objects`, kept in sync with swap-and-pop in removeObject.
  std::unordered_map<int64_t, std::vector<int>> chunks;
  std::unordered_set<int64_t> dirtyChunks;

  // streamKey(cx, cz) -> cellKeys of the blocks in that streaming chunk. Cell keys, not indices into
  // `objects`, so swap-and-pop never has to fix these up, and a set, so removing one block is O(1).
  // Separate from `chunks`: 80 isn't a multiple of the renderer's 15. No empty sets.
  std::unordered_map<int64_t, std::unordered_set<int64_t>> streamChunks;
  // Streaming chunks the server has sent, empty ones included (streamChunks has no empty lists).
  std::unordered_set<int64_t> loadedStreamChunks;

  // cellKey(pos) -> index into `objects`. Blocks sit on a fixed grid
  // (see snapToCell), so this doubles as both occlusion lookup and the
  // spatial index for collision - one block per cell, no duplicates.
  // Kept incrementally up to date (see addObject/removeObject) rather than
  // rebuilt wholesale, since a full rehash of the whole world on every
  // single block edit is what caused the multi-second collision-grid freeze.
  std::unordered_map<int64_t, int> occupiedCells;
};