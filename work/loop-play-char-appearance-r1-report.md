STATUS: IN PROGRESS — gates being recorded.

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
