STATUS: PENDING — WAVE 37 merges committed; Release, GDExtensions, and CTest 259/259 PASS; Godot gates in progress.

# Playable integration wave 37 — 2026-11-03

## Scope, evidence and merges

Assigned clean `codex/playable-integration` at wave-36-verified `76c5b35e`. Read the wave 36 report and `work/playable-wave36/` method, the three PASS audits named in the merge list, and the three branch reports before merging. Full no-ff merges in the requested order. No omissions or aborts. Builds and tests run on the Mac mini from an rsynced tree that excludes `.git`, `build/`, `work/` and `.godot/`. Private disc read-only, counts only. No push, PR, sibling edits, or process-name termination.

| Branch | Audited tip | PASS audit | Merge commit | Conflicts |
| --- | --- | --- | --- | --- |
| `codex/play-death-reload` | `e3df4e51` | `play-death-reload-r1` | `dc75d195` | none (tip already descended from `76c5b35e`) |
| `codex/native-arena` | `80cea03d` | `native-arena-r5` | `0791e953` | none; ort auto-merge |
| `codex/play-loading-readiness` | `d07f7e4e` | `play-loading-readiness-r3` | `8e39f89e` | none; ort auto-merge |

All three audited tips are ancestors of the merge HEAD `8e39f89e`.

## Conflict resolutions

No conflict markers. Non-trivial auto-merges, both sides kept:

- Arena into the death-reload tip: `native/CMakeLists.txt` gained `arena-scheduler-runtime` beside the existing `arena-match-host` test. `combat_runtime.hpp` kept `Services::death_reload_requested` and added `World::arena_begin` / `World::arena_start_wave`. `combat_runtime_api.cpp` kept the existing visit/reset/death bodies and added the two arena entry points that call `arena_match::Model::begin` and `start_wave`.
- Loading readiness into that result: `script_commands_tests.cpp` kept the integration tests added after `4dbee736` and the round-3 equipment fixup controls. `loading_equipment.hpp` / `loading_equipment.cpp` took the round-3 fixup and rollback on the already-merged round-2 loading code.

## Non-merge code changes

None yet. Source changes so far are the incoming audited work plus the auto-merges above.

## Gates

Evidence under ignored `work/playable-wave37/`. Native configure/build and the extension builds ran on the mini from the rsynced tree. Godot is 4.5.2.stable. Registered CTest count before execution is 259 (`ps2-class-restore`, `arena-scheduler-runtime`, plus the wave-36 set).

- Native Release configure/build **PASS**, exit 0, 110.77 s combined, `-j 4`. Evidence `configure.log`, `build.log`. New binaries `champions-ps2-class-restore-tests` and `champions-arena-scheduler-tests` are present.
- GDExtensions **PASS**, exit 0, parallelism 4, `arm64` and `x86_64`. Archive and level-texture `template_debug` / `template_release` each configured and linked. `lipo -archs` reports both architectures. Evidence `template_debug-*.log`, `template_release-*.log`, `texture-template_debug-*.log`, `texture-template_release-*.log`. Creature-animation `template_debug` **PASS**, universal, linked to `godot/creature_anim/bin`. Evidence `creature-template_debug-configure.log`, `creature-template_debug-build.log`.
- Full CTest `-j 3`: **PASS, 259/259, 694.03 s**, exit 0. Evidence `ctest.out`. Wave 36 was 257/257. The two added tests are `ps2-class-restore` (death-reload) and `arena-scheduler-runtime` (arena round 5). Loading round 3 extended `script-commands` in place. Two earlier full runs are not this gate: the driver left shell text on standard input, and `amx-validation-transport` then took its invalid-command return after printing format refusal 17. The same test passes with standard input closed. No test, fixture, or source was changed.
- Strict Godot dev matrix: PENDING. The inherited `ui_layout_smoke` four-argument diagnostic is not filtered. On this integration base the smoke already inspects the five-argument signature (`godot/tests/ui_layout_smoke.gd`) and does not issue the invalid call; arena round 5 and loading round 3 recorded the diagnostic only because their older trees still contained it.
- FULL_SAVE_LOAD 300/300: PENDING
- FULL_TRACE 36,402 bytes / `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`: PENDING. None of the three branch reports records a trace change.
- Journeys versus wave 36 (80/81, pickup 10→11, death-reload refusal): PENDING. Death-reload round 1 retained that same refusal. Loading round 3's 67/68 counts are that older branch's baseline, not this integration tree.
