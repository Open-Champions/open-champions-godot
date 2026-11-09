# Created-character appearance

`native/include/champions/char_appearance.hpp` ports the Observed/Executed
rules of `t3/research-char-appearance` at `7931517`
(`docs/spec/created-character-appearance.md`, cited `CCA:line`; audits
`research-char-appearance-r2`, `-r3-fix` and `-r4` PASS).
The starting-equipment map is reused from `t3/research-loading-readiness` at
`366b65a` (`docs/starting-equipment-r4.md`, cited `SE4:line`), as the spec
itself does (CCA:253-259). Constants that the spec names only through its
reference module come from `tools/ref_char_attachment.py` at the same commit
(`REF:line`). Tests use synthetic templates, tokens, handles and masks
(`native/tests/char_appearance_tests.cpp`, CTest `char-appearance`;
`godot/tests/char_appearance_smoke.gd`). No retail name or byte is embedded.

## Implemented

| Contract | Rule | Cite |
|---|---|---|
| Constructor requests | hair pair, beard pair, body pair; style+1, colour+97; style −1 omits its pair and clears both handles; body texture skin+1 | CCA:152-164 |
| Null pairs | a failed lookup stays null over poisoned handles; no substitute style | CCA:162-170 |
| Templates and prefix | six format addresses and the model prefix table, read from the owner's executable | CCA:148-156 |
| Archive selection | two players or byte `0x506040` select the second archive | CCA:146-149 |
| Cover flag | sticky; set only by an equipped part with both handles, per the token table | CCA:182-191 |
| Hair/beard gates | hair: uncovered, model 11, or model 10 skin 4; beard: uncovered, model 11, or a head item with the exception token or model 6; each layer also needs both handles | CCA:193-198 |
| Body show mask | all ones, or `0x179550` with bit one when the signed counter `0x41b514 >= 0x111`; drawn parts clear their hide qword | CCA:201-208 |
| Mask frame order | the body draw passes object `+0x220` as stored by the previous `Player_drawEquipped` | CCA:417-433 |
| Mesh runs | coalesced shown runs, empty spans skipped, 64-mesh EE edge preserved | CCA:229-232, REF:96-122 |
| Attachment selection | header `+0x13` −1 selects nothing, even with a nonnegative override; otherwise the override replaces −1 only; record `parent + byte22*16 + i*0x34`; joint×4; no clamp | CCA:210-227 |
| Matrix copy | nine rotation words and origin to the 3×4 packet; mirroring flips the first row | CCA:225-226, REF:83-87 |
| Starting visuals by body | class and weapon slot per body, armor 4/10/12; show mask all ones; attach index from the part header; hide-bit clear; class word not read | CCA:283-290, 639-642; SE4:44-56 |
| Off-hand iteration | pointer slot 1 forces override 1; `0x179978` whenever the override is not −1; model 2 also mirrors | CCA:291-298 |
| Shield bit | bodies 0–11 → 1,0,9,8,7,6,5,4,3,2,1,9; others bit 0 | CCA:262-279 |
| Shell parameters | bit 33 clear: count 0, block untouched; set: count 2, bytes 49/55, floats ×200/×100 | CCA:300-311 |
| Draw argument | absolute projection; 5 at ≥360, else `5-(360-m)*0x3c93e93e`, at least 1; inventory caller 1.5 | CCA:313-320 |
| `0x174e78` gate | part flag `0x20` and a positive count | CCA:322-325, 435-440 |
| Creation restart | bit `0x80` and count < 2; bit `0x08` picks the marked format; import literal → race slot byte+1 in 0..12; `anim_start` with flags 3 and the blend word loaded at `0x225584`: caller input, to be read from the executable by the consumer, and no consumer exists yet (forced to 0 on an empty state, kept at count 1 where the rate is the executed `0x3daaaaab`) | CCA:331-348 |
| Selection preview step | one `anim_advance` at speed 1 per run pass, active previews only (`active` is caller input); the draw pass writes no time | CCA:472-490 |
| Inventory stand | lump by model, idle by class, flags `0x43`, blend word loaded at `0x1c4078` (caller input, no consumer yet) forced off for cell 0; a current clip is not restarted; one unit step gives time `0x3f7fffff`, weight 1 | CCA:389-402 |
| `player_resetIdleAnim` | guards; four definition clips at `class*0x68+{0x44,0x4c,0x54,0x5c}`; count 4; cell 0 weight/life 1; flags `0x1205`; `+0x462` idled cell 0; time and rate not written | CCA:403-415 |

## Client use

- `native/src/campaign_fight.cpp` `reset_idle_anim` keeps each cell's time and
  rate words across the reset (CCA:409-410) and uses the class idled name for
  cell 0 when object `+0x462` is nonzero (CCA:407-409; refuses if the player
  lump lacks it). Its earlier cells 1–3 weight/life 0 follow
  `research-player-anim` `cda95dc` `player-animation-selection.md:58-66`.
- `ChampionsArchiveInspector.player_appearance_requests` formats the
  constructor requests from the executable's templates and prefix;
  `player_appearance_gates` exposes the gate rule; `inspect_model` reports
  header `+0x13` as `attachment_slot`.
- `godot/real_data/real_player.gd`, for an imported record with the four
  appearance bytes: plans each optional layer (absent / null pair / gate),
  and draws a layer in the body's rest-pose space (**Inferred**, see
  "Rest-pose layers" below) only when its gate passes
  for every cover and head input (hair on model 11 or model 10 skin 4, beard on
  model 11) and its mesh header `+0x13` is −1. New characters without the four
  bytes are unchanged.

## Refused, with reasons

- **Cover flag in the client.** It is set by equipped parts that hold both
  handles (CCA:182-185). The client does not resolve equipment resources
  (`native/include/champions/loading_equipment.hpp:21-22,36`: no formatter, header search or transfer
  stand-in), so a cover-dependent layer is reported `refused cover`.
- **Body show mask in the client.** The body consumes `+0x220` from the previous
  equipment pass (CCA:430-433). Without that pass the mask is not established;
  the body keeps its default-visible meshes. Equipment parts themselves are not
  drawn for the same reason.
- **Joint matrices: no pose input for part draws.** The PASS tip executes
  them (CCA:567-606; handoff 11, CCA:665-667): `J_j = P_j · T` per joint and
  slot 0 `V · P_j · T` for a part on an attach record, with row 0 of `V`
  negated when mirrored. The client gap is the pose input. The real-data player
  draws a rest pose; its explicit pose sample
  (`ChampionsArchiveInspector.sample_skinned_resources`) returns skinned body
  vertices only and refuses any model whose header `+0x13` is not −1
  (`native/src/skinning.cpp:19-20`), so no per-joint `P_j` reaches a part draw
  and `attachment_matrix` stays a copy. A layer whose header selects an
  attachment record is `refused joint matrix`. The private probe found no
  resolved hair or beard that selects one (59 and 14 pairs, all header −1).
  Rest-pose layers are hidden during pose samples, because skinning a header
  −1 part with the parent's joints is Inferred (see "Rest-pose layers" below).
- **Creation-preview step: ported, no client object.** The PASS tip executes
  the step (CCA:472-485): in the run pass, `runObjects` → `NPC_run` →
  `Creature_stepAnim` calls `anim_advance` from `0x1163a4` at speed 1 on each
  active preview, once per run pass. The draw pass and `playerSel_task` write
  no time (CCA:484, 487-490). `preview_run_pass` ports that step, and its test
  checks four chained run passes and an inactive preview. Which previews are
  active in a real session is Inferred (CCA:496-501), so `active` is caller
  input. The client has no creation-preview object: the frontend draws a 2D
  body-texture card (`godot/ui/frontend_menus.gd` `_draw_body_asset_preview`),
  so nothing calls the step. The intro install (CCA:503-524), preview
  visibility and the preview body mask (CCA:526-565; handoff 6-7, CCA:653-657) have no
  client consumer for the same reason and are not ported.
- **Pixel parity.** Not modelled: the shell packet image (CCA:446-447), the
  projection's three factors (CCA:317), console VU rounding and the GS
  rasterizer (CCA:448, 576, 628-629). Only the shell parameters and the gate
  are.
- **Blend words: caller input, no consumer yet.** The spec names the 0.2 and
  0.1 blend loads only by address (CCA:346, 396). The API takes them as caller
  input; a consumer is to read them from the executable at `0x225584` and
  `0x1c4078`, and none exists yet, so nothing reads them today. The research
  pins compiled decimal literals as truncated: 0.2 is `0x3e4ccccc` and 0.1 is
  `0x3dcccccc` (research `docs/spec/core.md:67`,
  `docs/issues/float-divergence.md:96` @ `7931517`). With `anim_start`'s rate
  `1.0 / (s × 60.0)` (`docs/spec/animation.md:240`, multiply first, then
  divide), `0x3e4ccccc` gives the executed count-1 rate `0x3daaaaab`
  (CCA:347-348) under the project's PS2 arithmetic, and the test asserts that
  constant. Round 1's `0x3daaaaaa` came from the rounded `0x3e4ccccd`, which is
  not the compiled word. A private probe confirmed that the owner's
  executable loads exactly those two words at the two sites (pass/fail only,
  `work/loop-play-char-appearance-r1-report.md`).
- **Rest-pose layers (Inferred).** A hair or beard mesh whose header `+0x13`
  is −1 selects no attachment record (CCA:210-215). The client draws it as a
  child of the body in the body's rest-pose space. No research row executes
  that case: the VU1 section runs only a part on an attach record
  (CCA:567-606). The choice rests on handoff 11 (CCA:665-667): mesh weights
  address joints as `index*4`, the joint qwords the parent's `model_draw`
  wrote, so a part with no attach record is taken to use the parent's joints,
  which for a rest-pose body is its bind pose. The layers are hidden during
  pose samples for the same reason.
- **Transition setups** (CCA:380-387) and the menu count/handoff rules
  (CCA:44-136) are outside this round's requested scope.
