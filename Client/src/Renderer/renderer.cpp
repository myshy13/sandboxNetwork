#include "Renderer/renderer.hpp"
#include "env.hpp"
#include <cfloat>
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

BoundingBox objectBox(const ObjectTransform &t);

Renderer::Renderer() {
  cubeMesh  = GenMeshCube(1, 1, 1);
  Model tmp = LoadModelFromMesh(cubeMesh);
  cubeMat   = tmp.materials[0];
}

Renderer::~Renderer() {
  for (int i = 0; i < BUFFER_COUNT; i++) {
    if (transformVBO[i])
      rlUnloadVertexBuffer(transformVBO[i]);
    if (colorVBO[i])
      rlUnloadVertexBuffer(colorVBO[i]);
  }
  UnloadMesh(cubeMesh);
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
    if (world.isOccluded(objects[i]))
      continue; // fully buried, never contributes a visible pixel

    BoundingBox box = objectBox(objects[i].getTransform());
    if (cell.indices.empty()) {
      cell.bounds = box;
    } else {
      cell.bounds.min = Vector3Min(cell.bounds.min, box.min);
      cell.bounds.max = Vector3Max(cell.bounds.max, box.max);
    }
    cell.indices.push_back(i);
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

  Object *targeted   = nullptr;
  float bestDistance = FLT_MAX;

  double cullStart = GetTime();

  for (auto &[key, cell] : grid) {
    if (!boxInFrustum(frustum, cell.bounds))
      continue; // whole cell is off-screen, skip ever block in it
    if (Vector3Distance(Vector3Scale(Vector3Add(cell.bounds.max, cell.bounds.min), 0.5), camera.position) > 400.0f)
      continue;

    for (int i : cell.indices) {
      Object &o         = objects[i];
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

      Matrix m = MatrixIdentity();
      m.m0     = t.scale.x;
      m.m5     = t.scale.y;
      m.m10    = t.scale.z;
      m.m12    = t.pos.x;
      m.m13    = t.pos.y;
      m.m14    = t.pos.z;
      instanceMats.push_back(m);

      Color c = o.getColor();
      instanceColors.push_back({c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f});
    }
  }

  lastCullMs      = (GetTime() - cullStart) * 1000.0;
  double gpuStart = GetTime();

  if (!instanceMats.empty()) {
    currentBuffer    = (currentBuffer + 1) % BUFFER_COUNT;
    int transformLoc = lighting.getTransformLoc();
    int colorLoc     = lighting.getColorLoc();
    ensureBufferCapacity(currentBuffer, instanceMats.size());

    rlUpdateVertexBuffer(transformVBO[currentBuffer], instanceMats.data(),
                         (int)(instanceMats.size() * sizeof(Matrix)), 0);
    rlUpdateVertexBuffer(colorVBO[currentBuffer], instanceColors.data(),
                         (int)(instanceColors.size() * sizeof(Vector4)), 0);

    rlEnableVertexArray(cubeMesh.vaoId);
    rlEnableVertexBuffer(transformVBO[currentBuffer]);
    for (int i = 0; i < 4; i++) {
      int loc = transformLoc + i;
      rlEnableVertexAttribute(loc);
      rlSetVertexAttribute(loc, 4, RL_FLOAT, false, sizeof(Matrix), i * sizeof(Vector4));
      rlSetVertexAttributeDivisor(loc, 1);
    }
    rlEnableVertexBuffer(colorVBO[currentBuffer]);
    rlEnableVertexAttribute(colorLoc);
    rlSetVertexAttribute(colorLoc, 4, RL_FLOAT, false, sizeof(Vector4), 0);
    rlSetVertexAttributeDivisor(colorLoc, 1);
    rlDisableVertexArray();

    cubeMat.shader = lighting.getShader();
    DrawMeshInstanced(cubeMesh, cubeMat, instanceMats.data(), (int)instanceMats.size());
  }

  lastGpuMs = (GetTime() - gpuStart) * 1000.0;

  return (targeted != nullptr && bestDistance <= REACH) ? targeted : nullptr;
}