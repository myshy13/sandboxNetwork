#pragma once

#include "Models/Object.hpp"
#include "raylib.h"
#include "sharedEnv.hpp"
#include <cstdint>
#include <functional>
#include <set>
#include <unordered_map>
#include <vector>

constexpr int SOURCE = 0;
constexpr int MAX_LEVEL = SHARED_WATER_MAX_LEVEL; // past this, water is removed

struct FluidWorld {
  std::vector<Object> &objects;
  const std::unordered_map<int64_t, int> &occupiedCells;
  const std::function<void(Vector3 pos, int level)> &setWaterLevel;
  const std::function<void(int index)> &removeBlock;
};

class FluidSim {
private:
  float flowAccumulator{0.0f};
  std::set<int64_t> activeCells;

public:
  void markActive(int64_t cellKey) { activeCells.insert(cellKey); }

  void tick(float dt, const FluidWorld &world);

  void processCell(int64_t cellKey, const FluidWorld world,
                   std::set<int64_t> &nextActive);
};