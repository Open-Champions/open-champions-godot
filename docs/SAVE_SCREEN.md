# Save/load screen play flow — confirm, write, and load handoff

`native/include/champions/memcard_screen.hpp` (`champions::frontend::memcard`)
is a word-level contract for the Return to Arms US v2.00 save and load
screens. It is authored only from `t3/research-save-screen` `c8895e2`
`docs/save-screen.md`, "Play flow" (lines 181–325), and its probe
`tools/difftest_save_screen_flow.py`, audited PASS by
`research-save-screen-r2.md` and `research-save-screen-r2-fix.md`. Every field
names the address it stands for. A branch the research did not execute throws
`memcard::Unknown` (a `frontend::Refusal`) instead of guessing. Card I/O is
returned to the host as queued `CardOp`s. No card bytes, localized text,
pixels or retail card behaviour are held.

This module is a contract, not a host card. The existing front-end `Flow`
save/load actions (native session files and PS2 slot import,
`docs/FRONTEND.md`) are unchanged, and no host directory source binds it
yet.

## What is implemented

Research citations are `docs/save-screen.md` line numbers at `c8895e2`.

| Behaviour | Native | Research |
|---|---|---|
| Settle | State 4 copies a mismatched 0x879e30 over 0x879e2c. Float bits 0x3f800000 at 0x879e34 snap: the float is cleared and 0x879e28 takes 0x879e24. Confirm is read only when the float compares equal to 0 and the two row words match; 0x3f000000, or unequal words, leave slot state, 0x4f8ef4 and the fade step alone. | 100–101, 193–200, 206–207 |
| Free row vs overwrite | Save screen (card state 3): scroll + row == cache count stores 7 at 0x4f8ef4 and fade step −16; at fade 0 it queues operation 8 and enters 7. A populated row enters 5; 5 takes 0x20 back to 4 or 0x80 to 11 and 0x88d400. 11 queues 0x0a on `/BASLUS-20973` and enters 12. 12, pump idle, 0x4f9540 ≥ 0 and fade 0, returns 1 and clears the slot state. | 167–172, 202–221 |
| List inputs | State 4 reads one press at a time. 0x40 is the confirm above. Bit 0x80 on a populated row enters 9 and leaves 0x879e24 and 0x88d400 at 0. The repeat mask 0x204000 (either bit) moves 0x879e24 to the selection + 1 when another cached slot exists. Both run only with the cursor settled, scroll 0 and card state 3; either with a nonzero 0x879e2c throws. State 9 stays on no press and returns to 4 on 0x20. `Screen::call(edge, repeat)` takes the owner's edge (+0x0c) and repeat (+0x10) words (lines 177–179); states 4, 5 and 9 throw when they differ, except state 4 with the free row armed, which reads no mask. | 169, 171, 177–179; `ref_save_screen.py:115-132`; `difftest_save_screen.py:21-46, 334-337`; flow 377–391 |
| Scroll/cache count | Visible row index is 0x879e2c + 0x879e28 against 0x4f9238. | 91–104, 202–207 |
| Free space | State 7 waits for 0x88d3c4, then with a positive fade step compares 0x88d3ac with 0x88d3ec + (count & 1): short looks up `MemcardNeedFreeSpaceBrowser` and leaves the fade, enough sets −16. With the fade at 0, 7 becomes 12. | 208–216 |
| Slot names | Fresh (0x8e1ad8 set, or index == count), with an empty directory (every record's name byte clear): count 0 gives `BASLUS-20973_slot0`; a clear byte and count 1–3 give `BASLUS-20973_slot1`. Otherwise the name is copied from 0x88d060 + index×64. | 228 |
| Write | Offline step 1 calls `save_build(slot byte 0x4f923c, 0)`; the recorded pair is (3, 0) with `g_numPlayers` 5, and the returned image is kept in 0x895580. Step 2, right after that build, queues open 0x202, one 0x4bc78-byte write, close and file info. | 229–230 |
| Net-byte path | 0x402791 set calls `save_packLocalPlayer` instead of `save_build`, then step 2. Step 2 itself on this path is refused (below). | 229 |
| Saved box | Step 3, entered with 0x4f8f29 clear, sets it and −16, then step 4 at fade 0. Step 4 looks up `MemcardContinue`, `MemcardSaveComplete`, and `MemcardSaveGood2` (new row: 0x8e1ad8 set and index == count) or `OverwriteGood` (occupied: byte clear and index ≠ count). Confirm sets −16; at fade 0 the latch is cleared and the card state stored −1. Keys are identifiers, never displayed text. | 231–232; flow 562, 581–599 |
| Latch lifecycle | `request_load` with 0x88d3c8 set returns before any store (0xa5 survives). The opening call clears the latch (0x217760). Load acceptance with the pump returning 0 stores 1 (0x2195a8). | 31, 251–263 |
| Error path | A nonzero 0x88d3bc moves to step 9 and closes the file. The latch and card state 3 are kept. | 234–236 |
| Cancel | The error box cancel stores −1 (0x21f254). It calls `memcard_requestSave` with the host's counted-living total (`Words::counted_living`; the probe planted a living player, flow 248) unless the network byte and a pending level name are both set; while 0x88d3d8 is 1 that request does not open. A negative total is refused before the −1 store, so the refusal writes nothing. | 237–251 |
| Directory copy | Load acceptance copies all 0x40 bytes from 0x88d040 + index×64 to 0x88d2c0. Step 0, stage 1, fade step 16. | 258–263 |
| Load | Step 2 queues open mode 1 of the record's name, a 0x4bc78-byte read and close. Step 4 stores the bytes; with 0x8e1ab9 0 it clears the latch, stores −1 and step 0. With 1, the latch is stored 1 (267–268) and `save_load` requests the header's level: `strife`, or `hub` + `west` as `hub_west`, in 0x41b6d0 and 0x8e1a98; marker 0xfd; 0x8e1ad6 1; live name untouched; image directory bytes untouched; step 5. Step 5 waits until the live qualified name matches, then stores −1 and clears the latch. | 263–286 |

Object records stay with the existing save-world loader
(`t3/research-save-world` `e7d8972` `docs/save-world.md` 81–91). The optional
`Services::restore` hook receives the read image for the existing loaders; the
module never calls `world_loadRecords`, `level_loadScriptAndObjects` or
`save_reload`. Death reload keeps `blocked_death_reload`
(`docs/PLAYER_DEATH_FLOW.md`).

## Native sequencing, disclosed

These link executed outcomes where the research shows both ends but not the
word between them:

- The fade is the host's: `fade_level` is read after the host applies the frame's
  fade. How 0x8e11fc is integrated into 0x8e11f8 is not in the research.
- On a committed save slot the native sets 0x88d3b0 to the row it compared
  (scroll + row), sets the latch (Observed at 0x218ba4, `docs/save-screen.md:31`)
  and enters stage 1, step 0. The probes planted 0x88d3b0.
- Offline step 1 advances to step 2 like the executed net path; the offline
  probe planted step 2.
- The saved box's confirm and close are linked by a native flag.
- Step 3's arm and its wait are linked by a native flag (`saved_step_armed`),
  not by 0x4f8f29. That byte is cleared by `Savepoint_run`, not by the screen,
  and can stay up across saves (`docs/issues/native-soak.md:547, 572`;
  `docs/monsters.md:1064-1066` on the same research pin). Step 3 entered with
  the byte already set throws; the host clears it the way `Savepoint_run` does.
- Every path that starts, restarts or leaves a save clears the three native
  save links (`saved_step_armed`, `saved_box_dismissing` and the private
  "step 1 built this image" flag): `request_save` opening card state 3, the
  committed slot, step 0, the card-error move to step 9, the error-box cancel,
  the saved box's close, and the load path (`request_load`, `accept_load`,
  `load_read_complete`). An aborted step cannot carry its arm, its dismiss or
  its build into a later save (audit `play-save-screen-r2` PSS2-1).
- One input word for both. The slot list reads two owner words, edge +0x0c
  and repeat +0x10 (`docs/save-screen.md:177-179`, 0x21c478–0x21c4ac). The
  probe planted the same value in both (`difftest_save_screen.py:33-34`), so
  which word each mask is tested against was not separated.
  `Screen::call(edge, repeat)` takes both and throws in states 4, 5 and 9 when
  they differ. The one-word `Screen::call(edge)` is a native convenience that
  passes the same word as both, the way the probe planted them. The screen
  steps read the edge word only; the flow probe planted only that word for
  steps 4 and 9 (flow 234, 258).
- The host runs the countdown 0x88d3c4 down. The research shows state 7
  waiting for it (`docs/save-screen.md:208-209`), not who decrements it; a
  negative value throws.
- Load steps 0, 1 and 3 were not executed; the host calls `queue_load_read()`
  after acceptance and `load_read_complete()` with the card's bytes.
- The load list's own confirm path was not executed (its acceptance ran with
  the list stubbed); `accept_load()` stands for that return of 1.

## Refused, with reasons

| Refused | Reason |
|---|---|
| Free-space units | What 0x88d3ac and 0x88d3ec count is Unknown (`docs/save-screen.md:216`); only their compare is ported. |
| Why slot 1 | Why a nonempty cache count with an empty directory names `_slot1` is Unknown (line 228; audit RSS-3). Counts above 3, a nonempty directory, or a set 0x8e1ad8 with a nonzero count throw. |
| 0x8e1ad8 writer | Who sets the fresh-name byte is Unknown (line 228); the host must supply it. The product prefix at 0x88d061 was supplied by the probe. |
| Pixels | Text widths, fades and GPU output are stubbed in every fixture (lines 56–58, 321). |
| Localized strings | Only lookup keys are used; their text is Unknown (lines 162–163, 321). |
| Retail card I/O | `memcard_browser` 0x219b90, the card queue and pump, real directory timestamps and a retail capture are not run (lines 27–29, 321–323). A busy pump throws. |

Also unexecuted and refused: slot state 12 with the network byte, a negative
0x4f9540, slot stages other than 0/1, save steps other than 0–4 and 9, a
confirm past the free row, 0x8e1ab9 other than 0/1, and an empty header level.

Refused in round 2 (audit `play-save-screen-r1`), each because the research
did not execute it:

| Refused | Reason |
|---|---|
| State-4 cancel 0x20 | The back fade at 0x21ccc0 is named (line 169) but no probe ran (4, 0x20) (`difftest_save_screen.py:334-335`); its stores are not recorded. |
| Combined state-4 presses | Each of 0x204000, 0x40 and 0x80 was executed alone. 0x40 with a repeat bit, or any two of the four masks, throws instead of acting as a confirm. |
| Repeat / 0x80 outside the fixture | Unsettled cursor, load list, nonzero scroll, the repeat on the last cached row, and 0x80 off a populated row (`difftest_save_screen.py:21-46`). |
| Delete confirm | State 9 with 0x40 (the fade and queue path 0x21d360–0x21d3c0) or any press but 0x20 (line 171; only (9, 0) and (9, 0x20) ran). |
| Saved-box key split | 0x8e1ad8 clear with index == count, or set with index ≠ count: the two discriminators agreed in both executed cases (flow 562, 581–599). The ordinary free-row save needs the 0x8e1ad8 writer, which is Unknown (line 228). |
| Stale 0x4f8f29 | Step 3 was executed with the byte at 0 on entry (flow 220–229, 583). |
| Net-path step 2 | What 0x895580 holds after `save_packLocalPlayer`, and step 2 on that path, were not run (line 229; flow 602–608). Offline step 2 without this save's `save_build` also throws (flow 561–574). |
| Named directory with count 0 | Every count-0 case had record 0's name byte clear (flow 181–185, 549–551). |
| Negative 0x88d3c4 | Only 0 and 10 were planted (flow 350, 374, 395). |

Refused in round 3 (audit `play-save-screen-r2`):

| Refused | Reason |
|---|---|
| Split edge and repeat words | States 4, 5 and 9 were executed with the two owner words equal (`difftest_save_screen.py:33-34`); which word each mask is tested against is Unknown. |
| Bit 0x80 with a scroll | Round 2 listed it, but the code checked the scroll only for the repeat. Bit 0x80 with a nonzero 0x879e2c now throws too (`difftest_save_screen.py:21-46, 334` ran scroll 0 only). |

## Tests

`ctest -R '^memcard-screen$'` runs `native/tests/memcard_screen_tests.cpp`.
Each case plants the words of one probe in the flow difftest and asserts the
recorded outcome. All inputs are synthetic.
