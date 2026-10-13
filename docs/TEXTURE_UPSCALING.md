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
interpolated alpha. Rounded zero alpha returns zero RGB. Transparent hidden colors
cannot create dark or colored fringes. Filtering is in decoded byte color space,
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
an input PNG and JSON manifest under `user://texture-upscale-cache/v1/`, Godot's
user data folder (the resolved absolute path is shown after export). The exported input is opaque RGB for this inspection recipe; its hash still includes the complete original decoded alpha. External replacement alpha is whatever the CLI produces (RGB PNG becomes opaque), not a recovered game blend rule. Game art
must stay there, outside any checkout. Nothing is downloaded or uploaded.

The filename key is SHA-256 of UTF-8:

```
oc-upscale-v1\n<width>\n<height>\nexternal-opaque-rgb\n<factor>\nrealesrgan-x4plus\n
```

followed by the complete decoded row-order RGBA8 bytes. It includes dimensions,
opaque RGB inspection mode, factor, recipe/model name and format version. `<key>.png` is the replacement;
`<key>.input.png` and `<key>.json` are the optional job input. A missing PNG shows
an explicit cache miss; there is no automatic fallback. The PNG header, dimensions
and 64 MiB file cap are checked before decoding; it must be exactly factor × the
source size, at most 4096 per axis. Malformed/wrong-sized replacements clear After.
RGBA is retained by Godot. External output and quality are not deterministic promises.

With an already installed [Real-ESRGAN ncnn Vulkan](https://github.com/xinntao/Real-ESRGAN-ncnn-vulkan#full-usages) compatible binary and the
`realesrgan-x4plus` model available to that tool:

```sh
python3 tools/fill_texture_upscale_cache.py /absolute/user/cache/<key>.json \
  --executable /absolute/path/to/realesrgan-ncnn-vulkan
```

The helper uses a subprocess argument array (no shell), validates exported-input
SHA-256 and bounded settings, rejects cache paths inside repositories and image
symlinks, and invokes `-i INPUT -o OUTPUT -n realesrgan-x4plus -s FACTOR -f png`.
The CLI reads a bounded, hash-verified temporary PNG snapshot, so the exported input cannot change between verification and inference. It runs from the installed executable folder, so the tool’s default `models/` folder is resolved there. It checks output dimensions and publishes by atomic rename within the user cache.
It has a ten-minute timeout and downloads no binaries/models. If your CLI does
not support the requested scale or PNG, it is not a compatible recipe;
use the native filter or an independently prepared replacement PNG. Changing
model/recipe requires a new version/key contract, not overwriting another recipe's
identity. Re-select/refresh preview after the helper finishes. No CLI is invoked
in public tests; synthetic fake-CLI tests verify the handoff boundaries.

## Validation and remaining work

Synthetic native tests cover exact nearest blocks, Scale2x/3x corners, composed
4×, literal bicubic alpha ramps at all factors, reversed ramps, transparent hidden
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
playability, inventory support or Norrath compatibility. No private-game validation
or installed AI model quality is claimed. Target-platform visual evidence and exact
commands/results are recorded with the local completion report.

## Local verification on 2026-10-13

Apple Silicon, Godot 4.5.2 with pinned godot-cpp; native C++ Release. All **101 native CTest cases passed** with `-j 3` (1577.96 seconds), including the new exact-pixel upscaler case. The existing
browser smoke passed **283 checks**, and the new texture-upscale smoke passed in
both the source project and an actual Mac debug export. The exported application
also ran graphically with the synthetic fixture: Before/After, 4× bicubic, factor
and filter controls, alpha toggle, and scrolling were inspected in its rendered
viewport. Opaque RGB resampling retains alpha-zero colors; ordinary premultiplied
resampling of the same synthetic input returns transparent black as specified.
The screenshot stays in the user's data folder and contains authored pixels only.
This local export has an arm64 extension; it is not verified as an Intel or release
client package. The optional external inference binary/model was not installed or
run. Six mocked CLI/cache boundary tests passed.

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
