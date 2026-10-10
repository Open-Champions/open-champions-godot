# Inspecting RTA animation joints

Select Return to Arms, open an owner-supplied GOB or LMP, and select an `.anm`
entry to inspect its joints. The viewer starts at tick zero. Enter a time in ticks
(60 ticks per second) and press **Sample** to inspect another moment. Sampling
preserves the camera; **Frame** centers the current valid joints. Drag to orbit
and scroll to zoom.

This uses the same bounded native [key decoder](ANIMATION_KEYS.md),
[pose sampler](ANIMATION_POSES.md) and [hierarchy transforms](ANIMATION_TRANSFORMS.md).
The bridge validates the archive fingerprint and selected row on every sample,
and returns owned positions and flags only after decoding succeeds. The binding
compiles the animation implementation with its strict floating-point policy.

Positions use the model-space joint origins, not the skin-matrix translations.
Presentation converts native `(x, y, z)` to Godot `(x, z, -y)`. Invalid joints and
links through invalid ancestors are hidden. Masked joints are distinguished as
clip diagnostics; partial-body blending is not implemented. Empty or all-invalid
clips display an empty state. Selection, filtering, title/archive changes and
failures clear stale geometry.

This viewer samples one explicitly chosen clip. It does not pair that clip with
a model, deform vertices, blend tracks, play sounds or run gameplay. Norrath
animation compatibility remains unverified. Original game content is not included.

Implementation and verification are tracked in
[issue 15](https://github.com/Open-Champions/open-champions-godot/issues/15).

## Verification

At source revision `fa7347b`, the Apple Silicon source project and actual
Universal 2 export each passed 199 browser checks. The suite covers fractional
and backward sampling, independent roots, invalid ancestors, mask bit 63, empty
clips, wrong titles/extensions/times, duplicate names, stale fingerprints,
malformed archives, copied results, context cleanup and pointer handling.
All 13 native CTest cases and 42 Python tooling tests also pass locally.

A supported long-duration clip exposed a float32 camera failure when a joint was
far from the origin. The viewer now rebases rendered geometry at Frame, retaining
the original mapped coordinates for inspection. Its regression checks a real
authored `.anm` through the bridge, a finite camera and preserved camera/origin
while sampling backward.

The actual Mac export was inspected graphically on Apple Silicon. Native file
selection, authored joint/line colors, fractional and backward sampling, drag,
wheel zoom, Frame, empty-clip display and Clear were exercised. A private RTA clip
with 29 joints and 28 links was also viewed at ticks 0 and 19.5, then reframed.
No original inputs or screenshots are included in the public repository or package.
These observations do not establish graphical behavior on other platforms or
whole-game compatibility.

## Five-platform evidence and packages

At `fa7347b0420edff90a0ee6fad3e285bef7e540c6`,
[Godot CI](https://github.com/Open-Champions/open-champions-godot/actions/runs/38058896676)
passed 199 source-project checks, 199 actual exported-application checks and
42 Python tooling tests on Apple Silicon and Intel macOS, ARM64 and x64 Windows,
and x64 Linux. [Native CI](https://github.com/Open-Champions/open-champions-godot/actions/runs/38058896662)
passed all 13 CTest cases on those hosts and the 35,960-component numeric trace
comparison. Cross-platform numeric checks cover authored fixtures, not PS2
arithmetic or game-wide network determinism.

Updated Mac and Windows development packages were delivered from this revision.
The Mac package is the locally inspected Universal 2 export; Windows packages
are the tested CI artifacts. Checksums and embedded source revisions were verified.
The packages contain no original game content. Windows and Intel Mac graphical
interaction, clean-machine installation, signing and notarization remain unverified.
