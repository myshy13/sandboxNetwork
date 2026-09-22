#include "terrain.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

Terrain::Terrain(uint32_t seed) : seedValue(seed) {}

// Multiply-add and modulo a prime instead of bit tricks; uint64_t so nothing overflows.
uint32_t Terrain::hash(int x, int z) const {
  constexpr uint64_t PRIME = 4294967291ULL; // largest prime below 2^32
  uint64_t h = seedValue;
  h = (h * 1103515245ULL + static_cast<uint32_t>(x) * 73856093ULL + 12345ULL) % PRIME;
  h = (h * 1103515245ULL + static_cast<uint32_t>(z) * 19349663ULL + 12345ULL) % PRIME;
  h = (h * h + 12345ULL) % PRIME; // squaring breaks the straight-line patterns
  return static_cast<uint32_t>(h);
}

uint32_t Terrain::lattice(int gx, int gz, uint32_t octave) const {
  return hash(gx + static_cast<int>(octave) * 1013, gz + static_cast<int>(octave) * 7919);
}

// x and z are in grid units; returns a smooth value in [0, 1).
float Terrain::noise(float x, float z, uint32_t octave) const {
  const int gx = static_cast<int>(floorf(x)); // floorf, not a plain cast: negatives round the wrong way
  const int gz = static_cast<int>(floorf(z));
  const float fx = x - gx;
  const float fz = z - gz;

  // The four corners, as fractions in [0, 1).
  const float a = lattice(gx, gz, octave) / 4294967296.0f;
  const float b = lattice(gx + 1, gz, octave) / 4294967296.0f;
  const float c = lattice(gx, gz + 1, octave) / 4294967296.0f;
  const float d = lattice(gx + 1, gz + 1, octave) / 4294967296.0f;

  const float u = fx * fx * (3 - 2 * fx); // smoothstep: no sharp creases at the grid lines
  const float v = fz * fz * (3 - 2 * fz);

  const float top = a + (b - a) * u;
  const float bottom = c + (d - c) * u;
  return top + (bottom - top) * v;
}

int Terrain::heightAt(int cellX, int cellZ) const {
  float total = 0.0f;
  float wavelength = 32.0f; // cells per hill at this layer
  float amplitude = 12.0f;  // how tall the hills are
  for (uint32_t octave = 0; octave < 3; octave++) {
    total += noise(cellX / wavelength, cellZ / wavelength, octave) * amplitude;
    wavelength /= 2;
    amplitude /= 2;
  }
  return std::max(1, static_cast<int>(total) + 1);
}

uint32_t Terrain::seed() const {
  return seedValue;
}
