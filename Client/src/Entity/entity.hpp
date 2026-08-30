#pragma once

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

class Entity {
public:
  virtual ~Entity() = default;

  virtual void Draw() const {
    Matrix matScale       = MatrixScale(transform.scale.x, transform.scale.y, transform.scale.z);
    Matrix matRotation    = QuaternionToMatrix(transform.rotation);
    Matrix matTranslation = MatrixTranslate(transform.translation.x, transform.translation.y, transform.translation.z);

    Matrix matTransform 
    = MatrixMultiply(MatrixMultiply(matScale, matRotation), matTranslation);

    rlPushMatrix();
    rlMultMatrixf(MatrixToFloat(matTransform));
    DrawCube(Vector3Zero(), 1.0f, 1.0f, 1.0f, WHITE);
    DrawCubeWires(Vector3Zero(), 1.0f, 1.0f, 1.0f, BLACK);
    rlPopMatrix();
  };

protected:
  Transform transform{};
  Vector3 velocity;
  bool onGround = true;
};