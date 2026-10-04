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

## Shadows (client only, cosmetic) (not done yet)

Per block, not per face: one "how sunlit" value per visible block. Nothing here touches the server, the protocol or
`Object` (arch.md: the client only simulates cosmetics).

- [ ] Per-block visibility: from just outside the block, step toward the sun in about half-cell steps (max ~10 cells) and
      ask `World::isSolid` at each step; any hit means shadowed. Start outside the block's own cell or every block
      shadows itself
- [ ] Store it in `GridCell` next to `faceMasks` (parallel to `indices`), filled when a chunk is rebuilt
- [ ] Recompute when the sun has moved a few degrees, spread over frames with a per-frame chunk budget (like the
      server's `CHUNKS_PER_TICK`); fade between the old and new value so blocks don't pop
- [ ] Apply it when `drawObjects` builds a block's colour: multiply the rgb by a darken factor (~0.5) if shadowed. No
      shader change for a first version (it also dims ambient; a per-instance sun-only factor is the proper later fix)
- [ ] Night: sun below the horizon skips the rays and shadows everything
- [ ] Settings toggle: `GameState` holds the bool, the settings screen writes it. The default comes from a
      `SHADOWS_DEFAULT` value defined in `CMakeLists.txt` (off in the `EMSCRIPTEN` branch, on native) and read as a
      plain `constexpr bool`, so there is no `#ifdef` in game code

Known limits: hard-edged per-block shadows; a tree in a neighbouring chunk can change a shadow without dirtying this
chunk (accept, or dirty the neighbours); unloaded chunks count as open air, so shadows can pop in as chunks load; water
casts none (not solid).

Later: ambient occlusion (darken corners where blocks meet, same neighbour-lookup idea at chunk rebuild).
