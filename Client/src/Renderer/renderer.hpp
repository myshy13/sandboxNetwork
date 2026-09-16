#pragma once

#include "Models/Object.hpp"
#include "Renderer/frustum.hpp"
#include "Shaders/lighting.hpp"
#include "World/world.hpp"
#include <raylib.h>
#include <unordered_map>
#include <vector>

class Renderer {
public:
  Renderer();
  ~Renderer();

  Object *drawObjects(std::vector<Object> &objects,
                      int objectsVersion,
                      const World &world,
                      const Ray &facing,
                      const Lighting &lighting,
                      const Camera3D &camera);
  size_t getLastDrawnCount() const { return instanceMats.size(); }

private:
  Mesh cubeMesh;
  Material cubeMat;

  unsigned int transformVBO = 0;
  unsigned int colorVBO     = 0;
  size_t bufferCapacity     = 0;

  std::vector<Matrix> instanceMats;
  std::vector<Vector4> instanceColors;

  // Coarse spatial index over `objects`, so a whole cell of blocks can be
  // frustum-culled without testing each block individually. Rebuilt only
  // when World::getVersion() changes (a block is placed/removed), not per frame.
  struct GridCell {
    std::vector<int> indices; // into the objects vector passed to drawObjects
    BoundingBox bounds{};
  };
  static constexpr float CELL_SIZE = 25.0f; // world units per grid cell (~5 blocks)
  std::unordered_map<int64_t, GridCell> grid;
  int cachedVersion = -1;

  void rebuildGrid(const std::vector<Object> &objects, const World &world);
  void ensureBufferCapacity(size_t count, int transformLoc, int colorLoc);
  static bool boxInFrustum(const Frustum &f, BoundingBox box);
};