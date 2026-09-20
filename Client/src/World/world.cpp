#include "world.hpp"
#include "Client/client.hpp"
#include "Models/Object.hpp"
#include "rlgl.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <raylib.h>
#include <raymath.h>
#include <vector>

// Axis-aligned box centred on an object (pos is the centre; see placeBlock).
inline BoundingBox objectBox(const ObjectTransform &t) {
  Vector3 half = Vector3Scale(t.scale, 0.5f);
  return {Vector3Subtract(t.pos, half), Vector3Add(t.pos, half)};
}

void World::drawHud() {
  constexpr float BOXSIZE = 50.0f; // square
  for (int i = 0; i < MAX_COLOURS; i++) {
    const Color &color = colors[i];
    if (activeColor == i) {
      DrawRectangle(GetScreenWidth() - BOXSIZE * MAX_COLOURS + i * BOXSIZE, GetScreenHeight() - BOXSIZE, BOXSIZE, BOXSIZE, WHITE);
    } else {
      DrawRectangle(GetScreenWidth() - BOXSIZE * MAX_COLOURS + i * BOXSIZE, GetScreenHeight() - BOXSIZE, BOXSIZE, BOXSIZE, GRAY);
    }
    DrawRectangle(GetScreenWidth() - BOXSIZE * MAX_COLOURS + i * BOXSIZE + 5, GetScreenHeight() - BOXSIZE + 5, BOXSIZE - 10, BOXSIZE - 10, color);
  }
}

void World::update() {
  int key = GetKeyPressed();
  if (key >= KEY_ONE && key < KEY_ONE + MAX_COLOURS) {
    activeColor = key - KEY_ONE;
  }
}

// Every placed block is this size, and the build grid has cells this size.
constexpr Vector3 blockSize = {5, 5, 5};

// Snap a world point to the centre of its blockSize-grid cell.
static Vector3 snapToCell(Vector3 p) {
  return {(floorf(p.x / blockSize.x) + 0.5f) * blockSize.x,
          (floorf(p.y / blockSize.y) + 0.5f) * blockSize.y,
          (floorf(p.z / blockSize.z) + 0.5f) * blockSize.z};
}

// Packs a grid cell's (x, y, z) into one hashable key, offset so negative
// coordinates don't collide with positive ones once shifted into place.
static int64_t cellKey(Vector3 coord) {
  Vector3 cell = Vector3Divide(coord, blockSize);

  constexpr int64_t OFFSET = 1 << 20;
  int64_t x                = std::lround(cell.x) + OFFSET;
  int64_t y                = std::lround(cell.y) + OFFSET;
  int64_t z                = std::lround(cell.z) + OFFSET;

  return (x << 42) | (y << 21) | z;
}

bool World::placeBlock(Ray aim, Client &client, const Vector3 &playerPos) {
  RayCollision best{};
  best.distance = FLT_MAX;
  // Walk the ray through the cell index instead of testing every block; only cells within REACH matter.
  constexpr float RAY_STEP = 0.25f; // far smaller than a cell, so no cell along the ray is skipped
  for (float t = 0.0f; t <= REACH; t += RAY_STEP) {
    auto it = occupiedCells.find(cellKey(snapToCell(Vector3Add(aim.position, Vector3Scale(aim.direction, t)))));
    if (it == occupiedCells.end()) {
      continue;
    }
    // The ray is inside this cell, so it hits the block; the exact test gives the entry point and face normal.
    RayCollision rc = GetRayCollisionBox(aim, objectBox(objects[it->second].getTransform()));
    if (rc.hit) {
      best = rc;
      break;
    }
  }

  Vector3 target;
  if (best.distance != FLT_MAX) {
    target = Vector3Add(best.point, Vector3Multiply(best.normal, Vector3Scale(blockSize, 0.5f)));
  } else if (aim.direction.y < 0.0f) {
    float dist = -aim.position.y / aim.direction.y;
    target     = Vector3Add(aim.position, Vector3Scale(aim.direction, dist));
  } else {
    return false; // aiming at the sky
  }

  if (Vector3Distance(aim.position, target) > REACH) {
    return false; // too far away
  }

  Vector3 cell = snapToCell(target);

  if (occupiedCells.contains(cellKey(cell))) {
    return false; // one block per cell
  }

  constexpr Vector3 PLAYER_SCALE = {1.5f, 10.0f, 1.5f};

  BoundingBox player;
  player.min = Vector3Subtract(
      playerPos, {PLAYER_SCALE.x * 0.5f, 0.0f, PLAYER_SCALE.z * 0.5f});
  player.max = Vector3Add(player.min, PLAYER_SCALE);

  // cell is the block's centre (see objectBox / snapToCell), not a corner.
  BoundingBox block = objectBox(ObjectTransform{cell, blockSize});

  if (!CheckCollisionBoxes(player, block)) {
    client.placeObject(Object{-1, ObjectTransform{cell, blockSize}, colors[activeColor]});
    return true;
  } else {
    return false;
  }
}

// ==== chunks ==== //
static int64_t chunkKey(Vector3 pos) {
  int cx = (int)floorf(pos.x / World::CHUNK_SIZE);
  int cz = (int)floorf(pos.z / World::CHUNK_SIZE);
  return (static_cast<int64_t>(cx) << 32) ^ static_cast<uint32_t>(cz);
}

void World::markDirty(Vector3 pos) {
  dirtyChunks.insert(chunkKey(pos));
  // Chunks span all y, so only the horizontal neighbours can be in another chunk.
  const Vector3 offsets[] = {{blockSize.x, 0, 0}, {-blockSize.x, 0, 0}, {0, 0, blockSize.z}, {0, 0, -blockSize.z}};
  for (const Vector3 &o : offsets) {
    dirtyChunks.insert(chunkKey(Vector3Add(pos, o)));
  }
}

const std::vector<int> *World::getChunk(int64_t key) const {
  auto it = chunks.find(key);
  return it == chunks.end() ? nullptr : &it->second;
}

// ==== object bookkeeping ==== //
void World::indexObject() {
  Vector3 pos = objects.back().getTransform().pos;
  int idx     = (int)objects.size() - 1;

  occupiedCells[cellKey(pos)] = idx;
  chunks[chunkKey(pos)].push_back(idx);
  markDirty(pos);
}

void World::addObject(const Object &object) {
  objects.push_back(object);
  indexObject();
}

void World::addObjects(const std::vector<Object> &newObjects) {
  objects.reserve(objects.size() + newObjects.size());
  for (const Object &o : newObjects) {
    objects.push_back(o);
    indexObject();
  }
}

void World::removeObject(Vector3 pos) {
  auto it = occupiedCells.find(cellKey(pos));
  if (it == occupiedCells.end())
    return;

  int removedIdx = (int)(it->second);
  int lastIdx    = (int)objects.size() - 1;

  occupiedCells.erase(cellKey(pos));
  std::vector<int> &list = chunks[chunkKey(pos)];
  std::erase(list, removedIdx);
  if (list.empty())
    chunks.erase(chunkKey(pos));
  markDirty(pos);

  // Swap-and-pop instead of erase, so only the moved object's index needs
  // fixing up - in occupiedCells and in its chunk - not every index after it.
  if (removedIdx != lastIdx) {
    Vector3 movedPos    = objects[lastIdx].getTransform().pos;
    objects[removedIdx] = objects[lastIdx];

    occupiedCells[cellKey(movedPos)] = removedIdx;
    std::vector<int> &movedList      = chunks[chunkKey(movedPos)];
    std::replace(movedList.begin(), movedList.end(), lastIdx, removedIdx);
    dirtyChunks.insert(chunkKey(movedPos)); // its index changed, so the renderer must relist it
  }
  objects.pop_back();
}

void World::damageObject(Vector3 pos) {
  auto it = occupiedCells.find(cellKey(pos));
  if (it == occupiedCells.end())
    return;

  objects[it->second].damage();
}

void World::clear() {
  for (const auto &[key, blocks] : chunks) {
    dirtyChunks.insert(key);
  }
  chunks.clear();
  occupiedCells.clear();
  objects.clear();
}

std::vector<Object> &World::getObjects() {
  return objects;
}

bool World::isOccluded(const Object &o) const {
  Vector3 pos = o.getTransform().pos;
  // +x
  if (!occupiedCells.contains(cellKey(Vector3Add(pos, {blockSize.x, 0, 0}))))
    return false;
  // -x
  if (!occupiedCells.contains(cellKey(Vector3Add(pos, {-blockSize.x, 0, 0}))))
    return false;
  // +y
  if (!occupiedCells.contains(cellKey(Vector3Add(pos, {0, blockSize.y, 0}))))
    return false;
  // -y
  if (!occupiedCells.contains(cellKey(Vector3Add(pos, {0, -blockSize.y, 0}))))
    return false;
  // +z
  if (!occupiedCells.contains(cellKey(Vector3Add(pos, {0, 0, blockSize.z}))))
    return false;
  // -z
  if (!occupiedCells.contains(cellKey(Vector3Add(pos, {0, 0, -blockSize.z}))))
    return false;
  return true;
}

bool World::boxCollides(BoundingBox box) const {
  // Blocks are one-per-cell on the fixed blockSize grid, so only the cells
  // box's own extent spans can possibly contain a hit.
  int minX = (int)floorf(box.min.x / blockSize.x);
  int maxX = (int)floorf(box.max.x / blockSize.x);
  int minY = (int)floorf(box.min.y / blockSize.y);
  int maxY = (int)floorf(box.max.y / blockSize.y);
  int minZ = (int)floorf(box.min.z / blockSize.z);
  int maxZ = (int)floorf(box.max.z / blockSize.z);

  for (int y = minY; y <= maxY; y++) {
    for (int z = minZ; z <= maxZ; z++) {
      for (int x = minX; x <= maxX; x++) {
        Vector3 cellPos = {(x + 0.5f) * blockSize.x, (y + 0.5f) * blockSize.y, (z + 0.5f) * blockSize.z};
        auto it         = occupiedCells.find(cellKey(cellPos));
        if (it == occupiedCells.end())
          continue;
        if (CheckCollisionBoxes(box, objectBox(objects[it->second].getTransform())))
          return true;
      }
    }
  }
  return false;
}