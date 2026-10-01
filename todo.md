# Todo

- `test_fluid.cpp` has uncommitted changes sitting next to the tree work — check `git diff Server/test_fluid.cpp` to make sure that's intentional before it rides along in a tree commit.
- `hasTree`'s neighbor search (`SPACING=4`, 80 hash calls per candidate column) — fine for now, but worth a comment or a note if chunk generation ever starts feeling slow, since it's an easy thing to forget was added for tree spacing.
- Water check in `generateChunk` — confirm `hasTree` is still only ever reached inside the `height >= env::WATER_HEIGHT` branch so trees never generate underwater; easy to accidentally break if that branch gets refactored later.
- Low priority / only if you feel like it: pick a second tree variant (different canopy shape or color) so forests don't look like one tree copy-pasted everywhere.
