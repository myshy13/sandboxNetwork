#pragma once

#include "Models/Object.hpp"
#include "Models/blocks.hpp"
#include "raylib.h"
#include <cstdint>
#include <functional>
#include <set>
#include <unordered_map>
#include <vector>

constexpr int SOURCE = 0;
constexpr int MAX_LEVEL = BLOCK_INFO[static_cast<size_t>(BlockType::Water)]
                              .flowDistance; // past this, the fluid is removed

struct FluidWorld {
  std::vector<Object> &objects;
  const std::unordered_map<int64_t, int> &occupiedCells;
  // Creates or updates a cell of `type`; a no-op on any other block.
  const std::function<void(Vector3 pos, BlockType type, int state)>
      &setFluidLevel;
  const std::function<void(int index)> &removeBlock;
  const std::function<void(int index, BlockType type)> &replaceBlock;
};

class FluidSim {
private:
  float flowAccumulator{0.0f};
  int passCount{0};
  std::set<int64_t> activeCells;

public:
  void markActive(int64_t cellKey) { activeCells.insert(cellKey); }

  void tick(float dt, const FluidWorld &world);

  void processCell(int64_t cellKey, const FluidWorld world,
                   std::set<int64_t> &nextActive);
};