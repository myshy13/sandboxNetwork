#pragma once

#include <raylib.h>

#include <array>
#include <cstddef>

enum class Tex {
  Heart,
  Grass,
  Dirt,
  Water,
  Leaves,
  Wood,
  Grass_Side,
  Planks,
  Glass,
  Count  // keep last: sizes the array and the path table
};

enum class Fon { PressStart2P, Count };

// Owns every loaded asset. Needs an open window, and must be destroyed before
// CloseWindow().
class AssetManager {
 public:
  AssetManager();
  ~AssetManager();
  AssetManager(const AssetManager&) = delete;
  AssetManager& operator=(const AssetManager&) = delete;

  const Texture2D& get(Tex name) const;
  const Font& get(Fon name) const;

 private:
  std::array<Texture2D, static_cast<size_t>(Tex::Count)> textures{};
  std::array<Font, static_cast<size_t>(Fon::Count)> fonts{};
};
