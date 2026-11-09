STATUS: MORE — CAMPAIGN INVENTORY R2 FIX VERIFIED (2026-11-09). Every finding of audit `play-campaign-inventory-r1.md` (VERDICT FIX: PCI-1 HIGH, PCI-2..5 MEDIUM, PCI-6..11 LOW) is fixed. Release build and the three GDExtensions built. Focused tests pass, and ten one-at-a-time mutations of the new rules are each caught by a test. Native CTest 260/260 from a clean native build. Strict Godot matrix 54/54. Full combat 2,645 checks, 0 failures. FULL_SAVE_LOAD 300/300, identical. FULL_TRACE unchanged at 36,402 bytes, `9c7922b2…c659`. Journeys 80 headless / 81 rendered, pickup 10→11, every step/result pair the same as wave 37. Every journey difference against wave 37 is explained below.

# Play campaign inventory — round 2 (audit fix) — 2026-11-09

Branch `codex/play-campaign-inventory`, worktree `~/Projects/open-champions/worktrees/play-campaign-inventory`. Round 1 ended at `b563d486`, from wave 37 `70d2de19`. Edits and commits are on the MacBook. Builds, tests, Godot and journeys ran on the Mac mini, in `~/malik-champions/open-champions/worktrees/play-campaign-inventory/`, with `cmake --build -j 4` and `ctest -j 3`. Research used: `research-inventory-reset` `ce2e086` (`docs/inventory-reset.md`, `docs/audio-save.md`, `docs/quests.md`) and `research-story-route-r3` `019a155` (`docs/story-route.md`). The disc at `../../private-validation/rta-disc` was used read-only and only counted (994 files before and after each journey). I did not push or open a PR, touched nothing under `~/champions-re` or `playable-integration`, and killed no process by name. Mini home paths in the logs I brought back are rewritten to `~/`.

## Fixes

### PCI-1 (HIGH): the bag copy goes through the session's guard

- **What was wrong.** Round 1 copied the previous host's record spans into the new host on every act transfer, ignoring the session's guard. After a character was created, or a companion loaded or cleared, the old cells could become the new character's inventory.
- **The fix.** `copy_live_bags` is gone. The session's own guarded copy is now the only carry: `carry_live_bags` runs under `BagCarryGuard` (`native/include/champions/campaign_entry.hpp`). The guard is the session's condition, unchanged: a player record is held, no starting items are pending, and `inventory_record_revision_ == player_record_revision_`. `session_controller.cpp` builds the guard from its own members (`bag_carry_guard()`, `act_carry()`).
- **When the guard fails.** `require_campaign_act_transfer` refuses any live cell with wave 37's exact text: "Act transfer with inventory is refused: live bag carry and flag-39 migration are unported." The three session preflights (the level request and the two mission-menu checks) pass the same guard.
- **When the guard holds.** The cells are classified in the destination's memory after the copy, or in the source's memory for a preflight.
- **Tests.** `test_bag_carry_guard` (`native/tests/campaign_inventory_tests.cpp`) covers character creation (starting items pending, revision bumped), a companion load (revision bumped), and a cleared record. In each case the act transfer refuses with wave 37's text, the destination gets no old cells (bag counts 0, no equipment pointer), its act and 0x4f7118 are unchanged, and the preflight refuses too. A same-act entry under each failed guard also carries no cells. An empty inventory still crosses without the guard, as in wave 37.

### PCI-2: no test-only duplicates; tests use the live path

`apply_campaign_travel`, `documented_story_acts`, `Request` and `Result` are deleted from `campaign_inventory.{hpp,cpp}`. In the client, acts come from `quests::read_act_tables(*executable_)`, and level flow's `begin_change` decides an act change. The documented act table now lives only in the test, as a fixture named `story_tables()` (cited `019a155 docs/story-route.md:242-255, :271-279`).

The cases below go through the code the client runs:

- **Through `prepare_campaign_entry` → `Host::change_level` → `clear_act_change_inventory`** (`test_live_travel`). Each case builds the destination from the source's campaign image and runs `carry_live_bags` under the guard, as the session does:
  - same act (`strife`);
  - `hub2` from act 0;
  - the mission-2 chain (`inmine2` in act 1);
  - an act change (`innovat1`), with the full removal, slide, weapon, stamp, keep-list and progress checks;
  - the card latch from the real front end;
  - a running death task (`prepare_campaign_entry(..., death_task=true)`);
  - the frontend word;
  - no latch reading.
- **Through `Host::change_level` directly** (`test_change_level_from_unbound`): from act −1. A first level with no previous host does not reach `change_level` in the client (the `else` branch of `prepare_campaign_entry` sets the act, unchanged from wave 37), so −1 is tested on the function itself. That test also covers two joined players and a record at or above `g_numPlayers` that is not walked.

### PCI-3: the latch is the front end's real latch

- **The fix.** The client's latch is `Flow::save_latch_` (`frontend.hpp`). `ui/frontend_menus.gd` forwards it to `ChampionsSessionController::set_save_latch` after every flow change (`forward_save_latch()`, called from `_refresh()`). `real_data_play.gd` forwards it again just before `enable_level_script`.
- **Into the entry.** The session passes the value in `ActCarry::card_latch`. `prepare_campaign_entry` mirrors it into the destination's 0x4f8f28 before `Host::change_level`, which reads it there (the comment is updated).
- **Default and refusal.** The session starts at 0, the value `Flow` is constructed with. Only the front end's card screen stores the latch, so it is also the value before any front end has booted. A latch other than 0 or 1 (−1 means no reading) refuses the act transfer: "the front end's card latch 0x4f8f28 has no reading, so whether the act-change clear runs is unknown."
- **Native test.** `latch_after_card_error()` drives a real `champions::frontend::Flow`: a slot is chosen (latch 1), the save fails, and circle returns to play with the latch still 1. The test passes that `save_latch()` into the live entry. The act is stored (act 1, 0x88d3d4 = 1), the clear is skipped, nothing is removed, 0x4f7118 stays 0, the bags and journal stay, and 0x4f8f28 is 1.
- **Godot test.** `godot/tests/frontend_smoke.gd` now also checks that the session accepted the front end's latch: 1 at the "saved" box, 0 after it closes (`FRONTEND_SMOKE checks=150 failures=0`).
- **The journey hits this case for real.** Its front-end save gets a card error, so the latch stays 1. The act transfers it makes later store the act and skip the clear (see the journey section).

### PCI-4: an unreadable name refuses; nothing is removed

- **The rule.** `campaign_inventory::unclassified_cell(memory, players)` returns a reason when a count is outside 0..64 or a cell's definition word is 0. For a flag-39 cell it also returns a reason when the definition's name pointer is 0, or the name reads as empty, in that memory.
- **Where it runs.** `require_campaign_act_transfer` runs it on the destination's memory after the copy, so it refuses before `change_level`. `clear_act_change_inventory` runs it again over every slot before its first edit, so `Host::change_level` cannot remove part of a bag. `remove_unkept` no longer reads a name it has not checked.
- **Tests** (`test_unreadable_names`), all without hand-copied definitions in the destination:
  - The act transfer refuses ("…has no name pointer in this memory…"). All cells, the act and the journal stay, and 0x4f7118 stays 0.
  - An empty name refuses instead of being removed.
  - Cells without flag 39 cross without readable names (no keep decision is needed).
  - `Host::change_level` from −1 with no readable names refuses "before any edit" and removes nothing.
- **In the live client.** The host's memory has no executable-backed item table on this path (round 1's audit: the only `memory.under` binding is `campaign_fight_probe.cpp`). So any carried flag-39 cell now refuses the act transfer instead of being stripped. The journey's carried cells have no flag 39 (none was refused for this reason).

### PCI-5: the save-load share is tested through `apply_load`

`share_on_load()` (`native/tests/ps2_save_tests.cpp`) loads two-player slot images through `apply_load` with a minimal bound inventory: remove slides the bag, and give appends the stored cell to the bag its category byte names. The cases:

1. Share word 1, and Aria's bag 2 holds a flag-39 cell. Bram's own flag-39 bag-2 cell is removed, his plain bag-2 cell stays first, and he receives the copy with stack count 3. Aria is unchanged, Bram's equipment is not the copy, and exactly one remove and one give are called.
2. Saved row 0 is copied (PCI-9).
3. Flag bit 8 refuses with `Unknown` before any write: no service call, and memory is byte-identical.
4. A negative saved row refuses with its own message, before any write.
5. With the share word 0, nothing is shared.

The mutation check confirms the tests bite: deleting the share call fails `line 1070`, and deleting the bit-8 refusal fails too.

### PCI-6: cite

`campaign_inventory.cpp` no longer holds the table. The fixture cites `019a155 docs/story-route.md:242-255` and `:271-279`, both checked at the pin: the table is at 242-255, and the "Between two story levels" paragraph, including line 272's act-0 names, is at 271-279.

### PCI-7: the "not ported" claim is withdrawn

Round 1 said `save_clearBuffer(0)` and `jumpgate_resetDestinations` have no client body. That was wrong. On `reset_act`, `level_flow::begin_change` runs `s.states = {}`, `s.save_buffer.clear()` and `s.gates.reset(o.hub)` (`native/src/level_flow.cpp:1012-1016`), and `Host::change_level` reaches it (`inventory-reset.md:68-72`).

### PCI-8: the share runs at research step 7

The `inventory_shareQuestItems(0)` call in `load_impl` (`native/src/ps2_save.cpp`) now runs after `vars_clearAll`/`vars_set`, the loaded flag, act, difficulty, play time, the pending name and marker, and the loaded name. It runs before the autosave countdown, matching `audio-save.md:129-138` steps 6–8. The stale comment "9. The share word is zero (checked above)" is replaced. Test case 1 checks the pending name and the countdown after the share.

### PCI-9: row 0 is not refused

`share_copy_unexecuted` now returns a reason string. A saved cell holds a definition row (`encode_saved_record`), so it refuses only a negative row, with the message "a saved bag-2 flag-39 cell has a negative definition row, so it has no definition to copy". Flag bit 8 keeps its own message. Row 0 is copied (test case 2).

### PCI-10: tests for the listed refusals

`test_listed_refusals` covers:

- a second local player: "Act transfer supports one local player.", destination untouched;
- a campaign-model bag, which now has its own message: "Act transfer with campaign-model bags is refused: they have no equipment pointers to slide.";
- a live cell with no definition.

In `mission2_host_tests.hpp` the old "nonempty live inventory refuses" case is relabelled to what it checks: "a carried live count whose cell has no definition refuses…". A second case is added: "a live count without the session's bag-carry guard refuses…".

### PCI-11: the journey records the clear, and newly reachable levels refuse unsupported commands

- **Recorded per entry.** `CampaignEntry` and the `enable_level_script` result now report:
  - `campaign_carried_cells`;
  - `campaign_act_removed` (`Host::act_change_removed()`, set by `change_level`);
  - `campaign_act_mark` (the destination's 0x4f7118);
  - `campaign_items_crossed_act`;
  - `save_latch`;
  - `requires_complete_commands`.

  `walk_exit_probe` (`godot/tests/loop_playthrough.gd`) writes one `source_loads` row per candidate load. Each row has those fields plus `script_stopped`, `last_refusal`, and the refusal text when the load fails.
- **Newly reachable levels refuse instead of running unaudited.** Wave 37 refused every act transfer that carried live cells, so no script on that route had ever run with them. The destination of such a transfer, and every later carried entry from it (`Host::items_crossed_act`), now runs with `require_complete_commands(true)`. The script stops at its first unsupported command with the command's name, instead of running past it. An entry without a previous host leaves that chain. Tests: the chain starts on a carrying act transfer, a later carried same-act entry stays on it, and a fresh entry leaves it.
- **The +38% stub rows, `model_getAttachPoint`, and the missing WarBoar path rows.** These are the journey questions; they are answered in the journey section below.

## Gates

Evidence is under `work/campaign-inventory/r2/`, committed with `git add -f`.

| Gate | Result | Commit |
|---|---|---|
| Release build and GDExtensions (`build.out`) | native Release: 0 errors. `mac-template_debug` (archive), `level-texture-template_debug` and `creature-template_debug`: exit 0, arm64. The only warnings are older ones (`ui_hud_round3_service.cpp` unused function, duplicate-library linker notes) | `8dbb15aa` |
| Focused tests (`focused.out`) | 7/7: campaign-inventory, ps2-save (459 checks), quests, frontend-exact, script-commands (with `mission2_host_tests.hpp`), level-flow-exact, mission2-actors. Ten mutations, each applied alone and then restored, were all caught: share call, bit-8 refusal, row-0 rule, guard, latch mirror, name pointer, empty name, death task, act-change clear, items-crossed chain | `d07e0765` |
| Native CTest (`work/ctest.out`) | **260/260 PASS** (`ctest -j 3`, clean `build/native`), including campaign-inventory, ps2-save, script-commands, quests, pickups | `323d9748` |
| Strict Godot matrix (`matrix.out`, `matrix/results.json`) | **54/54 PASS**, exit 0 (`tools/godot_matrix.py matrix --captures --timeout 1200`, Godot 4.5.2.stable.official.6ce3de25a) | `b94e23dd` |
| Full combat (`matrix/playable_wave_smoke_full_combat.log`) | `PLAYABLE_PLAY_RESULT` checks **2,645**, failures **0** | same |
| FULL_SAVE_LOAD | **300/300**. Continued and uninterrupted presentation are both `cb2309ab23d3e2906b5b9d4e95e5577c354869fce3d86cce9fe592a0d2c8d573` | same |
| FULL_TRACE | **36,402 bytes, `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`**, headless, unchanged | same |
| Journeys (`journey/`, rerun in `journey-rerun/`) | **80 headless / 81 rendered**, wrapper exit 1 (the retained `player_death` refusal, as in wave 37). Pickup **10→11** in both, produced, walked and picked up. 0 log errors. 8 refusal kinds. Disc 994/994 | `bdc0b6aa` |

The matrix and journeys ran on the code at `8dbb15aa`; later commits add only evidence and this report. The CTest run that failed four tests on stale objects is kept as `r2/ctest-stale-objects.out` (see the build note).

## Journeys against wave 37: every difference

Baseline: `work/campaign-inventory/base-70d2de19/`, the wave-37 build that round 1 rebuilt on the same mini. Every step name and result is the same as wave 37 at every index, in both modes. Below, "r1" is round 1's journey (`work/campaign-inventory/journey/`). The differences are listed in step order.

**Steps 18 and 27 (host timing, not this change).**

- **What differs.** `dialog_choice_close.choices_taken` and `hub_trigger_mission_menu` `addQuest`/`setMissionAvailable`. Headless: 4 / 2 / 3 against wave 37's 5 / 3 / 4. Rendered: 3 / 1 / 2 against wave 37's 5 / 3 / 4.
- **Proof it is timing.** I ran the same build twice. The rerun gives headless 5 / 3 / 4 (wave 37's values) and rendered 4 / 2 / 3. Apart from fps, memory and wall time, every other field of the two runs is identical (`journey-rerun/`).
- **Why the count moves.** The dialog loop (`loop_playthrough.gd:808-821`) presses Enter every 8 displayed frames until the box closes. How many choices land before it closes depends on display frames, and the dialog fps differed: 111 and 130 headless, 42 and 85 rendered, against wave 37's 145/163. The mini's load average was 80–130 during these runs. Round 1 already recorded the rendered counts flipping between runs.

**Step 57, `walk_in_exit_trigger`.**

- **Differences from wave 37.** `sources_examined` 2→4, `walk_attempts` 3→7, `exit_targets` 20→9, `level_index_after` 22→48, `entered_without_change` 0→1, `navigation_stalls` 3→6, `trigger_samples` 162→56. These are the same values as r1, and their cause is round 1's: an act transfer carrying classified cells is now accepted, so the probe's other-act sources load.
- **The new `source_loads` rows** show the path:

  | Index | Entry | Carried cells | Latch | Clear | Removed | 0x4f7118 | Script |
  |---|---|---|---|---|---|---|---|
  | 19 | same act from 36 (the `else` branch, unchanged from wave 37) | 0 | 1 | no | 0 | 0 | stopped without a refusal (`last_refusal` empty; complete-commands off) |
  | 22 | act change from 19 | 7 | 1 | **skipped by the latch** | 0 | 0 | complete-commands on; stopped at `refused preloadResource` |
  | 48 | act change from 22 | 7 | 1 | **skipped by the latch** | 0 | 0 | complete-commands on; stopped at `refused deleteMonster` |
  | 92, 96, 0, … | not loaded | | | | | | "this level has Creature death state that cannot be restored yet" (48's departing save) |

- **The latch.** The journey's front-end save step gets a card error (refusal `frontend: scene Save refused: Saving refused: <path> reload is not implemented.`). As in the original, that leaves the latch at 1, so both act changes store the act and skip the clear: removed 0, 0x4f7118 0. Round 1 read a 0x4f8f28 byte that nothing writes, so there the clear ran. That is PCI-3, now visible on real data.
- **No flag-39 refusals.** Neither transfer was refused for an unreadable name. Host memory has no definition table on this path, so none of the 7 carried cells is a flag-39 cell.
- **`quest_vars` 14 (wave 37) / 24 (r1) → 19.** This count is read on index 48. Its script now stops at `deleteMonster`, so fewer quest variables are created than in r1, where it ran on.
- **Two new refusal kinds:** `tick: scene Script after an act change with items stopped: refused preloadResource` and `… refused deleteMonster`. These are the precise reasons PCI-11 asked for, from the stops above. They are why the refusal kinds go from 6 to 8. `walk_in_exit_trigger`'s refusal is still the death-state reason, as in r1. The wave-37 reason "per-level object directory is full or unreadable" is gone, as in r1: at wave 37, index 22 was entered and its script refused at the act transfer, so the departing save of 22 had no readable directory.

**Step 60, `fight_level_departure`.** The reason is the death-state refusal from index 48, the same as r1.

**Steps 64/65 (headless; 65/66 rendered), the fight on index 99.**

- **Same values as r1.** These fields are identical to r1:
  - `walk_ticks` 52→26;
  - ticks 5,942→5,916;
  - boar state changes 495→553, attack entries 231→257, hit events 201→221;
  - prompts 501→477;
  - HP totals;
  - player end (562.2, 2842.4, 0)→(816.9, 4072.0, 10);
  - the drop position;
  - target handle 6,462,656→6,465,216.

  The fight level is loaded with `carry_campaign=false` (`loop_playthrough.gd:1460-1465`), so it is outside the new script mode. Its own load-time counts (`enemy_level`, `combat_moves`) are identical to wave 37. What differs is where the player starts, because the level the probe last entered differs (index 48 instead of 22).
- **`unconstructed host stub left out of a collision/target box`, 120,634 → 166,101.** This is not a count of stub objects. `World::State::box` (`native/src/combat_runtime_world.cpp:601-621`) adds 1 for every stub node it meets in the grid cells that a box query covers, each time a query runs. So the count is queries × stubs near the queried boxes. The player starts at (816.9, 4072.0) instead of (562.2, 2842.4), and the boars fight there (attack entries +26, hit events +20). Their arc and target queries run over a part of the grid that holds more stub nodes. Ticks fell by 26, but the stubs per query rose. The set of stubs constructed on the level (a load-time property) is unchanged.
- **`model_getAttachPoint`, 11,884 → 11,832.** The refusal is counted on each attach-point call (`native/src/campaign_fight.cpp:333-337`). The run is 26 ticks shorter and has 24 fewer prompts, so there are 52 fewer calls. Like the stub count, it follows the different fight, not a code change: `campaign_fight.cpp` is not in this branch's diff.
- **The WarBoar `path_requestToPoint` rows disappear** (wave 37: 72 wander, 1 chase; r1 and r2: none).
  - The fight's creatures run `arena_monsters::Model` (`combat_runtime_internal.hpp:16,100`). It asks the path service only in `Model::request_path` (`native/src/arena_monsters.cpp:1513-1516`), and only from `advance_waypoint` once the 16-entry waypoint list is used up (`:1517-1525`). That happens in two places. In `steer_without_path` (`:1577-1580`), every 64th frame per creature, the request is made only when `probe_rays` finds the straight line to the destination blocked. In `steer_along_path` (`:1591-1603`), it is made while a creature follows a path. Both wander (`move_to_dest`, toward a random home point) and chase (`move_to_target`) move through them.
  - Starting at the other entrance, the player is next to the boars from the start: the attack path is taken 26 more times and the boar state machine changes 58 more times. In this run no boar met a blocked straight line while wandering or chasing, so the request, and its refusal, never came.
  - The refusal path is unchanged, and the level still has no path service. Wander and chase refusals did not stop existing; they were not reached.
  - Outgoing player-damage refusals rose 43→87 because there was more contact.
- **Round 1's A/B covers all of this.** The A/B (`work/campaign-inventory/base-70d2de19/`) showed these fight differences follow from the probe ending on another level. Round 2's fight fields are byte-identical to r1.

**Host-only fields.** `fps`, `memory_bytes` and `swing_wall_msec` differ between runs.

## What is still refused

- The successful death reload (`player_death`, `saved_level_reloaded` false), as in wave 37.
- An act transfer with live cells when the session's bag-carry guard fails.
- An act transfer with a campaign-model bag, an imported record, a live cell with no definition, a flag-39 cell whose name cannot be read in the destination's memory, more than one local player, or no latch reading.
- Scripts on levels reached by carrying cells across an act stop at their first unsupported command. On the journey that is `preloadResource` on index 22 and `deleteMonster` on index 48.
- The `save_load` share with no inventory service bound, with flag bit 8 on a copied cell, or with a negative saved definition row.
- `rendered_pickup_verified` stays false.

## Unknowns, conservative and disclosed

- **Keep list.** These are only the executed names (`docs/quests.md:163-169`); none was added. Which retail definitions carry flag 39 is still not asserted.
- **No definition table in host memory.** In the live client, host memory has no executable-backed item-definition table. So a carried flag-39 cell refuses the act transfer instead of being kept or dropped. This holds until the destination can read definition names.
- **A refusal mid-`change_level`.** A refusal from `clear_act_change_inventory` inside `Host::change_level` happens after level flow's own act state has moved. In the client that host is a discarded candidate, because `enable_level_script` fails and keeps the old host. `prepare_campaign_entry` checks the same rule before `change_level`, so on the client path it refuses first.
- **The items-crossed chain lives in hosts.** `Host::items_crossed_act` is kept from host to host, not in a save. A session loaded from a save file starts without it.

## Build note

The first full CTest run on this round failed `pickups` (abort), `messages-exact`, `player-exact` and `monsters-1-exact`. The cause was stale objects, not this change:

1. The second build stopped at a test compile error before recompiling some objects that include `script_commands.hpp`.
2. A later `rsync -a` put back the MacBook's older mtime on that header, so make treated the old objects as current.

I deleted `build/native`, rebuilt it from scratch, and reran the whole suite; that clean run is the gate above. Later syncs touched every changed file.

## Cleanup

The mini's `build/` for this worktree is deleted, along with the scratch base copy. `godot/.godot` and the extension binaries under `godot/bin` and `godot/creature_anim/bin` are kept. No tests, baselines or fixtures were weakened. The old null-definition test was relabelled to say what it checks, and a guard case was added beside it.
