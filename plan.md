# Block registry (so a new block is a row, not a hunt)

Goal: the enum stays each block's identity (one byte, on the wire and in saves); everything else about a block is a
lookup on it. Today `== BlockType::Water` is tested about 17 times across the renderer, world, server and `FluidSim`,
and one `solid` flag has to mean both "players collide" and "hides the face behind it". Stage 1 is the cheap, mechanical
part. Per-block behaviour hooks wait for a second block that has logic (sand, lava), so they are designed from two real
cases, not from water alone. `Object` stays small: ~843,000 live, so no per-block pointers or virtuals.

## Stage 1 steps

1. **A row can't be misordered or misread.** In `Shared/Models/Object.hpp`: give `BlockInfo` a `type` field and use
   designated fields with defaults, so a row names only what differs (`.type = Water, .fluid = true, .translucent = true`).
   Add a compile-time check that row `i` has `type == i`; it replaces "match the enum by eye". Designated initialisers
   must follow the declaration order. Update `test_protocol.cpp` if it indexes the table positionally.
2. **Split "solid" from "hides what is behind it".** Add `opaque` (blocks light and hides the faces behind it). Today
   `rebuildChunk` culls a face whenever the neighbour `isSolid`, so a see-through but solid block would punch holes in
   its neighbours. Culling asks `opaque`; physics keeps asking `solid`. Add `World::occludes(pos)` next to `isSolid`.
   Also: a face between two blocks of the same translucent type is skipped (water-on-water already does this by hand).
3. **Replace the name checks with capability queries.** Outside `FluidSim` (which *is* the water behaviour) swap each
   `== BlockType::Water` for `isFluid(type)` or a new flag: `renderer.cpp` (2), `world.cpp` (3), `server.cpp` (~8).
   Mechanical; leave `fluidSim.cpp` alone until lava gives it a second case.
4. **Rename `Object::level` to `state`.** Water's level is really "this block's own state byte" (a crop's growth, a
   stair's facing). Same position in `serialize`, so the wire and the saves don't change.
5. **Hotbar from the table.** Build `Game::blockTypes` from the `placeable` rows, so adding a block doesn't mean editing a
   second list.
6. **Docs.** Update the "Block types" paragraph in `.claude/rules/arch.md`, and put the add-a-block recipe in it: enum
   value before `Count`, a `BLOCK_INFO` row, a `Tex` entry + path row + `BLOCK_TEX` row, the hotbar, and a
   `PROTOCOL_VERSION` bump.

Tricky: step 2 (an `opaque` that disagrees with the renderer's cull shows as holes in the world); step 3 (a missed site
keeps treating the new block as plain). Stage 1 changes nothing on the wire or on disk, so no version bump.

## Then: glass

- [ ] enum value + row: `solid`, `placeable`, `translucent`, **not** `opaque` (needs step 2); a 16x16 texture with a
      `Tex` entry, path row and `BLOCK_TEX` row (planks, already wired by the creator, is the worked example)
- [ ] decide whether glass casts a shadow (the shadow pass skips translucent blocks today, so it casts none)
- [ ] bump `PROTOCOL_VERSION` (a new block value on the wire; old saves still load)

## Stage 2 (when a second block has logic)

A server-side table of optional function pointers per block (`onTick`, `onNeighbourChange`, `onPlace`, `onBreak`,
`nullptr` = nothing); water's flow becomes its entry. A client-side shape per block (cube, or water's variable height).

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
- [x] A shadow radius of its own (`GameState::getShadowRadius()`, default 200 units, 50 to 400 from a slider on the
      settings screen, capped by the render distance): sharper map, ~6x fewer faces at the default; the shaders fade the
      shadow out over the map's outer tenth
- [ ] A depth-only shader for the pass (the lighting shader does per-pixel work that is thrown away)
- [x] Slope-scaled bias in both shaders (`0.0005 * tan(angle to the normal)`, clamped 0.0001 to 0.005); checked in the
      afternoon, the stripes are gone. Tune by eye if it changes (stripes = too small, floating shadows = too big)
- [ ] Night: skip the pass while the sun is below the horizon
- [ ] Web: check the depth-texture extension and `glsl100` in a browser once

Known limits: a low sun squashes the covered area into a thin ellipse of the map, so shadows blur along the sun's
direction; unloaded chunks count as open air, so shadows can pop in as chunks load; water casts none.

Later: ambient occlusion (darken corners where blocks meet, same neighbour-lookup idea at chunk rebuild).

Maybe later: cascaded shadow maps (2-3 maps of growing size around the player; the shader picks the smallest that
contains the pixel). Not wanted yet; do it only after the single map is snapped, cached and has its own radius.
