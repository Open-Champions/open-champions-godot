STATUS: PASS — save/load screen play-flow contract ported from Observed/Executed research only; all seven gates green on the Mac mini. CTest 260/260, strict matrix 54/54, FULL_SAVE_LOAD 300/300 and FULL_TRACE unchanged, journeys 80/81 with pickup 10→11. Not yet driven by the host `Flow` (no card directory source; retail card I/O refused).

# Play save screen round 1 — 2026-11-09

Branch `codex/play-save-screen`, base wave 37 VERIFIED `70d2de19`. No push or PR.
Research `t3/research-save-screen` `c8895e2` `docs/save-screen.md`, audited
`research-save-screen-r2.md` (FIX) then `research-save-screen-r2-fix.md` (PASS, 0/0/0).
Built and tested only on the Mac mini, in an rsynced copy of this worktree
(`--exclude .git --exclude build --exclude work`; `.godot` kept). Private disc read-only,
counts only. Mini home paths in copied logs are rewritten to `~/`.

## What the client had

- `frontend::Flow` (`native/src/frontend.cpp:552-575`, `:758-777`): `LoadGame` / `SaveGame` /
  `Saved` screens whose actions are host files (`.ocplay`/`.ocnsave`) and PS2 slot import.
  `save_latch_` 0x4f8f28 set to 1 on choosing, kept on error, cleared by the saved box.
  `load_from_frontend_` 0x8e1ab8. No slot state, settle, free-row, free-space, slot naming,
  write queue, or load handoff.
- `frontend_layout` + `Flow::set_card_cache` (menus round, `work/loop-save-screen-r1-report.md`):
  the style-2 tile frame, five indexed rows, BCD dates/clocks and the header preview from a
  caller-supplied cache. Drawing only.
- `ps2_save` (`native/include/champions/ps2_save.hpp`): the 0x4bc78 slot image builder/reader,
  `qualified_name`, `apply_load`, and the strict `reload_checkpoint` used by death reload.
- Death reload remains refused (`blocked_death_reload`, `docs/PLAYER_DEATH_FLOW.md:138`).

## What changed

New word-level contract `champions::frontend::memcard` in the `champions-frontend` library:
`native/include/champions/memcard_screen.hpp`, `native/src/memcard_screen.cpp`, CTest
`memcard-screen` (`native/tests/memcard_screen_tests.cpp`, synthetic fixtures, 118 checks),
`docs/SAVE_SCREEN.md`, a pointer in `docs/FRONTEND.md`, and the CMake registration in
`native/frontend_targets.cmake`. Existing `Flow`, session save/load, PS2 import, the death
reload refusal and every test, baseline and fixture are unchanged. `Flow` does not drive the
new contract: no host card directory source exists and retail card I/O is refused.

Observed/Executed behaviour implemented (research `docs/save-screen.md` line, probe
`tools/difftest_save_screen_flow.py` line):

| Behaviour | Native | Research |
|---|---|---|
| Settle | 0x3f800000 at 0x879e34 clears the float and copies 0x879e24 to 0x879e28; confirm is read only at float 0 with equal row words; 0x3f000000 or unequal words leave slot state, 0x4f8ef4 and fade | doc:193-200; flow:335-359, 637-645 |
| Scroll/cache count | 0x879e30 mismatch copied over 0x879e2c; row = 0x879e2c + 0x879e28 against 0x4f9238 | doc:100-101, 202-207 |
| Free row vs overwrite | free row: 7 at 0x4f8ef4, fade −16, at fade 0 op 8 and state 7; populated row → 5; 5 0x80 → 11 (+0x88d400); 11 queues 0x0a `/BASLUS-20973` → 12; 12 returns 1 and clears slot state | doc:167-172, 202-221; flow:361-452, 650-660; ref:115-128 |
| Free space | state 7 waits for 0x88d3c4; fade step > 0: 0x88d3ac < 0x88d3ec + (count & 1) looks up `MemcardNeedFreeSpaceBrowser`, else −16; fade 0 → 12 | doc:208-216; flow:392-439, 652-656 |
| Slot names | fresh (0x8e1ad8 or index == count): count 0 → `_slot0`; clear byte, empty directory, count 1-3 → `_slot1`; else copy 0x88d060 + 64·index | doc:228; flow:176-193, 534-559 |
| Write | offline `save_build(slot byte, 0)`, (3, 0) with `g_numPlayers` 5; open 0x202, one 0x4bc78 write, close, file info | doc:229-230; flow:195-218, 563-575 |
| Net-byte path | 0x402791 → `save_packLocalPlayer`, no `save_build`, step 2 | doc:229; flow:601-608 |
| Saved box | step 3 sets 0x4f8f29 and −16, fade 0 → 4; keys `MemcardContinue`, `MemcardSaveComplete`, `MemcardSaveGood2` / `OverwriteGood` (identifiers); confirm −16, fade 0 clears the latch and stores −1 | doc:231-232; flow:220-243, 583-599 |
| Latch lifecycle | `request_load` with 0x88d3c8 keeps 0xa5; opening stores 0; acceptance with the pump at 0 stores 1 | doc:31, 251-263; flow:286-303, 631-634, 664-668 |
| Error path | nonzero 0x88d3bc → step 9, close file, latch kept, card state 3 kept | doc:234-236; flow:245-252, 619-621 |
| Cancel | stores −1; reopen requested unless net byte and pending level both set; stage 1 keeps it closed | doc:237-251; flow:253-271, 611-625 |
| Directory copy | all 0x40 bytes 0x88d040 + 64·index → 0x88d2c0; step 0, stage 1, fade 16 | doc:258-263; flow:504-523, 664-668 |
| Load | open mode 1, read 0x4bc78, close; 0x8e1ab9 0 → latch 0, −1, step 0; 1 → pending/side `strife` or `hub_west`, marker 0xfd, 0x8e1ad6 1, live name and image directory bytes untouched, step 5; step 5 waits for the live qualified name, then −1 and latch 0 | doc:263-286; flow:454-500, 678-696 |

Object records stay with the save-world loader (only an optional `Services::restore` hook
receives the image). Nothing calls `save_reload`; `blocked_death_reload` is untouched.

Native sequencing between executed ends is disclosed in `docs/SAVE_SCREEN.md` ("Native
sequencing, disclosed"): host-applied fade, the committed row written to 0x88d3b0 (probes
planted it), offline step 1 → 2 (net path executed, offline planted), the saved-box
confirm→close flag, load steps 0/1/3 replaced by explicit `queue_load_read` /
`load_read_complete`, and `accept_load` standing for the load list's return of 1.

Refused with reasons (`docs/SAVE_SCREEN.md` "Refused"): free-space units (doc:216), why
slot 1 (doc:228), the 0x8e1ad8 writer (doc:228), pixels (doc:56-58, 321), localized
strings (doc:162-163, 321), retail card I/O including `memcard_browser`, a busy pump and
real timestamps (doc:27-29, 321-323). Every other unexecuted branch throws `Unknown`.

## Gates (Mac mini)

1. **Release build + GDExtensions: PASS.** `cmake -S native -B build/native
   -DCMAKE_BUILD_TYPE=Release`, `cmake --build -j 4`, exit 0; no warning from the new files
   (`work/pss/configure.log`, `build.log`). GDExtensions with
   `~/malik-champions/runtime/godot-cpp`, `-j 4`: archive and level-texture
   `template_debug` / `template_release`, creature-animation `template_debug`; all exit 0
   and `lipo -archs` reports `x86_64 arm64` for each (`work/pss/*template*.log`).
2. **Focused tests: PASS, 6/6.** `ctest -R
   '^(memcard-screen|frontend-exact|ps2-save|ps2-class-restore|menus-exact|native-save)$'
   -j 3`, 47.39 s (`work/pss/focused.log`); `memcard-screen` 118 checks. Registered tests:
   260 (259 + `memcard-screen`).
3. **Full CTest: PASS, 260/260**, `ctest --test-dir build/native --output-on-failure -j 3`
   with standard input closed, 2,695.71 s, exit 0 (`work/pss/ctest.out`). Wave 37 was
   259/259; the one added test is `memcard-screen`. It ran while the GDExtension build and
   then the matrix used the mini's second job slot, on a mini at load average ~80 from other
   users' jobs, hence the wall time.
4. **Strict Godot matrix: PASS, 54/54**, exit 0. `python3 -u tools/godot_matrix.py matrix
   --godot ~/malik-champions/runtime/Godot.app/Contents/MacOS/Godot --captures --timeout 1200`
   (`work/pss/matrix.out`, `work/pss/matrix/results.json`). The unchanged `check-log` found
   0 diagnostic errors in all 105 logs. A first run was 29/54: the mini copy had been seeded
   from a sibling worktree's `.godot`, which lacked the imported
   `res://assets/open_champions_avatar.png` (58 identical load errors, kept in
   `work/pss/matrix-stale-import.out`). One headless `--import` (`work/pss/editor-import.log`,
   exit 0) fixed the cache; no file, test or allowlist changed before the 54/54 rerun.
   Full synthetic combat: 2,645 checks, 0 failures.
5. **FULL_SAVE_LOAD: PASS, 300/300.** Continued and uninterrupted presentation SHA-256
   `cb2309ab23d3e2906b5b9d4e95e5577c354869fce3d86cce9fe592a0d2c8d573`, identical to each
   other and to wave 37. No change: this round adds a separate native contract and does not
   touch the session save/load path that fixture drives.
6. **FULL_TRACE: PASS, unchanged**, 36,402 bytes, SHA-256
   `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`. No baseline edited.
7. **Journeys: match wave 37.** Unchanged `tools/run_loop_playthrough.py`, serial headless
   then rendered, standard input closed, disc `../../private-validation/rta-disc` read-only
   (`work/pss/playthrough.out`, `work/pss/playthrough/*/result.json`; the 38 rendered PNGs
   stay in the mini's ignored `work/` and are not tracked).

   | Variant | Steps | Exit | Ending | Pickup | Log errors | Captures | `refused` rows |
   | --- | --- | --- | --- | --- | --- | --- | --- |
   | Headless | 80 | 1 | `player_death` refused; `saved_level_reloaded` false | **10→11**, produced/walked/picked up, 52 walk ticks | 0 | 0 | 27 |
   | Rendered | 81 | 1 | `player_death` refused; `saved_level_reloaded` false | **10→11**, produced/walked/picked up, 52 walk ticks | 0 | 38 | 28 |

   Six refusal kinds each; disc 994 files before and after. Wrapper exit 1 is the retained
   full-journey refusal (death reload stays blocked). `fps` and memory figures are host-only.

## Method notes

- The task brief said to edit and commit on the MacBook worktree and sync to the mini; that
  was followed (it supersedes the workflow note's mini-side commits). All commits are local
  to `codex/play-save-screen`.
- The first full rsync was too slow (native vectors ~1 GB), so the mini copy was seeded by a
  local mini rsync from the sibling `play-quest-log` copy (same `70d2de19` base), then the
  instructed `rsync -a --delete --exclude .git --exclude build --exclude work` ran from this
  worktree, with `--exclude .godot` added so the mini's `.godot` was kept as instructed. The
  stale import cache that came with the seed is the matrix first-run failure in gate 4.
- Logs copied into `work/pss/` had the mini's absolute home prefix rewritten to `~/`; only this report is
  tracked. The mini `build/` was deleted at the end.

## Remaining

No host directory source binds the contract; the front-end `Flow` save/load actions remain
the host-file and PS2-import paths. The refused items above, and the native sequencing
listed in `docs/SAVE_SCREEN.md`, need further research (load steps 0/1/3, the fade
integration, the writers of 0x88d3b0, 0x8e1ad8, 0x4f8f29 and of 0x88d3d8 after a save)
before `Flow` can drive a card. Death reload stays refused. No push, PR, sibling edit,
private-disc write, or process-name termination.
