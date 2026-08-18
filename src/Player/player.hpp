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
  Player();
};

#endif