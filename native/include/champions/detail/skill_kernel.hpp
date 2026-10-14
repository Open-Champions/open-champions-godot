// MIT; Copyright (c) 2026 TrustlessMike and contributors.
#pragma once
#include "champions/core_arithmetic.hpp"

// RTA only. Pure primitives from the skills contracts pinned at 356add27.
// Adapters own skipped reads, domain checks, rounding, stores and observations.
namespace champions::simulation::rta::detail::skill_kernel {
inline Raw capped_level(Raw level, Raw cap) noexcept {
  return s32(cap) > 0 && s32(cap) < s32(level) ? cap : level;
}
inline bool tier2_fallback(std::int32_t first) noexcept { return first < 0; }
inline Raw delta(Raw level, std::int32_t first) noexcept {
  return level - static_cast<Raw>(first);
}
// Keep conversion and evaluation separable so adapters retain their read order.
inline Raw statistic_step(Raw step, Raw level_delta) {
  return mul(step, from_int(level_delta));
}
inline Raw statistic_base(Raw signed_base) { return from_int(signed_base); }
inline Raw statistic_sum(Raw converted_base, Raw scaled_step) {
  return add(converted_base, scaled_step);
}
inline Raw lcg_step(Raw seed) noexcept {
  return seed * 0x19660du + 0x3c6ef35fu;
}
inline Raw lcg_result(Raw stepped_seed) noexcept { return stepped_seed >> 16; }
} // namespace champions::simulation::rta::detail::skill_kernel
