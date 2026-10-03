#include "Fluid/fluidSim.hpp"
#include "Server/blockHelpers.hpp"
#include "Terrain/structures.hpp"
#include "env.hpp"
#include "raymath.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <unordered_set>

// ==== checks keep going, so one run lists every failure ==== //
int failures = 0;

void check(bool ok, const char *name) {
  std::printf("%s  %s\n", ok ? "pass" : "FAIL", name);
  if (!ok)
    failures++;
}

// ==== stand-in for the Server's block store ==== //
// Same contract as Server::addBlock / removeBlock / setWaterLevel: one block
// per cell, swap-and-pop removal, water neighbours woken when a block is
// removed.
class TestWorld {
public:
  std::vector<Object> objects;
  std::unordered_map<int64_t, int> occupied;
  FluidSim sim;

  static Vector3 centre(int x, int y, int z) {
    return {(x + 0.5f) * BLOCK_SIZE, (y + 0.5f) * BLOCK_SIZE,
            (z + 0.5f) * BLOCK_SIZE};
  }

  void add(int x, int y, int z, BlockType type, uint8_t level = 0,
           bool activate = true) {
    Object o(nextId++, ObjectTransform{centre(x, y, z), blockSize}, type);
    o.setLevel(level);
    addObject(o, activate);
  }

  // A (2r+1) x (2r+1) solid slab at cell height y, centred on x = z = 0.
  void floor(int radius, int y) {
    for (int x = -radius; x <= radius; x++)
      for (int z = -radius; z <= radius; z++)
        add(x, y, z, BlockType::Dirt);
  }

  const Object *at(int x, int y, int z) const {
    auto it = occupied.find(cellKey(x, y, z));
    return it == occupied.end() ? nullptr : &objects[it->second];
  }

  bool waterAt(int x, int y, int z) const {
    const Object *o = at(x, y, z);
    return o && o->getType() == BlockType::Water;
  }

  int waterCount() const {
    int n = 0;
    for (const Object &o : objects)
      n += o.getType() == BlockType::Water;
    return n;
  }

  // Cell key -> level of every water block, to compare two moments.
  std::map<int64_t, int> waterSnapshot() const {
    std::map<int64_t, int> snap;
    for (const Object &o : objects)
      if (o.getType() == BlockType::Water)
        snap[blockKey(o.getTransform().pos)] = o.getLevel();
    return snap;
  }

  void removeAt(int x, int y, int z) {
    removeBlock(occupied.at(cellKey(x, y, z)));
  }

  FluidWorld world() { return FluidWorld{objects, occupied, setFn, removeFn}; }

  // Runs `passes` full flow passes.
  void step(int passes = 1) {
    for (int i = 0; i < passes; i++)
      sim.tick(env::FLOW_INTERVAL, world());
  }

private:
  int nextId = 1;
  std::function<void(Vector3, int)> setFn = [this](Vector3 pos, int level) {
    setWaterLevel(pos, level);
  };
  std::function<void(int)> removeFn = [this](int index) { removeBlock(index); };

  void addObject(const Object &o, bool activate) {
    objects.push_back(o);
    occupied[blockKey(o.getTransform().pos)] = (int)objects.size() - 1;
    if (activate && o.getType() == BlockType::Water)
      sim.markActive(blockKey(o.getTransform().pos));
  }

  void setWaterLevel(Vector3 pos, int level) {
    auto it = occupied.find(blockKey(pos));
    if (it == occupied.end()) {
      Object o(nextId++, ObjectTransform{pos, blockSize}, BlockType::Water);
      o.setLevel((uint8_t)level);
      addObject(o, true);
      return;
    }
    if (objects[it->second].getType() == BlockType::Water)
      objects[it->second].setLevel((uint8_t)level);
  }

  void removeBlock(int index) {
    const Vector3 pos = objects[index].getTransform().pos;
    occupied.erase(blockKey(pos));
    const int last = (int)objects.size() - 1;
    if (index != last) {
      objects[index] = objects[last];
      occupied[blockKey(objects[index].getTransform().pos)] = index;
    }
    objects.pop_back();

    const Vector3 offsets[] = {{BLOCK_SIZE, 0, 0},
                               {-BLOCK_SIZE, 0, 0},
                               {0, 0, BLOCK_SIZE},
                               {0, 0, -BLOCK_SIZE},
                               {0, BLOCK_SIZE, 0}};
    for (Vector3 offset : offsets) {
      const int64_t key = blockKey(Vector3Add(pos, offset));
      auto it = occupied.find(key);
      if (it != occupied.end() &&
          objects[it->second].getType() == BlockType::Water)
        sim.markActive(key);
    }
  }
};

// ==== cell keys ==== //
void testCellKeys() {
  bool centresMatch = true;
  for (int x = -20; x <= 20; x++)
    for (int y = -3; y <= 20; y++)
      for (int z = -20; z <= 20; z++)
        centresMatch &=
            blockKey(TestWorld::centre(x, y, z)) == cellKey(x, y, z);
  check(centresMatch,
        "blockKey(cell centre) == cellKey(cell), negatives included");

  check(blockKey({-0.1f, 0.1f, -0.1f}) == cellKey(-1, 0, -1),
        "blockKey floors negatives instead of truncating toward 0");

  std::unordered_set<int64_t> seen;
  bool unique = true;
  for (int x = -50; x <= 50; x++)
    for (int y = -5; y <= 30; y++)
      for (int z = -50; z <= 50; z++)
        unique &= seen.insert(cellKey(x, y, z)).second;
  check(unique, "no two cells share a key");
}

// ==== tick pacing ==== //
void testTickGating() {
  TestWorld w;
  w.floor(10, 0);
  w.add(0, 1, 0, BlockType::Water);

  w.sim.tick(env::FLOW_INTERVAL * 0.5f, w.world());
  check(w.waterCount() == 1, "no flow pass before FLOW_INTERVAL has passed");
  w.sim.tick(env::FLOW_INTERVAL * 0.5f, w.world());
  check(w.waterCount() == 5, "one flow pass spreads exactly one cell each way");
  const auto afterPass = w.waterSnapshot();
  w.sim.tick(0.01f, w.world());
  check(w.waterSnapshot() == afterPass,
        "accumulator resets after a pass (no pass every tick)");
}

// ==== sideways spread ==== //
void testSpreadsSideways() {
  TestWorld w;
  w.floor(10, 0);
  w.add(0, 1, 0, BlockType::Water);
  w.step();

  bool neighbours = true;
  for (auto [x, z] : {std::pair{1, 0}, {-1, 0}, {0, 1}, {0, -1}})
    neighbours &= w.waterAt(x, 1, z) && w.at(x, 1, z)->getLevel() == 1;
  check(neighbours,
        "a source on a floor spreads to its 4 neighbours at level 1");
  check(!w.waterAt(1, 1, 1) && !w.waterAt(0, 2, 0),
        "no diagonal or upward spread");
  check(w.at(0, 1, 0)->getLevel() == SOURCE, "a source keeps level 0");
}

void testSpreadCappedAtMaxLevel() {
  TestWorld w;
  w.floor(10, 0);
  w.add(0, 1, 0, BlockType::Water);
  w.step(30);

  bool levelsMatchDistance = true;
  for (const Object &o : w.objects) {
    if (o.getType() != BlockType::Water)
      continue;
    const Vector3 p = o.getTransform().pos;
    const int d = std::abs((int)floorf(p.x / BLOCK_SIZE)) +
                  std::abs((int)floorf(p.z / BLOCK_SIZE));
    levelsMatchDistance &= d <= MAX_LEVEL && o.getLevel() == d;
  }
  check(levelsMatchDistance, "on flat ground, level == distance from the "
                             "source, never past MAX_LEVEL");
  check(w.waterCount() == 2 * MAX_LEVEL * (MAX_LEVEL + 1) + 1,
        "pool is exactly the diamond MAX_LEVEL cells wide");
}

void testSolidBlocksFlow() {
  TestWorld w;
  w.floor(20, 0);
  for (int z = -MAX_LEVEL - 2; z <= MAX_LEVEL + 2;
       z++) // long enough that going around costs more than MAX_LEVEL
    w.add(1, 1, z, BlockType::Dirt);
  w.add(0, 1, 0, BlockType::Water);
  w.step(30);

  check(w.at(1, 1, 0) && w.at(1, 1, 0)->getType() == BlockType::Dirt,
        "water never replaces a solid block");
  check(!w.waterAt(2, 1, 0),
        "water can't flow through a wall (the way around is past MAX_LEVEL)");
}

// ==== falling ==== //
void testFallStopsAtWorldFloor() {
  TestWorld w;
  w.add(0, 4, 0, BlockType::Water);
  w.step(40);

  bool aboveFloor = true;
  for (const Object &o : w.objects)
    aboveFloor &= o.getTransform().pos.y >= 0;
  check(aboveFloor, "water never falls below y = 0");
  check(w.waterAt(0, 0, 0), "water falls all the way to the bottom cell");
}

void testFallenWaterIsNotASource() {
  TestWorld w;
  w.floor(5, 0);
  w.add(0, 3, 0, BlockType::Water);
  w.step(5);

  check(w.waterAt(0, 2, 0), "water falls into the empty cell below");
  check(w.waterAt(0, 2, 0) && w.at(0, 2, 0)->getLevel() != SOURCE,
        "fallen water is not a new source (sources don't multiply)");
}

void testWaterfallFallsStraight() {
  TestWorld w;
  w.floor(10, 0);
  w.add(0, 8, 0, BlockType::Dirt); // one-block ledge high above the floor
  w.add(0, 9, 0,
        BlockType::Water); // source on it: its flow runs off every edge
  w.step(40);

  bool noSheet = true;
  for (int y = 2; y <= 7; y++)
    for (auto [x, z] : {std::pair{2, 0}, {0, 0}, {1, 1}, {1, -1}})
      noSheet &= !w.waterAt(x, y, z);
  check(noSheet, "a waterfall falls straight down instead of spreading "
                 "sideways in mid-air");
  check(w.waterAt(1, 1, 0), "the waterfall reaches the ground");
  check(w.waterAt(2, 1, 0) && w.waterAt(1, 1, 1), "it spreads once it lands");
}

void testTwoSourcesMergeSupportedHole() {
  TestWorld w;
  w.floor(6, 0);
  for (int x = -3; x <= 3; x++)
    for (int z = -3; z <= 3; z++)
      w.add(x, 1, z, BlockType::Water); // every cell a source: a pond
  w.step(6);
  w.removeAt(0, 1, 0); // punch a hole in the middle, resting on the floor
  w.step(3);
  check(w.waterAt(0, 1, 0) && w.at(0, 1, 0)->getLevel() == SOURCE,
        "a hole in a pond with 2+ side sources and solid ground below refills "
        "as a source, not flowing water");
}

void testTwoSourcesDoNotMergeOverADrop() {
  TestWorld w;
  w.add(-1, 9, 0, BlockType::Water); // source
  w.add(1, 9, 0,
        BlockType::Water); // source, 1 cell gap between them, nothing below
  w.step(10);
  check(!w.waterAt(0, 9, 0) || w.at(0, 9, 0)->getLevel() != SOURCE,
        "two sources either side of an open drop don't create a floating "
        "source (it stays a waterfall)");
}

// ==== strengthening ==== //
void testStrongerNeighbourFeeds() {
  TestWorld w;
  w.floor(5, 0);
  w.add(0, 1, 0, BlockType::Water);
  w.add(1, 1, 0, BlockType::Water, 2,
        true); // too weak for its spot, but next to a source
  w.step();
  check(w.waterAt(1, 1, 0) && w.at(1, 1, 0)->getLevel() == 1,
        "water next to a source is fed by it and strengthens to level 1");
}

// ==== decay ==== //
void testUnfedWaterDoesNotGrow() {
  TestWorld w;
  w.floor(5, 0);
  w.add(0, 1, 0, BlockType::Water, 1);
  w.step();
  check(w.waterCount() <= 1, "unfed flowing water decays instead of spreading");
  w.step(20);
  check(w.waterCount() == 0, "unfed flowing water drains away completely");
}

void testMaxLevelUnfedRemoved() {
  TestWorld w;
  w.floor(3, 0);
  w.add(0, 1, 0, BlockType::Water, MAX_LEVEL);
  w.step();
  check(w.waterCount() == 0, "unfed water at MAX_LEVEL is removed in one pass");
}

void testEqualNeighboursDoNotPropUpEachOther() {
  TestWorld w;
  w.floor(10, 0);
  w.add(0, 1, 0, BlockType::Water);
  w.step(20); // settle into a diamond, level == distance from the source
  w.removeAt(0, 1, 0);
  const int before = w.at(1, 1, 0)->getLevel();
  w.step();
  check(w.waterAt(1, 1, 0) && w.at(1, 1, 0)->getLevel() > before,
        "a cell whose only neighbours are the same level (or weaker) decays "
        "instead of staying fed forever");
}

void testDrainsAfterSourceRemoved() {
  TestWorld w;
  w.floor(10, 0);
  w.add(0, 1, 0, BlockType::Water);
  w.step(20);
  check(w.waterAt(0, 1, 0) && w.at(0, 1, 0)->getLevel() == SOURCE,
        "the source is still a source after settling");
  if (w.waterAt(0, 1, 0))
    w.removeAt(0, 1, 0);
  w.step(50);
  check(w.waterCount() == 0, "removing the source drains the whole pool");
}

void testConverges() {
  TestWorld w;
  w.floor(10, 0);
  w.add(0, 1, 0, BlockType::Water);
  w.step(30);
  const auto before = w.waterSnapshot();
  w.step(10);
  check(w.waterSnapshot() == before,
        "a pool fed by one source settles and stops changing");
}

int main() {
  testCellKeys();
  testTickGating();
  testSpreadsSideways();
  testSpreadCappedAtMaxLevel();
  testSolidBlocksFlow();
  testFallStopsAtWorldFloor();
  testFallenWaterIsNotASource();
  testWaterfallFallsStraight();
  testTwoSourcesMergeSupportedHole();
  testTwoSourcesDoNotMergeOverADrop();
  testStrongerNeighbourFeeds();
  testEqualNeighboursDoNotPropUpEachOther();
  testUnfedWaterDoesNotGrow();
  testMaxLevelUnfedRemoved();
  testDrainsAfterSourceRemoved();
  testConverges();

  std::printf(failures ? "%d fluid test(s) FAILED\n" : "fluid tests passed\n",
              failures);
  return failures ? 1 : 0;
}
