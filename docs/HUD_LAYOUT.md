# Return to Arms UI layout service

This module computes documented logical coordinates for the Return to Arms US
v2.00 menu frames, player status layers, dynamic HUD elements, and FNT text layout. It
does not load art. The existing UI gallery and texture decoder supply original
resources from the player's own files. The portable API is in
`native/include/champions/ui_layout.hpp`; `ChampionsUiLayoutService` exposes
the same computations to Godot. Norrath is not established by this contract.

## Pinned evidence

Read-only research checkout: `<research-checkouts>/research-ui`, commit
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

The follow-up dynamic/text/parity contract is pinned to read-only research commit
`0b73858def976bf494c2b2803339aa54be814a62`, after HUD
`6f488a3f781e8c0024a1a2cd987f6d2f215a3e37`, text
`2f11c14eb6278c7b7fb7e2025a09472c1e1b9000`, and icon/flag
`ad8c34ee7e4591a29fed361322bed1e085403752`. SHA-256 at the pinned head:

| Contract file | SHA-256 |
|---|---|
| `docs/hud-dynamic.md` | `818558d9ca484b292d68a54b8979dd14007ca5b0a5b89979c365bd893350a6c7` |
| `docs/formats/ui-text.md` | `ca7d6e2f1e3e9d46a60b215cd297cfd7d33b2a41ad9caa0db124bac1c6685236` |
| `docs/ui-parity.md` | `b8b4125892217b922bd46f524f40edb9664e435d04428afa35bd51b2f5c64ddc` |
| `tools/ref_hud_dynamic.py` | `b6961fdfd9d328de173dcd1c561e9c34721432c3d651a5e9be3d05a3a74589d2` |
| `tools/ref_ui_text.py` | `38e891b3c614fa084ba4f1e442f517d3529435418aa767f232ea30b8f4849fef` |
| `tools/ref_ui_composite.py` | `14db1c8882f7d60016265decc2b495fe84583244d1568cef4fddfffadc598cc6` |
| `tools/test_ref_hud_dynamic.py` | `bfcdcf444bdcef51f0ae9c5e5550db784d48d722ca4d80f07065f381048a91f7` |
| `tools/test_ref_ui_text.py` | `6e58833196b4ae4e50066b59acc6c56973b6b2a6c2f8bf34e4e5f095e8fded2c` |
| `tools/test_ref_ui_composite.py` | `5a2fef7090922baf9c55a1d76ec5e1d8cf230974a9fefa4d841a342124985cb1` |

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
operation. Unknown FNT header +2 is not interpreted.

## Dynamic HUD API

`native/include/champions/hud_dynamic.hpp` supplies authored, Godot-free draw
records (`Sprite`, `Rect`, `Text`, `Frame`) and one-frame state transitions.
`drawSequence()` lists the ten top-level HUD submission stages, including
gated stages; `statusSequence()` lists the eleven operations inside each player
status. `composeHud(stage_records)` and `composeStatus(parts)` concatenate
submitted records in those orders. `painterIndices(records)` gives the actual
reverse-submission order within layers 0–7. A `Text` record's empty string is
a caller-owned content placeholder, as in the reference vector.

`pulse(tick)`, `pendingLevelColour`, `nameColour`, `nameWidthBudget`,
`pulseGrey`, `glowStep`, and `glowSprite` model the status adjuncts. The caller
supplies the arena/player colours and blink mask; the service does not infer
player or PvP flags. `noticeFrame(slot,timers,alphas,counts,tick,network,
hide_counts)` returns its submissions and updated eight-entry timer/alpha
arrays plus cleared indices. It must be called only for a local slot.
`glowSprite` is a clipped sprite with destination clip `(0,0,1000,1000)`;
the compact dynamic `Sprite` record follows the research reference and leaves
the clip call choice to the renderer. Notice and gold icons use plain sprites.
`countActiveNotices` models the count-line suppression relationships. Network
notice placement uses the slot-zero column.

`messageFrame` returns text/frame submissions and updated alpha while the
message timer is stepped by its caller. `bossMeterFrame` returns text, fill,
frame and updated HP/maximum/alpha/previous state; the caller applies the
documented camera/menu/suppression gates. `goldFloaterStep` takes and returns
three raw 32-bit PS2 float lanes for position and velocity; `goldFloaterAlpha`,
`goldCounterStep`, and `goldCounter` model the remaining gold fades and
counter submissions. Gold placement uses the raw slot, including network
games. `selectorCentre`, `selectorDirection`, `selectorFadeStep`,
`selectorRadii`, `selectorRing`, and `selectorHighlight` expose quick-cast
geometry and fade. Selector highlighting uses displayed-frame `tick`; the
status pulse uses simulation-frame `tick`. All raw float calculations reuse the
existing `champions-core` PS2 arithmetic and trig implementation.

These functions produce documented draw calls and state transitions. The
caller decides whether to submit each section, owns any text content and art,
and supplies slot state. The composer never invents unresearched gates.

## Text layout API

`native/include/champions/ui_text_layout.hpp` takes existing `FontMetrics` and
UTF-16 code units. `glyphIndexWide` implements the signed byte map, index-45
fallback, wide code scan, and newline sentinel. `measureWide` handles NUL/count,
including the PvP font's interlaced/progressive kerning difference.
`glyphRun` returns source rectangles, half-pixel X positions, Y, tint and final
pen position. `TextState` carries batch colour and button-icon alpha; glyph
indices 0x5b–0x68 use neutral RGB 0x606060 and the documented alpha restore.
The only in-string control is newline; there are no colour codes.

`drawWrapped` and `drawWrappedCentered` return line calls, height and the
left-wrap shown count. The caller supplies line height, clip band, language,
and the left-wrap typewriter budget. Width comparison is strict; left wrap
splits long words, centered wrap does not. A unit that cannot advance at its
given width throws instead of reproducing the original infinite loop.
`centeredXWide`, `centeredXAscii`, `fitAscii`, and `textScissor` expose the
remaining documented layout rules. `fitAscii` returns bounded storage while
preserving the original shortening rule, including an ellipsis that may exceed
the requested pixel width.

## Native parity oracle

`native/include/champions/ui_composite.hpp` implements the research
compositor on synthetic RGBA textures: `normalize` plain/clipped/UI-layer
draws, `painterOrder`, `composite`, `compare`, and `diffRecords`. A `TextBatch`
is one reversed-submission packet whose glyphs paint forward. Plain sprites
gain one Y row. The 640×448 framebuffer uses point texels, tint modulation
by 128, alpha-test `>1`, blend `((Cs-Cd)*As >> 7)+Cd`, and colour clamp.
Explicit clips clamp at a half-open bottom of 447. `compare` ignores alpha and
supports masks, RGB tolerance, and vertical slack. `diffRecords` compares
normalized painter records and reports insert/delete/replace regions. This is
an ideal 2D oracle; it does not reproduce 16-bit dither, fields, or the final
present pass. No captured original frame is available for Tier A/B acceptance.

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
`ChampionsUiDynamicLayoutService` adds `draw_sequence`, `status_sequence`,
`painter_indices`, `pulse`, `status_adjuncts`,
`count_active_notices`, `notice_frame`,
`message_frame`, `boss_meter_frame`, `gold_counter`, `gold_floater_step`,
`gold_fade`, `selector_direction`, `selector_frame`, `text_layout`,
`text_scissor`, and `fit_ascii`. `pulse` and `selector_direction` return integers;
the remaining methods return dictionaries with `ok` and result/error fields. HUD records
include `kind` and `layer`; the service returns submissions in original order.
`text_layout` accepts FNT bytes and wide units as `PackedInt32Array` plus an
explicit count, wrapping/clip/line-height/budget inputs, and colour/alpha;
it returns glyph calls, measured width, wide centered X, and optional wrap runs. Gold position,
velocity, and selector timer cross the boundary as raw 32-bit integer lanes.
The integrated `original_art` panel consumes native base-layer records and
`painter_indices` for its explicitly labelled local-slot status-region view.
It loads selected local resources, omits unavailable observation bars, and
keeps unknown gates and full framebuffer parity unclaimed. See the latest
[playable integration report](PLAYABLE_WAVE_2026_10_17.md). The wrappers copy Godot values
into or out of the portable core and perform no layout arithmetic.

## Round 3

`ChampionsUiHudRound3Service` and `native/src/hud_round3.cpp` follow research
`t3/research-ui` `1ed740a2aed879eedad9fdc8a9ee0cceed8ad402`
(`docs/hud-dynamic.md` sections 7–10, `docs/ui-parity.md` items 1 and 4,
`tools/ref_hud_dynamic.py`, `tools/ref_ui_composite.py`,
`tools/difftest_ui_r3.py`). The synthetic rows are
`native/tests/vectors/hud_r3.tsv`, regenerated by
`tools/generate_hud_r3_vectors.py`. That covers minimap mode words, the
quarter-step chase and blip colours, the bust panel, flying-number lifetime and
local draw, the compass anchor and label, help and notice box geometry, the
arena step gate, medal and net-camera prompts, the DIMX matrix and
`dither5`, and present colour `trunc(2 × channel × (0.8 × brightness + 0.2))`.
`composite()` does not apply dither or present gain.

The play overlay (`godot/ui/hud_round3.gd`) draws nothing. It lists one
refusal per element in `refusals`. Flying numbers come only from
`damage_apply`, for non-player victims, with a shared-seed constructor draw.
The combat runtime refuses that allocation, and play has no `g_viewProj`.
The compass label string `0x86aaf0` and its four gates are not supplied. The
minimap projector `0x168cac` is undocumented. The help, notice, medal,
net-camera and bust overlays need frame art, `color_scale`/`g_uiPulse`,
`player_slotColor` and producers that do not exist yet. `bustPanel` refuses the
branch with a maximum, because the second wrap of that text batch has no
documented arguments. Its sprites carry only the documented alpha
(`tint_rgb_known` false). The service exports that alpha and no RGB.

## Font pages and HUD crops (round 4)

Research head `3e39392` on `t3/research-ui`. The embedded FNT page is one
16×16 palette plus one image rectangle. Its GIF packet starts at the u32 at
TEX +0x10. That offset is not a fixed 0x80. Palette destination base is the
halfword at packet +0x14. The 8-bit image base is the halfword at packet
+0x474. `decodeFontPage` walks that packet and reuses the PSMT8 sampler.
`decodeFontFile` validates the FNT tables and decodes the suffix. Glyph
rectangles stay in woven field space; pass `weave=true` for that image.
FNT header +2 has no reader. `refuseFntHeaderLineHeight` always throws, and
line height remains a caller argument.

`hudArtRound4` lists the 34 original-art identities. The source rectangle is
`(0, 0, used width, used height)` from the TEX header. Relocation does not
change that size. Bars are 100×8, the status frame and glow are 160×46, and
the button is 83×38. The frame tiles stay 32×32 and the center stays
320×224. The original-art catalogue page and crop use those used sizes.
`hudTextGlyphs` centers an ASCII string with the existing half-pixel run and
a required caller tint; there is no default. The HUD view takes the colours
from `docs/hud-dynamic.md` section 3. The level number uses base
`0x80323c3c`. While a level is pending it uses the pending-level colour of the
pulse. The view draws it only when the observation supplies `level` and
`acknowledged_level`. The name is refused, and the reason is in
`text_refusals`. Its colour depends on the arena colour, `player_slotColor`,
the blink mask `0x505b04` and `ui_fitText`, and play observes none of these.

## Play-view HUD (2026-10-21)

Since wave 17 the play view drew no HUD. Cause: audit fixes `efcc28a` (HUD-1,
HUD-2) and `80d90e6` (HUD-3..5) on `codex/audit-fixes`, merged by `d2e7a8b`,
reduced `godot/ui/hud_round3.gd` to an empty draw with a refusal per element.
That was correct: every element it drew was invented. Nothing documented took
its place, because the documented base HUD only ever ran inside the
original-art side panel (`hud_view.gd`), from art the user picked row by row.

`godot/ui/play_hud.gd` is that base HUD over the play view. It extends
`hud_view.gd` and uses the same native records. Evidence is research-ui
`3e39392`: `docs/hud-layout.md` (base HUD and pending overlays),
`docs/hud-dynamic.md` section 1 (order, painter rule) and section 3,
`docs/formats/ui-fonts.md` (HUD font) and `docs/ui-parity.md` section 1
(sprite, text and flat-rectangle GS state). For offline slot 0 (X = 12) it
draws what `hud_drawPlayerStatus` `0x1d9cd4` submits from observed values:

| Element | Origin, clip | Texture or colour | Tint |
|---|---|---|---|
| health bar | (27,46), clip (27,24,fill_right,124) | `bar_red{(tick>>2)%11}.tex` | `0x80808080` |
| pending health | rows 47..51, left S+a, right min(S+b,180) | flat | `0x80000060` |
| pending mana | rows 66..70, same arithmetic | flat | `0x805a4632` |
| mana bar | (27,63), same clip shape | `bar_blue{frame}.tex` | `0x80acacac` |
| status frame | (8,36), clip (0,0,1000,1000) | `hud_statusbar00.tex` | `0x80808080` |

All are layer 6. They are painted in reverse submission order, so the frame is
painted first. `hudPendingRects` / `pendingRight` (`native/src/ui_layout.cpp`)
implement the overlay arithmetic of hud-layout.md ("Known partial contracts",
O/high, not function-executed). They truncate C+pending before the ratio and
take the cap by original slot. `native/tests/vectors/hud_pending.tsv` holds 9
cases. `tools/generate_hud_pending_vectors.py` writes it with the pinned
`tools/ee_emu.py` float model, read with `git show 3e39392:` and checked by
SHA-256. A flat rectangle uses its RGBAQ word as is, with alpha 0x80 = 1.0.
Sprites modulate by tint/128. Present gain and dither are not applied.

The 640×448 canvas is shown as a 4:3 picture (640×480 square pixels) centred
in the view. Original widescreen anchoring is not established, so a wider view
leaves the sides empty. When a game folder opens, `real_data.gd` loads every
original-art catalogue identity from its named lump (`HUD.LMP` for bars and
frame). It uses the panel's checks (name, used size, PSM, decode, shape) and
takes a row only when exactly one row matches. The HUD font is `FONT.LMP`
`gamefontsmall.fnt`, the row startup stores in the small-font pointer
`0x4f7104`, which `hud_drawPlayerStatus` reads (ui-fonts.md). It is taken only
when exactly one row matches. That settles FONT-2's font choice for this view.
The original-art panel still accepts any row as an explicit choice.

On the game-folder levels (111-level sweep, scripts on and off) the view
draws the status frame and the health bar on every level. The session
reports no positive mana maximum there, so the mana bar is refused rather
than drawn from a stand-in.

Not drawn. `refusals` names each one and the missing input:
- **Experience bar, level text, level-up glow.** They need the acknowledged
  level +0x58, level +0x59, experience +0x5c and the 0x41b554 rows. Play
  observes none of them.
- **Name.** The FONT-1 refusal stands, and there is no player name.
- **Portrait.** `0x1d9cd4` submits none. The only HUD portrait is the bust
  panel, which only script `showBust` shows.
- **Quick slots.** Notice and count icons need starter timers. The
  quick-cast selector needs the D-pad, skill icons and resref `0x86aa30`.
- **Gold.** It is drawn only while its alpha is above 0, after a gold floater.
  Play credits no gold.
- **Minimap.** The projector `0x168cac` is unknown.
- **Boss meter and message.** Nothing produces them.

`hud_round3.gd` and its refusals are unchanged. `assumed` lists the documented
gates that play does not observe: the status suppressors (`0x41b4f8`,
`0x4f66a8`, `g_shopOpen` `0x41b50c`, camera mode, `net_isSlotActive`, vehicle
+0x540), the hit flash bit `0x41b930`, the mana cooldown +0x4ae and the dead
bit 0x8. The drawn elements take them as clear. That is the offline,
on-foot play state, and pending overlays are drawn only while health > 0.
Tests: `godot/tests/play_hud_smoke.gd` (authored images) and the opt-in
`godot/tests/play_hud_real_smoke.gd` (a user's game folder; counts only).

## Remaining gates

TEX flag 0x1000, FNT header +2, the minimap projector `0x168cac`, compass mesh
extents, and the arena hermite point stay refused. The pending-potion
overlays are implemented from the documented arithmetic but not executed
against the original. The selector prompt tail, caller-driven status
visibility, the per-screen font choice outside the base HUD, real framebuffer
dither and game-frame parity remain unverified. The native code does not infer
them.
