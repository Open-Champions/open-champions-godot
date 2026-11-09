STATUS: MORE — CAMPAIGN INVENTORY R1 VERIFIED (2026-11-09). Audited carry-over rules are implemented and cited. Native CTest 260/260 (recorded at 763643d6, not re-run). Strict Godot matrix 54/54. FULL_SAVE_LOAD 300/300, identical. FULL_TRACE unchanged at 36,402 bytes, 9c7922b2…c659. Journeys 80 headless / 81 rendered, pickup 10→11, every step/result pair the same as wave 37. Simulation fields from `walk_in_exit_trigger` on differ. An A/B run on the same mini traces every one of those differences to the intended change: act transfer with a classified live inventory is no longer refused.

# Play campaign inventory — round 1 — 2026-11-09

Branch `codex/play-campaign-inventory`, from wave 37 `70d2de19`. A previous agent wrote `52229fa3` (implementation) and `763643d6` (CTest 260/260) and was then cut off by a provider limit. This round re-read both commits and the research, did not redo CTest, and ran the remaining gates on the Mac mini. Edits and commits are on the MacBook. Builds, Godot and journeys ran on the mini, in `~/malik-champions/open-champions/worktrees/play-campaign-inventory/`. The disc at `../../private-validation/rta-disc` was used read-only, counts only (994 files before and after each journey). No push, no PR, nothing touched under `~/champions-re` or `playable-integration`, and no process killed by name. Mini home paths in logs brought back are rewritten to `~/`.

## What the code does, against the research

The research is `~/Projects/champions-re-wt/research-inventory-reset` at `ce2e086` (audit `research-inventory-reset-r2-fix.md` PASS) and `research-story-route-r3` at `019a155`. Line numbers below are at those commits.

| Rule | Research | Code | Test (`native/tests/campaign_inventory_tests.cpp`) |
|---|---|---|---|
| Same-act travel leaves bags, equipment, quests, vars, gold, XP, mission and medal bytes. The act stays and 0x4f7118 stays 0 | `docs/inventory-reset.md:40-57` | `campaign_inventory.cpp:154-161`: `changed` only when `find_act` differs, and no clear otherwise. `script_commands.cpp:1086-1100`: `reset_act` gates the clear | `:154-167` (strife, hub2, inmine2), `expect_untouched` `:124-138` |
| An act change removes unkept flag-39 cells through `inventory_removeItem`. The equipment slot is cleared, the weapon word is cleared and its class set to 5, later pointers follow the slide, and stamps drop by one | `inventory-reset.md:65-85` | `remove_unkept` `campaign_inventory.cpp:47-82` calls the bound `inventory_removeItem` for each slot below `g_numPlayers` (`:131-142`) | `:168-200` |
| Kept names stay: contains `bloodstone` / `shardof`, or is exactly `spiritrunes`, `emptybag`, `fullbag`, `pickaxe`, `twistedkey`. Case-sensitive | `docs/quests.md:162-169`, cited at `inventory-reset.md:75-81` | `quests.cpp:195-200`, used at `campaign_inventory.cpp:68` | `:183` (pickaxe, bloodstonefay kept) |
| Ordinary items (including bit 38), gold, XP, level, mission and medal bytes stay | `inventory-reset.md:35-38, 79-82` | No write to those fields anywhere on the clear path | `:185, :196` |
| Quest-list count goes to 0 and used titles and texts get `NoQuests`. Variable names stay and values go to 0. Records at or above `g_numPlayers` are not walked | `inventory-reset.md:82-84`, `quests.md:286-299` | `clear_quest_lists` `:86-102`; `variables.quest_clear_all()` (`script_builtins.cpp:629-639` zeroes only the value byte) | `:187-193` (slot 2 canary stays) |
| Stores 1 at 0x4f7118 after the clear | `inventory-reset.md:72-73` | `campaign_inventory.cpp:141` (`ACT_MARK`) | `:172` |
| Card latch 0x4f8f28: the act is stored, the clear is skipped, the mark stays 0 | `inventory-reset.md:87-90` | `:161`; latch carried at `campaign_entry.hpp:107-108`, read at `script_commands.cpp:1091` | `:201-205` |
| Death task running: the act is stored and the clear is skipped | `inventory-reset.md:90-94` | `:161`; `session_controller.cpp` passes `fight_->death_view().running` | `:206-210` |
| Frontend word 0x41b510 nonzero: no act call, no clear, the word is not written, the stored act unchanged | `inventory-reset.md:59-63` | `:151-152` returns before `find_act`; `script_commands.cpp:1088`; `campaign_entry.hpp:115` skips the destination-act check | `:211-222` |
| Stored acts same=0, hub2=0, change=1, from −1=0 | `tools/difftest_inventory_reset.py:441-449` (`expect_act`), printed at `:546` | `:154-158` | strife→0 `:155`, hub2→0 `:159`, innovat1→1 `:170`, blood1 from −1→0 and cleared `:224` |
| Acts per level: mission-1 names and both hubs are act 0 (`hub` is a prefix of `hub2`). The innovat1/inmine1/inmine2/innovat2 chain is act 1. Missions 3–12 are one act each, and evil names share their story act | `research-story-route-r3` `019a155` `docs/story-route.md:237-255` (table), `:269-279` (between two story levels) | `documented_story_acts` `campaign_inventory.cpp:111-129` | `:154-170` |
| Quest-item sharing: bag 2 only. Other slots below `g_numPlayers` first lose their own bag-2 flag-39 cells (even with nothing to copy), then each source cell is copied with its stack count, not equipped. The source is unchanged. Bags 0/1 and bit 38 are not walked | `inventory-reset.md:96-108` | `share_quest_items` `:188-246`; called from `ps2_save.cpp:816-817` when 0x4f85b4 is 1 | `test_share` `:232-301` |
| The arena wipe `player_removeAllItems` 0x1bf600 is not the campaign path | `inventory-reset.md:113-137` | Not called. Disclosed in `campaign_inventory.hpp:13-15` | — |

## The unknown keep list, and other unknowns: conservative and disclosed

- **Keep list.** The names are the executed list only (`quests.md:162-169`). No name outside it is kept, and none was added (`docs/QUESTS.md`, `campaign_inventory.hpp:10-15`). Which retail definitions carry flag 39, and so which items a real campaign will strip, is not asserted anywhere. The flag is read from each live cell.
- **Unclassifiable cells refuse.** A flag-39 cell with a null definition refuses before any drop decision (`campaign_inventory.cpp:64-66`). An act transfer refuses before loading when the source has a campaign-model bag (no equipment pointers to slide), an imported record, more than one local player, or any live cell with a zero definition (`campaign_entry.hpp:10-23, 43-62`).
- **Share Unknowns refuse before any write.** Flag bit 8 (stackable) on a copied cell is Unknown in the research (`inventory-reset.md:109-111`). `save_load` refuses before any write when it is set (`ps2_save.cpp:785-787`, `share_copy_unexecuted` `:167-186`), and also when no inventory service is bound (`:783-784`). The give hint in `inventory_giveItem` is announced as 0 (`:238-241`).
- **Not ported on this stretch, outside the bags.** `save_clearBuffer(0)` and `jumpgate_resetDestinations` (`inventory-reset.md:68-72`) have no client body in this path. This round did not change that, and the gates do not exercise it.
- **Cite drift (not a code defect).** The inner comment `story-route.md:242-255 and :273-278` (`campaign_inventory.cpp:112-113`) points at the r3 head's line numbers. At `019a155` the same paragraph is `:271-279`. The header cite `019a155 docs/story-route.md:240-278` is correct. The comment was left unchanged so the gated tree stays exactly what was tested.

## Gates

Evidence is under `work/campaign-inventory/` (committed with `git add -f`: `matrix.out`, `matrix/results.json`, `matrix/playable_wave_smoke_full_combat.log`, the journey `playthrough.out` and `result.json` files, and `base-70d2de19/`).

The mini's existing build was current. The extension dylibs (2026-11-03 19:36-19:37) are newer than the last source edit (19:12). A dry-run sync showed no source difference from `763643d6`. The mini runs Godot 4.5.2.stable.official.6ce3de25a.

| Gate | Result | Commit |
|---|---|---|
| Native CTest | 260/260, recorded by the previous agent (`work/ctest.out`). Not re-run | `763643d6` |
| Strict Godot matrix (`tools/godot_matrix.py matrix --captures --timeout 1200`) | **54/54 PASS**, exit 0. Full combat 2,645 checks, 0 failures. The 5 "fail" substrings in the full-combat log are JSON field names in result lines, not failures | `be3cfa5b` |
| FULL_SAVE_LOAD | **PASS 300/300**. Continued and uninterrupted presentation are both `cb2309ab23d3e2906b5b9d4e95e5577c354869fce3d86cce9fe592a0d2c8d573`; trace `9c7922b2…c659` | `be3cfa5b` (same matrix case) |
| FULL_TRACE | **PASS, unchanged**: 36,402 bytes, `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`, headless | `be3cfa5b` (same matrix case) |
| Journeys (`tools/run_loop_playthrough.py`, after a headless editor import, exit 0) | **Headless 80 steps, rendered 81**, wrapper exit 1 (the retained `player_death` refusal, `saved_level_reloaded` false). Pickup **10→11** in both, produced, walked and picked up. 0 log errors. 6 refusal kinds. 27/28 `refused` rows. 38 rendered captures. Disc 994/994 | `38636190` |

FULL_SAVE_LOAD and FULL_TRACE come from the one full-combat matrix case, so the three gates share one commit.

## Journey differences, all explained

Every step name and result is the same as the wave-37 / death-reload 80/81 baseline at every index. These simulation fields differ:

| Step | Wave 37 (`70d2de19`) | This branch |
|---|---|---|
| `walk_in_exit_trigger` | sources_examined 2, walk_attempts 3, exit_targets 20, level_index_after 22, quest_vars 14/14, entered_without_change 0, navigation_stalls 3, trigger_samples 162 | 4, 7, 9, 48, 24/24, 1, 6, 56 |
| Refusal under that step, and the reason for `fight_level_departure` | `per-level object directory is full or unreadable` | `this level has Creature death state that cannot be restored yet` |
| `loot_pickup` walk_ticks | 52 | 26 |
| `enemy_fight` | ticks 5942, boar state changes 495, attack entries 231, hit events 201, prompts 501, HP total 31804.7→31588.3, player end (562.2, 2842.4, 0) | 5916, 553, 257, 221, 477, 31753.1→31536.6, (816.9, 4072.0, 10). Refusals: no WarBoar path-service rows (72 wander + 1 chase before), outgoing player damage 43→87. Loot drop at a different position |

**A/B proof.** The comparison baseline (death-reload r1) predates wave 37's arena and loading merges. To rule those out, wave 37 was rebuilt on the same mini, in scratch `pci-base`: the current tree with `git diff HEAD 70d2de19` applied, and the archive extension rebuilt arm64-only. The texture and creature extensions do not change between the two commits. The same journey there reproduces the baseline exactly: walk_ticks 52, the same refusal map, and every headless simulation field identical to death-reload r1 (`work/campaign-inventory/base-70d2de19/`). So every difference above comes from this branch.

**Cause.** `walk_exit_probe` (`godot/tests/loop_playthrough.gd:455-558`) loads one candidate source level after another with `load_real_level(index, true)`. `sources_examined` counts only loads that succeed. At `70d2de19`, `require_campaign_act_transfer` threw `Act transfer with inventory is refused` for any nonzero live bag count (`git show 70d2de19:native/include/champions/campaign_entry.hpp:23-26`). The journey's character carries 10 inventory units, so every other-act source failed to load (`real_data_play.gd:289-298`). The probe settled on one same-act source (index 22). There the departing object directory could not be saved, and every later load stopped on `per-level object directory is full or unreadable` (`real_data_play.gd:278-284`). On this branch, live cells that all have definitions are classified, so the act transfer runs (the purpose of `52229fa3`). The other-act sources load with their scripts. The probe examines 4 sources and stops at index 48. Index 48's departing save holds a dead built Creature, so the guard reports the death-state refusal instead. The quest-variable count (14 vs 24) is read on that final level, so it differs too. The probe and the departure step end on a different level, so the fight level is entered from a different previous level. The player starts elsewhere, which moves the walk to the drop (26 ticks against 52), the boar contact counts, the HP totals, the end position and the drop position. Load-time counts on the fight level (`enemy_level`, `combat_moves`) are identical.

No new refusal kind appeared and none disappeared. The refusal map swaps one level-change reason for the other existing one, which already appeared under `frontend` (Save). Pickup stays 10→11.

**Host-only noise.** `fps`, `memory_bytes` and `swing_wall_msec` differ. In rendered mode, `dialog_choice_close` `choices_taken` and the `hub_trigger_mission_menu` command counts alternate between 5 and 6 from run to run. The two wave-37 runs (death-reload r1 against the `pci-base` rerun) show the same alternation, so it is not caused by this branch.

## Refusals kept

- Successful death reload (`player_death`, `saved_level_reloaded` false), as in wave 37.
- Act transfer with a campaign-model bag, an imported record, a live cell with no definition, or more than one local player.
- `save_load` share with no inventory service bound, or with flag bit 8 on a copied cell.
- `rendered_pickup_verified` stays false.

## Cleanup

The mini's `build/` for this worktree was deleted, along with the scratch `pci-base` copy (and its build) and the patch file. `godot/.godot` was kept. No tests, baselines or fixtures were edited.
