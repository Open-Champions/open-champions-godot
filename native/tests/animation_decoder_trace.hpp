#pragma once
// Authored bounded integer/list transport. The explicit input describes state
// and commands; no corpus-specific recipe or expected value drives execution.
#include "champions/animation_decoder.hpp"
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace champions::test::animation_decoder_trace {
namespace A = simulation::rta::animation_decoder;
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
    if (depth > 12 || ++nodes > 262144)
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
        if (v.items.size() == 16384)
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
template <class C> std::string integers(const C &values) {
  std::vector<std::string> out;
  for (auto value : values)
    out.push_back(n(value));
  return sequence(out);
}
inline std::string bytes(const std::string &value) {
  std::vector<std::string> out;
  for (unsigned char ch : value)
    out.push_back(n(ch));
  return sequence(out);
}
inline std::vector<std::uint8_t> input_bytes(const Value &value,
                                             std::size_t maximum) {
  std::vector<std::uint8_t> out;
  for (const auto &x : value.bounded_array(maximum))
    out.push_back(x.integer<std::uint8_t>());
  return out;
}
inline std::string input_name(const Value &value) {
  const auto data = input_bytes(value, 4096);
  return std::string(data.begin(), data.end());
}
inline A::Limits input_limits(const Value &value) {
  const auto &v = value.array(6);
  return {v[0].integer<std::size_t>(), v[1].integer<std::size_t>(),
          v[2].integer<std::size_t>(), v[3].integer<std::size_t>(),
          v[4].integer<std::size_t>(), v[5].integer<std::size_t>()};
}
inline A::Track input_track(const Value &value) {
  const auto &v = value.array(9);
  return {v[0].integer<A::ClipId>(),   v[1].integer<A::SlotId>(),
          v[2].integer<R::Raw>(),      v[3].integer<R::Raw>(),
          v[4].integer<R::Raw>(),      v[5].integer<R::Raw>(),
          v[6].integer<R::Raw>(),      v[7].integer<std::uint32_t>(),
          v[8].integer<std::int16_t>()};
}
inline A::Joint input_joint(const Value &value) {
  const auto &v = value.array(16);
  A::Joint j;
  j.pt = v[0].integer<std::int64_t>();
  j.rt = v[1].integer<std::int64_t>();
  for (std::size_t i = 0; i < 3; ++i) {
    j.pos[i] = v[2 + i].integer<R::Raw>();
    j.vel[i] = v[5 + i].integer<R::Raw>();
  }
  for (std::size_t i = 0; i < 4; ++i) {
    j.quat[i] = v[8 + i].integer<R::Raw>();
    j.qvel[i] = v[12 + i].integer<R::Raw>();
  }
  return j;
}
inline A::Slot input_slot(const Value &value) {
  const auto &v = value.array(7);
  A::Slot s;
  s.index = v[0].integer<std::uint32_t>();
  s.clip = v[1].integer<A::ClipId>();
  s.cursor = v[2].integer<std::uint64_t>();
  s.tick = v[3].integer<std::int64_t>();
  s.owner = v[4].integer<A::TrackId>();
  s.yaw = v[5].integer<std::int32_t>();
  const auto &joints = v[6].array(A::MAX_JOINTS);
  for (std::size_t i = 0; i < joints.size(); ++i)
    s.joints[i] = input_joint(joints[i]);
  return s;
}
inline std::string joint(const A::Joint &j) {
  std::vector<std::string> values{n(j.pt), n(j.rt)};
  for (auto v : j.pos)
    values.push_back(n(v));
  for (auto v : j.vel)
    values.push_back(n(v));
  for (auto v : j.quat)
    values.push_back(n(v));
  for (auto v : j.qvel)
    values.push_back(n(v));
  return sequence(values);
}
inline std::string track(const A::Track &t) {
  return sequence({n(t.clip), n(t.slot), n(t.weight), n(t.rate), n(t.life),
                   n(t.life_rate), n(t.time), n(t.flags), n(t.sound_idx)});
}
inline std::string snapshot(const A::Snapshot &s) {
  std::vector<std::string> clips, tracks, slots;
  for (const auto &entry : s.clips) {
    const auto &c = entry.value;
    std::vector<std::string> bind, events;
    for (const auto &v : c.bind)
      bind.push_back(integers(v));
    for (const auto &v : c.events)
      events.push_back(sequence({n(v.seconds), n(v.mask)}));
    clips.push_back(sequence(
        {n(entry.id), bytes(c.name), integers(c.data), n(c.joint_count),
         n(c.length), sequence(bind), integers(c.depth), sequence(events),
         c.sound_times ? integers(*c.sound_times) : "_", n(c.sound_group),
         n(c.joint_mask), integers(c.words)}));
  }
  for (const auto &entry : s.tracks)
    tracks.push_back(sequence({n(entry.id), track(entry.value)}));
  for (const auto &slot : s.slots) {
    std::vector<std::string> joints;
    for (const auto &j : slot.joints)
      joints.push_back(joint(j));
    slots.push_back(
        sequence({n(slot.index), n(slot.clip), n(slot.cursor), n(slot.tick),
                  n(slot.owner), n(slot.yaw), sequence(joints)}));
  }
  const auto &l = s.limits;
  return sequence({sequence({n(s.sim_frame), n(s.cosmetic_seed)}),
                   sequence({n(l.input_bytes), n(l.owned_bytes), n(l.clips),
                             n(l.tracks), n(l.slots), n(l.key_records)}),
                   sequence(clips), sequence(tracks), sequence(slots),
                   integers(s.order)});
}
class Replay {
  A::Context context;

public:
  explicit Replay(const Value &value)
      : context(value.array(7)[4].bounded_array(256).size(),
                input_limits(value.array(7)[1])) {
    const auto &v = value.array(7);
    const auto &system = v[0].array(2);
    context.sim_frame = system[0].integer<std::uint32_t>();
    context.cosmetic_seed = system[1].integer<std::uint32_t>();
    for (const auto &entry : v[2].bounded_array(64)) {
      const auto &clip = entry.array(2);
      context.add_clip(input_bytes(clip[1], 16384), input_name(clip[0]));
    }
    for (const auto &entry : v[3].bounded_array(256))
      context.add_track(input_track(entry));
    const auto &slots = v[4].bounded_array(256);
    for (std::size_t i = 0; i < slots.size(); ++i)
      context.slot(static_cast<A::SlotId>(i + 1)) = input_slot(slots[i]);
    std::vector<A::SlotId> order;
    for (const auto &entry : v[5].bounded_array(256))
      order.push_back(entry.integer<A::SlotId>());
    context.order() = std::move(order);
    (void)context.snapshot();
  }
  std::string observe() const { return snapshot(context.snapshot()); }
  std::string command(const Value &value) {
    const auto &v = value.bounded_array(6);
    if (v.empty())
      throw std::runtime_error("empty command");
    const auto opcode = v[0].integer<unsigned>();
    switch (opcode) {
    case 0: {
      value.array(3);
      return n(context.add_clip(input_bytes(v[2], 16384), input_name(v[1])));
    }
    case 1:
      value.array(1);
      return n(context.alloc());
    case 2:
      value.array(2);
      context.touch(v[1].integer<A::SlotId>());
      break;
    case 3:
      value.array(2);
      return n(context.animtrack_decode(v[1].integer<A::TrackId>()));
    case 4:
      value.array(4);
      return integers(context.joint_pos(v[1].integer<A::SlotId>(),
                                        v[2].integer<std::int64_t>(),
                                        v[3].integer<R::Raw>()));
    case 5:
      value.array(4);
      return integers(context.joint_quat(v[1].integer<A::SlotId>(),
                                         v[2].integer<std::int64_t>(),
                                         v[3].integer<R::Raw>()));
    case 6: {
      value.array(4);
      std::vector<std::string> out;
      for (const auto &j : context.decode_clip(v[1].integer<A::ClipId>(),
                                               v[2].integer<R::Raw>(),
                                               v[3].integer<std::uint32_t>()))
        out.push_back(sequence({integers(j.position), integers(j.quaternion)}));
      return sequence(out);
    }
    case 7: {
      value.array(4);
      auto &t = context.track(v[1].integer<A::TrackId>());
      switch (v[2].integer<unsigned>()) {
      case 0:
        t.clip = v[3].integer<A::ClipId>();
        break;
      case 1:
        t.slot = v[3].integer<A::SlotId>();
        break;
      case 2:
        t.weight = v[3].integer<R::Raw>();
        break;
      case 3:
        t.rate = v[3].integer<R::Raw>();
        break;
      case 4:
        t.life = v[3].integer<R::Raw>();
        break;
      case 5:
        t.life_rate = v[3].integer<R::Raw>();
        break;
      case 6:
        t.time = v[3].integer<R::Raw>();
        break;
      case 7:
        t.flags = v[3].integer<std::uint32_t>();
        break;
      case 8:
        t.sound_idx = v[3].integer<std::int16_t>();
        break;
      default:
        throw std::runtime_error("unknown track field");
      }
      break;
    }
    case 8: {
      value.array(3);
      auto &destination = context.track(v[1].integer<A::TrackId>());
      const auto source = context.track(v[2].integer<A::TrackId>());
      destination = source;
      break;
    }
    case 9: {
      value.array(5);
      const auto index = v[2].integer<std::size_t>();
      if (index >= 4)
        throw std::runtime_error("event slot range");
      auto &event = context.clip(v[1].integer<A::ClipId>()).events[index];
      event = {v[3].integer<R::Raw>(), v[4].integer<std::uint8_t>()};
      break;
    }
    case 10:
      value.array(3);
      return n(context.clip(v[1].integer<A::ClipId>())
                   .word(v[2].integer<std::int64_t>()));
    default:
      throw std::runtime_error("unknown opcode");
    }
    return "_";
  }
};
inline std::string execute(unsigned config, const std::string &source) {
  if (config >= 16)
    throw std::runtime_error("invalid FPU config");
  const R::ScopedFpu fpu{R::Fpu{static_cast<R::Fpu::Model>(config / 4),
                                (config & 2) != 0, (config & 1) != 0}};
  const auto input = Parser(source).parse();
  const auto &v = input.array(7);
  Replay replay(input);
  const auto initial = replay.observe();
  const auto commands = v[6].canonical();
  constexpr std::size_t max_observation = 2 * 1024 * 1024;
  std::size_t size = initial.size() + commands.size() + 8;
  if (size > max_observation)
    throw std::runtime_error("observation budget exceeded");
  std::vector<std::string> results;
  for (const auto &cmd : v[6].bounded_array(128)) {
    std::string result;
    try {
      result = replay.command(cmd);
    } catch (const A::HostFault &) {
      result = "s486f73744661756c74";
    } catch (const A::ValueError &) {
      result = "s56616c75654572726f72";
    } catch (const A::IndexError &) {
      result = "s496e6465784572726f72";
    }
    auto step = sequence({result, replay.observe()});
    if (step.size() + 1 > max_observation - size)
      throw std::runtime_error("observation budget exceeded");
    size += step.size() + 1;
    results.push_back(std::move(step));
  }
  return sequence({initial, commands, sequence(results)});
}
// Scenario labels are coverage metadata only, never executable recipes.
inline const std::vector<std::string> &operations() {
  static const std::vector<std::string> names = {"field_width",
                                                 "sample_time",
                                                 "seek_restart",
                                                 "clip_identity",
                                                 "stale_owner_copy",
                                                 "protected_pool",
                                                 "allocation_preserves",
                                                 "root_baseline",
                                                 "slot_clip_mismatch",
                                                 "stale_64_joints",
                                                 "million_no_restart",
                                                 "closing_delta",
                                                 "convenience_owner",
                                                 "clip_parse",
                                                 "initialization_eof",
                                                 "record_eof",
                                                 "joint_raw",
                                                 "full_pool",
                                                 "tick_wrap",
                                                 "zero_joints",
                                                 "lru_touch",
                                                 "clip_word"};
  return names;
}
} // namespace champions::test::animation_decoder_trace
