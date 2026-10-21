STATUS: COMPLETE

Native job "arena" is local on `codex/arena` in `worktrees/arena`, branched from `codex/playable-integration` at `77d1260`. Not pushed. A Cursor Grok agent made the first two commits and stopped at its usage limit. Claude reviewed them against the research, fixed what did not trace, and finished the job.

## Evidence

Research `t3/research-arena` at `17810bb0f9a3474fbeae9e86d339cb72ccd0f154` (`docs/arena-modes.md`, `tools/ref_arena_modes.py`, `tools/test_ref_arena_modes.py`, `tools/difftest_arena_modes.py`, `tools/ref_core.py`, and now `docs/issues/claims-audit.md`). All six are pinned by SHA-256 in `native/tests/arena_modes_contract.json`. The generator checks every pin before it imports the reference.

The arena start table and the origin plant come from `level_flow` (research-levels section 18, already pinned there). The three cold arena indices 69, 70 and 71 come from the level-spawn-2 report.

## Review of the Grok commits

Every float pattern, address and integer in `arena_modes.hpp/.cpp` was traced to `ref_arena_modes.py` or `arena-modes.md`, and those traced. The following did not trace, and are fixed:

1. **Level file names embedded in code.** Both tables were hard-coded. The research prints them, but this tree does not copy level names: `level_flow` refuses `ArenaRotationTable` for the same reason. They are now caller data (`Match::names_a/names_b`). A missing or wrong-size table refuses. Tests use synthetic names.
2. **The end flag stored by `open_end_prompt`.** `arena_openEndPrompt` ranks and stores fade 0. The two mode-B wipe callers store the end flag, as executed. The mode-A kill-limit path documents countdown 1 and the prompt, with no end-flag store, so none is made now.
3. **Tie ranking by an invented insertion sort.** The research did not assert the qsort tie order, so equal kills now leave `rank_known` false.
4. **`arena_init` missed `arena_resetScores`.** claims-audit row 99 shows the call at 0x304dd0. Without it, mode B started at 0 lives on the real disc. That run is how this was found.
5. **Seed lost across `findFreeSpot`.** That search draws from `g_randSeed` 0x404464, the arena's own generator. `pick_spawn` returned the seed from before the search, so a respawn threw away those draws. `Search` now carries the seed by reference.
6. **Invented gates and defaults:**
   - The purchase tail had an extra "not after shutdown, mode B still set" gate. It is now gated on mode A only (branch 0x307170).
   - `seed = 1` and `budget = 100` were defaults; `init` stores 100.
   - `visit_creature` returned "no draw" for an object with neither flag.
   - Confirm and cancel on the same frame ran confirm.
   - A tick with no active slot treated everyone as out.

   Each unexecuted case now refuses.
7. **The measure tool's hack.** It used `visible = count==0 ? 1 : count` instead of the documented origin plant. Rewritten (below).

What was not executed but is kept as a structural reading is listed under "Task order that is read, not executed" in `docs/ARENA_MODES.md`:
- the tick-overlay position;
- the intro returning while positive;
- mode A skipping the mode-B scan;
- the intro cue only while mode A is clear;
- the mode-A intro arm.

## Refusals kept or added

These stay explicit refusals:
- the session word 0x4f7124;
- net message +0xf4;
- the empty-wave callback 0x3069e8 (gate recorded as `empty_wave_refused`);
- draw helper 0x307bbc;
- whether every lockstep machine runs the draw task;
- the monster builder 0x3065fc;
- the `arena_nextWave` body;
- SpellArena particles;
- the `player_respawn` and `arena_shutdown` bodies;
- saved player-record bytes;
- retail recipe bytes.

Added:
- the name tables;
- `game_scanCm1Vars(0)`, needed for a mode-A wrap or the mode-A level name;
- the tie rank;
- a visit to an object with neither flag;
- a tick with no active slot;
- confirm and cancel together.

The dead-countdown decrement is undocumented. The module never decrements it.

## Files

- `eb6e0ed`: names as caller data; the end flag, tie rank, visit, no-slot and scan refusals; purchase gate; docs.
- `25d6bd8`: reset in `init`, the claims-audit pin, seed threading, `begin()` returns its wave, and the rewritten `champions-arena-measure`.
- `123b342`: `arena-modes-damage`, the versus rules through the real `combat::damage_apply`.
- `6ffe4b5`: refuse confirm and cancel together; `docs/ARENA_MODES.md` rewritten (task-order readings, stand-ins, real-disc runs).

Earlier: Grok's `b84e6b7` (port, vectors, generator) and `2a43994` (measure, since replaced).

Vectors: 270 generated checks in `arena_modes_vectors.inc`, plus 159 hand checks and 9 damage checks.

## Commands

Arm64 Release:

```
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -j 3 --output-on-failure
```

- Build: exit 0, no warnings in the arena targets.
- CTest: **202/202 passed**, 835 s, no timeouts. This includes `arena-modes-exact` and `arena-modes-damage`.

Godot:
- The archive, level-texture and creature-animation extensions were rebuilt for this tree: Release, `template_debug`, arm64 only, not universal. The dylibs the worktree started with did not match playable-integration's.
- The wave-16 dev matrix (`run-godot.py`, 45 commands, output in gitignored `work/private/godot/`) passed 44/45 on the first run.
- The failure was `combined-full-combat`: 2,616 checks with 1 failure, "stale clip clears cosmetic owner and restores static mesh". This is the same intermittent check as wave 16, and `FULL_TRACE` `8dc349c1…aa0c` still matched.
- Rerun alone it passed: 2,616 checks, 0 failures. **Final 45/45.**
- The extensions do not link the arena module.

## Private disc (counts only)

`champions-arena-measure --exe <disc>/SLUS_209.73 --archives <disc>/BG/DATA --captures work/private/arena-captures` exited 0 in 7 s.

Executable tables: recipes 5, names_a 13, names_b 7, matching the research. Archives: paired 111, table levels 13, failures 0.

- **Mode B, all 13 levels:**
  - one wave-0 recipe of 2 classes, 1 ring step, 2 builds refused;
  - intro 180, latched;
  - placed 1/1 on a floor;
  - 1 purchase (lives 2 → 3), 108 overlay frames, 510 glyphs;
  - 4 deaths, 3 respawns, all on a floor, then 1 refused at −1;
  - 8 empty-wave gates refused;
  - end prompt at frame 1200 (rank known), fade 73 frames, confirm and shutdown;
  - next level on disc 1.
- **Mode A, 12 of 13 levels:**
  - placed 2/2 on a floor;
  - 5 kills reach the limit (5), end prompt at frame 900, rank known;
  - 4 respawns on a floor, 1 refused by the intermission gate;
  - intermission 481 frames, level change, next on disc 1.
- **Mode A on the 13th level** (table index 12) wraps to 0 and refuses: `game_scanCm1Vars(0)` was not supplied. This is the one refusal line.
- One mode-A level used the spread-240 retry once. No camera fallback occurred.

**Captures.** For indices 69, 70 and 71: a top-down picture for each mode, an overlay picture for mode B, and a frame trace, in gitignored `work/private/arena-captures/`. That is 9 PNG and 6 trace files.

I looked at them:
- Each top-down picture shows the arena floor and walls from the real terrain. The white spawn-table points match the alternate counts (7, 16 and 26, plus the start).
- Mode A puts the two slots on opposite sides of the radius-180 ring, facing the centre.
- Mode B puts slot 0 on the radius-36 ring. Its respawns land on a table point inside the 240 circle.
- The overlay picture shows the five glyphs following the hermite curve from the stand-in anchor to the stand-in screen point.

Stand-ins:
- seed 0;
- record HP and mana;
- frame counter;
- camera focus at slot 0's wave spot;
- the scripted gold and deaths;
- zeroing the dead countdown at each death;
- one fade step per frame;
- overlay anchor and screen point.

They are listed in `docs/ARENA_MODES.md` and labelled in the tool.

## Unverified or unsupported

- No console or live-game comparison.
- No Godot scene binding: no match starts from the scene.
- No monsters spawn, because the builder is refused, so mode B never reaches a second wave. `arena_nextWave` is refused, and the empty-wave callback is unknown.
- x86_64 was not run.
- `level_flow` still lists `arena_pickSpawnPoint` a0 = 0 as `ArenaRespawn`. `arena_modes` now implements it from the arena research. `level_flow` is outside this job's scope and was not edited.
