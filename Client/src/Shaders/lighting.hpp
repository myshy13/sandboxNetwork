// lighting.hpp
#pragma once
#include <raylib.h>

class Lighting {
public:
  Lighting();
  ~Lighting();

  void addDirectional(Vector3 pos, Vector3 tar, Color color);
  void addPoint(Vector3 pos, Vector3 tar, Color color);
  void setViewPos(Vector3 &cameraPos);
  void begin();
  void end();

  const Shader &getShader() const { return shader; }
  int getColorLoc() const { return colorLoc; }
  int getTransformLoc() const { return transformLoc; }

private:
  Shader shader;
  int viewPosLoc;
  int colorLoc;
  int transformLoc;
};