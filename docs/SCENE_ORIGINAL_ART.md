# Measured Return to Arms UI images in the native client

The reusable `res://ui/original_art/original_art_panel.tscn` accepts an existing
`ChampionsArchiveInspector` through `configure(inspector)`. Its other shell entry
points are `show_picker()`, `update_observation(observation: Dictionary)` and
`clear_art()`. `art_ready(ready: bool)` reports whether at least one measured
image is loaded. The shell can embed the Control without taking over Session input.

## Evidence and data boundary

This is RTA US v2.00 metadata. The entry list started from research commit
`a2b145b6b22f3c0451f861ed70faa1827f94bf09`. Round 4 research `3e39392`
replaces the page and crop with the TEX used size:

- `tools/godot/project/gameart/catalogue.gd` SHA-256
  `db1e91dd7f19806ed727dd40c16ae8304313fcfdcf6e3510ae5100257fddbee5`,
  entries `bar_red`, `bar_blue`, `button`, `frame_*`, `statusbar` and
  `statusbar_glow` (source lines 12, 14, 17, 26–34, 132–133).
- `docs/issues/native-gameart.md` SHA-256
  `3fa5a8438f85b1051111df30c35eb8b83859b68238b6ae66642bde9c6d17ee63`
  describes catalogue creation and separately reports its author's pixel
  comparisons. Those comparisons were not rerun by this client.

The small table in `godot/ui/original_art/catalogue.gd` records only archive
entry names, page dimensions, field rows and woven-space crops. Round 4 sets
the page and crop to the TEX used size: bars 100×8, button 83×38, status
frame and glow 160×46. Frame tiles are unchanged. It has no pixels,
pack references, extracted text, inferred anchors or inferred nine-slice margins.
Its `rect` is a crop after native shaping. The tile family is displayed as
individual intact images; it is not assembled into a guessed panel.

The operator opens a local GOB, LMP or TEX and explicitly chooses a catalogue
identity and flattened row. Archive path/parent, name and page dimensions must
match. Duplicate names remain separate rows. For loose TEX, the alleged LMP
origin is the operator's assertion because a lone file has no archive parent.
Metadata matching is not cryptographic proof of retail identity. The binding's
source fingerprint is passed back at decode time; changed files are refused.
Unsupported formats, wrong dimensions and failed native shapes do not replace
already loaded images. `clear_art()` unloads all images.

The existing native decoder supplies RGBA8 and retains its own alpha conversion.
`shape_ui_art` supplies the field weave and crop. GDScript only selects resources
and presents the result. Each image is nearest sampled at 1x or 2x, with no
filtering, aspect change, second alpha conversion or claimed original blend parity.

## Authored diagnostic policy

The displayed health and mana numbers read `hp`, `max_hp`, `mana` and `max_mana`
from an observation dictionary. Unsupported or missing values display as such.
The standalone red and blue bar images may be clipped left-to-right to
`floor(image_width * clamp(value / maximum, 0, 1))`; this is an authored client
diagnostic rule, **not** evidence of the game's original fill direction, rounding,
placement or layering. Values are never written back to Session. Whole frame,
button and status images are shown in an image collection beside the diagnostic,
without guessed overlay coordinates. When art is absent, the panel says
`Original art not loaded` and keeps the diagnostic numbers.

Original HUD composition, resource associations, text baseline and kerning,
font choice, button behavior, nine-slice assembly and final blending remain
unverified. No original layout or retail visual fidelity is claimed. No private
game input was available for this branch, and none is included in the repository.


## Playable integration wave 3

The panel also offers a native-positioned HUD status-region preview and explicit
HDR/DAT inventory picture grid. The existing single-image gallery retains its
authored clipping/placement; native HUD placement is a separate labelled view.
See [the integration report](PLAYABLE_WAVE_2026_10_17.md), [HUD layout](HUD_LAYOUT.md)
and [inventory pictures](INVENTORY_ICONS.md) for exact scope, source pins and gates.
