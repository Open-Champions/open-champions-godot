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
} // namespace champions
