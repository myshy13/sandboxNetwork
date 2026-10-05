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
  void updateAmbient(const float (&newAmbient)[4]);

  // Enable/disable an existing light.
  bool setLightEnabled(int index, bool enabled);

  void setViewPos(const Vector3& cameraPos);

  // Gives the shader the sun's view-projection matrix and its depth map.
  void setShadow(const Matrix& lightVP, unsigned int depthTextureId,
                 int mapSize);

  // Off makes the shader skip the shadow lookup entirely.
  void setShadowsEnabled(bool enabled);

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
  int reflectivityLoc = -1;
  int ambientLoc = -1;
  int lightVPLoc = -1;
  int shadowMapLoc = -1;
  int shadowResLoc = -1;
  int useShadowsLoc = -1;

  // Raylib resets texture slots 0-3 after every batch and the block texture
  // uses 0, so the shadow map lives well above them.
  static constexpr int SHADOW_SLOT = 10;

  std::array<Light, MAX_LIGHTS> lights{};
  int lightCount = 0;
};
