STATUS: IN PROGRESS — gates being recorded (2026-11-09).

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
