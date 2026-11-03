STATUS: MORE — WAVE 37 VERIFIED (2026-11-03). Three audited tips integrated; native CTest 259/259, strict Godot 54/54, combat/save-load/unchanged FULL_TRACE. Journey matches wave 36 at 80/81 steps with pickup 10→11 and explicit death-reload refusal. No non-merge source changes.

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

None. Source changes are the incoming audited work and the auto-merges above. No post-merge source correction was necessary.

## Gates

Evidence under ignored `work/playable-wave37/`. Native configure/build and the extension builds ran on the mini from the rsynced tree. Godot is 4.5.2.stable. Registered CTest count before execution is 259 (`ps2-class-restore`, `arena-scheduler-runtime`, plus the wave-36 set).

- Native Release configure/build **PASS**, exit 0, 110.77 s combined, `-j 4`. Evidence `configure.log`, `build.log`. New binaries `champions-ps2-class-restore-tests` and `champions-arena-scheduler-tests` are present.
- GDExtensions **PASS**, exit 0, parallelism 4, `arm64` and `x86_64`. Archive and level-texture `template_debug` / `template_release` each configured and linked. `lipo -archs` reports both architectures. Evidence `template_debug-*.log`, `template_release-*.log`, `texture-template_debug-*.log`, `texture-template_release-*.log`. Creature-animation `template_debug` **PASS**, universal, linked to `godot/creature_anim/bin`. Evidence `creature-template_debug-configure.log`, `creature-template_debug-build.log`.
- Full CTest `-j 3`: **PASS, 259/259, 694.03 s**, exit 0. Evidence `ctest.out`. Wave 36 was 257/257. The two added tests are `ps2-class-restore` (death-reload) and `arena-scheduler-runtime` (arena round 5). Loading round 3 extended `script-commands` in place. Two earlier full runs are not this gate: the driver left shell text on standard input, and `amx-validation-transport` then took its invalid-command return after printing format refusal 17. The same test passes with standard input closed. No test, fixture, or source was changed.
- Strict Godot dev matrix: **54/54 PASS**, exit 0, `--captures`, timeout 1200, isolated projects and user directories. Evidence `matrix/results.json`. The unchanged checker found 0 diagnostic errors in the matrix logs. `ui_layout_smoke` PASS. Its script still inspects the five-argument `hud_text_glyphs` signature and does not make the invalid four-argument call (`godot/tests/ui_layout_smoke.gd`). Arena round 5 and loading round 3 recorded that diagnostic only on their older trees, where the call was still present. It did not recur here and was not filtered or allowlisted.
- Full synthetic combat **2,645 checks, 0 failures** (`matrix/playable_wave_smoke_full_combat.log`).
- FULL_SAVE_LOAD **PASS, 300/300**. Continued and uninterrupted presentation SHA-256 `cb2309ab23d3e2906b5b9d4e95e5577c354869fce3d86cce9fe592a0d2c8d573`, identical.
- FULL_TRACE **PASS, unchanged**, 36,402 bytes, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`. Death-reload, arena round 5, and loading round 3 each recorded this same trace. No baseline was edited.
- Journeys: unchanged `tools/run_loop_playthrough.py`, serial headless then rendered, standard input closed. A headless editor import ran first because the rsync excluded `.godot` (exit 0, `editor-import.log`). Evidence `playthrough.out` and `playthrough/`.

## Journey result

| Variant | Steps | Exit | Ending | Pickup | Log errors | Captures |
| --- | --- | --- | --- | --- | --- | --- |
| Headless | 80 | 1 | `player_death` refused; `saved_level_reloaded` false | **10→11**, produced/walked/picked up, 52 walk ticks | 0 | 0 |
| Rendered | 81 | 1 | `player_death` refused; `saved_level_reloaded` false | **10→11**, produced/walked/picked up, 52 walk ticks | 0 | 38 |

Disc file count **994 before / 994 after**. Six refusal kinds each. Exact `refused` rows: 27 headless / 28 rendered, matching the death-reload round 1 accounting. Step name and result pairs match wave 36 at every index. The refusal maps match wave 36. Wrapper exit 1 is the retained full-journey refusal.

Host-only differences versus wave 36, not simulation results: headless `fps` 101→134 and `enemy_fight` `swing_wall_msec` 30157→25024; rendered `swing_wall_msec` 34560→27173. `swing_wall_msec` is `Time.get_ticks_msec()` around the swing loop (`godot/tests/loop_playthrough.gd:1578-1587`). `memory_bytes` also moved. Death-reload round 1 already recorded this same 80/81 refusal, pickup 10→11, and 52 walk ticks on top of wave 36. Arena round 5 did not run this journey. Loading round 3's 67/68 counts are that branch's older baseline, from before the later integration steps that reach 80/81; this merge did not remove those steps.

`rendered_pickup_verified` remains false. Successful death reload remains refused, as death-reload round 1 concluded: the journey stops on the dead-Creature save guard before the diagnostic lethal-hit sequence (`native/src/script_commands_objects.cpp:138-139`, `godot/scenes/real_data_play.gd:409-414`, `godot/tests/loop_playthrough.gd:1957-1958,2000-2001`).

## Remaining boundaries

All three requested no-ff merges are committed in order. All audited tips are ancestors of the merge. No source change beyond those merges. Unknown arrival commands, continuous Mission 1 execution, live arena/PvP, and successful death reload stay refused. No values were invented. No push, PR, private-disc write, sibling edit, or process-name termination.
