STATUS: PARTIAL — real multi-element terrain reload and save/load are verified; per-level object continuation remains unsupported.

# Play-save round 2 — 2026-10-23

## Scope and evidence

This work stays on `codex/play-save` from `98a3516`. The implementation uses
`t3/research-save-world` at `b2caffb`, its `docs/save-world.md`, and the
read-only re-audit `research-save-world-fix.md` (PASS, one LOW wording finding).
The LOW finding matters: the original sets saved spawn rank to 3 before
`preStart` and clears the loaded-from-save byte; it does **not** clear rank.
The `hub*` name uses directory row 0 even without an act-list name match.
This round does not port that directory or the class payloads.

## Change

Commits `6a1366b` and `9e796c0` change `gdextension/src/session_controller.cpp`,
`godot/scenes/real_data_play.gd`, `godot/tests/loop_playthrough.gd`, and
`godot/tests/session_sandbox_cases.gd`.

- The bounded collision-plane preview now accepts multiple plain, unrotated,
  constant-height elements when each displayed plane agrees with the native
  terrain query and every covered grid cell lists that element. Rotated and
  special-sample elements, oversized terrain and inconsistent coverage remain
  refused; moving elements are omitted from this preview.
  A two-element authored fixture exercises preview, save and continuation
  preparation; a second fixture verifies refusal when one grid cell omits the
  displayed element.
- The real-data path reloads the level's WORLD/TEX from the selected folder
  before native state restore, then compares decoded collision data with the
  save. The playthrough now requires more than one actual terrain element.
  It checks the level, HP, gold, quest summary, position and terrain-element
  count after warm and cold loads. That terrain count is a documented WORLD
  topology item, freshly reconstructed from level data; it is **not** a saved
  active-object record.
- The real-level UI labels world-object and in-flight script continuation as
  refused. Existing player, quest and position continuation is retained.

## Verification

- Arm64 Release native configure/build: passed.
- Focused CTest: `native-save`, `native-snapshot`, `native-save-access`,
  `native-save-partition`, `native-save-indexed`: **5/5 passed**.
- Full arm64 CTest: **231/231 passed** at `-j 3` (1,482.75 seconds).
  Relevant cases passed: all five native-save cases above, all three each of
  `terrain-file`, `world-queries`, and `world-geometry` (host/exact/transport),
  plus `real-world-adapter`.
- Godot dev matrix: **54/54 passed**. `FULL_TRACE` stayed **36,402 bytes**,
  SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`.
- Four-slice read-only sweep: **111/111 tried, loaded and played**;
  **230 checks passed, 0 failed**, 111 scripts attached, 76 started,
  0 faults, 0 callee refusals and 0 refusal categories. The aggregate
  contains 15,901 registered objects and 15,577 unbuilt constructor stubs;
  these are counts, not saved object-state evidence.
- `session_sandbox_smoke.gd`: **128 checks, 0 failures**, including the new
  two-element preview, native-continuation preparation and coverage refusal.
- Read-only real-data playthrough: headless and rendered both selected a
  **221-element** real level. Both saved through F5 and restored level, HP,
  gold, quest summary, position and the 221-element WORLD terrain after warm
  and cold loads. The rendered run produced 10 captures; Saved and loaded
  terrain were inspected. Both full harnesses exited 1 on the existing enemy
  combat and level-change steps outside this branch. Each logged one explicit
  world-object continuation refusal. The disc count remained **994 before /
  994 after**.

## Remaining refusal

This is not every-real-level object-state save/load. The real script host has
only a small constructor subset and no authoritative registry of eligible,
deleted, transient, filtered or thinned objects. The real scene has no native
per-level object/decal/automap directory, no verified class callbacks for
those live instances, and no integrated restore order for them. Chest and
ItemProp payloads remain Unknown in the research. Saving a player on a real
level succeeds, but active objects are excluded and their continuation is
explicitly labelled refused. Script PC, stack, deferred work and dialog are
not serialized in the original level body; the client starts a fresh script
machine. This round does not claim its entry effects match a full original
card load. The research re-audit's rank wording must also be corrected before
using that sentence as a port instruction.

## Commands

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^(terrain-file|world-queries|world-geometry|native-save|native-snapshot|native-save-access|native-save-partition|native-save-indexed)$' --output-on-failure -j 2
ctest --test-dir build/native --output-on-failure -j 3
cmake --build build/godot-extension --config Release --target champions-archive-extension --parallel 2
<Godot 4.5.2> --headless --path godot --editor --quit-after 120 --frame-delay 50
<Godot 4.5.2> --headless --path godot --script res://tests/session_sandbox_smoke.gd
python3 tools/run_loop_playthrough.py --godot <Godot 4.5.2> --game-folder <read-only RTA disc> --out work/play-save-r2/playthrough
python3 work/play-save-r2/run-godot-matrix.py
python3 work/play-save-r2/run_sweep.py
```
