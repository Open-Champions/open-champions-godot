STATUS: MORE — CAMPAIGN INVENTORY R3 FIX VERIFIED (2026-11-09). Every finding of audit `play-campaign-inventory-r2.md` (VERDICT FIX: PCR2-1 MEDIUM, PCR2-2..4 LOW) is fixed. A save or load the client refuses for its own reasons is no longer a card error: the front end's latch becomes unknown (−1), and the next act change refuses. The load screen's stores (open 0, slot 1, finished load 0) are modelled. In the live client any act transfer that carries a flag-39 cell refuses, because host memory has no item-definition table, so the act-change clear has not run on real data. Gates, all on the mini, from one sync and one clean build: Release build and the three GDExtensions built; focused tests 8/8, with 8 new mutations each caught; native CTest 259/260 under load, the one failure (`messages-exact`, a 120 s trace timeout) passing when rerun alone; strict Godot matrix 54/54; full combat 2,645 checks, 0 failures; FULL_SAVE_LOAD 300/300, identical; FULL_TRACE unchanged at 36,402 bytes, `9c7922b2…c659`. Journeys 80 headless / 81 rendered, pickup 10→11. Compared step by step, every step/result pair and every field matches wave 37, except the known dialog-timing counts and the new `source_loads` rows; there the act change to index 22 now refuses for the unknown latch.

# Play campaign inventory — round 3 (audit fix) — 2026-11-09

Audit `play-campaign-inventory-r2.md` (VERDICT FIX: PCR2-1 MEDIUM, PCR2-2..4 LOW), against tip `d2ca9b4f`. Edits and commits are on the MacBook, because the mini's copy of this worktree is a plain file copy with no git. Builds, tests, Godot and journeys ran on the Mac mini in `~/malik-champions/open-champions/worktrees/play-campaign-inventory/`. Every build used the one `build/` root, sequentially, with `cmake --build -j 4`, and `ctest -j 3`. Sources were synced as tracked files with `rsync -rlp --checksum` (no `-t`), so every changed file got a new mtime. I did not push or open a PR. I killed only PIDs I had started. Mini home paths in the logs I brought back are rewritten to `~/`.

Research used for the latch:
- `t3/research-save-world` `31a0ca6`: `docs/issues/sync-breakers.md:287` lists all nine stores; `:292` says nothing at session start touches the latch. `docs/issues/dynamic-tests.md:379` covers the save screen.
- `t3/research-save-screen` `c8895e2`: `docs/save-screen.md` covers the load screen. `:251-256`: `memcard_requestLoad` clears the latch when it opens, at 0x217760. `:262`: a load slot stores 1 at 0x2195a8. `:266`: step 4 with flag 0 clears it. `:280-284`: step 5 clears it once the live level name matches, and keeps it at 1 while the names differ.

## PCR2-1 (MEDIUM): the session never holds a latch value the original would not

### A client save refusal is not a card error

- **The new rule.** `Flow::save_refused(reason)` (`native/src/frontend.cpp`) is the client's own refusal. That covers a state it cannot save (`pickups.cpp:13-16`, "Saving refused: inventory/equipment reload is not implemented"), a cancelled file chooser, and a host write failure. The original would not have failed at any of these points. It would either have written the card (sub-state 4, then latch 0 when the box closes) or hit a card error (sub-state 9, latch 1). Which of the two is not known.
- **The unknown latch.** After a refusal, the latch becomes `LATCH_UNKNOWN` (−1, `frontend.hpp`).
- **The card-error path is kept.** `save_result(false, …)` is still the card-error path (sub-state 9, latch kept at 1), and `frontend_tests.cpp`'s existing "error keeps the latch" check is unchanged. The client never calls it, because it has no memory card.
- **The client now calls the refusal.** `ui/frontend_menus.gd` calls `save_refused` from `_save_session` when the scene does not write the save, and from `_report_refusal` (cancelled or missing chooser).
- **The unknown reaches the session.** `ChampionsSessionController::set_save_latch` now accepts −1. `forward_save_latch()` forwards it, so the "no latch reading" rule in `require_campaign_act_transfer` can now fire from the session. Any act change refuses before any edit: "Act transfer is refused: the front end's card latch 0x4f8f28 has no reading, so whether the act-change clear runs is unknown."

### Every store with a client point

| Original store | What it does | Client point | Value |
|---|---|---|---|
| 0x2170dc | boot (inside `save_init` 0x216fcc, which runs to at least 0x217448, `sync-breakers.md:443`) | `Flow` construction | 0 (unchanged) |
| 0x218ba4, 0x218c18 | a save slot is chosen (new row or overwrite, `dynamic-tests.md:379`) | SaveGame, `session` entry | 1 (unchanged) |
| 0x2190d0 | sub-state 4: the "saved" box closes | Saved, confirm | 0 (unchanged) |
| 0x217760 | `memcard_requestLoad` opens the load screen | Main `load_game` and Pause `load` | **0 (new)** |
| 0x2195a8 | a load slot is accepted | LoadGame, `session` and `ps2` entries | **1 (new)** |
| 0x219884 | the loaded level has been entered (step 5 name match) | `load_result(true)`: the host reports success after `load_real_level` has entered the saved level | **0 (new)** |
| 0x219810 | a load ends without a level change (step 4, flag 0) | `load_result(true)` for a save without a level; `import_result(true)` for a PS2 load slot | **0 (new)** |
| 0x219a18 | the load's error exit | none: the client has no card read that can fail | — |

- **Client exits with no matching store.** These hold `LATCH_UNKNOWN` instead of a guess: a client load refusal (`load_result(false)`), a refused PS2 load slot (`import_result(false)` on LoadGame), and a client save refusal. The original's error exit (0x219a18) also stores 0. But a client refusal is not that exit, because the original might instead have loaded and still be waiting in step 5 with latch 1 (`save-screen.md:281-284`). So it is not used.
- **Leaving the unknown state.** The unknown state ends only through a real store: opening the load screen (0), choosing a save slot (1), and then its result.
- **How the three load-end stores are matched.** The matching is by address order inside the load screen: 0x219810 lies before the step-5 name compare at 0x21984c (`save-screen.md:282`), and 0x219884 lies after it. No value depends on this matching, because all three store 0.
- **Net launch.** A net launch has no store in the original; the P1 patch in `sync-breakers.md:535` is not the original. The client refuses net games anyway (`Flow::set_net_game`).
- **Death reload.** Death reload never opens the load screen (`save-screen.md:291`), so it stores nothing.
- **Host saves and loads outside the front end.** The host menu's Save…/Load… dialogs (`play_flow.gd`) bypass `Flow`, so they store nothing. Outside a card screen the flow now holds only 0 or −1. A host load enters its level with `carry_campaign=false` and does not read the latch.

### Tests

- **`latch_store_vectors` (`native/tests/frontend_tests.cpp`, new; `PASS frontend 670 checks`):**
  - A save refusal holds −1. A retried slot stores 1, and the saved box clears it.
  - Backing out keeps −1, and an unrequested refusal is refused.
  - Opening the load screen from Pause stores 0, and from the title too.
  - A load slot stores 1. A load refusal holds −1, and a finished load stores 0.
  - The PS2 load slot stores 1. A refused import holds −1, and an imported load stores 0.
  - Leaving play stores nothing.
- **`test_latch_from_front_end` (`native/tests/campaign_inventory_tests.cpp`, new).** It drives a real `fe::Flow` into play:
  - A client save refusal (−1) makes the live act transfer to `innovat1` refuse, with all cells, the act, 0x4f7118 and the journal unchanged.
  - A same-act entry does not read the latch.
  - After the pause menu's load screen has stored 0, the same act change runs the clear (1 removed, 0x4f7118 = 1, 0x4f8f28 mirrored 0).
- **`godot/tests/frontend_smoke.gd` (added lines).** The real scene runs:
  - a save the host cannot write (a path under a regular file): the screen stays on save-game, and the flow and the session hold −1;
  - the retried save: 1 at the box, then 0;
  - a finished load: 0;
  - a load of an absent file: −1, kept after backing out;
  - reopening the load screen: 0.
- **Mutations** (`r3/mutate.out`, each applied alone, rebuilt, run, restored). All are caught:
  - the refusal treated as a card error (caught by both the campaign-inventory and frontend tests);
  - no store at the load screen's opening;
  - no store at a load slot;
  - no clear after a finished load;
  - no unknown after a load refusal;
  - the "no reading" refusal removed.

### The report's "As in the original" claim

The claim is withdrawn. Round 2 said "As in the original, that leaves the latch at 1". In fact the journey's latch of 1 came from the client's own save refusal, sent down the card-error path. It was not a card error, and the original would not have held 1 because of it. The PCI-3 paragraph and the journey section are corrected.

## PCR2-2 (LOW): gate table, clean-rebuild log, final-code gates

- **Round 2's table and build note are corrected in place** (see "Round 2 gates" below). The first native build failed, the second succeeded, and the clean rebuild is the gate tree.
- **The round-2 clean-rebuild logs are committed** (`r2/native-clean-configure.log`, `r2/native-clean-build.log`, `r2/clean.status`, `r2/build-times.txt`).
- **Round 2's matrix and journeys are not shown to be on the final code.** They used extensions built before two later syncs. So round 3 reran every gate after one sync and one clean build.
- **A stale-binary problem inside round 3, found by the gates and fixed before any gate counted.** My first round-3 CTest aborted `frontend-exact` with the mutated binary.
  - **Cause:** the mutation script restored and touched `frontend.cpp` in the same second as the mutated build, so make saw it as current.
  - **Second risk:** the mutations also ran while the extensions were compiling the same native sources.
  - **What I did:** I stopped my CTest and matrix by their PIDs and set that evidence aside (`r3/aborted/`, not counted). I checked that the mini's tracked files match the MacBook's byte for byte (an `rsync --checksum` dry run showed no difference), deleted `build/`, and rebuilt everything from scratch with nothing else running.
  - **Which runs count:** every gate below ran on that clean build, after the mutations. The mutation results stand: each mutant was built and run on its own.

## PCR2-3 (LOW): the flag-39 outcome

- **Where it is stated.** The status line and "What is still refused" now say it plainly: in the live client, any act transfer that carries a flag-39 cell refuses. Host memory has no item-definition table, so the cell's name cannot be read. The act-change removal has therefore not run on real data.
- **The journey sentence is reworded.** The correct inference runs one way: no carried transfer was refused for an unreadable name, so none of the carried cells has flag 39, because a flag-39 cell would have refused.

## PCR2-4 (LOW): a test that fails under the old share order

- **The new checks.** `ShareInventory` (`native/tests/ps2_save_tests.cpp`) records what memory holds when the share's first service call runs. Case 1 now checks four values at that moment:
  - the pending name is `crypt` (step 7);
  - the loaded flag is 1 (step 6);
  - quest variable `opened` is 7 (step 5);
  - the autosave countdown is still 0 (step 8 has not run).
- **The test now fails under the old order.** The `share-old-order` mutation moves the call back to its round-1 place, after the records and before `vars_set`. It fails `ps2-save: line 1101: inv.seen && inv.pending_at_share == "crypt"` (`r3/mutate.out`). The check count rises from 459 to 463.

## Round 3 gates

All on the Mac mini, on the final code. One tracked-file sync came first, then `build/` was deleted and everything was built from scratch in it, sequentially (`r3/build.sh`, 05:56–06:10). Evidence is under `work/campaign-inventory/r3/`, committed with `git add -f`.

| Gate | Result | Commit |
|---|---|---|
| Release build and GDExtensions (`build.status`, `native-build.log`, `*-template_debug-build.log`) | native Release exit 0, **0 errors** (10 warnings, all `ld` duplicate-library notes). `mac-template_debug` (archive), `level-texture-template_debug` and `creature-template_debug`: exit 0, 0 errors, arm64, installed 06:06–06:09. Their 3 warnings are in older code outside this diff (`level_texture.cpp:388`, `ui_hud_round3_service.cpp:53`, a linker note) | `4d42bae0` |
| Focused tests (`focused.out`) | 8/8: campaign-inventory, ps2-save (**463** checks), frontend-exact (**670** checks), quests, script-commands, level-flow-exact, mission2-actors, pickups | `4d42bae0` |
| Mutations (`mutate.out`) | 8/8 caught: share-old-order (PCR2-4), refusal treated as a card error (two tests), no clear when the load screen opens, no store at a load slot, no clear after a finished load, no unknown after a load refusal, "no reading" refusal removed | `08db61fb` |
| Native CTest (`ctest.out`, `ctest-messages-alone.out`) | **259/260** with `ctest -j 3` (load average 75–91 from other users' jobs). The one failure is `messages-exact`: its trace tool hit the test's 120 s subprocess timeout (`TimeoutExpired`, `ctest.out:93`). **Rerun alone it passed in 29.2 s.** All the others passed, including campaign-inventory, ps2-save, frontend-exact, script-commands, quests and pickups | `2f41a69e` |
| Strict Godot matrix (`matrix.out`, `matrix/results.json`) | **54/54 PASS**, exit 0 (`tools/godot_matrix.py matrix --captures --timeout 1200`). `frontend_smoke`: `FRONTEND_SMOKE checks=157 failures=0` (150 before; the new lines add 7 checks) | `4d42bae0` |
| Full combat (`matrix/playable_wave_smoke_full_combat.log`) | `PLAYABLE_PLAY_RESULT` checks **2,645**, failures **0** | same |
| FULL_SAVE_LOAD | **300/300** (`presents [300,300]`). Continued and uninterrupted are both `cb2309ab23d3e2906b5b9d4e95e5577c354869fce3d86cce9fe592a0d2c8d573` | same |
| FULL_TRACE | **36,402 bytes, `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`**, headless, unchanged | same |
| Journeys (`journey/`) | **80 headless / 81 rendered**, wrapper exit 1 (the retained `player_death` refusal, as in wave 37). Pickup **10→11** in both, produced, walked and picked up. 0 log errors. **6** refusal kinds (wave 37: 6). Disc 994 files before and after | `b350f056` |

Not counted: `r3/aborted/` holds the first round-3 CTest and matrix, which I stopped. They ran on a tree the mutation script had left stale (see PCR2-2).

## Round 3 journeys, compared step by step

`r3/journey/compare.txt` is the output of `r3/journey/compare.py`. For each mode it compares every step's name, result and every field. Host-only fields (`fps`, `memory_bytes`, `swing_wall_msec`) are left out.

**Against wave 37** (`base-70d2de19/`):
- Every step/result pair is the same in both modes.
- Every other field is identical, apart from two things.
  - **Steps 18 and 27** (`dialog_choice_close.choices_taken`, `hub_trigger_mission_menu` `addQuest`/`setMissionAvailable`) give 4/2/3 against 5/3/4. That is the host-timing count explained in round 2: rendered runs already gave 3/1/2 and 4/2/3 on one build.
  - **Step 57 (58 rendered), `walk_in_exit_trigger`, has the `source_loads` rows.** These are new fields from round 2.
- Every other step-57 field is wave 37's: `sources_examined` 2, `walk_attempts` 3, `exit_targets` 20, `level_index_after` 22, `navigation_stalls` 3, `trigger_samples` 162 and `quest_vars` 14.
- So is everything after step 57: the fight on index 99 (ticks 5,942, `walk_ticks` 52, the boar counts, the stub-box count 120,634, `model_getAttachPoint` 11,884, and the WarBoar wander 72 / chase 1 path refusals), the drop position, the pickup, and the refusal list.

**The `source_loads` rows (both modes, 91 rows):**

| Index | Entry | Latch | Outcome |
|---|---|---|---|
| 19 | same act from 36 (the latch is not read) | −1 | loaded |
| 22 | act change from 19 | −1 | **refused before any edit:** "Act transfer is refused: the front end's card latch 0x4f8f28 has no reading, so whether the act-change clear runs is unknown." |
| the other 89 (92, 96, 0, …, and the second pass) | not loaded | −1 | "Level change refused: per-level object directory is full or unreadable." This is index 19's departing save, the wave-37 reason |

- **Where the −1 comes from.** The journey's front-end save is refused by the client (`frontend: scene Save refused: Saving refused: <path> reload is not implemented.`). That is now a client refusal, so the flow and the session hold −1. Nothing between that save and step 57 stores the latch: every row reads −1.
- **The act change refuses, as PCR2-1 requires.** So the act-change clear did **not** run on real data in this round either: no act transfer that carried cells was accepted.

**Against round 2** (`r2/journey/`):
- **Step 57 is back to wave 37's values**, listed above. Round 2 accepted 22 and 48 with latch 1; round 3 refuses 22.
- **The two refusal kinds from round 2's complete-command stops are gone.** These were `refused preloadResource` and `refused deleteMonster`. Their levels are no longer entered.
- **The death-state reason in steps 57 and 60 is replaced by wave 37's directory reason.**
- **The fight fields return to wave 37's values.** The probe again ends on index 22, so the player starts where wave 37 started.

## What is still refused (current)

- **The successful death reload** (`player_death`, `saved_level_reloaded` false), as in wave 37.
- **Any act change while the front end's latch is unknown (−1).** That is the state after a client save refusal, a client load refusal or a refused PS2 load slot, until the load screen is opened or a save completes. On the journey this refuses index 22.
- **An act transfer with live cells when the session's bag-carry guard fails.**
- **An act transfer that carries a flag-39 cell, in the live client.** Host memory has no item-definition table on this path, so the cell's name cannot be read. Any carried flag-39 cell therefore refuses, and the act-change removal has not run on real data (not in round 2's journey, where the latch skipped it, and not in round 3's, where the unknown latch refuses first).
- **An act transfer with:**
  - a campaign-model bag;
  - an imported record;
  - a live cell with no definition;
  - more than one local player.
- **Scripts on levels reached by carrying cells across an act stop at their first unsupported command.** No such level is reached on the round-3 journey.
- **The `save_load` share** with no inventory service bound, with flag bit 8 on a copied cell, or with a negative saved definition row.
- **`rendered_pickup_verified`** stays false.

## Unknowns, conservative and disclosed

- **The latch after a client refusal.** The original would hold 0 or 1. Which one is not known, so the client holds −1 and refuses act changes. It does not guess.
- **The pairing of 0x219810, 0x219884 and 0x219a18 with the three load-screen ends.** This is by address order around the step-5 compare at 0x21984c. All three store 0, so no client value depends on the pairing.
- **Host saves and loads outside the front end** (the host menu's dialogs) store nothing. The original has no such path.
- **Keep list.** These are only the executed names (`docs/quests.md:163-169`); none was added. Which retail definitions carry flag 39 is still not asserted.
- **A refusal mid-`change_level`.** A refusal from `clear_act_change_inventory` inside `Host::change_level` happens after level flow's own act state has moved. In the client that host is a discarded candidate, because `enable_level_script` fails and keeps the old host. `prepare_campaign_entry` checks the same rule before `change_level`, so on the client path it refuses first.
- **The items-crossed chain lives in hosts.** `Host::items_crossed_act` is kept from host to host, not in a save. A session loaded from a save file starts without it.

## Round 3 cleanup

- **Deleted:** the mini's `build/` for this worktree.
- **Kept:** `godot/.godot`, the extension binaries under `godot/bin` and `godot/creature_anim/bin`, and the ignored scratch under `work/pci-r3/`.
- **Tests and baselines:** none weakened. Round 3 removes no test line (219 lines added under `native/tests` and `godot/tests`) and changes no `.tsv`, `.json` or fixture baseline.
- **No private paths or disc text:** no added line has a mini or MacBook home path, and the committed logs use `~/`. The journey JSON adds only counts, indices and client refusal strings.

---
# Round 2 record (audit `play-campaign-inventory-r1.md`), kept; corrected in place where round 3 found it wrong

Branch `codex/play-campaign-inventory`, worktree `~/Projects/open-champions/worktrees/play-campaign-inventory`. Round 1 ended at `b563d486`, from wave 37 `70d2de19`. Edits and commits are on the MacBook. Builds, tests, Godot and journeys ran on the Mac mini, in `~/malik-champions/open-champions/worktrees/play-campaign-inventory/`, with `cmake --build -j 4` and `ctest -j 3`. Research used: `research-inventory-reset` `ce2e086` (`docs/inventory-reset.md`, `docs/audio-save.md`, `docs/quests.md`) and `research-story-route-r3` `019a155` (`docs/story-route.md`). The disc at `../../private-validation/rta-disc` was used read-only and only counted (994 files before and after each journey). I did not push or open a PR, touched nothing under `~/champions-re` or `playable-integration`, and killed no process by name. Mini home paths in the logs I brought back are rewritten to `~/`.

## Round 2: Fixes

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
- **Corrected in round 3 (PCR2-1).** Round 2 said here that the journey's front-end save "gets a card error". It did not: the client refused that save for its own reasons, and round 2 sent every client refusal down the card-error path. Round 3 separates the two (see PCR2-1).

### PCI-4: an unreadable name refuses; nothing is removed

- **The rule.** `campaign_inventory::unclassified_cell(memory, players)` returns a reason when a count is outside 0..64 or a cell's definition word is 0. For a flag-39 cell it also returns a reason when the definition's name pointer is 0, or the name reads as empty, in that memory.
- **Where it runs.** `require_campaign_act_transfer` runs it on the destination's memory after the copy, so it refuses before `change_level`. `clear_act_change_inventory` runs it again over every slot before its first edit, so `Host::change_level` cannot remove part of a bag. `remove_unkept` no longer reads a name it has not checked.
- **Tests** (`test_unreadable_names`), all without hand-copied definitions in the destination:
  - The act transfer refuses ("…has no name pointer in this memory…"). All cells, the act and the journal stay, and 0x4f7118 stays 0.
  - An empty name refuses instead of being removed.
  - Cells without flag 39 cross without readable names (no keep decision is needed).
  - `Host::change_level` from −1 with no readable names refuses "before any edit" and removes nothing.
- **In the live client.** The host's memory has no executable-backed item table on this path (round 1's audit: the only `memory.under` binding is `campaign_fight_probe.cpp`). So any carried flag-39 cell refuses the act transfer instead of being stripped, and the act-change removal has not run on real data.

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

The `inventory_shareQuestItems(0)` call in `load_impl` (`native/src/ps2_save.cpp`) now runs after `vars_clearAll`/`vars_set`, the loaded flag, act, difficulty, play time, the pending name and marker, and the loaded name. It runs before the autosave countdown, matching `audio-save.md:129-138` steps 6–8. The stale comment "9. The share word is zero (checked above)" is replaced. Round 2 said test case 1 proved this order; it did not (PCR2-4). Round 3 adds a check that does.

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

## Round 2: Gates

Evidence is under `work/campaign-inventory/r2/`, committed with `git add -f`.

| Gate | Result | Commit |
|---|---|---|
| Release build and GDExtensions (`build.out`; corrected in round 3, PCR2-2) | native Release: the **first** build failed (`native exit 2`, 1 error: a test compile error in `campaign_inventory_tests.cpp`). The second build (`native-build2.log`) succeeded, and two incremental rebuilds followed. The gate's native tree is the clean rebuild (`r2/native-clean-configure.log`, `r2/native-clean-build.log`: 0 errors, 391 compiled sources, linker duplicate-library notes only; times in `r2/build-times.txt`). `mac-template_debug` (archive), `level-texture-template_debug` and `creature-template_debug`: exit 0, arm64, built once at 00:52–00:55 | `8dbb15aa` |
| Focused tests (`focused.out`) | 7/7: campaign-inventory, ps2-save (459 checks), quests, frontend-exact, script-commands (with `mission2_host_tests.hpp`), level-flow-exact, mission2-actors. Ten mutations, each applied alone and then restored, were all caught: share call, bit-8 refusal, row-0 rule, guard, latch mirror, name pointer, empty name, death task, act-change clear, items-crossed chain | `d07e0765` |
| Native CTest (`work/ctest.out`) | **260/260 PASS** (`ctest -j 3`, clean `build/native`), including campaign-inventory, ps2-save, script-commands, quests, pickups | `323d9748` |
| Strict Godot matrix (`matrix.out`, `matrix/results.json`) | **54/54 PASS**, exit 0 (`tools/godot_matrix.py matrix --captures --timeout 1200`, Godot 4.5.2.stable.official.6ce3de25a) | `b94e23dd` |
| Full combat (`matrix/playable_wave_smoke_full_combat.log`) | `PLAYABLE_PLAY_RESULT` checks **2,645**, failures **0** | same |
| FULL_SAVE_LOAD | **300/300**. Continued and uninterrupted presentation are both `cb2309ab23d3e2906b5b9d4e95e5577c354869fce3d86cce9fe592a0d2c8d573` | same |
| FULL_TRACE | **36,402 bytes, `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`**, headless, unchanged | same |
| Journeys (`journey/`, rerun in `journey-rerun/`) | **80 headless / 81 rendered**, wrapper exit 1 (the retained `player_death` refusal, as in wave 37). Pickup **10→11** in both, produced, walked and picked up. 0 log errors. 8 refusal kinds. Disc 994/994 | `bdc0b6aa` |

The CTest run that failed four tests on stale objects is kept as `r2/ctest-stale-objects.out` (see the build note). **Corrected in round 3 (PCR2-2):** round 2 said the matrix and journeys ran on the code at `8dbb15aa`. That was not shown. They used extensions built at 00:52–00:55, before two later syncs (01:08 and 02:03) and before the clean native rebuild (02:03–02:08), and round 2 did not record what those syncs changed. Round 3 therefore reran every gate after one sync and one clean build (see the round 3 gates).

## Round 2: Journeys against wave 37: every difference

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

- **The latch (corrected in round 3, PCR2-1).** The journey's front-end save step is refused by the client itself (`frontend: scene Save refused: Saving refused: <path> reload is not implemented.`). Round 2 sent that refusal down the card-error path and so held latch 1, and both act changes then stored the act and skipped the clear (removed 0, 0x4f7118 0). Round 2 wrote "As in the original, that leaves the latch at 1". That was wrong. It was not a card error, and the original's latch after it is unknown. Round 3 holds −1 there, and the act change refuses (see the round 3 journey).
- **No flag-39 refusals (reworded in round 3, PCR2-3).** Neither transfer was refused for an unreadable name. Host memory has no definition table on this path, so a carried flag-39 cell would have refused. None did, so none of the 7 carried cells has flag 39.
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

## Round 2: Build note

The first full CTest run on this round failed `pickups` (abort), `messages-exact`, `player-exact` and `monsters-1-exact`. The cause was stale objects, not this change:

1. The first build (`native-build.log`, corrected from "the second build" in round 3) stopped at a test compile error before recompiling some objects that include `script_commands.hpp`.
2. A later `rsync -a` put back the MacBook's older mtime on that header, so make treated the old objects as current.

I deleted `build/native`, rebuilt it from scratch, and reran the whole suite; that clean run is the gate above. Its configure and build logs are now committed (`r2/native-clean-configure.log`, `r2/native-clean-build.log`, `r2/clean.status`), with each log's time in `r2/build-times.txt`.

