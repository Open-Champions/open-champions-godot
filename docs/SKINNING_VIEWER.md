# Explicit model and clip inspection

The experimental Return to Arms viewer draws sampled positions from the
[Godot skinning bridge](SKINNING_BRIDGE.md). It displays default-visible meshes
with one opaque, two-sided, unlit color. It does not reuse rest-pose normals,
textures or inferred game materials.

## Choose and sample

Open a local GOB or LMP under the Return to Arms profile. Select a supported
`.vif` row and choose **Use model**. Then select an `.anm` row and choose
**Sample model + clip**. These are explicit archive row selections; identical
names or archive membership do not prove that the resources belong together.
The window always labels the pairing **unverified**.

Type a fractional tick and choose **Sample**. Sampling can move backward or
repeat; there is no automatic playback. Drag to orbit, scroll to zoom and use
**Frame** to fit the current visible geometry. Manual samples retain the camera
and render origin until Frame is requested.

Changing the archive, filter or game profile clears the pairing. Changing the
selected row clears the previous skinned geometry; the explicitly retained model
can be used to choose a clip within the same archive context. Failed reads,
unsupported inputs and stale fingerprints clear the result rather than retaining
an earlier pose.

## Bounds and evidence

Attachment models and partial-body clips remain rejected by the native bridge.
Norrath decoding still requires independent evidence. Malformed, nonfinite or
oversized geometry is rejected, including hidden meshes; hidden meshes are not
drawn. Empty or all-hidden results display an honest empty state.

The viewer maps native Z-up positions to Godot Y-up exactly once. Frame rebases
all visible mesh positions around one center to keep the camera usable at large
coordinates. Godot's single-precision positions can already have lost small
coordinate differences before rebasing; this is not binary64 rendering fidelity.

This is an asset inspector. Resource-selected retail pair compatibility,
original materials, normal deformation, blending, equipment assembly, playback
and game behavior require separate validation. Neither game is playable.

Tracked in [issue 18](https://github.com/Open-Champions/open-champions-godot/issues/18).

## Verified execution

At source revision `1f2685c7c58b1b880bf7140671742aa1c68cbb2a`,
[Godot CI](https://github.com/Open-Champions/open-champions-godot/actions/runs/38066843403)
passed 283 source-project checks, 283 actual exported-app checks and 49 Python
tooling tests on Apple Silicon and Intel macOS, ARM64 and x64 Windows, and x64
Linux. Downloaded exported-smoke logs independently confirm zero failures on
all five hosts. Authored checks cover geometry/axis mapping, hidden and empty
meshes, invalid mutable results, owned arrays, duplicate/filtered row identities,
stale archive cleanup, partial-pose/attachment rejection, manual time changes,
camera rebasing, input release and canceled-picker state.

[Native CI](https://github.com/Open-Champions/open-champions-godot/actions/runs/38066843390)
passed all 14 CTest cases per host and the existing five-host pose/skin trace
comparison. The portable numeric policy is unchanged by this presentation feature.

The actual local Universal 2 exported app was graphically inspected on Apple
Silicon macOS with an authored weighted VIF/ANM archive. The native picker,
explicit selections, visible unlit triangle, fractional 1.5 and backward 0 tick
samples, drag, wheel, Frame, close and Clear were exercised. The geometry changed
on sampling while camera orientation stayed fixed. No original assets were used
for this graphical check, and no retail pairing or visual fidelity is claimed.
Other hosts' checks are headless; Windows/Intel Mac graphical interaction and
clean-machine installation remain unverified.

The delivered Mac Universal 2 and Windows x64/ARM64 ZIPs are pinned to this source
revision; their checksums and embedded build metadata were independently checked.
They contain authored UI and the extension, without original game files. Earlier
deliveries were preserved locally. Packages remain unsigned and not notarized.
