#pragma once

#include <raylib.h>

#include <array>
#include <unordered_map>
#include <vector>

#include "AssetManager/manager.hpp"
#include "Models/Object.hpp"
#include "Renderer/frustum.hpp"
#include "Shaders/lighting.hpp"
#include "World/world.hpp"

struct Batch {
  std::vector<Matrix> mats;
  std::vector<Vector4> colors;
};

class Renderer {
 public:
  Renderer(const AssetManager& a);
  ~Renderer();

  Object* drawObjects(std::vector<Object>& objects, World& world,
                      const Ray& facing, const Lighting& lighting,
                      const Camera3D& camera);

  size_t getLastDrawnCount() const {
    int total{0};
    for (const auto& batch : opaque) {
      total += batch.mats.size();
    }
    for (const auto& batch : translucent) {
      total += batch.mats.size();
    }
    return total;
  }

  double getLastCullMs() const { return lastCullMs; }

  double getLastGpuMs() const { return lastGpuMs; }

  double getLastShadowMapMs() const { return lastShadowMapMs; }

  void shadowMap(const std::vector<Object>& objects, Camera3D camera,
                 const Lighting& lighting, Vector3 toSun);

  const Texture2D& getShadowTexture() { return shadowMapTarget.texture; }

  const Texture2D& getShadowDepth() { return shadowMapTarget.depth; }

  const Matrix& getLightMatrix() { return lightMatrix; }

 private:
  RenderTexture2D shadowMapTarget;
  const AssetManager& assets;
  // One quad, instanced once per *visible face*. Drawing whole cubes would put
  // two coincident faces at every block boundary, which z-fight at distance.
  Mesh faceMesh;
  Material cubeMat;
  Texture whiteTex;
  // Rotations taking the quad's +Y normal onto each of the 6 face
  // directions.
  static const Matrix FACE_ROT[6];
  static const Vector3 FACE_DIR[6];
  static const Matrix FACE_SPIN[6];
  // One face's instance matrix, placed relative to the camera (floating
  // origin).
  static Matrix faceMatrix(int f, Vector3 at, Vector3 size,
                           const Vector3& cameraPos);

  Matrix lightMatrix;

  // One batch per texture, plus a last one (index Tex::Count) for faces
  // with no texture. Opaque and translucent are separate so translucent can
  // draw last.
  static constexpr size_t BATCH_COUNT = static_cast<size_t>(Tex::Count) + 1;
  std::array<Batch, BATCH_COUNT> opaque;
  std::array<Batch, BATCH_COUNT> translucent;

  // 4 slots, not 2: opaque and water each draw separately now (see
  // drawObjects), so a frame uses 2 slots and needs a frame of headroom behind
  // it.
  static constexpr int BUFFER_COUNT = 2 * 2 * BATCH_COUNT;
  unsigned int transformVBO[BUFFER_COUNT] = {};
  unsigned int colorVBO[BUFFER_COUNT] = {};
  size_t bufferCapacity[BUFFER_COUNT] = {};
  int currentBuffer = 0;

  // Uploads mats/colors into buffer slot `slot` and issues one instanced draw.
  // depthWrite off for water: it must not occlude other water behind it, or
  // stacked faces punch holes in each other the way bug #2 (fixed) did.
  void drawBatch(const std::vector<Matrix>& mats,
                 const std::vector<Vector4>& colors, int slot, int transformLoc,
                 int colorLoc, bool depthWrite);
  void drawBatches(const std::array<Batch, BATCH_COUNT>& batches,
                   bool depthWrite, int transformLoc, int colorLoc);

  double lastCullMs = 0.0;
  double lastGpuMs = 0.0;
  double lastShadowMapMs = 0.0;

  // Visible (non-occluded) blocks of one World chunk, so the whole chunk can be
  // frustum-culled at once. Keyed by the same chunk key World uses; only chunks
  // World reports dirty get rebuilt, never the whole map.
  // Water's shape, in world units above its cell's floor: surface height, and
  // where each side face's visible strip starts.
  struct WaterShape {
    float top{0};
    float sideBottom[6]{};  // indexed by face; a lower water neighbour hides
                            // the face up to its own surface
  };

  struct GridCell {
    std::vector<int> indices;  // into the objects vector passed to drawObjects
    // Parallel to `indices`: bit f set means face f has no neighbour, so it's
    // drawn.
    std::vector<uint8_t> faceMasks;
    std::vector<WaterShape>
        waterShapes;  // parallel to `indices`, only read for water
    BoundingBox bounds{};
  };

  std::unordered_map<int64_t, GridCell> grid;

  void rebuildChunk(int64_t key, const std::vector<Object>& objects,
                    const World& world);
  void ensureBufferCapacity(int slot, size_t count);
  static bool boxInFrustum(const Frustum& f, BoundingBox box);
};