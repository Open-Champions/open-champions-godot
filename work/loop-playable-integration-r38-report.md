STATUS: MORE — wave 38 merges are committed. Remote gates have not run yet.

# Playable integration wave 38 — 2026-11-03

## Scope, evidence and merges

Assigned clean `codex/playable-integration` at wave-37-verified `70d2de19`. Read the wave 37 report and `work/playable-wave37/` method, the three PASS audits named in the wave 38 section of the merge list, and the three branch reports before merging. Full no-ff merges in the requested order. No omissions or aborts. Builds and tests run on the Mac mini from an rsynced tree that excludes `.git`, `build/` and `work/`, and keeps `godot/.godot`. Private disc read-only, counts only. No push, PR, sibling edits, or process-name termination.

| Branch | Audited tip | PASS audit | Merge commit | Conflicts |
| --- | --- | --- | --- | --- |
| `codex/play-progression` | `c583578a` | `play-progression-r3` | `91c96fd1` | `native/src/script_commands.cpp` |
| `codex/play-loading-readiness` | `5ab36040` | `play-loading-readiness-r4` | `27925364` | `native/src/campaign_fight.cpp` |
| `codex/play-mission2-route` | `cbcb264e` | `play-mission2-route-r1` | `b7e8e827` | none; ort auto-merge |

All three audited tips are ancestors of merge HEAD `b7e8e827`.

## Conflict resolutions

Non-trivial resolutions, both sides' behaviour kept:

- Progression into wave 37: `Host::load` resets both the existing Mission 1 contract (`mission1_`, `mission1_sites_reached_`) and the incoming hub-offer latch (`hub_offers_`, start, opening, action mission, action-running). The run-task save/load retain, the `setMissionAvailable` hub-action guard, and the Mission 1 dispatch stay in the same file. `script_commands.hpp` kept Mission 1 and loading-readiness members and added the hub-offer binding. `session_controller.cpp` kept the existing fight observation and added `bounded_hub_dialog` / `bounded_hub_start`.
- Loading readiness into that result: `loading_readiness` gained `report_arrival_limits` (default false) and the five named limits. Existing one-argument tests still compile. `session_controller.cpp` asks for those names on the live readiness string and still reports `fight_starting_equipment = runtime_record`. `frontend.cpp` took the round-4 creation note. `script_commands_tests.cpp` kept the integration cases and the round-4 selection controls.
- `campaign_fight.cpp` `setup_player`: round 4 replaced the comment above `refuse("starting_equipment: refused_unknown ...")`. That call is unchanged from round 3 (`d07f7e4e`). Integration removed the same call in `2707fa64` when `pickups::give_starting_items` replaced the unported stand-in, and wave 37's merge of round 3 kept that deletion because round 3 had not edited the lines. This resolution keeps the round-4 comment, so the fight records that it does not run initializer `0x1b1d70`, and does not restore the refuse call. Restoring it would label the ported grant "not ported" while `fight_observation` still publishes `runtime_record` / `inventory_equipItem`. `give_starting_items` stays.

Mission 2 did not conflict. Its new files are the direct-call route, the CMake test, and the route note.

## One admission path

Dialog admission reads the menu run task in one place: `native/src/script_dialog.cpp` sets `dialog_host_.mission_menu` from `campaign_->menu_run_registered()`. The campaign image does not carry that bit; `Host::load_campaign` retains the live bit on the same process. Mission 2 does not copy that path. `mission2_route::portal_row0` accepts row 0 through the existing `level_flow::GateTravel` counter and the pre-load stop. It does not call `missionMenu_run` or `menu_run_registered`. The scene does not call `mission2_route`.

## Non-merge code changes

None. Source changes are the incoming audited work and the resolutions above. No post-merge source correction was necessary.

## Gates

Evidence under ignored `work/playable-wave38/`. Native configure/build and the extension builds ran on the mini from the rsynced tree. Godot is the mini's 4.5.2 runtime. Registered CTest count before execution is 260 (`mission2-route` plus the wave-37 set of 259). Progression and loading round 4 extended existing tests in place.

- Native Release configure/build **PASS**, exit 0, 121.31 s wall for the remote configure and `-j 4` build. Evidence `configure.log`, `build.log`. New binary `champions-mission2-route-tests` is present. `champions-mission2-route` linked.
- GDExtensions **PASS**, exit 0, 317.59 s wall, parallelism 4, `arm64` and `x86_64`. Archive and level-texture `template_debug` / `template_release` each configured and linked. `lipo -archs` reports both architectures on all five libraries. Evidence `template_debug-*.log`, `template_release-*.log`, `texture-template_debug-*.log`, `texture-template_release-*.log`. Creature-animation `template_debug` **PASS**, universal, linked to `godot/creature_anim/bin`. Evidence `creature-template_debug-configure.log`, `creature-template_debug-build.log`.

- Full CTest `-j 3`: **PASS, 260/260, 810.97 s**, exit 0. Evidence `ctest.out`. Wave 37 was 259/259 in 694.03 s. The added test is `mission2-route` (Passed, 0.14 s). Progression's run-task coverage and loading round 4's selection controls extended `quests`, `dialog`, and `script-commands` in place. Standard input was closed. No test, fixture, or source was changed. The longer wall time is the shared mini: `netplay` alone took 183.68 s inside this run.

- Strict Godot dev matrix: **54/54 PASS**, exit 0, 131 s wall, `tools/godot_matrix.py matrix --captures --timeout 1200`, standard input closed, isolated projects and user directories. Evidence `matrix.out`, `matrix/results.json`. The case list is identical to wave 37's 54 cases in the same order, so there is no size difference. The unchanged `check-log` found 0 diagnostic errors across all 105 matrix logs. Nothing was filtered or allowlisted. `ui_layout_smoke` PASS. The previous agent had run one matrix (19:34, also 54/54) but recorded no command or exit code before the provider cut-off. That run is kept as `matrix-unrecorded-1934/` and is not this gate.
- Full synthetic combat **2,645 checks, 0 failures** (`matrix/playable_wave_smoke_full_combat.log`), same as wave 37.
- FULL_SAVE_LOAD **PASS, 300/300**. Continued and uninterrupted presentation SHA-256 `cb2309ab23d3e2906b5b9d4e95e5577c354869fce3d86cce9fe592a0d2c8d573`, identical, the same as wave 37.
- FULL_TRACE **PASS, unchanged**, 36,402 bytes, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`, headless renderer. No baseline was edited.

Journeys are pending.
