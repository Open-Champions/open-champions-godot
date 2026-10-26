STATUS: IN_PROGRESS

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

Gates: pending Release build, focused native tests, full CTest, dialog Godot tests,
and FULL_TRACE comparison. Existing integration/journey limits remain unchanged.

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
