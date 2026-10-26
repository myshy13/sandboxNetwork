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

  void Update(float dt, Camera3D& camera, const World& world, Input& input,
              bool zoomed);
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

  static void DrawBody(const Transform& transform, const bool outline = false,
                       const Color& c = WHITE);
  static void DrawPlayer(const Transform& transform, const std::string& name,
                         const Vector3& localPos);

  static Vector4 UsernameToVec4(const char* username) {
    uint32_t hash = 2166136261u;

    for (const unsigned char* p =
             reinterpret_cast<const unsigned char*>(username);
         *p; ++p) {
      hash ^= *p;
      hash *= 16777619u;
    }

    return {0.35f + ((hash & 0xFF) / 255.0f) * 0.55f,
            0.35f + (((hash >> 8) & 0xFF) / 255.0f) * 0.55f,
            0.35f + (((hash >> 16) & 0xFF) / 255.0f) * 0.55f, 1.0f};
  }

  static Color UsernameToColor(const char* username) {
    uint32_t hash = 2166136261u;

    // FNV-1a hash
    for (const unsigned char* p = (const unsigned char*)username; *p; p++) {
      hash ^= *p;
      hash *= 16777619u;
    }

    // Generate a vivid color
    unsigned char r = 80 + (hash & 0x7F);
    unsigned char g = 80 + ((hash >> 8) & 0x7F);
    unsigned char b = 80 + ((hash >> 16) & 0x7F);

    return (Color){r, g, b, 255};
  }
};
