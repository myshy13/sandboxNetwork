#include "manager.hpp"

#include <iterator>
#include <raylib.h>

// Same order as Tex.
constexpr const char* TEXTURE_PATHS[] = {
    "assets/images/heart.png",       // heart
    "assets/images/grass.png",       // grass
    "assets/images/dirt.png",        // dirt
    "assets/images/water.png",       // water
    "assets/images/leaves.png",      // leaves
    "assets/images/wood.png",        // wood
    "assets/images/grass_side.png",  // grass side
};

static_assert(std::size(TEXTURE_PATHS) == static_cast<size_t>(Tex::Count), "Tex and TEXTURE_PATHS are out of sync");

AssetManager::AssetManager() {
  for (size_t i = 0; i < textures.size(); i++) {
    textures[i] = LoadTexture(TEXTURE_PATHS[i]);
    SetTextureFilter(textures[i], TEXTURE_FILTER_POINT);
    if (!IsTextureValid(textures[i])) {
      // A missing file becomes an obvious magenta square instead of an invisible bug.
      TraceLog(LOG_WARNING, "AssetManager: failed to load %s", TEXTURE_PATHS[i]);
      Image placeholder = GenImageChecked(64, 64, 8, 8, MAGENTA, BLACK);
      textures[i]       = LoadTextureFromImage(placeholder);
      UnloadImage(placeholder);
    }
  }
}

AssetManager::~AssetManager() {
  for (const Texture2D &t : textures) {
    UnloadTexture(t);
  }
}

const Texture2D &AssetManager::get(Tex name) const {
  return textures[static_cast<size_t>(name)];
}
