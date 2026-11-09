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
| Creation restart | bit `0x80` and count < 2; bit `0x08` picks the marked format; import literal → race slot byte+1 in 0..12; `anim_start` with flags 3 and the blend word at `0x225584`, read from the executable (forced to 0 on an empty state, kept at count 1) | CCA:331-348 |
| Inventory stand | lump by model, idle by class, flags `0x43`, blend word at `0x1c4078` forced off for cell 0; a current clip is not restarted; one unit step gives time `0x3f7fffff`, weight 1 | CCA:389-402 |
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
  and draws a layer in the body's rest-pose space only when its gate passes
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
- **Joint matrices.** Animated joint evaluation is Unknown (CCA:236-238,
  433-434, 460-461). A layer whose header selects an attachment record is
  `refused joint matrix`; rest-pose layers are hidden during explicit pose samples.
- **Creation-preview step.** No direct call in `0x225478–0x22e700` targets
  `anim_advance`, and the task that steps the preview is Unknown
  (CCA:352-378, 651-654). The creation install stores time 0 and weight 0 and
  no step is added. The client has no creation-preview object.
- **Pixel parity.** The shell packet image, the projection's three factors,
  and VU/GS output are not modelled (CCA:317, 447-448, 630-633). Only the
  shell parameters and the gate are.
- **Blend words.** The spec names the 0.2 and 0.1 blend loads only by
  address (CCA:346, 396), so the API takes them as caller input. The executed
  count-1 rate `0x3daaaaab` (CCA:347-348) is not reproduced from an assumed
  `0x3e4ccccd`: under the project's PS2 arithmetic that word gives
  `0x3daaaaaa`. No word is embedded.
- **Transition setups** (CCA:380-387) and the menu count/handoff rules
  (CCA:44-136) are outside this round's requested scope.
