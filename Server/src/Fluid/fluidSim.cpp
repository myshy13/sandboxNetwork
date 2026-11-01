#include "fluidSim.hpp"
#include "Models/blocks.hpp"
#include "Server/blockHelpers.hpp"
#include "env.hpp"
#include "raymath.h"
#include <algorithm>
#include <cstddef>
#include <utility>

void FluidSim::tick(float dt, const FluidWorld &world) {
  flowAccumulator += dt;
  if (flowAccumulator < env::FLOW_INTERVAL)
    return;
  flowAccumulator -= env::FLOW_INTERVAL;
  passCount++;

  // Cells woken during this pass wait for the next one, so flow moves one cell
  // per pass in every direction.
  const std::set<int64_t> current = std::exchange(activeCells, {});
  for (int64_t cellKey : current) {
    processCell(cellKey, world, activeCells);
  }
};

void FluidSim::processCell(int64_t cellKey, const FluidWorld world,
                           std::set<int64_t> &nextActive) {
  auto it = world.occupiedCells.find(cellKey);
  if (it == world.occupiedCells.end())
    return;
  Object &occupant = world.objects[it->second];
  // Each fluid only feeds, rests on and spreads as itself.
  const BlockType type = occupant.getType();
  const Vector3 pos = occupant.getTransform().pos;

  if (!isFluid(type))
    return;
  // ==== lava specific checks ==== //
  if (type == BlockType::Lava) {
    for (Vector3 offset :
         {Vector3{BLOCK_SIZE, 0, 0}, Vector3{-BLOCK_SIZE, 0, 0},
          Vector3{0, 0, BLOCK_SIZE}, Vector3{0, 0, -BLOCK_SIZE},
          Vector3{0, -BLOCK_SIZE, 0}, Vector3{0, BLOCK_SIZE, 0}}) {
      auto neighbourIt =
          world.occupiedCells.find(blockKey(Vector3Add(pos, offset)));
      if (neighbourIt == world.occupiedCells.end())
        continue;
      BlockType neighbourType = world.objects[neighbourIt->second].getType();
      if (neighbourType == BlockType::Water) {
        world.replaceBlock(it->second, BlockType::Stone);
        return;
      }
    }
  }

  // ==== water specific checks ==== //
  if (type == BlockType::Water) {
    for (Vector3 offset :
         {Vector3{BLOCK_SIZE, 0, 0}, Vector3{-BLOCK_SIZE, 0, 0},
          Vector3{0, 0, BLOCK_SIZE}, Vector3{0, 0, -BLOCK_SIZE},
          Vector3{0, -BLOCK_SIZE, 0}, Vector3{0, BLOCK_SIZE, 0}}) {
      auto neighbourIt =
          world.occupiedCells.find(blockKey(Vector3Add(pos, offset)));
      if (neighbourIt == world.occupiedCells.end())
        continue;
      BlockType neighbourType = world.objects[neighbourIt->second].getType();
      if (neighbourType == BlockType::Lava) {
        world.replaceBlock(neighbourIt->second, BlockType::Stone);
        processCell(cellKey, world, nextActive);
        return;
      }
    }
  }

  if (passCount % BLOCK_INFO[static_cast<size_t>(type)].flowEvery != 0) {
    nextActive.insert(cellKey); // needs to be processed again next pass
    return;
  }

  const int level = occupant.getState();

  const int maxLevel = flowDistance(type);

  // Wakes the cells this one can feed: its four sides and the cell below.
  auto wakeNeighbours = [&]() {
    for (Vector3 offset :
         {Vector3{BLOCK_SIZE, 0, 0}, Vector3{-BLOCK_SIZE, 0, 0},
          Vector3{0, 0, BLOCK_SIZE}, Vector3{0, 0, -BLOCK_SIZE},
          Vector3{0, -BLOCK_SIZE, 0}}) {
      nextActive.insert(blockKey(Vector3Add(pos, offset)));
    }
  };

  const Vector3 belowPos = Vector3Subtract(pos, {0, BLOCK_SIZE, 0});
  const int64_t belowKey = blockKey(belowPos);

  // Never fall below the world floor - nothing generates down there, so an
  // open drop at the edge of generated terrain would otherwise fall forever.
  if (belowPos.y >= 0 && !world.occupiedCells.contains(belowKey)) {
    // Fallen water is fed from above, never a source itself; keep its strength.
    world.setFluidLevel(belowPos, type, std::max(level, 1));
    nextActive.insert(belowKey);
    nextActive.insert(cellKey);
    return;
  }

  int newLevel = level;
  if (level != SOURCE) {
    // 2x adjacent source checking
    int fedCount = 0;

    // Strongest level this cell can be fed: side water at n gives n + 1, water
    // above gives its own strength.
    int fedLevel = maxLevel + 1; // past maxLevel = nothing feeds it
    auto checkFeeder = [&](Vector3 offset, bool above) {
      auto n = world.occupiedCells.find(blockKey(Vector3Add(pos, offset)));
      if (n == world.occupiedCells.end())
        return;
      const Object &neighbour = world.objects[n->second];
      if (neighbour.getType() != type)
        return;
      // A side neighbour only feeds if it's currently stronger than this cell -
      // otherwise two equal-strength neighbours could prop each other up
      // forever instead of ever draining (this is what let a whole pond hang at
      // max level and vanish in one pass, all at once, once its source was
      // removed). Water above always feeds: it's genuinely upstream.
      if (!above && neighbour.getState() >= level)
        return;
      const int feeds = above ? std::max<int>(neighbour.getState(), 1)
                              : neighbour.getState() + 1;
      fedLevel = std::min(fedLevel, feeds);
      if (!above && neighbour.getState() == SOURCE)
        fedCount++;
    };

    checkFeeder({0, BLOCK_SIZE, 0}, true);
    checkFeeder({BLOCK_SIZE, 0, 0}, false);  // +x
    checkFeeder({-BLOCK_SIZE, 0, 0}, false); // -x
    checkFeeder({0, 0, BLOCK_SIZE}, false);  // +z
    checkFeeder({0, 0, -BLOCK_SIZE}, false); // -z

    // Two side sources make this a source too, but only resting on solid ground
    // or another source - otherwise a gap between two sources over a drop would
    // grow a source hanging in mid-air instead of staying a waterfall.
    auto belowIt = world.occupiedCells.find(belowKey);
    const bool supported =
        belowIt != world.occupiedCells.end() &&
        (world.objects[belowIt->second].getType() != type ||
         world.objects[belowIt->second].getState() == SOURCE);
    if (fedCount >= 2 && supported) {
      world.setFluidLevel(pos, type, SOURCE);
      wakeNeighbours();
      nextActive.insert(cellKey);
      return;
    }

    const bool fed = fedLevel <= maxLevel;
    // Unfed water weakens a step per pass; fed water takes whatever
    // strength its best feeder gives.
    newLevel = fed ? fedLevel : level + 1;

    if (newLevel > maxLevel) {
      world.removeBlock(it->second); // `it` is stale from here on
      wakeNeighbours();
      return;
    }
    if (newLevel != level) {
      world.setFluidLevel(pos, type, newLevel);
      nextActive.insert(cellKey);
      wakeNeighbours();
    }
    if (!fed)
      return; // draining water doesn't spread
  }

  // Flowing water only spreads once it lands on something solid, so a
  // waterfall falls straight down; a source always spreads.
  auto below = world.occupiedCells.find(belowKey);
  const bool onGround =
      belowPos.y < 0 || (below != world.occupiedCells.end() &&
                         world.objects[below->second].getType() != type);
  if (newLevel + 1 > maxLevel || (newLevel != SOURCE && !onGround))
    return;

  auto spreadTo = [&](Vector3 offset) {
    const Vector3 neighbourPos = Vector3Add(pos, offset);
    const int64_t neighbourKey = blockKey(neighbourPos);
    if (world.occupiedCells.contains(neighbourKey))
      return; // only empty cells take a new flow
    world.setFluidLevel(neighbourPos, type, newLevel + 1);
    nextActive.insert(neighbourKey);
  };

  spreadTo({BLOCK_SIZE, 0, 0});  // +x
  spreadTo({-BLOCK_SIZE, 0, 0}); // -x
  spreadTo({0, 0, BLOCK_SIZE});  // +z
  spreadTo({0, 0, -BLOCK_SIZE}); // -z
};
