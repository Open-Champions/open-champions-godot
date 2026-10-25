#pragma once
#include "champions/session_anim_cache.hpp"

namespace champions::simulation::rta::session_control {
inline constexpr const char *CONTROL_ANIM_CACHE_POLICY =
    "rta-control-anim-cache-v1";
inline constexpr std::size_t MAX_ANIM_READY_TICKS = 128, MAX_ANIM_COMMANDS = 64,
                             MAX_ANIM_COMMANDS_PER_FRAME = 4;
// Authored save-compatible domain. Values are independently owned, even equal
// values registered twice. There is no pointer, filename or callback setup.
struct AnimationCache {
  std::vector<animation_runtime::Clip> clips;
  std::size_t slots = 2;
  std::int32_t capacity = 4;
  std::size_t calls_per_tick = 10, history = 1024;
  std::size_t key_records = 256;
  std::uint32_t loop_subtractions = 256;
};
enum class AnimationCommandKind : std::uint8_t { Start, Stop, Restart };
struct AnimationCommand {
  std::uint64_t id = 0;
  std::int64_t frame = 0;
  AnimationCommandKind kind = AnimationCommandKind::Start;
  animation_runtime::ClipId clip = 0;
  bool consumed = false, completed = false;
};
session::AnimationCacheLimits control_animation_limits(const AnimationCache &);
void validate_control_animation(const AnimationCache &);
AnimationCache diagnostic_animation_cache();
} // namespace champions::simulation::rta::session_control
