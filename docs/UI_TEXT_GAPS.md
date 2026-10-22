# UI text gaps (help box, bust meter, compass, chime, fades, glyph byte)

Authored port of research `t3/research-ui-text-gaps` (523b3db): `docs/ui-text-gaps.md`,
`docs/spec/ui-text-gaps.md`, `tools/ref_ui_text_gaps.py`. Hashes of the pinned files are
in `native/tests/ui_text_gaps_contract.json`. The research ran the original functions in
an emulator; nothing here ran on a game. Words: **E** executed by the research,
**I** inferred, **U** unknown. Inferred and Unknown items stay refused and labelled.

## What is ported (`native/src/ui_text_gaps.cpp`, namespace `champions::ui_text_gaps`)

| Piece | Function | Research status |
|---|---|---|
| UI fade 0.12 and pulse 0.8 ramps (session step 13), `ui_startPulse` 2.0, `ui_restartFadeIn` 0.0 | `rampsStep`, `fadeRamp`, `pulseRamp` | E `ui.ramps` |
| Button layout words, glyph byte 0x41b978 | `buttonLayout`, `confirmGlyph`, `padMaskToGlyph`, `glyphText` | E `layout.glyph`; layout byte 1 observed, layout 0 inferred |
| `help_task`: inner ctile frame, glyph, `Affirmation` label, confirm loop | `helpTask`, `helpInnerFrame` | E `help.task` |
| `hud_drawBust`: gamefontsmall caption, one wrap per branch, alphas 110/128, neutral RGB, meter | `hudDrawBust` | E `bust.draw` |
| `ui_emitSpriteQuad` field rule | `spriteQuad`, `fieldCalls`, `wovenLine` | E `sprite.fields`; `wovenLine` is I |
| `hud_drawCompass`: world matrix, two passes, label | `compassWorld`, `hudDrawCompass` | E `compass.draw` |
| `soundSeq_updateAll` free rules | `soundSeqUpdateAll` | E `sound.seqUpdate` |
| showName of a quoted key | `strings::TextTable::lookup` (already drops quotes) | E `dialog.showName` |

`helpBox().inner_height` is now `h + 76` (the research corrected research-ui), and the old
`hud_dynamic::bustPanel`, which refused a maximum and guessed the font, is replaced by
`hudDrawBust`. `hud_r3.tsv` was regenerated from the corrected reference: only the four
`help_box` rows changed.

## Contract tests

`tools/generate_ui_text_gaps_vectors.py` imports the pinned research models (no game files)
and writes `native/tests/vectors/ui_text_gaps.tsv`: 3,698 data rows over seeds 20973 and 7 for
ramps, layouts, pad glyphs, help task (every call and the closing state), bust draw (every
call), sprite quads, woven lines, compass (matrix, angle, calls), sound-slot updates and
showName lookups. `ui-text-gaps-exact` replays every row and states the documented facts
(the nine-value fade ramp, 0x7e, the inner frame height, one wrap per branch, two compass
passes, the free rules) directly. `dialog_host_tests` runs a whole conversation with the
host's stepped fade: the first tick that reads exactly 1.0 follows the ninth step after the
closing restart (the dialog machine ends it within a tick of that), so the end runs later than
with a fade held at 1.0.

## Wiring

- `script_commands::Host` owns `g_uiPulse` / `g_uiFadeIn` (`ui_ramps`, `ui_frame_step`).
  `ui_startPulse` runs when a help box opens; a dialog's `fade_restart` events set the fade to
  0.0; the first dialog draw reads the restarted value. The controller passes the host fade to
  `dialog_runTask`, so the end handler waits for exactly 1.0.
- `ChampionsSessionController.ui_frame()` is session step 13: it returns the pulse and fade the
  frame drew with, then steps them. Until `set_ui_frame_driver(true)` every ready tick counts as
  one displayed frame as a **headless host proxy**, not a recovered scheduler equivalence. This
  proxy affects the nine-step delay of dialog message 3. The play scene's text HUD drives the
  fade at 60 Hz.
- The dialog cursor and "more" glyph are drawn: the controller reads 0x7e from the host's
  observed button layout (also used by the wave-24 dialog-gaps port) and
  turns the dialog's `ascii` call into a glyph in the font, tinted by the dialog's own 0x606060.
- `text_hud.gd` draws the help box (inner frame, glyph, label) and bust (both branches) from
  native plans (`help_plan`, `bust_plan`). It also uses the researched colour scaling,
  which copies the alpha byte. The compass label plan is ported, but the scene refuses
  it until the three researched gate inputs are supplied.

## Still refused or labelled

- **Compass mesh and scene label.** The scene does not know the 0x4f8424 gate, player slot
  0x4f841c, or player object, and refuses drawing either part instead of assuming those
  gates are open. The mesh also needs the game camera's view matrix and a model path in
  the scene; the second pass's light state is **U**. The native label and two-pass plans
  remain replayed against the research. All 3 disc calls are in levels outside the table.
- **Chime slot life.** The two-update lifetime is **I**, and it needs the sound cursor of
  `anim_advance`, which the native runtime refuses. `soundSeqUpdateAll` is ported and replayed, but
  the play host's chime slots are not freed. The research's `chime_frames` rows are not replayed.
- **Layout byte other than 1.** Layout 0 is inferred from code and others write nothing:
  `confirmGlyph` refuses them.
- **Initial g_uiFadeIn.** Its data value is not documented; the host starts at 1.0.
- **Help display-list gate.** Free display-list units are not observed by this host; `help_plan`
  assumes the documented minimum of 3,000 is available. The plan's draw remains labelled.
- **g_simFrame.** The host does not expose the simulation frame counter at 0x44fdcc. The play
  scene refuses the animated meter bar while still drawing the caption, field and portrait.
  The portable exact plan continues to replay the bar against synthetic frame inputs.
- **Bust hide latch.** `Hud::tick_message` sets the host latch for four local players without
  network play, and `text_hud()` exports it as `latch`; the scene passes that real value into
  the bust fade. The `hud_spellSelect` latch path is not modelled.
- **Sound slot time offset.** The pinned spec table calls the track time `+0x18`, while the
  research prose says `+0x20`. This discrepancy remains unresolved; the sound-slot model is
  test-only and the host does not free chime slots.
- **Notice boxes** (help_task from 0x1dcff4) were not re-executed by the research.
- **Odd tops of sprites on a raster-exact client** are not modelled: the client weaves, which is the
  research's inferred progressive rule.
