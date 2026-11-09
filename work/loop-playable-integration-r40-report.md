STATUS: IN PROGRESS — wave 40 (2026-11-09). Merges committed; gates running on the mini.

# Playable integration wave 40 — 2026-11-09

## Scope, evidence and merges

Assigned clean `codex/playable-integration` at wave-39-verified `f9176248`. Followed the wave 39 method (`work/loop-playable-integration-r39-report.md`) and `~/Projects/champions-re-wt/cursor-runs/mini-workflow.md`. Merged the three commits in the wave 40 section of `cursor-runs/audits/wave37-merge-list.txt`, in order, as full no-ff merges on the MacBook (git only), using the exact SHAs. No omissions or aborts. `codex/play-quest-log` and `codex/play-vendors-r2` fork from wave 37 `70d2de19`; `codex/play-story-route` forks from wave 38 `16b66176`. Each branch tip equals its listed SHA.

| Branch | Audited tip | PASS audit | Merge commit | Conflicts |
| --- | --- | --- | --- | --- |
| `codex/play-quest-log` | `124cf0ca` | `play-quest-log-r1` | `486852af` | none; ort auto-merge (`docs/QUESTS.md`, `native/include/champions/quests.hpp`, `native/src/quests.cpp`) |
| `codex/play-vendors-r2` | `18e90ff4` | `play-vendors-r2-fix` | `086d0a0c` | none; ort auto-merge (`native/CMakeLists.txt`, `script_commands.hpp`, `simulation_targets.cmake`, `script_builtins.cpp`, `script_commands.cpp`, `script_items.cpp`, `session_level_flow.cpp`, `level_flow_tests.cpp`, `script_commands_tests.cpp`) |
| `codex/play-story-route` | `20fa62f6` | `play-story-route-r1-fix` | `39a62abb` | none; ort auto-merge (`quests.hpp`, `script_builtins.hpp`, `script_commands.hpp`, `script_commands_targets.cmake`, `quests.cpp`, `script_builtins.cpp`, `script_commands.cpp`) |

All three audited tips are ancestors of merge HEAD `39a62abb`.

## Conflict resolutions

No textual conflicts, so there was nothing to resolve by hand. Every line in the merge result comes from one side or the other. Because ort combined edits from several branches in the same files, each API that one branch changes was checked against the callers on the other branches and on waves 38 and 39:

- **Quest-log `Campaign::quest_clear_all` / `clear_for_act_change` split** (`native/src/quests.cpp:466-480`). `clear_for_act_change` now calls `quest_clear_all` and then does the same flag-39 and quest-variable clears as before. The only caller is still `quests_flow.cpp:10`. Story-route's `quests.cpp` change is in `menu_tick` (`:314-315`) and `Campaign::tick_menu` (`:711`). Neither touches the act clear. `Variables::quest_clear_all` (`script_builtins.cpp:629`, used by `ps2_save.cpp:809`) is a different, older function; quest-log did not change it.
- **Story-route `MenuWorld::row0_open`** (`quests.hpp:120`) defaults to false. Only `Campaign::tick_menu` sets it. Quest-log and vendors do not build `MenuWorld`.
- **Vendors `Variables::on_script_quest_store`** (`script_builtins.hpp:349`) is called from `setv`/`addv` (`script_builtins.cpp:1095-1103`) and `vars_set` (`script_items.cpp:476-477`). Only `Host::set_vendor_stock` (`script_commands.cpp:1408-1438`) installs it. No production code calls `set_vendor_stock`. The live client binds no `VendorStock`, as the vendors report says (`work/loop-play-vendors-r2-report.md:58`). So the hook is empty in every scene and journey. Give-item's `vars_set` neighbour, the `script_runByName` `givePlayerItem` refusal (`script_items.cpp:479-496`), is a separate branch of the same function and is unchanged.
- **Story-route `Variables::before_entry`** (`script_builtins.hpp:211`, called at `script_builtins.cpp:2158`) is a separate field from vendors' `on_script_quest_store`. `Host::install` sets it to `gate_entry` (`script_commands.cpp:358`). `gate_entry` does nothing without an `EntryGate`. Only `session_controller.cpp:1076` sets a gate, from `story_route::gate_for(level)`, which returns a gate only for the missions 3-5 story levels.
- **Story-route `Host::enter` gate** runs before the mission 1 entry and the normal run. Give-item's `CommandScope` and wave 38's `setMissionAvailable` guard are in `Host::command_map()` dispatch. That is a different function, and the order there is unchanged from wave 39.
- **Story-route `Counts::entry_refusals`** (`script_commands.hpp:87`) adds a field. `Counts` is not built field by field anywhere. `vendor_stock.cpp:607` assigns its own `counts_` type.
- **Vendors `SessionHooks::shop_stock`** (`session_level_flow.hpp:24`) is optional and is called after the existing `shop_stock` event (`session_level_flow.cpp:164-169`). Exits-travel's (wave 39) unwritten-state refusal in the same function is earlier (`:118-122`) and unchanged. `level_play.cpp:137` lists only the event name.
- **Exits-travel `level_flow::LevelStates::store` → `StoreResult`, `ChangeResult::saved_bytes`** (wave 39). None of the three incoming branches calls `LevelStates::store`. Vendors' `Host::change_level` reads only `result.reset_act` (`script_commands.cpp:1107-1117`). Vendors' `level_flow_tests.cpp` hunks add `storeLevel` checks to the act-change cases and a `shop_stock` hook to the integration case. They merge cleanly with exits-travel's `level-flow-exact` additions, and the test runs in CTest below.
- **Vendors `loading_readiness` stock branch** replaces the literal address list with `items::kShopDestinationAddresses` (same four addresses). With no `VendorStock` bound, it still reports `Applicable transition stock generation/RNG unavailable.`, as in wave 39.
- **Story-route `session_controller.cpp`** changes (`enable_level_script`, `script_level_request`, `script_mission_menu_state`, `script_mission_menu_tick`) and quest-log's `session_controller.hpp` / `session_controller_text.cpp` (`quest_journal_calls`) are in different functions and files. Story-route's `real_data_play.gd` change only applies to missions 3-12.
- **Build wiring.** Vendors adds `vendor_stock.cpp` to `champions-items` (`simulation_targets.cmake`). Story-route adds `story_route.cpp` to `champions-script-commands` and the probe/test targets (`script_commands_targets.cmake`). Quest-log adds `quest_log.cpp` and its test (`quests_targets.cmake`). The edits are disjoint.

## Privacy check (PSN-9)

No tracked file at merge HEAD contains the mini's absolute home path. None of the 35 files the three merges changed contains a macOS absolute home path (the `/` `Users` `/` prefix; written split here so this report adds no match). The set of tracked files containing that prefix is the same as wave 39's `f9176248`: 25 files, all older MacBook-side paths (3 in `docs/`, 22 earlier `work/loop-*-report.md`). The three incoming reports name the mini layout only in the workflow's allowed `~/malik-champions/...` form (`work/loop-play-story-route-r1-report.md:182,190,300`, `work/loop-play-vendors-r2-report.md:67,70`).

## Non-merge code changes

None. Source changes are the incoming audited work only.

## Gates

Pending.
