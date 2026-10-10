# RTA joint hierarchy and skin matrices

`buildRtaAnimationTransforms(skeleton, pose)` converts a validated local pose into
owned model-space joint transforms and skin matrices. It follows the published
[RTA matrix contract](https://github.com/Open-Champions/champions-return-to-arms-re/blob/45668f7/docs/formats/anim-audio.md#25-from-joint-values-to-matrices),
observed at `0x3e5c40` and its callers `0x16ed40` and `0x16f594–0x16f674`.

Matrices are row-major 3x4 arrays, with column-vector multiplication `v' = R*v+t`
and Z-up coordinates. Local rotation uses the pose's w/x/y/z quaternion. Each
model transform is `parent_model * local`; roots have an identity parent.
The skin matrix retains the model rotation and sets translation to
`model_rotation * (bind_offset_raw / 64) + model_translation`. The stored bind
offset already carries the offset sign; it is not negated again. Its interpretation
as a negative bind position is inferred from samples.

## Validity and numeric policy

The operation validates the complete mutable skeleton and pose before use.
Invalid rotations produce zero matrices with an explicit false validity flag,
and invalidate descendants. Independent roots remain usable. Mask flags remain
diagnostic: a partial-body clip needs blending before it can drive a complete
character. No identity rotation is substituted for missing rotation data.

Computation uses ordered binary64 operations with fast-math and floating-point
contraction disabled, matching the native sampler's policy. The original runtime
writes fixed-point matrices; these native results do not reproduce that rounding
or overflow behavior. Cross-platform fixture agreement does not establish PS2
arithmetic equivalence or game-wide multiplayer determinism.

## Scope

This is a portable library operation. The [joint viewer](ANIMATION_VIEWER.md) uses
model-space origins from these matrices. Model/clip pairing, vertex skinning, blending, looping, root motion,
sounds and gameplay remain separate work. Norrath needs independent format and
behavior evidence. Neither game is playable.

Implementation and validation are tracked in
[issue 14](https://github.com/Open-Champions/open-champions-godot/issues/14).

## Local and private evidence

All 447 authored animation checks pass normally and with AddressSanitizer and
UndefinedBehaviorSanitizer. Transform cases cover rotated parent/child composition,
signed and rotated bind offsets, independent roots, deep hierarchies, a constructed
identity skin pose, invalid ancestors, malformed public input and arithmetic overflow.

The published `Anm.model_positions` reference was compared with 131 private RTA
clips over 2,480 queries, including key boundaries, fractional times, backward order
and repeats. All 79,085 joint origins passed the 1e-9 absolute tolerance; the largest
observed coordinate difference was `5.684341886080802e-14`. No invalid rotations
occurred in these sampled clips; authored tests cover that policy. This comparison
checks origins only, not complete skin matrices. Original inputs and expected
values remain private and outside this checkout.

The authored trace gate now includes six cases and 17 samples per platform,
covering 290 joint samples with local poses and both 3x4 matrices. It compares
35,960 numeric components across four peers against Linux, using
`1e-12 + 1e-12 * max(abs(a), abs(b))`. Independent expected matrices exercise
noncommuting parent/child rotations and signed bind translation; an invalid
ancestor case verifies independent roots remain usable. Schema, coverage and
validity flags must match exactly. All 21 comparator tests and 13 CTest cases
pass locally.


## Platform execution evidence

At source revision `2bc0387437e0e9de7901ee96f84334d42b89daf9`,
[native CI](https://github.com/Open-Champions/open-champions-godot/actions/runs/38056659477)
passed all 13 CTest cases on Apple Silicon and Intel macOS, ARM64 and x64 Windows,
and x64 Linux. The aggregate job passed 21 comparator tests and the actual
five-platform trace comparison. A separate comparison of downloaded artifacts
found zero numeric difference across all 35,960 compared components. This covers
the authored fixtures, not all possible inputs or original-runtime equivalence.

[Godot CI](https://github.com/Open-Champions/open-champions-godot/actions/runs/38056659478)
passed on the same five hosts: 150 source-project checks, 150 checks in each
actual exported application, and 42 Python tooling tests (21 packaging and 21
trace-comparator tests). These headless checks protect existing viewer behavior;
the transform builder is not yet compiled into the Godot binding or exposed in
the viewer. No new download was promoted, and existing delivered viewer packages
remain at `a13e71d`.
