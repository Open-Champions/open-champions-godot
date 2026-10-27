STATUS: MORE — NHR6-1 and NHR6-2 fixed; Release, full CTest and affected Godot gates pass. Headless/rendered playthrough executed to completion but both remain red on existing gameplay/presentation boundaries.

# Native Horsemen round 7 — 2026-10-26

Base: e294193c. Assigned branch only; integration lifetime pattern inspected read-only. No actor lifetime ownership changes are required for these property callback fixes.

NHR6-1: the mounted fixture initializes the documented MiniBoss key address and the death-path key addresses reached by its existing run bodies. Unknown/empty key memory on a non-null property list refuses explicitly; a null list returns zero independently of key data. Registered callback regression uses the documented MiniBoss key address rather than a host string.

NHR6-2: original property adapters interpret argument three as occurrence index and return zero for absent entries. The audited lists contain each key once; validated nonzero occurrences return zero. The host-only integer fallback helper remains separate. Callback tests cover occurrences zero and one for both keys, missing keys at zero/one/seven, and unavailable key refusal.

Evidence: audited research pin 338d459c47d5e8e49dc4d1f393111910b2026f05, ref_arena_monsters.py:105-109,473-479,588-589; docs/spec/arena-monsters.md section 2.5; docs/objects.md:105-108; ref_objects.py:199-233; fixed lists from docs/spec/horsemen-mount.md section 2.2. No original executable difftest rerun or new original-data claim.

Release configure/build (-j 4): PASS. Final focused CTest: 5/5 PASS, 4.93 seconds. Initial iterations exposed missing authored animation service in the new callback test and uninitialized death-path keys; corrected using the same pinned research. A null-list key decode regression was caught by combat-play and fixed before this milestone.

The required **pending full CTest** note was committed in implementation milestone `37639d06` before starting the aggregate run. Documentation clarification is `9a0e965c`.

## Final gate results

- Release configure: `cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release`, PASS. Release build: `cmake --build build/native -j 4`, PASS. The player library and trace runner are arm64 with `-O3 -DNDEBUG`; no timeout, wrapper, vector, or acceptance setting was changed.
- Focused CTest: `ctest --test-dir build/native -R '^(horsemen-runtime-fight|horsemen-mount-contract|script-commands|combat-play|fixture-save)$' --output-on-failure`, **5/5 PASS, 4.93 seconds**.
- Full CTest: `ctest --test-dir build/native --output-on-failure -j 3`, **233/233 PASS, 980.69 seconds**, exit 0. `player-exact`: **PASS, 130.20 seconds**. `session-facing-exact`: **PASS, 198.67 seconds**. No test timed out or failed; no isolated timeout rerun was required. This supersedes round 6's incomplete native validation for the current implementation, without rewriting its historical failures.
- Godot archive extension: Release cached configuration, Universal 2, `cmake --build build/mac-template_debug --target champions-archive-extension -j 4`, PASS, 10.84 seconds. Existing cache binds this worktree as the source root; the separate integration runtime/dependency checkout was read only.
- Affected Godot tests, using the existing 4.5.2 runtime and `--headless --path godot --script res://tests/<script>.gd`: `playable_wave_smoke_full_combat` (`-- --ready-ticks=640`), **2,620 checks, zero failures**, 19.84 seconds; `playable_wave_smoke_wave12`, **34 checks, zero failures**, 1.67 seconds; `scene_play_flow_smoke`, **65 checks, zero failures**, 3.07 seconds. All exit 0; all three logs have zero engine/script/parse errors. This is the affected subset, not a new claim that the full 54-command matrix was rerun.
- `FULL_TRACE`: **unchanged**, 36,402 bytes, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`. Save/load continuation retains the same trace and equal presentation hashes, with 300 presents in both runs. The default trace excludes the opt-in mounted pair; direct registered-callback regressions and the mounted fight provide the affected coverage.
- `python3 tools/run_loop_playthrough.py --godot <existing-runtime> --game-folder <read-only-owner-disc> --out work/native-horsemen/r7-playthrough`: **FAIL, exit 1, 14.87 seconds**. Headless and rendered were both executed, each wrote a result with 19 steps and one diagnostic refusal kind. Headless has zero screenshots; rendered has 10 local screenshots. Disc file count remained **994 before / 994 after**. Both logs have zero engine/script/parse errors. The driver and GDScript are byte-identical temporary copies from authored commit `ab06ce0` (same hashes recorded in round 6); they and the generated UID were removed after validation. No gate criteria were changed.

## Playthrough blockers and acceptance boundary

Both modes pass boot, character creation, level entry, script enabling, ordinary interaction opening/closing dialog, host save, and return to main. Both retain these red stages:

- `enemy_fight`: zero actors before/after, attack input unhandled, no drops; combat is not enabled in this real-level flow. The existing explicit Full fixture is a separate passing test and cannot establish a real-level fight.
- `pause_save`: the menu has no save entry. `native/src/frontend.cpp` explicitly documents that normal pause has no save entry; save is reached through a savepoint, script or host. The unchanged driver still requires a pause save entry. Adding one or relabelling this stage would alter the acceptance contract, so neither was done. Host save passes separately.
- `load_and_carry`: the load callback reports its existing single-plain-terrain limitation (`gdextension/src/session_controller.cpp`); the frontend remains on the load screen. Retained HP/gold alone do not prove restoration.
- `level_change`: no pending script transition and no exposed exit target; no actual level change occurred.
- Rendered additionally refuses `dialog_visual`: frame open with resources ready, but zero box sprites and zero glyphs. The local dialog screenshot was visually inspected and confirms missing dialog presentation; no private image is committed.

These are the same outstanding flow requirements disclosed by the round-6 report. The pinned Horsemen property research supplies no implementation contract for the missing real-level combat composition, terrain/save restoration, transition discovery, or dialog draw work. They remain refused/unverified rather than guessed. **The playthrough gate is executed and documented, but is not green; overall acceptance remains MORE.**

No actor pool ownership or lifetime code changed; the integration branch's World/object/serial identity retirement pattern was inspected read only. No original game data, decoded content, private names, screenshots or absolute private paths are added to tracked artifacts. No push, PR, named-process kill, source-disc write, or sibling-worktree edit was performed.

Ignored local evidence: `work/native-horsemen/r7-{configure,build,focused,ctest}.log`, `r7-gates.json`, `r7-gates-runner.log`, the extension/Godot logs, `r7-godot-log-audit.json`, `r7-full-trace.json`, `r7-harness-provenance.json`, and the two playthrough result/log directories with counts-only run summary. Historical iteration logs remain local.

2026-10-26 coordination: both audit fixes are committed and all requested native/affected Godot comparisons pass; owner must retain the failed playthrough acceptance boundary when integrating.
