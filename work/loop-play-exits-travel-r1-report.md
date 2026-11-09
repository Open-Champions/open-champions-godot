STATUS: PENDING GATES

# Play exits travel, round 1 — 2026-11-09

Branch `codex/play-exits-travel`, worktree
`~/Projects/open-champions/worktrees/play-exits-travel`, from wave 37 VERIFIED
`70d2de19`. Research: `research-exits-r3@e86f3de`,
`docs/exits-r4-travel.md` (audit `research-exits-r4` PASS) and the round-three
handoff contracts in `docs/exits-r3-handoff.md` (audited PASS). Edits and
commits are on the MacBook worktree. Builds, tests, Godot and journeys ran only
on the Mac mini, in a synced copy at
`~/malik-champions/open-champions/worktrees/play-exits-travel`. The private disc
was read only, counts only.

## What the client did before this change

- **Act selection.** `level_flow::find_act` (`native/src/level_flow.cpp`) and
  `quests::find_act` (`native/src/quests.cpp:180`) already searched the current
  act first, then acts 0..12, with a prefix match. `begin_change` changed the
  act even when the clear block was skipped. This matches
  `exits-r4-travel.md:12-23` and `:65-66`, so it was not changed.
- **Departure directory.** `LevelStates::store` returned `bool` (true meaning
  "bytes written"). A non-hub name needed an **exact** current-act row, following
  the older `docs/level-flow.md` 7.3 reading. An 11th key **threw**, because 7.3
  called it unreachable. The entry was already created when the bytes did not
  fit, and the size budget (`0x16118 + sum + size <= 0x4bc78`) and the card 4 /
  flag 0x800 skip were already correct.
- **Arrival lookup.** `begin_change` set `had_saved_state` from whether owner
  bytes existed for the key, not from the directory key. The idle-voice
  countdown (300 on a miss) used that value.
- **Loader consumer.** `begin_change` already skipped the store for an empty
  level or a death task, and skipped the act check in the front end. It cleared
  only on an act change with no latch and no death task. It wrote `prev` before
  splitting at the first `_` (a leading `_` leaves the base empty), saved the
  orientation byte into `orientation_copy` and forced `0xfe`, and emitted
  `player_selection_apply_pending` on every path. The caller's nonzero
  store-result branch was never modelled.
- **Placement gates.** `place_players` already copied only x/y on recall and
  overwrote every slot's z with terrain. It skipped the search for an unloaded
  level, PvP, rank 3 and slot 0, passed 36/14 to the search, added `8000`
  (`0x1f40`) to facing with 16-bit wrap, and built a slot only with hp > 0, the
  peer/active test and the medal slot. `apply_act_entry` wrote `None` to every
  record, dead slots included, unless card state was 4 or a death task ran.
- **Live Godot client.** Each level is a fresh script `Host`.
  `prepare_campaign_entry` (`native/include/champions/campaign_entry.hpp`) runs
  `change_level` only for a table-proved act transfer, on a new host with no
  current level, so no departure entry is stored. Nothing in the live client
  reads `prev`, `orientation_copy` or `states` (checked with grep). The two
  arrival refusals are in `gdextension/src/session_controller.cpp`.

## What changed (Observed only)

- `LevelStates::store` returns `StoreResult` (`native/include/champions/level_flow.hpp`).
  `ret` is the original register, 0 on every path, and is not a status
  (`exits-r4-travel.md:27`). `skipped` is set for card 4 or flag 0x800 (`:28-29`).
  `entry` is set when the key exists afterwards, written or not (`:36-41`).
  `writer_offset` is the data offset passed to `0x218600`, or none (`:38-40`).
- `save_levelIndexInAct` is now a prefix match: the first current-act row the
  name starts with. `storm1a` while in act 8 reserves the `storm1` size
  (`:30-34`; `tools/ref_exit_travel.py:91-106`). Act −1 stores nothing unless
  the forced byte is set (`ref_exit_travel.py:97-100`).
- The key search covers the first ten entries. A new key is appended,
  including as an 11th entry after ten (`:37-38`; `ref_exit_travel.py:109-128`).
  An existing key keeps its stored size (`:37`).
- New `LevelStates::stored` implements `save_levelStateStored` `0x2182d4`, a
  directory-key lookup (`:110-111`). `begin_change` uses it for
  `had_saved_state` and the idle-voice countdown: a miss stores 300, a hit
  leaves it unchanged, even when the writer was never called
  (`difftest_exit_travel.py:440-455`). `ChangeResult::saved_bytes` reports
  whether owner bytes exist.
- `ChangeResult` and `begin_change` now state in the header that the loader's
  return register is not a status (`:122-123`), and list the consumer order
  (`:51-85`).
- `SessionAdapter::change` refuses (`Unsupported`) a directory hit that has no
  writer bytes, because what the original restores from that unwritten area is
  Unknown (`:40-43`).
- `docs/LEVEL_FLOW.md` records the two corrections and the new bounds.

The new and updated tests use synthetic act tables (13 acts shaped like the
probe: `hub` in every act, `storm1` before `storm1a` in act 8, `honor` in act 6)
with authored sizes. No retail size or value is used. `travel_r4_cases` in
`native/tests/level_flow_tests.cpp` covers:
- the ten `difftest_exit_travel.py:78-89` act cases, through both `find_act`
  and `begin_change`;
- the fifteen `:142-164` departure cases (directory, writer offset, `ret == 0`);
- the act −1 rule, and `stored` / 12th-entry refusals;
- the loader cases from `:295-466`: empty level, death task, front end, latch,
  same act versus new act with `vars_clearAll` keeping the name, directory hit
  without bytes, miss → 300, leading `_`, orientation saved / `0xfe`;
- the `None` write gates;
- the placement gates: recall x/y only, terrain z, slot 0 unsearched, 36/14,
  `0xf000 + 0x1f40 → 0x0f40`, rank 3 / unloaded / PvP, and the hp / peer /
  medal construction gates.

`integration()` adds the adapter refusal for a directory hit with no bytes.

Two existing assertions encoded the superseded reading. Each was replaced by
a stronger one; no check was removed:
- `CHECK(!states.store("aa_x", acts[0]))` (exact row) is now: `aa_x` takes
  the `aa` row, size 50, written at offset 150, `ret == 0`.
- `refuses(... "eleven" ...)` is now: the 11th key is appended at offset 10
  with size 35000; a 12th refuses; the directory stays at 11 entries.

## Not changed, with reasons

- **Disc load.** `game_loadLevel` `0x1e55c0` was supplied in the probe and is
  Unknown (`exits-r4-travel.md:85`, `:125`). The existing loader path stays as
  it was; no new disc reads.
- **Free-spot geometry.** What the original search reads from geometry or
  `g_randSeed` is Unknown (`:99-100`, `:126`). `FindSpot` stays a caller
  hook. The probe saw integer registers `0x24`/`0xe`. The client passes the
  same numbers as float bit patterns (36.0, 14.0) to its own hook. That hook
  contract is unchanged, and no arrival coordinate parity is claimed.
- **Object-writer bytes.** The probe supplied the body of `0x218600`. Retail
  object, decal and explored-map bytes are Unknown (`:41-43`, `:127`).
  `LevelStates::blobs` stays owner serialization. A key with no bytes now
  refuses on restore.
- **Scripts on entry.** Script bodies are Unknown (`:127-128`;
  `exits-r3-handoff.md:59-60`). The `preStart`/`start`/post-action hooks are
  unchanged, and the live controller keeps its Automatic Trigger and
  mission-script refusals.
- **Both arrival refusals stay.** `session_controller.cpp` keeps "Portal arrival
  and JumpGate prompt have no direct script action in this host" and "Portal
  arrival refused: destination constructors, post-constructor seed,
  blocking-object grid and live actor facing are unavailable." Player and
  destination constructors, menu states 0–4 and the final live arrival are
  Unknown (`exits-r4-travel.md:125-129`; `exits-r3-handoff.md:157-159`).
- **Observed but not carried in this round, because they were outside the
  requested list:**
  - the `0x4f7118 = 1` store in the clear block (`:72`);
  - the voice word `0x505b00` → `0x506058 = 20` (`:112-114`);
  - the medal restore gate on `0x4f713c` and `0x4f85c0` (`:62-64`). The
    existing `restore_medal_player_if_saved` event is unchanged; the probe never
    entered that restore.
- **Card word versus card state.** The store reads the word `0x88d3a0`. The
  `None` write uses the supplied `0x217550`. The client still passes one
  `card_state` to both; whether they can differ was not run.
- **Empty act-row names.** The reference skips them (`ref_exit_travel.py:63`),
  but no probe case contained one. The three `find_act` copies are left as
  they were.
- **Round-three handoff contracts** (768 prompt cases, 800 host cases, 144
  requests, 7 orientation cases, 6 side-buffer cases;
  `exits-r3-handoff.md:185-191`). These were carried by the earlier play-exits
  work (`orientation_from_props`, `new_world_gate`, `PushTrigger::prompt`,
  `Pending`). No contract change was needed here, and those tests pass
  unchanged.

## Gates (Mac mini)

Evidence is under ignored `work/play-exits-travel-r1/` (mini home path replaced
with `~/`). Scripts: `build.sh`, `tests.sh`, `godot.sh`, `compare.py` there.
The rsync was the requested command plus `-c`. Before it, the mini directory
was seeded locally from the mini's `playable-integration-w38` tree because the
link ran below 40 KB/s. The checksum pass then made the tree identical to this
worktree: 1,573 files, 22 transferred, with `--delete`. `godot/.godot` was
absent, so one headless import runs before the matrix. Mini load average was
about 30–38 throughout, from other agents.

1. **Release build + GDExtensions: PASS.** Native Release configure/build
   exit 0 (2 s + 118 s, `-j 4`; no warnings in the changed files).
   `mac-template_debug` / `mac-template_release`, `level-texture` debug and
   release, and `creature_anim` template_debug all exit 0, parallelism 4.
   `lipo -archs` gives `x86_64 arm64` for all five libraries
   (`build-results.txt`). My script's final listing loop stopped on a zsh
   no-match glob after every build step had passed, so the listing was
   recorded by hand.
2. **Focused tests: PASS, 11/11** (24 s): `level-flow-exact` (10,525 exact
   checks), `level-flow-water-adapter`, `level-play`, `quests`,
   `script-commands`, `mission2-actors`, `campaign-actors`,
   `campaign-fight-adopt`, `campaign-fight-boundaries`, `campaign-rewards`,
   `player-death-flow` (`ctest-focused.out`).

3. **Full CTest `-j 3`: PASS, 259/259, 1,704 s**, exit 0, stdin closed
   (`ctest.out`). 259 tests registered, the same as wave 37. The run took longer
   than wave 37's 694 s under a mini load average of about 30–38
   (`netplay` 402 s, `player-exact` 179 s).

4. **Strict Godot matrix: 54/54 PASS**, exit 0, `--captures --timeout 1200`,
   295 s, after one headless import (exit 0, 25 s; `editor-import.log`).
   Rechecking every one of the 105 matrix logs with the unchanged `check-log`
   finds 0 diagnostics (`matrix/results.json`, `matrix.out`). Full synthetic
   combat: 2,645 checks, 0 failures.

5. **FULL_SAVE_LOAD: PASS, 300/300.** Continued and uninterrupted
   presentation SHA-256 are identical, `cb2309ab23d3e2906b5b9d4e95e5577c354869fce3d86cce9fe592a0d2c8d573`,
   the same as wave 37 (`matrix/playable_wave_smoke_full_combat.log`).
6. **FULL_TRACE: PASS, unchanged.** 36,402 bytes, SHA-256
   `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`. No
   baseline was edited. The combat trace does not go through
   `LevelStates` / `begin_change`.

GATES_PENDING

## Final

PENDING
