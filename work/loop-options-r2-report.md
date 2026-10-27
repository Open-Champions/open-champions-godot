STATUS: PASS — NOPT-1 through NOPT-6 fixed; aggregate CTest 224/226 under load, both timeout cases passed isolated.

# Native options audit fixups

Branch `codex/play-options`, parent `67fcb1c`. Audit `native-options-r1.md`: NOPT-1 through NOPT-6. Contract: `t3/research-options-menu` at `603b2b7`, `t3/research-combat-ctors` music-volume contract at `e9777eb`, and `t3/research-saves` config offsets at `8d43286`. No game bytes or decoded assets are tracked.

## Changes

- NOPT-1: the native effect-voice mixer averages asymmetric left/right output in mono and restores separate sides in stereo. Synthetic voice tests exercise the switch directly and through the Options request.
- NOPT-2: removed the invented slider bars and linear fill. Both lists still draw the documented disc-backed frames and medium-font rows; unknown value artwork is refused.
- NOPT-3: float change detection uses the existing PS2 comparison, toggles retain integer comparison, and a zero-count commit leaves the image untouched. A signed-zero vector covers brightness, music and effects.
- NOPT-4: toggle accept reads the +0x10 repeat word, with held frames 1, 25 and 29 checked against fixed expected values.
- NOPT-5: front-end exit always enters state 14. With unchanged values, accept and cancel refuse the unresolved choice and do not write; changed accept/cancel retain their documented effects.
- NOPT-6: all thirteen keys, kinds and saved offsets are asserted against literal pinned goldens independent of `OPTION_ROWS`.

## Verification

- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`; `cmake --build build/native -j 4`: passed on arm64 after the final source edits.
- `ctest --test-dir build/native -R '^(frontend-exact|native-audio|music-volume-host|music-volume-exact|ui-layout-exact|ps2-save)$' --output-on-failure -j 1`: 6/6 passed; `frontend-exact` passed again after the integrated mixer test.
- `ctest --test-dir build/native --output-on-failure -j 3`: 224/226 passed in 3,275.20 seconds under heavy shared load. `player-exact` reached its declared 240-second CTest limit; `session-facing-exact` failed because its subprocess reached its 600-second limit. Isolated `session-facing-exact` passed 1/1 in 511.70 seconds. Isolated `player-exact` again reached 240 seconds, then passed 1/1 in 317.95 seconds with a temporary 900-second timeout in the generated CTest file. Its original 240-second timeout was restored. No content assertion failed.
- `cmake --build build/godot-extension --target champions-archive-extension -j 2`: passed after the final source edits. `python3 -u work/options-matrix/run-godot.py`: 54/54 exit 0, with `FRONTEND_SMOKE` 142 checks and zero failures. The existing `scene_play_flow` fixture retains two script-error log lines despite its zero exit.
- `bash work/options-matrix/run-sweep.sh`: 111/111 attempted, loaded and played with scripts; 230 checks passed, zero top-level failures or refusal entries. Script diagnostics still count 14 refusal stops and 266 stub refusals. This is a bounded sweep, not a full playthrough. Private disc was read only; counts only.
- Rendered `frontend_capture.gd`: 24/24 screenshots under ignored `work/`. Inspected both 1280×960 option lists; documented rows, frame art and font are visible, value bars are absent, and the refusal label appears. The test rejects submitted slider rectangles.
- Synthetic full combat: 2,620 checks, zero failures. `FULL_TRACE` 31,622 bytes, SHA-256 `4da5705531dd885150aa42a715fff04e88274b3ac65ea449d98c72070a4b78d9`, unchanged from R1. This is bounded regression evidence, not a full playthrough.

## Evidence limits

The controls sub-screen, executed pause parent transition and pad routing, pause communications destination, actual card config filename, localized value glyphs and retail pixels remain unverified. State-14 behavior for unchanged settings is refused. Non-audio gameplay setters still need verified client paths.
