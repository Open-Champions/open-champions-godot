#pragma once
#include "session_anim_cache_trace.hpp"
#include "session_facing_trace.hpp"
namespace champions::test::control_anim_cache_trace {
using namespace session_trace;
namespace C = R::session_control;
inline std::string setup(const C::AnimationCache &a) {
  R::animation_decoder::Snapshot d;
  for (const auto &c : a.clips)
    d.clips.push_back({d.clips.size() + 1, c});
  auto tree =
      animation_runtime_trace::parsed(animation_decoder_trace::snapshot(d));
  return sequence({animation_runtime_trace::encoded(tree.children[2]),
                   n(a.slots), n(a.capacity), n(a.calls_per_tick), n(a.history),
                   n(a.key_records), n(a.loop_subtractions)});
}
inline std::string control(const C::Snapshot &s) {
  auto base = s;
  base.config.animation_cache.reset();
  base.session.animation_cache.reset();
  std::vector<std::string> commands;
  for (const auto &c : s.animation_commands)
    commands.push_back(sequence({n(c.id), n(c.frame), n(unsigned(c.kind)),
                                 n(c.clip), n(c.consumed), n(c.completed)}));
  return sequence(
      {facing_trace::control(base),
       s.config.animation_cache ? setup(*s.config.animation_cache) : "_",
       s.session.animation_cache
           ? session_anim_cache_trace::cache(*s.session.animation_cache)
           : "_",
       n(s.next_animation_command), sequence(commands),
       n(s.selected_animation_track)});
}
inline C::Config config(unsigned f = 0) {
  auto c = facing_trace::config(f, 0, true);
  c.max_ready_ticks = C::MAX_ANIM_READY_TICKS;
  c.animation_cache = C::diagnostic_animation_cache();
  // Three equal-valued distinct IDs exercise identity and compaction.
  c.animation_cache->clips.resize(3, c.animation_cache->clips.front());
  return c;
}
} // namespace champions::test::control_anim_cache_trace
