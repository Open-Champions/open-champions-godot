STATUS: MORE — dialog integration implemented; the journey retains existing gameplay refusals and the native player CTest deadline remains unresolved.

# Play dialog — 2026-10-24–25

Worktree `play-dialog`, branch `codex/play-dialog`, base `252f34a`.
Both earlier tips, `01b6e8a` and `fcfc7ac`, are ancestors of that base;
no duplicate merge or port was added. All commits are local.
Implementation commits: `bbf3dfb` (native host and synthetic cases), `34634c2`
(scene HUD/voice gates and the real-data harness), `6897873` (shared text option).

## Evidence and changes

Research: `research-dialog` `334578d` (`docs/dialog.md`, reference and
executed vectors), and `research-dialog-gaps` `79a2e4a`
(`docs/spec/dialog-gaps.md`, reference and vectors). The supplied
`cursor-runs/audits/research-dialog-gaps.md` is PASS. NPC slot-3 overrides
are observed at `0x29eed0` in the former pin's `docs/monsters-2.md`.

- Connected script `trigger` to documented base `Creature_trigger` paths.
  Preflight checks all named targets before dispatch; unknown classes and
  NPC override domains refuse. The mutex and active-player rules are kept.
- Mirrored `g_hudSuppressed` to the binder and HUD at open, rollback and end,
  including before the post action. Choice actions cannot show a HUD bust
  or compass while open; post actions can. The scene also hides/restores the
  status and auxiliary HUD layers and clears the front-end gate on detach.
- Passed stream-1 status into the text-off dialog draw gate. Added
  speaker/choice/cursor observations as counts. The NPC machine draws a
  speaker name and no portrait, exactly as the contract states.
  Draw and run tasks share the text option; an explicit playing/silent probe
  checks both the prompt and a blocked confirmation and restores text-on mode.
- Added authored script/object/DBIN integration regressions. The private
  scene harness checks HUD gating, speaker glyphs, last-choice traversal,
  fresh conversation starts and closure; it pumps displayed-frame fades
  during cleanup and stops confirming an accepted ending choice.

Native implementation/tests are in `script_dialog.cpp`, `script_commands.cpp`,
`script_commands.hpp`, `dialog_census.cpp`, `dialog_tests.cpp`,
`dialog_host_tests.hpp` and `dialog_play_tests.hpp`. Presentation/acceptance
changes are in `session_controller.cpp`, `real_data_play.gd`,
`real_data_dialog.gd` and `real_data_scene.gd`. Contract notes: [DIALOG.md](DIALOG.md).

The audit-fix work report is [loop-play-dialog-r2-report.md](../work/loop-play-dialog-r2-report.md).
This document retains the original integration results below; the work report records fresh gates.

## Gates

All results are local arm64 macOS with Godot 4.5.2 and the pinned godot-cpp.
Command paths below abbreviate the explicitly selected disc, Godot executable
and pinned binding checkout as `$RTA_DISC`, `$GODOT_BIN`, `$GODOT_CPP_DIR`.
Detailed output remains in ignored `work/play-dialog/` and `build/`.

- `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`;
  `cmake --build build/native -j 4`: **passed**, with the test-only compiler
  workaround below; final incremental build **32.50 s**.
- Focused CTest (`-R '^(dialog|dialog_gaps|script-commands|strings-exact|ui-text-gaps-exact|frontend-exact)$' -j 3`):
  **6/6 passed**, **4.73 s**.
- `ctest --test-dir build/native --output-on-failure -j 3`: **239/241 passed**,
  **3,444.64 s**, exit 8. `player-exact` hit CTest's **240.07 s** limit;
  `session-facing-exact` hit its trace subprocess's **600 s** limit
  (**602.15 s** test duration). No assertion failure was reported in the
  parallel run. Isolated `-R '^<name>$' -j 1` reruns: `player-exact` again
  timed out at **240.07 s**; `session-facing-exact` **passed in 390.26 s**.
  The identical player harness command, run directly with its internal limits
  unchanged, **passed all 274,304 exact complete-state cases and transport
  refusals in 302.02 s**, exit 0. Command: `python3
  tools/test_player_vectors.py --native build/native/champions-player-trace
  --vectors native/tests/vectors/player.tsv.gz` (the runner used CTest's exact
  Python executable and absolute paths). This separates the outer CTest
  deadline from a replay mismatch; **the full CTest gate is not clean**.
- Release configure/builds of `gdextension`, `gdextension/level_texture` and
  `gdextension/creature_anim`, with `GODOTCPP_TARGET=template_debug`,
  `CMAKE_OSX_ARCHITECTURES=arm64` and `$GODOT_CPP_DIR`: passed.
- Existing 54-command Godot development matrix: **54/54 exit 0**.
  `FULL_TRACE`: **36,402 bytes**, SHA-256
  `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`,
  unchanged. `FULL_SAVE_LOAD`: **300/300**.
- Four-slice `real_data_scene.gd --all --scripts --ticks=60` sweep: all
  slices exit 0; **111 tried / 111 loaded / 111 played**, **230 checks**,
  **0 failures**, no scene refusal kinds; **111 scripts attached / 76 started**.
- `real_data_dialog.gd` first-choice headless and last-choice rendered
  passes each load **111 levels**, with **24 talkers on 19 levels**, **24
  attempts / 22 opened / 22 closed / 0 stalls**. Both keep the same **2
  missing-`dialogOffset` opening refusals**. Each takes **48 choice confirmations**;
  the rendered pass draws the box on **1,309 frames** and speaker glyphs on
  **1,017**, with **10,205 checks / 0 failures**. The first-choice run includes
  the explicit playing/silent stream-status probe, restored before ticking.
  Three rendered captures were inspected: body, choices and closed, showing
  the status HUD hidden during talking and restored afterward.
  After the text-option bridge change, a separate last-choice
  `--voice-gate-probe` run passed **776 checks / 0 failures**, including
  blocked input and hidden prompt during the explicit playing-status stimulus.
- Native counts-only dialog census: **120 archives / 120 scripts**, **52
  DBIN files / 300 nodes / 328,800 bytes**, including **34 NPC files / 263
  nodes / 685 choices**; **26 talkers / 52 attempts / 44 opened / 44 ended /
  0 stalls**. The **8 refused openings** are **4 missing camera offsets / 4
  quoted missing-text keys**. Structural counts match the research census.
- Direct native replays: `champions-dialog-tests` **1,122,397 checks / 0 failed**;
  `champions-dialog-gaps-tests` **6,293 cases / 6,315 checks / 0 failed**.
- `python3 tools/run_loop_playthrough.py --godot "$GODOT_BIN" --game-folder
  "$RTA_DISC" --out work/play-dialog/playthrough-final`: **both variants exit 1**,
  **24 steps each**, **13 rendered screenshots**. Physical use opens a dialog;
  the rendered box, prompt and choice-close stages succeed. The journey still
  reports `blocked_unported_combat` for the ground-loot, pickup and container-
  Gold work, plus **2 refused exit-key script actions** while finding a working
  level transition. Save/load and the eventual level change succeed.
  Final-build rendered prompt and post-dialog captures were also inspected.
  Disc file count: **994 before / 994 after**.

The first matrix attempt lacked the separate helper extensions and aborted
before useful runtime verification. They were built and the complete matrix
was rerun. An intermediate native dialog binary preceded the NPC guard edit;
its three new override regressions failed. Only the final rebuilt results
above count as acceptance.

Local compiler workaround: AppleClang 21 spent over six CPU minutes in SROA
while compiling the unchanged `level_flow_tests.cpp` at `-O3`. A local ignored
CMake project-include applies `-O0` to
`champions-level-flow-tests` only:
`cmake_language(DEFER CALL target_compile_options champions-level-flow-tests PRIVATE -O0)`.
The final native configure adds
`-DCMAKE_PROJECT_INCLUDE="$PWD/work/play-dialog/native-test-compile.cmake"`.
The dialog and level-flow product libraries retain `-O3` and their strict FP
flags; all trace executables remain at `-O3`. The one test-harness translation unit is the
only optimization exception; it contains no client implementation. A reduced-
optimization profile also showed exception-edge dominator work. The final
local override disables optimization for that harness.
No repository CMake file or sibling process was changed.

## Refusals and limits

- Missing/incomplete `dialogOffset`, absent box word, unsafe buffer/choice
  bounds and the quoted-key `NF:` uncertainty retain their explicit refusals.
- Trigger targets without a documented base slot-3 route refuse before any
  target runs. NPC cutscene and special-level animation override domains,
  including an unknown level, remain conservatively refused.
- Conversation camera poses remain published, not applied to the Godot follow
  camera. NPC model/mouth rendering and `creatures_freezeAll` body integration
  are unchanged boundaries. Cutscene line-list playback and network dialog
  are separate paths. Voice/lip-sync alignment remains unverified.
- Deferred shops remain counted host events; vendor presentation is separate.
  The bounded use adapter still lacks the original interaction prompt's
  cone and hostile-creature filtering, as documented in `DIALOG.md`.
- No game text, retail bytes or extracted assets are in the patch. Real-data
  inputs are read-only and results are counts only. Other platforms and x64
  trace comparison were not run.
