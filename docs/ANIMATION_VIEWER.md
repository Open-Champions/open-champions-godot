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
