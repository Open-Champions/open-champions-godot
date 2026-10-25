# Arena modes

RTA US v2.00 versus (mode A, `g_pvpModeA` 0x4f711c) and wave arena (mode B,
`g_pvpModeB` 0x4f7120), from research `t3/research-arena` commit
`17810bb0f9a3474fbeae9e86d339cb72ccd0f154`. The prose is that checkout's
`docs/arena-modes.md`. The executable model is `tools/ref_arena_modes.py`.
`tools/difftest_arena_modes.py` reported 302 passing checks against the
original functions. Nothing in that round was observed on a console.
`docs/issues/claims-audit.md` row 99 (same commit) supplies the second caller
of `arena_resetScores`. All six files are pinned by SHA-256 in
[`native/tests/arena_modes_contract.json`](../native/tests/arena_modes_contract.json).

`native/src/arena_modes.cpp` is the port. Floats are raw patterns from
`champions-core`. Recipe bytes stay caller arguments: the research reads them
from the executable and does not publish them, so this tree does not embed
them. The 13-name table (0x5037b8) and the 7-name table (0x503798) are caller
arguments too (`Match::names_a`, `Match::names_b`). The research prose prints
them, but this tree does not copy level file names, the same rule as
`level_flow`'s `ArenaRotationTable`. A table of the wrong size refuses. Tests
use synthetic names.

`arena_executable.{hpp,cpp}` reads both tables (and, for the measure tool, the
recipes) from the owner's executable at run time: ELF32 little-endian,
file-backed `PT_LOAD` bytes only, a null pointer within 32 entries, and exactly
13 and 7 names (`read_names`). `mode_a_index` matches a level name ASCII
case-insensitively. Nothing it reads is stored in the tree.

## Callers (wave 17)

- `level_flow::arena_respawn` forwards the registered spawn table to
  `pick_spawn` with flag 0 (the respawn pick that research-levels left open).
  `level-flow-exact` replays the three generated flag-0 cases through it.
- The Godot session reads the mode-A table from the game folder's executable
  (`ChampionsSessionController.load_arena_tables`) and starts a level in mode
  A (`level_flow::arena_mode_a_start_ground`) only when its story snap has no
  ground and the table lists it. On the private disc that is the three cold
  arena indices, and the real-data sweep is 111 / 111 again.
- No match runs in the scene. `Match` (waves, scores, respawn timing) is not
  bound to Godot.

## What runs

- Shared generator `seed * 0x19660d + 0x3c6ef35f`, draw `seed >> 16`, the
  word `g_randSeed` 0x404464.
- Score block: reset, kill credit, suicide, life loss, the mode B life grant,
  and the gold purchase (`150 / 100` becomes `50 / 200`, HUD word `0x80`).
- `Match::init` is `arena_init`: it returns while mode B is clear; otherwise
  it calls `arena_resetScores` (0x304dd0, claims-audit row 99), stores wave
  −1 and the purchase budget 100. `Match::begin` stores waves-left 3 and
  returns `arena_startWave(1)`.
- Overlay timer, glyph parameters, hermite, and the prompt fade step. The
  timer clamp that copies lives is the store in the reference. The difftest
  did not take that branch; a step that stays at or below 2.0 was executed.
- `arena_pickSpawnPoint` for both `a0 != 0` and `a0 == 0`, including the
  offline 240-unit candidate test and the camera-target fallback.
  `world_findFreeSpot` draws from `g_randSeed`, the same word as the arena's
  own draws, so `Search` receives the seed by reference and `pick_spawn`
  returns it after the search's draws.
- Wave planning, ring step, monster level, intro lock, intermission limits
  `480` and `960`, kill limit, wipe, and the live-match player scan.
- Mode A map rotation and mode B record selection. Confirm uses
  `arena_levelName`. Cancel clears mode B and stores session exit 1. The mode-A
  wrap to 0 and the mode-A `arena_levelName` need `game_scanCm1Vars(0)`
  (`Match::scan0`). Its live value is not in this research, so an empty
  `scan0` refuses there.
- `arena_openEndPrompt` ranks and stores fade 0. It does not store the end
  flag. The two mode-B wipe paths store the end flag before the call, as
  executed. The mode-A kill limit stores countdown 1 and calls the prompt; no
  end-flag store is documented there, so none is made. With distinct kill
  counts the rank is the one any sort gives. Equal counts leave the rank empty
  (`rank_known` false): the research did not assert the qsort tie order.
- The purchase tail runs while mode A is clear (branch 0x307170). No other
  gate is applied.
- Damage: `combat::damage_apply` already applies the mode-A armour cut to
  every victim and the later player-only half when `World::pvp_a` is set.
  `arena_modes::armour_bound` and `pvp_half` are the same formulas.
  `champions-arena-modes-damage-tests` runs the research's executed pair
  through the real pipeline: a player (armour 0, flat 40) takes half with mode
  A set; a monster (armour 300, flat 100) takes exactly 100 with mode A clear
  and `armour_bound(100, 300)` with it set, which is not half; no stun event in
  mode A.

The run task exists only while mode B is set. Mode A alone never installs it.
Damage still reads mode A. The task does not advance the fade; a caller
supplies confirm and cancel as booleans. Button bit `0x100` in the difftest
is a fixture, not a documented mask. Confirm and cancel on the same ready
frame were not executed and refuse.

### Task order that is read, not executed

The research executes each task branch on its own. Where they meet, this
port uses the order below. No vector distinguishes another order, so these
are structural readings, not executed facts:

- Paused, then the intro counter, then the rest. A positive intro decrements
  and returns, like the documented return at 0.
- `arena_tickOverlay` once per tick after the intro, before the match logic.
  The task difftest follows that call; its position is not asserted. The
  purchase's timer 0 survives its own frame, which this order keeps.
- With the intermission running, the wipe check and the increment. Otherwise
  mode A checks the kill limit, and mode B (end flag clear) runs the
  empty-wave gate and the player scan. Mode A does not run the mode-B scan.
- The intro cue plays only while mode A is clear (executed with mode A clear).
- In mode A, `arena_startWave` resets the scores, then arms the intro and the
  match word as in mode B. The mode-A difftest did not read the intro.

## Refusals

| Item | Why |
|---|---|
| Session word `0x4f7124` | Unknown. Facing treats it as an input. |
| Net message `+0xf4` | Unknown. |
| Empty-wave callback `0x3069e8` | The 128-frame gate was read. The callback was not run. A true gate is recorded (`empty_wave_refused`) and does not invent a living count. |
| Draw helper `0x307bbc` | Not run. |
| Draw task on every lockstep machine | Unknown. Fade step is the executed store. |
| Monster builder `0x3065fc` | Only the call (class, argument 0) was executed. |
| `arena_nextWave` body | Not run. The task records argument 0 or 1 and does not advance the wave. |
| SpellArena particles | Positions were stubbed. Flag bit and tint gate are implemented. |
| `player_respawn` body | Stubbed. The caller records spread 2 and radius 3, then refills the fixture's max HP and mana. |
| `arena_shutdown` body | Stubbed. Confirm records the call. |
| Saved player-record bytes | Not in this research. Confirm counts one restore request per active slot. |
| Retail recipe bytes | Not published. A missing record is a refusal, not a guessed table. |
| Level-name tables | Not copied. A missing or wrong-size table refuses. |
| `game_scanCm1Vars(0)` | Live value unknown. A mode-A wrap or mode-A level name without it refuses. |
| Rank order for equal kills | qsort tie order not asserted. `rank_known` stays false. |
| `arena_visitCreature` on an object with neither flag | Not executed. Refuses. |
| A task tick with no active slot | Not executed. The wipe and live scans refuse. |
| Confirm and cancel together | Not executed. Refuses. |
| Dead-countdown decrement | No writer other than the respawn's 240 is documented. The module never decrements it. |

`0x4f7124` is not derived here. Direct stores found by the research are zeroes.
The session writers and the net field stay unknown.

## Real-disc runs

`champions-arena-measure --exe FILE --archives DIR [--captures DIR]` reads
the owner's executable and archive directory and writes neither. It maps the
ELF's file-backed `PT_LOAD` segments, reads the recipe count (0x503886), the
records (0x503868) and both name tables, and keeps them in memory. For each
paired level whose name is in either table, it plants the origin at entry 0
with count 1 (`LEVEL_FLOW.md`, `arena_init`), registers each
`ArenaPlayerStart` at 0 and each `ArenaPlayerAlternate` at −1, then runs mode B
(one player) and mode A (two players). Standard output is counts only; refusal
text is cut before any `:`. With `--captures`, the three cold arena indices
(69, 70, 71, the level-spawn-2 set) also get a top-down picture, an overlay
picture and a frame trace, written only to the named directory. Use a
gitignored directory such as `work/private/`.

Scenario inputs that no research supplies are stand-ins, named as such in the
tool and its traces:

- seed 0 (one of the two seeds research-levels section 18 executed);
- max HP 100 and max mana 50;
- `g_simFrame` is the loop counter;
- the camera focus and camera target are slot 0's wave position;
- mode B: 150 gold and one buyer at frame 300, then deaths at 600, 800, 1000
  and 1200 (null killer); mode A: slot 1 kills slot 0 at 300, 450, 600, 750
  and 900, with the menu-index-0 kill limit 5;
- each death zeroes the dead countdown, because its decrement is not
  documented;
- after the end flag, one `arena_drawPrompt` fade step per frame (the
  draw-task route goes through the unexecuted helper 0x307bbc), then confirm;
- overlay anchor (64, 400) and projected point (320, 224);
- `findFreeSpot` runs on the real terrain with an empty object list.

The wave placement is followed by `player_ctor_z` at the placement radius,
as `level_flow` does for the arena start.

## Reproduce

```
python3 tools/generate_arena_modes_vectors.py \
  <research-checkouts>/research-arena \
  native/tests/arena_modes_vectors.inc
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^arena-modes' --output-on-failure
./build/native/champions-arena-measure --exe DISC/SLUS_209.73 \
  --archives DISC/BG/DATA --captures work/private/arena-captures
```

The generator checks the pinned SHA-256 of every pinned research file before
importing the reference.
