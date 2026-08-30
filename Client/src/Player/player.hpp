#ifndef SANDBOXNET_PLAYER
#define SANDBOXNET_PLAYER

#include "Entity/entity.hpp"
#include "Models/Object.hpp"
#include <raylib.h>
#include <raymath.h>
#include <vector>

enum Perspective {
  FirstPerson,
  ThirdPerson
};

class Player : public Entity {
private:
  const float speed = 90.0f;
  float jumpPower   = 40.0f;

  float yaw   = 0.0f;
  float pitch = 0.0f;

  Perspective perspective = FirstPerson;

public:
  // When false, Update still runs physics (gravity, momentum, collision) but
  // ignores keyboard/mouse - used while the chat box has focus.
  bool inputEnabled = true;

  void Update(float dt, Camera3D &camera, const std::vector<Object> &blocks);
  void UpdateCamera(Camera3D &camera) const;
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
  void setPosition(Vector3 pos) {
    transform.translation = pos;
    velocity              = Vector3Zero();
    onGround              = false;
    yaw                   = 0.0f;
    transform.rotation    = {};
  }
  Player();

  static void DrawPlayer(const Transform &transform);
};

#endif