#pragma once

#include <array>
#include <raylib.h>

enum class Tex {
  Heart,
  Count // keep last: sizes the array and the path table
};

// Owns every loaded asset. Needs an open window, and must be destroyed before CloseWindow().
class AssetManager {
public:
  AssetManager();
  ~AssetManager();
  AssetManager(const AssetManager &)            = delete;
  AssetManager &operator=(const AssetManager &) = delete;

  const Texture2D &get(Tex name) const;

private:
  std::array<Texture2D, static_cast<size_t>(Tex::Count)> textures{};
};
