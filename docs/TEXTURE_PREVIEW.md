# RTA lump texture preview

The archive browser can preview the documented 32-bit, direct and PSMCT32-uploaded 4-bit, and 8-bit lump-texture subsets in a
selected Return to Arms GOB or LMP. Select a `.tex` or `.etex` entry, then use the
preview control. Norrath decoding is explicitly unavailable pending an independently
identified baseline and format contract. Merely selecting RTA does not detect or
certify the archive's edition.

## Supported contract

The portable decoder is independently authored from the published
[model/texture research](https://github.com/Open-Champions/champions-return-to-arms-re/blob/c3d7264/docs/formats/model-texture.md)
for the US v2.00 RTA baseline. It accepts unrelocated 0x80-byte lump headers,
32-bit PSMCT32, direct and PSMCT32-uploaded 4-bit PSMT4, and the documented 8-bit PSMT8 storage forms. The direct 4-bit form first
uploads a 16-entry RGBA palette in index order, then one low-nibble-first image.
The 32-bit and direct 4-bit forms require one bounded image rectangle covering every used pixel.
The general API accepts 32-bit packets with omitted initial `TRXPOS`, using the
documented zero destination origin; the strict `decodeRtaTexture32` API still
requires an explicit position. Explicit positions override that initial state.
The 8-bit form has a 256-entry RGBA palette in GS CLUT order, followed by
32-bit upload rectangles into bounded temporary GS memory. The decoder samples
that memory as PSMT8 indices and returns palette colors; unwritten texels use
index zero. It supports cropped and multiple upload rectangles with the
documented half-width upload pitch. Padding outside the used dimensions may be cropped. It returns
owned RGBA8 pixels in stored row order and cropped to the used dimensions. Header,
packet, register, transfer and allocation bounds are validated before publishing
pixels. Unsupported variants fail explicitly.

Streamed world texture cells, blank render surfaces, unknown
registers and relocated headers are not decoded. The decoder is separate from
Godot and can be tested without any original game files. The browser revalidates
the archive and selected entry before previewing; a content fingerprint rejects a
selection after its file changes. It does not trust a UI-supplied byte offset.

## Display semantics

RGB preview ignores stored alpha so that the underlying colors remain inspectable.
The optional alpha view shows an opaque grayscale channel scaled from 0..128 to 0..255,
saturating above 128. It does not infer opacity or a material's blend equation.
Model and sprite alpha have different uses in the original renderer. Field weaving,
orientation in the game, filtering, wrapping, lighting and material blending are
not reproduced by this initial inspection view.

No game data is included in the source, tests, packages or public screenshots.
Original inputs remain on the owner's device. The preview is an asset-inspection
feature; it is not gameplay, a native game scene or a compatibility claim for Norrath.

## Initial 32-bit verification (revision 4c0b60e)

Local native validation passed nine CTest cases, including 349 texture checks.
The texture test also passed AddressSanitizer and UndefinedBehaviorSanitizer.
Godot editor and actual Mac export smoke checks passed 47 checks.

The final decoder output matched all RGBA bytes of one unique private 128×128
RTA texture against the published research decoder. Four available level copies
contained the same pixels, so they are counted as one sample. This establishes
that sample comparison; it does not verify rendering against a running PS2 game
or every texture on the disc. Private samples and pixel exports remain private.

The exported Mac application was visually checked with an authored asymmetric
fixture: RGB colors, alpha-channel display, selection/filter behavior and explicit
unsupported-format errors. The real private archive opened and listed its entries.
A desktop-inspection service failure prevented the final private-image click;
that graphical check was unverified at that revision and is completed below. A separate headless Godot check read
the original archive through the real binding, matched all 128×128 RGBA bytes
and successfully constructed an ImageTexture. Windows graphical inspection is also
unverified. At source revision `4c0b60e`, the [Godot platform run](https://github.com/Open-Champions/open-champions-godot/actions/runs/38037733451)
passed on macOS arm64/x64, Windows arm64/x64 and Linux x64. Each job built
separate debug/release bindings, passed all 47 editor and exported-application
checks, ran 21 packaging tests and produced a checksum-bearing package. The
[native foundation run](https://github.com/Open-Champions/open-champions-godot/actions/runs/38037733460)
also passed all nine CTest cases on those five hosts. Automated headless execution
does not establish graphical behavior or a clean-machine installation.

## Plain 4-bit extension

The new API `decodeRtaTexture` accepts the documented plain 4-bit form as well as
the existing 32-bit subset. `decodeRtaTexture32` remains restricted to 32-bit input.
The result owns RGBA pixels; the historical `Texture32` type name describes that
output storage, not the input bit depth. Register-state and cropped-transfer
regressions use authored fixtures. All nine CTest cases and 811 texture checks pass locally, including AddressSanitizer
and UndefinedBehaviorSanitizer. The Godot source smoke passes 56 checks.

Twelve private 4-bit samples from twelve RTA archives produced twelve distinct
pixel hashes. Every decoded RGBA byte matched the research decoder; sample widths ranged
from 10 to 640 pixels, including cropped transfers and non-power-of-two widths.
One original 93×10 archive entry also passed through the real Godot binding and
ImageTexture construction headlessly. The private comparison caught two cases
now covered by authored regressions: transfer position persists after the palette,
and buffer stride need cover the used/uploaded width rather than the whole
power-of-two sampling width. The 32-bit API retains its previous acceptance rules.
At revision `86782af`, the exported Universal 2 Mac application passed all 56
automated checks and was inspected graphically on Apple Silicon. It rendered a
private 93×10 paletted texture in RGB and alpha-channel views, then a private
128×128 32-bit texture selected through a filtered nested archive listing.
Changing archives cleared the old preview. A separate authored 4×4 palette
fixture rendered its asymmetric colors and supplies the public screenshot. No
original game pixels are included in public artifacts. This closes the earlier
private-image screen-check blocker; Windows graphical checks remain unverified.

The [native foundation run](https://github.com/Open-Champions/open-champions-godot/actions/runs/38038789852)
passed all nine CTest cases on five hosts. The
[Godot run](https://github.com/Open-Champions/open-champions-godot/actions/runs/38038789861)
also passed on all five hosts: Apple Silicon/Intel macOS, Windows ARM64/x64 and
Linux x64. Each job passed 56 editor checks, 56 actual exported-application checks
and 21 packaging tests. Delivered Mac Universal 2 and Windows ARM64/x64 packages
had their source revision, ZIP checksum, per-file inventory and smoke-log hash
verified; both Windows extension binaries import only `KERNEL32.dll`.
Graphical verification applies only to the Apple Silicon Mac run above.

## 8-bit extension

The generalized decoder now replays bounded PSMCT32 image transfers and reads
PSMT8 indices using the published page/block/column layout. Palette indices
exchange bits 3 and 4 when locating the stored RGBA entry. Image memory starts
at index zero, and later uploads replace earlier bytes where rectangles overlap.
This models asset upload addressing only; no game simulation or GPU emulation
is introduced. The temporary address space is capped at 4 MiB. Stored row order
and the existing alpha inspection semantics are preserved.

Local validation passed all nine CTest cases and 2,486 texture checks, including
AddressSanitizer/UndefinedBehaviorSanitizer and an independent review. Tests
cover literal address vectors, palette permutation, nonuniform multi-page data,
partial/overlapping rectangles, inherited transfer state and malformed inputs.
The Godot source smoke passes 72 checks.

Sixteen distinct private textures from sixteen RTA archives matched every
reference RGBA byte, including inputs with up to 52 image rectangles. The
private disc executable was freshly checked against the pinned RTA baseline.
A 383×189 original archive entry also passed the real Godot binding and
ImageTexture construction headlessly. These comparisons validate the decoder
on those inputs; they do not establish full game rendering or disc-wide coverage.

The exported Universal 2 Mac application at `c5d3a3e` passed 72 automated
checks and was inspected graphically on Apple Silicon. It visibly rendered the
private 383×189 creature texture in RGB and alpha-channel views. An authored
16×16 patterned fixture was also displayed and used for the public screenshot;
no original pixels are included in published artifacts. Windows graphical and
clean-machine checks remain unverified.

At source revision `c5d3a3e`, the
[Godot platform run](https://github.com/Open-Champions/open-champions-godot/actions/runs/38040560587)
passed on Apple Silicon/Intel macOS, Windows ARM64/x64 and Linux x64. Each job
passed 72 editor checks, 72 exported-application checks and 21 packaging tests.
The [native foundation run](https://github.com/Open-Champions/open-champions-godot/actions/runs/38040560509)
passed all nine CTest cases on the same five hosts. Delivered Mac Universal 2
and Windows ARM64/x64 packages were checked against that source revision, ZIP
checksums, per-file inventories and smoke-log hashes. Both Windows extension
binaries import only `KERNEL32.dll`. These automated checks do not extend the
Apple Silicon graphical evidence to other platforms or establish playable games.

## 4-bit PSMCT32-upload contract (recorded before implementation)

Evidence: the research sibling named in the reconstruction repository's README,
[format notes](https://github.com/Open-Champions/champions-return-to-arms-re/blob/d0a4733040deaf7555f06142a1339426d7aed4a2/docs/formats/model-texture.md#tex-pixel-data)
and [reference decoder](https://github.com/Open-Champions/champions-return-to-arms-re/blob/d0a4733040deaf7555f06142a1339426d7aed4a2/tools/model_info.py)
(`parse_texture`, `walk_upload`, `gs_addr4`, `decode_texture`). The supplied
`../../champions-return-to-arms` checkout owns reconstruction; it links to this
separate research project. Only documentation and the authored reference tool
were consulted, with local reference copies under ignored `work/`.

The next layout retains the existing unrelocated 0x80-byte lump header:
little-endian u16 used width/height at +0/+2, allocation blocks at +4, packet
qwords at +6, flags at +8 (storage kind 0x2000), zero relocation marker at +0xa,
u32 TEX0 fragment at +0xc, and packet offset 0x80 at +0x10. Runtime bytes
+0x14..+0x7f are zero. TEX0 has TBW in bits 0..5, PSMT4 (0x14) in bits 6..11,
log2 surface width/height in bits 12..15/16..19, and existing TCC/TFX fields;
bits above 22 are refused.

One palette destination precedes one image destination. The palette is exactly
8x2 PSMCT32 RGBA (64 bytes) at the origin, in index order: no CLUT bit exchange.
The image destination selects PSMCT32 rather than the existing direct PSMT4
upload. Its DBW must be half TEX0 TBW, so TBW must be nonzero and even. Host-to-local
rectangles contain four row-major bytes per uploaded word, rounded to GIF qwords.
Base pointers, source coordinates and reserved fields remain zero. Later
rectangles reuse the image destination and may inherit position/extent, but must
restart TRXDIR; overlapping writes apply in order. Mixed direct/PSMCT32 uploads
and further destination buffers are refused.

Replay PSMCT32 into zero-initialized GS memory using the existing 64x32 page,
8x8 block and word mapping. Sample PSMT4 with 128x128 pages and 32x16 blocks.
The 4x8 block table is:

```text
0 2 8 10
1 3 9 11
4 6 12 14
5 7 13 15
16 18 24 26
17 19 25 27
20 22 28 30
21 23 29 31
```

For local block row `yy = y % 16`, let `r = yy % 4`,
`W = [0,1,4,5,8,9,12,13]`, and `s = 4` when `((yy+2)/4)%2` is one,
otherwise zero. The word is `(yy/4)*16 + (r%2)*2 + W[(x%8)^s]`;
its nibble is `r/2 + 2*((x/8)%4)`. With page pitch `DBW`, page
`(y/128)*DBW + x/128` and block from the table, the nibble address is
`2*(page*8192 + block*256 + word*4) + nibble`. Read byte `address/2`
and select low/high nibble by `address%2`. Unwritten texels select palette zero.
Output is cropped to used dimensions in stored row order; no field weaving.
Alpha remains `min(255, raw*255/128)`, an inspection conversion rather than a
material blend rule.

Existing defensive limits apply: used and surface dimensions 1..1024,
allocation blocks 1..16384, packet/input bounded by the u16 qword field,
zero trailing entry padding, upload rectangles within `DBW*64` by 1024,
exact payload qword counts, and every upload/sample within 4 MiB GS memory.
The block count is header metadata, not a new allocation-size inference.
Unknown flags/registers, relocation, streamed world cells, blank surfaces and
Norrath remain refused. This contract adds upload addressing, not GPU emulation
or original interface rendering.

### Local native verification (2026-10-13, macOS arm64)

The authored tests pass 3,205 checks. Literal coordinates exercise all eight
nibble lanes, both block axes and both page axes. A nonuniform 256x256 image
from four uploaded pages matches the independent reference FNV-1a checksum
`c1e7df4e2851ea18`; the bytes and palette are authored synthetic data.
Sparse/overlapping rectangles, persistent transfer state, used-dimension cropping,
truncated prefixes, pitch/format mismatches, runtime headers and GS overflow
refusals are also covered. Existing direct 4-bit, 8-bit and strict 32-bit tests
remain in the same executable.

Commands and results:

```sh
cmake -S native -B build/native
cmake --build build/native -j 4
build/native/champions-texture-tests
ctest --test-dir build/native --output-on-failure -R '^(texture-parser|create-texture-fixtures|archive-parser|create-fixtures|cli-.*|private-validation-report)$'
c++ -std=c++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -I native/include native/src/texture.cpp native/tests/texture_tests.cpp -o build/native/texture-tests-sanitized
build/native/texture-tests-sanitized
git diff --check
```

All commands above passed; scoped CTest passed 10/10. The sanitizer executable
passed all 3,205 checks without diagnostics. An initial direct test execution
used an object compiled during a test edit and failed a mutation check; rebuilding
the object resolved that stale-executable failure.

The broader command `ctest --test-dir build/native --output-on-failure -j 4`
encountered an unchanged `objects-runtime-transport` failure: its subprocess
exceeded the existing 60-second timeout. The broader run was interrupted while
unchanged session trace cases were still running (exit 130), so the complete
100-case suite is **not** certified passing. No object, session, creature or
animation source was changed for this extension.

Private-file acceptance was not measured. The existing GDExtension preview calls
`decodeRtaTexture` and retains its explicit RTA-only guard, so no binding/API
change is needed. New graphical rendering, reference comparisons on private
inputs and native platforms beyond this local arm64 run remain unverified; the
real-texture/reference/native-platform progress milestone remains open.

## Private acceptance and implicit-origin PSMCT32 (2026-10-13)

A read-only, texture-only coverage probe under ignored `work/texture-research/`
recursively inspected `../../private-validation`, including loose TEX/ETEX and
recognized GOB/LMP children. It skipped symbolic links, capped input files at
512 MiB and counted payload occurrences, without extracting assets or recording
names, paths or pixels. The selected collection includes repeated inputs and
negative fixtures; these totals are neither byte-deduplicated nor full-disc totals.
Both runs inspected 2,253 recognized files with zero archive traversal refusals.
Count-only reports are retained privately as `acceptance-before.tsv` and
`acceptance-after.tsv`. The baseline probe compiled the texture source from
`7787d84`; the second probe compiled the updated source.

| Header/upload layout | Baseline decoded | Updated decoded | Total | Updated refused |
|---|---:|---:|---:|---:|
| PSMCT32 | 34 | 38 | 40 | 2 |
| Direct PSMT4 | 576 | 576 | 578 | 2 |
| PSMCT32-uploaded PSMT4 | 2 | 2 | 2 | 0 |
| PSMCT32-uploaded PSMT8 | 1,601 | 1,601 | 1,604 | 3 |
| Blank render surface | 0 | 0 | 2 | 2 |
| Non-lump header | 0 | 0 | 110 | 110 |
| **All occurrences** | **2,213** | **2,217** | **2,336** | **119** |

The baseline additionally refused four PSMCT32 packets because `TRXDIR` lacked
an explicit position setup. All four supplied a destination buffer and extent,
but omitted `TRXPOS`. The pinned research reference `walk_upload` explicitly
initializes `dsax` and `dsay` to zero; `decode_texture` reads PSMCT32 rectangles
at those coordinates. The new general-API behavior implements that exact initial
origin, retaining one complete covering rectangle, all existing register/packet
bounds and the historical strict API. It does not supply missing extents,
directions or buffers, or accept repeated explicit positions.

Remaining refusal reasons (first failing decoder check, counts only):

| Layout | Reason | Count |
|---|---|---:|
| PSMCT32 | Used dimensions or buffer width do not fit the texture surface | 2 |
| Direct PSMT4 | IMAGE destination exceeds the bounded texture surface | 2 |
| PSMT8 | Dimensions outside 1..1024 | 3 |
| Blank surface | Storage kind has no supported stored pixel image | 2 |
| Non-lump header | Input exceeds bounded lump packet size | 109 |
| Non-lump header | Dimensions outside 1..1024 | 1 |

The non-lump group is classified only by failure of the lump header's zero
relocation marker/0x80 packet-offset contract; it is not identified as 110
relocated or streamed textures. Its largest refusal count does not justify
guessing a decoder. The consulted format notes identify streamed flags 0x0001
and 0x0100 only from code and say they never occur in the documented lumps.
An exact streamed implementation still needs the cell/header layout, packet
range and external-data addressing contract. A relocated input needs the
original load base, valid pointer-to-input mapping and patched GS destination
address contract. The current API supplies none of those. Blank surfaces contain
no stored pixels; inventing their contents would not decode an image. Other
pixel formats still need documented upload, sampling and palette contracts.
No unsupported layout was guessed or normalized from private data.

Evidence is the same pinned research revision
`d0a4733040deaf7555f06142a1339426d7aed4a2` linked above: the format notes and
authored `tools/model_info.py` (`walk_upload`, `decode_texture`). A fresh retrieval
of that reference matched the previously consulted local copy exactly.
Running the pinned Python decoder on the authored 4x4 fixture with and without
its initial `TRXPOS` produced identical results for all 16 pixels.
Authored tests cover every pixel of an asymmetric implicit-origin image,
alpha/cropping, strict/general API separation, explicit position override,
duplicate positions, missing setup fields, wrong direction and every truncated
prefix. The native test and ASAN/UBSAN builds both pass **3,487 checks**.

Commands (local macOS arm64):

```sh
git show 7787d84:native/src/texture.cpp > work/texture-research/texture-before.cpp
c++ -std=c++17 -O2 -I native/include native/src/archive.cpp work/texture-research/texture-before.cpp work/texture-research/coverage.cpp -o work/texture-research/coverage-before
work/texture-research/coverage-before ../../private-validation > work/texture-research/acceptance-before.tsv
c++ -std=c++17 -O2 -I native/include native/src/archive.cpp native/src/texture.cpp work/texture-research/coverage.cpp -o work/texture-research/coverage
work/texture-research/coverage ../../private-validation > work/texture-research/acceptance-after.tsv
python3 work/texture-research/check-implicit-origin.py
cmake -S native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native -j 4
build/native/champions-texture-tests
ctest --test-dir build/native --output-on-failure -R '^(texture-parser|create-texture-fixtures|private-validation-report)$'
c++ -std=c++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -I native/include native/src/texture.cpp native/tests/texture_tests.cpp -o build/native/texture-tests-sanitized
build/native/texture-tests-sanitized
ctest --test-dir build/native --output-on-failure -j 3
git diff --check
```

All listed commands passed. The affected CTest selection passes 3/3; the full
Release suite passes **100/100** in **375.31 seconds**, with no timeout reruns.
This completes the local full-suite check left uncertified in the preceding
extension's historical verification section. Private acceptance measures successful decoding only;
private pixel parity, actual application rendering, other native platforms,
other RTA revisions and Norrath compatibility remain unverified for this change.
