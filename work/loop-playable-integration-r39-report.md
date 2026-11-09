STATUS: IN PROGRESS — WAVE 39 (2026-11-09). Merges committed; gates running on the Mac mini.

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

`codex/play-spells-numeric` commit `6db68a82` put `~/malik-champions/open-champions/worktrees/play-spells-numeric/build/native` into `work/play-spells-numeric/ctest.out`; `cfb807da` replaced it with `<mini worktree>`. At merge HEAD, `work/play-spells-numeric/ctest.out` line 1 reads `Test project <mini worktree>/build/native`, and no file under `work/play-spells-numeric/` contains `malik-champions`. No tracked file at the tip contains `/Users/case`. None of the files the three merges changed contains `/Users/`. The set of tracked files containing `/Users/` is identical to wave 38's `16b66176`: older MacBook-side paths (`/Users/syndicatemike/...`) in `docs/INTEGRATION.md`, `docs/WARBOAR_AI_REPORT.md`, `docs/WARBOAR_LIFETIME_FIX_REPORT.md` and `work/loop-mission-menu-r1-report.md`, which predate this wave and which this wave did not touch. Two incoming reports name the mini layout in the workflow's allowed `~/malik-champions/...` form (`work/loop-play-exits-travel-r1-report.md:12`, `work/loop-play-give-item-r1-report.md:5`).

## Non-merge code changes

None. Source changes are the incoming audited work and the resolution above.

## Gates

Evidence under ignored `work/playable-wave39/` (copied back from the mini; the mini home path is rewritten to `~` in copies). Native configure/build and the extension builds ran on the mini from the rsynced tree: rsync excluded `.git`, `build`, `work`, `.godot`, `godot/bin` and `godot/creature_anim/bin`. The w39 tree was seeded from the mini's `play-exits-travel` tree, then delta-rsynced with `--delete` from this worktree at `856bcb68`. A checksum (`-c`) dry run then reported no differences. Godot is the mini's 4.5.2 runtime. godot-cpp is `~/malik-champions/runtime/godot-cpp` at the pinned `e83fd0904c13356ed1d4c3d09f8bb9132bdc6b77`. Commands: `work/playable-wave39/gates.sh`. Parallelism is `-j 2` throughout, as instructed, because the mini is under heavy load (load average ~90–100).

Disk: `df -h ~` showed 39 GB free at start, 24 GB before the native build and 19 GB before the extension builds. Other jobs on the mini drove it down to 6 GB for a few minutes during the extension build; it then recovered to 30–48 GB. This wave's footprint is about 3.5 GB: tree 1 GB, `build/native` 2.2 GB, extension builds about 0.5 GB.

- Native Release configure/build **PASS**, exit 0, 1,145 s wall at `-j 2` (wave 38 took 121 s at `-j 4` on a less loaded mini). New binaries `champions-give-player-item-tests` and `champions-spell-numerics-tests` are present. The only linker warnings are the same 10 `ignoring duplicate libraries` notes as wave 38, identical by count and text. Evidence `configure.log`, `build.log`.
- GDExtensions **PASS**, exit 0, 2,024 s wall, parallelism 2, `arm64;x86_64`. Archive and level-texture `template_debug` / `template_release` each configured and linked. Creature-animation `template_debug` linked to `godot/creature_anim/bin`. `lipo -archs` reports `x86_64 arm64` on all five libraries (`ext-lipo.txt`). Compiler warnings are the same two as wave 38, four times each: `ui_hud_round3_service.cpp:53` unused `submission` and `level_texture.cpp:388` missing `stored_normals` initializer. Evidence `template_*-*.log`, `texture-template_*-*.log`, `creature-template_debug-*.log`.

- Full CTest `-j 2`: **PASS, 262/262, 2,611.42 s**, exit 0 (`100% tests passed, 0 tests failed out of 262`). Evidence `ctest.out`. Wave 38 was 260/260. The two added tests come from the merges: `give-player-item` (Passed, 0.26 s; `codex/play-give-item`) and `spell-numerics` (Passed, 0.28 s; `codex/play-spells-numeric`). `codex/play-exits-travel` extended `native/tests/level_flow_tests.cpp` in place (`level-flow-exact` Passed, 0.31 s). `mission2-route` Passed, 0.17 s, after the `level_flow` API change. Standard input was closed. No test, fixture or source was changed. The longer wall time comes from `-j 2` on a mini at load ~100: `netplay` alone took 258.28 s (wave 38: 183.68 s) and `player-exact` 73.05 s.

- Headless editor import. The first import (`--headless --path godot --editor --quit-after 120 --frame-delay 50`, exit 0, 18 s, `editor-import.log`) quit before the first filesystem scan finished (`WARNING: Scan thread aborted`). That left `godot/.godot/imported` empty. Wave 38's tree had a populated `.godot`. This wave's rsync excludes `.godot` as instructed, so the tree started without one. The first matrix run therefore failed 25 of 54 cases, all on the same missing import (`res://.godot/imported/open_champions_avatar.png-….ctex` / `Failed loading resource: res://assets/open_champions_avatar.png`). It is kept as `matrix-noimport/`, `matrix-noimport.out` (29/54, exit 1) and is not this gate. A complete import (`--headless --path godot --import`, which waits for all imports and then quits; exit 0, 18 s, `editor-import-full.log`, 0 error lines) produced the project's only imported asset: `.ctex` + `.md5`, from the single `*.import` file in the project. No source, test, baseline or tool changed. The full matrix was then rerun from scratch.
- Strict Godot dev matrix: **54/54 PASS**, exit 0, 282 s wall, `tools/godot_matrix.py matrix --captures --timeout 1200`, standard input closed, isolated projects and user directories. Evidence `matrix.out`, `matrix/results.json`. The case list matches wave 38's 54 cases in the same order. The unchanged `check-log` found 0 diagnostic errors across all 105 matrix logs (`check-log.out`). Nothing was filtered or allowlisted.
- Full synthetic combat **2,645 checks, 0 failures** (`matrix/playable_wave_smoke_full_combat.log`, `PLAYABLE_PLAY_RESULT`), same as wave 38. `codex/play-spells-numeric` changed live cast-debit numerics in `player.cpp` / `skills_b.cpp`. Its own audited gates also kept combat at 2,645/0 and the trace unchanged (`work/play-spells-numeric/full-combat.txt`, `full-trace.txt`).
- FULL_SAVE_LOAD **PASS, 300/300**. Continued and uninterrupted presentation SHA-256 are both `cb2309ab23d3e2906b5b9d4e95e5577c354869fce3d86cce9fe592a0d2c8d573`, the same as wave 38.
- FULL_TRACE **PASS, unchanged**, 36,402 bytes, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`, headless renderer. No baseline was edited.
