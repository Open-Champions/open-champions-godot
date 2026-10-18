#include "internal.hpp"
namespace champions::simulation::rta::creature_anim {
void Runtime::step_sounds(StateId, TrackId id, Raw old_time,
                          std::uint32_t flags) {
  auto &t = track(id);
  const auto &c = clip(t.clip);
  const auto &times = *c.sound_times;
  auto raw = [&](int i) {
    if (i < 0 || static_cast<std::size_t>(i) >= times.size())
      throw HostFault("sound cursor outside event table");
    return times[i];
  };
  auto tick = [&](int i) { return from_int(static_cast<Raw>(raw(i))); };
  auto end_run = [&](int first) {
    auto end = first + 1;
    while (raw(end) == raw(first))
      ++end;
    return end;
  };
  int index = t.sound_idx;
  if (index > 0 && le(old_time, tick(index - 1)))
    t.sound_idx = index = 0;
  if (lt(tick(index), old_time)) {
    do {
      index = s16(index + 1);
    } while (lt(tick(index), old_time));
    t.sound_idx = static_cast<std::int16_t>(index);
  }
  bool muted = false;
  if (c.sound_group) {
    auto &cell = cue_throttle[c.sound_group];
    auto frame = static_cast<std::uint8_t>(draw_frame());
    const int limit = group_limits[c.sound_group] < 128
                          ? group_limits[c.sound_group]
                          : int(group_limits[c.sound_group]) - 256;
    if (cell[0] != frame) {
      cell[0] = frame;
      cell[1] = 1;
    } else if (limit - 2 < cell[1])
      muted = true;
    else
      ++cell[1];
  }
  auto emit = [&](int first, int end) {
    if (!muted) {
      if (sound_log.size() >= 65536)
        throw ResourceLimit("sound range log full; consume before stepping");
      sound_log.push_back({t.clip, first, end});
    }
    t.sound_idx = static_cast<std::int16_t>(s16(end));
  };
  index = t.sound_idx;
  while (lt(tick(index), t.time)) {
    auto end = end_run(index);
    emit(index, end);
    index = t.sound_idx;
  }
  auto span = from_int(static_cast<Raw>(c.length));
  if (le(span, t.time) && (flags & LOOP)) {
    auto wrapped = sub(t.time, span);
    t.sound_idx = 0;
    if (lt(tick(0), wrapped))
      do {
        int first = t.sound_idx;
        emit(first, end_run(first));
      } while (lt(tick(t.sound_idx), wrapped));
  }
}
} // namespace champions::simulation::rta::creature_anim
