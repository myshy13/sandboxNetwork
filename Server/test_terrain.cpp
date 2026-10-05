#include "Terrain/terrain.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <climits>
#include <iostream>
#include <map>

int main() {
  // init
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count();
  srand(ms);

  Terrain terrain1(rand());
  Terrain terrain2(terrain1.seed());
  Terrain terrain3(terrain1.seed() + 1);

  // test the tests
  assert(1 == 1);

  // generation
  assert(terrain1.heightAt(5, -7) == terrain2.heightAt(5, -7));
  assert(terrain1.heightAt(-43, 2) == terrain2.heightAt(-43, 2));

  std::map<int, int> heightCount;

  int differenceCount = 0;
  int totalDifference = 0;
  int height1max = INT_MIN;
  int height1min = INT_MAX;

  for (int x = -200; x <= 200; x++) {
    for (int z = -200; z <= 200; z++) {
      int height1 = terrain1.heightAt(x, z);
      int height3 = terrain3.heightAt(x, z);

      int difference = height1 - height3;
      totalDifference += difference > 0 ? difference : -difference;

      if (height1 != height3) {
        differenceCount++;
      }

      // min/max
      height1min = std::min(height1min, height1);
      height1max = std::max(height1max, height1);

      heightCount[height1]++;
    }
  }
  assert(totalDifference > 10);
  assert(differenceCount > 10);

  std::cout << "TERRAIN INFO:\n";
  std::cout << "|- Height ---- | - Count -|\n";
  for (auto [height, count] : heightCount) {
    std::cout << "| " << height << " | " << count << " |\n";
  }
  std::cout << "|-------------------------|\n";

  // Heights must be in range [1, 22] (see heightAt)
  assert(height1min >= 1 && height1max <= 22);
  assert(height1max - height1min >= 5); // not flat

  // Neighbours barely differ (this would catch integer division)
  for (int x = -50; x < 50; x++) {
    for (int z = -50; z < 50; z++) {
      int h = terrain1.heightAt(x, z);
      int h1 = terrain1.heightAt(x + 1, z);
      int h2 = terrain1.heightAt(x, z + 1);
      assert(abs(h - h1) <= 3 && abs(h - h2) <= 3);
    }
  }

  assert(terrain1.seed() + 1 == terrain3.seed());

  // 3D hash: deterministic, seeded, and every axis changes the result
  assert(terrain1.hash(3, 4, 5) == terrain2.hash(3, 4, 5));
  assert(terrain1.hash(3, 4, 5) != terrain3.hash(3, 4, 5));
  assert(terrain1.hash(3, 4, 5) != terrain1.hash(4, 4, 5));
  assert(terrain1.hash(3, 4, 5) != terrain1.hash(3, 5, 5));
  assert(terrain1.hash(3, 4, 5) != terrain1.hash(3, 4, 6));
  assert(terrain1.hash(-3, -4, -5) == terrain2.hash(-3, -4, -5)); // negatives

  // Layers must not repeat each other: a column's damage rolls (y = 0..9) are
  // different sequences in neighbouring columns, and roughly even over 0..2.
  {
    int same = 0, total = 0;
    int bucket[3] = {};
    for (int x = -20; x < 20; x++) {
      for (int z = -20; z < 20; z++) {
        for (int y = 0; y < 10; y++) {
          bucket[terrain1.hash(x, y, z) % 3]++;
          // a layer above/beside should only match by chance (about 1 in 3)
          same += (terrain1.hash(x, y, z) % 3) == (terrain1.hash(x + 1, y, z) % 3);
          same += (terrain1.hash(x, y, z) % 3) == (terrain1.hash(x, y + 1, z) % 3);
          total += 2;
        }
      }
    }
    for (int b : bucket) {
      assert(b > 16000 * 0.30 && b < 16000 * 0.37); // 16000 rolls, ~1/3 each
    }
    assert(same > total * 0.30 && same < total * 0.37);
  }

  printf("terrain tests passed\n");
}