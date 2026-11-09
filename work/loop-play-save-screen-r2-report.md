STATUS: IN PROGRESS — save-screen round 2 fixes all ten findings of audit `play-save-screen-r1` (0 HIGH, 5 MEDIUM, 5 LOW). Gates below are filled as they finish on the Mac mini.

# Play save screen round 2 — 2026-11-09

Branch `codex/play-save-screen`, round 1 tip `24c80514`, audit
`cursor-runs/audits/play-save-screen-r1.md` (VERDICT FIX). Research is the
same PASS pin as round 1: `t3/research-save-screen` `c8895e2`
(`docs/save-screen.md` = doc, `tools/difftest_save_screen_flow.py` = flow,
`tools/ref_save_screen.py` = ref, `tools/difftest_save_screen.py` = dt), audited
PASS in `research-save-screen-r2-fix.md`. No push or PR.

## Method

All builds, tests, Godot runs and journeys ran on the Mac mini, in
`~/malik-champions/open-champions/worktrees/play-save-screen`. The mini's bare
`repo.git` is shallow and lacks this branch's base, so a bundle could not be
fetched into it. Instead the mini worktree is its own shallow git repository at
`24c80514` (one pack of the 104 objects of that tree that `repo.git` lacks,
with `repo.git/objects` as a read-only alternate; `repo.git` itself was not
written). Every round-2 commit was made there and brought to the MacBook
worktree with `git fetch` from that repository plus `git merge --ff-only`.
One build root `build/` (`build/native`, plus one reused `build/gdx` for the
extensions, removed after each), `-j 4`, `ctest -j 3`. Logs stay in the
ignored `work/pss2/` on the mini; mini home paths are written as `~/` here.

Test-edit rule: no test or baseline that existed before this branch was
touched. `native/tests/memcard_screen_tests.cpp` is this branch's own new
test; two of its round-1 lines were changed because the audit directs it
(PSS-2 fixture, PSS-5 self-checking assertion). Every other change to it is
an added case.

## Findings → fixes → evidence

| ID | Sev | Fix (native/src/memcard_screen.cpp unless noted) | Research line it implements or refuses | Test evidence (`memcard_screen_tests.cpp`) |
|---|---|---|---|---|
| PSS-1 | MED | State 4 reads 0x204000, 0x40, 0x80 and 0x20 one at a time. 0x80 on a populated row enters 9 (0x879e24, 0x88d400 untouched). The repeat mask (either bit) sets 0x879e24 to the selection + 1 when another cached slot exists. New state 9: no press stays, 0x20 returns to 4. **Refused (Unknown):** state-4 cancel 0x20 (no probe ran it; its back-fade stores are unrecorded), any combined press (so 0x40\|0x4000 is no longer a confirm), repeat/0x80 with an unsettled cursor, on the load list, with a scroll, the repeat on the last row, 0x80 off a populated row, and state 9 with any press but 0x20. | doc:169, 171; ref:115-132; dt:21-46 (settled save fixture, count 2, row 0), dt:334-337 (executed `(4,0)`, `(4,0x40)`, `(4,0x80)`, `(4,0x204000)`, `(9,0)`, `(9,0x20)` only) | `state4_inputs`: (4,0x80)→9; (9,0) stays; (9,0x20)→4; (4,0x204000)→0x879e24 = 1 and nothing else written; each bit alone; 0x20, 0x40\|0x4000, 0x40\|0x200000 on the free row, 0x40\|0x80, unsettled, free-row 0x80, load list, last row, scroll, state-9 0x40/0x80 all Unknown with nothing written |
| PSS-2 | MED | Step 4 throws Unknown when `(0x8e1ad8 != 0) != (index == count)`. | flow:562, 581-586 (byte clear, index 1 ≠ count 2 → `OverwriteGood`); flow:181, 591-599 (byte set, index 0 = count 0 → `MemcardSaveGood2`); doc:228 (0x8e1ad8 writer Unknown) | `saved_box` new-row fixture now sets `fresh_name = 1` like flow:591; `saved_key_discriminator`: both split combinations Unknown, the agreeing occupied case gives `OverwriteGood` |
| PSS-3 | MED | Step 3's arm and wait are linked by the native flag `Words::saved_step_armed`, disclosed in `docs/SAVE_SCREEN.md`. Entering step 3 with 0x4f8f29 already set throws Unknown. The wrong comment is replaced with the citation. | doc:231; flow:220-229, 583 (byte 0 on entry); `docs/issues/native-soak.md:547` (`Savepoint_run` clears the flag), `:572` (the flag stays up); `docs/monsters.md:1064-1066` | `saved_flag_stale`: stale byte Unknown and step stays 3; the arm sets the native flag; fade 0 enters 4 without clearing 0x4f8f29; a second save in the same `Screen` is Unknown until the host clears the byte, then arms again |
| PSS-4 | MED | Step 2 throws Unknown on the network byte, and offline unless step 1's `save_build` filled the buffer during this save (private `image_built_`). The size refusal stays first. | doc:229; flow:602-608 (net path executed to step 2's entry only); flow:561-574 (offline step 2 after `save_build`) | `net_step2`: net step 1 → 2, then step 2 Unknown with no op queued over a planted old buffer; planted offline step 2 without a build Unknown; `save_write` (build then write) still passes |
| PSS-5 | MED | Test only: `save_build` returns a 0x4bc78-byte pattern the module cannot produce; the test asserts `w.image` equals it. The level-name claim is dropped (it belongs to the `save_build` port). | doc:229 (the level name is `save_build`'s output) | `save_write`: "step 1 stores save_build's return in 0x895580" |
| PSS-6 | LOW | Test only: overwrite on row 1 of 2 with 0x88d3b0 planted to 7. | doc:202 (scroll + row); disclosed link 2 | `overwrite_row_one`: commit writes 1 over 7 |
| PSS-7 | LOW | The flag-1 branch of `load_read_complete` stores `latch = 1`. | doc:267-268; flow:473 (0xa5 planted), :687 (latch 1) | `load_latch_flag1`: 0xa5 re-planted before step 4, latch 1 after |
| PSS-8 | LOW | New `Words::counted_living` (host-supplied, like `num_players`); the error-box cancel passes it to `request_save`. | doc:238-240, 246-251; flow:248 (`living(h.e)` in every corner), :624 (0x88d3d8 still 1) | `card_error_living`: counted 1, offline and net, request called and closed by stage 1; negative count refused. The original four-corner `card_error` is unchanged and still passes |
| PSS-9 | LOW | `make_slot_name` requires an empty directory for every count, including 0. | doc:228; flow:181-185 (set byte: prefix at 0x88d061, name byte 0x88d060 clear), :549-551 (clear byte, nothing planted) | `slot_name_named_count0`: count 0 with a named record Unknown, set and clear byte; the probe's 0x88d061 prefix layout still gives slot0 |
| PSS-10 | LOW | State 7 throws Unknown on a negative 0x88d3c4. "The host runs 0x88d3c4 down" added to "Native sequencing, disclosed". | doc:208-209; flow:350, 374, 395 (only 10 and 0 planted) | `countdown_negative` |

`memcard-screen` grew from 118 to 166 checks. Docs: `docs/SAVE_SCREEN.md`
gains the list-input row, the corrected slot-name/write/saved-box/cancel/load
rows, two disclosed native links (step-3 flag, host countdown) and a round-2
"Refused" table with the reason for each. No disc text in tracked files: only
the card identifiers, lookup keys and research level names round 1 already
used. Nothing outside the module, its test and its doc changed, so the
`Flow`, the session save/load path, the GDExtensions and the Godot project are
untouched.

## Gates (Mac mini)

1. **Release build -j 4 + GDExtensions: PASS.** `cmake -S native -B build/native
   -DCMAKE_BUILD_TYPE=Release` (exit 0), `cmake --build build/native -j 4` of the
   round-1 tree exit 0 in 448 s, then the incremental rebuild with the fixes exit 0
   in 17 s with no warning (`work/pss2/configure.log`, `build-base.log`, `build.log`;
   the 10 warnings in the full build are pre-existing files, none in `memcard_screen*`).
   GDExtensions with `~/malik-champions/runtime/godot-cpp`, `-DCMAKE_BUILD_TYPE=Release`,
   `CMAKE_OSX_ARCHITECTURES=arm64;x86_64`, `-j 4`, one reused `build/gdx`: archive and
   level-texture `template_debug` / `template_release`, creature-animation
   `template_debug`; all PASS, 1,229 s wall, and `lipo -archs` reports `x86_64 arm64`
   for all five libraries (`work/pss2/gdext.out`, `*-configure.log`, `*-build.log`).
2. **Focused tests: PASS, 6/6**, 49.29 s: `ctest -R
   '^(memcard-screen|frontend-exact|ps2-save|ps2-class-restore|menus-exact|native-save)$'
   -j 3`, standard input closed (`work/pss2/focused.log`). `memcard-screen` 166 checks.
   Registered tests: 260 (unchanged; no test added to CTest this round).

3. **Full CTest: PASS, 260/260**, `ctest --test-dir build/native --output-on-failure
   -j 3`, standard input closed, 2,174.28 s, exit 0 (`work/pss2/ctest.out`). Baseline
   260/260. No test timed out, so no rerun was needed. It ran next to the GDExtension
   builds and then the Godot matrix (the second heavy-job slot) on a mini at load average
   about 80–120 from other jobs. `netplay` alone took 278.95 s. `memcard-screen` passed
   in 0.01 s.

GATES_REST
