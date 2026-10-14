#pragma once
// Complete owned Session animation observations and an input-driven authored
// task harness. The expected TSV columns are never read by this interpreter.
#include "animation_clock_trace.hpp"
#include "animation_runtime_trace.hpp"
#include "session_snapshot_trace.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace champions::test::session_anim_cache_trace {
using namespace session_trace;
namespace D = animation_decoder_trace;
namespace Clock = simulation::rta::animation_clock;
namespace RT = animation_runtime_trace;
namespace A = simulation::rta::animation_runtime;
inline std::string decoded(const S::AnimationCacheDecoded &d) {
  std::vector<std::string> joints;
  for (const auto &j : d.joints)
    joints.push_back(sequence({integers(j.position), integers(j.quaternion)}));
  return sequence({n(d.track), n(d.slot), n(d.time), sequence(joints)});
}
inline std::string
optional_decoded(const std::optional<S::AnimationCacheDecoded> &d) {
  return d ? decoded(*d) : "_";
}
inline std::string cache(const S::AnimationCacheSnapshot &a) {
  std::vector<std::string> bindings, operations;
  for (const auto &b : a.bindings) {
    const auto &i = b.binding;
    bindings.push_back(sequence(
        {sequence({n(i.id), n(i.world_serial), n(i.address),
                   n(i.adapter_generation)}),
         n(b.state), n(b.calls_this_tick), n(b.advanced_this_tick),
         optional_number(b.last_event), optional_decoded(b.last_decoded)}));
  }
  for (const auto &op : a.operations)
    operations.push_back(
        sequence({text(op.kind), n(op.binding), n(op.clip), n(op.track),
                  n(op.frame), n(op.parameter), n(op.flags), n(op.event_index),
                  sequence({n(op.event.seconds), n(op.event.mask)}),
                  optional_number(op.result), optional_decoded(op.decoded),
                  n(op.completed)}));
  auto r = RT::snapshot(a.runtime);
  const auto &l = a.limits.runtime;
  const auto &d = l.decoder;
  auto limits =
      sequence({sequence({n(d.input_bytes), n(d.owned_bytes), n(d.clips),
                          n(d.tracks), n(d.slots), n(d.key_records)}),
                n(l.states), n(l.state_cells), n(l.max_loop_subtractions)});
  return sequence({sequence({n(a.runtime.decoder.slots.size()), limits,
                             n(a.limits.calls_per_tick), n(a.limits.history)}),
                   r, sequence(bindings), text(a.policy), text(a.phase),
                   n(a.depth), n(a.calls_this_tick), sequence(operations),
                   n(a.object_run_completed),
                   optional_number(a.task_priority)});
}
inline std::string observation(const S::Snapshot &s) {
  if (s.turning || s.recovery || s.creature_movement || s.animation)
    throw std::runtime_error(
        "animation cache trace cannot omit other active extensions");
  auto base = s;
  base.animation_cache.reset();
  return sequence({snapshot_base_fields(base),
                   s.animation_cache ? cache(*s.animation_cache) : "_"});
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
      std::string result = "_";
      std::int32_t event = 0;
      if (code == 0) {
        command.array(5);
        result = n(session.start_animation_cache(
            a[1].integer<S::HpId>(), a[2].integer<A::ClipId>(),
            a[3].integer<R::Raw>(), a[4].integer<std::uint32_t>()));
      } else if (code == 1) {
        command.array(4);
        session.stop_animation_cache(a[1].integer<S::HpId>(),
                                     a[2].integer<A::ClipId>(),
                                     a[3].integer<R::Raw>());
      } else if (code == 2) {
        command.array(5);
        event = session.advance_animation_cache(a[1].integer<S::HpId>(),
                                                a[2].integer<R::Raw>());
        result = n(event);
      } else if (code == 3) {
        command.array(5);
        session.set_animation_cache_event(
            a[1].integer<A::ClipId>(), a[2].integer<std::size_t>(),
            {a[3].integer<R::Raw>(), a[4].integer<std::uint8_t>()});
      } else if (code == 6) {
        command.array(3);
        result = decoded(session.decode_animation_cache(
            a[1].integer<S::HpId>(), a[2].integer<A::TrackId>()));
      } else if (code == 4 || code == 7) {
        command.array(2);
        auto id = a[1].integer<unsigned>();
        if (id < 1 || id > 2)
          throw std::runtime_error("unknown destroy ID");
        session.with_world([&](O::World &w) {
          w.destroy(*w.objects.at(addresses[id - 1]), 3);
          if (code == 7) {
            auto *o = w.new_object(0x400, {});
            o->flags = 32;
            o->tag = 0;
          }
        });
        continue;
      } else if (code == 5) {
        command.array(1);
        throw TaskFailure();
      } else
        throw std::runtime_error("unknown task opcode");
      effects.push_back(sequence({text("command"), command.canonical(), result,
                                  n(session.snapshot().world->frame)}));
      if (code == 2 &&
          (static_cast<std::uint32_t>(event) & a[3].integer<std::uint32_t>()))
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
    const auto &l = a[2].array(4);
    S::AnimationCacheLimits limits;
    limits.runtime = RT::limits(l[1]);
    limits.calls_per_tick = l[2].integer<std::size_t>();
    limits.history = l[3].integer<std::size_t>();
    try {
      session.enable_animation_cache(l[0].integer<std::size_t>(), limits);
      for (const auto &row : a[3].bounded_array(64)) {
        const auto &c = row.array(2);
        session.add_animation_cache_clip(RT::D::input_bytes(c[1], 16384),
                                         RT::D::input_name(c[0]));
      }
      for (const auto &row : a[4].bounded_array(16)) {
        const auto &r = row.array(2);
        session.bind_animation_cache(r[0].integer<S::HpId>(),
                                     r[1].integer<std::int32_t>());
      }
    } catch (const S::Error &) {
    } catch (const A::ValueError &) {
    } catch (const A::HostFault &) {
    } catch (const A::ResourceLimit &) {
    }
  }
  std::string run() {
    const auto retained = session.snapshot();
    const auto initial = observation(retained);
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
      } catch (const A::ResourceLimit &) {
        result = text("Refusal");
      } catch (const A::HostFault &) {
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
    if (observation(retained) != initial)
      throw std::runtime_error("snapshot mutated after live work");
    return sequence(
        {input.canonical(), initial, sequence(steps), sequence(effects)});
  }
};
inline std::string execute(unsigned config, const std::string &source) {
  Replay replay(config, D::Parser(source).parse());
  return replay.run();
}
} // namespace champions::test::session_anim_cache_trace
