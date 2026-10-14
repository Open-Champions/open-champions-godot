#include "champions/control_anim_cache.hpp"
#include <stdexcept>
namespace champions::simulation::rta::session_control {
session::AnimationCacheLimits
control_animation_limits(const AnimationCache &v) {
  session::AnimationCacheLimits l;
  l.runtime.decoder = {1024, 16384, 4, 8, 4, v.key_records};
  l.runtime.states = 1;
  l.runtime.state_cells = 8;
  l.runtime.max_loop_subtractions = v.loop_subtractions;
  l.calls_per_tick = v.calls_per_tick;
  l.history = v.history;
  return l;
}
void validate_control_animation(const AnimationCache &v) {
  if (v.clips.empty() || v.clips.size() > 4 || v.slots < 2 || v.slots > 4 ||
      v.capacity < 1 || v.capacity > 8 || !v.calls_per_tick ||
      v.calls_per_tick > 10 || !v.history || v.history > 1024 ||
      !v.key_records || v.key_records > 256 || !v.loop_subtractions ||
      v.loop_subtractions > 256)
    throw std::invalid_argument("control animation setup bounds");
  animation_runtime::Runtime check(v.slots,
                                   control_animation_limits(v).runtime);
  for (const auto &c : v.clips) {
    if (c.joint_count < 1 || c.joint_count > 4 || c.length < 1 ||
        c.length > 65536 || c.bind.size() != std::size_t(c.joint_count) ||
        c.depth.size() != std::size_t(c.joint_count) || c.words.size() > 512 ||
        (c.sound_times && c.sound_times->size() > 64))
      throw std::invalid_argument("control animation clip domain");
    check.add_clip(c);
  }
  check.add_state(v.capacity);
}
AnimationCache diagnostic_animation_cache() {
  // Authored one-joint stream: initial signed position/quaternion then a
  // position-velocity record at tick zero, followed by distant lookahead.
  std::vector<bool> bits;
  auto put = [&](unsigned v, unsigned width) {
    for (unsigned i = width; i; --i)
      bits.push_back((v >> (i - 1)) & 1);
  };
  put(7, 4);
  put(16, 8);
  put(0, 8);
  put(0, 8);
  put(15, 4);
  put(32767, 16);
  put(0, 16);
  put(0, 16);
  put(0, 16);
  put(0, 8);
  put(1, 1);
  put(0, 6);
  put(7, 4);
  put(32, 8);
  put(0, 8);
  put(0, 8);
  put(255, 8);
  put(1, 1);
  put(0, 6);
  put(0, 4);
  put(0, 3);
  animation_runtime::Clip c;
  c.name = "Authored diagnostic joint";
  c.joint_count = 1;
  c.length = 120;
  c.bind.resize(1);
  c.depth.resize(1);
  c.events[0] = {0x3d088888, 1};
  c.words.resize((bits.size() + 15) / 16);
  for (std::size_t i = 0; i < bits.size(); ++i)
    if (bits[i])
      c.words[i / 16] |= std::uint16_t(1u << (15 - i % 16));
  AnimationCache v;
  v.clips.push_back(std::move(c));
  return v;
}
} // namespace champions::simulation::rta::session_control
