# Bounded RTA terrain-file loading

The portable `terrain_file::decode` accepts explicitly selected, unrelocated
Return to Arms `.world` bytes and returns owned post-fixup `LevelData`. It follows
the immutable `world_info.World` and `ref_world.Level(bytes)` Python contracts.
The existing terrain/query kernel and Session consume that data unchanged.
This projection does not load render geometry, textures, objects, scripts or saves.

Heightfields share identity by file offset and retain offset-sorted registry order.
The loader preserves ordered/repeated terrain-cell references, signed sample and
transform words, uninterpreted path cells, low-16 element flags and reset stamps.
Moving pieces rewrite flagged shared samples in file order and set their stored
maximum to 2000, including fields without flagged samples. Equal contents at
separate offsets remain distinct. Caller bytes are never modified.

The parser bounds all offset spans and count products before allocation. Default
hard caps are 64 MiB input, 65,535 chunks, 10,000 pieces, 8,388,608 aggregate list
reads (including terminators), and 16,777,216 piece sample visits. Existing Level
storage/query limits also apply. Callers may lower these caps. Raw exponent tests
validate metadata finiteness without host floating point. Returned topology is
validated by the existing Level constructor. Missing offsets for nonempty grids,
negative piece counts and native budget refusals are explicit safety policy;
they are not claims about every malformed Python exception or original executable.

## Measured evidence

Local Apple Silicon Release and ASAN/UBSAN runs pass 312 API checks and 133 exact
Python-derived cases: 13 successful parses, 96 parse-and-query cases over all 16
arithmetic configurations, and 24 original parser refusals. All six linked
simulation source units and both new entrypoints were instrumented; macOS leak
detection was disabled. Three independent complete generations produced identical
gzip and manifest bytes. Fixtures publish through flushed temporary files and
atomic replacement. Actual trace SHA-256:
`c26d3998c77dc70e56a4755cf82124b4ee8955a965a3edbb106e8a6c7efbc045`.

The three new CTest gates, simulation boundary, artifact-contract checks and
private-directory CLI check passed. The scoped 288 Python tests passed with 155
explicit native-path/platform skips; dedicated CTest runs the real transport
path. GCC 15 syntax and LLVM 20 formatting cover the changed C++ files.
Five-platform execution remains a separate pull-request CI gate.

The optional private-directory validator now recognizes loose and nested WORLD
entries and reports decoded/refused terrain counts separately. An owner-selected
RTA full-disc collection retained its prior results: 4,087/4,087 unique animations,
1,455/1,456 models and 2,065/2,188 textures. That selected collection contained no
recognized WORLD entry, so **no retail terrain-file validation is established**.
The public CLI test loads authored bytes and never needs original content.

Original world-render fixups, tree geometry, texture loading, live piece/level
construction, original controls and Norrath remain separate work. Parsing
acceptance does not establish a playable game or revision detection.
