#pragma once
#include <raylib.h>

#include <vector>

struct Particle {
  Vector2 home;  // where it lives (spawn point)
  Vector2 pos;   // where it's drawn now
  float size;
  float depth;  // 0.3..1.0, how strongly it reacts (bigger = closer)
  float phase;  // desyncs the bobbing
  Color color;
};

constexpr float PARALLAX = 40.0f;  // max px of mouse-driven shift
constexpr float BOB = 12.0f;       // px of up/down bobbing
constexpr float SMOOTH = 6.0f;     // higher = snappier
constexpr float REPEL_R = 120.0f;  // optional: push away from the cursor
constexpr float REPEL_K = 0.6f;

class Background {
  struct Particle {
    Vector2 home;
    Vector2 pos;
    float size;
    float depth;
    float phase;
    Color color;
  };

  std::vector<Particle> particles;

 public:
  explicit Background(int count = 32);  // spawns the particles
  void frame();                         // update + draw, call once per frame
};