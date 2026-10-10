# RTA weighted vertex positions

The portable `skinRtaModelPositions(model, transforms)` operation produces owned
model-space positions in each mesh's original corner order. It applies
`sum(weight / 255 * (skin_rotation * rest_position + skin_translation))` in stored
influence order. This follows the published
[weights and joints contract](https://github.com/Open-Champions/champions-return-to-arms-re/blob/45668f7/docs/formats/model-texture.md#weights-and-joints).
Input weights and joint indices remain the exact decoded bytes; no influence
is merged or silently dropped.

The model decoder now retains the signed attachment slot at header `+0x13`.
Direct skinning rejects attached parts: they require a parent's attachment
transform, which this operation does not implement. Partial-body transform sets
are also rejected until blending can supply a complete pose. All referenced
joints, including explicit zero-weight entries, require valid matrices.

The operation validates the mutable input and bounds allocation. Positions use
ordered binary64 arithmetic with fast-math and floating-point contraction disabled.
Nonfinite inputs and arithmetic are rejected. This is a native numeric policy,
not reproduction of the PS2's fixed-point or vector-unit rounding.

This operation does not modify normals, UVs, visibility or materials. It does not
bind a model to a skeleton, certify a pair, deform the Godot viewer or implement
playback. Norrath needs independent evidence. Neither game is playable.

## Pairing evidence boundary

The VIF format has no skeleton identity. Matching names, counts or archive
membership cannot establish compatibility. The published
[animation binding contract](https://github.com/Open-Champions/champions-return-to-arms-re/blob/45668f7/docs/formats/anim-audio.md#28-binding-a-clip-to-a-model)
describes runtime resource selection by object code.

One concrete candidate is the RingPortal inner `apblobs` model and animation,
identified in the published
[RingPortal research](https://github.com/Open-Champions/champions-return-to-arms-re/blob/45668f7/docs/monsters-2.md#L7811).
Two private archive candidates were found, but both model files use flags outside
the current decoder's supported subset. They remain rejected; no original-pair
skinning or rendered fidelity is claimed from this milestone.

Implementation and validation are tracked in
[issue 16](https://github.com/Open-Champions/open-champions-godot/issues/16).


## Private arithmetic comparison

Thirty supported private models supplied 433 meshes and 241,896 corners. Applying
an authored set of alternating identity/Z90 rotations and joint-index translations
matched a separate calculation using the published model reader with zero observed
difference (absolute tolerance 1e-10). Coverage included 196,388 rigid corners,
38,925 two-influence corners, 6,450 three-influence corners and 133 four-influence
corners. Two attachment models were rejected as intended.

These matrices are authored test inputs, not a claim that any animation matches
those models. Original data and expected positions remain private. The comparison
checks decoded corner arithmetic; no original rendered/runtime fidelity is claimed.

## Authored checks and platform gate

All 67 skinning checks pass normally and with AddressSanitizer and
UndefinedBehaviorSanitizer; all 415 model checks pass with the sanitizers as well.
The local build passes 14 CTest cases and 49 Python tooling tests, including 28
trace-comparator tests. Cases cover rigid and two-to-four influence weights,
zero-weight references, joint 63, duplicate seam corners, malformed mutable inputs,
attachment rejection, partial poses, finite arithmetic and allocation budgets.

Seven independently anchored skinning cases contribute 24 corner positions per
platform. Together with the existing 17 pose/transform samples, the strict-FP
trace gate compares 36,248 numeric components across four peers against Linux.
It requires exact schema, revision, coverage and metadata, with numeric tolerance
`1e-12 + 1e-12 * max(abs(a), abs(b))`. Five-platform execution is recorded below
once CI completes.
