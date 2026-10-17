#include "UI/Background/background.hpp"

#include <cmath>

#include "UI/Theme/theme.hpp"

Background::Background(int count) {
  const int sizes[3] = {16, 24, 32};
  for (int i = 0; i < count; i++) {
    Particle p;
    p.home = {(float)GetRandomValue(0, GetScreenWidth()),
              (float)GetRandomValue(0, GetScreenHeight())};
    p.pos = p.home;
    p.size = (float)sizes[GetRandomValue(0, 2)];
    p.depth = 0.3f + (p.size - 16.0f) / 16.0f * 0.7f;  // bigger = reacts more
    p.phase = GetRandomValue(0, 628) / 100.0f;
    p.color = (i % 2 == 0) ? Theme::Blue : Theme::LogoRed;
    p.color.a = 110;  // see-through, so it stays behind the UI
    particles.push_back(p);
  }
}

void Background::frame() {
  const float dt = GetFrameTime();
  const Vector2 mouse = GetMousePosition();
  const Vector2 mid = {GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f};
  // Mouse position relative to screen centre, -1..1 on each axis
  const Vector2 m = {(mouse.x - mid.x) / mid.x, (mouse.y - mid.y) / mid.y};

  for (int x = 0; x < GetScreenWidth(); x += 40)
    DrawRectangle(x, 0, 2, GetScreenHeight(), Theme::GridLine);
  for (int y = 0; y < GetScreenHeight(); y += 40)
    DrawRectangle(0, y, GetScreenWidth(), 2, Theme::GridLine);

  for (Particle& p : particles) {
    // Target = home, shifted toward the mouse a little (parallax) plus a bob
    Vector2 target = {p.home.x + m.x * PARALLAX * p.depth,
                      p.home.y + m.y * PARALLAX * p.depth +
                          std::sinf((float)GetTime() * 1.5f + p.phase) * BOB};

    // Optional: shove it away from the cursor when the cursor gets close
    const float dx = target.x - mouse.x, dy = target.y - mouse.y;
    const float dist = std::sqrtf(dx * dx + dy * dy);
    if (dist < REPEL_R && dist > 0.001f) {
      const float push = (REPEL_R - dist) * REPEL_K;
      target.x += dx / dist * push;
      target.y += dy / dist * push;
    }

    // Frame-rate independent smoothing toward the target
    const float k = 1.0f - std::expf(-SMOOTH * dt);
    p.pos.x += (target.x - p.pos.x) * k;
    p.pos.y += (target.y - p.pos.y) * k;

    DrawRectangle((int)std::roundf(p.pos.x), (int)std::roundf(p.pos.y),
                  (int)p.size, (int)p.size, p.color);
  }
}