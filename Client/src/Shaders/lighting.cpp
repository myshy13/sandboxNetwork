#include "Shaders/lighting.hpp"
#include "Shaders/rlights.h"
#include "env.hpp"
#include <raylib.h>
#include <rlgl.h>

#define RLIGHTS_IMPLEMENTATION
#include "Shaders/rlights.h"

static int glslVersion() { return rlGetVersion() == RL_OPENGL_ES_20 ? 100 : 330; }

Lighting::Lighting() {
  if (glslVersion() == 330) {
// don't use on web builds
#ifdef DEBUG
    shader = LoadShader("/Users/hamish/Desktop/Code/sandboxNetwork/Client/assets/shaders/glsl330/lighting.vs", "/Users/hamish/Desktop/Code/sandboxNetwork/Client/assets/shaders/glsl330/lighting.fs");
#else
    shader = LoadShader("assets/shaders/glsl330/lighting.vs", "assets/shaders/glsl330/lighting.fs");

#endif
    shader = LoadShader("assets/shaders/glsl330/lighting.vs", "assets/shaders/glsl330/lighting.fs");
  } else {
    shader = LoadShader("assets/shaders/glsl100/lighting.vs", "assets/shaders/glsl100/lighting.fs");
  }
  viewPosLoc = GetShaderLocation(shader, "viewPos");

  float ambient[4] = {0.3f, 0.3f, 0.3f, 1.0f};
  SetShaderValue(shader, GetShaderLocation(shader, "ambient"), ambient, SHADER_UNIFORM_VEC4);

  colorLoc = GetShaderLocationAttrib(shader, "instanceColor");
  transformLoc = GetShaderLocationAttrib(shader, "instanceTransform");

  shader.locs[SHADER_LOC_MATRIX_MODEL] = transformLoc;
}

Lighting::~Lighting() {
  UnloadShader(shader);
}

void Lighting::addDirectional(Vector3 pos, Vector3 tar, Color color) {
  CreateLight(LIGHT_DIRECTIONAL, pos, tar, color, shader);
}

void Lighting::addPoint(Vector3 pos, Vector3 tar, Color color) {
  CreateLight(LIGHT_POINT, pos, tar, color, shader);
}

void Lighting::setViewPos(Vector3 &cameraPos) {
  float p[3] = {cameraPos.x, cameraPos.y, cameraPos.z};
  SetShaderValue(shader, viewPosLoc, p, SHADER_UNIFORM_VEC3);
}

void Lighting::begin() { BeginShaderMode(shader); }
void Lighting::end() { EndShaderMode(); }
