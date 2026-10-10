# Explicit RTA skin sampling in Godot

`ChampionsArchiveInspector.sample_skinned_model(path, model_row_index,
animation_row_index, profile, archive_fingerprint, tick)` exposes the portable
position skinning operation to Godot. Select both original archive row indices
explicitly and pass the fingerprint returned by `inspect_archive`. The method
reads one owned archive snapshot, validates its entire nested structure and
resolves both rows from that snapshot. Duplicate entry names cannot redirect a
selection. It does not infer a companion animation from a name or folder.

Only the existing bounded Return to Arms VIF and ANM subsets are accepted.
Attachments, partial-body clips, invalid referenced joints, malformed influences
and unsupported formats retain their existing rejection rules. The tick must be
finite, nonnegative and within the clip duration. Out-of-range samples are
rejected rather than clamped. Norrath remains rejected pending independent format evidence.

## Result

Successful results contain `ok`, `error`, `model_name`, `animation_name`, `tick`,
`duration_ticks`, `triangle_count`, `pairing_verified` and `meshes`. Each mesh
contains its original `index`, `visible_by_default` and an owned
`PackedVector3Array` of `positions` in original triangle-corner order. Positions
remain in native Z-up model space. Conversion to Godot's single-precision values
must stay finite. Normals, UVs, textures and materials are not returned by this
position-only method.

`pairing_verified` is always false. Archive membership and accepted joint indices
do not certify model/animation compatibility. A consumer must show that boundary
and obtain separate resource-selection evidence before claiming an original pair.
The method returns a fresh empty failure result after any read, parse, numeric or
allocation error; it never publishes partially skinned geometry.

## Scope and next step

This bridge is an integration primitive. The [experimental sampled model viewer](SKINNING_VIEWER.md) now consumes it
with explicit selections and unlit geometry. Retail pairing, original material
rendering and playback remain unverified or unfinished. Neither game is playable.

An initial rendered consumer should use explicit selections and unlit geometry,
retain unsupported attachment/blend rejection and label the pairing as unverified.
Original material rendering and normal deformation require their own evidence.
The [native arithmetic evidence](SKINNING_POSITIONS.md) and
[animation binding contract](https://github.com/Open-Champions/champions-return-to-arms-re/blob/45668f7/docs/formats/anim-audio.md#28-binding-a-clip-to-a-model)
remain separate from this adapter's checks. Claude's ongoing PS2 animation testing
is not replaced by the native binary64 policy.

Implementation and verification are tracked in
[issue 17](https://github.com/Open-Champions/open-champions-godot/issues/17).

## Authored integration checks

The source project passes 242 checks through Godot 4.5.2 and the real extension
on Apple Silicon macOS. The new cases independently anchor rigid and two-, three-
and four-influence positions, rotations and translations. They exercise fractional,
backward and repeated ticks, joint 63, hidden mesh retention, owned results,
duplicate names and explicit cross-container row selection. Rejection cases cover
unsupported profiles, attachment models, partial masks (including out-of-table
bits), invalid zero-weight references, malformed streams, stale fingerprints and
later malformed containers. Failure checks require empty geometry and reset metadata.

The same local tree passes all 14 native CTest cases and 49 Python tooling tests.
Both arm64/x86_64 slices build for the Mac debug and release extension libraries.
No browser UI or rendering behavior is added by this method; the new numeric
integration checks do not establish original-game rendering or model/clip fidelity.

## Five-platform execution

At source revision `c2d915c7ee47e81bd3e7c9be3429bb29822510b2`,
[Godot CI](https://github.com/Open-Champions/open-champions-godot/actions/runs/38065630448)
passed on Apple Silicon and Intel macOS, ARM64 and x64 Windows, and x64 Linux.
Each host passed 242 source-project checks, 242 checks in the actual standalone
export and 49 Python tooling tests. Downloaded exported-smoke logs independently
confirm all five 242-check runs had zero failures. These are headless integration
checks; no new rendering or graphical interaction is claimed.

[Native CI](https://github.com/Open-Champions/open-champions-godot/actions/runs/38065630446)
passed 14 CTest cases on each host. Its aggregate job passed 28 comparator tests
and compared 36,248 double components across the existing authored pose/skin
traces. This is fixture agreement within the documented tolerance, not all-input
or PS2-runtime equivalence.

The local Universal 2 export at this revision also passed 242 checks on Apple
Silicon. Existing user-facing downloads remain at `fa7347b`, since the bridge
adds no selectable browser feature yet. No original game assets entered source,
fixtures or exported packages.
