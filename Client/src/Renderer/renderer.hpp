#pragma once

#include "Models/Object.hpp"
#include "Renderer/frustum.hpp"
#include "Shaders/lighting.hpp"
#include <raylib.h>
#include <vector>

class Renderer {
public:
  Renderer();
  ~Renderer();

  Object *drawObjects(std::vector<Object> &objects,
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

  void ensureBufferCapacity(size_t count, int transformLoc, int colorLoc);
  static bool boxInFrustum(const Frustum &f, BoundingBox box);
};