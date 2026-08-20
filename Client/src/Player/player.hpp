#ifndef SANDBOXNET_PLAYER
#define SANDBOXNET_PLAYER

#include "Entity/entity.hpp"
#include "structs.hpp"
#include <raylib.h>

enum Perspective {
  FirstPerson,
  ThirdPerson
};

class Player : public Entity {
private:
  // bool local{true}; // Server/ Local player
  float speed     = 90.0f;
  float jumpPower = 40.0f;

  float yaw   = 0.0f;
  float pitch = 0.0f;

  Perspective perspective = FirstPerson;

public:
  void Update(float dt, Camera3D &camera) override;
  void Draw() const override;
  Transform getTransform() {
    return transform;
  };
  float getYaw() {
    return yaw;
  };
  float getPitch() {
    return pitch;
  };

  Perspective getPerspective() {
    return perspective;
  }
  Player();

  static void DrawPlayer(const Transform &transform);
};

#endif