// Drop-in replacement for raylib's DrawCubeWiresV that draws real geometry
// (cylinders + corner spheres) instead of 1px GL lines, so the edges have a
// controllable thickness and don't vanish/flicker depending on angle,
// distance or driver line-width limits.
//
// Usage:
//   #include "cubewires_thick.h"
//   DrawCubeWiresThickV(position, size, 0.05f, RED);   // thickness in world units

#ifndef CUBEWIRES_THICK_H
#define CUBEWIRES_THICK_H

#include "raylib.h"


// position  = cube center (same as DrawCubeWiresV)
// size      = full width/height/length (same as DrawCubeWiresV)
// thickness = line diameter in world units
static inline void DrawCubeWiresThickV(Vector3 position, Vector3 size, float thickness, Color color)
{
    const float r = thickness * 0.5f;
    const int sides = 6;                    // cylinder facets; raise for rounder lines

    const float hx = size.x * 0.5f;
    const float hy = size.y * 0.5f;
    const float hz = size.z * 0.5f;

    // 8 corners, index bits: x = bit0, y = bit1, z = bit2
    Vector3 c[8];
    for (int i = 0; i < 8; i++)
    {
        c[i] = (Vector3){
            position.x + ((i & 1) ? hx : -hx),
            position.y + ((i & 2) ? hy : -hy),
            position.z + ((i & 4) ? hz : -hz)
        };
    }

    // 12 edges: pairs of corners that differ in exactly one bit
    static const int edges[12][2] = {
        {0,1},{2,3},{4,5},{6,7},   // along X
        {0,2},{1,3},{4,6},{5,7},   // along Y
        {0,4},{1,5},{2,6},{3,7}    // along Z
    };

    for (int e = 0; e < 12; e++)
        DrawCylinderEx(c[edges[e][0]], c[edges[e][1]], r, r, sides, color);

    // Spheres at the corners hide the gaps where cylinders meet
    for (int i = 0; i < 8; i++)
        DrawSphereEx(c[i], r, 6, 6, color);
}

// Convenience overload matching DrawCubeWires(x, y, z, w, h, l, color)
static inline void DrawCubeWiresThick(Vector3 position, float width, float height, float length,
                                      float thickness, Color color)
{
    DrawCubeWiresThickV(position, (Vector3){ width, height, length }, thickness, color);
}

#endif // CUBEWIRES_THICK_H