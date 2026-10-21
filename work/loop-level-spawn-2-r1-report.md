STATUS: COMPLETE

Native job level-spawn-2 is local on `codex/level-spawn-2` in `worktrees/level-spawn-2`, branched from `codex/playable-integration` at `037e26d`. Not pushed.

A cold story start whose radius-12 snap is the sentinel, with no Player1 and with an `ArenaPlayerStart`, now uses the mode-A wave start: ring radius 180, slot 0, count 1, `findFreeSpot` spread 36 then 240, then `player_ctor_z` at radius 12. The search uses an empty object list and seed 0, one of the two seeds section 18 executed. It is not the live cycle-counter seed. A level that already has a floor at the origin, or a Player1 that took, stays on the story path.

## Evidence

Levels research `t3/research-levels` at `13fafa465d5dad7acaeb1ee8becd30c4c8e4788b` (`docs/level-flow.md` section 18, `work/cursor-report.md`). This job did not re-run the ELF or the act/mission name search. The generator checked the pinned hashes, including `tools/difftest_levels_r4.py`, and appended the round-4 lines. The round 1–3 prefix of `level_flow_vectors.inc` is unchanged.

`cos16(0)` times 180 is the radius. `sin16(0)` times 180 is not 0. Slot 0's angle is 0 for every positive count, so that centre does not depend on count.

## Refusals kept

- `arena_pickSpawnPoint` with a0 = 0 (`arena_respawn`).
- The unchecked net-peer index at message offset 0xf4 (`arena_net_peer_index`).
- The live seed at `Player1_ctor`, and the class-constructor radii inside `findFreeSpot`.
- The mode-A name table. The index step is `arena_mode_a_next`. The names are not copied.

## Files

- `53c4883` — mode-A ring, record offset, spawn-table register, wave ground, the story fallback, synthetic vectors and `round4_cases`, contract pin, `docs/LEVEL_FLOW.md`.
- `98dc5a8` — cold-arena counts in `champions-level-spawn-measure`. The origin-test pass is unchanged.

## Commands

Arm64 Release:

```
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -j 3 --output-on-failure
```

Build exit 0. CTest 196/196 passed, 675.84 s. No timeout, so nothing was rerun alone. `champions-level-flow-tests` alone reported 10335 exact checks. x86_64 was not run.

The archive extension was rebuilt Release / `template_debug` / arm64 and linked into `godot/bin`. The level-texture libraries are the unchanged playable-integration binaries. The creature-animation debug library was built so the three creature-anim smokes had a library to load. These extension builds are arm64, not universal.

## Private disc (counts only)

`champions-level-spawn-measure` on the paired archive directory, read-only:

- paired 111, origin-test 18, story ground 18/18
- origin radius-4 floor 11, origin radius-12 floor 11, marker radius-12 floor 18
- empty-list search snap 36/36, arena floor 2/2, scenario misses 0
- geometry skipped 62, decode failures 0, measure failures 0
- cold 3, cold origin floor 0, start floor 3, ring floor 3
- cold search snap 6/6, stand-in 6/6, lattice misses 0, story ground 3/3
- cold water chunks 7, 24, 112; alternates 7, 16, 26
- cold decode failures 0, stand-in failures 0
- paired indices 69, 70, 71 (same order as the scene catalogue)

The four-slice scene sweep (`work/run-slices-level-spawn-2.sh`, 60 ready ticks, the existing recipe and light JSON left in the playable-integration worktree) exited 0 on every slice:

- candidates 111, tried 111, loaded 111, played 111
- refused 0, failures 0, checks passed 119
- story ground 111, marker taken 98
- water tiles shown 9451, entries 4, ticks in water 252 of 3024 observed
- pieces 177, claimed 174, drawn claimed 52

The previous sweep on this branch was 108/111 loaded, 95 markers taken, 116 checks, 9308 water tiles. The three newly loaded levels account for the marker, check and water-tile differences.

Captures of those three indices (1280×720, not headless, 9 images under gitignored `work/private/captures-spawn2/`): loaded 3, played 3, story ground 3, marker taken 3, failures 0, ready tick 59, health full. Each play view has the figure on the arena surface. One stands in the water. The other two stand on rock. The overviews show the arena meshes. None is an empty void.

## Godot smokes

The wave-13 matrix, 43 commands: 40 passed on the first run. The three creature-anim commands exited immediately because `godot/creature_anim/bin` was not built yet. After that debug library was built they passed: 32, 25 and 231 checks. Final result 43/43.
