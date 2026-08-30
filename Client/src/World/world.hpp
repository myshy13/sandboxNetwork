#pragma once

#include "Client/client.hpp"
#include "Models/Object.hpp"
#include <raylib.h>
#include <vector>

class World {
  private:
    std::vector<Object> objects{};
  public:
    void draw();
    void placeBlock(Ray aim, Client &client);
    void addObject(const Object &object);
    void removeObject(int id);
    const std::vector<Object> &getObjects() const;
};