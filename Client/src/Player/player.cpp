#include "Player/player.hpp"

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include <cstdlib>

#include "GameState/gameState.hpp"
#include "Input/input.hpp"
#include "Input/inputState.hpp"
#include "Raylib/text3D.hpp"
#include "env.hpp"

#define PLAYERCOLOR RED

constexpr float GRAVITY = 140.0f;

void Player::Update(float dt, Camera3D& camera, const World& world,
                    Input& input) {
  if (inputEnabled && input.pressed(Action::Click) && !IsCursorHidden()) {
    input.setMouseLook(true);
  }
  // ==== player movement ====
  Vector3 lookForward =
      Vector3RotateByQuaternion({0.0f, 0.0f, -1.0f}, transform.rotation);

  // Flattened, used for ground movement only
  Vector3 moveForward = lookForward;
  moveForward.y = 0.0f;
  moveForward = Vector3Normalize(moveForward);

  Vector3 right =
      Vector3RotateByQuaternion({1.0f, 0.0f, 0.0f}, transform.rotation);
  Vector3 moveRight = right;
  moveRight.y = 0.0f;
  moveRight = Vector3Normalize(moveRight);

  constexpr float GROUND_Y = 0.0f;

  // ==== collision helpers ====
  Vector3 half = {transform.scale.x * 0.5f, 0.0f, transform.scale.z * 0.5f};

  // True if a box overlaps any placed block.
  auto hitsBlock = [&](BoundingBox b) { return world.boxCollides(b); };
  auto hitsWater = [&](BoundingBox b) { return world.boxCollides(b, isFluid); };
  // The player's body box, bottom at the feet (translation), scale tall.
  auto blocked = [&](Vector3 feet) -> bool {
    return hitsBlock(
        {Vector3Subtract(feet, half),
         Vector3Add(Vector3Subtract(feet, half), transform.scale)});
  };

  // Grounded = on the floor plane, or a thin slab just under the feet touches
  // a block. A foot slab (not the whole body) so standing beside a wall
  // doesn't count as standing on it. Recomputed each frame -> walk off a
  // ledge and you start falling next frame.
  BoundingBox feetSlab{
      Vector3Subtract(transform.translation, {half.x, 0.2f, half.z}),
      Vector3Add(transform.translation, {half.x, 0.0f, half.z})};
  onGround = transform.translation.y <= GROUND_Y || hitsBlock(feetSlab);
  if (onGround && velocity.y <= 0.0f) {
    velocity.y = 0.0f;
    if (transform.translation.y < GROUND_Y) {
      transform.translation.y = GROUND_Y;
    }
  }

  BoundingBox probe{
      Vector3Subtract(transform.translation, {half.x, 0.0f, half.z}),
      Vector3Add(
          Vector3Subtract(transform.translation, {half.x, 0.0f, half.z}),
          {transform.scale.x, transform.scale.y * 0.5f, transform.scale.z})};

  bool inWater = hitsWater(probe);

  Vector3 moveDir = Vector3Zero();

  if (inputEnabled) {
    Vector2 move = input.getMove();

    moveDir = Vector3Add(moveDir, Vector3Scale(moveForward, move.y));
    moveDir = Vector3Add(moveDir, Vector3Scale(moveRight, move.x));
  }

  float multiplier = onGround ? 1.0f : inWater ? 0.5f : 0.05f;
  if (input.down(Action::Sneak)) {
    multiplier *= 0.2f;
  }

  // Cap at 1, don't force it: diagonals can't be faster, a half-tilted stick
  // stays slower.
  if (Vector3Length(moveDir) > 1.0f) {
    moveDir = Vector3Normalize(moveDir);
  }

  if (!inWater && inputEnabled && (onGround) && input.down(Action::Jump)) {
    onGround = false;
    velocity.y = jumpPower;
  }

  waterMoveCooldown -= dt;
  if (inputEnabled && inWater && input.down(Action::Jump) &&
      waterMoveCooldown <= 0) {
    onGround = false;
    // if they're moving slow enough, use full swim power
    if ((velocity.y > 0 ? velocity.y : -velocity.y) < 30.0f) {
      velocity.y = 0;
    }
    velocity.y += swimPower;
    waterMoveCooldown = waterMoveCooldownTime;
  }

  // Push and damp in one exact step, so top speed doesn't depend on frame rate.
  const float dampBase = inWater ? 0.6f : onGround ? 0.7f : 0.9f;  // per 1/60 s
  const float damping = powf(dampBase, dt * 60.0f);
  // The speed the old per-frame version settled at when running at 60 fps.
  const float topSpeed = speed * multiplier * dampBase / (1.0f - dampBase);
  velocity.x = velocity.x * damping + moveDir.x * topSpeed * (1.0f - damping);
  velocity.z = velocity.z * damping + moveDir.z * topSpeed * (1.0f - damping);

  Vector2 horizontalVel = {velocity.x, velocity.z};
  if (Vector2Length(horizontalVel) > 50.0f) {
    horizontalVel = Vector2Scale(Vector2Normalize(horizontalVel), 50.0f);
    velocity.x = horizontalVel.x;
    velocity.z = horizontalVel.y;
  }
  if (!onGround) {
    if (inWater) {
      velocity.y -= GRAVITY * dt * 0.2f;
      // Terminal velocity: cap how fast we can fall.
      constexpr float TERMINAL_VELOCITY = -20;
      velocity.y = Clamp(velocity.y, TERMINAL_VELOCITY, jumpPower);
    } else {
      velocity.y -= GRAVITY * dt;
      // Terminal velocity: cap how fast we can fall.
      constexpr float TERMINAL_VELOCITY = -120.0f;
      velocity.y = Clamp(velocity.y, TERMINAL_VELOCITY, jumpPower);
    }
  }
  // to stop tiny fractions
  if (Vector3LengthSqr(velocity) < 0.01f) {
    velocity = Vector3Zero();
  }

  // ==== move + collide, one axis at a time ====
  Vector3 pos = transform.translation;
  Vector3 step = Vector3Scale(velocity, dt);

  bool autoJump = GameState::shared().getAutoJump();

  pos.x += step.x;
  if (blocked(pos)) {
    pos.x -= step.x;
    velocity.x = 0.0f;
  }

  pos.z += step.z;
  if (blocked(pos)) {
    pos.z -= step.z;
    velocity.z = 0.0f;
  }

  pos.y += step.y;
  if (blocked(pos)) {
    if (step.y < 0.0f) {
      onGround = true;  // landed on a block top
    }
    pos.y -= step.y;
    velocity.y = 0.0f;
  }

  constexpr int autoJumpPrediction = 15;
  if (autoJump && onGround && !inWater && !input.down(Action::Sneak) &&
      inputEnabled && moveDir != Vector3Zero()) {
    Vector3 newPos = Vector3Add(
        pos, {step.x * autoJumpPrediction, 0, step.z * autoJumpPrediction});
    if (blocked(newPos) &&
        !blocked(Vector3Add(
            newPos, {0, env::BLOCKSIZE.y * 1.2, 0}))) {  // 1.2 for a small gap
      onGround = false;
      velocity.y = jumpPower;
    }
  }

  transform.translation = pos;

  // ==== mouse rotaton =====
  Vector2 mouseDelta = input.getLook();
  if (!inputEnabled) mouseDelta = {0.0f, 0.0f};

  float mouseSensitivity = 0.00301f;

  yaw -= mouseDelta.x * mouseSensitivity;
  pitch -= mouseDelta.y * mouseSensitivity;

  float pitchLimit = 89.0f * DEG2RAD;
  pitch = Clamp(pitch, -pitchLimit, pitchLimit);

  transform.rotation = QuaternionFromEuler(pitch, yaw, 0.0f);

  UpdateCamera(camera);
}

// Separate from Update because the camera has to follow the body even when
// movement is frozen - a respawn while paused moves us, and the view has to
// come along or it looks like the respawn never happened.
void Player::UpdateCamera(Camera3D& camera) const {
  Vector3 head =
      Vector3Add(transform.translation, {0.0f, transform.scale.y, 0.0f});
  camera.position = head;
  // camera.target only has to be *a* point in the look direction -
  // GetCameraMatrix and the frustum cull just need a direction out of it, so
  // this loses nothing they use. Anything that needs the exact direction (the
  // origin-relative render camera, the pick ray) should use getLookForward()
  // instead: far from the origin, this add rounds away anything finer than
  // head's float precision, which is coarser than a mouselook step, so reading
  // the direction back out of this point would stutter.
  camera.target = Vector3Add(head, getLookForward());
}

Vector3 Player::getLookForward() const {
  return Vector3RotateByQuaternion({0.0f, 0.0f, -1.0f}, transform.rotation);
}

Player::Player() {
  Vector3 spawnPos;
  spawnPos.x = rand() % 200 - 100;
  spawnPos.z = rand() % 200 - 100;
  spawnPos.y = 10;
  transform.rotation = QuaternionFromEuler(0, 0, 0);
  transform.scale = env::PLAYER_SCALE;
  transform.translation = spawnPos;
  onGround = false;
}

void Player::DrawBody(const Transform& transform, const bool outline) {
  Matrix matScale =
      MatrixScale(transform.scale.x, transform.scale.y, transform.scale.z);
  Matrix matRotation = QuaternionToMatrix(transform.rotation);
  Matrix matTranslation =
      MatrixTranslate(transform.translation.x, transform.translation.y,
                      transform.translation.z);

  Matrix matTransform =
      MatrixMultiply(MatrixMultiply(matScale, matRotation), matTranslation);

  rlPushMatrix();
  rlMultMatrixf(MatrixToFloat(matTransform));
  // outline: wireframe only, since the Renderer draws the lit solid box.
  if (outline) {
    DrawCubeWiresV({0.0f, 0.5f, 0.0f}, {1, 1, 1}, BLACK);
  } else {
    DrawCubeV({0.0f, 0.5f, 0.0f}, {1, 1, 1}, WHITE);
  }
  rlPopMatrix();
}

void Player::DrawPlayer(const Transform& transform, const std::string& name,
                        const Vector3& localPos) {
  DrawBody(transform, true);

  Vector3 p = transform.translation + Vector3{0, transform.scale.y + 2.0f, 0};
  constexpr float FONT_SIZE = 2, SPACING = 0.05f;
  float halfW =
      MeasureTextEx(GetFontDefault(), name.c_str(), FONT_SIZE, SPACING).x *
      0.5f;
  Vector3 d = Vector3Subtract(localPos, p);  // pos difference

  rlPushMatrix();
  rlTranslatef(p.x, p.y, p.z);
  rlRotatef(atan2f(d.x, d.z) * RAD2DEG, 0.0f, 1.0f, 0.0f);
  rlRotatef(90.0f, 1.0f, 0.0f, 0.0f);
  DrawText3D(GetFontDefault(), name.c_str(), {-halfW, 0, 0}, FONT_SIZE, SPACING,
             1.0f, true, WHITE);
  rlPopMatrix();
}
