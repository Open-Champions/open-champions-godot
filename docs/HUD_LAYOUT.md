# Return to Arms static UI layout service

This module computes documented logical coordinates for the Return to Arms US
v2.00 menu frames, base player status layers, and basic FNT glyph metrics. It
does not load art. The existing UI gallery and texture decoder supply original
resources from the player's own files. The portable API is in
`native/include/champions/ui_layout.hpp`; `ChampionsUiLayoutService` exposes
the same computations to Godot. Norrath is not established by this contract.

## Pinned evidence

Read-only research checkout: `~/Projects/champions-re-wt/research-ui`, commit
`5af769188dfbd47e15693f5041ae5d6d90292bc9` (`t3/research-ui`). SHA-256:

| Contract file | SHA-256 |
|---|---|
| `docs/hud-layout.md` | `c3d022af255a873a2d59887a1e3f1d3549bcde57207a4bf18a817066aa2fea05` |
| `docs/formats/ui-fonts.md` | `055b33112d5fb59b2bd8c892f93d4bb6b6e43830c88dffe38f669621ec0f98dd` |
| `tools/ref_ui_layout.py` | `48d90779422da84db753cf217b7a7ed6409ca6fce548ba259c837e4d664b06fe` |
| `tools/test_ref_ui_layout.py` | `51b8d99da1193df108314ebfc13a2454776271f173d93d861af0058e98ad6d42` |

The C++ synthetic tests port that reference's frame, slot, fill, HUD, and font
vectors. They do not establish a rendered match with the game. No private game
files are present for acceptance measurements in this checkout.

## Portable API

All positions are in the original 640 × 448 logical coordinate system. For a
square-pixel 4:3 presentation, map to 640 × 480 before choosing final scaling.
Frame parts are **separate textures**, with whole-resource source rectangles.
There is no original nine-slice atlas. `frameSourceRect(name)` returns a half-open
source `(x,y,width,height)` for the nine documented parts in the `tile`,
`btile`, or `ctile` family. `frameBorder(style)` returns the 32 or 16 pixel
assembly border. Unknown resource names throw `FormatError`.

`menuFrame(x,y,width,height,style,no_center,no_border)` returns ordered `Draw`
records. Style 2 uses `tile`, style 1 `btile`, and all other styles `ctile`.
Each record names a resource, destination `(x,y)`, and optional destination
clip `(left,top,right,bottom)`. The order is original **submission order**;
a painter renderer reverses it within the common sort layer. Unclipped corner
sprites receive the original plain-sprite +1 Y during rendering; clipped draws
do not. The function does not itself apply that rendering adjustment. Style 2
has a 4-pixel bottom clip overlap and a 26-pixel center overlap. Frames smaller
than two borders retain overlapping corners. Authored safety limits require
nonnegative width/height no greater than 4096 and checked integer coordinates.

`hudSourceRect(name)` returns whole-resource sizes for the 11 frames (00–10)
of each red/green/blue bar, `hud_multi.tex`, `hud_statusbar00.tex`, and its
glow resource. Names outside the documented set are refused.

`hudOrigins(network,local_slots,player_count)` returns four X origins in slot
order. The default is `[12,478,167,323]`. Network local-slot promotion follows
the documented loop; valid player counts are 0 through 4. The caller handles
all status visibility gates.

`barClipRight(origin,current,maximum)` executes the documented divide,
multiply by 99, add origin, and truncate sequence with the reference PS2 raw
single-precision arithmetic model. Inputs require finite nonnegative current,
positive finite maximum, and nonnegative origin. Fill is deliberately not
clamped; full fill ends at `origin+99` in the synthetic vectors.

`hudBaseLayers(...)` returns only health, optional local experience and mana,
then the local or remote status frame. Records retain original resource names,
X/Y, destination clipping, tint words, and sort layer 6. The animation frame
is `(tick >> 2) % 11`. The caller supplies selected slot X, tick and observed
values after original visibility gates. The function does not render, tint,
blend, or decide whether a player is visible.

`fontMetrics(fnt_bytes)` uses the existing `readRtaUiFont` metadata reader and
adds the documented signed 256-entry byte map, signed glyph coordinates,
kerning starts, and bounded six-byte kerning table. It requires the pinned
nonoverlapping table spans and at least a 0x40-byte embedded TEX suffix.
`glyphRects(index,pen_x,text_y)` returns logical source and destination
half-open rectangles. `widthAscii(bytes,interlaced,count)` measures byte ASCII
with NUL termination, glyph-zero fallback, and kerning. It is a measurement
operation, not a text draw service. Unknown FNT header +2 is not interpreted.

## Godot API

Instantiate `ChampionsUiLayoutService` in GDScript. Methods `menu_frame`,
`frame_source_rect`, `hud_source_rect`, `hud_origins`, `bar_clip_right`, `hud_base_layers`, and
`font_metrics` return dictionaries with `ok`, result fields, or `error`.
`menu_frame` takes all seven arguments and returns `draws` and `border`.
`frame_source_rect` and `hud_source_rect` take a resource name and return `rect`.
`hud_origins` takes a network flag, a `PackedInt32Array` of local slot indices,
and player count; it returns `origins`. `bar_clip_right` returns `right`.
`hud_base_layers` takes the portable API's arguments and returns `layers`.
`font_metrics` takes FNT bytes, ASCII bytes, interlace flag, byte count, glyph
index (negative to omit rectangles), pen X, and text Y; it returns glyph and
kerning counts, `ascii_width`, and optional `source`/`destination` rectangles.
The wrapper copies Godot values into or out of the portable core and does no
layout calculation itself. `original_art` can consume these records later.

## Remaining gates

Dynamic notifications, potion indicators, spell ring, compass, minimap,
status visibility/packet-space conditions, styled or wrapped text, wide-code
fallback, glyph raster edge sampling, unknown flags, actual font selection by
screen, GS field clipping and blending, and rendered visual parity require
further evidence or integration. This module does not guess those behaviors.
