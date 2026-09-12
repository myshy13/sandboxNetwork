#pragma once

#include <raylib.h>

class Lighting {
private:
  Shader shader;
  int viewPosLoc;
  int colorLoc;

public:
  Lighting();
  ~Lighting();

  void addDirectional(Vector3 pos, Vector3 tar, Color color);
  void addPoint(Vector3 pos, Vector3 tar, Color color);
  const Shader &getShader() const {
    return shader;
  }
  int getColorLoc() const {
    return colorLoc;
  };

  void setViewPos(Vector3 &cameraPos);
  void begin();
  void end();
};