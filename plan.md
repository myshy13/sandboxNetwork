# Plan: block types (Grass / Dirt / Water / ...) and textures

Goal: a block's *type* decides how it looks and behaves; its colour is only a fallback look (and the damage shading).
New block = one enum value + one `BLOCK_INFO` row + one texture entry.

## Where things stand

- [x] `BlockType { Grass, Dirt, Water, Leaves, Wood }` + `BLOCK_INFO` (solid, placeable, fluid, colour) in `Shared/Models/Object.hpp`
- [x] Colour is derived, never stored or sent: `Object::getColor()` = the type's `BLOCK_INFO` colour, a quarter darker per
      point of damage. The only writers are the type (at construction) and `damage()`
- [x] `PROTOCOL_VERSION` 9 / `saveFormatVersion` 4 (colour left the wire and the save files)
- [x] Server terrain and trees pass types only; the server ignores everything but the type in `PlaceObject`
- [x] Hotbar is a list of `BlockType`s (`Game::blockTypes`); slots draw their colour, with the texture over it if `blockTex` has one
- [x] `BLOCK_TEX` / `blockTex()` in `Client/src/AssetManager/blockTex.hpp` (Grass, Dirt have a texture; the rest are `nullopt`)
- [x] Swimming and "place over it" use the `fluid` flag instead of `== BlockType::Water`
- [ ] The world itself is still drawn from colours: nothing in the renderer reads `BLOCK_TEX` yet (step 4)

## Design (one recommendation)

**One instanced batch per texture, not an atlas.**
The fragment shader already multiplies `texture0` by `fragColor`, and the face quad already has UVs 0..1. So
texturing is: swap `cubeMat.maps[MATERIAL_MAP_DIFFUSE].texture` before each `drawBatch`. No shader change, no new
vertex attribute. An atlas (tile index per instance) only pays off once there are ~10+ textures; revisit then.

**Colour is a tint**, derived from type + damage. Textured blocks get a `WHITE` base colour so only damage shades them.

**Texture choice lives on the client**, indexed by `BlockType`. The server and `Shared` never see textures.

## Steps

1. **Assets** — grass and dirt tiles exist. Still needed: a `grass_top`/`grass_side` split, `leaves`, `wood`, `water`
   (16x16 or 32x32) in `Client/assets/images/`.
2. **AssetManager** — add a `Tex` entry + `TEXTURE_PATHS` row per new tile (same order, the `static_assert` guards it).
   Set `SetTextureFilter(..., TEXTURE_FILTER_POINT)` after loading so blocks look crisp, not blurred.
3. **Face -> texture table** — `BLOCK_TEX` holds one texture per type today. Grass needs {top, side, bottom}, so widen
   the rows to three slots. Face index 2 is +Y (top), 3 is -Y (bottom), the rest are sides (matches `FACE_DIR`).
4. **Renderer batches** — today there are two batches (opaque, water). Replace the two `instanceMats/instanceColors`
   vectors with one pair *per `Tex`* (arrays sized `Tex::Count`, plus the existing water set). In `drawObjects`, push
   each face into the batch picked by `blockTex`; blocks with `nullopt` go into a plain colour batch. At draw time loop
   the batches: set the material texture, then `drawBatch`.
   - *Tricky:* `currentBuffer` rotates through `BUFFER_COUNT` VBO slots per `drawBatch` call. More batches per frame
     means more slots needed, or a slot reused within a frame gets overwritten before the GPU reads it.
   - *Tricky:* water stays last and with depth writes off (see the existing comment).
5. **Flip colours to tints** — once a type has a texture, set its `BLOCK_INFO` colour to `WHITE` so the texture isn't
   tinted green/brown. Damage still darkens it, which is the intended use.
6. **Leaves + opacity** — if the leaf texture has see-through holes, add an `opaque` flag next to `solid`: physics asks
   `solid`, face culling asks `opaque`.
7. **Tests** — `cd Shared/Protocol && make test` and `cd Server && make test` (both already cover the colour rules).
8. **Adding a type later (the recipe this plan proves)**
   1. enum value before `Count` in `BlockType`
   2. row in `BLOCK_INFO` (the compiler checks the count, not the order: match the enum by eye)
   3. `Tex` entry + path row, then a `BLOCK_TEX` row (`std::nullopt` until the art exists)
   4. add it to `Game::blockTypes` if players can place it
   5. bump `PROTOCOL_VERSION` and `saveFormatVersion` (type values on the wire/disk changed)
   6. server terrain/placement if it spawns naturally
   Candidates: Sand, Stone, Snow (matches the biome item in `backlog.md`).
9. **Docs** — `.claude/rules/arch.md` describes block types and colour; update it when the renderer batches per
   texture, and tick the matching `backlog.md` line.

## Later (not in this plan)

- Texture atlas + per-instance tile index, once the batch count gets annoying.
- Water animation (scroll UV by `GetTime()` in the shader).
- Non-cube block shapes (ramp, slab) — own item in `backlog.md`.
