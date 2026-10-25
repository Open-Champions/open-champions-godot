# Return to Arms level textures

`champions-level-texture` reads the documented `<LEVEL>.TEX` directory, owns a selected
cell, parses its 0x40-byte texture headers and shared palette groups, and decodes
the 0x010c block-table form. Its C++17 decoder uses the canonical 1–16-bit Huffman
tables, all five back-references, eight successor picks, the 256-byte upload
permutation, PSMT8 index placement, and the GS CLUT bit-3/bit-4 exchange. It
preserves zero for uncovered texels and later-rectangle overwrite order. Header
+4 allocation blocks and the relocated +6 qword count are checked/computed.
`relocate_offset` models the documented 128 KB segment-cache map; the PC loader
uses cell-relative offsets directly. Flag 0x0001 remains refused because the
additional indirection and preparation routine have no disc input or complete
contract. No Norrath compatibility is claimed.

`bind_world` connects decoded images to the existing RTA WORLD geometry metadata:
chunk cell +0x34, texture header offset +0x20, source triangles, strip UV slots,
and UV coordinates stored as texels ×16. It returns triangle positions, normalized
UVs, and shared owned RGBA pixels for each surface. The adapter also reads the
chunk's GS state, follows each packet's DIRECT ALPHA_1 and TEST_1 writes in
source order, and uses the batch GIF tag ABE bit. The sampler is bilinear with
clamp-to-edge addressing and no mip maps. This portable adapter has no Godot
dependency. An isolated `level_texture_preview` GDExtension and Godot scene turn
those surfaces into `ArrayMesh` instances with owned image textures. The scene
accepts only caller-selected WORLD/TEX paths (`load_level` or `--world`/`--tex`),
uses shaders for opaque, reverse-alpha blending, additive blending and the
0x2f alpha cutout, and publishes replacements atomically. The original palette
alpha byte is sampled before the cutout test and converted from GS 0..128 to
Godot 0..1 before blending.
`level_textured_session.tscn` inherits the existing controllable Session scene.
Its new script prepares the same textured surfaces and native WORLD collision
candidate, commits the controller, then replaces the neutral mesh in one
synchronous publication. Native Z-up vertices use the Session display mapping
`(x, z, -y)`; positions and UVs otherwise retain their documented values.

To run the isolated preview, build `gdextension/level_texture` in Release with
the repository's pinned `godot-cpp` checkout, then open
`res://scenes/level_texture_preview.tscn` in Godot 4.5.2. Supply the matching
WORLD and TEX paths through `load_level(world_path, tex_path)` or launch the
scene with `-- --world <path> --tex <path>`. The inherited Session scene accepts
the same flags or `load_textured_terrain(world_path, tex_path)`. The app never
bundles those inputs.
`gdextension/src/session_controller.cpp` and `godot/scenes/session_sandbox.gd`
are owned by the running `session-play-runtime` job, so the default scene's
single-file picker remains neutral. The inherited scene is the textured route;
it needs an explicitly selected matching WORLD/TEX pair. The research contract
supplies UV and texture addressing; no UV formula was guessed.
Environment-mapped chunks (flag 0x10) use the documented camera rows, quarter
turn, scroll and environment UV formula. The first pass is context-2 ALPHA
0x2a and the second pass is ALPHA 9. The environment image is caller-supplied;
this adapter does not infer a lump name. Until that image is set, the surface
draws its base pass opaque: the ALPHA 9 second pass is only documented over the
context-2 first pass. Per-vertex environment UV was
not re-difftested against VU program 0x48. Scroll stays 0 unless the caller
sets level-option bit 2, because that word is runtime state and not a WORLD
field. Console send order is layers 0 through 7, with same-texture groups in
submission order. Flag 0x200 is layer 4 and still depth-tests. Automap cells
become 10-texel quads (UV origin n×160, far edge +158), layer 5, point
sampled, ALPHA 0x44 and no depth test. The atlas pixel format and the HUD
window are not decoded, so those quads are not placed on the play camera.

`model_rows` and `model_0x46_position` expose the two documented VIF row sources
as an explicit caller choice: the dormant 0x175950 zero-row setup or caller-supplied
rows inherited from a preceding level chunk. Inherited mode refuses without those
rows. Signed 32-bit wrapping occurs before division by 16. The existing full
`.vif` model decoder still refuses 0x46 packets: they omit weights and require a
separate packet parser, so this row helper alone does not claim model rendering.

## Pinned evidence

Research checkouts `t3/research-textures` and `t3/research-levels` are read-only.
The implementation uses:

| Source | Commit | SHA-256 |
|---|---|---|
| `docs/formats/level-texture.md` | `809d982b4098bb3de731d6141b78c04a88e315ac` | `b1685d121f9aca04f21603138eb7ccfe24f26f6f0cdf938f8f14fa5ce58faf2a` |
| `tools/ref_leveltex.py` | `ea47b26aa063dff88f0945f37637e85e6a7b449e` | `12fd442c230ebf4b2781f45265d6d3e83b5b2c757425c5d64b2fde093bc9e898` |
| `tools/test_ref_leveltex.py` | `ea47b26aa063dff88f0945f37637e85e6a7b449e` | `7892c83a9b722d334c00ec6ae491326914cc0b89348335e0ea2b431476754bba` |
| `docs/formats/model-row-registers.md` | `ea47b26aa063dff88f0945f37637e85e6a7b449e` | `86b925725b922479d829e5e3e33f3d7efadaa6b3c216e15f761d5ab98e9f46ab` |
| `tools/ref_model_rows.py` | `ea47b26aa063dff88f0945f37637e85e6a7b449e` | `6f2d9f3b28e52d9a82b7ea580da20e819ecb5fd78a0c241e397429f800e352a6` |
| `tools/test_ref_model_rows.py` | `ea47b26aa063dff88f0945f37637e85e6a7b449e` | `5444a4fb3221e744a78de9c03855539e09b2101edce05a4291bc48e99dd64d27` |
| `docs/level-flow.md` §13 | `18e1a34d92f7b7ad1c305a949785ff5d0d57e702` | `0451ec955dc714fa70f331e77cbf8133ff7bbdd52ab01fb10cbef9edfdc8d0f1` |
| `tools/ref_levels.py` | `18e1a34d92f7b7ad1c305a949785ff5d0d57e702` | `00b3f3ee3a83421494beb68880791060b735e5d7b3c525186224194f75b47dda` |
| `tools/test_ref_levels.py` | `18e1a34d92f7b7ad1c305a949785ff5d0d57e702` | `a3b1a2611f418fad94d5594eabeb5f2b755cee04f749b1fd52b424eb32e539ab` |
| `tools/model_info.py` (GIF tag and A+D layout) | `18e1a34d92f7b7ad1c305a949785ff5d0d57e702` | `9cb38fadec899fde8ed457d7daf050bb7f6ebab80795494e27d697470c088870` |
| `docs/formats/level-draw.md` | `391cf6b77780d51f978aca8eca1b2010749cf5b7` | `c0a78f862fde79f5c33f957a037369c44b21ec714ad49c72eba14ada5953d0ea` |
| `tools/ref_leveldraw.py` | `391cf6b77780d51f978aca8eca1b2010749cf5b7` | `936cd0ee0cce9d1d9006459984e76ba6c56ff014ebdbc8d1b8c08c33eaf63bca` |
| `tools/test_ref_leveldraw.py` | `391cf6b77780d51f978aca8eca1b2010749cf5b7` | `fc48e00b86ea240fd43a02115bd44b90a983b5aaea291a68be828c5416115031` |
| `docs/spec/render-pipeline.md` | `391cf6b77780d51f978aca8eca1b2010749cf5b7` | `3eb890d80325c7b04a584de04af8360fc8f4c0e412d3a21656b4e99db570a04b` |
| `docs/formats/water-draw.md` | `a9486c7f0052e533d3cc5c88f3d98d8f07bffdce` | `a6d827ef663f5783512b8f2a1a72f874b543f96ffead241346dc73ad59f619aa` |
| `tools/ref_waterdraw.py` | `a9486c7f0052e533d3cc5c88f3d98d8f07bffdce` | `7c2a005a39784d65b2e65ff8f321e1bd0a580820ccf202c2f77f0f6d27cb77b7` |
| `tools/test_ref_waterdraw.py` | `a9486c7f0052e533d3cc5c88f3d98d8f07bffdce` | `d8a28cf81b05234a26eeb4041227e300cf7488972da7f285ac812cb5e082dea9` |

The research reports 111 level files, 4,808 cells, 157,820 textures, all flags
0x10c, and 10,558,802 decoded block-stream uses on the US v2.00 disc. Those
are research counts, not a fresh native private-file run. The historic **109
refusals** in `docs/TEXTURE_PREVIEW.md` were loose level files presented to a
lump decoder, not 109 textures. Private validation is unavailable under this
job's rules; it was neither searched for nor run. Tests use only authored
synthetic fixtures derived from the pinned reference vectors.

## Local verification

The generated synthetic cell contains two textures sharing one palette group,
L-shaped and partial block coverage, multiple page widths, and short/long
Huffman codes. An authored mutation checks two independent groups in one cell.
Separate authored bitstreams exercise every back-reference and
successor symbol. Complete indices, coverage and RGBA outputs match the pinned
Python reference by independent 64-bit FNV-1a values. Native tests also cover
every permutation byte against PSMCT32/PSMT8 address formulas, all 256 short
Huffman prefixes, relocation arithmetic, malformed headers, WORLD/UV binding,
header state priorities, DIRECT writes and the batch ABE bit. On local macOS
arm64, the updated Release test passes 1,067 checks; the earlier decoder-only
ASAN/UBSAN test passed 1,062 checks. Pinned Python suites pass 21, 8 and 50
tests. The Godot 4.5.2 isolated preview passes 8 headless and 10 graphical
checks; the inherited Session scene passes 10 headless and 12 graphical
checks, including a ready step, failure preserving the old ground, and a
captured viewport image inspected for actual textured geometry. A separate
graphical material test confirms a 64/128 reverse-alpha blend and an 80/128
cutout by inspecting rendered pixels.
The original neutral Session smoke still passes 124 checks.
This checks the source scenes and synthetic
assets, not a private level, exported package, other CPU or operating system.

The updated full Release command, `ctest --test-dir build/native
--output-on-failure -j 3`, passed all 149 tests in 691.90 seconds.

## Playable integration (wave 8)

The released decoder and portable binding above now also apply the level-draw
contract in the play shell. Dynamically lit surfaces still use the merged
native-FX `shade_world` model and the existing synthetic fixture inputs.
Constant chunks bake `world_constant_channel` on that same base (128 in the
play fixture): negative sums wrap as `(2 × sum) mod 256`, and non-negative
sums clamp at 255. Original WORLD light entities remain unloaded. See
[PLAYABLE_LEVEL_TEXTURES.md](PLAYABLE_LEVEL_TEXTURES.md) for selection, material,
constant chunk colour, save-resource ownership and the remaining gaps.
One private level was measured with the existing decoders. Its counts and
captures stay in gitignored `work/private/` and are not copied here.
The earlier isolated-scene ownership note describes the release baseline, not
the current integrated shell.

## Streamed display and frame cost (perf, 2026-10-21)

The game-folder scene (`real_data_play.tscn`) no longer decodes every texture
cell of a level. `ChampionsLevelTextureStream`
(`gdextension/level_texture/level_texture_stream.cpp`) keeps the WORLD
geometry and the still-compressed TEX file, and
`scenes/level_texture_stream.gd` keeps the 3 × 3 cells around the camera focus
resident, as the original does (research `docs/level-flow.md` 13.1,
`docs/formats/world.md`):
- cell id `row × 100 + column` with `floorf(x / size) + 50`, `size` from WORLD
  +0x68 (0 means 960);
- focus = camera position + forward × 600 (`camera_updateFocus` 0x18a520);
  before play, the session start;
- nine slots; the missing window cell nearest the focus is read first, one per
  frame; a chunk whose cell is not resident is not drawn.

Client choices, not original evidence: "nearest" is the squared row/column
distance to the focus cell, and a full set of slots reuses the resident cell
farthest from the focus. The original slot order and reuse rule are not
documented.

Batching per cell: batches whose result does not depend on frame order (no
blend, or the darken and cutout modes; no environment pass; not a moving piece,
chunk flag 0x20) share atlas pages. Each image keeps a one-texel clamped
border, so bilinear filtering at a UV edge reads what clamp-to-edge reads on
the image. Atlasing is used only when every UV of the batch lies in [0, 1].
Blended, environment and moving-piece batches keep the full loader's
consecutive runs. `draw_slots` gives their draw order over the resident cells
from geometry alone. On the largest disc level it equals `bind_world`'s order
for the whole level, and `bind_cells`' order for each of its 148 windows.
Fixtures and other callers keep the full bind (`stream_cells` is opt-in).

FX lighting: `ChampionsFxBridge.shade_world_many` returns `shade_world`'s
colour for each vertex in one call. The renderer reuses its unshaded material
copies instead of building (and compiling) new ones on every relight. The
game-folder stream now prepares and commits the collision session once, with
the same validated surface and triangle summary but no neutral mesh. Its
textured root is the only terrain display published. The first nine-cell
window is complete before play.

While walking, one missing cell at a time is decoded on a worker thread.
The main thread creates and lights its groups over multiple frames, then
publishes the complete cell. The nearest-missing order, nine-slot limit and
whole-cell visibility are unchanged. `end_session()` retains the global VAT;
the game-folder audio bridge installs that read-only bank once per bridge
lifetime, avoiding a second 108 MB copy during resets. Measurements and the
water-mesh check are in `work/loop-perf-r2-report.md`.

Constant-light groups (wave 18): `cell_view(cell, light_base)` colours them
only for a non-negative base. The streamed display passes -1 and tags each
such group `level_constant_light` / `level_constant_base` -1, as
`level_texture_terrain.gd` does for the full display, so
`playable_fx.light_constant` applies the loaded level's base level (0x80, 0x40
or 0x20, TEX-3) and the texel × colour / 128 presentation (TEX-2). Before the
wave-18 merge the stream used the removed fixed base 128.

Counts and timings only, US v2.00 disc copy, 1280×720, GL Compatibility on
Apple M5 Pro: see `work/loop-perf-r1-report.md`. The 111-level headless sweep
keeps every deterministic count of wave 16. Only display counts change: meshes,
drawn claimed pieces (52 → 14, because only resident cells draw) and lighting
calls. Synthetic smokes: `tests/level_texture_stream_smoke.gd` and
`tests/fx_shade_batch_smoke.gd`.
