STATUS: FIXED — all three play-save-r3 audit findings are addressed; original dead-Creature load effects remain refused.

# Play-save round 3 — 2026-10-23

## Scope and evidence

Started from `codex/play-save` at `170fb63`. The field layouts and ten-slot
lookup/overflow behavior come from `t3/research-save-world` `73d2816`,
`docs/save-world.md`, and its round-2 audit. The later
`research-save-world-r2-fix.md` audit is PASS at `e7d8972`; it confirms that
the unprobed load effects remain Unknown. This is an authored native companion,
not a PS2 card image or a claim of original `Creature_loadState` parity.

## Change

- Added a versioned native directory keyed by qualified level name. It has ten
  readable slots; a duplicate rewrites its slot, while the first overflow key
  retains a write-only gap name and later misses alias that gap's body. Native
  encoding has its own 32 MiB bound and refuses a save or transition when a
  requested level has no readable slot. The original act-row reserved sizes,
  thinning, and failed-fit buffer behavior are not represented by this format.
- The real script factory currently constructs five Creature subclasses. Their
  records use `objects.ob` ordinal, class, original position and angle as
  identity. The companion captures the writer-verified variant, raw current
  and maximum HP words, mapped flag bits and optional sign-extended `s16`
  charm timer at `+0x312`. Restoration writes these fields before `start`
  and rejects a changed record identity, missing constructor, deleted or dead
  built object. It leaves the separate `s16` root timer at `+0x314` untouched.
  It does not run unverified animation, motion, class or other world-service
  effects.
- Real save/load hashes the source archive and stores the native directory in
  a checked `.worldstate` companion when the built Creature records are
  eligible. Older version-1 associations load with an empty object directory.
  The playthrough picks an actual constructed Creature level for its save step;
  warm/cold byte comparisons and revisit checks run only after a successful save.

Changed files: `native/include/champions/level_object_state.hpp`,
`native/src/level_object_state.cpp`, `native/include/champions/script_commands.hpp`,
`native/src/script_commands_objects.cpp`, `native/script_commands_targets.cmake`,
`native/tests/level_objects_tests.cpp`, `gdextension/src/session_controller.hpp`,
`gdextension/src/session_controller.cpp`, `godot/scenes/real_data_play.gd`, and
`godot/tests/loop_playthrough.gd`.

## Audit fixes

- **PSR3-1:** The optional charm flag now reads only `s16 +0x312` and its
  saved `s32` is the signed extension of that halfword. Restore writes only
  two bytes; a nonzero root timer at `+0x314` cannot select charm and is not
  overwritten. Research: `t3/research-save-world` `e7d8972`,
  `docs/save-world.md` and `docs/creatures.md`.
- **PSR3-2:** Saving or restoring a built Creature with HP at or below zero
  refuses the field-only companion. The original's conditional death-animation
  handle and load effects have no confirmed native reconstruction here.
- **PSR3-3:** Native assertions now compare the independently specified alive
  writer bytes, including negative charm sign extension, test a nonzero root
  timer with zero charm, verify adjacent-byte preservation, and exercise dead
  save and restore refusal. The playthrough distinguishes the exact dead-state
  refusal from an unrelated save failure, and the host save screen gives a
  specific reason. Warm/cold roundtrips remain conditional integration coverage.

Audit-fix files: `native/src/script_commands_objects.cpp`,
`native/tests/level_objects_tests.cpp`, `godot/scenes/real_data_play.gd`,
`godot/tests/loop_playthrough.gd`, and this report.

## Verification

- Arm64 Release configure/build: passed. Final relevant CTest ran serially:
  `native-save`, `native-snapshot`, `native-save-access`,
  `native-save-partition`, `native-save-indexed`, `level-objects`,
  `script-commands`, `script-effects`: **8/8 passed**. The final
  `level-objects` case includes exact signed charm bytes, root preservation,
  dead-state refusal, changed-field restore, duplicate, overflow and
  deleted-object refusal.
- Full arm64 CTest (`-j 3`) completed **230/231 passed**. The unrelated
  `player-exact` case timed out at 240.12 seconds under machine load; its
  original aggregate timeout is retained separately. The isolated `player-exact`
  rerun **passed 1/1 in 167.76 seconds**.
- Godot dev matrix: **54/54 process exits passed**. `FULL_TRACE` remained
  **36,402 bytes**, SHA-256
  `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`.
- Four-slice read-only sweep: **111/111 tried, loaded and played**;
  **230 checks passed, 0 failed**, 111 scripts attached, 76 started, 324
  constructed objects and 15,577 unbuilt constructor stubs. All four slice
  processes exited 0. Only aggregate counts were retained in this report.
- Final Godot editor import and GDExtension rebuild: passed. The read-only
  real-data playthrough selected a 1,512-element level with 13 built Creature
  records. Both headless and rendered runs reached F5 Save and reported the
  exact dead-state refusal (`refused_as_expected`). Both wrote 26 step results;
  rendered wrote 10 captures. The textured level and save refusal screen were
  inspected without a new visual error. The full journey exited 1 in both
  modes; no file was saved, so warm/cold object restoration and revisit could
  not be verified in this real-data run. The disc file count stayed **994
  before / 994 after**.

## Remaining limits

Chest and ItemProp have verified byte counts and selected restored fields in
the research, but the real client still builds neither constructor, so this
round does not fabricate either object. Creature death animation, motion,
callbacks and world services; Chest interaction flag, zero-gold branch,
animation arguments/state and class-definition argument; and ItemProp
state-bit branches, Prop load and transform arguments/state remain refused.
Other live objects, deletion/filter/thinning policies, decals and automap are
not a complete original world snapshot. The real script host restarts a fresh
machine and does not resume PC, stack, deferred work or dialog. The existing
player/quest companion still loads after `start`, so original card-load order,
original `Creature_loadState` effects and full gameplay parity remain unverified.
The real-data playthrough's selected level contains a built Creature with
nonpositive HP; this client refuses its save until the death-animation handle
and load effects have targeted evidence and an implementation.

## Commands

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^(level-objects|script-commands|script-effects|native-save|native-snapshot|native-save-access|native-save-partition|native-save-indexed)$' --output-on-failure -j 1
ctest --test-dir build/native --output-on-failure -j 3
ctest --test-dir build/native -R '^player-exact$' --output-on-failure -j 1
cmake --build build/godot-extension --config Release --target champions-archive-extension --parallel 2
<Godot 4.5.2> --headless --path godot --editor --quit-after 120 --frame-delay 50
python3 work/play-save-r3-run-godot-matrix.py
python3 work/play-save-r3-run_sweep.py
python3 tools/run_loop_playthrough.py --godot <Godot 4.5.2> --game-folder <read-only RTA disc> --out work/play-save-r3/playthrough-audit-final
```
