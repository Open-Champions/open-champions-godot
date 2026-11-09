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

Pending.
