# RTA skeleton metadata

The portable `champions-animation` library reads the skeleton metadata in an
unrelocated Return to Arms US v2.00 `.anm` file. It follows the published
[animation contract](https://github.com/Open-Champions/champions-return-to-arms-re/blob/45668f7/docs/formats/anim-audio.md),
sections 2.1, 2.2, 2.5 and 2.8. The addressed readers include `0x1707b8`
(relocation), `0x16f590`/`0x16f5c8` (depths) and `0x16f58c`/`0x16f5d8`
(bind offsets). The implementation is independently authored.

## Returned data

`readRtaAnimationSkeleton(Bytes)` returns owned values: duration in ticks,
64-bit joint mask, joint depths, parent indices and exact signed 16-bit bind
offsets. There are 60 ticks per second. A set mask bit means that the clip does
not drive that joint. Each root has parent index -1; other parents are the
nearest earlier joint one depth level above. Multiple roots are supported.

Bind offsets retain source units of 1/64 in Z-up model space. Their use in the
pose calculation is observed in the published research; their interpretation
as negative model-space bind positions is inferred from samples. This reader
performs no matrix calculation or coordinate conversion. No joint names or
model-to-skeleton association are invented.

## Supported subset and limits

Inputs are bounded to 16 MiB and 0–64 joints. The supported layout starts its
depth list at `0x70`, followed by nonoverlapping bind records and then at least
four bytes at the key-stream offset. Bind-record padding must be zero. Depth
transitions cannot skip a parent level. Zero-joint metadata is valid but supplies
no usable skeleton. Structurally valid depths beyond the observed sample maximum
are accepted; this does not establish original runtime support for those depths.

Optional sound pointers must reference bounded prefixes after the key prefix
and retain their documented order when both are present. Their contents, the
key stream and unconsumed header fields are not interpreted. Metadata success
therefore does not certify a complete animation file. Duration is retained as
an unsigned 32-bit value without assuming a playback policy or allocating by it.

The decoder is available in the portable library; it is not wired into the Godot
viewer. Key decoding and local pose evaluation are separate
[library operations](ANIMATION_POSES.md). Skeleton binding, playback, sound/events,
root motion and looping remain work. Neither same-archive membership nor matching
joint counts establish that a model and skeleton belong together. Norrath needs
separate format evidence. Neither game is playable.

## Verification

The 193 authored skeleton checks cover branching hierarchies, multiple roots,
64-joint chains, exact signed extrema, high mask bits, owned output and malformed
or truncated ranges. They pass with AddressSanitizer and UndefinedBehaviorSanitizer.
All 12 local native CTest cases pass.

An independent comparison uses the published `anim_audio_info.py` reader against
128 distinct private lump clips selected from a 4,510-clip header survey. All
4,035 joint records agree exactly for depth, parent and signed bind offsets;
duration and masks also agree. The selected clips include zero and 64 joints,
depth 16, 64 independent roots and all three nonzero joint-mask cases in the
survey. The survey is not a full validation of all clips or stream-file clips.
Original files and expected asset values stay outside the checkout; only aggregate
results are reported here. This comparison does not validate native key playback.

Implementation and platform evidence are tracked in
[issue 11](https://github.com/Open-Champions/open-champions-godot/issues/11).

At `f578925`, the [native platform run](https://github.com/Open-Champions/open-champions-godot/actions/runs/38050985092)
passed all 12 CTest cases on Apple Silicon/Intel macOS, Windows ARM64/x64 and
Linux x64. The [Godot platform run](https://github.com/Open-Champions/open-champions-godot/actions/runs/38050985089)
passed 150 source and 150 actual exported-app checks plus 21 packaging tests per
host. These viewer checks guard existing behavior; the skeleton library is not
yet connected to presentation. Existing delivered downloads remain at `a13e71d`.
