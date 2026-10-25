#pragma once
#include "champions/animation_runtime.hpp"
#include <optional>

namespace champions::simulation::rta::session {
// Authored host bounds, distinct from the original runtime behavior.
inline constexpr const char *ANIMATION_CACHE_POLICY =
    "rta-session-anim-cache-v1";
struct AnimationCacheLimits {
  animation_runtime::Limits runtime{};
  std::size_t calls_per_tick = 64, history = 4096;
};
struct AnimationCacheDecoded {
  animation_runtime::TrackId track = 0;
  animation_runtime::SlotId slot = 0;
  Raw time = ZERO;
  std::vector<animation_runtime::JointValues> joints;
};
struct AnimationCacheOperation {
  std::string kind;
  std::uint64_t binding = 0;
  animation_runtime::ClipId clip = 0;
  animation_runtime::TrackId track = 0;
  std::uint32_t frame = 0;
  Raw parameter = ZERO;
  std::uint32_t flags = 0;
  std::size_t event_index = 0;
  animation_decoder::Event event{};
  std::optional<std::int32_t> result;
  std::optional<AnimationCacheDecoded> decoded;
  bool completed = false;
};
} // namespace champions::simulation::rta::session
