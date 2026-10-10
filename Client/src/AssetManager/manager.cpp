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
    "assets/images/planks.png",      // wooden planks
    "assets/images/glass.png",       // glass
};

// Same order as Tex.
constexpr const char* FONT_PATHS[] = {
    "assets/fonts/pressStart2P.ttf",  // press start 2p
};

static_assert(std::size(TEXTURE_PATHS) == static_cast<size_t>(Tex::Count), "Tex and TEXTURE_PATHS are out of sync");

AssetManager::AssetManager() {
  for (size_t i = 0; i < textures.size(); i++) {
    textures[i] = LoadTexture(TEXTURE_PATHS[i]);
    SetTextureFilter(textures[i], TEXTURE_FILTER_POINT);
    if (!IsTextureValid(textures[i])) {
      // A missing file becomes an obvious magenta square instead of an
      // invisible bug.
      TraceLog(LOG_WARNING, "AssetManager: failed to load %s",
               TEXTURE_PATHS[i]);
      Image placeholder = GenImageChecked(64, 64, 8, 8, MAGENTA, BLACK);
      textures[i] = LoadTextureFromImage(placeholder);
      UnloadImage(placeholder);
    }
  }

  for (size_t i = 0; i < fonts.size(); i++) {
    fonts[i] = LoadFont(FONT_PATHS[i]);
    if (!IsFontValid(fonts[i])) {
      // A missing file becomes an obvious magenta square instead of an
      // invisible bug.
      TraceLog(LOG_WARNING, "AssetManager: failed to load %s",
               TEXTURE_PATHS[i]);

      // fallback font
      fonts[i] = GetFontDefault();
    }
  }
}

AssetManager::~AssetManager() {
  for (const Texture2D& t : textures) {
    UnloadTexture(t);
  }

  for (const Font& f : fonts) {
    UnloadFont(f);
  }
}

const Texture2D& AssetManager::get(Tex name) const {
  return textures[static_cast<size_t>(name)];
}

const Font& AssetManager::get(Fon name) const {
  return fonts[static_cast<size_t>(name)];
}
