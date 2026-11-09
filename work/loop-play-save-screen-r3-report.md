STATUS: PASS — save-screen round 3 fixes all four findings of audit `play-save-screen-r2` (0 HIGH, 1 MEDIUM, 3 LOW). Every gate is green on the Mac mini: Release build and five universal GDExtensions, focused 6/6, CTest 260/260, strict matrix 54/54, combat 2,645/0, FULL_SAVE_LOAD 300/300, FULL_TRACE unchanged (36,402 bytes, `9c7922b2…c659`), and journeys step for step equal to round 2 (80/81, pickup 10→11, death-reload refusal kept).

# Play save screen round 3 — 2026-11-09

Branch `codex/play-save-screen`, round 2 tip `69931140`, audit
`cursor-runs/audits/play-save-screen-r2.md` (VERDICT FIX: 0 HIGH, 1 MEDIUM,
3 LOW). Research is the same PASS pin as rounds 1 and 2:
`t3/research-save-screen` `c8895e2` (`docs/save-screen.md` = doc,
`tools/difftest_save_screen_flow.py` = flow, `tools/ref_save_screen.py` = ref,
`tools/difftest_save_screen.py` = dt). No push or PR.

## Method

Same setup as round 2. All builds, tests, Godot runs and journeys ran on the
Mac mini in `~/malik-champions/open-champions/worktrees/play-save-screen`,
which is its own small shallow repository (round 2's report, "Method"). The
four source files were edited on the MacBook, then copied with
`rsync -R -rlp --checksum` (no `-t`) and `touch`ed on the mini. SHA-256 of all
four matched on both machines before the build. Every commit was made on the
mini and brought to the MacBook with `git fetch` plus `git merge --ff-only`.
One build root `build/` (`build/native`, plus `build/gdx` reused one extension
at a time by the round-2 script), `-j 4`, `ctest -j 3`. Logs stay in the
ignored `work/pss3/` on the mini. Mini home paths are written as `~/` here.

Test-edit rule: no test or baseline that existed before this branch changed.
`native/tests/memcard_screen_tests.cpp` is the branch's own test. Round 3
only adds to it: four new cases, one helper, and one added assertion after
the existing `card_error_living` `bad` refusal. No existing line was changed
or removed (`git diff 69931140 -- native/tests` has 0 deletions).

## Findings → fixes → evidence

| ID | Sev | Fix (native/src/memcard_screen.cpp unless noted) | Research line it implements or refuses | Test evidence (`memcard_screen_tests.cpp`) |
|---|---|---|---|---|
| PSS2-1 | MED | New private `Screen::reset_save_links()` clears `saved_step_armed`, `saved_box_dismissing` and `image_built_`. It is called on every path that starts, restarts or leaves a save: `request_save` opening card state 3, the committed slot (`call`, beside `w.step = 0`), step 0, the card-error divert to step 9, the error-box cancel, the saved box's close, and the load path (`request_load`, `accept_load`, `load_read_complete`). Disclosed in `docs/SAVE_SCREEN.md` "Native sequencing". | doc:231, 234-236; flow:220-252, 561-574, 583, 615-621; `docs/issues/native-soak.md:547` | `aborted_arm_resave`: arm step 3, wait, card error → step 9 (arm cleared), cancel; the same `Screen` re-enters step 3 at fade 0 with 0x4f8f29 still up → Unknown, step and fade unchanged; after the host clears it, step 3 arms again with −16 and only then enters 4. A confirmed saved box aborted by a card error does not close a later box at fade 0. `request_save`, the commit and step 0 each clear links the host left planted. `stale_build_then_load`: step 1 builds; (a) step-2 card error, cancel, load, planted step 2; (b) no divert, load, planted step 2; (c) step-2 card error, another buffer, planted step 2 — all Unknown with no op queued and the loaded bytes untouched |
| PSS2-2 | LOW | Code fixed to match the doc: the nonzero-scroll refusal now covers bit 0x80 as well as the repeat (it sits before the branch split). | dt:21-46, 334 (0x80 executed with scroll 0, row 0 only) | `delete_with_scroll`: scroll 1, row 0, count 2, 0x80 → Unknown, slot state, 0x879e24, 0x88d400, 0x4f8ef4 and the fade step unchanged |
| PSS2-3 | LOW | The step-9 cancel checks `counted_living >= 0` (when it will call `memcard_requestSave`) before storing −1. | doc:237-251 | `card_error_living` `bad` case gains one assertion: card state still 3, step 9, stage 1 after the refusal |
| PSS2-4 | LOW | Took the two words, as the original reads them. `Screen::call(edge, repeat)` passes both to the slot list; states 4, 5 and 9 throw Unknown before any store when they differ. State 4 with the free row armed (0x4f8ef4 = 7) reads no mask and was executed with edge 0 and repeat 0x40, so it is not refused. The screen steps read the edge word only, as flow:234 and :258 planted only that word. The one-word `call(edge)` remains as a native convenience that passes the same word as both, and is listed under "Native sequencing, disclosed". | doc:177-179 (edge +0x0c, repeat +0x10, 0x21c478-0x21c4ac); dt:33-34 (both planted equal); flow:377-391 (armed free row, edge 0, repeat 0x40); flow:234, 258 | `two_words`: equal words act as before; split words in state 4 (0x80/0, 0/0x204000, 0x40/0), state 5 and state 9 → Unknown with nothing written; armed free row with edge 0 and repeat 0x40 queues operation 8 and enters 7; step 4's confirm with repeat 0 starts the dismiss |

Mutation check (`work/pss3/mutate.py`, `mutations.log`): each fix was
removed in turn on the mini, the test target rebuilt, and the test run.
All six mutants fail: reset made a no-op (fails at check 170), the divert's
reset removed (170), the three load-path resets removed (191, case b), the
0x80 scroll check removed (196), the cancel order reverted (163), and the
two-word check removed (199). The source was restored and rebuilt afterwards
(211 checks, clean tree). The first run printed stale results for two mutants
because `make` missed a same-second mtime change; the logged rerun spaces
the writes 1.2 s apart.

`memcard-screen` grew from 166 to 211 checks. Docs: `docs/SAVE_SCREEN.md`
updates the list-input row (both words, the 0x80 scroll refusal) and the
cancel row (no partial write), adds the link-reset and two-word items to
"Native sequencing, disclosed", and adds a round-3 "Refused" table. No disc
text: no new lookup key, string or identifier. The module still has no
caller outside itself, its test and its CMake line.

## Gates (Mac mini)

1. **Release build -j 4: PASS.** `cmake -S native -B build/native
   -DCMAKE_BUILD_TYPE=Release` (exit 0) on a fresh `build/`, then
   `cmake --build build/native -j 4`, exit 0 in 440 s
   (`work/pss3/configure.log`, `build.log`). 10 warnings, the same count as
   round 2's full build, none in `memcard_screen*`. `df -h ~/malik-champions`
   showed 49–51 GB free before each build.
2. **Focused tests: PASS, 6/6**, 49.07 s: `ctest -R
   '^(memcard-screen|frontend-exact|ps2-save|ps2-class-restore|menus-exact|native-save)$'
   -j 3`, standard input closed (`work/pss3/focused.log`). `memcard-screen` 211
   checks (round 2: 166). 260 tests registered (unchanged).
3. **Full CTest: PASS, 260/260**, `ctest --test-dir build/native --output-on-failure
   -j 3`, standard input closed, 1,945.11 s, exit 0 (`work/pss3/ctest.out`). Baseline
   260/260. No test timed out, and none needed a rerun. It ran next to the GDExtension
   builds, the matrix and the journeys (two heavy jobs at most) on a mini at load average
   about 60–90 from other jobs. `netplay` took 310.62 s. `memcard-screen` passed.
4. **GDExtensions: PASS.** The round-2 script with logs in `work/pss3`
   (`~/malik-champions/runtime/godot-cpp`, `-DCMAKE_BUILD_TYPE=Release`,
   `CMAKE_OSX_ARCHITECTURES=arm64;x86_64`, `-j 4`, one reused `build/gdx`,
   removed at the end): archive and level-texture `template_debug` /
   `template_release`, creature-animation `template_debug`, all PASS, 1,017 s
   wall. `lipo -archs` reports `x86_64 arm64` for all five libraries, each
   rebuilt this run (06:04–06:14) (`work/pss3/gdext.out`, `*-configure.log`,
   `*-build.log`). It ran next to the native build, then next to CTest (two heavy
   jobs at most).
5. **Strict Godot matrix: PASS, 54/54**, exit 0, 260 s, on the first run.
   Before it, one `--headless --path godot --import` (exit 0, 6 s,
   `work/pss3/godot-import.log`) was run, as round 2's audit asked. The cache
   was already present. The import changed no tracked file. It left one
   untracked Godot `godot/tests/mission1_real.gd.uid`, which is not committed
   and was deleted after the Godot runs. `python3 -u tools/godot_matrix.py matrix
   --godot ~/malik-champions/runtime/Godot.app/Contents/MacOS/Godot --out
   work/pss3/matrix --captures --timeout 1200`, standard input closed
   (`work/pss3/matrix.out`, `matrix/results.json`). The unchanged `check-log`
   finds 0 errors in all 105 logs (`work/pss3/check-log.out`). Case names, order
   and status are identical to wave 38's `results.json` (saved in round 2 as
   `work/pss2/w38-matrix-results.json`) and to round 2's run. Nothing was
   filtered or allowlisted.
6. **Combat: PASS, 2,645 checks, 0 failures** (`PLAYABLE_PLAY_RESULT`,
   `matrix/playable_wave_smoke_full_combat.log`), the same as wave 38 and round 2.
7. **FULL_SAVE_LOAD: PASS, 300/300.** Continued and uninterrupted presentation
   SHA-256 `cb2309ab23d3e2906b5b9d4e95e5577c354869fce3d86cce9fe592a0d2c8d573`,
   identical to each other and to round 2 and waves 37/38.
8. **FULL_TRACE: PASS, unchanged**, 36,402 bytes, headless, SHA-256
   `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`. No baseline
   was edited.
9. **Journeys: PASS, step for step equal to round 2 (and so to wave 38).**
   Unchanged `tools/run_loop_playthrough.py --godot … --game-folder
   ../../private-validation/rta-disc --out work/pss3/playthrough` via round 2's
   `journey.sh` with logs in `work/pss3`, serial headless then rendered, standard
   input closed, read-only disc, after the import above. Wrapper exit 1 (as in
   every wave), 201 s (`work/pss3/playthrough.out`,
   `work/pss3/playthrough/*/result.json`; the 38 rendered PNGs stay in the
   mini's ignored `work/` and are not tracked). Disc files 994 before and 994
   after.

   | Variant | Steps | Ending | Pickup | Captures | Refusal kinds (counts) |
   | --- | --- | --- | --- | --- | --- |
   | Headless | 80 | `player_death` refused; `saved_level_reloaded` false | **10→11**, produced/walked/picked up, 52 walk ticks, 0 retries | 0 | 6, equal to round 2 |
   | Rendered | 81 | `player_death` refused; `saved_level_reloaded` false | **10→11**, produced/walked/picked up | 38 | 6, equal to round 2 |

   Compared index by index with round 2's `work/pss2/playthrough/*/result.json`:
   every step name and result is identical in both variants, the `refusals`
   map (kind and count) is identical, and every refused row is identical.
   Besides the host-only `fps` and `memory_bytes`, three fields differ:
   - Headless step 18 `dialog_choice_close` `choices_taken` is 4 (round 2: 5;
     wave 38: 3; wave 37: 5). Step 27 `hub_trigger_mission_menu`
     `script_executed_commands` follows it (`addQuest` 2 vs 3). Round 2's report
     records these counts moving between waves 37 and 38 on the same native code,
     from the close-fade dialog retry. Nothing on this path calls the memcard
     module.
   - `enemy_fight` `swing_wall_msec` (wall-clock time on a loaded mini) in both
     variants.

## Remaining

Same as round 2. No host card directory source binds the contract, so `Flow`
still uses host files and PS2 import. These stay refused pending research:
the state-4 cancel back fade, the delete confirm, combined presses, the
repeat and 0x80 outside the settled fixture (now including 0x80 with a
scroll), split edge and repeat words, the saved-box key split (needs the
0x8e1ad8 writer), a stale 0x4f8f29, net-path step 2, count 0 with a named
directory, a negative countdown, load steps 0/1/3, the fade integration, and
the writers of 0x88d3b0 and 0x8e1ad8. Death reload stays refused. The mini
`build/` was deleted at the end. No push, PR, sibling edit, private-disc
write, install, `/tmp` use or process-name kill. Only PIDs this run started
were waited on.
