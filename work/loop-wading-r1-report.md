STATUS: COMPLETE — the player's wading step is ported and replayed against the research cases; arm64 CTest 228/228, Godot dev matrix 54/54, sweep 111/111 with scripts on and off, FULL_TRACE unchanged. Not run: export and x86_64/Rosetta.

# Player wading, round 1 — 2026-10-21

Branch `codex/wading` from wave-23 `b75c174` (the wave-23 report exists). Evidence: `t3/research-wading`
`942a6ba` (`docs/spec/player-wading.md`, `tools/ref_wading.py`, `tools/difftest_wading.py`; audit passed), which
cites `t3/research-combat-walk` `combat-movement.md`. No value comes from outside that pin; each constant in
`native/include/champions/player_wading.hpp` carries its instruction address. Nothing was pushed, nothing was
written in the research repository (only its research worktree was read), the disc was read only and only counts
are reported.

## What changed (commits on `codex/wading`)

1. `fe38a58` `champions/player_wading.hpp`: `Player_move` step (f) as a pure `step()` in the original call order
   (hurting-water damage, wading ripple 300/12 at `(x-0.0, y, z-12)` every frame while `s >= 0.01` else every
   16th frame, bit cleared when the ripple touches nothing; entry test every 4th frame with `water_test(12)`, one
   shared draw, splash 1.0 / 0.4, entry ripple 3000 / 1000 at radius 24 with its result ignored, airborne burst),
   plus `foley()` for `sound_playFoley` (water variant from the cosmetic seed, never the shared seed). The depth
   band is the two services' own query boxes, so it is not hand-coded. Contract test `player-wading-exact`.
2. `6d90a70` runtime wiring: `combat_runtime` `Services` gain `water_disturb` and optional `water_height`;
   `player_water` runs `step()`; the `Player_run` state 7 / 0xb ripples reach the service; the flat fixture is a
   dry world; a wader with no `water_disturb` service refuses. `real_world::Wader` runs the same step on a
   WORLD water image. Tests `player-wading-runtime` and an extended `real-world-adapter`.
3. `3bb0d02` binding, scene and docs: `ChampionsRealWorld.wade_step`, `real_world.gd` steps once per ready tick,
   scene counters `water.wading_*`, `docs/PLAYER_WADING.md`, stale "wading does not disturb the water" text fixed in
   `WATER_DRAW.md`, `REAL_DATA.md`, `COMBAT_RUNTIME.md`, `real_world.gd`.

The six `combat_runtime_*` / `potion_ctors` test fixtures that build their own `Services` now also stub
`water_disturb` to 0, the same dry answer their `water_test` stub already gave; corpora and expected values are unchanged.

## Contract tests (replay of the research reference cases)

`tools/generate_player_wading_vectors.py RESEARCH_DIR native/tests/vectors` runs `ref_wading.py` on authored
synthetic memory (no game data) and writes `player_wading.txt.gz` (5,191 cases; the manifest pins the research
commit and the SHA-256 of five research files). `player-wading-exact` replays them against the real
`creature_water::Service`: 12,695 checks, 0 failures.

- the band sweep of `difftest_wading` (2,652 cases: three feet heights, entry and wading, dz and edge distance in
  quarter steps) giving entry for dz in [-1, 60] / 13 from the edge and staying for [-13, 48] / 12;
- 2,400 call records: widths 384/120/768, five heights, five frames, two stick speeds, level flag, cut-skip byte;
  the in-water bit, call order and arguments, the shared seed, and every ripple grid cell;
- the seven 32-frame sequences of research section 9 (entries / exits / draws / grid frames match its table);
- 132 `sound_playFoley` choices, with cosmetic-seed advance and no shared draw.

Two mutations (wading strength 300 -> 304, and the entry gate `& 3` -> `& 7`) fail 1,156 and 464 checks, so the test
bites. `player-wading-runtime` (21 checks) ticks a Full player over a synthetic chunk: entry (one 0.4 splash, a
1000 / 749 / 499 ring), a still wader ripples only on 16th frames (+300 each), the deep band re-enters with no ring, a
wader above the band leaves, a missing service refuses, and 40 ticks in water leave position, facing, blend and
velocity words equal to the same 40 ticks on land (no speed effect).

## Verification

- Native Release arm64: `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`, build `-j 4`, no errors.
  `ctest -j 3`: **228/228** (226 of wave 23 plus the two new tests), 894 s. The new test executable is Mach-O arm64.
- Godot dev matrix (`work/wading/run-godot.py`, the wave-23 54-command list on arm64 debug extensions): **54/54 exit 0**.
  The creature-animation extension was built first (three commands). Full combat 2,620 checks, 0 failures;
  `scene_play_flow` 65 checks.
- **FULL_TRACE `4da5705531dd885150aa42a715fff04e88274b3ac65ea449d98c72070a4b78d9`, 31,622 bytes**, identical to wave 23:
  the Full fight is on dry fixture ground, so the wading step never fires there.
- Water smoke on real levels (`--wade`, 2,400 ticks, one level each; indices only): level 30 reached water. 48 ticks in
  water, one entry, 1 shared draw (counted), 1 splash, **48 ripples, all touching a chunk** (entry + 47 wading), 3
  hurting-water frames counted. Levels 20, 28 and 29: the harness walk did not reach the water, so 0 wading events.
  Not looked at by eye: the runs are headless, so ring visibility in a window is unchecked.
- 111-level sweep, four slices x 60 ready ticks, `work/run-slices.sh` (the wave-21 script): scripts on `w24on` and
  scripts off `w24off`: **111 tried, 111 loaded, 111 played, 230 checks passed, 0 failures, empty refusal list** each,
  as wave 23. Every deterministic count equals wave 23 except the ones below.

### Change in water counts (sweep totals, wave 23 -> now; identical for scripts on and off)

| Count | Before | After |
|---|---|---|
| `water.wading_steps` (ready ticks stepped, levels with a drawn water instance) | 0 | 2,832 |
| `water.wading_ticks_in_water` (the wader's bit set) | 0 | 224 |
| `water.wading_entries` / `water.wading_draws` / `water.wading_splashes` | 0 | 4 / 4 / 4 |
| `water.wading_disturbs` / `water.wading_touched` | 0 | 21 / 21 |
| `water.wading_exits`, `water.wading_hurts` | 0 | 0 |

The counts are new, not changed: the position-based counts are untouched (`water.entries` 4, `water.ticks_in_water` 252,
`water.ticks_observed` 3,024, 9,451 tiles). 224 ticks with the wader's bit against 252 position-based is the hysteresis:
the bit needs a 4th-frame entry test, and the wading band sits 12 lower than the entry band. 21 ripples in 224 ticks: the
sweep player is idle, so a wader ripples only on 16th frames (the research rule). `text.frames_drawn.bust` moved 27 -> 29
with scripts on; it follows display cadence, is not wading, and is unchanged with scripts off.

## Authored choices and what is not covered

- The Godot real-level scenes run the diagnostic control walk, not `Player_move`. `Wader` therefore takes frame = ready
  tick, feet = observed position, and `moving` = feet moved since the previous observation as the stand-in for
  `s >= 0.01`; it is never airborne. These are authored stand-ins, labelled in `real_world.hpp` and `PLAYER_WADING.md`.
- **Counted, not applied** there: the entry's shared-seed draw (the control session has no shared seed to advance) and
  the hurting-water `damage_apply` (3 of 90 level rows set the bit; what it means is Unknown in the research).
- The order of the player's ripple against the water tick of the same frame is not in the research. The scene disturbs
  first, then ticks.
- Not wired: the footstep sound (`foley()` is tested, but the format strings are not recorded and the cosmetic seed's
  start value is unknown), the splash sound and burst (presentation), script `cflag` / whole-word stores of `+0xa8`.
- `Composition::Full` itself still plays on the flat dry fixture; real WORLD water reaches it only through a
  host-supplied `water_test` / `water_disturb`.
- Not run: macOS export, x86_64 (Rosetta) CTest, universal extension builds (the dev extensions were arm64-only), the
  Windows/Linux builds.

## Commands

```sh
python3 tools/generate_player_wading_vectors.py <research-wading worktree> native/tests/vectors
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release && cmake --build build/native -j 4
ctest --test-dir build/native -j 3
work/wading/build-gd.sh                      # arm64 debug GDExtensions
python3 work/wading/run-godot.py             # 54-command dev matrix
work/run-slices.sh w24on --scripts ; work/run-slices.sh w24off
python3 -I work/wading/aggregate.py          # sweep totals against w23on / w23off
```

Logs and counts-only summaries are under ignored `work/wading/` and `work/private/`.
