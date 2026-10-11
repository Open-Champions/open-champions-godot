#pragma once
// Authored bounded integer/list transport. The explicit input describes state
// and commands; no corpus-specific recipe or expected value drives execution.
#include "champions/animation_clock.hpp"
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace champions::test::animation_clock_trace {
namespace A = simulation::rta::animation_clock;
namespace R = simulation::rta;
inline std::string sequence(const std::vector<std::string> &v) {
  std::string out = "[";
  for (const auto &x : v) {
    if (out.size() > 1)
      out += ',';
    out += x;
  }
  return out + ']';
}
template <class T> std::string n(T value) { return std::to_string(value); }
struct Value {
  bool list = false, negative = false;
  std::uint64_t magnitude = 0;
  std::vector<Value> items;
  const std::vector<Value> &array(std::size_t count) const {
    if (!list || items.size() != count)
      throw std::runtime_error("invalid DSL shape");
    return items;
  }
  const std::vector<Value> &bounded_array(std::size_t maximum) const {
    if (!list || items.size() > maximum)
      throw std::runtime_error("DSL list budget or shape");
    return items;
  }
  template <class T> T integer() const {
    static_assert(std::is_integral_v<T> && !std::is_same_v<T, bool>);
    if (list)
      throw std::runtime_error("expected DSL integer");
    if constexpr (std::is_signed_v<T>) {
      const auto max =
          static_cast<std::uint64_t>(std::numeric_limits<T>::max());
      if (magnitude > max + (negative ? 1ULL : 0ULL))
        throw std::runtime_error("DSL signed integer range");
      if (negative && magnitude == max + 1ULL)
        return std::numeric_limits<T>::min();
      const auto positive = static_cast<T>(magnitude);
      return negative ? static_cast<T>(-positive) : positive;
    } else {
      if (negative || magnitude > std::numeric_limits<T>::max())
        throw std::runtime_error("DSL unsigned integer range");
      return static_cast<T>(magnitude);
    }
  }
  std::string canonical() const {
    if (!list)
      return (negative ? "-" : "") + n(magnitude);
    std::vector<std::string> out;
    for (const auto &v : items)
      out.push_back(v.canonical());
    return sequence(out);
  }
};
class Parser {
  const std::string &text;
  std::size_t at = 0, nodes = 0;
  Value value(unsigned depth) {
    if (depth > 10 || ++nodes > 8192)
      throw std::runtime_error("DSL depth/node budget");
    if (at == text.size())
      throw std::runtime_error("truncated DSL");
    Value v;
    if (text[at] == '[') {
      v.list = true;
      ++at;
      if (at < text.size() && text[at] == ']') {
        ++at;
        return v;
      }
      for (;;) {
        if (v.items.size() == 2048)
          throw std::runtime_error("DSL list budget");
        v.items.push_back(value(depth + 1));
        if (at == text.size())
          throw std::runtime_error("truncated DSL list");
        if (text[at] == ']') {
          ++at;
          return v;
        }
        if (text[at++] != ',')
          throw std::runtime_error("invalid DSL separator");
      }
    }
    if (text[at] == '-') {
      v.negative = true;
      ++at;
    }
    const auto start = at;
    while (at < text.size() && text[at] >= '0' && text[at] <= '9') {
      const unsigned digit = static_cast<unsigned>(text[at++] - '0');
      if (v.magnitude >
          (std::numeric_limits<std::uint64_t>::max() - digit) / 10)
        throw std::runtime_error("DSL integer overflow");
      v.magnitude = v.magnitude * 10 + digit;
    }
    if (at == start || (at - start > 1 && text[start] == '0') ||
        (v.negative && v.magnitude == 0))
      throw std::runtime_error("noncanonical DSL integer");
    return v;
  }

public:
  explicit Parser(const std::string &source) : text(source) {}
  Value parse() {
    auto result = value(0);
    if (at != text.size())
      throw std::runtime_error("trailing DSL bytes");
    return result;
  }
};
inline const std::vector<std::string> &operations() {
  static const std::vector<std::string> names = {"anim_clear",
                                                 "anim_push",
                                                 "anim_push_with_weights",
                                                 "anim_push_chained",
                                                 "anim_start",
                                                 "anim_start_blend_in_out",
                                                 "anim_stop",
                                                 "anim_drop_non_persistent",
                                                 "anim_release_caches",
                                                 "anim_state_dtor",
                                                 "anim_sync_to_sim_frame",
                                                 "anim_randomize_top_time",
                                                 "anim_sync_top_to_below",
                                                 "anim_jump_top_to_end",
                                                 "anim_snap_top_to_end",
                                                 "anim_find_track",
                                                 "anim_find_track_by_flag",
                                                 "anim_fade_in_track",
                                                 "anim_fade_out_track",
                                                 "anim_set_track_weight",
                                                 "anim_state_copy",
                                                 "anim_advance"};
  return names;
}
class Replay {
  A::AnimSystem system;
  std::vector<A::ClipPtr> clips;
  std::vector<A::AnimState> states;
  A::ClipPtr clip(const Value &v) const {
    const auto id = v.integer<std::uint32_t>();
    if (id > clips.size())
      throw std::runtime_error("unknown clip ID");
    return id ? clips[id - 1] : nullptr;
  }
  std::size_t clip_id(const A::ClipPtr &c) const {
    if (!c)
      return 0;
    for (std::size_t i = 0; i < clips.size(); ++i)
      if (c == clips[i])
        return i + 1;
    throw std::runtime_error("unregistered output clip");
  }
  A::AnimState &state(const Value &v) {
    return states.at(v.integer<std::uint32_t>());
  }
  void field(A::Track &t, unsigned key, const Value &v) {
    switch (key) {
    case 0:
      t.clip = clip(v);
      break;
    case 1:
      t.slot = v.integer<std::uint64_t>();
      break;
    case 2:
      t.weight = v.integer<R::Raw>();
      break;
    case 3:
      t.rate = v.integer<R::Raw>();
      break;
    case 4:
      t.life = v.integer<R::Raw>();
      break;
    case 5:
      t.life_rate = v.integer<R::Raw>();
      break;
    case 6:
      t.time = v.integer<R::Raw>();
      break;
    case 7:
      t.flags = v.integer<decltype(t.flags)>();
      break;
    case 8:
      t.sound_idx = v.integer<std::int16_t>();
      break;
    default:
      throw std::runtime_error("unknown track field");
    }
  }
  static std::string pointer(const A::AnimState &s, const A::Track *p) {
    if (!p)
      return "_";
    for (std::size_t i = 0; i < s.tracks.size(); ++i)
      if (p == &s.tracks[i])
        return n(i);
    throw std::runtime_error("output pointer is not a state cell");
  }

public:
  explicit Replay(const Value &initial) {
    const auto &v = initial.array(4);
    const auto &sys = v[0].array(2);
    system.sim_frame = sys[0].integer<std::uint32_t>();
    system.cosmetic_seed = sys[1].integer<std::uint32_t>();
    system.limits.max_loop_subtractions =
        v[1].array(1)[0].integer<std::uint32_t>();
    if (system.limits.max_loop_subtractions > 65536)
      throw std::runtime_error("DSL loop budget");
    for (const auto &entry : v[2].bounded_array(64)) {
      const auto &row = entry.array(5);
      if (row[0].integer<std::uint32_t>() != clips.size() + 1)
        throw std::runtime_error("nonsequential clip registry");
      auto c = std::make_shared<A::Clip>();
      c->length = row[1].integer<std::int32_t>();
      c->joint_mask = row[2].integer<std::uint64_t>();
      const auto &events = row[3].array(4);
      for (std::size_t i = 0; i < 4; ++i) {
        const auto &e = events[i].array(2);
        c->events[i] = {e[0].integer<R::Raw>(), e[1].integer<std::uint8_t>()};
      }
      const auto sound = row[4].integer<unsigned>();
      if (sound > 1)
        throw std::runtime_error("invalid sound presence");
      c->has_sound_table = sound != 0;
      clips.push_back(std::move(c));
    }
    for (const auto &entry : v[3].bounded_array(16)) {
      const auto &row = entry.array(5);
      const auto cap = row[2].integer<std::int32_t>();
      A::AnimState s(cap);
      s.pose = row[0].integer<std::uint64_t>();
      s.count = row[1].integer<std::int32_t>();
      s.flags = row[3].integer<decltype(s.flags)>();
      if (s.pose || s.count < 0 || s.count > cap)
        throw std::runtime_error("invalid initial state");
      const auto &tracks = row[4].array(static_cast<std::size_t>(cap));
      for (std::size_t i = 0; i < tracks.size(); ++i) {
        const auto &t = tracks[i].array(9);
        for (unsigned f = 0; f < 9; ++f)
          field(s.tracks[i], f, t[f]);
        if (s.tracks[i].slot)
          throw std::runtime_error("cache-bearing DSL unsupported");
      }
      states.push_back(std::move(s));
    }
    if (states.empty())
      throw std::runtime_error("empty state registry");
  }
  std::string snapshot() const {
    std::vector<std::string> cs, ss;
    for (std::size_t i = 0; i < clips.size(); ++i) {
      const auto &c = *clips[i];
      std::vector<std::string> es;
      for (const auto &e : c.events)
        es.push_back(sequence({n(e.seconds), n(e.mask)}));
      cs.push_back(sequence({n(i + 1), n(c.length), n(c.joint_mask),
                             sequence(es), n(c.has_sound_table)}));
    }
    for (const auto &s : states) {
      std::vector<std::string> ts;
      for (const auto &t : s.tracks)
        ts.push_back(sequence({n(clip_id(t.clip)), n(t.slot), n(t.weight),
                               n(t.rate), n(t.life), n(t.life_rate), n(t.time),
                               n(t.flags), n(t.sound_idx)}));
      ss.push_back(sequence(
          {n(s.pose), n(s.count), n(s.capacity), n(s.flags), sequence(ts)}));
    }
    return sequence({sequence({n(system.sim_frame), n(system.cosmetic_seed)}),
                     sequence({n(system.limits.max_loop_subtractions)}),
                     sequence(cs), sequence(ss)});
  }
  std::string command(const Value &command) {
    const auto &c = command.bounded_array(9);
    if (c.empty())
      throw std::runtime_error("empty command");
    const auto op = c[0].integer<unsigned>();
    static constexpr unsigned sizes[] = {2, 4, 8, 4, 5, 6, 4, 2, 2, 2, 2, 2, 2,
                                         2, 2, 3, 3, 4, 4, 4, 3, 3, 3, 5, 5};
    if (op >= 25 || c.size() != sizes[op])
      throw std::runtime_error("unknown command or arity");
    if (op == 24) {
      auto p = clip(c[1]);
      if (!p)
        throw std::runtime_error("null event clip");
      p->events.at(c[2].integer<unsigned>()) = {c[3].integer<R::Raw>(),
                                                c[4].integer<std::uint8_t>()};
      return "_";
    }
    if (op == 15 && c[1].integer<std::int64_t>() == -1) {
      if (A::anim_find_track(nullptr, clip(c[2])))
        throw std::runtime_error("null find result");
      return "_";
    }
    auto &s = state(c[1]);
    switch (op) {
    case 0:
      A::anim_clear(s);
      break;
    case 1:
      return pointer(
          s, A::anim_push(s, clip(c[2]), c[3].integer<std::uint32_t>()));
    case 2:
      return pointer(s, A::anim_push_with_weights(
                            s, clip(c[2]), c[3].integer<std::uint32_t>(),
                            c[4].integer<R::Raw>(), c[5].integer<R::Raw>(),
                            c[6].integer<R::Raw>(), c[7].integer<R::Raw>()));
    case 3:
      return pointer(s, A::anim_push_chained(s, clip(c[2]),
                                             c[3].integer<std::uint32_t>()));
    case 4:
      return pointer(s, A::anim_start(c[4].integer<R::Raw>(), s, clip(c[2]),
                                      c[3].integer<std::uint32_t>()));
    case 5:
      return pointer(s, A::anim_start_blend_in_out(
                            c[4].integer<R::Raw>(), c[5].integer<R::Raw>(), s,
                            clip(c[2]), c[3].integer<std::uint32_t>()));
    case 6:
      A::anim_stop(c[3].integer<R::Raw>(), s, clip(c[2]));
      break;
    case 7:
      A::anim_drop_non_persistent(s);
      break;
    case 8:
      A::anim_release_caches(s);
      break;
    case 9:
      A::anim_state_dtor(s);
      break;
    case 10:
      A::anim_sync_to_sim_frame(system, s);
      break;
    case 11:
      A::anim_randomize_top_time(system, s);
      break;
    case 12:
      A::anim_sync_top_to_below(system, s);
      break;
    case 13:
      A::anim_jump_top_to_end(system, s);
      break;
    case 14:
      A::anim_snap_top_to_end(s);
      break;
    case 15:
      return pointer(s, A::anim_find_track(&s, clip(c[2])));
    case 16:
      return n(A::anim_find_track_by_flag(s, c[2].integer<std::uint32_t>()));
    case 17:
      A::anim_fade_in_track(c[3].integer<R::Raw>(), s, clip(c[2]));
      break;
    case 18:
      A::anim_fade_out_track(c[3].integer<R::Raw>(), s, clip(c[2]));
      break;
    case 19:
      A::anim_set_track_weight(c[3].integer<R::Raw>(), s, clip(c[2]));
      break;
    case 20: {
      auto &destination = state(c[2]);
      destination = A::anim_state_copy(system, s);
      return n(c[2].integer<unsigned>());
    }
    case 21:
      return n(A::anim_advance(system, c[2].integer<R::Raw>(), s));
    case 22: {
      const auto count = c[2].integer<std::int32_t>();
      if (count < 0 || count > s.capacity)
        throw std::runtime_error("invalid count mutation");
      s.count = count;
      break;
    }
    case 23: {
      const auto f = c[3].integer<unsigned>();
      if (f == 1)
        throw std::runtime_error("cache mutation unsupported");
      field(s.tracks.at(c[2].integer<unsigned>()), f, c[4]);
      break;
    }
    default:
      throw std::runtime_error("unknown opcode");
    }
    return "_";
  }
};
inline std::string execute(unsigned config, const std::string &source) {
  if (config >= 16)
    throw std::runtime_error("invalid FPU config");
  const R::ScopedFpu context{R::Fpu{static_cast<R::Fpu::Model>(config / 4),
                                    (config & 2) != 0, (config & 1) != 0}};
  const auto input = Parser(source).parse();
  const auto &pair = input.array(2);
  Replay replay(pair[0]);
  const auto initial = replay.snapshot();
  if (initial != pair[0].canonical())
    throw std::runtime_error("initial snapshot mismatch");
  std::vector<std::string> steps;
  for (const auto &c : pair[1].bounded_array(128)) {
    std::string result;
    try {
      result = replay.command(c);
    } catch (const A::HostFault &) {
      result = "s486f73744661756c74";
    }
    steps.push_back(sequence({result, replay.snapshot()}));
  }
  return sequence({initial, pair[1].canonical(), sequence(steps)});
}
} // namespace champions::test::animation_clock_trace
