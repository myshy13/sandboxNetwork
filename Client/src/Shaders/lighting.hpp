#pragma once

#include <raylib.h>

class Lighting {
private:
  Shader shader;
  int viewPosLoc;

public:
  Lighting();
  ~Lighting();

  void addDirectional(Vector3 pos, Vector3 tar, Color color);
  void addPoint(Vector3 pos, Vector3 tar, Color color);

  void setViewPos(Vector3 &cameraPos);
  void begin();
  void end();
};