#pragma once
#include "champions/detail/checkpoint_fields.hpp"
namespace champions::simulation::rta::detail {
template <class T>
inline void cache_vector(Checkpoint &a, std::vector<T> &v, std::size_t cap) {
  const auto n = a.count(v.size(), cap);
  if (a.reading)
    v.resize(n);
  for (auto &x : v)
    a.value(x);
}
inline void checkpoint_fields(Checkpoint &a, animation_decoder::Limits &v) {
  a.fields(v.input_bytes, v.owned_bytes, v.clips, v.tracks, v.slots,
           v.key_records);
}
inline void checkpoint_fields(Checkpoint &a, animation_runtime::Limits &v) {
  a.fields(v.decoder, v.states, v.state_cells, v.max_loop_subtractions);
}
inline void checkpoint_fields(Checkpoint &a, animation_decoder::Event &v) {
  a.fields(v.seconds, v.mask);
}
inline void checkpoint_fields(Checkpoint &a, animation_decoder::Clip &v) {
  a.fields(v.name);
  cache_vector(a, v.data, 1024);
  a.fields(v.joint_count, v.length);
  cache_vector(a, v.bind, 4);
  cache_vector(a, v.depth, 4);
  a.fields(v.events);
  bool sounds = bool(v.sound_times);
  a.value(sounds);
  if (a.reading) {
    if (sounds)
      v.sound_times.emplace();
    else
      v.sound_times.reset();
  }
  if (sounds)
    cache_vector(a, *v.sound_times, 64);
  a.fields(v.sound_group, v.joint_mask);
  cache_vector(a, v.words, 512);
}
inline void checkpoint_fields(Checkpoint &a, animation_decoder::Track &v) {
  a.fields(v.clip, v.slot, v.weight, v.rate, v.life, v.life_rate, v.time,
           v.flags, v.sound_idx);
}
inline void checkpoint_fields(Checkpoint &a, animation_decoder::Joint &v) {
  a.fields(v.pt, v.rt, v.pos, v.vel, v.quat, v.qvel);
}
inline void checkpoint_fields(Checkpoint &a, animation_decoder::Slot &v) {
  a.fields(v.index, v.clip, v.cursor, v.tick, v.owner, v.yaw, v.joints);
}
inline void checkpoint_fields(Checkpoint &a,
                              animation_decoder::ClipSnapshot &v) {
  a.fields(v.id, v.value);
}
inline void checkpoint_fields(Checkpoint &a,
                              animation_decoder::TrackSnapshot &v) {
  a.fields(v.id, v.value);
}
inline void checkpoint_fields(Checkpoint &a, animation_decoder::Snapshot &v) {
  a.fields(v.sim_frame, v.cosmetic_seed, v.limits);
  cache_vector(a, v.clips, 4);
  cache_vector(a, v.tracks, 8);
  cache_vector(a, v.slots, 4);
  cache_vector(a, v.order, 4);
}
inline void checkpoint_fields(Checkpoint &a, animation_runtime::AnimState &v) {
  a.fields(v.pose, v.count, v.capacity, v.flags);
  cache_vector(a, v.tracks, 8);
}
inline void checkpoint_fields(Checkpoint &a,
                              animation_runtime::StateSnapshot &v) {
  a.fields(v.id, v.value);
}
inline void checkpoint_fields(Checkpoint &a, animation_runtime::Snapshot &v) {
  a.value(v.limits);
  cache_vector(a, v.states, 1);
  a.value(v.decoder);
}
inline void checkpoint_fields(Checkpoint &a,
                              animation_runtime::JointValues &v) {
  a.fields(v.position, v.quaternion);
}
inline void checkpoint_fields(Checkpoint &a,
                              session::AnimationCacheDecoded &v) {
  a.fields(v.track, v.slot, v.time);
  cache_vector(a, v.joints, 4);
  Checkpoint::require(v.track && v.slot && !v.joints.empty() &&
                          v.time <= 0x43000000u,
                      "checkpoint decoded sample domain");
  for (const auto &joint : v.joints) {
    for (auto raw : joint.position)
      Checkpoint::require((raw & 0x7f800000u) != 0x7f800000u,
                          "checkpoint decoded position domain");
    for (auto raw : joint.quaternion)
      Checkpoint::require((raw & 0x7f800000u) != 0x7f800000u,
                          "checkpoint decoded quaternion domain");
  }
}
inline void checkpoint_fields(Checkpoint &a,
                              session::AnimationCacheOperation &v) {
  a.fields(v.kind, v.binding, v.clip, v.track, v.frame, v.parameter, v.flags,
           v.event_index, v.event, v.result, v.decoded, v.completed);
}
inline void checkpoint_fields(Checkpoint &a,
                              session_control::AnimationCache &v) {
  cache_vector(a, v.clips, 4);
  a.fields(v.slots, v.capacity, v.calls_per_tick, v.history, v.key_records,
           v.loop_subtractions);
  session_control::validate_control_animation(v);
}
inline void checkpoint_fields(Checkpoint &a,
                              session_control::AnimationCommand &v) {
  a.fields(v.id, v.frame, v.kind, v.clip, v.consumed, v.completed);
}
} // namespace champions::simulation::rta::detail
