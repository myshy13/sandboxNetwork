#pragma once

#include "Client/client.hpp"
#include "Models/Object.hpp"
#include <raylib.h>
#include <vector>

#define MAX_COLOURS 5
#define REACH 50.0f

class World {
private:
  const Color colors[MAX_COLOURS] = {WHITE, BROWN, GREEN, DARKGRAY, RED};
  int activeColor                 = 0;
  std::vector<Object> objects{};

public:
  void draw(const Ray &facing);
  void drawHud();
  bool placeBlock(Ray aim, Client &client, const Vector3 &playerPos);
  void addObject(const Object &object);
  void update();
  void removeObject(int id);
  void damageObject(int id);
  const std::vector<Object> &getObjects() const;
};