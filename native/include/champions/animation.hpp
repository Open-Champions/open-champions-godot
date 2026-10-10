#pragma once
#include "champions/archive.hpp"
#include <array>
#include <cstdint>
#include <vector>

namespace champions {
struct RtaAnimationSkeleton {
    std::uint32_t duration_ticks; //60 ticks per second; no looping policy is stored here.
    std::uint64_t joint_mask; // Set bit means the clip does not drive that joint.
    std::vector<std::uint8_t> depths;
    std::vector<std::int16_t> parents; // Joint indices; -1 for each root.
    std::vector<std::array<std::int16_t, 3>> bind_offsets; // Exact signed file units /64, Z-up model space.
};
// Skeleton metadata only, from an unrelocated RTA .anm file (0..64 joints,
// at most16MiB). Supports header -> depth list at0x70 -> bind records -> keys,
// with optional later sound pointers. Rejects invalid ranges, depth transitions,
// and nonzero bind-record padding. Unknown header fields are not interpreted.
// Success does NOT validate key/sound contents, build a pose, bind a model,
// provide playback, or establish Norrath compatibility. Returned storage is owned.
RtaAnimationSkeleton readRtaAnimationSkeleton(Bytes bytes);
enum class AnimationKeyChannel : std::uint8_t { Rotation = 0, Position = 1 };
struct RtaAnimationInitial {
    std::array<std::int16_t, 3> position_raw{}; // File units /64.
    std::array<std::int16_t, 4> quaternion_wxyz_raw{}; // File units /4096; not normalized.
    std::uint8_t position_width = 0;
    std::uint8_t rotation_width = 0;
};
struct RtaAnimationKey {
    std::uint64_t tick = 0;
    std::uint8_t delta_ticks = 0;
    std::uint8_t joint = 0;
    AnimationKeyChannel channel = AnimationKeyChannel::Rotation;
    std::uint8_t width = 0;
    // Velocity, not pose: position xyz /256 (fourth entry0), rotation wxyz /131072.
    std::array<std::int16_t, 4> values_raw{};
};
inline constexpr std::size_t max_animation_keys = 200000;
struct RtaAnimationKeys {
    RtaAnimationSkeleton skeleton;
    std::vector<RtaAnimationInitial> initial;
    std::vector<RtaAnimationKey> keys;
};
// Bounded unrelocated lump key stream. Sound-bearing clips require both sound
// pointers and the documented relative-offset block signature before the names.
// The terminal delta255 must end in zero bits at the next4-byte stream boundary;
// external stream padding is unsupported. Real delta255 records remain valid.
// Limits:16MiB input,64 joints,200000 output records. Raw values are owned.
// Does not parse sounds, evaluate poses, animate, pair models, or support Norrath.
RtaAnimationKeys decodeRtaAnimationKeys(Bytes bytes);
struct RtaAnimationJointPose {
    std::array<double, 3> position{}; // Local joint values, Z-up; no hierarchy or root-motion extraction.
    std::array<double, 4> quaternion_wxyz{}; // Normalized final sample, or all zeros when invalid.
    bool rotation_valid = false;
    bool drives_joint = false; // False when the skeleton's joint-mask bit is set.
};
struct RtaAnimationPose {
    double tick = 0;
    std::vector<RtaAnimationJointPose> joints;
};
// Stateless single-clip sample at finite0..duration ticks. Validates the complete
// mutable input (including keys after the query), then replays keys <=floor(tick)
// in stored order. Settled values use exact int64 units, final arithmetic uses
// ordered binary64 operations and final-only quaternion normalization. This is an
// authored numeric policy, not PS2 floating-point equivalence. Tiny squared norms
// are rescaled before normalization to avoid underflow. Compile without
// fast-math or floating-point contraction. Zero-norm rotation remains zero/invalid.
// No looping, blending, root motion, hierarchy, skinning, playback, or Norrath claim.
RtaAnimationPose sampleRtaAnimationPose(const RtaAnimationKeys& animation, double tick);
struct RtaAnimationTransform {
    std::array<double, 12> model{}; // Row-major3x4; column vectors: v'=R*v+t.
    std::array<double, 12> skin{}; // Same rotation; translation=model.R*(bind_raw/64)+model.t.
    bool valid = false; // Invalid joint/ancestor leaves both matrices allzero.
    bool drives_joint = false; // Own clip mask flag only; no partial-body blending.
};
struct RtaAnimationTransforms {
    double tick = 0;
    std::vector<RtaAnimationTransform> joints;
};
// Parent-first model transforms and skin matrices using the stored signed bind
// offsets directly (never negated again). Validates all public input fields,
// parent/depth consistency, mask flags and finite tick/values. Valid rotations
// require |squared_norm-1|<=1e-12; invalid rotations must be allzero. An invalid
// ancestor invalidates descendants; independent roots remain usable. Nonfinite
// arithmetic is rejected. Binary64 inspection policy, not PS2 fixed-point proof.
// No pose blending, model pairing, skin deformation, playback or Norrath claim.
RtaAnimationTransforms buildRtaAnimationTransforms(const RtaAnimationSkeleton& skeleton,
                                                  const RtaAnimationPose& pose);
} // namespace champions
