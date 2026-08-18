#pragma once

struct Vector4 {
  float x; // Vector x component
  float y; // Vector y component
  float z; // Vector z component
  float w; // Vector w component
};

struct Vector3 {
  float x; // Vector x component
  float y; // Vector y component
  float z; // Vector z component
};

struct Transform {
  Vector3 translation; // Translation
  Vector4 rotation;    // Rotation
  Vector3 scale;       // Scale
};