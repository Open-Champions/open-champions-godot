# Return to Arms font data and explicit field shaping

PROGRESS: 2 of 2 reference functions ported

2026-10-14. Implementation commit: `772320d`. The denominator is exactly `Fnt.parse` and `_shape` in the pinned
research source. Both bounded valid-input contracts are independently authored
in portable C++17 and compared with executed GDScript outputs. This does not
count the full game-art loader, inferred `font()` renderer, HUD or Session.

## Immutable evidence

Research repository: `champions-return-to-arms-re`, immutable commit
`576cbb1a969464175c80e1834b92e6d72b8beaae` (RTA US v2.00 research, not Norrath).
Sources were read first from that checkout and verified against Git objects.
The sibling advanced during this task; generation now reads `git show <pin>:<path>`
and checks the source hash, independent of its current HEAD or working files.
No sibling files or assets were modified or imported into the client.

SHA-256 of full pinned source files:

| Source relative to research repository | SHA-256 |
|---|---|
| `docs/issues/native-gameart.md` | `dfd9d0b3b651329e7c5304e18ba480d04bafd42176dd40af39df1ff34e825b23` |
| `tools/godot/project/gameart/gameart.gd` | `12aab860371d01154776e1352b46634c41f9ef776158fed09a770c4c51d7e057` |
| `tools/godot/project/gameart/catalogue.gd` | `db1e91dd7f19806ed727dd40c16ae8304313fcfdcf6e3510ae5100257fddbee5` |
| `docs/spec/render-pipeline.md` | `3eb890d80325c7b04a584de04af8360fc8f4c0e412d3a21656b4e99db570a04b` |
| `docs/formats/model-texture.md` | `152a6073eff7b4c434774f24bd2b94076a56eb4216d2fd8e12f2b6ef1a1b6751` |

The issue reports its author's 122/122 original catalogue pixel comparisons and
loader timing/cache measurements. Those are **reported research validation**,
not validation executed in this checkout. We did not execute that loader or load
its disc, catalogue content, cache or art pack. The catalogue's page dimensions,
field-row counts and woven-space crop rectangles describe particular measured
inputs. They do not supply a universal half-height rule or authorize automatic
art identification. Render-pipeline section 7 supports raw glyph/kerning record
structure and original sprite/text draw context; it does not establish our
original layout, baseline or complete blend behavior.

## Portable font subset

`readRtaUiFont(Bytes)` in `native/include/champions/ui_font.hpp` and
`native/src/ui_font.cpp` returns owned ordered glyphs, ordered kerning records,
three offsets and an owned embedded texture suffix. It does not decode textures.
`decodeFontPage` in `ui_font_page.hpp` decodes that suffix when it is one
8-bit font page. A 32-bit lump suffix still uses `decodeRtaTexture`. FNT
header +2 is refused as a line height.

| Input | Implemented meaning |
|---|---|
| Minimum size | 0x214 bytes; not a claim that a table begins at 0x214 |
| +0 | little-endian u16 glyph count |
| +8 / +12 / +16 | little-endian u32 glyph / kerning / embedded texture offsets |
| Glyph stride | 16 bytes; seven returned fields at +0,+2,+4,+6,+8,+10,+12 |
| Returned glyph fields | code,u0,u1,v0,v1 as u16; signed s16 y offset; u16 advance |
| Kerning stride | 6 bytes; u16 left index, u16 right index, signed s16 amount |
| Kerning end | only complete records before texture offset; stop before left index 0xffff |
| Texture | every byte from texture offset through the bounded end of supplied input |

Duplicate codes, record order, signed extrema, unused metric values and raw
indices are preserved. No glyph lookup map, index reconciliation, sorting,
normalization or kerning application is performed. Bounded kerning offset at or
after texture offset yields an empty table, matching the reference. A final
partial kerning record is ignored, matching its loop condition. A terminator
need not exist; complete records through the bound are returned.

Unknown/unimplemented: header fields at +2..+7 and elsewhere, character-map
location/semantics, the glyph's eighth word at +14 and first-kerning-link behavior,
font choice/scales, original baseline and text placement. Raw UV bounds are not
validated against a decoded page and do not establish original draw semantics.
No speculative minimum offsets, table separation or metric range constraints
are added; overlapping bounded ranges are not guessed to be invalid.

Authored safety policy, distinct from parity: reject null input, input below the
minimum, glyph offset beyond input, insufficient complete glyph records,
kerning offset beyond input, or texture offset at/after input end. Validate
ranges using subtraction/division before record reads, avoiding offset/count
wrap on 32-bit hosts. No malformed-input behavior of unchecked GDScript reads
is claimed as an original format contract. Allocation failures are errors.

## Portable explicit shape subset

`shapeRtaUiArt` in `native/include/champions/ui_art_shape.hpp` and
`native/src/ui_art_shape.cpp` consumes decoded RGBA8 plus a caller-supplied row
count and optional crop. It returns owned pixels; channel bytes and order within
a row are unchanged. It never calls the texture decoder or converts alpha.

Supported domain: positive input width/height each 1..1024, exact
`width * height * 4` RGBA byte length, integer rows 0..input height. Rows 0 retain
the stored page. For positive rows, the uncropped output height is `2 * rows`;
row `2*k` selects stored row `k`, and row `2*k+1` selects
`min(rows+k,height-1)`. This deliberately preserves the reference's last-row
clamp even when rows exceed half-height. Odd and padded pages are supported.
Maximum uncropped output is 1024x2048 (8 MiB of RGBA8).

A crop is optional. When present, x/y are nonnegative integers, width/height
positive integers, and the entire rectangle must fit inside the resulting
stored-or-woven page. Crop coordinates are **after shaping**, not measurements
in the original stored page. No crop returns the entire resulting page.

Outside-domain safety refusals include zero/oversized dimensions, mismatched
pixel length, rows beyond source height, negative rows in the binding, zero-size,
negative, fractional or out-of-bounds crops and overflow-sized inputs. Godot's
out-of-bounds region padding and its negative-rows no-weave behavior are deliberately
unsupported here; their outputs are not parity claims. All bounded arithmetic
is checked before copying. Neither flag 0x0040, filename, page appearance nor a
catalogue measurement selects a row count automatically. No filtering,
recoloring, second alpha conversion, texture classifier or display-aspect
correction is part of this operation.

## Gallery integration

The existing gallery's **Open GOB / LMP / TEX / FNT** picker accepts an explicitly
selected loose FNT. Existing bounded archive/folder listing includes FNT entries
alongside the existing flag-based texture candidates. The 64 MiB source limit,
20,000 flattened rows, nested GOB/LMP validation and SHA-256 selection checks
are reused. Fonts are identified by explicit entry extension, never selected as
an automatic rendering font. Malformed FNT entries stay selectable to report an
error; listing does not claim their page dimensions before parsing.

`inspect_ui_font` validates the selected row and current file fingerprint,
refuses non-RTA/non-FNT selections, copies raw glyph/kerning metadata to owned
Godot arrays, and attempts the existing texture decoder on the embedded suffix.
Metadata remains successful/displayable when that decoder refuses. The existing
decoder retains its `min(255, raw * 255 / 128)` alpha inspection conversion;
we do not replace it with the research loader's doubled-alpha policy. Font parity
compares raw suffix bytes, and shape parity starts with already decoded RGBA8;
neither claim includes the research `Tex` decoder or complete loader. Pixel success
is separately reported by `page.ok`; no texture decoder file/semantics changed.

The gallery shows offsets, record counts, ordered raw values and a supported
embedded stored page. **Explicit shape preview** calls the native operation on
the retained decoded page with hand-entered rows (initially zero) and optional
crop (initially disabled). Stored mode retains the gallery's historical opaque
RGB inspection; shape mode creates an RGBA8 image from the exact native bytes.
This difference is labeled and is not original blending reconstruction. Both
panes keep 1x / nearest integer enlargement.

Invalid shape settings clear displayed pixels while retaining the decoded page
for correction or return to stored mode. New/failed/stale source selection clears
old metadata and pixels. Font measurement saves and all shaped-preview saves are
disabled. Existing texture measurement JSON remains schema version 1 in
`original_stored_pixels` / `stored_no_weaving`, with the same validation and
save-time fingerprint checks. No shaped rectangles are silently written as
stored measurements. No replacement art, font substitution or FontFile text
renderer is introduced.

## Executed synthetic validation

Host: Apple Silicon arm64, macOS 27.2 (26B5101f), Apple Clang C++17, optimized
Release builds. Runtime: Godot `4.5.2.stable.official.6ce3de25a`, official ZIP
SHA-512 verified against the existing project pin. godot-cpp was checked at
`e83fd0904c13356ed1d4c3d09f8bb9132bdc6b77`. Existing pinned dependencies were
reused read-only from the integration worktree; our binaries, tests and export
are this worktree's own builds.

`tools/ui_font_shape_vectors.py` writes authored inputs and extracts only the two
unchanged reference units into ignored `work/ui-font-shape-reference/`, then
runs a minimal Godot harness. It generates six font cases and 52 shape cases.
Native exact tests compare all successful output fields, suffix bytes, dimensions
and shaped pixel bytes. Vector SHA-256:
`4028d16231b4727903a7ff6413522436933692004b51d8e7ae54153468c1ec38`.
No original game content is required or loaded.

The 780 native assertions (font 636, shape 144) include separately labeled
malformed-input safety tests. Parity covers signed extrema, duplicate codes,
terminator exclusion, complete unterminated tables, partial kerning tails,
empty glyph/kerning tables, bounded reversed offsets, no weave, odd/padded pages,
clamped rows, and full/partial explicit crops. Safety covers every truncated
prefix through the texture bound, invalid/overflow offsets and counts,
null data, all short RGBA lengths, oversized pixels, dimensions, rows and crops.
Both test binaries also pass AddressSanitizer/UndefinedBehaviorSanitizer.
Direct x86_64 builds executed under Rosetta pass the same exact vectors and
assertions as arm64. This compares integer/byte results across CPU slices; it
is not native Intel hardware, Windows, Linux or a Session trace verification.

Headless source and actual exported debug app both pass the existing 45 gallery
checks plus 23 font/shape checks. The separate archive suite passes 283 checks.
New gallery checks exercise loose and nested duplicate FNT selection, metadata,
unsupported embedded pages, raw signed fields, stored/woven switching, exact
RGBA/no-weave behavior, crops, invalid recovery and stale archive fingerprints, and TEX shaped-mode save refusal with unchanged
stored-coordinate measurement behavior.

The actual Universal 2 debug export also executed with the compatibility renderer
on Apple M5 Pro. Its synthetic control transitions and viewport capture checks
pass **88 assertions** (45 existing + 43 font/shape/render checks). Rendered
1050x720 gallery captures, including scrolled previews, were inspected for font
selection/metadata, stored/woven pages, explicit crop, invalid crop clearing and
stored-mode recovery. This found and fixed cramped kerning metadata and detached
crop controls; both now display correctly. Captures contain only authored pixel
grids and raw synthetic records; copies are retained under ignored
`work/ui-font-checks/rendered/`.

Physical mouse/file-picker inspection could not execute: Computer Use returned
`cgWindowNotFound` for the exact export twice and timed out when resolving the
running bundle. The screenshots are exported Godot viewport output, and control
transitions were driven by the synthetic test harness, not a successful desktop
click session. Initial visual-test assertions incorrectly required an entire
label control to fit horizontally rather than checking the visible status text;
a subsequent GDScript type-inference error in that check was also corrected.
Final headless and rendered suites have no script errors or failed assertions.
The exported debug binary/library include arm64 and x86_64; graphical execution
here was arm64 only.

The full optimized native run completed **125/130 passes**, exit 8, in
3150.25 seconds. Five cases failed due to time limits: `skills-a-exact` and
`player-exact` reached CTest timeouts; `monsters-1-exact` and `skills-b-exact`
reported subprocess timeouts; `skills-c-exact` reached its CTest timeout.
All font/shape, archive and texture cases passed. Each affected timeout was
rerun with a single-test CTest filter; results below are separate from the
full run. No files belonging to these other agents were edited. No successful
before-change baseline run was performed here, so these are out-of-scope
validation failures, not proof of a pre-existing failure.

| Individually filtered retry | Result | Elapsed seconds |
|---|---|---:|
| `player-exact` | CTest timeout; exit 8 | 240.30 |
| `monsters-1-exact` | CTest timeout; exit 8 | 241.28 |
| `skills-b-exact` | subprocess timeout after 120 seconds; exit 8 | 142.59 |
| `skills-a-exact` | CTest timeout; exit 8 | 120.66 |
| `skills-c-exact` | CTest timeout; exit 8 | 243.06 |

These retries did not establish successful full-suite validation. The bounded
font/shape operations and gallery path are complete with their own executed
checks; the broader client suite is not green on this run. Timeout diagnostics
are retained in `work/full-ctest.log` and `work/retry-*.log`.

Commands executed from this worktree (all final affected checks pass; full-suite
results and initial corrected failures are distinguished above). Logs stay under
ignored `work/`; build output stays under `build/`.

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native --output-on-failure -R '^(ui_font|ui_art_shape)$'
ctest --test-dir build/native --output-on-failure -R '^(archive-parser|texture-parser|texture-upscale)$'
ctest --test-dir build/native --output-on-failure -j 3
ctest --test-dir build/native --output-on-failure -R '^player-exact$'
ctest --test-dir build/native --output-on-failure -R '^monsters-1-exact$'
ctest --test-dir build/native --output-on-failure -R '^skills-b-exact$'
ctest --test-dir build/native --output-on-failure -R '^skills-a-exact$'
ctest --test-dir build/native --output-on-failure -R '^skills-c-exact$'
python3 tools/ui_font_shape_vectors.py --research ../../champions-return-to-arms-re \
  --godot ../integration/work/runtime/Godot.app/Contents/MacOS/Godot
cmake -S gdextension -B build/godot-extension \
  -DGODOT_CPP_DIR="$PWD/../integration/work/deps/godot-cpp" \
  -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug \
  '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64'
cmake --build build/godot-extension --config Release --parallel 4
../integration/work/runtime/Godot.app/Contents/MacOS/Godot \
  --headless --path godot --editor --quit-after 120 --frame-delay 50
../integration/work/runtime/Godot.app/Contents/MacOS/Godot \
  --headless --path godot --quit-after 300 --script res://tests/archive_smoke.gd
../integration/work/runtime/Godot.app/Contents/MacOS/Godot \
  --headless --path godot --quit-after 300 --script res://tests/ui_art_gallery_smoke.gd
../integration/work/runtime/Godot.app/Contents/MacOS/Godot \
  --headless --path godot --export-debug 'macOS universal' \
  "$PWD/build/exports/UIFontGallery.app"
'build/exports/UIFontGallery.app/Contents/MacOS/Open Champions' \
  --headless -- --browser-smoke-test --ui-gallery-smoke-test
'build/exports/UIFontGallery.app/Contents/MacOS/Open Champions' \
  -- --browser-smoke-test --ui-gallery-visual-test
lipo -archs godot/bin/champions_archive.macos.template_debug.dylib
git diff --check
```

Direct sanitizer / second-architecture commands (both units executed):

```sh
c++ -std=c++17 -O1 -g -fsanitize=address,undefined -I native/include \
  -DUI_FONT_SHAPE_VECTORS=\"$PWD/native/tests/ui_font_shape_vectors.txt\" \
  native/src/ui_font.cpp native/src/ui_art_shape.cpp native/tests/ui_font_tests.cpp \
  -o work/ui-font-checks/font-asan
work/ui-font-checks/font-asan
c++ -std=c++17 -O1 -g -fsanitize=address,undefined -I native/include \
  -DUI_FONT_SHAPE_VECTORS=\"$PWD/native/tests/ui_font_shape_vectors.txt\" \
  native/src/ui_font.cpp native/src/ui_art_shape.cpp native/tests/ui_art_shape_tests.cpp \
  -o work/ui-font-checks/shape-asan
work/ui-font-checks/shape-asan
c++ -std=c++17 -O2 -arch x86_64 -I native/include \
  -DUI_FONT_SHAPE_VECTORS=\"$PWD/native/tests/ui_font_shape_vectors.txt\" \
  native/src/ui_font.cpp native/src/ui_art_shape.cpp native/tests/ui_font_tests.cpp \
  -o work/ui-font-checks/font-x64
arch -x86_64 work/ui-font-checks/font-x64
c++ -std=c++17 -O2 -arch x86_64 -I native/include \
  -DUI_FONT_SHAPE_VECTORS=\"$PWD/native/tests/ui_font_shape_vectors.txt\" \
  native/src/ui_font.cpp native/src/ui_art_shape.cpp native/tests/ui_art_shape_tests.cpp \
  -o work/ui-font-checks/shape-x64
arch -x86_64 work/ui-font-checks/shape-x64
```

## Remaining original font/HUD gates

Private acceptance measurements were skipped: `../../private-validation` is
unavailable per the owner; no substitute location was searched. No original
pixels, game text, names, decoded data or assets are stored in public fixtures.
Original-game comparisons, other RTA revisions, Norrath, Windows/Linux graphics,
Intel hardware and release-ABI exports remain unverified for this change.

Remaining font evidence includes the unused header/map/link semantics, original
chosen resources/scales, baseline and text-layout rules, clipping integration,
kerning application and original blend/shadow rules. The research renderer's
inferred Open Sans cap ratio, shadow deductions, fallback baselines and descent
multiplier are deliberately not implemented. No substitute fonts, automatic
font choices, ISO reader, art-pack discovery, cache download, complete loader,
HUD composition, nine-slice contract or Session work belongs to this subset.
Original composition still needs reviewed resource mappings, source/destination
rectangles, pivots, layers, fill/mask rules and original-game comparisons.
