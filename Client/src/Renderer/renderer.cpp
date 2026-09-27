#include "Renderer/renderer.hpp"
#include "GameState/gameState.hpp"
#include "Models/Object.hpp"
#include "env.hpp"
#include <cfloat>
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

BoundingBox objectBox(const ObjectTransform &t);

// Face order: +X, -X, +Y, -Y, +Z, -Z.
const Vector3 Renderer::FACE_DIR[6] = {
    {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};

const Matrix Renderer::FACE_ROT[6] = {
    MatrixRotateZ(-PI / 2), MatrixRotateZ(PI / 2),
    MatrixIdentity(), MatrixRotateX(PI),
    MatrixRotateX(PI / 2), MatrixRotateX(-PI / 2)};

Renderer::Renderer() {
  faceMesh  = GenMeshPlane(1, 1, 1, 1); // lies in XZ, normal +Y; FACE_ROT turns it
  Model tmp = LoadModelFromMesh(faceMesh);
  cubeMat   = tmp.materials[0];
}

Renderer::~Renderer() {
  for (int i = 0; i < BUFFER_COUNT; i++) {
    if (transformVBO[i])
      rlUnloadVertexBuffer(transformVBO[i]);
    if (colorVBO[i])
      rlUnloadVertexBuffer(colorVBO[i]);
  }
  UnloadMesh(faceMesh);
}
void Renderer::ensureBufferCapacity(int slot, size_t count) {
  if (count <= bufferCapacity[slot])
    return;
  bufferCapacity[slot] = count;

  if (transformVBO[slot])
    rlUnloadVertexBuffer(transformVBO[slot]);
  if (colorVBO[slot])
    rlUnloadVertexBuffer(colorVBO[slot]);

  transformVBO[slot] = rlLoadVertexBuffer(nullptr, (int)(count * sizeof(Matrix)), true);
  colorVBO[slot]     = rlLoadVertexBuffer(nullptr, (int)(count * sizeof(Vector4)), true);
}

void Renderer::drawBatch(const std::vector<Matrix> &mats, const std::vector<Vector4> &colors,
                         int slot, int transformLoc, int colorLoc, bool depthWrite) {
  if (mats.empty())
    return;

  ensureBufferCapacity(slot, mats.size());
  rlUpdateVertexBuffer(transformVBO[slot], mats.data(), (int)(mats.size() * sizeof(Matrix)), 0);
  rlUpdateVertexBuffer(colorVBO[slot], colors.data(), (int)(colors.size() * sizeof(Vector4)), 0);

  rlEnableVertexArray(faceMesh.vaoId);
  rlEnableVertexBuffer(transformVBO[slot]);
  for (int i = 0; i < 4; i++) {
    int loc = transformLoc + i;
    rlEnableVertexAttribute(loc);
    rlSetVertexAttribute(loc, 4, RL_FLOAT, false, sizeof(Matrix), i * sizeof(Vector4));
    rlSetVertexAttributeDivisor(loc, 1);
  }
  rlEnableVertexBuffer(colorVBO[slot]);
  rlEnableVertexAttribute(colorLoc);
  rlSetVertexAttribute(colorLoc, 4, RL_FLOAT, false, sizeof(Vector4), 0);
  rlSetVertexAttributeDivisor(colorLoc, 1);
  rlDisableVertexArray();

  if (!depthWrite)
    rlDisableDepthMask();
  DrawMeshInstanced(faceMesh, cubeMat, mats.data(), (int)mats.size());
  if (!depthWrite)
    rlEnableDepthMask();
}

bool Renderer::boxInFrustum(const Frustum &f, BoundingBox box) {
  for (const auto &p : f.planes) {
    Vector3 pv = {
        p.x >= 0 ? box.max.x : box.min.x,
        p.y >= 0 ? box.max.y : box.min.y,
        p.z >= 0 ? box.max.z : box.min.z,
    };
    if (p.x * pv.x + p.y * pv.y + p.z * pv.z + p.w < 0)
      return false;
  }
  return true;
}

void Renderer::rebuildChunk(int64_t key, const std::vector<Object> &objects, const World &world) {
  grid.erase(key);

  const std::vector<int> *blocks = world.getChunk(key);
  if (!blocks)
    return; // chunk is empty now

  GridCell cell;
  for (int i : *blocks) {
    const ObjectTransform &t = objects[i].getTransform();

    // A face is drawn only where there's no neighbour to hide it, so no two
    // faces ever land on the same plane.
    // Water-on-water faces are skipped too: stacked transparent faces would add up their alpha.
    const bool water = objects[i].getType() == BlockType::Water;
    uint8_t mask     = 0;
    for (int f = 0; f < 6; f++) {
      Vector3 neighbour = Vector3Add(t.pos, Vector3Multiply(FACE_DIR[f], t.scale));
      if (!world.isSolid(neighbour) && !(water && world.isWater(neighbour)))
        mask |= (uint8_t)(1 << f);
    }
    if (mask == 0)
      continue; // fully buried, never contributes a visible pixel

    BoundingBox box = objectBox(t);
    if (cell.indices.empty()) {
      cell.bounds = box;
    } else {
      cell.bounds.min = Vector3Min(cell.bounds.min, box.min);
      cell.bounds.max = Vector3Max(cell.bounds.max, box.max);
    }
    cell.indices.push_back(i);
    cell.faceMasks.push_back(mask);
  }

  if (!cell.indices.empty())
    grid[key] = std::move(cell);
}

Object *Renderer::drawObjects(std::vector<Object> &objects,
                              World &world,
                              const Ray &facing,
                              const Lighting &lighting,
                              const Camera3D &camera) {
  Frustum frustum = extractFrustum(camera); // <-- replaces the manual matrix build

  for (int64_t key : world.takeDirtyChunks()) {
    rebuildChunk(key, objects, world);
  }

  instanceMats.clear();
  instanceColors.clear();
  waterMats.clear();
  waterColors.clear();

  Object *targeted   = nullptr;
  float bestDistance = FLT_MAX;

  double cullStart = GetTime();

  for (auto &[key, cell] : grid) {
    if (!boxInFrustum(frustum, cell.bounds))
      continue; // whole cell is off-screen, skip ever block in it
    if (Vector3Distance(Vector3Scale(Vector3Add(cell.bounds.max, cell.bounds.min), 0.5), camera.position) > GameState::shared().getRenderDistance())
      continue;

    for (size_t n = 0; n < cell.indices.size(); n++) {
      Object &o         = objects[cell.indices[n]];
      ObjectTransform t = o.getTransform();
      BoundingBox box   = objectBox(t);

      if (!boxInFrustum(frustum, box))
        continue;

      // Only blocks within REACH (+ one block, for the box's own extent) can be the pick target.
      float pickRange = REACH + t.scale.x;
      if (Vector3DistanceSqr(facing.position, t.pos) <= pickRange * pickRange) {
        RayCollision rc = GetRayCollisionBox(facing, box);
        if (rc.hit && rc.distance < bestDistance) {
          bestDistance = rc.distance;
          targeted     = &o;
        }
      }

      bool water = o.getType() == BlockType::Water;
      // Only the surface layer drops - a block with water above it is fully
      // submerged, so shrinking its top would open a gap inside the water body.
      bool surface = water && !world.isWater(Vector3Add(t.pos, {0, t.scale.y, 0}));
      Color c      = o.getColor();
      if (water) {
        c.a = 100;
      }
      Vector4 colour = {c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f};
      uint8_t mask   = cell.faceMasks[n];

      // Water sits WATER_DROP below a full block, so its surface reads as "not quite
      // full" without opening a gap against whatever's below it.
      constexpr float WATER_DROP = 0.6f;

      for (int f = 0; f < 6; f++) {
        if (!(mask & (1 << f)))
          continue;

        Vector3 at = Vector3Add(t.pos, Vector3Scale(FACE_DIR[f], t.scale.x * 0.5f));
        Matrix m;

        // The quad is scaled before FACE_ROT rotates it onto the face, so which
        // scale axis becomes "vertical" in world space depends on the face:
        // top/bottom (identity/180 rotation) never rotate y into the plane at
        // all, so only x/z (the footprint) matter there; the four side faces
        // rotate their scale.x (+-X faces) or scale.z (+-Z faces) onto world Y.
        if (surface && f == 2) {
          // Top face: drop it, footprint untouched.
          at.y -= WATER_DROP;
          m = MatrixMultiply(MatrixScale(t.scale.x, t.scale.y, t.scale.z), FACE_ROT[f]);
        } else if (surface && (f == 0 || f == 1)) {
          // +-X side faces.
          at.y -= WATER_DROP * 0.5f; // shrinking is centred - shift half the drop so only the top edge moves
          m = MatrixMultiply(MatrixScale(t.scale.x - WATER_DROP, t.scale.y, t.scale.z), FACE_ROT[f]);
        } else if (surface && (f == 4 || f == 5)) {
          // +-Z side faces.
          at.y -= WATER_DROP * 0.5f;
          m = MatrixMultiply(MatrixScale(t.scale.x, t.scale.y, t.scale.z - WATER_DROP), FACE_ROT[f]);
        } else {
          // Solid faces, submerged water (any face, not just the surface layer),
          // and water's own bottom face: full size, no shift - must sit flush
          // against whatever's below/beside it.
          m = MatrixMultiply(MatrixScale(t.scale.x, t.scale.y, t.scale.z), FACE_ROT[f]);
        }

        at = Vector3Subtract(at, camera.position);
        m  = MatrixMultiply(m, MatrixTranslate(at.x, at.y, at.z));

        // Water goes in its own batch, drawn after the opaque one below: mixed into
        // the same batch, an unsorted translucent face can write depth in front of an
        // opaque one behind it and punch a hole through solid geometry.
        if (water) {
          waterMats.push_back(m);
          waterColors.push_back(colour);
        } else {
          instanceMats.push_back(m);
          instanceColors.push_back(colour);
        }
      }
    }
  }

  lastCullMs      = (GetTime() - cullStart) * 1000.0;
  double gpuStart = GetTime();

  cubeMat.shader   = lighting.getShader();
  int transformLoc = lighting.getTransformLoc();
  int colorLoc     = lighting.getColorLoc();

  currentBuffer = (currentBuffer + 1) % BUFFER_COUNT;
  drawBatch(instanceMats, instanceColors, currentBuffer, transformLoc, colorLoc, /*depthWrite=*/true);

  currentBuffer = (currentBuffer + 1) % BUFFER_COUNT;
  drawBatch(waterMats, waterColors, currentBuffer, transformLoc, colorLoc, /*depthWrite=*/false);

  lastGpuMs = (GetTime() - gpuStart) * 1000.0;

  return (targeted != nullptr && bestDistance <= REACH) ? targeted : nullptr;
}