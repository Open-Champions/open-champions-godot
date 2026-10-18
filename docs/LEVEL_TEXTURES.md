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
Environment-mapped chunks still show their decoded base texture: their
context-2 first pass needs `envtex.etex` from `env.lmp`, VU1 camera-normal
coordinates and optional level-flag scroll. Per-vertex point lighting and
console depth ≥ ordering also remain outside this adapter, so a full frame is
not claimed to match the GS. The 0x200 layer is exposed in metadata; Godot's
transparent sorting has not been validated against the console. The automap
tile is a separate PSMT4 lump, but the tile-code mapping is undocumented and
belongs with the level-flow automap work; this job does not guess that mapping.

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
Huffman prefixes, relocation arithmetic, malformed headers, and WORLD/UV
binding. On local macOS arm64, the Release and ASAN/UBSAN tests each pass
1,062 checks. Pinned Python suites pass 21 and 8 tests. The Godot 4.5.2 editor
imports both GDExtensions; the isolated headless preview passes 8 checks, and
the graphical preview passes 10. The inherited Session scene passes 10 headless
and 12 graphical checks, including a ready step, failure preserving the old
ground, and a captured viewport image inspected for actual textured geometry.
The original neutral Session smoke still passes 124 checks.
This checks the source scenes and synthetic
assets, not a private level, exported package, other CPU or operating system.

The required full Release command, `ctest --test-dir build/native
--output-on-failure -j 3`, finished 148/149 in 1,188.19 seconds. The unrelated
`player-exact` case alone hit its 240-second timeout under host contention;
its required isolated rerun, `ctest --test-dir build/native --output-on-failure
-R '^player-exact$' -j 1`, passed in 177.90 seconds. All 149 named tests thus
have passing evidence, although the single full-suite invocation was 148/149.
