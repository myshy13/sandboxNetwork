#pragma once

#include <cstdint>

class Terrain {
public:
  explicit Terrain(uint32_t seed);
  int heightAt(int cellX,
               int cellZ) const; // ground height in cells, at least 1
  uint32_t hash(int x, int z)
      const; // per-cell randomness (block damage now, tree placement later)
  uint32_t seed() const; // so the save can store it
private:
  uint32_t seedValue{};
  float noise(float x, float z, uint32_t octave) const;
  uint32_t lattice(int gx, int gz,
                   uint32_t octave) const; // hash of one lattice corner
};
