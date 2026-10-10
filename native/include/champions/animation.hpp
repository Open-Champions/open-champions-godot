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
} // namespace champions
