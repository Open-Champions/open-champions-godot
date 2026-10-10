# RTA local poses at explicit ticks

`sampleRtaAnimationPose(const RtaAnimationKeys&, double tick)` computes a single
clip's local joint positions and normalized quaternions. It uses the published
[RTA animation contract](https://github.com/Open-Champions/champions-return-to-arms-re/blob/45668f7/docs/formats/anim-audio.md),
sections 2.3–2.6, and the independently decoded [key records](ANIMATION_KEYS.md).
Relevant original addresses are `0x16e468`, `0x16ed40` and `0x16d748`.

Each call starts from the initial pose. Keys through `floor(tick)` are processed
in stored order. A new key first settles its channel's previous velocity to the
key tick, then replaces that velocity. The last segment is sampled at fractional
time. Only the resulting quaternion is normalized; stored bases remain unchanged.
This makes backward and repeated queries independent of previous calls. The tick
must be finite and within `[0, duration]`; sampling at duration does not wrap.

The result keeps explicit w/x/y/z quaternion order and a `drives_joint` flag from
the clip's mask. A zero-norm quaternion remains zero with `rotation_valid=false`,
matching the published reference reader's fallback rather than inventing an
identity rotation. Very small nonzero rotations use a scaled norm when squaring would underflow.
Original runtime behavior for these degenerate cases is unknown.
All public input arrays and key records are validated, including keys after the
requested time, because callers can construct or modify these structures.

## Numeric policy

Settled positions use signed 64-bit integers in units of 1/256; settled quaternion
components use units of 1/131072. Initial raw values are multiplied by 4 and 32,
respectively. Validated channel intervals total at most `UINT32_MAX`, and raw
velocities have magnitude at most 32768, so settled magnitudes remain below 2^48.
This bounds integer arithmetic and keeps settled values exactly representable in
binary64. Fractional sampling and final normalization use explicitly ordered
binary64 operations. The library build disables fast-math and floating-point
contraction; MSVC uses strict floating-point mode.

This is an authored native policy. It follows the recovered velocity semantics
but does not reproduce every PS2 floating-point rounding step. Cross-platform
fixture agreement and reference comparisons are distinct from PS2 execution or
network determinism evidence.

## Scope

The sampler is a portable library operation. It does not form hierarchy matrices,
apply skin weights, pair a model and animation, blend clips, extract root motion,
wrap time, play sounds or animate the Godot viewer. The existing 64-joint and
200,000-key budgets remain. Norrath compatibility requires independent evidence.
Neither game is playable.

## Local checks

All 329 authored animation checks pass with AddressSanitizer and
UndefinedBehaviorSanitizer. Coverage includes key boundaries, same-tick
replacement, fractional/backward/repeated queries, mask bit 63, zero and tiny
rotations, extreme duration/velocity values, mutated future records and budgets.

## Private comparison

The sampler was compared with the committed `Anm.sample` reference on 131 private
clips, using 2,480 time queries and 79,085 joint samples. Queries include clip
endpoints, fractional times around key boundaries, backward order and repeats.
All positions, quaternion components and flags agree; the maximum observed
position and quaternion-component difference is zero on the local Apple Silicon
host. The comparison uses absolute tolerances of 1e-10 for positions and 1e-12
for quaternion components. These sampled results do not establish all-input or
PS2 arithmetic equivalence. Source clips and expected values remain private.

Implementation and cross-platform evidence are tracked in
[issue 13](https://github.com/Open-Champions/open-champions-godot/issues/13).

## Platform trace gate

Four authored cases produce 15 samples per platform: changing position/rotation
channels, a 64-joint mask and extreme duration, a zero quaternion and a tiny
nonzero rotation. The producer checks independent expected values, including a
nonidentity quaternion. The comparator requires the exact five platform identities,
source revision, case coverage, sample times, joint counts and discrete flags.
It rejects missing, duplicate, nonfinite, malformed and placeholder results.

CI collects separate trace artifacts and compares 7,924 double components against
the Linux trace. The allowed difference is `1e-12 + 1e-12 * max(abs(a), abs(b))`.
A native-host failure or missing artifact fails the aggregate gate. All 13 CTest
cases and 15 comparator tests pass locally; the trace producer also passes
AddressSanitizer and UndefinedBehaviorSanitizer.

## Verified platform results

At source revision `5fcab627b6eb49d7ae36c4f7c2a8589543222e3e`,
[native CI](https://github.com/Open-Champions/open-champions-godot/actions/runs/38054835131)
passed all 13 CTest cases on Apple Silicon and Intel macOS, ARM64 and x64 Windows,
and x64 Linux. The aggregate job passed 15 comparator tests and compared the actual
five-host traces. A separate comparison of the downloaded artifacts found a maximum
absolute difference of zero across all 7,924 compared components. This establishes
agreement for these fixtures, not all-input, PS2 or multiplayer equivalence.

[Godot CI](https://github.com/Open-Champions/open-champions-godot/actions/runs/38054835119)
also passed on all five hosts: 150 checks using the source project, 150 checks in
each actual exported application, and 36 Python tooling tests (21 packaging tests
and 15 trace-comparator tests). These headless checks protect the existing viewer;
the sampler has no viewer integration yet. No new download was promoted from this
library-only milestone, and existing delivered viewer packages remain at `a13e71d`.
