STATUS: DONE (2026-11-09). Section 6 Observed rows implemented or locked by synthetic tests; Release build, GDExtensions, focused tests PASS; full CTest 260/260 (two load timeouts pass alone); strict Godot 54/54; FULL_SAVE_LOAD 300/300 and FULL_TRACE unchanged; journeys 80/81 with pickup 10→11 and unchanged step/result pairs. Populated-page draws, parchment crop and pixel parity stay refused.

# Play quest log, round 1 — 2026-11-08/09

Branch `codex/play-quest-log`, worktree `~/Projects/open-champions/worktrees/play-quest-log`, from wave 37 VERIFIED `70d2de19`. Edits and commits on the MacBook; every build, test, Godot run and journey on the Mac mini in a synced copy under `~/malik-champions/open-champions/worktrees/play-quest-log`. Logs are under ignored `work/ql/` with the mini home path replaced by `~/`. Research: `t3/research-quest-log` `965ec52` `docs/spec/quest-log.md` section 6, audits `research-quest-log-r2` (FIX) and `research-quest-log-r2-fix` (PASS).

## What the client already had

- `quests::Campaign::quest_add` / `quest_remove` and the binder's `Variables::quest_add` / `quest_remove` (`native/src/quests.cpp`, `native/src/script_builtins.cpp`) already matched the add early return, the strcpy tails, the placeholder and the slide from the last match.
- `mission_set_complete` (both) already set or cleared bit 1 on all four records and skipped mission 13.
- The list clear existed only inside `Campaign::clear_for_act_change` (lists, flag-39 items and variable values together).
- `Variables::save_player_record` / `restore_player_record` already copied the whole record and restored the live medal range.
- `ChampionsSessionController::quest_journal()` returned record 0's keys resolved to text; `godot/ui/quest_journal.gd` validated the 320×350 parchment header and refused all layout.
- No model of the page's text calls; no test of section 6's list, writer, completion or save-span claims.

## What changed

| Item | Change | Research |
|---|---|---|
| Storage order, `quest_add` early return | tests only (`test_add`, `test_binder_list`) | `quest-log.md:213`, `ref_quest_log.py:73-86` |
| `quest_remove`, A,B,A → NoQuests,B | tests only (`test_remove`, `test_binder_list`) | `quest-log.md:214-215`, `ref_quest_log.py:89-107` |
| `mission_setComplete` leaves the list | tests only; binder test records every store and finds none in the four spans | `quest-log.md:216` |
| `quest_clearAll` over the first `g_numPlayers` lists | new `Campaign::quest_clear_all`; `clear_for_act_change` calls it first | `quest-log.md:217`, `ref_quest_log.py:110-118` |
| `addQuest` / `removeQuest` as the only journal writers | host test: 16 other campaign commands, together and one by one, leave the binder span byte-identical; the two writers change it | `quest-log.md:225-232` |
| Page text-call arguments | new `quest_log::page_calls`, `row_positions` (`native/include/champions/quest_log.hpp`, `native/src/quest_log.cpp`): `text_begin(font, 6, 0x80323c3c, -1)`, `text_setClip(0, 119, 320, 440)`, `0x1802e0(0x80323c3c, -1)`, title lookup, `text_drawWrapped(24, y, ptr, 288, 82, 418, 21)`, `0x1802e0(0x802d2d2d, -1)`, body lookup, the same wrap, `text_end`; y from 114 plus each caller-supplied wrap return, +22 after the body; storage order; no completed row | `quest-log.md:259-263`, `75-76`, `78`, `219-223`, `ref_quest_log.py:121-133` |
| Empty state | `page_calls` count 0: begin, `NoQuests` lookup, `text_drawWideCentered(160, 224, ptr, 1000000)`, end; `quest_journal_calls()`; the journal interprets those calls (gamefontsmall, layer 6) | `quest-log.md:77`, `263-265` |
| `savePlayer` / `restorePlayer` quest span | tests only, medal slots 0 and 1: copy span equals live span with tails after a `0xa5` fill; restore over a `0x5a` fill returns title, body and tails; the live medal byte stays 4 after 9; the other span is untouched | `quest-log.md:269-277` |

New CTest `quest-log` (`champions-quest-log-tests`, synthetic keys `A`, `B`, `Q0`, `B0` …). Docs: `docs/QUESTS.md` "Quest log (2026-11-08)".

## Refused, with reasons

- **Populated page draw / pixel parity of entries.** A loaded font's `text_drawWrapped` return is Unknown (`quest-log.md:76`, `288`), so every y after the first title is not a value; `page_calls` takes the return from the caller and `quest_journal_calls()` refuses a non-empty list instead of inventing one. The stores inside `0x1802e0` are Unknown (`quest-log.md:75`, `289`); the call is emitted by address name only.
- **Parchment crop.** Header validated; the `(0, −4, 104, 5, 0x80808080)` submission is not made because the displayed crop and the third argument's role are Unknown (`quest-log.md:74`, `290`).
- **Pixel parity in general.** Coordinates are call arguments; no retail frame was measured (`quest-log.md:16-21`, `82-84`). The empty-state glyph placement is this client's centring of the Observed call.
- Out of scope: `save_build` / `save_load` player block (no card image here), which `addQuest` sites a session reaches, quest-variable round trip.

## Gates (mini)

- **Release build -j 4: PASS**, configure 2.4 s, build 2 m 32 s, exit 0 (`work/ql/configure.log`, `build.log`). No warning in the new files. Commit `9e6d1b6a`.
- **Focused tests: PASS**, `ctest -R '^(quest-log|quests)$'`: 2/2 (`work/ql/focused.log`).
- **GDExtensions: PASS**, exit 0 for all ten configure/build steps: archive and level-texture `template_debug` / `template_release`, creature-animation `template_debug`; every library `lipo -archs` = `x86_64 arm64` (`work/ql/gdext-status.log`). Commit `e33d4116`.
- **Full CTest -j 3: 260 registered (259 + `quest-log`); 258/260 passed in the full run (50 m 6 s), and the two failures were the harness's own subprocess timeouts under a mini load average of 75–170 from other agents:** `messages-exact` (`champions-messages-trace` past its 120 s limit) and `session-facing-exact` (past its 600 s limit). Rerun alone, both **PASS** (98.17 s, 306.32 s; `work/ql/ctest.out`, `ctest-rerun.out`). Neither touches quest code. No test changed.
- **Strict Godot dev matrix: 54/54 PASS**, exit 0, `--captures --timeout 1200`, unchanged `tools/godot_matrix.py`, 5 m 22 s (`work/ql/matrix/results.json`, `matrix.out`). A first attempt is kept as `work/ql/matrix-attempt1-import-aborted/`: its bounded editor case (`--quit-after 120 --frame-delay 50`) logged `Scan thread aborted` at a mini load average near 170, so `assets/open_champions_avatar.png` was never imported and every case loading it failed on that missing `.ctex` (the only diagnostics). Following the task's rule, one full headless import (`Godot --headless --path godot --import`, exit 0, 0 errors, `work/ql/editor-import.log`) created the worktree's `godot/.godot`, which the runner copies; the rerun passed. No runner, test or allowlist changed.
- **Full synthetic combat: 2,645 checks, 0 failures** (`matrix/playable_wave_smoke_full_combat.log`).
- **FULL_SAVE_LOAD: PASS, 300/300.** Continued and uninterrupted presentation SHA-256 both `cb2309ab23d3e2906b5b9d4e95e5577c354869fce3d86cce9fe592a0d2c8d573`, identical to wave 37.
- **FULL_TRACE: PASS, unchanged**, 36,402 bytes, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`. Expected: the change touches neither combat nor the trace.
- **Journeys: PASS against the wave-37 shape.** Unchanged `tools/run_loop_playthrough.py`, headless then rendered, standard input closed, read-only disc `../../private-validation/rta-disc` (`work/ql/playthrough/`, `playthrough.out`; first run kept as `playthrough-run1/`). Both runs gave the same result:

| Variant | Steps | Refused rows | Refusal kinds | Exit | Pickup | Walk ticks | Log errors | Captures |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Headless | 80 | 27 | 6 | 1 | **10→11**, produced/walked/picked up | 52 | 0 | 0 |
| Rendered | 81 | 28 | 6 | 1 | **10→11**, produced/walked/picked up | 52 | 0 | 38 |

Disc files 994 before and 994 after, both runs. Compared with death-reload round 1's journey (`play-death-reload/work/death-reload/playthrough`, which wave 37 matched step for step), the step name/result pairs are identical at every index in both variants, and so are the six refusal kinds. Wrapper exit 1 is still the retained full-journey refusal (`player_death`, `saved_level_reloaded` false). `rendered_pickup_verified` remains false.

Every difference, explained:

1. **`journal_after_dialog.refusal` text, and its refusal-map key.** Old: `journal layout refused: font, wrap, entry positions, empty-state position and parchment crop unresolved`. New: the precise reasons, i.e. a loaded font's wrap return Unknown (`quest-log.md` 76, 288), the `0x1802e0` stores Unknown (75, 289), and the parchment crop Unknown (74, 290). Intended. The step is still `layout_refused` with `entry_count` 1, `art_ready` true, and 0 text, glyph and sprite submissions, so the journey's unchanged check (`godot/tests/loop_playthrough.gd:823-826`, prefix `journal layout refused:`) passes. The journey reaches the journal with one quest, so the new empty-page draw is not exercised there; the native `quest-log` test covers its calls.
2. **`dialog_choice_close.choices_taken`, and with it `hub_trigger_mission_menu.script_executed_commands` `addQuest` / `setMissionAvailable`.** Here: headless 4 (`addQuest` 2, `setMissionAvailable` 3), rendered 3 (1, 2), the same in both runs. Death-reload r1: 5 (3, 4) in both variants. This depends on load, not on this change. The close loop presses Enter every 8 displayed frames (`godot/tests/loop_playthrough.gd:783-793`), and the dialog and HUD controls advance on wall-clock accumulators capped at 4 frames per displayed frame (`godot/ui/text_hud.gd:91-99`). So the number of choices that land before the dialog closes depends on frame rate. These runs had a mini load average of 110–170 from other agents (journal-step fps 86/99 headless and 25/33 rendered here, against 144/124 in the baseline). Other journeys on the same wave-37 base `70d2de19`, read with `work/ql/choices.py`, range from 3 to 6: `play-give-item` 5/5, `play-levelup-hud` and `play-mission2-route` 5/6, `play-vendors-r2` 4/3, `play-char-appearance` and `play-story-route` 3/3. All have unchanged step/result pairs. Nothing this branch changes runs in that loop. The journal only computes while visible, `quest_journal_calls` is called only by the visible journal with an empty list, `quest_clear_all` runs only on an act change, and `quest_log` is otherwise unused.
3. **Host-only fields** `fps`, `memory_bytes` and `swing_wall_msec` differ as in every wave (wall clock and allocator).

## Commits

- `9e6d1b6a` native page calls, `quest_clear_all`, CTest `quest-log`; Release build and focused tests.
- `e33d4116` controller `quest_journal_calls`, journal empty-page draw and precise refusal, `docs/QUESTS.md`; GDExtensions.
- `69e9a787`, `7baa6755` and this report: the gate records.

## Boundaries

No push, PR, sibling-repository edit, `~/champions-re` or playable-integration access, or process killed by name (only my own rsync PID). No test, baseline, fixture or runner was weakened or edited. Synthetic fixtures only. No game text is tracked; `NoQuests` is the placeholder identifier the repo already used (`quests::QUEST_PLACEHOLDER`). No value was invented: wrap returns are supplied by the caller and refused in the session. The mini's `build/` was deleted at the end. Its `godot/.godot` import cache stays for later runs.
