#pragma once
// Complete owned Session animation observations and an input-driven authored
// task harness. The expected TSV columns are never read by this interpreter.
#include "animation_clock_trace.hpp"
#include "session_snapshot_trace.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace champions::test::session_animation_trace {
using namespace session_trace;
namespace D = animation_clock_trace;
namespace Clock = simulation::rta::animation_clock;
inline std::string animation(const S::AnimationSnapshot &a) {
  std::vector<std::string> clips, bindings, operations;
  for (const auto &c : a.clips) {
    std::vector<std::string> events;
    for (const auto &e : c.value.events)
      events.push_back(sequence({n(e.seconds), n(e.mask)}));
    clips.push_back(sequence(
        {n(c.id), sequence({n(c.value.length), n(c.value.joint_mask),
                            sequence(events), n(c.value.has_sound_table)})}));
  }
  for (const auto &b : a.bindings) {
    std::vector<std::string> tracks;
    for (const auto &t : b.tracks)
      tracks.push_back(
          sequence({n(t.clip), n(t.slot), n(t.weight), n(t.rate), n(t.life),
                    n(t.life_rate), n(t.time), n(t.flags), n(t.sound_idx)}));
    bindings.push_back(sequence(
        {sequence({n(b.binding.id), n(b.binding.world_serial),
                   n(b.binding.address), n(b.binding.adapter_generation)}),
         n(b.pose), n(b.count), n(b.capacity), n(b.flags), sequence(tracks),
         n(b.calls_this_tick), n(b.advanced_this_tick),
         optional_number(b.last_event)}));
  }
  for (const auto &op : a.operations)
    operations.push_back(
        sequence({text(op.kind), n(op.binding), n(op.clip), n(op.frame),
                  n(op.parameter), n(op.flags), n(op.event_index),
                  sequence({n(op.event.seconds), n(op.event.mask)}),
                  optional_number(op.result), n(op.completed)}));
  return sequence(
      {sequence({n(a.limits.clips), n(a.limits.bindings), n(a.limits.cells),
                 n(a.limits.calls_per_tick),
                 n(a.limits.clock.max_loop_subtractions)}),
       sequence({n(a.system.sim_frame), n(a.system.cosmetic_seed),
                 n(a.system.limits.max_loop_subtractions)}),
       sequence(clips), sequence(bindings), text(a.policy), text(a.phase),
       n(a.depth), n(a.calls_this_tick), n(a.total_cells), sequence(operations),
       n(a.object_run_completed), optional_number(a.task_priority)});
}
inline std::string observation(const S::Snapshot &s) {
  if (s.turning)
    throw std::runtime_error(
        "session animation trace cannot omit active turning extension");
  if (s.creature_movement)
    throw std::runtime_error(
        "session animation trace does not omit creature extension");
  auto base = s;
  base.animation.reset();
  return sequence({snapshot_base_fields(base),
                   s.animation ? animation(*s.animation) : "_"});
}
struct TaskFailure : std::runtime_error {
  TaskFailure() : std::runtime_error("authored task failure") {}
};
inline bool boolean(const D::Value &v) {
  auto x = v.integer<unsigned>();
  if (x > 1)
    throw std::runtime_error("DSL boolean range");
  return x != 0;
}
inline N::ByteVector byte_list(const D::Value &v, std::size_t limit) {
  N::ByteVector out;
  for (const auto &x : v.bounded_array(limit))
    out.push_back(x.integer<std::uint8_t>());
  return out;
}
class Replay {
  D::Value input;
  S::Session session;
  std::array<std::uint32_t, 2> addresses{};
  std::array<std::uint64_t, 2> serials{};
  std::vector<std::string> effects;
  std::size_t ready = 0;
  static S::Config config(unsigned cfg, const D::Value &value) {
    const auto &a = value.array(7);
    if (cfg > 15 || a[0].integer<unsigned>() > 1)
      throw std::runtime_error("DSL config");
    S::Config c;
    c.model = static_cast<R::Fpu::Model>(cfg / 4);
    c.mul_bias = cfg & 2;
    c.div_nearest = cfg & 1;
    c.peers = 2;
    c.local = 0;
    c.network_mode = 1;
    c.seed = (a[0].integer<unsigned>() ? 0xdeadbeefu : 1u) ^ 0xa5a5a5a5u;
    return c;
  }
  void effect(const std::string &name, std::vector<std::string> fields = {}) {
    auto s = session.snapshot();
    std::vector<std::string> row{text(name)};
    row.insert(row.end(), fields.begin(), fields.end());
    row.insert(row.end(), {n(s.world->frame), n(s.network.net_frame),
                           n(s.world->shared_seed)});
    effects.push_back(sequence(row));
  }
  void task(unsigned index) {
    effect(std::array<std::string, 3>{"hp_pre", "hp_apply", "hp_post"}[index]);
    if (index == 0) {
      ++ready;
      return;
    }
    if (index != 1)
      return;
    const auto &programs = input.items[5].bounded_array(32);
    if (ready > programs.size())
      return;
    for (const auto &command : programs.at(ready - 1).bounded_array(64)) {
      const auto &a = command.bounded_array(6);
      if (a.empty())
        throw std::runtime_error("empty task command");
      auto code = a[0].integer<unsigned>();
      std::optional<std::int32_t> result;
      if (code == 0) {
        command.array(5);
        result = session.start_animation(
            a[1].integer<S::HpId>(), a[2].integer<S::AnimationClipId>(),
            a[3].integer<R::Raw>(), a[4].integer<std::uint32_t>());
      } else if (code == 1) {
        command.array(4);
        session.stop_animation(a[1].integer<S::HpId>(),
                               a[2].integer<S::AnimationClipId>(),
                               a[3].integer<R::Raw>());
      } else if (code == 2) {
        command.array(5);
        result = session.advance_animation(a[1].integer<S::HpId>(),
                                           a[2].integer<R::Raw>());
      } else if (code == 3) {
        command.array(5);
        session.set_animation_event(
            a[1].integer<S::AnimationClipId>(), a[2].integer<std::size_t>(),
            {a[3].integer<R::Raw>(), a[4].integer<std::uint8_t>()});
      } else if (code == 4) {
        command.array(2);
        auto id = a[1].integer<unsigned>();
        if (id < 1 || id > 2)
          throw std::runtime_error("unknown destroy ID");
        session.with_world([&](O::World &w) {
          w.destroy(*w.objects.at(addresses[id - 1]), 3);
        });
        continue;
      } else if (code == 5) {
        command.array(1);
        throw TaskFailure();
      } else
        throw std::runtime_error("unknown task opcode");
      effects.push_back(sequence({text("command"), command.canonical(),
                                  optional_number(result),
                                  n(session.snapshot().world->frame)}));
      if (code == 2 &&
          (static_cast<std::uint32_t>(*result) & a[3].integer<std::uint32_t>()))
        session.take_damage(a[1].integer<S::HpId>(), a[4].integer<R::Raw>(), {},
                            0x40);
    }
  }

public:
  Replay(unsigned cfg, D::Value value)
      : input(std::move(value)), session(config(cfg, input)) {
    const auto &a = input.items;
    const auto variant = a[0].integer<unsigned>();
    S::ObjectBindings b;
    b.on_run = {"authored_on_run", [this](O::GameObject &o) {
                  effect("run", {n(o.serial)});
                  o.tag = o.tag.value_or(0) + 1;
                }};
    b.on_destroy = {
        "authored_on_destroy", [this](O::GameObject &o, std::int64_t flags) {
          session.with_world([&](O::World &w) { w.destroy(o, flags); });
        }};
    b.is_frozen = {"authored_is_frozen",
                   [this] { return boolean(input.items[1]); }};
    b.res_find = {"authored_res_find",
                  [](const auto &, const auto &) { return 0u; }};
    b.end_of_tick = {"authored_end_of_tick", [this] { effect("end"); }};
    session.enable_objects(variant ? 0xdeadbeefu : 1u, b);
    for (unsigned i = 0; i < 3; ++i)
      session.register_object_task(
          0xf1000001 + i,
          std::array<std::string, 3>{"hp_pre", "hp_apply", "hp_post"}[i],
          20 + 10 * i, 0, [this, i](S::Session &) { task(i); });
    session.with_world([&](O::World &w) {
      for (unsigned i = 0; i < 2; ++i) {
        auto *o = w.new_object(
            0x400, {R::from_int(i ? 3 : 1), R::from_int(i ? 4 : variant), 0});
        o->flags = 32;
        o->tag = 0;
        addresses[i] = o->id;
        serials[i] = o->serial;
      }
    });
    session.with_world(
        [&](O::World &w) { w.run_every_frame(*w.objects.at(addresses[0])); });
    session.enable_hp();
    for (unsigned i = 0; i < 2; ++i) {
      S::HpInitial initial;
      initial.hp = 0x42c80000;
      session.bind_hp(serials[i], addresses[i], initial);
    }
    for (unsigned i = 0; i < 2; ++i)
      session.seed_hp_backing(i + 1, {{0xc8, 4, 0},
                                      {0x231, 1, 0},
                                      {0x2dc, 4, 0},
                                      {0x2e0, 2, 0},
                                      {0x304, 4, 0},
                                      {0x3b0, 2, 0}});
    const auto &l = a[2].array(5);
    S::AnimationLimits limits;
    limits.clips = l[0].integer<std::size_t>();
    limits.bindings = l[1].integer<std::size_t>();
    limits.cells = l[2].integer<std::size_t>();
    limits.calls_per_tick = l[3].integer<std::size_t>();
    limits.clock.max_loop_subtractions = l[4].integer<std::uint32_t>();
    session.enable_animation(limits);
    for (const auto &row : a[3].bounded_array(64)) {
      const auto &c = row.array(4);
      Clock::Clip clip;
      clip.length = c[0].integer<std::int32_t>();
      clip.joint_mask = c[1].integer<std::uint64_t>();
      clip.has_sound_table = boolean(c[3]);
      const auto &events = c[2].array(4);
      for (unsigned i = 0; i < 4; ++i) {
        const auto &e = events[i].array(2);
        clip.events[i] = {e[0].integer<R::Raw>(), e[1].integer<std::uint8_t>()};
      }
      session.add_animation_clip(clip);
    }
    for (const auto &row : a[4].bounded_array(16)) {
      const auto &r = row.array(2);
      session.bind_animation(r[0].integer<S::HpId>(),
                             r[1].integer<std::int32_t>());
    }
  }
  std::string run() {
    const auto initial = observation(session.snapshot());
    auto previous = parsed(initial);
    std::vector<std::string> steps;
    for (const auto &entry : input.items[6].bounded_array(32)) {
      const auto &v = entry.array(9);
      std::string result;
      try {
        session.set_level_pending(boolean(v[7]));
        std::vector<N::ByteVector> inbox;
        for (const auto &p : v[8].bounded_array(16))
          inbox.push_back(byte_list(p, 2048));
        auto r = session.submit(
            byte_list(v[0], 8), v[1].integer<std::int64_t>(),
            v[2].integer<std::int64_t>(), v[3].integer<std::int64_t>(),
            boolean(v[4]), boolean(v[5]), v[6].integer<std::int64_t>(), inbox);
        result = sequence({sends(r.sent), n(r.held), n(r.steps)});
      } catch (const TaskFailure &) {
        result = text("TaskFailure");
      } catch (const Clock::HostFault &) {
        result = text("HostFault");
      } catch (const S::Error &) {
        result = text("Refusal");
      }
      const auto current = parsed(observation(session.snapshot()));
      const auto patch = delta(previous, current);
      if (encoded(apply_delta(previous, patch)) != encoded(current))
        throw std::runtime_error("lossless delta reconstruction failed");
      steps.push_back(sequence({result, patch}));
      previous = current;
    }
    return sequence(
        {input.canonical(), initial, sequence(steps), sequence(effects)});
  }
};
inline std::string execute(unsigned config, const std::string &source) {
  Replay replay(config, D::Parser(source).parse());
  return replay.run();
}
} // namespace champions::test::session_animation_trace
