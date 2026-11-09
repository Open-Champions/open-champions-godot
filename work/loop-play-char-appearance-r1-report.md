STATUS: DONE — all gates recorded. Native CTest 260 registered: 259/260 in the full run plus the one timeout rerun alone PASS; strict Godot 55/55; FULL_SAVE_LOAD 300/300; FULL_TRACE unchanged; journeys 80/81 with pickup 10→11. Joint matrices, the creation-preview step, pixel parity, the client cover flag and the client body mask stay refused.

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

The first draft hard-coded the 0.2 creation blend as `0x3e4ccccd`. The test
against the spec's executed count-1 rate `0x3daaaaab` (CCA:346-347) failed:
that word gives `0x3daaaaaa` under the project's PS2 arithmetic (a mini probe
of `core_arithmetic`, and the research `ee_emu.fp_div` agree). The spec gives
the load only by address (`0x225584`), so the blend words for creation and
inventory (`0x1c4078`) are now caller input and nothing is embedded
(`914b4282`).

## Private acceptance probe (counts only)

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
