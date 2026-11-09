STATUS: VERIFIED — WAVE 38 (2026-11-08). Three audited tips integrated; native CTest 260/260, strict Godot 54/54, combat 2,645/0, FULL_SAVE_LOAD 300/300, FULL_TRACE unchanged. Journeys 80/81 with pickup 10→11, ending at the death-reload refusal. One mission-menu admission path. No non-merge code changes.

# Playable integration wave 38 — 2026-11-03, gates finished 2026-11-08

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

Rechecked on the merged tree (2026-11-08): `dialog_host_.mission_menu` has exactly one writer, `native/src/script_dialog.cpp:285`, reading `campaign_->menu_run_registered()`. Its readers are the two suppression points (`script_dialog.cpp:300,310`) and the open-time gate (`script_dialog.cpp:447`). `menu_run_` is set only by the quest run task (`native/src/quests.cpp:664,693,724`) and retained across same-process load (`native/src/script_commands.cpp:1124-1126`). `native/src/mission2_route.cpp` does not reference the run task or `mission_menu`, and nothing outside its own files and test calls `mission2_route`. `hud_round3.cpp:329` copies a caller-supplied `mission_menu` into the medal prompt for presentation; it is not an admission path.

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

- Journeys: unchanged `tools/run_loop_playthrough.py`, serial headless then rendered, standard input closed, read-only disc. A headless editor import ran first (`--editor --quit-after 120 --frame-delay 50`, exit 0, 10 s, `editor-import.log`). Wrapper exit 1, 94 s wall. Evidence `playthrough.out`, `playthrough/run-summary.json`, `playthrough/*/result.json`, `playthrough/*/godot.log`.

## Journey result

| Variant | Steps | Godot exit | Ending | Pickup | Refused rows | Refusal kinds | Log errors | Captures |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Headless | 80 | 1 | `player_death` refused; `saved_level_reloaded` false | **10→11**, produced/walked/picked up, 52 walk ticks, 0 retries | 27 | 8 | 0 | 0 |
| Rendered | 81 | 1 | `player_death` refused; `saved_level_reloaded` false | **10→11**, produced/walked/picked up, 52 walk ticks, 0 retries | 28 | 8 | 0 | 38 |

Disc file count **994 before / 994 after**. Step name and result pairs match wave 37 at every index in both variants. The refused-row counts match too: 27 headless, 28 rendered. Wrapper exit 1 is the retained full-journey refusal, as in wave 37.

Refusal kinds 6→8. This comes from a merged branch. The two added kinds are the hub-action refusal that `codex/play-progression` introduced, `native/src/script_dialog.cpp:320` (`hub action beyond documented first/second offers is Unknown`). It is reported once by the dialog confirm and once by the tick. The six wave-37 kinds are unchanged. Progression round 3's audited report records the same eight kinds on its own journeys (`work/loop-play-progression-r3-report.md:47`). Mission 2 round 1 recorded six because it did not contain progression. The node-3 retry count is headless 2 and rendered 3. That is within the expected 1–4 range. Progression explains it as the close-fade retry, not a second grant (`work/loop-play-progression-r3-report.md:34`). Neither variant has a `run-task termination` refusal.

Host-only differences versus wave 37, not simulation results: headless loot `fps` 134→103 and `enemy_fight` `swing_wall_msec` 25024→26021; rendered loot `fps` 2→4 and `swing_wall_msec` 27173→24342 (`Time.get_ticks_msec()` around the swing loop, `godot/tests/loop_playthrough.gd:1740`). `memory_bytes` also moved slightly. Other agents share the mini.

`rendered_pickup_verified` remains false, as in wave 37. Successful death reload remains refused at the dead-Creature save guard, as in wave 37.

## Remaining boundaries

All three requested no-ff merges are committed in order, and every audited tip is an ancestor. No source, test, baseline, or fixture was changed beyond those merges and the conflict resolutions above; this gate session made no code change. Unknown arrival commands, the hub2 dialog and loader body, continuous Mission 1 execution, live arena/PvP, and successful death reload stay refused. No values were invented. No push, PR, private-disc write, edit under `~/champions-re`, or process-name termination. The mini's w38 `build/` was deleted after verification.
