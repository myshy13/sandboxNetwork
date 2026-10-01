# Plan: block types instead of colours

Today a block is "a cube with a `Color`": the server picks colours, they travel on the wire, and the client draws
what it is told. After this, a block is **a `BlockType`** (Grass, Dirt, Stone, Log, Leaves, Water). The server
only knows *what* a block is; *how it looks* is a client-only lookup. That is the step that later lets one type
get a texture without touching the server or the protocol.

## Design (decided)

- `Object` keeps `type` + `durability`; the `Color` field, its constructors and its serialization go away.
- `BlockType` = `Grass, Dirt, Stone, Log, Leaves, Water`. `Solid` is removed at the end: "solid" stops being a
  type and becomes a question, `isSolid(type)` (= not Water). Don't compare against `Solid` anywhere again.
- Colour lives in one client-only table, `blockColor(BlockType)` (`Client/src/World/blockStyle.hpp`), plus the
  durability darkening. Not in `Shared/`: the server must not need raylib's `Color`/`ColorBrightness`.
- Hotbar becomes six block types, `{Grass, Dirt, Stone, Log, Leaves, Water}`. Arbitrary colours (white, red,
  dark grey) are dropped on purpose; players pick a material, not a paint.
- Not changing: collision, fluid sim, chunk streaming, instancing (colour stays a per-instance attribute).

## Steps

Steps 1-2 are additive and leave the game exactly as it is. Step 3 is the one atomic change (the `Object`
constructor is shared, so server, client and tests can't move separately); let the compiler list what's left.

1. **Enum and helpers (additive).** In `Shared/Models/Object.hpp`: add the new enum values *next to* `Solid`,
   add `inline bool isSolid(BlockType)` and `inline bool isPlaceable(BlockType)`, and `constexpr int MAX_DURABILITY = 3`
   (replaces the literal `3` in `durability{3}`). Then swap every `== BlockType::Solid` / `!= BlockType::Water`
   test over to `isSolid` (`World::isSolid`, `World::boxCollides` default argument, `World::placeBlock`,
   `Server::handleReceive` placement check). Nothing renders or sends differently.
2. **Client style table (not wired yet).** `blockStyle.hpp`: `Color blockColor(BlockType)` with one `switch` and
   a `default` (never index an array by an enum that came off the wire), and
   `Color shaded(Color, int durability)` = darken once per lost durability point, using `MAX_DURABILITY`.
   Pick the colours to match today's look (grass green, dirt brown, water blue) so step 3 is visually a no-op.
3. **The switch (one commit).** In this order, so each compile error points at the next job:
   1. `Object`: drop `color`, `getColor`, the two colour constructors and the colour fields in `serialize`; `damage()`
      only decrements. Constructor becomes `(id, transform, type)`.
   2. Server: `generateChunk` passes `Grass` (top), `Dirt` (below, and the floor under water), `Water`.
      `TreeBlock` holds a `BlockType` (`Log` / `Leaves`) instead of a `Color`; `structures.hpp` stops including raylib.
      `Server::handleReceive` PlaceObject: build the block from `type` only, and replace the old whitelist with
      `isPlaceable(type)`.
   3. Client: `Renderer` (`o.getColor()` -> `shaded(blockColor(o.getType()), o.getDurability())`), the hotbar in
      `world.hpp` / `world.cpp` (`colors[]` -> `BlockType hotbar[]`, the HUD squares draw `blockColor`, the
      hard-coded `activeColor == 5` water test becomes `type == BlockType::Water`, and the "transparent slot is an
      empty hand" check needs a new rule: decide whether the hotbar keeps an empty slot).
   4. Tests: `Shared/Protocol/test_protocol.cpp` (asserts on `getColor()` become asserts on `getType()` and
      `getDurability()`; the comment about `ColorBrightness` can go) and `Server/test_fluid.cpp` (two `Object`
      constructions pass a colour).
   5. Versions: bump `proto::PROTOCOL_VERSION` (7 -> 8) and `env::saveFormatVersion` (2 -> 3). The server already
      refuses old saves with a clear message, so existing worlds must be moved aside.
4. **Cleanup.** Remove `Solid` from the enum (the compiler finds stragglers), make `TREE_SHAPE`
   `inline constexpr` instead of `static constexpr` (header rule), update `.claude/rules/arch.md` (Shared/Models
   description, "colour lives client-side"), tick the backlog, and delete the stash `colour from type + damage (WIP)`.
5. **Later, not now: textures.** The renderer batches by solid/water with a colour per instance. A textured block
   would swap the colour attribute for an atlas index per instance (and `blockStyle` would return a tile instead of
   a colour). Same table, same call sites; that is why `blockStyle` is the single place that knows appearance.

## Tricky parts

- **An enum from the wire is untrusted.** cereal reads the `uint8_t` raw, so a client can send `type = 200` in
  `PlaceObject`. `isPlaceable` must reject out-of-range values *before* anything uses the type, and the client's
  `blockColor` needs the `default` branch for the same reason.
- **Old saves break.** `Object`'s serialized layout changes, so old chunk files misread rather than fail loudly.
  That is exactly what the `saveFormatVersion` bump is for; don't skip it.
- **Visual no-op check.** Darkening moves from "multiply the stored colour on each hit" to "recompute from type and
  durability on draw". Check that a fully damaged block looks the same as before; the old factor was 0.25 per hit.
- **`Server/` tests build `Object` too.** `test_fluid.cpp` is outside `src/`, so a grep of `src/` misses it.
- **Fluid must not care.** `FluidSim` only asks `getType() == Water`; after step 3 it should need no changes.

## Existing WIP

`git stash show -p stash@{0}` already has a draft of 3.1 and part of 3.2 (it keeps `typeToColor` in `Shared`,
which this plan moves to the client). Use it as a reference, don't pop it onto the current tree: it predates the
trees rebase and will conflict in `server.cpp`.
