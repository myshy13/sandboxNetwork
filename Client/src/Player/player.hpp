#ifndef SANDBOXNET_PLAYER
#define SANDBOXNET_PLAYER

#include "Entity/entity.hpp"
#include <raylib.h>

class Player : public Entity {
private:
  // bool local{true}; // Server/ Local player
  float speed     = 90.0f;
  float jumpPower = 40.0f;

  float yaw   = 0.0f;
  float pitch = 0.0f;

public:
  void Update(float dt, Camera3D &camera) override;
  void Draw() const override;
  Transform getTransform() {
    return transform;
  };
  Player();

  static void DrawPlayer(const Transform &transform) {
    Matrix matScale    = MatrixScale(transform.scale.x, transform.scale.y, transform.scale.z);
    Matrix matRotation = QuaternionToMatrix(transform.rotation);
    Matrix matTranslation =
        MatrixTranslate(transform.translation.x, transform.translation.y, transform.translation.z);

    Matrix matTransform = MatrixMultiply(MatrixMultiply(matScale, matRotation), matTranslation);

    rlPushMatrix();
    rlMultMatrixf(MatrixToFloat(matTransform));
    DrawCube(Vector3Zero(), 1.0f, 1.0f, 1.0f, RED);
    DrawCubeWires(Vector3Zero(), 1.0f, 1.0f, 1.0f, BLACK);
    rlPopMatrix();
  }
};

#endif