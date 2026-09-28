#pragma once

#include "Models/Object.hpp"
#include "cereal/archives/binary.hpp"
#include "cereal/details/helpers.hpp"
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <optional>
#include <string>
#include <iostream>
#include <utility>
#include <vector>

// Packs a chunk's (x, z) coordinates into one hashable key: x in the high 32
// bits, z in the low 32. A chunk spans every height, so there is no y.
inline int64_t chunkKey(int cx, int cz) {
  // The unsigned cast keeps a negative cz from sign-extending over the x half.
  return (static_cast<int64_t>(cx) << 32) | static_cast<uint32_t>(cz);
}

// Inverse of chunkKey.
inline std::pair<int, int> chunkCoords(int64_t key) {
  return {static_cast<int>(key >> 32), static_cast<int32_t>(key)};
}

struct SavedChunk {
  uint32_t formatVersion;
  std::vector<Object> blocks;
  template <class A> void serialize(A &ar) { ar(formatVersion, blocks); }
};

inline std::string chunkFilePath(std::string savePath, int cx, int cz) {
  return savePath + "/chunks/c." + std::to_string(cx) + "." +
         std::to_string(cz) + ".bin";
}

inline bool writeChunkFile(std::string path, SavedChunk &chunk) {
  try {
    std::filesystem::create_directories(
        std::filesystem::path(path).parent_path());
    std::ofstream os(path + ".tmp", std::ios::binary);
    {
      cereal::BinaryOutputArchive ar(os);
      ar(chunk);
    }

    std::filesystem::rename(path + ".tmp", path);
  } catch (const cereal::Exception &e) {
    std::cerr << "ERR: Failed to save chunk at path: " << path << "\n";
    std::cerr << e.what() << "\n";
    return false;
  }
  return true;
}

inline std::optional<SavedChunk> readChunkFile(std::string path) {
  std::ifstream is(path, std::ios::binary);
  if (!is.is_open())
    return std::nullopt;
  try {
    cereal::BinaryInputArchive ar(is);
    SavedChunk chunk;
    ar(chunk);
    return chunk;
  } catch (const cereal::Exception &e) {
    std::cerr << "ERR: Failed to read chunk at path: " << path << "\n";
    std::cerr << e.what() << "\n";
    return std::nullopt;
  }
}