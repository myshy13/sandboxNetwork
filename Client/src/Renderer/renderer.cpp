#include "Renderer/renderer.hpp"

#include <complex.h>
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cfloat>
#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

#include "AssetManager/blockTex.hpp"
#include "AssetManager/manager.hpp"
#include "GameState/gameState.hpp"
#include "Models/Object.hpp"
#include "Shaders/lighting.hpp"
#include "env.hpp"

BoundingBox objectBox(const ObjectTransform& t);

// Face order: +X, -X, +Y, -Y, +Z, -Z.
const Vector3 Renderer::FACE_DIR[6] = {{1, 0, 0},  {-1, 0, 0}, {0, 1, 0},
                                       {0, -1, 0}, {0, 0, 1},  {0, 0, -1}};

const Matrix Renderer::FACE_ROT[6] = {
    MatrixRotateZ(-PI / 2), MatrixRotateZ(PI / 2), MatrixIdentity(),
    MatrixRotateX(PI),      MatrixRotateX(PI / 2), MatrixRotateX(-PI / 2)};

// Turns the quad about its own normal so a texture's up edge ends up facing
// world up on the side faces. Top and bottom have no "up", so they stay put.
const Matrix Renderer::FACE_SPIN[6] = {
    MatrixRotateY(PI / 2), MatrixRotateY(-PI / 2), MatrixIdentity(),
    MatrixIdentity(),      MatrixIdentity(),       MatrixRotateY(PI)};

// The quad is spun (texture upright), then scaled, before FACE_ROT turns it
// onto its face: the +-X faces turn size.x onto world Y, the +-Z faces size.z.
Matrix Renderer::faceMatrix(int f, Vector3 at, Vector3 size,
                            const Vector3& cameraPos) {
  Matrix m = MatrixMultiply(
      MatrixMultiply(FACE_SPIN[f], MatrixScale(size.x, size.y, size.z)),
      FACE_ROT[f]);
  at = Vector3Subtract(at, cameraPos);
  return MatrixMultiply(m, MatrixTranslate(at.x, at.y, at.z));
}

Renderer::Renderer(const AssetManager& a) : assets(a) {
  static constexpr int SHADOW_MAP_SIZE = 2048;

  faceMesh =
      GenMeshPlane(1, 1, 1, 1);  // lies in XZ, normal +Y; FACE_ROT turns it
  Model tmp = LoadModelFromMesh(faceMesh);
  cubeMat = tmp.materials[0];
  whiteTex = cubeMat.maps[MATERIAL_MAP_DIFFUSE].texture;

  shadowMapTarget = LoadRenderTexture(SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
}

Renderer::~Renderer() {
  for (int i = 0; i < BUFFER_COUNT; i++) {
    if (transformVBO[i]) rlUnloadVertexBuffer(transformVBO[i]);
    if (colorVBO[i]) rlUnloadVertexBuffer(colorVBO[i]);
  }
  UnloadMesh(faceMesh);

  UnloadRenderTexture(shadowMapTarget);
}

void Renderer::ensureBufferCapacity(int slot, size_t count) {
  if (count <= bufferCapacity[slot]) return;
  bufferCapacity[slot] = count;

  if (transformVBO[slot]) rlUnloadVertexBuffer(transformVBO[slot]);
  if (colorVBO[slot]) rlUnloadVertexBuffer(colorVBO[slot]);

  transformVBO[slot] =
      rlLoadVertexBuffer(nullptr, (int)(count * sizeof(Matrix)), true);
  colorVBO[slot] =
      rlLoadVertexBuffer(nullptr, (int)(count * sizeof(Vector4)), true);
}

void Renderer::drawBatches(const std::array<Batch, BATCH_COUNT>& batches,
                           bool depthWrite, int transformLoc, int colorLoc) {
  for (size_t i = 0; i < BATCH_COUNT; i++) {
    cubeMat.maps[MATERIAL_MAP_DIFFUSE].texture =
        i == static_cast<size_t>(Tex::Count) ? whiteTex
                                             : assets.get(static_cast<Tex>(i));
    currentBuffer = (currentBuffer + 1) % BUFFER_COUNT;

    drawBatch(batches[i].mats, batches[i].colors, currentBuffer, transformLoc,
              colorLoc, depthWrite);
  }
};

void Renderer::drawBatch(const std::vector<Matrix>& mats,
                         const std::vector<Vector4>& colors, int slot,
                         int transformLoc, int colorLoc, bool depthWrite) {
  if (mats.empty()) return;

  ensureBufferCapacity(slot, mats.size());
  rlUpdateVertexBuffer(transformVBO[slot], mats.data(),
                       (int)(mats.size() * sizeof(Matrix)), 0);
  rlUpdateVertexBuffer(colorVBO[slot], colors.data(),
                       (int)(colors.size() * sizeof(Vector4)), 0);

  rlEnableVertexArray(faceMesh.vaoId);
  rlEnableVertexBuffer(transformVBO[slot]);
  for (int i = 0; i < 4; i++) {
    int loc = transformLoc + i;
    rlEnableVertexAttribute(loc);
    rlSetVertexAttribute(loc, 4, RL_FLOAT, false, sizeof(Matrix),
                         i * sizeof(Vector4));
    rlSetVertexAttributeDivisor(loc, 1);
  }
  rlEnableVertexBuffer(colorVBO[slot]);
  rlEnableVertexAttribute(colorLoc);
  rlSetVertexAttribute(colorLoc, 4, RL_FLOAT, false, sizeof(Vector4), 0);
  rlSetVertexAttributeDivisor(colorLoc, 1);
  rlDisableVertexArray();

  if (!depthWrite) rlDisableDepthMask();
  DrawMeshInstanced(faceMesh, cubeMat, mats.data(), (int)mats.size());
  if (!depthWrite) rlEnableDepthMask();
}

bool Renderer::boxInFrustum(const Frustum& f, BoundingBox box) {
  for (const auto& p : f.planes) {
    Vector3 pv = {
        p.x >= 0 ? box.max.x : box.min.x,
        p.y >= 0 ? box.max.y : box.min.y,
        p.z >= 0 ? box.max.z : box.min.z,
    };
    if (p.x * pv.x + p.y * pv.y + p.z * pv.z + p.w < 0) return false;
  }
  return true;
}

// Water surface height above pos's cell floor (0 = no water): full under more
// water, so a falling column reads as one stream; otherwise lower the weaker
// the flow.
static float waterHeight(const World& world, Vector3 pos) {
  const int level = world.waterLevel(pos);
  if (level < 0) return 0.0f;
  const float size = env::BLOCKSIZE.y;
  if (world.isWater(Vector3Add(pos, {0, size, 0}))) return size;
  constexpr float SURFACE_DROP =
      0.6f;  // even a source sits a little below full, so it reads as water
  constexpr int STEPS = SHARED_WATER_MAX_LEVEL + 1;
  return (size - SURFACE_DROP) *
         (STEPS - std::min(level, SHARED_WATER_MAX_LEVEL)) / STEPS;
}

void Renderer::rebuildChunk(int64_t key, const std::vector<Object>& objects,
                            const World& world) {
  grid.erase(key);

  const std::vector<int>* blocks = world.getChunk(key);
  if (!blocks) return;  // chunk is empty now

  GridCell cell;
  for (int i : *blocks) {
    const ObjectTransform& t = objects[i].getTransform();

    // A face is drawn only where there's no neighbour to hide it, so no two
    // faces ever land on the same plane.
    // Water-on-water faces are skipped too: stacked transparent faces would add
    // up their alpha.
    const bool water = objects[i].getType() == BlockType::Water;
    WaterShape shape;
    if (water) shape.top = waterHeight(world, t.pos);
    uint8_t mask = 0;
    for (int f = 0; f < 6; f++) {
      Vector3 neighbour =
          Vector3Add(t.pos, Vector3Multiply(FACE_DIR[f], t.scale));
      if (world.isSolid(neighbour)) continue;
      if (water && world.isWater(neighbour)) {
        if (FACE_DIR[f].y != 0)
          continue;  // above/below is the same body of water
        // A side only shows where it rises above the neighbour's surface.
        shape.sideBottom[f] = waterHeight(world, neighbour);
        if (shape.sideBottom[f] >= shape.top) continue;
      }
      mask |= (uint8_t)(1 << f);
    }
    if (mask == 0) continue;  // fully buried, never contributes a visible pixel

    BoundingBox box = objectBox(t);
    if (cell.indices.empty()) {
      cell.bounds = box;
    } else {
      cell.bounds.min = Vector3Min(cell.bounds.min, box.min);
      cell.bounds.max = Vector3Max(cell.bounds.max, box.max);
    }
    cell.indices.push_back(i);
    cell.faceMasks.push_back(mask);
    cell.waterShapes.push_back(shape);
  }

  if (!cell.indices.empty()) grid[key] = std::move(cell);
}

Object* Renderer::drawObjects(std::vector<Object>& objects, World& world,
                              const Ray& facing, const Lighting& lighting,
                              const Camera3D& camera) {
  Frustum frustum =
      extractFrustum(camera);  // <-- replaces the manual matrix build

  for (int64_t key : world.takeDirtyChunks()) {
    rebuildChunk(key, objects, world);
  }

  for (auto& b : opaque) {
    b.colors.clear();
    b.mats.clear();
  }
  for (auto& b : translucent) {
    b.colors.clear();
    b.mats.clear();
  }

  Object* targeted = nullptr;
  float bestDistance = FLT_MAX;

  double cullStart = GetTime();

  for (auto& [key, cell] : grid) {
    if (!boxInFrustum(frustum, cell.bounds))
      continue;  // whole cell is off-screen, skip ever block in it
    if (Vector3Distance(
            Vector3Scale(Vector3Add(cell.bounds.max, cell.bounds.min), 0.5),
            camera.position) > GameState::shared().getRenderDistance())
      continue;

    for (size_t n = 0; n < cell.indices.size(); n++) {
      Object& o = objects[cell.indices[n]];
      ObjectTransform t = o.getTransform();
      BoundingBox box = objectBox(t);

      if (!boxInFrustum(frustum, box)) continue;

      // Only blocks within REACH (+ one block, for the box's own extent) can be
      // the pick target.
      float pickRange = REACH + t.scale.x;
      if (Vector3DistanceSqr(facing.position, t.pos) <= pickRange * pickRange) {
        RayCollision rc = GetRayCollisionBox(facing, box);
        if (rc.hit && rc.distance < bestDistance) {
          bestDistance = rc.distance;
          targeted = &o;
        }
      }

      bool water = o.getType() == BlockType::Water;
      const Color c = o.getColor();  // type colour, darkened by damage
      Vector4 colour = {c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f};
      // Every face of a block is either translucent or not, so pick the array
      // once here rather than per face.
      auto& batches = isTranslucent(o.getType()) ? translucent : opaque;
      uint8_t mask = cell.faceMasks[n];
      const WaterShape& shape = cell.waterShapes[n];
      const float floorY = t.pos.y - t.scale.y * 0.5f;

      for (int f = 0; f < 6; f++) {
        if (!(mask & (1 << f))) continue;

        Vector3 at =
            Vector3Add(t.pos, Vector3Scale(FACE_DIR[f], t.scale.x * 0.5f));
        Vector3 size = t.scale;

        if (water && f == 2) {
          at.y = floorY + shape.top;
        } else if (water && f != 3) {
          // Side face: only the strip from where it becomes visible up to the
          // surface.
          const float bottom = shape.sideBottom[f];
          at.y = floorY + (bottom + shape.top) * 0.5f;
          ((f == 0 || f == 1) ? size.x : size.z) = shape.top - bottom;
        }
        const Matrix m = faceMatrix(f, at, size, camera.position);

        // Index of the face's texture; Tex::Count (the last slot) = untextured.
        const size_t faceTex = static_cast<size_t>(
            blockTex(o.getType(), indexToFace(f)).value_or(Tex::Count));

        batches[faceTex].colors.push_back(colour);
        batches[faceTex].mats.push_back(m);
      }
    }
  }

  lastCullMs = (GetTime() - cullStart) * 1000.0;
  double gpuStart = GetTime();

  cubeMat.shader = lighting.getShader();
  int transformLoc = lighting.getTransformLoc();
  int colorLoc = lighting.getColorLoc();

  drawBatches(opaque, true, transformLoc, colorLoc);
  drawBatches(translucent, false, transformLoc, colorLoc);

  lastGpuMs = (GetTime() - gpuStart) * 1000.0;

  return (targeted != nullptr && bestDistance <= REACH) ? targeted : nullptr;
}

void Renderer::shadowMap(const std::vector<Object>& objects, Camera3D camera,
                         const Lighting& lighting, Vector3 toSun) {
  double startTime = GetTime();
  // get default raylib values
  double defaultNear = rlGetCullDistanceNear();
  double defaultFar = rlGetCullDistanceFar();
  const float radius = GameState::shared().getRenderDistance();
  float D = 2 * radius;
  Camera3D shadowCamera = {
      toSun * D, {0, 0, 0}, {0, 0, 1}, 2 * radius, CAMERA_ORTHOGRAPHIC};

  BeginTextureMode(shadowMapTarget);
  ClearBackground(WHITE);
  rlSetClipPlanes(1, D + radius + 100);
  BeginMode3D(shadowCamera);
  // draw objects
  std::vector<Matrix> mats;
  std::vector<Vector4> colors;
  for (const auto& [_, cell] : grid) {
    if (Vector3Distance(
            Vector3Scale(Vector3Add(cell.bounds.max, cell.bounds.min), 0.5f),
            camera.position) > radius + 10)
      continue;
    for (size_t n = 0; n < cell.indices.size(); n++) {
      const Object& o = objects[cell.indices[n]];
      if (o.isTranslucent()) {
        continue;
      }
      for (int f = 0; f < 6; f++) {
        if (!(cell.faceMasks[n] & (1 << f))) continue;
        const ObjectTransform& t = o.getTransform();
        Vector3 at =
            Vector3Add(t.pos, Vector3Scale(FACE_DIR[f], t.scale.x * 0.5f));
        mats.push_back(faceMatrix(f, at, t.scale, camera.position));
        colors.push_back({0, 0, 0, 255});  // dummy color, not used
      }
    }
  }
  cubeMat.shader = lighting.getShader();
  cubeMat.maps[MATERIAL_MAP_DIFFUSE].texture = whiteTex;

  for (size_t i = 0; i < BATCH_COUNT; i++) {
    cubeMat.maps[MATERIAL_MAP_DIFFUSE].texture =
        i == static_cast<size_t>(Tex::Count) ? whiteTex
                                             : assets.get(static_cast<Tex>(i));
    currentBuffer = (currentBuffer + 1) % BUFFER_COUNT;
  }

  drawBatch(mats, colors, currentBuffer, lighting.getTransformLoc(),
            lighting.getColorLoc(), true);

  EndMode3D();
  EndTextureMode();

  // restore default values
  rlSetClipPlanes(defaultNear, defaultFar);
  lastShadowMapMs = (GetTime() - startTime) * 1000;
}
