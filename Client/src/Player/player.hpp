#pragma once

#include <raylib.h>
#include <raymath.h>

#include <string>

#include "Game/Entity/entity.hpp"
#include "Input/input.hpp"
#include "World/world.hpp"

constexpr float waterMoveCooldownTime = 0.3f;

class Player : public Entity {
 private:
  const float speed = 90.0f;
  float jumpPower = 45.0f;
  float swimPower = 15.0f;

  float yaw = 0.0f;
  float pitch = 0.0f;

  float waterMoveCooldown = 0.5f;

 public:
  // When false, Update still runs physics (gravity, momentum, collision) but
  // ignores keyboard/mouse - used while the chat box has focus.
  bool inputEnabled = true;

  void Update(float dt, Camera3D& camera, const World& world, Input& input);
  void UpdateCamera(Camera3D& camera) const;
  // Exact look direction, unaffected by how far transform.translation is from
  // the origin.
  Vector3 getLookForward() const;

  const Transform& getTransform() const { return transform; };

  float getYaw() { return yaw; };

  float getPitch() { return pitch; };

  void setPosition(Vector3 pos) {
    transform.translation = pos;
    velocity = Vector3Zero();
    onGround = false;
    yaw = 0.0f;
    pitch = 0.0f;
    transform.rotation = QuaternionIdentity();  // not {}: a zero quaternion has
                                                // no look direction
  }

  Player();

  static void DrawBody(const Transform& transform, const bool outline = false);
  static void DrawPlayer(const Transform& transform, const std::string& name,
                         const Vector3& localPos);
};
