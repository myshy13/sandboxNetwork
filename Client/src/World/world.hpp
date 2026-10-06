#pragma once

#include <raylib.h>

#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Client/client.hpp"
#include "Models/Object.hpp"

class World {
 private:
  std::vector<Object> objects{};
  float timeOfDay{};
  float dayLengthSecs{};
  bool freezeTime{false};

 public:
  bool placeBlock(Ray aim, Client& client, const Vector3& playerPos,
                  BlockType type);
  void addObject(const Object& object);
  void removeObject(Vector3 pos);
  // indexes each object as it's added, no full rebuild.
  void addObjects(const std::vector<Object>& newObjects);
  void damageObject(Vector3 pos);
  void setWaterLevel(Vector3 pos, uint8_t level);
  // Empties the world (leaving a session); old chunks stay dirty so the
  // renderer drops them.
  void clear();

  void update(float dt);

  // ==== streaming chunks ==== //
  // Adds a chunk's blocks and records it as loaded, even when it has none.
  void addChunk(int cx, int cz, const std::vector<Object>& blocks);
  // Removes every block in the chunk. Safe if the chunk was never loaded.
  void unloadChunk(int cx, int cz);
  bool isChunkLoaded(int cx, int cz) const;

  std::vector<Object>& getObjects();
  // True if a block sits in the cell containing pos (the renderer's
  // face-neighbour test).
  bool isOccupied(Vector3 pos) const;
  // checks if a block type is solid (can the player collide with it)
  bool isSolid(Vector3 pos) const;
  // should the block be occluded (is it opaque)
  bool occludes(Vector3 pos) const;
  // True if the cell containing pos holds a fluid block.
  bool isFluid(Vector3 pos) const;
  // Whether the specified cell contains water
  bool isWater(Vector3 pos) const;
  // The flow level of the water in pos's cell (0 = source), or -1 if there's
  // no water there.
  int waterLevel(Vector3 pos) const;
  // True if box overlaps a placed block. Only tests the handful of grid
  // cells box spans, not every object - see occupiedCells.
  // `matches` picks which block types count (default: solid ones).
  using BlockPredicate = bool (*)(BlockType);
  bool boxCollides(BoundingBox box, BlockPredicate matches = ::isSolid) const;

  static constexpr float CHUNK_SIZE =
      15.0f;  // world units per chunk (3 blocks)

  // The server's streaming chunk (16x16 cells, every height), not the renderer
  // chunk above.
  static constexpr float STREAM_CHUNK_SIZE =
      80.0f;  // must match CHUNK_SIZE in Server/src/Server/server.cpp

  // Which streaming chunk a world x or z falls in; floors as a float, since an
  // (int) cast rounds toward zero.
  static int streamChunkCoord(float v) {
    return (int)floorf(v / STREAM_CHUNK_SIZE);
  }

  // Indices into getObjects() of every block in a chunk, or null if it's empty.
  const std::vector<int>* getChunk(int64_t key) const;

  // Chunks whose blocks (or neighbours' occlusion) changed since the last call.
  std::unordered_set<int64_t> takeDirtyChunks() {
    return std::exchange(dirtyChunks, {});
  }

  void setTimeSettings(TimeSetting timeSettings) {
    timeOfDay = timeSettings.timeOfDay;
    dayLengthSecs = timeSettings.dayLengthSecs;
    freezeTime = timeSettings.freezeTime;
  }

  float getTime() const { return timeOfDay; }

  void setDayLength(float seconds) { dayLengthSecs = seconds; }

  float getDayLength() const { return dayLengthSecs; }

 private:
  // Indexes objects[objects.size() - 1] (the object just appended) into
  // occupiedCells and chunks.
  void indexObject();
  // Marks pos's chunk and its 6 face-neighbours' chunks dirty (occlusion looks
  // at neighbours).
  void markDirty(Vector3 pos);

  // chunkKey(pos) -> indices into `objects`, kept in sync with swap-and-pop in
  // removeObject.
  std::unordered_map<int64_t, std::vector<int>> chunks;
  std::unordered_set<int64_t> dirtyChunks;

  // streamKey(cx, cz) -> cellKeys of the blocks in that streaming chunk. Cell
  // keys, not indices into `objects`, so swap-and-pop never has to fix these
  // up, and a set, so removing one block is O(1). Separate from `chunks`: 80
  // isn't a multiple of the renderer's 15. No empty sets.
  std::unordered_map<int64_t, std::unordered_set<int64_t>> streamChunks;
  // Streaming chunks the server has sent, empty ones included (streamChunks has
  // no empty lists).
  std::unordered_set<int64_t> loadedStreamChunks;

  // cellKey(pos) -> index into `objects`. Blocks sit on a fixed grid
  // (see snapToCell), so this doubles as both occlusion lookup and the
  // spatial index for collision - one block per cell, no duplicates.
  // Kept incrementally up to date (see addObject/removeObject) rather than
  // rebuilt wholesale, since a full rehash of the whole world on every
  // single block edit is what caused the multi-second collision-grid freeze.
  std::unordered_map<int64_t, int> occupiedCells;
};