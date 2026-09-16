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
  if (transformVBO)
    rlUnloadVertexBuffer(transformVBO);
  if (colorVBO)
    rlUnloadVertexBuffer(colorVBO);
  UnloadMesh(cubeMesh);
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

void Renderer::ensureBufferCapacity(size_t count, int transformLoc, int colorLoc) {
  if (count <= bufferCapacity)
    return; // change this if needed
  bufferCapacity = count;

  if (transformVBO)
    rlUnloadVertexBuffer(transformVBO);
  if (colorVBO)
    rlUnloadVertexBuffer(colorVBO);

  transformVBO = rlLoadVertexBuffer(nullptr, (int)(count * sizeof(Matrix)), true);
  colorVBO     = rlLoadVertexBuffer(nullptr, (int)(count * sizeof(Vector4)), true);

  rlEnableVertexArray(cubeMesh.vaoId);

  rlEnableVertexBuffer(transformVBO);
  for (int i = 0; i < 4; i++) {
    int loc = transformLoc + i;
    rlEnableVertexAttribute(loc);
    rlSetVertexAttribute(loc, 4, RL_FLOAT, false, sizeof(Matrix), i * sizeof(Vector4));
    rlSetVertexAttributeDivisor(loc, 1);
  }

  rlEnableVertexBuffer(colorVBO);
  rlEnableVertexAttribute(colorLoc);
  rlSetVertexAttribute(colorLoc, 4, RL_FLOAT, false, sizeof(Vector4), 0);
  rlSetVertexAttributeDivisor(colorLoc, 1);

  rlDisableVertexArray();
}

static int64_t cellKey(int cx, int cz) {
  return (static_cast<int64_t>(cx) << 32) ^ static_cast<uint32_t>(cz);
}

void Renderer::rebuildGrid(const std::vector<Object> &objects, const World &world) {
  grid.clear();
  for (int i = 0; i < (int)objects.size(); i++) {
    if (world.isOccluded(objects[i]))
      continue; // fully buried, never contributes a visible pixel

    ObjectTransform t = objects[i].getTransform();
    int cx            = (int)floorf(t.pos.x / CELL_SIZE);
    int cz            = (int)floorf(t.pos.z / CELL_SIZE);
    GridCell &cell    = grid[cellKey(cx, cz)];
    BoundingBox box   = objectBox(t);
    if (cell.indices.empty()) {
      cell.bounds = box;
    } else {
      cell.bounds.min = Vector3Min(cell.bounds.min, box.min);
      cell.bounds.max = Vector3Max(cell.bounds.max, box.max);
    }
    cell.indices.push_back(i);
  }
}

Object *Renderer::drawObjects(std::vector<Object> &objects,
                              int objectsVersion,
                              const World &world,
                              const Ray &facing,
                              const Lighting &lighting,
                              const Camera3D &camera) {
  Frustum frustum = extractFrustum(camera); // <-- replaces the manual matrix build

  if (objectsVersion != cachedVersion) {
    rebuildGrid(objects, world);
    cachedVersion = objectsVersion;
  }

  instanceMats.clear();
  instanceColors.clear();

  Object *targeted   = nullptr;
  float bestDistance = FLT_MAX;

  for (auto &[key, cell] : grid) {
    if (!boxInFrustum(frustum, cell.bounds))
      continue; // whole cell is off-screen, skip every block in it

    for (int i : cell.indices) {
      Object &o         = objects[i];
      ObjectTransform t = o.getTransform();
      BoundingBox box   = objectBox(t);

      RayCollision rc = GetRayCollisionBox(facing, box);
      if (rc.hit && rc.distance < bestDistance) {
        bestDistance = rc.distance;
        targeted     = &o;
      }

      if (!boxInFrustum(frustum, box))
        continue;

      Matrix scaleM    = MatrixScale(t.scale.x, t.scale.y, t.scale.z);
      Matrix translate = MatrixTranslate(t.pos.x, t.pos.y, t.pos.z);
      instanceMats.push_back(MatrixMultiply(scaleM, translate));

      Color c = o.getColor();
      instanceColors.push_back({c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f});
    }
  }

  if (!instanceMats.empty()) {
    int colorLoc     = lighting.getColorLoc();
    int transformLoc = lighting.getTransformLoc();
    ensureBufferCapacity(instanceMats.size(), transformLoc, colorLoc);

    rlUpdateVertexBuffer(transformVBO, instanceMats.data(),
                         (int)(instanceMats.size() * sizeof(Matrix)), 0);
    rlUpdateVertexBuffer(colorVBO, instanceColors.data(),
                         (int)(instanceColors.size() * sizeof(Vector4)), 0);

    cubeMat.shader = lighting.getShader();
    DrawMeshInstanced(cubeMesh, cubeMat, instanceMats.data(), (int)instanceMats.size());
  }

  return (targeted != nullptr && bestDistance <= REACH) ? targeted : nullptr;
}