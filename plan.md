# Day time and lighting

## Steps

1. Add a time variable to the meta.bin file and the server
2. send that to the client in the handshake (During server connection handler) Bump `PROTOCOL_VERSION` by 1
3. create the handler in the client. **Optional:** Add it to the debug menu
4. move the lights and change the light color based on the time in client

# Lighting prerequisites and shadows

Builds on the day time steps above: shadows need a sun direction that follows the time, and the shader's light and the
shadow rays must read the same direction or lit and shaded sides will disagree.

## Before the time steps (so the look is right first)

- [x] Gamma: convert the texture's rgb to linear right after sampling it in `assets/shaders/glsl330/lighting.fs` and
      `glsl100/lighting.fs` (`pow` by 2.2, alpha untouched). The final `pow(1/2.2)` already exists; this is its other half
- [x] One sun: keep a single `addDirectional` in `Game::Game` instead of three (two overhead lights stack to ~1.7x on top
      faces and clip). Raise ambient (the `ambient / 10.0` in the shader) so shaded faces stay visible but dark

## Time-of-day lighting (step 4 above, in detail)

- [x] `Lighting` gets an update call that moves the sun's direction and colour each frame from the synced time, since the
      lights are only created once today **Revision:** Every 0.1 seconds, not every frame
- [x] One function turns time into a sun direction; both the shader light and the shadow rays call it **After:** `Lighting.cpp` Owns the function

## Shadows (client only, cosmetic)

One sun shadow map, not per block (per-block rays can't reach 1/8-block accuracy and cost far too much on the CPU).
`Renderer::shadowMap` draws every opaque, exposed face within the render distance from the sun into a 2048x2048 depth
texture, through an orthographic camera centred on the player. The lighting shaders sample it (3x3 PCF) and scale the
sun's light; ambient is left alone. Nothing here touches the server, the protocol or `Object`.

Done:

- [x] Depth target: `Raylib/shadowMap.hpp` (`LoadShadowmapRenderTexture`, after raylib's shadowmap example)
- [x] The pass: `Renderer::shadowMap`, with `Renderer::faceMatrix` shared with `drawObjects` so the two can't drift
- [x] The sun's view-projection matrix is captured in the pass and handed over by `Lighting::setShadow` (texture slot 10)
- [x] Shader lookup in `glsl330` and `glsl100`; pixels outside the map count as lit
- [x] Settings toggle: `GameState` bool, settings screen button, `SHADOWS_DEFAULT` from `CMakeLists.txt` (off on web).
      `useShadows` makes the shader skip the lookup and `Game::frame` skips the pass

Left:

- [ ] Cache the map: redraw only when the texel-snapped centre moves, the sun has moved a few degrees, or a chunk went
      dirty (the pass costs ~6 ms every frame today)
- [x] Snap the camera centre to whole texels, or shadow edges crawl as you walk (the snap grid turns with the sun, so it
      still hops about once a second; see bug #3)
- [x] A shadow radius of its own (`Renderer::SHADOW_RADIUS`, 200 units, capped by the render distance): sharper map, ~6x
      fewer faces; the shaders fade the shadow out over the map's outer tenth
- [ ] A depth-only shader for the pass (the lighting shader does per-pixel work that is thrown away)
- [x] Slope-scaled bias in both shaders (`0.0005 * tan(angle to the normal)`, clamped 0.0001 to 0.005); written, not yet
      checked in the afternoon. Tune by eye (stripes = too small, floating shadows = too big)
- [ ] Night: skip the pass while the sun is below the horizon
- [ ] Web: check the depth-texture extension and `glsl100` in a browser once

Known limits: a low sun squashes the covered area into a thin ellipse of the map, so shadows blur along the sun's
direction; unloaded chunks count as open air, so shadows can pop in as chunks load; water casts none.

Later: ambient occlusion (darken corners where blocks meet, same neighbour-lookup idea at chunk rebuild).

Maybe later: cascaded shadow maps (2-3 maps of growing size around the player; the shader picks the smallest that
contains the pixel). Not wanted yet; do it only after the single map is snapped, cached and has its own radius.
