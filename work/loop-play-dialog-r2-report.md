STATUS: DONE — PD-1 and PD-2 fixed; requested gates passed. Unsupported dialog targets remain explicit refusals.

PD-1: Attached-dialog trigger callee preflight now throws RefusedResult regardless
of result use, increments refusal/stopped counters, and preserves whole-set
preflight. Other command policies are unchanged. Statement-form regressions
require Stop::Refused and no campaign mutation for mixed targets, unknown/special
level domains, and cutscene NPCs. Synthetic script values are authored fixtures.

Evidence: research-dialog@334578d docs/dialog.md:92-93 establishes the base route;
docs/monsters-2.md:4304-4310 establishes NPC overrides; docs/spec/player-tick.md:75
pins the existing cutscene word. No target conditions or gameplay values changed.
Unconditional entry termination implements the audit's host refusal requirement.

PD-2: This is the intended work report; docs/PLAY_DIALOG.md now links it and
separates historical gates from this revision's validation.

Initial checkpoint: gates pending at implementation commit. The completed results
below supersede that checkpoint. Existing integration/journey limits remain unchanged.

Checkpoint: Release configure and build (`cmake --build build/native -j 4`)
passed. Existing ignored test-only level-flow -O0 workaround retained exactly
as documented in docs/PLAY_DIALOG.md; product libraries stay Release.
Focused CTest (dialog, dialog_gaps, script-commands, strings-exact,
ui-text-gaps-exact, frontend-exact): 6/6 passed, 1.69 s.
Release Godot extension rebuild (-j 4): passed.

Gate checkpoint: pending full CTest (`ctest --test-dir build/native
--output-on-failure -j 3`). This checkpoint is committed before starting it.

Runtime checkpoint: Godot 4.5.2 script-commands and play-hud smokes exit 0
(0.94 s / 0.50 s). Full-combat smoke exits 0 (299.59 s), including its soak
and save/load equality checks. FULL_TRACE is unchanged: 36,402 bytes,
SHA-256 9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659.
First-choice headless private dialog traversal exits 0 (93.91 s): 111 levels,
24 attempts, 22 opened, 22 closed by the harness, 10,161 checks, 0 failures.
There are 2 retained missing-camera-offset opening refusals and 3 newly visible
`confirm: refused trigger` stalls. The harness explicitly leaves stalled dialogs;
22 closed is not a claim of 22 normal completions. These refusals are the
expected consequence of stopping unsupported triggers instead of silently
continuing. This revision does not add support for those target domains.

Last-choice headless traversal with --voice-gate-probe: exit 0, 109.59 s,
111 levels, 24 attempts, 22 opened, 22 harness-closed, 10,142 checks,
0 failures, 1 voice-gate probe. It retains the same 2 opening refusals and
3 explicit refused-trigger stalls. No presentation code changed; these runs
verify runtime behavior and counts, not a fresh rendered-pixel acceptance.
At that checkpoint full CTest was still running; the final result follows.

## Final gate results — 2026-10-26

- Release native configure/build: PASS (`cmake -S native -B build/native
  -DCMAKE_BUILD_TYPE=Release`; `cmake --build build/native -j 4`).
- Focused CTest: PASS, 6/6, 1.69 s, -j 3 (selection listed above).
- Full CTest: PASS, **241/241**, **1,282.06 s**, exit 0:
  `ctest --test-dir build/native --output-on-failure -j 3`.
  The pending-full-CTest checkpoint was committed as e6b3fa2d before launch.
  No tests timed out; no isolated reruns were necessary. In this run
  player-exact passed in 134.25 s and session-facing-exact in 239.46 s.
  Full-run dialog / dialog_gaps / script-commands passed in 0.48 / 0.07 /
  0.48 s respectively.
- Godot Release extension rebuild: PASS (`cmake --build
  build/godot-extension -j 4`), using the existing pinned binding checkout.
- Godot runtime gates: all five commands exit 0. Script-command / HUD smoke,
  full-combat smoke, first-choice dialog traversal, and last-choice traversal
  with voice probe are recorded above. Dialog invocations use
  `res://tests/real_data_dialog.gd` with explicitly selected read-only private
  input/cache/summary arguments, headless; the second adds `--last-choice
  --voice-gate-probe`.
- FULL_TRACE: unchanged (size/hash above); FULL_SAVE_LOAD reports 300/300
  presents with identical uninterrupted/continued presentation and state hashes.
- Diff whitespace check: PASS. Changes are confined to the host refusal path,
  authored regression entries, documentation link and this report. No private
  input paths, decoded content or new game names are committed.

Evidence logs (ignored): work/play-dialog/r2-configure.log, r2-build.log,
r2-focused.log, r2-full.log, r2-ctest-results.json, r2-godot-build.log,
and r2-runtime/ (counts-only summaries and runtime logs). Earlier historical
results in docs/PLAY_DIALOG.md are not promoted into fresh acceptance.

Implementation commit: 8d7650d7. All commits are local; no push or PR.
Remaining boundary: three confirmed unsupported-trigger dialog stalls in each
traversal now surface explicitly, with harness cleanup. No claim of full game
playthrough completion, new target support or rendered-pixel validation is made.
