# Return to Arms original UI frames: evidence gate

Status: blocked before atlas construction. There are **zero documented frame
definitions** in the supplied evidence: none associates an original resource with
an explicit source rectangle and border contract. This is not a claim that the
game has no frame art. No replacement art, guessed rectangles, invented border
widths, font substitutes or HUD composition have been added.

## Evidence and scope

Read-only research baseline: `champions-return-to-arms-re` commit
`356add27d50e50972b2a671b31e8f4ec93e6778c`, inspected 2026-10-13.
Contracts are for the RTA US v2.00 baseline, not Norrath.

- [UI research](https://github.com/Open-Champions/champions-return-to-arms-re/blob/356add27d50e50972b2a671b31e8f4ec93e6778c/docs/ui.md): inventory, shop, front-end and HUD task ownership; HP/max HP record fields; menu behavior. It describes code observations rather than measured frame artwork/layout. Its panel words initially set to `0x140`/`0x1c0` do not establish texture rectangles, screen placement or border widths.
- [Format index](https://github.com/Open-Champions/champions-return-to-arms-re/blob/356add27d50e50972b2a671b31e8f4ec93e6778c/docs/FORMATS.md): GOB/LMP tables and texture dump naming. A dump hash is not a semantic frame identifier.
- [Model/texture contract](https://github.com/Open-Champions/champions-return-to-arms-re/blob/356add27d50e50972b2a671b31e8f4ec93e6778c/docs/formats/model-texture.md), texture header/storage sections: dimensions, palette/upload formats and interface fields. The other supplied format documents cover WORLD and animation/audio; they provide no UI frame catalog.
- [Render pipeline](https://github.com/Open-Champions/champions-return-to-arms-re/blob/356add27d50e50972b2a671b31e8f4ec93e6778c/docs/spec/render-pipeline.md), sections 3, 6 and 7: 2D coordinates, point sampling, sprite blend state and a partial font description.
- [Game messages](https://github.com/Open-Champions/champions-return-to-arms-re/blob/356add27d50e50972b2a671b31e8f4ec93e6778c/docs/spec/game-messages.md): inventory message columns/rows are gameplay grid arguments, not atlas or screen rectangles.
- [Startup](https://github.com/Open-Champions/champions-return-to-arms-re/blob/356add27d50e50972b2a671b31e8f4ec93e6778c/docs/startup.md), level load: `hud.lmp` resource group. [Runtime](https://github.com/Open-Champions/champions-return-to-arms-re/blob/356add27d50e50972b2a671b31e8f4ec93e6778c/docs/runtime.md) and [skills](https://github.com/Open-Champions/champions-return-to-arms-re/blob/356add27d50e50972b2a671b31e8f4ec93e6778c/docs/skills.md): `icons.lmp`, blank icon `icblank.tex` and the `ic<name>.tex` skill-icon convention. These publicly documented names do not identify frame art.

## Archive and texture identification

GOB records name contained resources; LMP records name entries and give bounded
payload offsets/sizes. Interface, effect and menu lumps contain `.tex` entries.
The documentation identifies the HUD resource lump and skill-icon lookup above,
but does **not** enumerate the HUD frame entries, health/mana layers, inventory
framing, shop framing, window border pieces, button states, cursor images or font
resource pairs. The exact containing archives and entries for those requested
frames therefore remain unknown. Neither name matching nor decoded appearance is
a documented layout contract. No private entry names are recorded here.

A lump texture has a 0x80-byte header: used width/height are u16 at +0/+2,
flags at +8, TEX0 fragment at +0x0c, packet offset at +0x10 and packet length
in qwords at +6. Allocated power-of-two dimensions do not equal a frame rectangle.
Supported native decoding covers PSMCT32, plain PSMT4 and the documented PSMT8
uploads/CLUT permutation. Four menu backgrounds described in the research use
PSMT4 uploaded as PSMCT32; that storage form remains unsupported by this decoder.

Interface sprite pictures store even lines in the upper half and odd lines in
the lower half. A progressive presentation must weave them: output row `2*k`
comes from stored row `k`, output row `2*k+1` from `k+h/2`. This permutation does
not alter colors. The flag `0x0040` alone cannot select weaving: the research
explicitly reports both field-stored and non-field-stored images with that flag.
Resource/draw-path identification is required before weaving a particular asset.

## Original coordinates and layout knowledge

| Fact | Established | Still missing |
|---|---|---|
| Screen | 640x448 UI layout displayed in a 4:3 composition; 3D target 1280x448 | Per-frame screen rectangles and safe-area offsets |
| Coordinate transform | Sprite `(x,y)` becomes GS `X=2*x+1408`, `Y=y+1824`; x grows right, y grows down; alternate field offsets are described separately | Frame-specific origin/pivot, alignment and clipping |
| Aspect | 640x448 storage has non-square display pixels; square-pixel equivalent composition is 640x480 (vertical factor 15/14) | No original widescreen anchoring contract; any extension would be explicitly presentation policy |
| Texture dimensions | Header supplies whole used dimensions | Semantic entry mapping, atlas subrectangles and whether whole textures are frames |
| Nine-slice | No frame slice definitions documented | Left/top/right/bottom source widths, corner/edge pieces, tiling vs stretching, minimum sizes; whether original code uses nine-slice at all |
| HUD health/mana | UI research lists HP/max HP at player +0xac/+0xb0; mana interpretation at +0xc0/+0xc4 is marked inferred | Frame/fill/mask resources; positions, sizes, fill direction, clipping, layering, variants for local player counts |
| Buttons/cursor | UI behavior and input ownership described | Resource pairs, state rectangles, hotspots, spacing and placement |
| Fonts | Texture, 256-entry map, 16-byte glyph records (code, UV bounds, y offset, advance, first kerning record), 6-byte kerning records, scissor clipping | File header/table offsets, field widths/signedness, UV units, kerning encoding, resource pairing, chosen fonts/scales and text baselines |

An integer enlargement of stored pixels and a correct 4:3 display aspect are
separate transforms: displaying 640x448 with equal x/y scale gives the wrong
aspect. A future renderer must preserve integer nearest-neighbor texel enlargement
and account for 15/14 vertical display correction in the composed viewport.
It must not claim every final pixel is an equal square integer block after that
aspect correction. Centered 4:3 composition is a possible modern presentation
policy, not evidence of original widescreen behavior.

## Count-only private decoder measurement

Local Apple Silicon measurement at client baseline `103aac5`, 2026-10-13.
Only private inputs beneath `../../private-validation` were measured; no private
files were modified and no decoded bytes, names, hashes or pictures were saved. Scratch
measurement code and its executable stayed under ignored `work/`.
The scratch C++17 counter called the existing `readGob`, `readLmp`, `payload`
and `decodeRtaTexture` APIs, visiting recognized loose files and container entries.
It counted successful decodes without inspecting or publishing pixels.

| Scope | Total occurrences | Decoded | Refused |
|---|---:|---:|---:|
| All TEX/ETEX payloads, loose plus nested | 2,336 | 2,211 | 125 |
| Header flag 0x0040 candidates (not confirmed UI) | 622 | 620 | 2 |
| Textures in identifiable `hud.lmp`/`icons.lmp` containers | 0 | 0 | 0 |
| Containers | 37 | 37 | 0 |

Loose inputs include 2,209 TEX and 7 ETEX files. Counts are occurrences, not
byte-unique assets or full-disc coverage. The corpus includes samples and negative
fixtures. Zero identifiable HUD/icon containers means the available containers
do not establish that resource mapping; it does not mean the loose textures
contain no HUD art. Available JSON metadata did not establish a complete UI
texture provenance catalog. **Exact UI-art coverage remains unmeasurable** until
UI resource identities are documented and mapped to these private inputs.
The candidate subset must never be labeled “620 UI frames decoded.”

Measurement commands (local scratch executable is not a shipped tool):

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
c++ -std=c++17 -O2 -I native/include work/ui_texture_counts.cpp \
  build/native/libchampions-archive.a build/native/libchampions-texture.a \
  -o work/ui_texture_counts
work/ui_texture_counts ../../private-validation
```

## Runtime path and integration gate

The current binding `ChampionsArchiveInspector.inspect_texture` in
`gdextension/src/archive_inspector.cpp` calls `read_selected` to validate the
archive fingerprint and selected row, rejects non-RTA/non-texture input, then
calls the native decoder and copies owned RGBA into Godot values. It returns
stored row order. It does not select original frame resources, weave interface
fields, supply atlas rectangles or recover fonts. Existing [texture preview](TEXTURE_PREVIEW.md)
is an inspection path; its alpha conversion is not a complete original blend
emulation. [Browser build/test instructions](GODOT_BROWSER.md) remain applicable
once a supported viewer exists.

**Upscaler integration point (planned, not implemented):** after native decoding
and documented per-resource field weaving, before Godot ImageTexture creation.
Keep logical rectangles and borders in original pixel units and map them to the
integer enlarged texture. Reuse a landed native upscaler rather than copying it;
use nearest-neighbor integer enlargement until then. At inspection,
`codex/item-upscale` points to the same `103aac5` baseline as this branch and
there is no upscaler implementation in this checkout's `native/`.

Implementation stops here under the task's no-guessing rule. An empty frame
catalog, synthetic decorative frame, arbitrary whole-texture selection or a
placeholder “UI frames” scene would not rebuild a documented original frame.
No native atlas API/tests, Godot viewer, nine-slice renderer, fonts or Session
HUD binding have been added. Simulation modules are untouched. Godot headless
and exported visual checks for this feature are not run because no supported
frame scene can be constructed from the available contracts.

To unblock, research must provide a revision-specific resource manifest plus
source rectangles and border measurements for at least one frame. For a composed
HUD, additionally provide exact destination rectangles, pivots, layer order,
health/mana fill and mask rules, local-player arrangements and text baselines.
For fonts, provide the bounded on-disc FNT layout and resource pairing described
as missing above. Record which UI assets require field weaving. Compare these
measurements with the original running UI; private screenshots/assets remain
outside the repository. Only then can synthetic atlas fixtures test the contract
and the runtime viewer display original art at 1x and integer-scaled sizes.

## Verification for this evidence-only change

On Apple Silicon macOS, Release configure and `cmake --build build/native -j 4`
passed. `ctest --test-dir build/native --output-on-failure -j 3` passed **100/100**
tests (1,590.20 seconds). No timeout retry was needed. These are existing native
baseline checks, not UI-atlas or visual-rendering tests. The count-only scratch
probe compiled and exited zero; authored empty-LMP and malformed-texture fixtures
also passed its grouping/refusal assertions. `git diff --cached --check` passed.
Godot headless, exported-app rendering, original-game visual comparison and
other target platforms remain unverified for UI frames because implementation
is blocked at the resource/layout evidence gate.
