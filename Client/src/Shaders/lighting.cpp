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

  // Ambient light.
  float ambient[4] = {
      0.05f,
      0.05f,
      0.05f,
      1.0f,
  };

  float reflectivity = 0.1f;

  SetShaderValue(shader, GetShaderLocation(shader, "ambient"), ambient,
                 SHADER_UNIFORM_VEC4);
  SetShaderValue(shader, GetShaderLocation(shader, "reflectivity"),
                 &reflectivity, SHADER_UNIFORM_FLOAT);

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
  if (index < 0 || index >= lightCount) {
    return false;
  }

  lights[index].position = pos;
  lights[index].target = tar;
  lights[index].color = color;

  UpdateLightValues(shader, lights[index]);

  return true;
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

void Lighting::begin() { BeginShaderMode(shader); }

void Lighting::end() { EndShaderMode(); }

DirectionalLight Lighting::timeToLight(float time) const {
  time = std::fmod(time, 1.0f);

  if (time < 0.0f) time += 1.0f;

  // 0.8 = midday.
  //
  // One complete rotation per day.
  // 0.8 -> highest point
  // 0.3 -> lowest point
  //
  // Using sin/cos means the position is continuous across
  // the 1.0 -> 0.0 boundary.

  const float angle = (time - 0.8f) * 2.0f * PI;
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

  // Midnight
  if (time < 0.15f) {
    return Color{8, 15, 35, 255};
  }

  // Sunrise: 0.15 -> 0.30
  if (time < 0.30f) {
    float t = (time - 0.15f) / 0.15f;

    // Dark blue -> light blue.
    unsigned char r = static_cast<unsigned char>(25 + 75 * t);
    unsigned char g = static_cast<unsigned char>(40 + 130 * t);
    unsigned char b = static_cast<unsigned char>(80 + 170 * t);

    return Color{r, g, b, 255};
  }

  // Daytime: 0.30 -> 0.70
  if (time < 0.70f) {
    return Color{100, 180, 255, 255};
  }

  // Afternoon -> midday brightness
  if (time < 0.80f) {
    float t = (time - 0.70f) / 0.10f;

    unsigned char r = static_cast<unsigned char>(100 - 10 * t);
    unsigned char g = static_cast<unsigned char>(180 + 5 * t);
    unsigned char b = static_cast<unsigned char>(255);

    return Color{r, g, b, 255};
  }

  // 0.80 = brightest point / midday
  if (time < 0.90f) {
    float t = (time - 0.80f) / 0.10f;

    // Blue sky gradually darkens.
    unsigned char r = static_cast<unsigned char>(90 - 45 * t);
    unsigned char g = static_cast<unsigned char>(185 - 80 * t);
    unsigned char b = static_cast<unsigned char>(255 - 80 * t);

    return Color{r, g, b, 255};
  }

  // Evening -> night.
  if (time < 0.97f) {
    float t = (time - 0.90f) / 0.07f;

    // Stay blue; don't introduce purple/red.
    unsigned char r = static_cast<unsigned char>(45 - 35 * t);
    unsigned char g = static_cast<unsigned char>(105 - 90 * t);
    unsigned char b = static_cast<unsigned char>(175 - 140 * t);

    return Color{r, g, b, 255};
  }

  // Night.
  return Color{8, 15, 35, 255};
}
