#pragma once
#include <raylib.h>

#include <array>

#include "Shaders/rlights.h"

struct DirectionalLight {
  Vector3 pos;
  Vector3 tar;
  Color color;
};

class Lighting {
 public:
  Lighting();
  ~Lighting();

  Lighting(const Lighting&) = delete;
  Lighting& operator=(const Lighting&) = delete;

  // Creates a new light and returns its index.
  // Returns -1 if MAX_LIGHTS has been reached.
  int addDirectional(Vector3 pos, Vector3 tar, Color color);
  int addPoint(Vector3 pos, Vector3 tar, Color color);

  // Updates an existing light.
  // Returns false if the index is invalid.
  bool updateLight(int index, Vector3 pos, Vector3 tar, Color color);

  // Enable/disable an existing light.
  bool setLightEnabled(int index, bool enabled);

  void setViewPos(const Vector3& cameraPos);

  void begin();
  void end();

  const Shader& getShader() const { return shader; }

  int getColorLoc() const { return colorLoc; }

  int getTransformLoc() const { return transformLoc; }

  // Returns the sun's position, target and colour for a given time.
  // time: 0 -> 1 represents one full day.
  DirectionalLight timeToLight(float time) const;

  Color skyColor(float time) const;

 private:
  Shader shader{};

  int viewPosLoc = -1;
  int colorLoc = -1;
  int transformLoc = -1;

  std::array<Light, MAX_LIGHTS> lights{};
  int lightCount = 0;
};
