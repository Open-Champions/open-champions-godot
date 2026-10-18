# Playable scene integration, wave 1 — 2026-10-17

STATUS: MORE

Coordinator explicitly split the original five-component brief into two waves.
Wave 1 includes the four scene components plus three completed native jobs.
`session-play-runtime` is not merged; the 512-ready-tick diagnostic cap is an
expected limitation. No long-session or retail-playability claim is made.

Worktree: `~/Projects/open-champions/worktrees/playable-integration`, branch
`codex/playable-integration`. Clean starting HEAD:
`60def02263f2bcb9685cb06c5a0c7409ac5ccab4`. Each source worktree was inspected
read-only and clean at its exact authorized tip; its final `work/` report was read.
No canonical/source worktree, research client, PR73, push, PR or issue changed.

## Serial local merges

| Branch | Source tip | Local merge |
|---|---|---|
| scene-level-library | 95a6dda | 2341a79 |
| scene-animation-preview | 2606de2 | 4fd8c43 |
| scene-original-art | 1af101e | 719cf45 |
| scene-play-flow | 2f0a072 | 47dce83 |
| skills-spell-kernel | 3ef9b9a | be912f4 |
| skills-a-ctor | c094b96 | 20de046 |
| creature-water-2 | 23699af | 8e199a6 |

Commands: `git merge --no-ff --no-edit codex/<branch>`, in table order.
All seven merges were clean. Git automatically combined disjoint Skills-A
constructor and spell-kernel edits. Existing strict numerical flags, target
platform gates and historical vector/manifest bytes were preserved.
Full hashes: ignored `work/playable-wave/merge-audit.json`.
Historical corpus byte audit: `work/playable-wave/historical-corpus.json`.

## Integration-only changes

- Add the water source/header to the explicit simulation-boundary census,
  increasing its actual union from 71 to 73. Add rejection coverage for host
  arithmetic/presentation dependencies in those files. Wave 2 must retain this
  additive registration when it merges its own census additions.
- Make the shell's existing absent-component smoke case explicitly return null
  through its existing factory seam. This preserves the missing-component
  assertions after the real components are installed. Its other factory cases
  still cover adapter lifecycle; the new combined suite uses production paths
  with **no injected components**.
- Add `godot/tests/playable_wave_smoke.gd` for the merged real-extension flow.
  Synthetic WORLD/VIF/ANM/TEX fixtures are generated only in its isolated test
  directory. It drives UI signals and shell input; screenshots are from the
  actual exported graphical viewport, not a browser or a mocked renderer.

## Evidence sources and boundaries

The four scene components retain research pin
`a2b145b6b22f3c0451f861ed70faa1827f94bf09`; the spell kernel retains
`356add27d50e50972b2a671b31e8f4ec93e6778c`; constructor/water supplements use
`2502e2bfac58203ac0174fccd7e0953b36d06439`. Exact contract hashes and supported
refusal domains remain in `SCENE_LEVEL_LIBRARY.md`, `SCENE_ANIMATION_PREVIEW.md`,
`SCENE_ORIGINAL_ART.md`, `SCENE_PLAY_FLOW.md`, `SKILLS_SPELL_KERNEL.md`,
`SKILLS_A.md` and `CREATURE_WATER.md`. Integration changes no original algorithms.

Reference-function coverage unchanged; client integration only. Included source
branches establish their own standalone function counts; these are not gameplay
percentages. Water remains an opt-in supplied-grid service, not Session water
support. Cosmetic pairing and diagnostic art placement remain explicitly authored.
Private validation is unavailable; no alternative locations were searched.
Windows, physical Intel execution, Norrath, original HUD fidelity, retail asset
pairing, combat/progression and complete retail levels remain unverified.

Additional integration fixes found by actual export inspection:

- Animation picker Play was disabled whenever the shell paused for that picker.
  Play/Restart now retain the user's explicit playback intent while suspended;
  `observe()` still refuses to advance cosmetic time until native session Resume.
  The picker reports `waiting for session resume`. The combined test presses the
  visible enabled button before hiding the picker, then verifies native-tick pose
  advancement. It never invokes a hidden Play control to bypass this interaction.
- Give the animation panel an opaque background and small content margins so
  underlying shell text cannot overlap its labels.
- Give the art picker's wrapping status label the row list's existing 680-pixel
  minimum width. Before first layout, zero-width wrapping expanded the dialog to
  over 2,000 pixels, placing Close offscreen. The combined graphical test asserts
  the dialog fits its window; the re-exported Close control is visible.

These are presentation/assembly fixes only. No native numerical body, retention
algorithm, original-art pixels, save schema or gameplay behavior was changed.

## Save and test interpretation

The initial combined test expected all four submissions to be ready ticks; the
existing startup handshake instead yields three. It also compared presentation
metadata as if it were saved authority. These test assumptions were corrected
against the unchanged sandbox/save tests and bridge. The exact comparison now
excludes only `terrain_name`, `terrain_note`, `terrain_mode` and
`save_history_pending`; all authoritative observations, raw32 state and counters
must match. Both replay and indexed snapshot continuation are tested.

Existing legacy saves reconstruct collision ground, not the selected WORLD
render mesh/source association. Their loaded display is explicitly labelled
`Restored collision ground`; the original WORLD can be reselected through the
library. Indexed snapshots can retain deferred historical validation; a later
save attempt can materialize it even when filesystem writing fails. That flag
is diagnostic metadata, not a changed live game state. These limitations are
recorded, not hidden by rewriting the still-owned runtime/save implementation.

The combined journey covers actual entry, enabled production components, two
WORLD geometries and return, native movement, visible picker playback, exact
sampled corners, two native-shaped synthetic UI images at 1x/2x, pause/input
release, replay/snapshot saves, changed state, identical continuation, failed
WORLD/art/clip/save ownership, the expected cap, reset, selection/re-entry,
focus loss and teardown. Existing shell tests also cover archive exit/return and
keyboard navigation. No physical mouse or native OS file picker was operated:
scripted UI signals and shell input drove the export.

## Commands and local evidence

From this worktree (all exit 0 unless the final CTest section says otherwise):

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
python3 tools/check_simulation_boundary.py .
python3 tools/test_core_simulation_boundary.py
python3 -m unittest discover -s tools -p 'test_*.py'
ctest --test-dir build/native --output-on-failure -j 3
```

Native build passed. Boundary checker: 73 unique existing paths; 38 tests pass.
Python tooling: 712 tests, OK, 385 skipped. Packaging unit-test mocks do not count
as platform execution. Native full-run result is recorded in the closing section.

Both extension variants were built here, without borrowing a sibling binary:

```sh
for target in template_debug template_release; do
  cmake -S gdextension -B "build/mac-$target" \
    -DGODOT_CPP_DIR="$PWD/../integration/work/deps/godot-cpp" \
    -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET="$target" \
    '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64'
  cmake --build "build/mac-$target" --config Release --parallel 4
done
```

Godot is `../integration/work/runtime/Godot.app/Contents/MacOS/Godot`, verified
`4.5.2.stable.official.6ce3de25a`; read-only godot-cpp pin is
`e83fd0904c13356ed1d4c3d09f8bb9132bdc6b77`.

The full headless batch passed: bounded editor import, archive, frame-rate,
facing, recovery, sandbox, texture-upscale, UI gallery, control-animation-cache,
level-library, animation-preview, original-art, scene-play-flow, combined wave
and default launch. Exact argument arrays, elapsed times and statuses:
`work/playable-wave/godot-results.json`. Logs: `godot-*.log` in that directory.
Final changed scene scripts were rechecked in the actual release export.

```sh
../integration/work/runtime/Godot.app/Contents/MacOS/Godot \
  --headless --path godot --editor --quit-after 120 --frame-delay 50
../integration/work/runtime/Godot.app/Contents/MacOS/Godot \
  --headless --path godot --script res://tests/playable_wave_smoke.gd
../integration/work/runtime/Godot.app/Contents/MacOS/Godot \
  --headless --path godot --export-release 'macOS universal'
ditto -xk build/exports/macos-universal/OpenChampions.zip build/exports/wave1-final
```

Export logs and final execution argument arrays are `export-final.log` and
`export-results.json`; hashes/architecture slices are `export-artifacts.json`.
The embedded release extension equals the locally built release extension byte
for byte. Both executable and library have arm64/x86_64 slices; only arm64 was
executed, using macOS OpenGL 4.1 Metal Compatibility on Apple M5 Pro.

Reproduce the graphical combined run (use 960x720 and a different capture folder
for 4:3; start the executable without test arguments for normal entry):

```sh
'build/exports/wave1-final/Open Champions.app/Contents/MacOS/Open Champions' \
  --resolution 1280x720 --script res://tests/playable_wave_smoke.gd -- \
  --capture-dir="$PWD/work/playable-wave/captures-16x9"
```

Actual export headless routes: animation 42 checks, art 63, shell 62, combined
68, sandbox 124, browser 283, plus normal default launch: all pass. The browser
route requires both `--browser-smoke-test` and `--smoke-test`; an initial manual
invocation omitted the latter and was terminated, then corrected successfully.
The final recorded batch uses the correct route and exits normally.

Final graphical combined runs: 102 checks at **1280x720** and 102 at **960x720**,
zero failures. Captures: `work/playable-wave/captures-{16x9,4x3}/`, 16 PNGs each:
entry, library, level, animation picker/zero/four, art picker/1x/2x, pause, loaded
save, WORLD/art/clip/save refusals and expected cap. Inspected actual mesh changes,
shaped synthetic pixels, legible picker/error text, reachable Close and menu
controls, and narrow/wide scrolling. These are synthetic diagnostic visuals.

A final narrow exported stopped-state capture exposed a stale OptionButton
minimum-width cache after save formats were rebuilt while its menu was hidden.
The shell now reserves width from the actual advertised labels plus control
padding. The graphical regression verifies that the selected format is legible;
no supported method, format selection or serialization behavior changed.

Source integration commit: `cb1052bd5bc09a3160e963a330d73825c5b04743`. Final runtime/scripts in the export
match that source; final handoff documentation is committed separately.
Final ZIP SHA-256: `7bb025e1b79920aae1f7fd916b6eb24ba174964376e78d84927ebda5a4e919a9`.
Local/embedded release dylib SHA-256: `86fd38306973527599bf4cf6768aa0d184bd5f6ebf5a13f6246ce165a35e830d`.
Source subtree IDs and all 48 capture hashes: `work/playable-wave/source-capture-provenance.json`.

The brief's additional **800x600** layout also passed the same 102-check actual
export flow. Sixteen further PNGs are in `work/playable-wave/captures-800x600/`;
log: `exported-combined-800x600.log`. Entry, art Close, save failure and capped
session controls remain readable/reachable with scrolling. No code change was
needed for this additional size. Total final captures: 48 across three sizes.

## Full native sweep

`ctest --test-dir build/native --output-on-failure -j 3`: **151/152 passed**,
1167.79 seconds, exit 8. The only failing result was `player-exact`, which hit
its unchanged 240-second limit under host contention (240.04 seconds). Every
other native test passed, including all merged Skills-A/B/C and shared-kernel
checks, the constructor supplement, water and boundary tests.

Full log: `work/playable-wave/ctest.log`; merged skills breakdown:
`work/playable-wave/merged-skills-results.json`. The prescribed isolated retry
is `ctest --test-dir build/native --output-on-failure -j 1 -R '^player-exact$'`;
it **passed 1/1 in 196.75 seconds**, exit 0. No timeout, corpus or numerical
flag changed. All 152 named CTests have passing evidence across the original
full run and this isolated rerun; the original full command remains 151/152,
not a fully green sweep. Retry log: `work/playable-wave/ctest-alone-player-exact.log`.
