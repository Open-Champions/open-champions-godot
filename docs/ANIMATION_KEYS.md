# RTA initial poses and velocity keys

The portable `champions-animation` library now decodes the initial poses and
velocity-key records in supported unrelocated RTA US v2.00 lump `.anm` files.
It follows the published [animation contract, section 2.3](https://github.com/Open-Champions/champions-return-to-arms-re/blob/45668f7/docs/formats/anim-audio.md#23-key-stream),
backed by the original decoder at `0x16e468`. The implementation is independently
authored and builds on the [skeleton metadata reader](ANIMATION_METADATA.md).

`decodeRtaAnimationKeys(Bytes)` returns owned skeleton metadata, initial joint
values and key records in their original order. Initial positions retain signed
integers in units of 1/64; quaternion components retain signed integers in units
of 1/4096, explicitly ordered **w, x, y, z** and not normalized. Each channel's
stored bit width is retained. Keys preserve accumulated tick, delta tick, joint,
channel, width and raw signed values. Position keys describe velocity in units
of 1/256 per tick; rotation keys describe quaternion velocity in units of
1/131072 per tick. They are not absolute poses or interpolation targets.

## Boundaries and supported subset

Fields are read MSB first from little-endian 16-bit words, including fields that
cross word boundaries. Every read stays within the key section. For sound-bearing
clips, both optional pointers and the preceding block's relative-offset signature
must validate before that block bounds the key stream. Sound data remains opaque.
Without sound pointers, the stream ends at the end of the supplied file.

A delta of 255 terminates the stream only when followed by canonical zero padding
to its next four-byte boundary. Real 255-tick keys remain records. This also handles
the documented 359- and 599-tick clips whose terminal falls within their duration,
without reading padding or neighboring data as additional keys. Actual keys beyond
the declared duration are rejected. Extra external padding, including padded
stream/lip-sync containers, is outside this initial subset.

The existing 16 MiB input and 64-joint limits remain. Output is capped at 200,000
key records; the cap applies even when every delta is zero. Ticks accumulate in
64-bit integers. Zero-joint clips may contain a terminal but no joint records.
The separate skeleton-only API keeps its prior, narrower validation contract.

This code does not evaluate a pose at a time, form matrices, pair a model with a
skeleton, bind skin weights, implement looping, play sounds or animate Godot.
No Norrath compatibility or playable-game result is established. The library
remains independent of presentation and wall-clock time.

## Verification

All 274 authored animation checks pass normally and with AddressSanitizer and
UndefinedBehaviorSanitizer. Cases include signed widths 1 and 16, crossing words,
distinct quaternion components, zero deltas, joint 63, real delta-255 keys,
long-duration terminals, exact 32-bit alignment, truncation, malformed padding,
sound-boundary isolation and the exact output-budget boundary. All 12 native
CTest cases pass locally.

A private reference-reader survey processed 4,510 lump clips. From it, 131 distinct
clips were selected to retain the prior hierarchy coverage and add key-stream
edge cases. All 4,091 initial joint poses, 17,763 position keys and 37,941 rotation
keys match the published `anim_audio_info.py` reader exactly, including widths,
raw components, record order and tick values. The comparison deliberately reorders
the reference reader's x/y/z/w quaternion output to the native w/x/y/z contract.
Skeleton metadata also agrees. The largest surveyed clip has 8,756 records.

The survey found no real delta-255 key or one-bit velocity-key field; authored
fixtures cover those cases. Original clips and expected asset values remain
outside the checkout. A reference comparison establishes covered decoding,
not original runtime equivalence, time evaluation or all-disc native support.

Tracked in [issue 12](https://github.com/Open-Champions/open-champions-godot/issues/12).
