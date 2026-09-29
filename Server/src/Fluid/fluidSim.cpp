#include "fluidSim.hpp"
#include "Server/blockHelpers.hpp"
#include "env.hpp"
#include "raymath.h"

void FluidSim::tick(float dt, const FluidWorld &world) {
  flowAccumulator += dt;
  if (flowAccumulator < env::FLOW_INTERVAL)
    return;
  flowAccumulator -= env::FLOW_INTERVAL;
  std::set<int64_t> nextActive{};
  for (int64_t cellKey : activeCells) {
    processCell(cellKey, world, nextActive);
  }
  activeCells = nextActive;
};

void FluidSim::processCell(int64_t cellKey, const FluidWorld world,
                           std::set<int64_t> &nextActive) {
  auto it = world.occupiedCells.find(cellKey);
  if (it == world.occupiedCells.end())
    return;
  Object occupant = world.objects[it->second];
  if (occupant.getType() != BlockType::Water)
    return;

  Vector3 pos = occupant.getTransform().pos;
  int level = occupant.getLevel();

  Vector3 belowPos = Vector3Subtract(pos, {0, BLOCK_SIZE, 0});
  int64_t belowKey = blockKey(belowPos);

  // Never fall below the world floor - nothing generates down there, so an
  // open drop at the edge of generated terrain would otherwise fall forever.
  if (belowPos.y >= 0 && !world.occupiedCells.contains(belowKey)) {
    world.setWaterLevel(belowPos, level);
    nextActive.insert(belowKey);
    nextActive.insert(cellKey);
    return;
  }

  if (level != 0) {
    int64_t fed =
        -1; // sentinel: no feeding neighbour found yet (not a valid level)

    auto checkFeeder = [&](Vector3 offset, bool anyLevel) {
      int64_t neighbourKey = blockKey(Vector3Add(pos, offset));
      auto n = world.occupiedCells.find(neighbourKey);
      if (n == world.occupiedCells.end())
        return;
      const Object &neighbour = world.objects[n->second];
      if (neighbour.getType() != BlockType::Water)
        return;
      if (anyLevel || neighbour.getLevel() == level - 1)
        fed = level - 1;
    };

    checkFeeder({0, BLOCK_SIZE, 0},
                true); // vertical: water above feeds regardless of its level
    checkFeeder({BLOCK_SIZE, 0, 0}, false);  // +x
    checkFeeder({-BLOCK_SIZE, 0, 0}, false); // -x
    checkFeeder({0, 0, BLOCK_SIZE}, false);  // +z
    checkFeeder({0, 0, -BLOCK_SIZE}, false); // -z

    if (fed == -1) {
      if (level + 1 > MAX_LEVEL) {
        world.removeBlock(it->second);
      } else {
        world.setWaterLevel(pos, level + 1);
        nextActive.insert(cellKey);
      }
    }
  }

  // Spread outward into empty neighbours. Runs for source cells too (a
  // source never changes itself, level != 0 above, but always keeps trying
  // to spread), so it's outside the if (level != 0) block above.
  if (level + 1 <= MAX_LEVEL) {
    auto spreadTo = [&](Vector3 offset) {
      Vector3 neighbourPos = Vector3Add(pos, offset);
      int64_t neighbourKey = blockKey(neighbourPos);
      if (world.occupiedCells.contains(neighbourKey))
        return; // only empty cells take a new flow
      world.setWaterLevel(neighbourPos, level + 1);
      nextActive.insert(neighbourKey);
    };

    spreadTo({BLOCK_SIZE, 0, 0});  // +x
    spreadTo({-BLOCK_SIZE, 0, 0}); // -x
    spreadTo({0, 0, BLOCK_SIZE});  // +z
    spreadTo({0, 0, -BLOCK_SIZE}); // -z
  }
};