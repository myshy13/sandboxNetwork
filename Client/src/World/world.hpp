#pragma once

#include "Client/client.hpp"
#include "Models/Object.hpp"
#include <raylib.h>
#include <unordered_set>
#include <vector>

#define MAX_COLOURS 6

class World {
private:
  const Color colors[MAX_COLOURS] = {WHITE, BROWN, GREEN, DARKGRAY, RED, BLUE};
  int activeColor                 = 0;
  std::vector<Object> objects{};

public:
  void drawHud();
  bool placeBlock(Ray aim, Client &client, const Vector3 &playerPos);
  void addObject(const Object &object);
  void update();
  void removeObject(int id);
  void setObjects(std::vector<Object> newObjects);
  void damageObject(int id);
  std::vector<Object> &getObjects();
  // bumped whenever objects are added/removed, so Renderer knows its spatial grid is stale
  int getVersion() const { return version; }
  bool isOccluded(const Object &o) const;

private:
  bool cellsDirty{true};
  int version = 0;

  void rebuildOccupiedCells();

  std::unordered_set<int64_t> occupiedCells;
};
