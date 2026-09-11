#include "world.hpp"
#include "Client/client.hpp"
#include "Models/Object.hpp"
#include <cfloat>
#include <cmath>
#include <raylib.h>
#include <raymath.h>

// Axis-aligned box centred on an object (pos is the centre; see placeBlock).
static BoundingBox objectBox(const ObjectTransform &t) {
  Vector3 half = Vector3Scale(t.scale, 0.5f);
  return {Vector3Subtract(t.pos, half), Vector3Add(t.pos, half)};
}

void World::draw(const Ray &facing) {
  DrawPlane({0, 0, 0}, {800, 800}, Color{45, 55, 45, 255}); // dark enough for the grid lines to read against it
  DrawGrid(40, 20);

  Object *targeted   = nullptr;
  float bestDistance = FLT_MAX;
  for (Object &o : objects) {
    ObjectTransform t = o.getTransform();
    DrawCubeV(t.pos, t.scale, o.getColor());
    RayCollision rc = GetRayCollisionBox(facing, objectBox(t));
    if (rc.hit && rc.distance < bestDistance) {
      bestDistance = rc.distance;
      targeted     = &o;
    }
  }
  if (targeted != nullptr) {
    ObjectTransform t = targeted->getTransform();
    if (bestDistance <= REACH) {
      DrawCubeWiresV(t.pos, t.scale, BLACK);
    }
  }
};

void World::drawHud() {
  constexpr float BOXSIZE = 50.0f; // square
  for (int i = 0; i < MAX_COLOURS; i++) {
    const Color &color = colors[i];
    if (activeColor == i) {
      DrawRectangle(GetScreenWidth() - BOXSIZE * MAX_COLOURS + i * BOXSIZE, GetScreenHeight() - BOXSIZE, BOXSIZE, BOXSIZE, WHITE);
    } else {
      DrawRectangle(GetScreenWidth() - BOXSIZE * MAX_COLOURS + i * BOXSIZE, GetScreenHeight() - BOXSIZE, BOXSIZE, BOXSIZE, GRAY);
    }
    DrawRectangle(GetScreenWidth() - BOXSIZE * MAX_COLOURS + i * BOXSIZE + 5, GetScreenHeight() - BOXSIZE + 5, BOXSIZE - 10, BOXSIZE - 10, color);
  }
}

void World::update() {
  int key = GetKeyPressed();
  if (key >= KEY_ONE && key < KEY_ONE + MAX_COLOURS) {
    activeColor = key - KEY_ONE;
  }
}

// Every placed block is this size, and the build grid has cells this size.
constexpr Vector3 blockSize = {5, 5, 5};

// Snap a world point to the centre of its blockSize-grid cell.
static Vector3 snapToCell(Vector3 p) {
  return {(floorf(p.x / blockSize.x) + 0.5f) * blockSize.x,
          (floorf(p.y / blockSize.y) + 0.5f) * blockSize.y,
          (floorf(p.z / blockSize.z) + 0.5f) * blockSize.z};
}

bool World::placeBlock(Ray aim, Client &client, const Vector3 &playerPos) {
  RayCollision best{};
  best.distance = FLT_MAX;
  for (Object &o : objects) {
    RayCollision rc = GetRayCollisionBox(aim, objectBox(o.getTransform()));
    if (rc.hit && rc.distance < best.distance) {
      best = rc;
    }
  }

  Vector3 target;
  if (best.distance != FLT_MAX) {
    target = Vector3Add(best.point, Vector3Multiply(best.normal, Vector3Scale(blockSize, 0.5f)));
  } else if (aim.direction.y < 0.0f) {
    float dist = -aim.position.y / aim.direction.y;
    target     = Vector3Add(aim.position, Vector3Scale(aim.direction, dist));
  } else {
    return false; // aiming at the sky
  }

  if (Vector3Distance(aim.position, target) > REACH) {
    return false; // too far away
  }

  Vector3 cell = snapToCell(target);

  for (Object &o : objects) {
    if (Vector3DistanceSqr(o.getTransform().pos, cell) < 0.01f) {
      return false;
    }
  }

  constexpr Vector3 PLAYER_SCALE = {1.5f, 10.0f, 1.5f};

  BoundingBox player;
  player.min = Vector3Subtract(
      playerPos, {PLAYER_SCALE.x * 0.5f, 0.0f, PLAYER_SCALE.z * 0.5f});
  player.max = Vector3Add(player.min, PLAYER_SCALE);

  // cell is the block's centre (see objectBox / snapToCell), not a corner.
  BoundingBox block = objectBox(ObjectTransform{cell, blockSize});

  if (!CheckCollisionBoxes(player, block)) {
    client.placeObject(Object{-1, ObjectTransform{cell, blockSize}, colors[activeColor]});
    return true;
  } else {
    return false;
  }
}

void World::addObject(const Object &object) {
  objects.push_back(object);
}

void World::removeObject(int id) {
  std::erase_if(objects, [id](const Object &o) { return o.getId() == id; });
}

void World::damageObject(int id) {
  for (Object &o : objects) {
    if (o.getId() == id) {
      o.damage();
    }
  }
}

const std::vector<Object> &World::getObjects() const {
  return objects;
}