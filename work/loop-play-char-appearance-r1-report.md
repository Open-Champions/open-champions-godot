STATUS: DONE — fix round for audit play-char-appearance-r1 complete (CA-1..CA-6 fixed, re-pinned to research 7931517). Fix-round gates on the mini: build PASS; focused 11/11; full CTest 260/260; strict Godot 55/55; combat 2,645/0; FULL_SAVE_LOAD 300/300; FULL_TRACE unchanged (36,402 bytes, 9c7922b2…); journeys 80/81, pickup 10→11, step/result pairs identical to baseline. See "Fix round 1" at the end. Round-1 text below is kept as recorded; superseded statements are marked.

# Play: created-character appearance, round 1 — 2026-11-09

Branch `codex/play-char-appearance`, worktree
`~/Projects/open-champions/worktrees/play-char-appearance`, from wave 37 VERIFIED
`70d2de19`. Research: `t3/research-char-appearance` at `153d1c3`,
`docs/spec/created-character-appearance.md` (cited `CCA:line`), audits
`research-char-appearance-r2` and `research-char-appearance-r3-fix` both PASS.
Starting-equipment map reused from `t3/research-loading-readiness` `366b65a`
`docs/starting-equipment-r4.md` (`SE4:line`). Builds and tests ran only on the
Mac mini, in an rsynced copy at
`~/malik-champions/open-champions/worktrees/play-char-appearance`. Logs are under
ignored `work/ca-r1/` (mini paths rewritten to `~/`).

## What the client did before this round

- `godot/real_data/real_player.gd` drew the class body's VIF rest pose from the
  selection archive, with the record skin texture only for imported PS2 records
  that carry `+0x20..+0x23`; new characters got a neutral material. Hair and
  beard were never looked up: any non −1 style only appended "Hair/beard
  attachments refused: part draw path unported".
- Body meshes were filtered by each mesh's default-visible bit, not object
  `+0x220`. No equipment part was drawn; `loading_equipment.hpp:21-22,36` has no
  formatter, header search or transfer for the item resources.
- `ModelAttachPoint` decoding existed (`model.cpp` table 1, 16 × `0x34`), and
  `skinning.cpp`/`sample_skinned_resources` reject any model whose header
  `+0x13` is not −1. No attachment selection, matrix copy, shield bit, shell
  parameter or `0x174e78` gate existed.
- `campaign_fight.cpp` `reset_idle_anim` pushed the four class clips with
  `anim_push_with_weights`, which rewrites time 0 and rate 0, and ignored
  object `+0x462`. No creation-preview or inventory-stand pose existed.

## Changes

See `docs/CHAR_APPEARANCE.md` for the rule-by-rule table with cites.

1. New Godot-free module `champions-char-appearance`
   (`native/include/champions/char_appearance.hpp`, `native/src/char_appearance.cpp`),
   CTest `char-appearance` (`native/tests/char_appearance_tests.cpp`, synthetic only).
2. `campaign_fight.cpp` `reset_idle_anim`: time and rate words survive
   (CCA:395-396); `+0x462` uses the class idled name for cell 0 (CCA:393-395,
   REF:339-340), refusing when the lump lacks it.
3. Archive extension: `player_appearance_requests` (templates and prefix read from
   the owner's executable, CCA:148-156), `player_appearance_gates`, and
   `inspect_model` → `attachment_slot` (header `+0x13`).
4. `real_player.gd`: imported records plan hair/beard (absent, null pair, gate)
   and draw a layer in rest pose only when its gate is cover-independent and its
   header `+0x13` is −1. Pose samples hide those layers.
5. `godot/tests/char_appearance_smoke.gd`, added to the public matrix list.

## Correction made during the round

> Superseded in the fix round (CA-2): the compiled 0.2 is the truncated `0x3e4ccccc` (research `core.md:67`), which gives the executed `0x3daaaaab`; see "Fix round 1".

The first draft hard-coded the 0.2 creation blend as `0x3e4ccccd`. The test
against the spec's executed count-1 rate `0x3daaaaab` (CCA:346-347) failed:
that word gives `0x3daaaaaa` under the project's PS2 arithmetic (a mini probe
of `core_arithmetic`, and the research `ee_emu.fp_div` agree). The spec gives
the load only by address (`0x225584`), so the blend words for creation and
inventory (`0x1c4078`) are now caller input and nothing is embedded
(`914b4282`).

## Private acceptance probe (counts only)

> Corrected in the fix round (CA-4): the archives differ. The consecutive scan gives model 10 hair 3 vs 1, and index 1 is absent from the second archive; the equal membership totals are a coincidence. See "Fix round 1".

`work/ca-r1/appearance_probe.gd` (untracked, ignored), headless on the mini
against the read-only disc: for all 12 models and styles 0..7 (colour 0, skin 0)
it formats the constructor requests from the executable's own templates and
looks them up in both selection archives. Per archive: body mesh present
**96/96**; hair pairs **59** (null 37); beard pairs **14** (null 82); every
resolved hair and beard mesh has header `+0x13` = −1 (59 and 14), so no
resolved layer needs an attachment record. The two archives give the same
counts; which file is `0x5353e0` versus `0x535b68` (CCA:146-149) is not
established by this probe, and `real_player.gd` keeps its existing
player-count archive choice and status note. Log `work/ca-r1/appearance-probe.log`.

## Gates

### Gate 1: Release build + GDExtensions — PASS

Final source `914b4282`. Native Release configure/build exit 0 (first full build
305 s on `ba488b79`, incremental 87 s on the final tree, `-j 4`). Archive
`template_debug`/`template_release`, level-texture debug/release and
creature-animation `template_debug` configured fresh and linked, exit 0;
`lipo -archs` reports `x86_64 arm64` for all five. Overall 655 s. Evidence
`work/ca-r1/build-status.txt`, `configure.log`, `build.log`, `*-build.log`.
`champions-char-appearance-tests`: 8,510 checks passed.

### Gate 2: focused tests — PASS

`ctest -R '^(char-appearance|campaign-fight.*|player-death-flow|ps2-class-restore|script-commands|real-data.*|animation-clock.*)$' -j 3`:
**11/11 PASS**, 54 s, exit 0. `char-appearance` is new; `campaign-fight-adopt`,
`campaign-fight-boundaries`, `player-death-flow`, `ps2-class-restore` and
`script-commands` link the changed `reset_idle_anim`; the three animation-clock
tests cover the `anim_start`/`anim_advance` the pose helpers call. Evidence
`work/ca-r1/ctest-focused.out`.

### Gate 3: full CTest — PASS after one timeout rerun

`ctest --test-dir build/native --output-on-failure -j 3`, standard input closed:
**259/260 passed**, 3,026.67 s, exit 8. Registered count is **260**: wave 37's
259 plus the new `char-appearance`. The one failure is `messages-exact`, a
Python harness timeout: `subprocess.TimeoutExpired: … champions-messages-trace …
timed out after 120 seconds` (`work/ca-r1/ctest.out:76-99`), at a mini load
average of 92–137 from other agents' jobs. Following the mini rule for a
timeout-only failure, it was rerun alone: **1/1 PASS, 23.50 s**
(`work/ca-r1/ctest-messages-exact-rerun.out`). That trace binary does not link
`champions-char-appearance` or `campaign_fight.cpp`. No test, vector or
baseline was changed.

### Gate 4: strict Godot matrix — PASS

`tools/godot_matrix.py matrix --captures --timeout 1200`, Godot 4.5.2 on the
mini, isolated project and user directories: **55/55 PASS**, exit 0, 1,572 s.
Wave 37 was 54/54; the added case is `char_appearance_smoke`
(`CHAR_APPEARANCE_SMOKE_PASS checks=23 failures=0`). The unchanged
`godot_log.error_lines` checker finds **0 diagnostic errors in 107 logs**. A
headless editor import ran first because the rsync excludes `.godot` (exit 0,
`work/ca-r1/editor-import.log`). Evidence `work/ca-r1/matrix/results.json`.
Full synthetic combat `PLAYABLE_PLAY_RESULT` **2,645 checks, 0 failures**.

### Gate 5: FULL_SAVE_LOAD — PASS

`presents [300,300]`; continued and uninterrupted presentation SHA-256 both
`cb2309ab23d3e2906b5b9d4e95e5577c354869fce3d86cce9fe592a0d2c8d573`, identical
to wave 37 (`work/ca-r1/matrix/playable_wave_smoke_full_combat.log`).

### Gate 6: FULL_TRACE — PASS, unchanged

**36,402 bytes**, SHA-256
`9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`, renderer
headless. No change to explain: the synthetic Full fight uses the
`combat_runtime_world.cpp` fixture cells, not `campaign_fight.cpp`
`reset_idle_anim`, and the new module has no caller in that path. No baseline
was edited.

### Gate 7: journeys — PASS, matches 80/81 and pickup 10→11

Unchanged `tools/run_loop_playthrough.py`, serial headless then rendered,
standard input closed, read-only disc. Compared field by field with the
death-reload round's final journey
(`play-death-reload/work/death-reload/gates-r1-final/playthrough`), which wave 37
reports as matching step-for-step.

| Variant | Steps | Exit | Ending | Pickup | Refused rows | Log errors | Captures |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Headless | 80 | 1 | `player_death` refused; `saved_level_reloaded` false | **10→11**, produced/walked/picked up, 52 walk ticks | 27 | 0 | 0 |
| Rendered | 81 | 1 | same | **10→11**, 52 walk ticks | 28 | 0 | 38 |

Disc files 994 before / 994 after. Six refusal kinds each, and the refusal maps
are identical. Step name and result pairs are identical at every index. Wrapper
exit 1 is the retained full-journey refusal. `rendered_pickup_verified` stays
false.

Every difference, other than host `fps`, `memory_bytes` and `swing_wall_msec`:

- `dialog_choice_close.choices_taken`: 3 headless / 3 rendered, against 5 / 6.
  The baseline itself varies between runs of the same code: the earlier
  death-reload journey had 5 / 5. The session advances by wall-clock time each
  frame (`godot/scenes/session_sandbox.gd:91-100`, `advance_time(elapsed)`),
  on top of the driver's explicit steps, while the choice loop presses Enter
  every 8 frames (`godot/tests/loop_playthrough.gd:784-795`). This run's dialog
  ran at 26 / 13 fps against 60 / 76 for the baseline, so more native ticks pass
  between presses and fewer choices land before the box closes. The step result
  (`closed`) is the same.
- `hub_trigger_mission_menu.script_executed_commands`: `addQuest` 1 against 3 / 4
  and `setMissionAvailable` 2 against 4 / 5. Every other command count is
  identical. These follow from the fewer dialog choices above. The step result
  is the same.

Neither path is touched by this branch. The journey creates a new character, so
`real_player.gd` gets no appearance bytes and takes the unchanged
neutral-material path (`player_visual`/`player_pose_sample` have no field
difference). The campaign `reset_idle_anim` runs once at start on a fresh state
where time and rate are already 0, and no `player_resetIdleAnim` refusal
appears. Evidence `work/ca-r1/playthrough.out`, `work/ca-r1/playthrough/`.

## Remaining boundaries

> Superseded in the fix round (CA-3): the reasons and cites now follow `7931517`, and the preview step is ported. See "Fix round 1".

- Refused, with reasons in `docs/CHAR_APPEARANCE.md`: animated joint matrices
  (CCA:236-238, 433-434, 460-461); who advances the creation preview
  (CCA:351-364, 482-485), so no step is added; pixel parity, the shell packet
  image and the projection's three factors (CCA:317, 433-434, 458-461).
- In the client, the cover flag and the body show mask stay refused, because
  equipped parts are not resolved
  (`native/include/champions/loading_equipment.hpp:21-22,36`). Cover-dependent
  hair/beard report `refused cover`; the body keeps default-visible meshes.
- The blend words at `0x225584` and `0x1c4078` are caller input. No client
  consumer exists yet for the creation preview or the inventory stand.
- Not run: arm64/x64 trace comparison (no deterministic trace changed),
  transition setups (CCA:366-373), and the menu count/handoff rules (CCA:44-136).
- No push, PR, sibling edit, research or playable-integration change, private-disc
  write, or process-name kill. Mini paths in retained logs are rewritten to `~/`.

# Fix round 1 (audit `play-char-appearance-r1`, VERDICT FIX) — 2026-11-09

Audit `~/Projects/champions-re-wt/cursor-runs/audits/play-char-appearance-r1.md`:
0 HIGH, 2 MEDIUM, 4 LOW. Starting tip `963416d1`. Research re-pinned to the
PASS tip `t3/research-char-appearance` `7931517` (audit
`research-char-appearance-r4` PASS); every CCA/REF cite in the branch now
points there. The mini copy is an rsynced tree, not a git worktree, and the
mini bare repo has no `codex/play-char-appearance` branch. As in round 1,
commits were made on the MacBook worktree (each one a fast-forward of the
branch) and the tree was rsynced to
`~/malik-champions/open-champions/worktrees/play-char-appearance` for every
build, test, probe and Godot run. Logs are under ignored `work/ca-r2/`.

| Step | Commit |
| --- | --- |
| Re-pin to `7931517`, CA-1 | `2a90e7b2` |
| CA-2, CA-5 | `f9cc5879` |
| CA-3, CA-6 | `109f4990` |
| Gates and this report | see the final commits |

### Re-pin

**Finding.** Round 1 cited `153d1c3`. The spec gained 182 lines by
`7931517`, so CCA line numbers after the pose section moved, and two refusal
reasons were stale (CA-3).
**Fix.** A line map built from the two spec revisions (`difflib` over
`git show 153d1c3:` / `7931517:`) moved every cite whose text did not change.
I checked the cites that fall in rewritten text by hand: the class-word cite
is now handoff item 3 (CCA:639-642), the restart range is CCA:331-342, and the
refusal cites were rewritten (CA-3). `tools/ref_char_attachment.py` gained
lines only after its old line 523, so every REF cite in use (25-374) keeps its
number.
**Evidence.** `git diff 963416d1..2a90e7b2`; no implemented rule changed
between the pins (audit, "Version gap").

### CA-1 (MEDIUM) — out-of-range race byte

**Finding.** `creation_restart` returned "no restart" for an import-literal
row whose race byte + 1 is outside 0..12. That is the same result as a failed
gate. The reference says the input name is kept (`REF:345-350`,
`race_literal`: "None keeps the input"). But no out-of-range byte was ever
executed: the creation rows run bytes −1..11 only
(`tools/difftest_char_picture.py:483-486` @ `7931517`), and
`expected_name` checks that the address exists (`:409-410`).
**Fix.** Because the research never ran that case, the port refuses it
instead of guessing: `throw Refused("race byte outside the executed -1..11
copy range")`. The gate still decides first, so a failed gate is still
`nullopt` even for an unexecuted byte. A descriptor name that is not the
literal never reads the byte. The round-1 test row I added (bytes 12 and −2
→ `nullopt`) is replaced by refusal rows for 12, −2, 127 and −128, a gate-first
row, and a non-literal row. No pre-existing test was changed.
**Evidence.** `native/src/char_appearance.cpp` `creation_restart`;
`native/tests/char_appearance_tests.cpp` `poses()`; gate 2.

### CA-2 (MEDIUM) — executed count-1 rate

**Finding.** The count-1 install test recomputed its expected rate with the
port's own `blend_rate` formula on a synthetic blend, so it checked itself.
The executed `0x3daaaaab` (CCA:347-348) was never asserted. Round 1 reported
that the "obvious" 0.2 word `0x3e4ccccd` gives `0x3daaaaaa`.
**Resolution.** The research publishes the exact word and the operation
order:
- Word. Research `docs/spec/core.md:67` @ `7931517`: "Decimal literals were
  truncated by the compiler" (0.1f is `0x3dcccccc`).
  `docs/issues/float-divergence.md:96`: "0.2 is stored as 0x3e4ccccc". So the
  creation word is `0x3e4ccccc`, not the rounded `0x3e4ccccd`. Round 1 assumed
  the rounded value.
- Order. `docs/spec/animation.md:240`: `rate(s)` = 1.0 when `s < 1/60`, else
  `1.0 / (s × 60.0)`. The multiply comes first, then the divide. `anim_start`
  uses `rate(blendIn)` (`:253`), and so does the port's `blend_rate`
  (`native/src/animation_clock.cpp:44-46`).
- `0x3e4ccccc × 60.0` truncates to `0x413fffff`, and `1.0 / 0x413fffff`
  truncates to `0x3daaaaab`, which equals the executed value. No fudge is
  needed.

**Fix.** The test now feeds `0x3e4ccccc` and asserts `t->rate == 0x3daaaaab`
(and flags 3, count 2). The inventory rows use the truncated `0x3dcccccc`,
which cell 0 forces off. Both words stay caller input; nothing in the library
embeds them.
**Private probe (pass/fail only).** `work/ca-r2/blend_probe.py`, run on the
mini against the read-only `SLUS_209.73`: `0x225584`/`0x225588` and
`0x1c4078`/`0x1c407c` are `lui`/`ori` pairs into `$at` that build exactly
`0x3e4ccccc` and `0x3dcccccc`, each followed by `mtc1`. Result:
`BLEND_PROBE creation PASS`, `inventory PASS`, overall **PASS**
(`work/ca-r2/blend-probe.log`). So the test's input is the executable's own
word, and the port's `anim_start` turns it into the executed rate.
**Evidence.** Gate 2 (`char-appearance` passes with the constant).

### CA-3 (LOW) — stale refusal reasons; port what the PASS tip executes

**Finding.** At `7931517` the "Whole frames" section executes the preview
step and the VU1 joint/attach matrices. The round-1 reasons ("Unknown") were
stale.
**Fix, by item:**
- **Preview step — ported.** CCA:472-485: the run pass steps each active
  preview once (`runObjects` → `NPC_run` → `Creature_stepAnim` →
  `anim_advance` from `0x1163a4`, speed 1). The draw pass and the menu code
  write no time (CCA:484, 487-490). The native module already owns the
  preview's clip install and restart gate, so I added
  `preview_run_pass(system, state, active)`. Which previews are active is
  Inferred (CCA:496-501), so `active` is caller input. Only the
  `anim_advance` is modelled, not the rest of `Creature_stepAnim`. The test
  chains four run passes from the install and gets
  `0x3f7fffff, 0x3fffffff, 0x403fffff, 0x407ffffe`, which are the research
  reference `unit_advance` values (REF:374, flags 3, speed 1), as the frame
  rows check them (`tools/difftest_char_frame.py:336-342`). An inactive
  preview writes nothing.
  **Still refused: client object.** The client has no creation-preview
  object; the frontend draws a 2D body-texture card
  (`godot/ui/frontend_menus.gd` `_draw_body_asset_preview`). So nothing calls
  the step. For the same reason, the intro install (CCA:503-524), preview
  visibility and the preview body mask (CCA:526-565) are not ported.
- **Joint matrices — refusal kept, reason updated.** Executed: `J_j = P_j · T`
  and slot 0 `V · P_j · T` (CCA:567-606, handoff 11 CCA:665-667). The client
  gap is the pose input. `sample_skinned_resources` returns skinned body
  vertices only, and refuses any model whose header `+0x13` is not −1
  (`native/src/skinning.cpp:19-20`). So no per-joint `P_j` reaches a part
  draw, and `attachment_matrix` stays a copy. No resolved retail hair or beard
  selects an attach record anyway (round-1 probe: 59 + 14 pairs per archive,
  all header −1).
- **Pixel parity — refusal kept, cites updated** to the shell image
  (CCA:446-447), the projection factors (CCA:317), and VU rounding and the GS
  (CCA:448, 576, 628-629).

**Evidence.** `docs/CHAR_APPEARANCE.md` "Refused, with reasons";
`char_appearance.hpp` header block; gate 2.

### CA-4 (LOW) — per-archive probe

**Finding.** Round 1 reported identical counts for both selection archives,
against CCA:71 and CCA:166-169.
**Fix.** New probe `work/ca-r2/archive_probe.gd` (untracked; counts only).
It resolves each archive name from the executable's own strings at
`0x5353e0` (first) and `0x535b68` (second), opens and lists each file on its
own, prints a fingerprint, and reports per archive: pair membership (styles
0..7, colour 0), the consecutive scan of CCA:46-53 per model, the hair
membership bits per style, and model 10 hair index 1.

| | First (`0x5353e0`) | Second (`0x535b68`) |
| --- | --- | --- |
| Fingerprint / size / rows | `b62add3c57bd` / 8,637,288 / 574 | `95d5e268f9bf` / 12,245,760 / 604 |
| Hair pairs / null (membership) | 59 / 37 | 59 / 37 |
| Beard pairs / null | 14 / 82 | 14 / 82 |
| Consecutive hair styles, total | **59** | **57** |
| Model 10: scan body/hair/colour/beard | 5 / **3** / 1 / 0 | 5 / **1** / 1 / 0 |
| Model 10 hair membership, styles 0..7 | `11100000` | `10110000` |
| Model 10 hair index 1 pair | present | **absent** |

The two files differ. Model 10 differs as the research says. Its scan
gives 3 styles in the first archive and 1 in the second; with the optional
absent entry that is hair count 4 / 2, exactly CCA:71. Index 1 is absent from
the second archive and present in the first (CCA:166-169). Every other row of
the scan matches the CCA:68-81 table (body, hair + optional, colour, and beard
+ absent). The equal membership totals are a coincidence, not a double read:
the second archive lacks model 10 style 1 but has style 3, which the first
lacks. Round 1's probe did read both files, but membership alone cannot show
the difference, and its report wrongly said the counts were identical.
`real_player.gd` needs no change: for model 10 skin 3 from the second archive
it keeps a null pair and substitutes nothing (CCA:166-170).
**Evidence.** `work/ca-r2/archive-probe.log`.

### CA-5 (LOW) — blend word "read from the executable"

**Finding.** The doc row claimed the word is read from the executable;
nothing reads it.
**Fix.** I corrected the doc instead of adding a reader, because no consumer
exists. The "Creation restart" and "Inventory stand" rows and the "Blend
words" paragraph now say: caller input, to be read from the executable at
`0x225584` / `0x1c4078` by the consumer, no consumer yet. The header comment
says the same and cites the research's truncated words.
**Evidence.** `docs/CHAR_APPEARANCE.md`; `char_appearance.hpp`.

### CA-6 (LOW) — rest-pose layer space

**Finding.** Drawing a header −1 hair/beard in the body's rest-pose space was
stated as fact, with no research citation.
**Fix.** It is now labelled **Inferred** in `real_player.gd` (`_draw_layer`
comment and the pose-sample hide comment) and in a new doc item,
"Rest-pose layers (Inferred)". No research row executes a part with no attach
record: the VU1 section runs only a part on an attach record (CCA:567-606).
The basis is handoff 11 (CCA:665-667): mesh weights address joints as
`index*4` in the parent's joint qwords. The draw behaviour is unchanged.
**Evidence.** `godot/real_data/real_player.gd:204-215`; the journey and
matrix gates below.

## Fix-round gates (all on the mini, source `109f4990`)

The mini load average was about 90 during the build and about 2 for the tests
and Godot runs. `df -h ~/malik-champions` showed 36–48 GB free at every check,
never under 30 GB. There was one build root, `build/` (native plus the five
GDExtension configures as subdirectories, built serially with `-j 4`). It was
deleted when the gates finished (6.3 GB). Mini paths in the retained logs are
rewritten to `~/`.

| Gate | Result | Evidence (`work/ca-r2/`) |
| --- | --- | --- |
| 1. Release build + GDExtensions | **PASS**: native exit 0 (467 s); archive debug/release, level-texture debug/release and creature-animation debug exit 0, all `x86_64 arm64`; 1,542 s overall | `build-status.txt`, `*build.log` |
| 2. Focused tests | **11/11 PASS**, 29 s (same selection as round 1); `champions-char-appearance-tests` **8,520 checks** (round 1: 8,510) | `ctest-focused.out` |
| 3. Full CTest `-j 3` | **260/260 PASS**, 1,499 s, exit 0, no timeout, so no rerun was needed | `ctest.out` |
| 4. Strict Godot matrix | **55/55 PASS**, exit 0, 176 s; `CHAR_APPEARANCE_SMOKE_PASS checks=23 failures=0`; the unchanged `godot_log.error_lines` finds **0** diagnostic errors in 107 logs | `matrix/results.json`, `matrix-run.log` |
| Combat | `PLAYABLE_PLAY_RESULT` **2,645 checks, 0 failures** | `matrix/playable_wave_smoke_full_combat.log` |
| 5. FULL_SAVE_LOAD | **`presents [300,300]`**; continued and uninterrupted presentations both `cb2309ab23d3e2906b5b9d4e95e5577c354869fce3d86cce9fe592a0d2c8d573` (unchanged) | same log |
| 6. FULL_TRACE | **36,402 bytes**, SHA-256 `9c7922b2df46aba6542a230d9c60baa04de2f4113982bd533ae79adc33b0c659`, **unchanged** | same log |
| 7. Journeys | **PASS**: headless 80 steps, rendered 81, wrapper exit 1 (the retained full-journey refusal), pickup **10→11** with 52 walk ticks, 0 log errors, 38 captures, disc files 994/994 | `playthrough.out`, `playthrough/` |

**Journey comparison, step by step** (`work/ca-r2/compare_journey.py`: every
field of every step except host `fps`, `memory_bytes` and `swing_wall_msec`,
plus the refusal maps):

- Against the death-reload baseline
  (`play-death-reload/work/death-reload/gates-r1-final/playthrough`).
  Headless matches field for field at all 80 steps, now including
  `dialog_choice_close.choices_taken` 5 and `addQuest`/`setMissionAvailable`
  3/4. Rendered has identical step/result pairs at all 81 steps. The only
  field differences are the known wall-clock pair: `choices_taken` 3 against
  6, which leads to `addQuest` 1 against 4 and `setMissionAvailable` 2
  against 5. Refusal maps are identical in both variants.
- Against round 1 (`work/ca-r1/playthrough`), rendered matches field for
  field. Headless differs only in that same dialog triple, 5 against round
  1's 3.
- This confirms the round-1 explanation, which the audit accepted. At step 18
  the dialog ran at 122 fps headless and gave 5 choices, as the baseline did
  at 145 fps. The rendered run at 46 fps gave 3, as round 1 did at 38/10 fps.
  This round changed no session, dialog or script code.

## Fix-round status

All six findings are fixed, and every gate is recorded above. Still refused,
with the current reasons in `docs/CHAR_APPEARANCE.md`:
- joint-matrix composition for attached parts (no per-joint `P_j` reaches a
  part draw);
- the creation-preview, intro and preview-mask consumers (no client preview
  object; the step itself is ported);
- pixel parity;
- the client cover flag and body mask (equipment unresolved);
- out-of-range race bytes (never executed).

Rest-pose hair/beard drawing is labelled Inferred. No push, no PR, no
research or sibling edit, no private-disc write, nothing installed, no
process killed by name, and nothing under `/tmp` on the mini.
