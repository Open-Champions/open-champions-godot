# Item-art upscaling in the texture inspector

This is optional presentation processing of decoded RGBA8, off by default. It
changes neither archives nor simulation. Select a supported RTA `.tex`/`.etex`,
preview it, choose a filter and 2×/3×/4×. The decoded **Before** and processed
**After** views share the alpha-channel inspection toggle. Both views clear on
archive, entry, search or profile changes. Nearest display sampling avoids a
second hidden smoothing pass. Off has no processed image.

## What is item art, and what can already be decoded?

Evidence: read-only `champions-return-to-arms-re`, revision
`356add27d50e50972b2a671b31e8f4ec93e6778c`, targeting RTA US v2.00.
The sources below are format/behavior notes, not imported implementations or assets.

- [Model/texture format](https://github.com/Open-Champions/champions-return-to-arms-re/blob/356add27d50e50972b2a671b31e8f4ec93e6778c/docs/formats/model-texture.md): item models are `.vif` lumps with same-stem `.tex` companions in the same LMP. The model stores no texture filename/material record; callers supply the texture. Level `<LEVEL>.TEX` cells are a separate container. `.etex` is the environment texture form, not an inventory-icon classification.
- [Item tables](https://github.com/Open-Champions/champions-return-to-arms-re/blob/356add27d50e50972b2a671b31e8f4ec93e6778c/docs/data-tables.md), §1: definitions carry a picture key and model resource identity; `g_itemModels` has model/texture filenames and loaded pointers.
- [Startup notes](https://github.com/Open-Champions/champions-return-to-arms-re/blob/356add27d50e50972b2a671b31e8f4ec93e6778c/docs/startup.md), Picture cache: inventory/list pictures stream from `piccache.dat`, indexed by `piccache.hdr`. Groups carry name stems and entries carry suffix numbers, first sector and sector count (2048-byte units), and width/height. The runtime builds DMA block chains and a picture header. `item_buildPictureFilename` also supplies worn-equipment texture names; the equipment stream is outside that investigation. `wcache.hdr/dat` is inferred to be the weapon/equipment cache, not the world texture container.
- [Item specification](https://github.com/Open-Champions/champions-return-to-arms-re/blob/356add27d50e50972b2a671b31e8f4ec93e6778c/docs/spec/items.md), §12: picture cache/filename behavior is outside the exact item reference tests. The native item rules do not supply loaded art.

The existing decoder (`texture.hpp`, `texture.cpp`, [preview contract](TEXTURE_PREVIEW.md)) accepts bounded unrelocated 0x80-byte lump headers: one PSMCT32 image, plain low-nibble-first PSMT4 with 16 RGBA colors, and documented PSMT8 uploads with a 256-color CLUT. Thus a supported item-model companion texture already yields owned RGBA8 and can use this feature. Unsupported 4-bit-as-32-bit uploads, relocated headers and world cells still refuse. No `piccache.hdr/dat` or equipment-cache reader exists here: **inventory-cache icons are not newly decoded by this change**. A picture already available as a supported standalone lump can be inspected, without certifying its original item association.

The format notes explain stacked even/odd fields for interface pictures. The
current decoder preserves stored rows and does not weave fields. Do not infer
field layout solely from flag 0x40 or weave every model texture. Item-specific
filename resolution, picture-cache extraction and correctly oriented inventory
rendering need their own reader and evidence. RTA assumptions are not applied to Norrath.

## Owner fidelity policy (round 2)

The game's decoded art is the only input. No native filter redraws, recolors or
substitutes art. Upscaling remains off initially. The first enabled choice is
**Nearest, 2×**, the measured faithful default. All three factors tie at zero
error for nearest; 2× wins that tie by smallest output allocation. Pixel-art and
bicubic remain explicit alternatives and display their measured error, rather
than implying equal preservation.

`measureTextureFidelity` is portable integer C++17. For each original pixel it
sums the corresponding factor × factor output block per RGBA channel. It reports
`sum(abs(block_sum - source_channel * factor²)) / factor²`: the exact total
absolute byte error after box downsampling, with no rounding that could hide a
small change. Divide that total by original RGBA byte count for mean absolute
error. It also reports the largest channel error, output pixels with RGBA tuples
absent from the original palette, changed pixels in a clamped uniform 3×3 source
neighborhood, changes to 0/255 source alpha blocks, and exact factor/aspect ratio.
Hidden transparent RGB is included: these bytes can matter for model inspection.
This is a preservation test, not a claim of perceptual quality or PS2 rendering.

All native filters now restore exact source RGBA in flat neighborhoods and
source-alpha-zero blocks; alpha-255 blocks retain exactly 255. This overrides
smoothing at hard masks and preserves their block-aligned silhouette. Partially
transparent transitions still use premultiplied bicubic. Output dimensions must
be precisely the integer factor on both axes. The Before pixels stay untouched.
RGB inspection is explicitly measured against an opaque presentation copy;
ordinary alpha inspection and AI replacements are measured against decoded RGBA.
Palette counters use complete RGBA tuples, so combining a neighbor's RGB with a
restored alpha can count as a new tuple even in the edge filter.

Exact fidelity acceptance requires **zero** round-trip error, new palette pixels,
flat-region changes and hard-alpha changes, plus preserved aspect. There is no
user tolerance switch. Native alternatives report failures for comparison;
external replacements are refused on any failure. Passing these checks is
necessary, not sufficient, to establish the original game's appearance.

### Measured synthetic ranking

Six authored inputs (24 source pixels total) cover corners, multi-color edges,
binary-alpha/hidden RGB, singleton flat color, and partial alpha. Tests also
assert literal 6/4 downsample error, palette/flat counts, alpha mutation refusal,
aspect refusal and nearest's zero error at all factors. Exact integer output
and ranking agree on arm64 and x64 (x64 executed through macOS compatibility).

| Factor | Nearest total error | Pixel-art total error | Bicubic total error | New palette pixels, nearest / edge / cubic |
|---|---:|---:|---:|---:|
| 2× | 0/4 | 810/4 | 2477/4 | 0 / 0 / 64 |
| 3× | 0/9 | 1920/9 | 5064/9 | 0 / 0 / 110 |
| 4× | 0/16 | 4560/16 | 10544/16 | 0 / 0 / 240 |

All native cases have zero flat and hard-alpha changes and exact dimensions.
Rank by round-trip error, then preservation violations, then allocation on ties:
**nearest → pixel-art → bicubic** at each factor in these measurements.

## Portable filters

`native/include/champions/texture_upscale.hpp` exposes `upscaleTexture` and
`TextureFilter`. No Godot or additional library is linked.

| Filter | Rule |
|---|---|
| Nearest | Replicate each exact RGBA tuple in an integer square. |
| PixelArt | Handwritten Scale2x/EPX and Scale3x neighborhood rules; 4× is two 2× passes. Compare all four channels exactly; replicate edge pixels for neighbors beyond the boundary. |
| Bicubic | Separable 4-tap Catmull–Rom cubic, pixel-center coordinates, clamped borders. Signed integer rational weights, 64-bit accumulators, nearest integer rounding. |

Pixel-art rules were authored from Andrea Mazzoleni's published
[algorithm description](https://www.scale2x.it/algorithm); the author explicitly permits use of the algorithm on the [download page](https://www.scale2x.it/download). No GPL implementation,
xBRZ, hqx, SIMD source, or upstream code file is incorporated. This is a new
implementation under this repository's license, not a relicensed upstream package.

Bicubic multiplies RGB by alpha before interpolation, interpolates alpha with the
same weights, clamps negative/ringing premultiplied colors, then divides by the
interpolated alpha. The interpolation kernel returns zero RGB at rounded zero alpha. The final
preservation pass restores source transparent RGB and hard alpha masks; hidden
RGB does not contribute to premultiplied interpolation. Filtering is in decoded byte color space,
not linear light; this is an inspection choice. Original PS2 inverted/model alpha
and sprite blend equations are not recovered by resampling. The decoder's existing
0..128 to 0..255 alpha conversion precedes all filters.

Input must be nonempty, exactly width × height × 4 bytes, at most 1024 per axis.
Factors are only 2, 3 and 4. Dimensions use 64-bit arithmetic before allocation;
output is capped at 4096 per axis and 64 MiB. Enum values outside the three filters
refuse. Allocation failure propagates to the existing bridge error boundary.
All output arithmetic is integer-based, independent of host floating-point modes.
This does not alter authoritative game-state traces.

The original four-argument `inspect_texture` remains unchanged. The explicit
`inspect_texture_upscaled(path,row,profile,fingerprint,filter,factor,opaque_rgb=false)` reuses its
archive/profile/fingerprint validation, retains original fields, and adds
`upscaled_rgba`, `upscaled_width`, `upscaled_height`. Filter strings are `nearest`,
`pixel-art`, `bicubic`; failures return `ok=false` and an error. The optional
`opaque_rgb` is a presentation choice, never inferred from a title/texture flag.
The UI sets it for RGB inspection and regenerates with it off for alpha inspection.
It forces a temporary input copy's alpha to 255 and retains the original RGBA.
This is necessary because stored model alpha zero can denote opaque model colors;
ordinary transparent-image interpolation would erase those colors. The portable
API and the bridge default always use ordinary decoded alpha.

## Optional external AI cache

Choose **External AI cache** to load a replacement if it exists. Preview alone
never exports art or launches a process. **Export AI cache input** explicitly writes
an input PNG and JSON manifest under `user://texture-upscale-cache/v2/`, Godot's
user data folder (the resolved absolute path is shown after export). The exported input preserves decoded RGBA. External output must preserve it under
the exact fidelity policy above; an RGB-only result fails whenever original alpha
is not opaque. This does not recover the game’s blend rule. Game art
must stay there, outside any checkout. Nothing is downloaded or uploaded.

The filename key is SHA-256 of UTF-8:

```
oc-upscale-v2\n<width>\n<height>\nexternal-rgba\n<factor>\nrealesrgan-x4plus\n
```

followed by the complete decoded row-order RGBA8 bytes. It includes dimensions,
decoded RGBA mode, factor, recipe/model name and format version. `<key>.png` is the replacement;
`<key>.input.png` and `<key>.json` are the optional job input. A missing PNG shows
an explicit cache miss; there is no automatic fallback. The PNG header, dimensions
and 64 MiB file cap are checked before decoding; it must be exactly factor × the
source size, at most 4096 per axis. Malformed/wrong-sized replacements clear After.
RGBA is retained by Godot, then the real native `texture_fidelity` binding gates
the replacement before display. Version 1 cache entries are not reused. External
output and quality are not deterministic promises.

With an already installed [Real-ESRGAN ncnn Vulkan](https://github.com/xinntao/Real-ESRGAN-ncnn-vulkan#full-usages) compatible binary and the
`realesrgan-x4plus` model available to that tool:

```sh
python3 tools/fill_texture_upscale_cache.py /absolute/user/cache/<key>.json \
  --executable /absolute/path/to/realesrgan-ncnn-vulkan \
  --godot /absolute/path/to/Godot
```

The helper uses a subprocess argument array (no shell), validates exported-input
SHA-256 and bounded settings, rejects cache paths inside repositories and image
symlinks, and invokes `-i INPUT -o OUTPUT -n realesrgan-x4plus -s FACTOR -f png`.
The CLI reads a bounded, hash-verified temporary PNG snapshot, so the exported input cannot change between verification and inference. It runs from the installed executable folder, so the tool’s default `models/` folder is resolved there. It checks output dimensions, invokes `godot/tools/cache_fidelity.gd` headlessly
against the input snapshot and candidate, and only then publishes by atomic rename
within the user cache. That script uses the identical native fidelity check; a
missing runtime/binding or failing check refuses publication and preserves any
previous cache entry.
It has a ten-minute timeout and downloads no binaries/models. If your CLI does
not support the requested scale or PNG, it is not a compatible recipe;
use the native filter or an independently prepared replacement PNG. Changing
model/recipe requires a new version/key contract, not overwriting another recipe's
identity. Re-select/refresh preview after the helper finishes. No CLI is invoked
in public tests; synthetic fake-CLI tests verify the handoff boundaries.

## Validation and remaining work

Synthetic native tests cover exact nearest blocks, Scale2x/3x corners, composed
4×, hard-alpha preservation at all factors and premultiplied partial-alpha transitions, reversed ramps, transparent hidden
RGB, singleton/clamped edges, invalid enum/factor and malformed/extreme dimensions.
The Godot smoke exercises all nine native options, retained Before pixels, After
alpha display, cache miss/hit/wrong-size, key changes, Norrath refusal and cleanup.

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^texture-upscale$' --output-on-failure
ctest --test-dir build/native --output-on-failure -j 3
# Build/import the pinned Godot dependencies as in GODOT_BROWSER.md, then:
godot --headless --path godot --quit-after 120 --script res://tests/texture_upscale_smoke.gd
godot --headless --path godot --quit-after 120 --script res://tests/archive_smoke.gd
python3 -m unittest discover -s tools -p 'test_texture_upscale_cache.py'
```

Applying upscaling to model-viewer materials or actual equipped/in-scene item
models is outside this feature. The client has no integrated equipment/item
rendering scene. UV/material/alpha semantics and cache-based resource assignment
need separate work; a larger preview does not establish original visual fidelity,
playability, inventory support or Norrath compatibility. Round 2 count-only decoder/upscaler measurements are recorded below; installed
AI model quality remains unverified. Target-platform visual evidence and exact
commands/results are recorded with the local completion report.

## Local verification on 2026-10-13

Apple Silicon, Godot 4.5.2 with pinned godot-cpp; native C++ Release. All **101 native CTest cases passed** with `-j 3` (1577.96 seconds), including the new exact-pixel upscaler case. The existing
browser smoke passed **283 checks**, and the new texture-upscale smoke passed in
both the source project and an actual Mac debug export. The exported application
also ran graphically with the synthetic fixture: Before/After, 4× bicubic, factor
and filter controls, alpha toggle, and scrolling were inspected in its rendered
viewport. Opaque RGB resampling retains alpha-zero colors; round 1 ordinary premultiplied resampling returned transparent black. Round 2
now restores uniform hidden RGB, as required by the owner’s preservation policy.
The screenshot stays in the user's data folder and contains authored pixels only.
This local export has an arm64 extension; it is not verified as an Intel or release
client package. The optional external inference binary/model was not installed or
run. Six mocked CLI/cache boundary tests passed in round 1; round 2 adds fidelity-refusal coverage.

The exact build and additional verification commands used from this worktree:

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^texture-upscale$' --output-on-failure
ctest --test-dir build/native --output-on-failure -j 3
cmake -S gdextension -B build/godot-extension \
  -DGODOT_CPP_DIR="$PWD/../../open-champions-godot/work/deps/godot-cpp" \
  -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug
cmake --build build/godot-extension -j 4
../../open-champions-godot/work/runtime/Godot.app/Contents/MacOS/Godot \
  --headless --path godot --editor --quit-after 120 --frame-delay 50
../../open-champions-godot/work/runtime/Godot.app/Contents/MacOS/Godot \
  --headless --path godot --quit-after 120 --script res://tests/texture_upscale_smoke.gd
../../open-champions-godot/work/runtime/Godot.app/Contents/MacOS/Godot \
  --headless --path godot --quit-after 120 --script res://tests/archive_smoke.gd
python3 -m unittest discover -s tools -p 'test_texture_upscale_cache.py'
clang++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -fsanitize=address,undefined \
  -I native/include native/src/texture_upscale.cpp native/tests/texture_upscale_tests.cpp \
  -o build/upscale-sanitized
build/upscale-sanitized
clang++ -std=c++17 -O2 -arch x86_64 -I native/include \
  native/src/texture_upscale.cpp native/tests/texture_upscale_tests.cpp -o build/upscale-x64
build/upscale-x64
../../open-champions-godot/work/runtime/Godot.app/Contents/MacOS/Godot \
  --headless --path godot --export-debug 'macOS universal' \
  "$PWD/build/exports/upscale-review/OpenChampions.zip" --quit
unzip -q build/exports/upscale-review/OpenChampions.zip -d build/exports/upscale-review/unpacked
'build/exports/upscale-review/unpacked/Open Champions.app/Contents/MacOS/Open Champions' \
  --headless --quit-after 120 --script res://tests/texture_upscale_smoke.gd
'build/exports/upscale-review/unpacked/Open Champions.app/Contents/MacOS/Open Champions' \
  --quit-after 120 --script res://tests/texture_upscale_smoke.gd -- --visual
```

The sanitizer and x64 exact-pixel executables both printed PASS. These compare
synthetic integer filter outputs on both architectures on this Mac (x64 via its
compatibility runtime); they do not establish Windows client support or replace
simulation state-trace verification. All game data remained outside the checkout.


## Round 2 item-art coverage and evidence boundary

The scaler accepts **all** bounded RGBA8 that `decodeRtaTexture` produces,
including 32-bit, plain 4-bit, 8-bit, model companions, interface art and ETEX;
it has no filename-based item whitelist. No model texture is excluded by an
item classification assumption. It does not yet attach results to scene models.

`champions-texture-fidelity --rta PRIVATE_DIRECTORY` recursively reads loose
TEX/ETEX and bounded GOB/LMP payloads without extraction or writes. It emits only
counts and aggregated fidelity statistics, never names, hashes or pixels.
Counts are occurrences; explicitly select a deduplicated corpus for unique-byte
counts. Successful upscaling and exact-fidelity acceptance are separate columns.
Decoder refusals stay in the denominator.

The read-only model/texture validation collection has **40 texture occurrences**
in **20 containers**, **38 decoded**. Every filter/factor upscaled **38/38**
decoded inputs (100%; 38/40, 95% of supplied texture occurrences). Nearest passes
38/38 exact fidelity; edge and cubic each pass 24/38. This is model/texture sample
coverage, not an asserted inventory of item-only art.

| Factor | Nearest error | Pixel-art error | Bicubic error | New palette pixels, nearest / edge / cubic |
|---|---:|---:|---:|---:|
| 2× | 0/4 | 1532145/4 | 5909792/4 | 0 / 746 / 545207 |
| 3× | 0/9 | 4096128/9 | 13673182/9 | 0 / 1876 / 1069684 |
| 4× | 0/16 | 7452475/16 | 26549631/16 | 0 / 3400 / 2141997 |

All have zero flat-region/hard-alpha changes and exact aspect ratio. Ranking
again selects nearest at every factor. Original RGBA remains private.

### Broader private texture corpus (count-only)

The byte-deduplicated RTA US v2 corpus contains **2,188 TEX/ETEX inputs**.
The decoder accepts **2,065** and refuses **123**. All nine filter/factor choices
upscale **2,065/2,065 decoded inputs (100%)**, or **2,065/2,188 supplied texture
inputs (94.38%)**. Nearest passes exact fidelity on **2,065/2,065** at all factors;
edge passes 923/2,065 and bicubic 907/2,065. Unsupported source layouts remain
explicit decoder refusals, not fabricated images. The corpus covers every stored
texture category available to this decoder, but does not identify which inputs
are item art; those percentages must not be relabeled as item-only coverage.

| Factor | Nearest error | Pixel-art error | Bicubic error | New palette pixels, nearest / edge / cubic |
|---|---:|---:|---:|---:|
| 2× | 0/4 | 185188606/4 | 770817849/4 | 0 / 106428 / 154662375 |
| 3× | 0/9 | 491441135/9 | 1837514730/9 | 0 / 271833 / 307274037 |
| 4× | 0/16 | 931285424/16 | 3421499814/16 | 0 / 523157 / 588518177 |

All native results preserve aspect, flat neighborhoods and hard-alpha masks.
This independently confirms **nearest → pixel-art → bicubic** at each factor,
with nearest's exact original palette and zero box error. The measured synthetic
output byte totals are 384 at 2×, 864 at 3×, and 1,536 at 4×; this allocation tie
break selects 2× after nearest's equal zero fidelity error across factors.
No decoded images, art names or per-input hashes enter the repository or report.

### Inventory-cache refusal: missing documented pixel contract

Read-only research remains at `356add27d50e50972b2a671b31e8f4ec93e6778c`;
the reconstruction sibling is at `adb1e7c3a9b1c41e86aed3f9f73cfef95c088c36`.
Searching their documentation/reference tooling found the cache index notes in
`docs/startup.md` above but no executable pixel contract for its sector payloads.
The index establishes dimensions and sector ranges. It does **not** establish
serialized pointer bases, sector payload/header placement, palette/PSM/upload
layout, how the runtime synthesizes its 64-byte picture header and block-chain
markers, or inventory field orientation. It cannot establish that those sectors
are standalone 0x80-byte TEX lumps. Worn-equipment cache payloads also lack an
independent documented decoder. No speculative reader was added.

The available private corpus is byte-deduplicated by extension and carries no
item-model table association manifest, executable or PICCACHE HDR/DAT pair.
`g_itemModels` has 134 rows in the documented table, but a row count is not an
art count or item-specific sample denominator. Therefore an **item-only private
coverage percentage** and cache-icon decode/upscale coverage cannot be verified.
Completion of owner item (d) is blocked on the serialized cache pixel contract
and read-only associated private item inputs (or an owner-verified association
manifest). Broader texture measurements do not silently substitute for that proof.

### AI settings and limits

Use the original pixels, fixed integer scale, PNG output and the exported recipe.
Do not use face enhancement, color correction, sharpening, diffusion redraw,
extra denoising or alternate source art. The compatibility recipe uses
`realesrgan-x4plus` with no extra enhancement switches. Its learned model can
still invent detail; these settings are not a fidelity guarantee. Prefer nearest
for guaranteed preservation. Any AI output with nonzero measured error, new RGBA
palette entries, altered flat regions/alpha edges or wrong aspect is rejected,
including seemingly attractive results. No installed AI binary/model was run.


## Round 2 verification commands and results (2026-10-13)

Apple Silicon native Release and pinned Godot 4.5.2/template_debug binding.
The modified files are the native upscaler/header/exact tests, count-only CLI and
its additive CMake target; GDExtension texture bridge; Godot preview UI/cache,
headless cache checker and smoke; optional CLI helper/tests; and this document.
No shared simulation targets, roadmap, workflows or sibling files were edited.

```sh
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
ctest --test-dir build/native -R '^texture-upscale$' --output-on-failure
ctest --test-dir build/native --output-on-failure -j 3
cmake -S gdextension -B build/godot-extension \
  -DGODOT_CPP_DIR="$PWD/../../open-champions-godot/work/deps/godot-cpp" \
  -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug
cmake --build build/godot-extension -j 4
../../open-champions-godot/work/runtime/Godot.app/Contents/MacOS/Godot \
  --headless --path godot --editor --quit-after 120 --frame-delay 50
../../open-champions-godot/work/runtime/Godot.app/Contents/MacOS/Godot \
  --headless --path godot --quit-after 120 --script res://tests/texture_upscale_smoke.gd
../../open-champions-godot/work/runtime/Godot.app/Contents/MacOS/Godot \
  --headless --path godot --quit-after 120 --script res://tests/archive_smoke.gd
GODOT_FIDELITY_RUNTIME="$PWD/../../open-champions-godot/work/runtime/Godot.app/Contents/MacOS/Godot" \
  python3 -m unittest discover -s tools -p 'test_texture_upscale_cache.py'
build/native/champions-texture-tests --write-fixtures 'build/native/texture fixtures'
build/native/champions-texture-fidelity --rta 'build/native/texture fixtures'
build/native/champions-texture-fidelity --rta ../../private-validation/rta-disc-20261010
build/native/champions-texture-fidelity --rta ../../private-validation/rta/textured-model-validation
clang++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -fsanitize=address,undefined \
  -I native/include native/src/texture_upscale.cpp native/tests/texture_upscale_tests.cpp \
  -o build/upscale-fidelity-sanitized
build/upscale-fidelity-sanitized
clang++ -std=c++17 -O2 -arch x86_64 -I native/include \
  native/src/texture_upscale.cpp native/tests/texture_upscale_tests.cpp \
  -o build/upscale-fidelity-x64
build/upscale-fidelity-x64
cmp build/upscale-fidelity-sanitized.log build/upscale-fidelity-x64.log
mkdir -p build/exports/upscale-fidelity-final
../../open-champions-godot/work/runtime/Godot.app/Contents/MacOS/Godot \
  --headless --path godot --export-debug 'macOS universal' \
  "$PWD/build/exports/upscale-fidelity-final/OpenChampions.app" --quit
'build/exports/upscale-fidelity-final/OpenChampions.app/Contents/MacOS/Open Champions' \
  --headless --quit-after 120 --script res://tests/texture_upscale_smoke.gd
'build/exports/upscale-fidelity-final/OpenChampions.app/Contents/MacOS/Open Champions' \
  --quit-after 120 --script res://tests/texture_upscale_smoke.gd -- --visual
```

All **101 native CTest cases passed** with `-j 3` in **863.32 seconds**, with no
timeouts or failures. Build, targeted CTest, source/exported-app texture smoke, 283-check archive smoke,
all eight cache-helper tests (including a synthetic CLI with the real native
publication gate), both count-only private scans, synthetic CLI coverage and
ASan/UBSan/x64 exact-pixel tests passed. Synthetic CLI coverage accepts 3/4
supplied TEX fixtures, with the unsupported fixture retained in its denominator.
Explicit Norrath invocation and unavailable-directory checks refuse with exit
codes 2 and 1 respectively, without disclosing the supplied path in errors.
The sanitizer/x64 ranking logs match byte for byte.

The final application was run graphically with synthetic Before/After pixels,
4× bicubic, alpha controls and the wrapped fidelity readout visible in its actual
1,120×780 viewport. Its screenshot remains in the user's data folder. The export
uses the arm64 debug extension: this does not establish an Intel/release/Windows
package. No installed AI inference model or private PS2 rendering was evaluated.

An initial ZIP export failed because its destination folder did not exist; after
creating it the export passed, including headless and graphical smoke. A later
ZIP refresh stalled inside macOS `NSFileManager`/FileProvider trash handling
(process sampling verified the wait); only that owned export was terminated.
Exporting the final build directly to a fresh `.app` destination passed instead.
No authentication, FileProvider settings or other agent processes were changed.

Owner (a)–(c) are implemented with measured preservation, ranked defaults and
identical AI load/publication gates. Owner (d)'s generic decoder coverage and
count-only broader-corpus measurement are implemented. Its item-only denominator
and inventory-cache pixel decoder remain blocked on the evidence listed above.
