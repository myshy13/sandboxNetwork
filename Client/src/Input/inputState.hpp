#pragma once

#include <raylib.h>

#include <bitset>
#include <optional>

enum class Action {
  Shoot,
  Jump,
  Sneak,
  Place,

  Pause,
  Openchat,
  OpenChatCommands,
  TabKills,
  Click,

  OpenDebug,
  ShowChunkBorders,
  ShowCollision,
  ChangeClipping,
  Reconnect,
  Zoom,

  Count
};

struct InputState {
  std::bitset<static_cast<size_t>(Action::Count)> actions;
  Vector2 move{0, 0};
  float scroll{0}; // y only
  Vector2 look{0, 0}; // current frame's look delta
  Vector2 pointer{0,0};
  std::optional<int> newHotBarSlot;
};
