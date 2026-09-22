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
                      World &world,
                      const Ray &facing,
                      const Lighting &lighting,
                      const Camera3D &camera);
  size_t getLastDrawnCount() const { return instanceMats.size(); }

  double getLastCullMs() const { return lastCullMs; }
  double getLastGpuMs() const { return lastGpuMs; }

private:
  // One quad, instanced once per *visible face*. Drawing whole cubes would put two
  // coincident faces at every block boundary, which z-fight at distance.
  Mesh faceMesh;
  Material cubeMat;
  // Rotations taking the quad's +Y normal onto each of the 6 face directions.
  static const Matrix FACE_ROT[6];
  static const Vector3 FACE_DIR[6];

  static constexpr int BUFFER_COUNT       = 2; // double-buffered: CPU writes one while GPU reads the other
  unsigned int transformVBO[BUFFER_COUNT] = {0, 0};
  unsigned int colorVBO[BUFFER_COUNT]     = {0, 0};
  size_t bufferCapacity[BUFFER_COUNT]     = {0, 0};
  int currentBuffer                       = 0;

  std::vector<Matrix> instanceMats;
  std::vector<Vector4> instanceColors;

  double lastCullMs = 0.0;
  double lastGpuMs  = 0.0;
  // Visible (non-occluded) blocks of one World chunk, so the whole chunk can be
  // frustum-culled at once. Keyed by the same chunk key World uses; only chunks
  // World reports dirty get rebuilt, never the whole map.
  struct GridCell {
    std::vector<int> indices; // into the objects vector passed to drawObjects
    // Parallel to `indices`: bit f set means face f has no neighbour, so it's drawn.
    std::vector<uint8_t> faceMasks;
    BoundingBox bounds{};
  };
  std::unordered_map<int64_t, GridCell> grid;

  void rebuildChunk(int64_t key, const std::vector<Object> &objects, const World &world);
  void ensureBufferCapacity(int slot, size_t count);
  static bool boxInFrustum(const Frustum &f, BoundingBox box);
};