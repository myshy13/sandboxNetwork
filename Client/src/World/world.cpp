#include "world.hpp"

#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <vector>

#include "Client/client.hpp"
#include "Models/Object.hpp"
#include "Models/blocks.hpp"
#include "env.hpp"
#include "rlgl.h"

// Axis-aligned box centred on an object (pos is the centre; see placeBlock).
BoundingBox objectBox(const ObjectTransform& t) {  // not inline: renderer.cpp
                                                   // declares and calls it too
  Vector3 half = Vector3Scale(t.scale, 0.5f);
  return {Vector3Subtract(t.pos, half), Vector3Add(t.pos, half)};
}

// Snap a world point to the centre of its env::BLOCKSIZE-grid cell.
static Vector3 snapToCell(Vector3 p) {
  return {(floor(p.x / env::BLOCKSIZE.x) + 0.5f) * env::BLOCKSIZE.x,
          (floor(p.y / env::BLOCKSIZE.y) + 0.5f) * env::BLOCKSIZE.y,
          (floor(p.z / env::BLOCKSIZE.z) + 0.5f) * env::BLOCKSIZE.z};
}

// Packs a grid cell's (x, y, z) into one hashable key, offset so negative
// coordinates don't collide with positive ones once shifted into place.
static int64_t cellKey(Vector3 coord) {
  Vector3 cell = Vector3Divide(coord, env::BLOCKSIZE);

  constexpr int64_t OFFSET = 1 << 20;
  int64_t x = std::floor(cell.x) + OFFSET;
  int64_t y = std::floor(cell.y) + OFFSET;
  int64_t z = std::floor(cell.z) + OFFSET;

  return (x << 42) | (y << 21) | z;
}

bool World::placeBlock(Ray aim, Client& client, const Vector3& playerPos,
                       BlockType type) {
  RayCollision best{};
  best.distance = FLT_MAX;
  // Water hit directly: you're replacing it in place, not building against a
  // face.
  bool onWater = false;
  // Walk the ray through the cell index instead of testing every block; only
  // cells within REACH matter.
  constexpr float RAY_STEP =
      0.25f;  // far smaller than a cell, so no cell along the ray is skipped
  for (float t = 0.0f; t <= REACH; t += RAY_STEP) {
    auto it = occupiedCells.find(cellKey(
        snapToCell(Vector3Add(aim.position, Vector3Scale(aim.direction, t)))));
    if (it == occupiedCells.end()) {
      continue;
    }
    // The ray is inside this cell, so it hits the block; the exact test gives
    // the entry point and face normal.
    RayCollision rc =
        GetRayCollisionBox(aim, objectBox(objects[it->second].getTransform()));
    if (rc.hit) {
      best = rc;
      onWater = objects[it->second].isFluid();
      break;
    }
  }

  Vector3 target;
  if (best.distance != FLT_MAX && onWater) {
    // Nudge past the surface along the ray (not the face normal - water has no
    // "outward" face here) so snapToCell lands inside the water's own cell, not
    // the neighbour it's bordering.
    target = Vector3Add(best.point, Vector3Scale(aim.direction, 0.01f));
  } else if (best.distance != FLT_MAX) {
    target = Vector3Add(
        best.point,
        Vector3Multiply(best.normal, Vector3Scale(env::BLOCKSIZE, 0.5f)));
  } else if (aim.direction.y < 0.0f) {
    float dist = -aim.position.y / aim.direction.y;
    target = Vector3Add(aim.position, Vector3Scale(aim.direction, dist));
  } else {
    return false;  // aiming at the sky
  }

  if (Vector3Distance(aim.position, target) > REACH) {
    return false;  // too far away
  }

  Vector3 cell = snapToCell(target);

  auto occupant = occupiedCells.find(cellKey(cell));
  if (occupant != occupiedCells.end() &&
      !::isFluid(objects[occupant->second].getType())) {
    return false;  // one block per cell - only a fluid can be placed over
  }

  BoundingBox player;
  player.min = Vector3Subtract(playerPos, {env::PLAYER_SCALE.x * 0.5f, 0.0f,
                                           env::PLAYER_SCALE.z * 0.5f});
  player.max = Vector3Add(player.min, env::PLAYER_SCALE);

  // cell is the block's centre (see objectBox / snapToCell), not a corner.
  BoundingBox block = objectBox(ObjectTransform{cell, env::BLOCKSIZE});

  if (!::isSolid(type) || !CheckCollisionBoxes(player, block)) {
    Object o = Object{ObjectTransform{cell, env::BLOCKSIZE}, type};
    client.placeObject(o);

    return true;
  } else {
    return false;
  }
}

void World::update(float dt) {
  if (!freezeTime) {
    timeOfDay += dt / dayLengthSecs;
    if (timeOfDay >= 1) {
      timeOfDay -= 1;
    }
  }
}

// ==== chunks ==== //
static int64_t chunkKey(Vector3 pos) {
  int cx = (int)floorf(pos.x / World::CHUNK_SIZE);
  int cz = (int)floorf(pos.z / World::CHUNK_SIZE);
  return (static_cast<int64_t>(cx) << 32) ^ static_cast<uint32_t>(cz);
}

// Streaming chunk key: cx in the high 32 bits, cz in the low 32 (same packing
// as the server's chunkKey).
static int64_t streamKey(int cx, int cz) {
  return (static_cast<int64_t>(cx) << 32) | static_cast<uint32_t>(cz);
}

static int64_t streamKeyAt(Vector3 pos) {
  return streamKey(World::streamChunkCoord(pos.x),
                   World::streamChunkCoord(pos.z));
}

void World::markDirty(Vector3 pos) {
  dirtyChunks.insert(chunkKey(pos));
  // Chunks span all y, so only the horizontal neighbours can be in another
  // chunk.
  const Vector3 offsets[] = {{env::BLOCKSIZE.x, 0, 0},
                             {-env::BLOCKSIZE.x, 0, 0},
                             {0, 0, env::BLOCKSIZE.z},
                             {0, 0, -env::BLOCKSIZE.z}};
  for (const Vector3& o : offsets) {
    dirtyChunks.insert(chunkKey(Vector3Add(pos, o)));
  }
}

const std::vector<int>* World::getChunk(int64_t key) const {
  auto it = chunks.find(key);
  return it == chunks.end() ? nullptr : &it->second;
}

// ==== object bookkeeping ==== //
void World::indexObject() {
  Vector3 pos = objects.back().getTransform().pos;
  int idx = (int)objects.size() - 1;

  occupiedCells[cellKey(pos)] = idx;
  chunks[chunkKey(pos)].push_back(idx);
  streamChunks[streamKeyAt(pos)].insert(cellKey(pos));
  markDirty(pos);
}

void World::addObject(const Object& object) {
  Vector3 pos = object.getTransform().pos;
  if (!isChunkLoaded(streamChunkCoord(pos.x), streamChunkCoord(pos.z))) {
    return;
  }
  objects.push_back(object);
  indexObject();
}

void World::addObjects(const std::vector<Object>& newObjects) {
  // No reserve() here: it allocates exactly what you ask, so calling it per
  // chunk copies the whole vector every time.
  for (const Object& o : newObjects) {
    objects.push_back(o);
    indexObject();
  }
}

void World::removeObject(Vector3 pos) {
  auto it = occupiedCells.find(cellKey(pos));
  if (it == occupiedCells.end()) return;

  int removedIdx = (int)(it->second);
  int lastIdx = (int)objects.size() - 1;

  occupiedCells.erase(cellKey(pos));
  std::vector<int>& list = chunks[chunkKey(pos)];
  std::erase(list, removedIdx);
  if (list.empty()) chunks.erase(chunkKey(pos));
  // The streaming index holds cell keys, which don't change when blocks swap
  // places, so it needs no fix-up below.
  auto streamIt = streamChunks.find(streamKeyAt(pos));
  streamIt->second.erase(cellKey(pos));
  if (streamIt->second.empty()) streamChunks.erase(streamIt);
  markDirty(pos);

  // Swap-and-pop instead of erase, so only the moved object's index needs
  // fixing up - in occupiedCells and in its chunk - not every index after it.
  if (removedIdx != lastIdx) {
    Vector3 movedPos = objects[lastIdx].getTransform().pos;
    objects[removedIdx] = objects[lastIdx];

    occupiedCells[cellKey(movedPos)] = removedIdx;
    std::vector<int>& movedList = chunks[chunkKey(movedPos)];
    std::replace(movedList.begin(), movedList.end(), lastIdx, removedIdx);
    dirtyChunks.insert(chunkKey(
        movedPos));  // its index changed, so the renderer must relist it
  }
  objects.pop_back();
}

void World::damageObject(Vector3 pos) {
  auto it = occupiedCells.find(cellKey(pos));
  if (it == occupiedCells.end()) return;

  objects[it->second].damage();
}

void World::setWaterLevel(Vector3 pos, uint8_t level) {
  auto it = occupiedCells.find(cellKey(pos));
  if (it == occupiedCells.end()) return;

  objects[it->second].setState(level);
  markDirty(
      pos);  // its surface height (and its neighbours' side faces) changed
}

void World::clear() {
  for (const auto& [key, blocks] : chunks) {
    dirtyChunks.insert(key);
  }
  chunks.clear();
  streamChunks.clear();
  loadedStreamChunks.clear();
  occupiedCells.clear();
  objects.clear();
}

// ==== streaming chunks ==== //
void World::addChunk(int cx, int cz, const std::vector<Object>& blocks) {
  loadedStreamChunks.insert(streamKey(cx, cz));
  if (!blocks.empty()) {
    streamChunks[streamKey(cx, cz)].reserve(
        blocks.size());  // one new set, sized once: no rehashing while adding
  }
  addObjects(blocks);
}

void World::unloadChunk(int cx, int cz) {
  const int64_t key = streamKey(cx, cz);
  loadedStreamChunks.erase(key);

  auto it = streamChunks.find(key);
  if (it == streamChunks.end()) return;  // never loaded, or an empty chunk

  // Copy the positions first: removeObject edits this very set (and
  // swap-and-pops `objects`).
  std::vector<Vector3> positions;
  positions.reserve(it->second.size());
  for (int64_t cell : it->second)
    positions.push_back(objects[occupiedCells.at(cell)].getTransform().pos);

  for (const Vector3& pos : positions) removeObject(pos);
}

bool World::isChunkLoaded(int cx, int cz) const {
  return loadedStreamChunks.contains(streamKey(cx, cz));
}

std::vector<Object>& World::getObjects() { return objects; }

bool World::isOccupied(Vector3 pos) const {
  return occupiedCells.contains(cellKey(pos));
}

bool World::isSolid(Vector3 pos) const {
  auto it = occupiedCells.find(cellKey(pos));
  return it != occupiedCells.end() && objects[it->second].isSolid();
}

BlockType World::typeAt(Vector3 pos) const {
  auto it = occupiedCells.find(cellKey(pos));
  if (it == occupiedCells.end()) {
    return BlockType::Count;
  }
  return objects[it->second].getType();
}

bool World::occludes(Vector3 pos) const {
  auto it = occupiedCells.find(cellKey(pos));
  return it != occupiedCells.end() && objects[it->second].isOpaque();
}

bool World::isFluid(Vector3 pos) const {
  auto it = occupiedCells.find(cellKey(pos));
  return it != occupiedCells.end() && objects[it->second].isFluid();
}

bool World::isTranslucent(Vector3 pos) const {
  auto it = occupiedCells.find(cellKey(pos));
  return it != occupiedCells.end() && objects[it->second].isTranslucent();
}

bool World::isWater(Vector3 pos) const {
  auto it = occupiedCells.find(cellKey(pos));
  return it != occupiedCells.end() &&
         objects[it->second].getType() == BlockType::Water;
}

int World::waterLevel(Vector3 pos) const {
  auto it = occupiedCells.find(cellKey(pos));
  if (it == occupiedCells.end() ||
      objects[it->second].getType() != BlockType::Water)
    return -1;
  return objects[it->second].getState();
}

bool World::boxCollides(BoundingBox box, BlockPredicate matches) const {
  // Blocks are one-per-cell on the fixed env::BLOCKSIZE grid, so only the cells
  // box's own extent spans can possibly contain a hit.
  int minX = (int)floorf(box.min.x / env::BLOCKSIZE.x);
  int maxX = (int)floorf(box.max.x / env::BLOCKSIZE.x);
  int minY = (int)floorf(box.min.y / env::BLOCKSIZE.y);
  int maxY = (int)floorf(box.max.y / env::BLOCKSIZE.y);
  int minZ = (int)floorf(box.min.z / env::BLOCKSIZE.z);
  int maxZ = (int)floorf(box.max.z / env::BLOCKSIZE.z);

  for (int y = minY; y <= maxY; y++) {
    for (int z = minZ; z <= maxZ; z++) {
      for (int x = minX; x <= maxX; x++) {
        Vector3 cellPos = {(x + 0.5f) * env::BLOCKSIZE.x,
                           (y + 0.5f) * env::BLOCKSIZE.y,
                           (z + 0.5f) * env::BLOCKSIZE.z};
        auto it = occupiedCells.find(cellKey(cellPos));
        if (it == occupiedCells.end()) continue;
        const Object& o = objects[it->second];
        if (matches(o.getType()) &&
            CheckCollisionBoxes(box, objectBox(o.getTransform())))
          return true;
      }
    }
  }
  return false;
}
