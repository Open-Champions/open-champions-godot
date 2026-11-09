STATUS: VERIFIED — WAVE 39 (2026-11-09). Three audited tips integrated; native CTest 262/262, strict Godot 54/54, combat 2,645/0, FULL_SAVE_LOAD 300/300, FULL_TRACE unchanged. Journeys 80/81 with pickup 10→11, ending at the death-reload refusal. Rendered refusal kinds 8→6: the hub-action retry count fell to 0 from frame timing, reproduced on a rerun (explained below). No non-merge code changes.

# Playable integration wave 39 — 2026-11-09

## Scope, evidence and merges

Assigned clean `codex/playable-integration` at wave-38-verified `16b66176`. Followed the wave 38 method (`work/loop-playable-integration-r38-report.md`) and `~/Projects/champions-re-wt/cursor-runs/mini-workflow.md`. Merged the three commits in the wave 39 section of `cursor-runs/audits/wave37-merge-list.txt`, in order, as full no-ff merges on the MacBook (git only). No omissions or aborts. All three branches fork from wave 37 `70d2de19`.

| Branch | Audited tip | PASS audit | Merge commit | Conflicts |
| --- | --- | --- | --- | --- |
| `codex/play-give-item` | `ae265217` | `play-give-item-r1` | `8e8ed866` | `native/src/script_commands.cpp` |
| `codex/play-spells-numeric` | `cfb807da` | `play-spells-numeric-r1-fix` | `ec77f7dd` | none; ort auto-merge (`native/CMakeLists.txt`) |
| `codex/play-exits-travel` | `91c76e2c` | `play-exits-travel-r1` | `856bcb68` | none; ort auto-merge |

All three audited tips are ancestors of merge HEAD `856bcb68`.

## Conflict resolutions

One conflict. Both sides kept:

- Give-item into wave 38, `native/src/script_commands.cpp`, `Host::command_map()` dispatch lambda, after `DialogSync`. HEAD (progression, wave 38) added the `setMissionAvailable` hub-action guard: it moves the count from executed to refused/stopped and throws `RefusedResult` before the handler runs (research-mission-unlocks@950b2538 `docs/spec/mission-unlocks.md:88-113`). Give-item added `CommandScope`, which sets `script_items::Service::command` to the running handler's name and restores it on exit, for nested entry runs started by an item notification. The resolution puts the guard first, then the scope, directly before `result = function(machine, params)`. The scope covers exactly the handler call, as on the give-item branch. A refused hub write never enters the handler, so it never sets the item-service command name. No line from either side was changed.

Semantic check on the auto-merged exits-travel change: `level_flow::LevelStates::store` now returns `StoreResult` instead of `bool`, and `ChangeResult` gained `saved_bytes`. The non-test callers are both in `native/src/level_flow.cpp`: `begin_change` at `:1018-1022`, which exits-travel itself updated to take `.written()`, and the older checkpoint call at `:1151` (from `23c15811`), which ignores the return value and so compiles unchanged. `gdextension/src/session_controller.cpp:1452` calls `store` on a different type (`level_states_`, pointer-returning `lookup`). Wave 38's `native/src/mission2_route.cpp` uses only `GateList`, `GateTravel`, `Pending` and the orientation constants, none of which changed.

## Privacy check (PSN-9)

`codex/play-spells-numeric` commit `6db68a82` put `~/malik-champions/open-champions/worktrees/play-spells-numeric/build/native` into `work/play-spells-numeric/ctest.out`; `cfb807da` replaced it with `<mini worktree>`. At merge HEAD, `work/play-spells-numeric/ctest.out` line 1 reads `Test project <mini worktree>/build/native`, and no file under `work/play-spells-numeric/` contains `malik-champions`. No tracked file at the tip contains the mini's absolute home path. None of the files the three merges changed contains a macOS absolute home path (the `/` `Users` `/` prefix; written split here so this report adds no match). The set of tracked files containing that prefix is the same as wave 38's `16b66176`: 25 files, all older MacBook-side paths under this machine's own user (3 in `docs/`: `INTEGRATION.md`, `WARBOAR_AI_REPORT.md`, `WARBOAR_LIFETIME_FIX_REPORT.md`; 22 earlier `work/loop-*-report.md` files). They predate this wave, and this wave did not touch them. Two incoming reports name the mini layout in the workflow's allowed `~/malik-champions/...` form (`work/loop-play-exits-travel-r1-report.md:12`, `work/loop-play-give-item-r1-report.md:5`).

## Non-merge code changes

None. Source changes are the incoming audited work and the resolution above.

## Gates

Evidence under ignored `work/playable-wave39/` (copied back from the mini; the mini home path is rewritten to `~` in copies). Native configure/build and the extension builds ran on the mini from the rsynced tree: rsync excluded `.git`, `build`, `work`, `.godot`, `godot/bin` and `godot/creature_anim/bin`. The w39 tree was seeded from the mini's `play-exits-travel` tree, then delta-rsynced with `--delete` from this worktree at `856bcb68`. A checksum (`-c`) dry run then reported no differences. Godot is the mini's 4.5.2 runtime. godot-cpp is `~/malik-champions/runtime/godot-cpp` at the pinned `e83fd0904c13356ed1d4c3d09f8bb9132bdc6b77`. Commands: `work/playable-wave39/gates.sh`. Parallelism is `-j 2` throughout, as instructed, because the mini is under heavy load (load average ~90–100).

Disk: `df -h ~` showed 39 GB free at start, 24 GB before the native build and 19 GB before the extension builds. Other jobs on the mini drove it down to 6 GB for a few minutes during the extension build; it then recovered to 30–48 GB. This wave's footprint at the end was about 7.3 GB: tree 1 GB plus `build/` 6.3 GB, which includes native build and CTest outputs and the five extension builds. `build/` was deleted after verification, and free space was 53 GB afterwards.

- Native Release configure/build **PASS**, exit 0, 1,145 s wall at `-j 2` (wave 38 took 121 s at `-j 4` on a less loaded mini). New binaries `champions-give-player-item-tests` and `champions-spell-numerics-tests` are present. The only linker warnings are the same 10 `ignoring duplicate libraries` notes as wave 38, identical by count and text. Evidence `configure.log`, `build.log`.
- GDExtensions **PASS**, exit 0, 2,024 s wall, parallelism 2, `arm64;x86_64`. Archive and level-texture `template_debug` / `template_release` each configured and linked. Creature-animation `template_debug` linked to `godot/creature_anim/bin`. `lipo -archs` reports `x86_64 arm64` on all five libraries (`ext-lipo.txt`). Compiler warnings are the same two as wave 38, four times each: `ui_hud_round3_service.cpp:53` unused `submission` and `level_texture.cpp:388` missing `stored_normals` initializer. Evidence `template_*-*.log`, `texture-template_*-*.log`, `creature-template_debug-*.log`.

- Full CTest `-j 2`: **PASS, 262/262, 2,611.42 s**, exit 0 (`100% tests passed, 0 tests failed out of 262`). Evidence `ctest.out`. Wave 38 was 260/260. The two added tests come from the merges: `give-player-item` (Passed, 0.26 s; `codex/play-give-item`) and `spell-numerics` (Passed, 0.28 s; `codex/play-spells-numeric`). `codex/play-exits-travel` extended `native/tests/level_flow_tests.cpp` in place (`level-flow-exact` Passed, 0.31 s). `mission2-route` Passed, 0.17 s, after the `level_flow` API change. Standard input was closed. No test, fixture or source was changed. The longer wall time comes from `-j 2` on a mini at load ~100: `netplay` alone took 258.28 s (wave 38: 183.68 s) and `player-exact` 73.05 s.

- Headless editor import. The first import (`--headless --path godot --editor --quit-after 120 --frame-delay 50`, exit 0, 18 s, `editor-import.log`) quit before the first filesystem scan finished (`WARNING: Scan thread aborted`). That left `godot/.godot/imported` empty. Wave 38's tree had a populated `.godot`. This wave's rsync excludes `.godot` as instructed, so the tree started without one. The first matrix run therefore failed 25 of 54 cases, all on the same missing import (`res://.godot/imported/open_champions_avatar.png-….ctex` / `Failed loading resource: res://assets/open_champions_avatar.png`). It is kept as `matrix-noimport/`, `matrix-noimport.out` (29/54, exit 1) and is not this gate. A complete import (`--headless --path godot --import`, which waits for all imports and then quits; exit 0, 18 s, `editor-import-full.log`, 0 error lines) produced the project's only imported asset: `.ctex` + `.md5`, from the single `*.import` file in the project. No source, test, baseline or tool changed. The full matrix was then rerun from scratch.
- Strict Godot dev matrix: **54/54 PASS**, exit 0, 282 s wall, `tools/godot_matrix.py matrix --captures --timeout 1200`, standard input closed, isolated projects and user directories. Evidence `matrix.out`, `matrix/results.json`. The case list matches wave 38's 54 cases in the same order. The unchanged `check-log` found 0 diagnostic errors across all 105 matrix logs (`check-log.out`). Nothing was filtered or allowlisted.
- Full synthetic combat **2,645 checks, 0 failures** (`matrix/playable_wave_smoke_full_combat.log`, `PLAYABLE_PLAY_RESULT`), same as wave 38. `codex/play-spells-numeric` changed live cast-debit numerics in `player.cpp` / `skills_b.cpp`. Its own audited gates also kept combat at 2,645/0 and the trace unchanged (`work/play-spells-numeric/full-combat.txt`, `full-trace.txt`).
- FULL_SAVE_LOAD **PASS, 300/300**. Continued and uninterrupted presentation SHA-256 are both `cb2309ab23d3e2906b5b9d4e95e5577c354869fce3d86cce9fe592a0d2c8d573`, the same as wave 38.
- FULL_TRACE **PASS, unchanged**, 36,402 bytes, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`, headless renderer. No baseline was edited.

- Journeys: unchanged `tools/run_loop_playthrough.py`, serial headless then rendered, standard input closed, read-only disc. They ran after the complete import above. Wrapper exit 1, 186 s wall. Evidence `playthrough-raw.out`, `playthrough/run-summary.json`, `playthrough/*/result.json`, `playthrough/*/godot.log`. A second, identical run (`playthrough-rerun/`, wrapper exit 1) was made only to test the rendered refusal-kind difference. Both runs are reported.

## Journey result

| Variant | Steps | Godot exit | Ending | Pickup | Refused rows | Refusal kinds | Hub-action count | Log errors | Captures |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Headless | 80 | 1 | `player_death` refused | **10→11**, produced/walked/picked up, 52 walk ticks, 0 retries | 27 | 8 | 1 (rerun 1) | 0 | 0 |
| Rendered | 81 | 1 | `player_death` refused | **10→11**, produced/walked/picked up, 52 walk ticks, 0 retries | 28 | **6** (rerun 6) | **0** (rerun 0) | 0 | 38 |

Disc file count **994 before / 994 after** in both runs. Step name and result pairs match wave 38 at every index in both variants and both runs. The refused-row counts match too: 27 headless, 28 rendered. Wrapper exit 1 is the retained full-journey refusal, as in wave 38. Neither variant has a `run-task termination` refusal. `rendered_pickup_verified` remains false, as in wave 38. Successful death reload remains refused at the dead-Creature save guard.

Headless refusal kinds are the same eight as wave 38. Only the count suffix of the hub-action reason changed: `count=2` → `count=1`.

Rendered refusal kinds 8→6. The two kinds absent in wave 39 rendered are the pair that progression introduced: `dialog: confirm: hub action beyond documented <path> offers is Unknown count=N` and its `tick: dialog confirm: …` echo (`native/src/script_commands.cpp` `setMissionAvailable` guard; wave 38 report). The other six rendered kinds are unchanged. The difference comes from frame timing, not from a merged branch:

- The count is the post-grant node-3 retry on the close-fade schedule. It is not a second grant (`work/loop-play-progression-r3-report.md:34`). Progression r3 recorded counts 1–4 depending on frame speed.
- The confirm loop (`godot/tests/loop_playthrough.gd:785-792`) presses ENTER every 8 frames while the dialog is open and calls `advance()` once per frame. The scene also advances native ticks by monotonic wall time on every frame (`godot/scenes/session_sandbox.gd:91-105`, `controller.advance_time(elapsed, …)`). Slower frames therefore run more ticks between confirms. The fade then closes after fewer confirms, and fewer retries reach the guard.
- Observed counts follow frame rate. Wave 38: headless 2 at 145 fps, rendered 3 at 170 fps. Wave 39: headless 1 at 91 fps, rendered 0 at 34/26 fps (the dialog steps' `fps`). At 0, no retry reaches the guard, so neither reason is reported and the two kinds disappear. The rerun reproduced headless 1 at 115 fps and rendered 0 at 35/29 fps, again with `choices_taken` 3 and the quest set 0→1.
- The dialog outcome is unchanged: `dialog_choice_close` closed with `choices_taken` 3, and `opening_choice_quest` set 0→1 in both variants. `hub_trigger_mission_menu` (step 27) is identical to wave 38 except for `fps` and `memory_bytes`.
- None of the three merges touched `godot/`, `tools/run_loop_playthrough.py`, `native/src/script_dialog.cpp`, `native/src/quests.cpp` or `gdextension/src/session_controller.cpp`. The only shared file is `native/src/script_commands.cpp`. There the guard still runs first and still fires on this build (headless count 1 in both runs). `script-commands`, `dialog` and `quests` CTest passed.

Host-only differences versus wave 38, not simulation results. Across all four runs, the only per-step field that differs from wave 38 other than `fps` and `memory_bytes` is `enemy_fight.swing_wall_msec` (`Time.get_ticks_msec()` around the swing loop): headless 26021→49366 (rerun 46760), rendered 24342→46577 (rerun 51955). Loot `fps`: headless 103→64, rendered 4→2. `enemy_hp_before_total` / `enemy_hp_after_total`, drops, actors and fight awards are identical to wave 38. So `codex/play-spells-numeric`'s cast-debit numerics did not change this fight. The mini ran at load ~90–100 during the journeys.

## Remaining boundaries

All three requested no-ff merges are committed in order, and every audited tip is an ancestor. No source, test, baseline, fixture or tool was changed beyond those merges and the conflict resolution above. This gate session made no code change. The only process difference from wave 38 is environmental: a complete `--import` was needed because the tree started without `.godot`. Unknown arrival commands, the hub2 dialog and loader body, continuous Mission 1 execution, live arena/PvP, an unwritten saved level-state area (`session_level_flow.cpp`, exits-travel) and successful death reload stay refused. No values were invented. No push, PR, private-disc write, edit under `~/champions-re`, or process-name termination; only PIDs this session started were stopped. The mini's w39 `build/` was deleted after verification.
