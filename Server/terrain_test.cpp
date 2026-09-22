#include "Server/terrain.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <climits>
#include <iostream>

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
    }
  }
  assert(totalDifference > 10);
  assert(differenceCount > 10);

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

  printf("terrain tests passed\n");
}