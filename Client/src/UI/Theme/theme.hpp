#pragma once
#include <raylib.h>

namespace Theme {
// Palette
inline constexpr Color Ground{13, 18, 38, 255};
inline constexpr Color GridLine{21, 28, 58, 255};
inline constexpr Color Blue{30, 76, 194, 255};
inline constexpr Color BlueHi{90, 140, 255, 255};
inline constexpr Color BlueDark{27, 58, 158, 255};
inline constexpr Color Red{230, 57, 70, 255};
inline constexpr Color RedHi{255, 92, 104, 255};
inline constexpr Color RedDark{143, 29, 44, 255};
inline constexpr Color LogoRed{255, 71, 87, 255};
inline constexpr Color Panel{26, 33, 66, 255};
inline constexpr Color PanelHi{38, 47, 92, 255};
inline constexpr Color PanelBorder{91, 102, 143, 255};
inline constexpr Color PanelShadow{7, 10, 24, 255};
inline constexpr Color Text{200, 230, 255, 255};
inline constexpr Color TextMuted{141, 150, 189, 255};
inline constexpr Color Focus{60, 100, 210, 255};

// Metrics
inline constexpr float Border = 4.0f;
inline constexpr float Depth = 8.0f;  // resting shadow depth

// A raised block: shadow, border, fill. Hover lifts it by half the depth,
// pressed sinks it onto its shadow. Returns the face rectangle (put text here).
inline Rectangle DrawBlock(Rectangle rec, Color fill, Color border,
                           Color shadow, bool hovered, bool down,
                           float depth = Depth) {
  Rectangle face = rec;
  if (down)
    face.y += depth;
  else if (hovered)
    face.y -= depth * 0.5f;

  if (!down)
    DrawRectangleRec(Rectangle{rec.x, rec.y + depth, rec.width, rec.height},
                     shadow);
  DrawRectangleRec(face, border);
  DrawRectangleRec(Rectangle{face.x + Border, face.y + Border,
                             face.width - Border * 2, face.height - Border * 2},
                   fill);
  return face;
}

// A sunken track (the slider bar): border plus dark fill. Returns the inner
// rectangle.
inline Rectangle DrawTrack(Rectangle rec) {
  DrawRectangleRec(rec, PanelBorder);
  Rectangle inner{rec.x + Border, rec.y + Border, rec.width - Border * 2,
                  rec.height - Border * 2};
  DrawRectangleRec(inner, Panel);
  return inner;
}
}  // namespace Theme