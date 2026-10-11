# Supplied terrain collision and isolated movement

This RTA module ports the published finite terrain-query contract from
`tools/ref_world.py` at `6831458ee7ee12bb166d1dce4eb8e4340095bc48`.
The specification is `docs/spec/world-queries.md` at
`b6881f52457bd715058248c34d65888c797ba657`. The independent vector generator
hash-pins the world model, its `world_info.py` import and the existing core
arithmetic contract. No retail bytes or live research working-tree code are
expected-output inputs.

## Supported data and operations

`Level` owns caller-supplied **post-fixup** data. It performs no file parsing or
moving-piece sample rewrite. Heightfields are stored once; element references
preserve sharing independently of offset labels. Element rectangles, cached raw
bounds, flags, offsets, stamps, ordered grid references, pieces and uninterpreted
path metadata remain observable in owned snapshots. Inconsistent stored height
maxima remain inputs; the module does not repair them from sample values.

The supported operations collect terrain elements, query floor height and height
below a point, sample height at a point, test blocked samples, sweep a circle,
slide, apply the gravity/ground tail and move an isolated object with explicit
velocity. Moving-piece collision and height operations affect subsequent queries.
All gameplay arithmetic uses integer raw patterns through the existing core.
No host floating-point operation, Godot dependency or viewer arithmetic enters
these rules.

The caller uses `Level::next_stamp()` before direct collection; the modeled compound queries
advance it at their original points. Collection retains cell order, duplicate
suppression and the original limit-minus-one stop. A clear sweep has no contact
normal in the Python contract. Slide retains the last written normal and negative
contact times; it does not clamp them into a new movement policy.

`object_move_isolated` explicitly covers the published no-dynamic-neighbor subset.
Push-out, touch and standing on other objects are absent from that model. The
separate ground operation accepts an explicit standing height. The module does
not supply Player_move, Creature_moveBy, input mapping, water, root animation,
world loading, ray queries, pathfinding or a Session movement adapter.

## Bounds and evidence limits

Construction validates owned topology and bounded array sizes. Per-call work
limits are an authored safety policy, shared across nested queries; refusals
retain already completed visit-stamp changes. They are tested separately from
finite original-model parity. The Python model already clips sample spans to a
heightfield: the original instruction loop walks outside samples individually,
which can produce an enormous workload on pathological movement. Native parity
with that bounded model does not establish the original loop's running time.

Local verification passed all three affected CTest cases and 376 API checks.
All 3,276 complete cases match the original Python outputs under all sixteen
arithmetic settings, in both optimized and ASan/UBSan builds. All five linked
runtime sources (three core and two terrain units) and both new entrypoints were
instrumented. macOS leak detection was disabled; no leak-check claim is made.
Computed trace SHA256:
`8427d784893c4b9e217e3a79d65f8605d59be6d97ad4e4a27ced9d67319a887a`.

Three independent full generator runs produced identical compressed fixtures and
manifests. The generator also checked 71,808 world/core arithmetic comparisons;
a separate root check verified 4,704 floor/ceil comparisons. Both cover all sixteen
settings. Hand tests include retained snapshots, the exact piece mask, malformed
topology, caller-controlled stamp refresh/wraparound and budget exhaustion across
nested slide and ground calls.

The Python suite ran 252 tests with 123 explicit environment/native skips; native
transport tests ran through CTest. All six owned C++ files pass LLVM 20 formatting;
both runtime units and entrypoints pass GCC 15 syntax checks. The arithmetic and
dependency boundary covers 46 simulation files. Existing runtime sources and
legacy fixtures remain unchanged from the verified PR46 baseline.

Platform CI runs the full 62-case suite and compares 25 game-system suites on
macOS arm64/x64, Windows arm64/x64 and Linux x64. Completed platform evidence
belongs to this implementation's pull request, separately from local evidence.
The eight authored terrain fixtures do not establish original level loading.

This is a collision service prerequisite. Playable movement, original-console
networking, measured console FPU behavior and Norrath compatibility need separate
evidence. Existing title labels do not validate retail revision or content.
