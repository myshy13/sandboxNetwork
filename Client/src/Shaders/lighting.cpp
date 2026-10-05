#include "lighting.hpp"

#include <raylib.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>

#include "Shaders/rlights.h"

#define RLIGHTS_IMPLEMENTATION
#include "Shaders/rlights.h"

static int glslVersion() {
  return rlGetVersion() == RL_OPENGL_ES_20 ? 100 : 330;
}

Lighting::Lighting() {
  if (glslVersion() == 330) {
#ifdef DEBUG
    shader = LoadShader(
        "/Users/hamish/Desktop/Code/sandboxNetwork/Client/assets/shaders/"
        "glsl330/lighting.vs",
        "/Users/hamish/Desktop/Code/sandboxNetwork/Client/assets/shaders/"
        "glsl330/lighting.fs");
#else
    shader = LoadShader("assets/shaders/glsl330/lighting.vs",
                        "assets/shaders/glsl330/lighting.fs");
#endif
  } else {
    shader = LoadShader("assets/shaders/glsl100/lighting.vs",
                        "assets/shaders/glsl100/lighting.fs");
  }

  viewPosLoc = GetShaderLocation(shader, "viewPos");
  reflectivityLoc = GetShaderLocation(shader, "reflectivity");
  ambientLoc = GetShaderLocation(shader, "ambient");
  lightVPLoc = GetShaderLocation(shader, "lightVP");
  shadowMapLoc = GetShaderLocation(shader, "shadowMap");
  shadowResLoc = GetShaderLocation(shader, "shadowMapResolution");
  useShadowsLoc = GetShaderLocation(shader, "useShadows");

  // Ambient light.
  float defaultAmbient[4] = {
      0.05f,
      0.05f,
      0.05f,
      1.0f,
  };

  float reflectivity = 0.0f;

  SetShaderValue(shader, ambientLoc, defaultAmbient, SHADER_UNIFORM_VEC4);
  SetShaderValue(shader, reflectivityLoc, &reflectivity, SHADER_UNIFORM_FLOAT);

  colorLoc = GetShaderLocationAttrib(shader, "instanceColor");
  transformLoc = GetShaderLocationAttrib(shader, "instanceTransform");

  shader.locs[SHADER_LOC_MATRIX_MODEL] = transformLoc;
}

Lighting::~Lighting() { UnloadShader(shader); }

int Lighting::addDirectional(Vector3 pos, Vector3 tar, Color color) {
  if (lightCount >= MAX_LIGHTS) {
    return -1;
  }

  const int index = lightCount++;

  lights[index] = CreateLight(LIGHT_DIRECTIONAL, pos, tar, color, shader);

  return index;
}

int Lighting::addPoint(Vector3 pos, Vector3 tar, Color color) {
  if (lightCount >= MAX_LIGHTS) {
    return -1;
  }

  const int index = lightCount++;

  lights[index] = CreateLight(LIGHT_POINT, pos, tar, color, shader);

  return index;
}

bool Lighting::updateLight(int index, Vector3 pos, Vector3 tar, Color color) {
  if (index < 0 || index >= lightCount) return false;

  lights[index].position = pos;
  lights[index].target = tar;
  lights[index].color = color;

  UpdateLightValues(shader, lights[index]);

  return true;
}

void Lighting::updateAmbient(const float (&newAmbient)[4]) {
  SetShaderValue(shader, ambientLoc, newAmbient, SHADER_UNIFORM_VEC4);
}

bool Lighting::setLightEnabled(int index, bool enabled) {
  if (index < 0 || index >= lightCount) {
    return false;
  }

  lights[index].enabled = enabled;

  UpdateLightValues(shader, lights[index]);

  return true;
}

void Lighting::setViewPos(const Vector3& cameraPos) {
  float p[3] = {
      cameraPos.x,
      cameraPos.y,
      cameraPos.z,
  };

  SetShaderValue(shader, viewPosLoc, p, SHADER_UNIFORM_VEC3);
}

void Lighting::setShadow(const Matrix& lightVP, unsigned int depthTextureId,
                         int mapSize) {
  SetShaderValueMatrix(shader, lightVPLoc, lightVP);
  SetShaderValue(shader, shadowResLoc, &mapSize, SHADER_UNIFORM_INT);

  // The sampler reads a texture slot, so bind the depth texture to ours first.
  const int slot = SHADOW_SLOT;
  rlEnableShader(shader.id);
  rlActiveTextureSlot(slot);
  rlEnableTexture(depthTextureId);
  rlSetUniform(shadowMapLoc, &slot, SHADER_UNIFORM_INT, 1);
}

void Lighting::setShadowsEnabled(bool enabled) {
  const int value = enabled ? 1 : 0;
  SetShaderValue(shader, useShadowsLoc, &value, SHADER_UNIFORM_INT);
}

void Lighting::begin() { BeginShaderMode(shader); }

void Lighting::end() { EndShaderMode(); }

DirectionalLight Lighting::timeToLight(float time) const {
  time = std::fmod(time, 1.0f);

  if (time < 0.0f) time += 1.0f;

  // 0.5 = midday.
  //
  // One complete rotation per day.
  // 0.5 -> highest point (sunrise 0.25, sunset 0.75)
  // 0.0 -> lowest point
  //
  // Using sin/cos means the position is continuous across
  // the 1.0 -> 0.0 boundary.

  const float angle = (time - 0.25f) * 2.0f * PI;
  const float radius = 100.0f;

  DirectionalLight light{};

  light.pos = {
      std::cos(angle) * radius,
      std::sin(angle) * radius,
      0.0f,
  };

  light.tar = {
      0.0f,
      0.0f,
      0.0f,
  };

  // Sun is above the horizon when sin(angle) > 0.
  const float height = std::max(0.0f, std::sin(angle));

  const float intensity = height;

  light.color = {
      static_cast<unsigned char>(255.0f * intensity),
      static_cast<unsigned char>(230.0f * intensity),
      static_cast<unsigned char>(190.0f * intensity),
      255,
  };

  return light;
}

Color Lighting::skyColor(float time) const {
  time = std::fmod(time, 1.0f);
  if (time < 0.0f) time += 1.0f;

  constexpr Color NIGHT{8, 15, 35, 255};
  constexpr Color DAY{100, 180, 255, 255};

  // 0.5 = midday: dawn 0.15 -> 0.30 and dusk 0.70 -> 0.85 mirror around it.
  if (time < 0.15f) return NIGHT;
  if (time < 0.30f) return ColorLerp(NIGHT, DAY, (time - 0.15f) / 0.15f);
  if (time < 0.70f) return DAY;
  if (time < 0.85f) return ColorLerp(DAY, NIGHT, (time - 0.70f) / 0.15f);
  return NIGHT;
}
