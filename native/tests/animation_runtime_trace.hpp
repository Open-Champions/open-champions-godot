#pragma once
// Explicit authored inputs drive the sole canonical runtime. Expected columns
// are never parsed by this interpreter or used to construct state.
#include "animation_decoder_trace.hpp"
#include "champions/animation_runtime.hpp"
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace champions::test::animation_runtime_trace {
namespace D = animation_decoder_trace;
namespace A = simulation::rta::animation_runtime;
namespace R = simulation::rta;
using D::integers;
using D::n;
using D::sequence;
inline std::string text(const std::string &s) {
  static const char hex[] = "0123456789abcdef";
  std::string out = "s";
  for (unsigned char c : s) {
    out += hex[c >> 4];
    out += hex[c & 15];
  }
  return out;
}
inline A::Limits limits(const D::Value &v) {
  const auto &a = v.array(4);
  return {D::input_limits(a[0]), a[1].integer<std::size_t>(),
          a[2].integer<std::size_t>(), a[3].integer<std::uint32_t>()};
}
inline std::string snapshot(const A::Snapshot &s) {
  const auto &l = s.limits;
  const auto &d = l.decoder;
  std::vector<std::string> states;
  for (const auto &entry : s.states) {
    const auto &v = entry.value;
    states.push_back(
        sequence({n(entry.id), sequence({n(v.pose), n(v.count), n(v.capacity),
                                         n(v.flags), integers(v.tracks)})}));
  }
  return sequence(
      {sequence({sequence({n(d.input_bytes), n(d.owned_bytes), n(d.clips),
                           n(d.tracks), n(d.slots), n(d.key_records)}),
                 n(l.states), n(l.state_cells), n(l.max_loop_subtractions)}),
       sequence(states), D::snapshot(s.decoder)});
}
// Only computed native observations enter this tree parser. Never expected
// data.
struct Tree {
  bool list = false;
  std::string atom;
  std::vector<Tree> children;
};
inline Tree parse_tree(const std::string &s, std::size_t &p, std::size_t &nodes,
                       unsigned depth = 0) {
  if (depth > 16 || ++nodes > 1048576 || p >= s.size())
    throw std::runtime_error("computed observation depth/node budget");
  Tree v;
  if (s[p] == '[') {
    v.list = true;
    ++p;
    if (s.at(p) != ']')
      for (;;) {
        if (v.children.size() >= 65536)
          throw std::runtime_error("computed observation list budget");
        v.children.push_back(parse_tree(s, p, nodes, depth + 1));
        if (s.at(p) != ',')
          break;
        ++p;
      }
    if (s.at(p++) != ']')
      throw std::runtime_error("computed observation list");
  } else {
    auto begin = p;
    while (p < s.size() && s[p] != ',' && s[p] != ']')
      ++p;
    v.atom = s.substr(begin, p - begin);
    if (v.atom.empty())
      throw std::runtime_error("computed observation atom");
  }
  return v;
}
inline Tree parsed(const std::string &s) {
  if (s.size() > 8 * 1024 * 1024)
    throw std::runtime_error("computed observation byte budget");
  std::size_t p = 0, nodes = 0;
  auto v = parse_tree(s, p, nodes);
  if (p != s.size())
    throw std::runtime_error("computed observation tail");
  return v;
}
inline std::string encoded(const Tree &v) {
  if (!v.list)
    return v.atom;
  std::vector<std::string> rows;
  for (const auto &x : v.children)
    rows.push_back(encoded(x));
  return sequence(rows);
}
inline void changes(const Tree &a, const Tree &b,
                    std::vector<std::size_t> &path,
                    std::vector<std::string> &rows) {
  if (a.list && b.list && a.children.size() == b.children.size()) {
    for (std::size_t i = 0; i < a.children.size(); ++i) {
      path.push_back(i);
      changes(a.children[i], b.children[i], path, rows);
      path.pop_back();
    }
  } else if (a.list || b.list || a.atom != b.atom)
    rows.push_back(sequence({integers(path), encoded(b)}));
}
inline std::string delta(const Tree &a, const Tree &b) {
  std::vector<std::size_t> path;
  std::vector<std::string> rows;
  changes(a, b, path, rows);
  return sequence(rows);
}
inline Tree apply_delta(Tree v, const std::string &source) {
  auto patches = parsed(source);
  if (!patches.list)
    throw std::runtime_error("internal delta list");
  for (const auto &p : patches.children) {
    if (!p.list || p.children.size() != 2 || !p.children[0].list)
      throw std::runtime_error("internal delta shape");
    auto *target = &v;
    for (const auto &i : p.children[0].children) {
      if (i.list || !target->list)
        throw std::runtime_error("internal delta path");
      auto index = D::Parser(i.atom).parse().integer<std::size_t>();
      target = &target->children.at(index);
    }
    *target = p.children[1];
  }
  return v;
}
inline void delta_anchor() {
  auto a = parsed("[[18446744073709551615,-9223372036854775808],[],_]");
  auto b = parsed(
      "[[18446744073709551614,-9223372036854775808],[0],s486f73744661756c74]");
  auto d = delta(a, b);
  if (d != "[[[0,0],18446744073709551614],[[1],[0]],[[2],s486f73744661756c74]"
           "]" ||
      encoded(apply_delta(a, d)) != encoded(b))
    throw std::runtime_error("lossless delta literal anchor");
}
class Replay {
  A::Runtime runtime;
  void set_track(const D::Value &value) {
    const auto &v = value.array(4);
    auto &t = runtime.track(v[1].integer<A::TrackId>());
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
  }

public:
  explicit Replay(const D::Value &input)
      : runtime(input.array(8)[5].bounded_array(256).size(),
                limits(input.array(8)[1])) {
    const auto &v = input.items;
    const auto &sys = v[0].array(2);
    runtime.set_sim_frame(sys[0].integer<std::uint32_t>());
    runtime.set_cosmetic_seed(sys[1].integer<std::uint32_t>());
    for (const auto &row : v[2].bounded_array(64)) {
      const auto &c = row.array(2);
      runtime.add_clip(D::input_bytes(c[1], 16384), D::input_name(c[0]));
    }
    std::vector<std::pair<A::TrackId, A::Track>> values;
    for (const auto &row : v[3].bounded_array(256))
      values.push_back({runtime.add_track(), D::input_track(row)});
    for (const auto &row : v[4].bounded_array(64)) {
      const auto &s = row.array(5);
      auto capacity = s[2].integer<std::int32_t>();
      const auto &cells = s[4].array(static_cast<std::size_t>(capacity));
      const auto id = runtime.add_state(capacity);
      runtime.set_state_scalars(id, s[0].integer<std::uint64_t>(),
                                s[1].integer<std::int32_t>(),
                                s[3].integer<std::uint32_t>());
      const auto ids = runtime.state(id).tracks;
      for (std::size_t i = 0; i < cells.size(); ++i)
        values.push_back({ids[i], D::input_track(cells[i])});
    }
    for (const auto &p : values)
      runtime.track(p.first) = p.second;
    const auto &slots = v[5].bounded_array(256);
    for (std::size_t i = 0; i < slots.size(); ++i)
      runtime.slot(static_cast<A::SlotId>(i + 1)) = D::input_slot(slots[i]);
    std::vector<A::SlotId> order;
    for (const auto &id : v[6].bounded_array(256))
      order.push_back(id.integer<A::SlotId>());
    runtime.order() = std::move(order);
  }
  std::string observe() const { return snapshot(runtime.snapshot()); }
  std::string command(const D::Value &value) {
    const auto &v = value.bounded_array(9);
    if (v.empty())
      throw std::runtime_error("empty command");
    auto sid = [&] { return v.at(1).integer<A::StateId>(); };
    auto cid = [&] { return v.at(2).integer<A::ClipId>(); };
    auto flags = [&] { return v.at(3).integer<std::uint32_t>(); };
    auto raw = [&](std::size_t i) { return v.at(i).integer<R::Raw>(); };
    switch (v[0].integer<unsigned>()) {
    case 0:
      value.array(2);
      runtime.anim_clear(sid());
      break;
    case 1:
      value.array(4);
      return n(runtime.anim_push(sid(), cid(), flags()));
    case 2:
      value.array(8);
      return n(runtime.anim_push_with_weights(sid(), cid(), flags(), raw(4),
                                              raw(5), raw(6), raw(7)));
    case 3:
      value.array(4);
      return n(runtime.anim_push_chained(sid(), cid(), flags()));
    case 4:
      value.array(5);
      return n(runtime.anim_start(raw(4), sid(), cid(), flags()));
    case 5:
      value.array(6);
      return n(runtime.anim_start_blend_in_out(raw(4), raw(5), sid(), cid(),
                                               flags()));
    case 6:
      value.array(4);
      runtime.anim_stop(raw(3), sid(), cid());
      break;
    case 7:
      value.array(2);
      runtime.anim_drop_non_persistent(sid());
      break;
    case 8:
      value.array(2);
      runtime.anim_release_caches(sid());
      break;
    case 9:
      value.array(2);
      runtime.anim_state_dtor(sid());
      break;
    case 10:
      value.array(2);
      runtime.anim_sync_to_sim_frame(sid());
      break;
    case 11:
      value.array(2);
      runtime.anim_randomize_top_time(sid());
      break;
    case 12:
      value.array(2);
      runtime.anim_sync_top_to_below(sid());
      break;
    case 13:
      value.array(2);
      runtime.anim_jump_top_to_end(sid());
      break;
    case 14:
      value.array(2);
      runtime.anim_snap_top_to_end(sid());
      break;
    case 15:
      value.array(3);
      return n(runtime.anim_find_track(sid(), cid()));
    case 16:
      value.array(3);
      return n(runtime.anim_find_track_by_flag(sid(),
                                               v[2].integer<std::uint32_t>()));
    case 17:
      value.array(4);
      runtime.anim_fade_in_track(raw(3), sid(), cid());
      break;
    case 18:
      value.array(4);
      runtime.anim_fade_out_track(raw(3), sid(), cid());
      break;
    case 19:
      value.array(4);
      runtime.anim_set_track_weight(raw(3), sid(), cid());
      break;
    case 20:
      value.array(2);
      return n(runtime.anim_state_copy(sid()));
    case 21:
      value.array(3);
      return n(runtime.anim_advance(raw(2), sid(), false, false, false));
    case 22:
      value.array(2);
      return n(runtime.add_state(v[1].integer<std::int32_t>()));
    case 23:
      value.array(3);
      return n(
          runtime.add_clip(D::input_bytes(v[2], 16384), D::input_name(v[1])));
    case 24:
      value.array(2);
      return n(runtime.add_track(D::input_track(v[1])));
    case 25:
      set_track(value);
      break;
    case 26:
      value.array(5);
      runtime.set_state_scalars(sid(), v[2].integer<std::uint64_t>(),
                                v[3].integer<std::int32_t>(),
                                v[4].integer<std::uint32_t>());
      break;
    case 27: {
      value.array(5);
      auto &events = runtime.clip(v[1].integer<A::ClipId>()).events;
      auto i = v[2].integer<std::size_t>();
      if (i >= events.size())
        throw std::runtime_error("event index range");
      events[i] = {raw(3), v[4].integer<std::uint8_t>()};
      break;
    }
    case 28:
      value.array(3);
      if (v[1].integer<unsigned>() == 0)
        runtime.set_sim_frame(v[2].integer<std::uint32_t>());
      else if (v[1].integer<unsigned>() == 1)
        runtime.set_cosmetic_seed(v[2].integer<std::uint32_t>());
      else
        throw std::runtime_error("system field range");
      break;
    case 29:
      value.array(1);
      return n(runtime.alloc());
    case 30:
      value.array(2);
      runtime.touch(v[1].integer<A::SlotId>());
      break;
    case 31:
      value.array(2);
      return n(runtime.animtrack_decode(v[1].integer<A::TrackId>()));
    case 32:
      value.array(4);
      return integers(runtime.joint_pos(v[1].integer<A::SlotId>(),
                                        v[2].integer<std::int64_t>(), raw(3)));
    case 33:
      value.array(4);
      return integers(runtime.joint_quat(v[1].integer<A::SlotId>(),
                                         v[2].integer<std::int64_t>(), raw(3)));
    case 34: {
      value.array(4);
      std::vector<std::string> out;
      for (const auto &j :
           runtime.decode_clip(v[1].integer<A::ClipId>(), raw(2),
                               v[3].integer<std::uint32_t>()))
        out.push_back(sequence({integers(j.position), integers(j.quaternion)}));
      return sequence(out);
    }
    case 35:
      value.array(3);
      runtime.track(v[1].integer<A::TrackId>()) =
          runtime.track(v[2].integer<A::TrackId>());
      break;
    case 36:
      value.array(3);
      return n(runtime.clip(v[1].integer<A::ClipId>())
                   .word(v[2].integer<std::int64_t>()));
    case 37: {
      value.array(4);
      auto &w = runtime.clip(v[1].integer<A::ClipId>()).words;
      auto i = v[2].integer<std::int64_t>();
      if (i < 0)
        i += static_cast<std::int64_t>(w.size());
      if (i < 0 || static_cast<std::uint64_t>(i) >= w.size())
        throw A::IndexError("clip word index");
      w[static_cast<std::size_t>(i)] = v[3].integer<std::uint16_t>();
      break;
    }
    default:
      throw std::runtime_error("unknown opcode");
    }
    return "_";
  }
};
inline std::string execute(unsigned cfg, const std::string &source) {
  if (cfg > 15)
    throw std::runtime_error("configuration range");
  R::ScopedFpu context(R::Fpu(static_cast<R::Fpu::Model>(cfg / 4),
                              (cfg & 2) != 0, (cfg & 1) != 0));
  delta_anchor();
  auto input = D::Parser(source).parse();
  Replay replay(input);
  const auto initial = replay.observe();
  auto previous = parsed(initial);
  std::vector<std::string> steps;
  std::size_t bytes = initial.size() + source.size();
  for (const auto &cmd : input.array(8)[7].bounded_array(256)) {
    std::string result;
    try {
      result = replay.command(cmd);
    } catch (const A::HostFault &) {
      result = text("HostFault");
    } catch (const A::ValueError &) {
      result = text("ValueError");
    } catch (const A::IndexError &) {
      result = text("IndexError");
    }
    auto current = parsed(replay.observe());
    auto patch = delta(previous, current);
    if (encoded(apply_delta(previous, patch)) != encoded(current))
      throw std::runtime_error("lossless delta reconstruction");
    auto row = sequence({result, patch});
    bytes += row.size();
    if (bytes > 2 * 1024 * 1024)
      throw std::runtime_error("computed record byte budget");
    steps.push_back(std::move(row));
    previous = std::move(current);
  }
  return sequence({input.canonical(), initial, sequence(steps)});
}
} // namespace champions::test::animation_runtime_trace
