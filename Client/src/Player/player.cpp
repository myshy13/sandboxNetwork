#include "Player/player.hpp"
#include "Models/Object.hpp"
#include "Raylib/text3D.hpp"
#include <cstdlib>
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#define PLAYERCOLOR RED

constexpr float GRAVITY = 140.0f;

void Player::Update(float dt, Camera3D &camera, const std::vector<Object> &blocks) {
  if (inputEnabled && IsMouseButtonDown(MOUSE_LEFT_BUTTON) && !IsCursorHidden()) {
    DisableCursor();
  }

  // ==== player movement ====
  Vector3 lookForward = Vector3RotateByQuaternion({0.0f, 0.0f, -1.0f}, transform.rotation);

  // Flattened, used for ground movement only
  Vector3 moveForward = lookForward;
  moveForward.y       = 0.0f;
  moveForward         = Vector3Normalize(moveForward);

  Vector3 right     = Vector3RotateByQuaternion({1.0f, 0.0f, 0.0f}, transform.rotation);
  Vector3 moveRight = right;
  moveRight.y       = 0.0f;
  moveRight         = Vector3Normalize(moveRight);

  constexpr float GROUND_Y = 0.0f;

  // ==== collision helpers ====
  Vector3 half = {transform.scale.x * 0.5f, 0.0f, transform.scale.z * 0.5f};

  // True if a box overlaps any placed block.
  auto hitsBlock = [&](BoundingBox b) {
    for (const Object &o : blocks) {
      ObjectTransform t = o.getTransform();
      Vector3 h         = Vector3Scale(t.scale, 0.5f);
      if (CheckCollisionBoxes(b, {Vector3Subtract(t.pos, h), Vector3Add(t.pos, h)})) {
        return true;
      }
    }
    return false;
  };
  // The player's body box, bottom at the feet (translation), scale tall.
  auto blocked = [&](Vector3 feet) {
    return hitsBlock({Vector3Subtract(feet, half),
                      Vector3Add(Vector3Subtract(feet, half), transform.scale)});
  };

  // Grounded = on the floor plane, or a thin slab just under the feet touches
  // a block. A foot slab (not the whole body) so standing beside a wall
  // doesn't count as standing on it. Recomputed each frame -> walk off a
  // ledge and you start falling next frame.
  BoundingBox feetSlab{Vector3Subtract(transform.translation, {half.x, 0.2f, half.z}),
                       Vector3Add(transform.translation, {half.x, 0.0f, half.z})};
  onGround = transform.translation.y <= GROUND_Y || hitsBlock(feetSlab);
  if (onGround && velocity.y <= 0.0f) {
    velocity.y = 0.0f;
    if (transform.translation.y < GROUND_Y) {
      transform.translation.y = GROUND_Y;
    }
  }

  Vector3 moveDir = Vector3Zero();

  if (inputEnabled) {
    if (IsKeyDown(KEY_W))
      moveDir = Vector3Add(moveDir, moveForward);
    if (IsKeyDown(KEY_S))
      moveDir = Vector3Subtract(moveDir, moveForward);
    if (IsKeyDown(KEY_D))
      moveDir = Vector3Add(moveDir, moveRight);
    if (IsKeyDown(KEY_A))
      moveDir = Vector3Subtract(moveDir, moveRight);
  }

  float multiplier = onGround ? 1.0f : 0.05f;
  if (IsKeyDown(KEY_LEFT_SHIFT)) {
    multiplier *= 0.2f;
  }

  if (Vector3Length(moveDir) > 0.0f) {
    moveDir  = Vector3Normalize(moveDir); // prevents diagonal movement being faster
    velocity = Vector3Add(velocity, Vector3Scale(moveDir, speed * multiplier * dt * 60));
  }

  if (inputEnabled && onGround && IsKeyDown(KEY_SPACE)) {
    onGround   = false;
    velocity.y = jumpPower;
  }

  Vector2 horizontalVel = {velocity.x, velocity.z};
  if (Vector2Length(horizontalVel) > 50.0f) {
    horizontalVel = Vector2Scale(Vector2Normalize(horizontalVel), 50.0f);
    velocity.x    = horizontalVel.x;
    velocity.z    = horizontalVel.y;
  }
  float damping = powf(onGround ? 0.7f : 0.9f, dt * 60.0f);
  velocity.x *= damping;
  velocity.z *= damping;
  if (!onGround) {
    velocity.y -= GRAVITY * dt;
    // Terminal velocity: cap how fast we can fall. Upper bound is jumpPower so
    // an upward launch is never clamped away.
    constexpr float TERMINAL_VELOCITY = -60.0f;
    velocity.y                        = Clamp(velocity.y, TERMINAL_VELOCITY, jumpPower);
  }
  // to stop tiny fractions
  if (Vector3LengthSqr(velocity) < 0.01f) {
    velocity = Vector3Zero();
  }

  // ==== move + collide, one axis at a time ====
  Vector3 pos  = transform.translation;
  Vector3 step = Vector3Scale(velocity, dt);

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
      onGround = true; // landed on a block top
    }
    pos.y -= step.y;
    velocity.y = 0.0f;
  }

  transform.translation = pos;

  // ==== mouse rotaton =====
  Vector2 mouseDelta = GetMouseDelta();
  if (!inputEnabled)
    mouseDelta = {0.0f, 0.0f};

  float mouseSensitivity = 0.003f;

  yaw -= mouseDelta.x * mouseSensitivity;
  pitch -= mouseDelta.y * mouseSensitivity;

  float pitchLimit = 89.0f * DEG2RAD;
  pitch            = Clamp(pitch, -pitchLimit, pitchLimit);

  transform.rotation = QuaternionFromEuler(pitch, yaw, 0.0f);

  UpdateCamera(camera);
}

// Separate from Update because the camera has to follow the body even when
// movement is frozen - a respawn while paused moves us, and the view has to
// come along or it looks like the respawn never happened.
void Player::UpdateCamera(Camera3D &camera) const {
  Vector3 lookForward = Vector3RotateByQuaternion({0.0f, 0.0f, -1.0f}, transform.rotation);

  Vector3 head    = Vector3Add(transform.translation, {0.0f, transform.scale.y, 0.0f});
  camera.position = head;
  camera.target   = Vector3Add(head, lookForward);
}

Player::Player() {
  Vector3 spawnPos;
  spawnPos.x            = rand() % 200 - 100;
  spawnPos.z            = rand() % 200 - 100;
  spawnPos.y            = 10;
  transform.rotation    = QuaternionFromEuler(0, 0, 0);
  transform.scale       = {1.5f, 10, 1.5f};
  transform.translation = spawnPos;
  onGround              = false;

  DisableCursor();
}

void Player::DrawPlayer(const Transform &transform, const std::string &name, const Vector3 &localPos) {
  Matrix matScale    = MatrixScale(transform.scale.x, transform.scale.y, transform.scale.z);
  Matrix matRotation = QuaternionToMatrix(transform.rotation);
  Matrix matTranslation =
      MatrixTranslate(transform.translation.x, transform.translation.y, transform.translation.z);

  Matrix matTransform = MatrixMultiply(MatrixMultiply(matScale, matRotation), matTranslation);

  rlPushMatrix();
  rlMultMatrixf(MatrixToFloat(matTransform));
  DrawCubeV({0.0f, 0.5f, 0.0f}, {1, 1, 1}, WHITE);
  DrawCubeWiresV({0.0f, 0.5f, 0.0f}, {1, 1, 1}, BLACK);
  rlPopMatrix();

  Vector3 p                 = transform.translation + Vector3{0, transform.scale.y + 2.0f, 0};
  constexpr float FONT_SIZE = 2, SPACING = 0.05f;
  float halfW = MeasureTextEx(GetFontDefault(), name.c_str(), FONT_SIZE, SPACING).x * 0.5f;
  Vector3 d   = Vector3Subtract(localPos, p); // pos difference

  rlPushMatrix();
  rlTranslatef(p.x, p.y, p.z);
  rlRotatef(atan2f(d.x, d.z) * RAD2DEG, 0.0f, 1.0f, 0.0f);
  rlRotatef(90.0f, 1.0f, 0.0f, 0.0f);
  DrawText3D(GetFontDefault(), name.c_str(), {-halfW, 0, 0}, FONT_SIZE, SPACING, 1.0f, true, WHITE);
  rlPopMatrix();
}
