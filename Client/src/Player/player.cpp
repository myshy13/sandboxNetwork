#include "Player/player.hpp"
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#define PLAYERCOLOR RED

constexpr float GRAVITY = 140.0f;

void Player::Update(float dt, Camera3D &camera) {
  if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
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
  if (velocity.y <= 0.0f && transform.translation.y <= GROUND_Y) {
    onGround                = true;
    velocity.y              = 0;
    transform.translation.y = GROUND_Y;
  }

  Vector3 moveDir = Vector3Zero();

  if (IsKeyDown(KEY_W))
    moveDir = Vector3Add(moveDir, moveForward);
  if (IsKeyDown(KEY_S))
    moveDir = Vector3Subtract(moveDir, moveForward);
  if (IsKeyDown(KEY_D))
    moveDir = Vector3Add(moveDir, moveRight);
  if (IsKeyDown(KEY_A))
    moveDir = Vector3Subtract(moveDir, moveRight);

  float multiplier = onGround ? 1.0f : 0.05f;
  if (IsKeyDown(KEY_LEFT_SHIFT)) {
    multiplier *= 0.2f;
  }

  if (Vector3Length(moveDir) > 0.0f) {
    moveDir  = Vector3Normalize(moveDir); // prevents diagonal movement being faster
    velocity = Vector3Add(velocity, Vector3Scale(moveDir, speed * multiplier));
  }

  if (onGround && IsKeyDown(KEY_SPACE)) {
    onGround   = false;
    velocity.y = jumpPower;
  }

  Vector2 horizontalVel = {velocity.x, velocity.z};
  if (Vector2Length(horizontalVel) > 50.0f) {
    horizontalVel = Vector2Scale(Vector2Normalize(horizontalVel), 50.0f);
    velocity.x    = horizontalVel.x;
    velocity.z    = horizontalVel.y;
  }
  if (onGround) {
    float damping = powf(0.7f, dt * 60.0f);
    velocity.x *= damping;
    velocity.z *= damping;
  }
  if (!onGround) {
    velocity.y -= GRAVITY * dt;
    velocity.y = Clamp(velocity.y, -20.0f, jumpPower);
  }
  // to stop tiny fractions
  if (Vector3LengthSqr(velocity) < 0.01f) {
    velocity = Vector3Zero();
  }

  transform.translation = Vector3Add(transform.translation, Vector3Scale(velocity, dt));

  // ==== mouse rotaton =====
  Vector2 mouseDelta = GetMouseDelta();

  float mouseSensitivity = 0.003f;

  yaw -= mouseDelta.x * mouseSensitivity;
  pitch -= mouseDelta.y * mouseSensitivity;

  float pitchLimit = 89.0f * DEG2RAD;
  pitch            = Clamp(pitch, -pitchLimit, pitchLimit);

  transform.rotation = QuaternionFromEuler(pitch, yaw, 0.0f);

  if (IsKeyPressed(KEY_C)) {
    perspective = perspective == FirstPerson ? ThirdPerson : FirstPerson;
  }

  // ==== update camera ====
  Vector3 head = Vector3Add(transform.translation, {0.0f, transform.scale.y, 0.0f});
  if (perspective == ThirdPerson) {
    Vector3 up      = {0.0f, 1.0f, 0.0f};
    camera.position = Vector3Add(head, Vector3Subtract(up, Vector3Scale(lookForward, 5.0f)));
    camera.target   = head;
  } else {
    camera.position = head;
    camera.target   = Vector3Add(head, lookForward);
  }
}

Player::Player() {
  transform.rotation    = QuaternionFromEuler(0, 0, 0);
  transform.scale       = {2, 10, 2};
  transform.translation = {0, 10, 0};
  onGround              = false;

  DisableCursor();
}

void Player::Draw() const {
  if (perspective == ThirdPerson) {
    Transform bodyTransform = transform;
    bodyTransform.rotation  = QuaternionFromEuler(0.0f, yaw, 0.0f);
    DrawPlayer(bodyTransform);
  }
};

void Player::DrawPlayer(const Transform &transform) {
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
}