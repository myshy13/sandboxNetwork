#pragma once

#include <cmath>
#include <raylib.h>
#include <raymath.h>
struct Frustum {
  Vector4 planes[6]; // each plane: (a, b, c, d) where ax+by+cz+d=0, normal pointing inward
};

inline Frustum extractFrustum(const Camera3D &camera) {
  Matrix view = GetCameraMatrix(camera);
  Matrix proj = MatrixPerspective(camera.fovy * DEG2RAD,
                                  (float)GetScreenWidth() / GetScreenHeight(),
                                  0.05, 1000.0);
  Matrix mvp  = MatrixMultiply(view, proj);

  Frustum f;
  f.planes[0] = {mvp.m3 + mvp.m0, mvp.m7 + mvp.m4, mvp.m11 + mvp.m8, mvp.m15 + mvp.m12};
  f.planes[1] = {mvp.m3 - mvp.m0, mvp.m7 - mvp.m4, mvp.m11 - mvp.m8, mvp.m15 - mvp.m12};
  f.planes[2] = {mvp.m3 + mvp.m1, mvp.m7 + mvp.m5, mvp.m11 + mvp.m9, mvp.m15 + mvp.m13};
  f.planes[3] = {mvp.m3 - mvp.m1, mvp.m7 - mvp.m5, mvp.m11 - mvp.m9, mvp.m15 - mvp.m13};
  f.planes[4] = {mvp.m3 + mvp.m2, mvp.m7 + mvp.m6, mvp.m11 + mvp.m10, mvp.m15 + mvp.m14};
  f.planes[5] = {mvp.m3 - mvp.m2, mvp.m7 - mvp.m6, mvp.m11 - mvp.m10, mvp.m15 - mvp.m14};

  for (auto &p : f.planes) {
    float len = sqrtf(p.x * p.x + p.y * p.y + p.z * p.z);
    p.x /= len;
    p.y /= len;
    p.z /= len;
    p.w /= len;
  }
  return f;
}