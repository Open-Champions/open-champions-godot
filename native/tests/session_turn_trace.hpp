#pragma once
// Complete owned Session turning observations and explicit input-driven tasks.
// Expected TSV fields never select a command or supply a native state value.
#include "animation_clock_trace.hpp"
#include "session_creature_trace.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace champions::test::session_turn_trace {
using namespace session_trace;
namespace D = animation_clock_trace;
inline std::string turning(const S::TurningSnapshot &t) {
  std::vector<std::string> bindings, callbacks, external, writes, calls, events,
      operations;
  for (const auto &b : t.bindings)
    bindings.push_back(sequence(
        {sequence({n(b.binding.id), n(b.binding.world_serial),
                   n(b.binding.address), n(b.binding.adapter_generation)}),
         sequence({n(b.initial.rate), n(b.initial.angular_velocity),
                   n(b.initial.slow)}),
         n(b.calls_this_tick), n(b.turned_this_tick),
         optional_number(b.last_result)}));
  for (const auto &c : t.callbacks)
    callbacks.push_back(sequence({text(c.kind), text(c.name), n(c.present)}));
  for (const auto &x : t.external)
    external.push_back(text(x));
  for (const auto &w : t.writes)
    writes.push_back(write(w));
  for (const auto &c : t.calls)
    calls.push_back(creature_trace::movement_call(c));
  for (const auto &e : t.events)
    events.push_back(sequence({n(unsigned(e.kind)), write(e.write),
                               creature_trace::movement_call(e.call)}));
  for (const auto &o : t.operations)
    operations.push_back(
        sequence({text(o.kind), n(o.binding), n(o.mode), integers(o.point),
                  n(o.frame), optional_number(o.result), n(o.completed)}));
  return sequence({sequence({n(t.limits.bindings), n(t.limits.calls_per_tick)}),
                   sequence(bindings),
                   text(t.policy),
                   text(t.phase),
                   text(t.publication_error),
                   text(t.memory_owner),
                   n(t.depth),
                   n(t.calls_this_tick),
                   memory_limits(t.model_limits),
                   n(unsigned(t.callback_policy)),
                   n(t.borrowed_memory),
                   sequence(callbacks),
                   sequence(external),
                   sequence(writes),
                   sequence(calls),
                   sequence(events),
                   n(t.draws),
                   sequence(operations),
                   n(t.object_run_completed),
                   optional_number(t.task_priority)});
}
inline std::string observation(const S::Snapshot &s) {
  if (s.creature_movement || s.animation)
    throw std::runtime_error("session turn trace cannot omit active movement "
                             "or animation extension");
  auto base = s;
  base.turning.reset();
  return sequence(
      {snapshot_base_fields(base), s.turning ? turning(*s.turning) : "_"});
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
inline R::creature_movement::Vec3 vector3(const D::Value &v) {
  const auto &a = v.array(3);
  return {a[0].integer<R::Raw>(), a[1].integer<R::Raw>(),
          a[2].integer<R::Raw>()};
}
inline N::ByteVector byte_list(const D::Value &v, std::size_t maximum) {
  N::ByteVector bytes;
  for (const auto &x : v.bounded_array(maximum))
    bytes.push_back(x.integer<std::uint8_t>());
  return bytes;
}
class Replay {
  D::Value input;
  S::Session session;
  std::array<std::uint32_t, 2> addresses{};
  std::array<std::uint64_t, 2> serials{};
  // Borrowed actor identities remain owned by this Replay's Session, which
  // outlives all task callbacks; retired generations must not redirect by
  // address.
  std::array<O::GameObject *, 2> retained{};
  std::vector<std::string> effects;
  std::size_t ready = 0;
  static S::Config config(unsigned cfg, const D::Value &value) {
    const auto &a = value.array(8);
    const auto &net = a[0].array(5);
    if (cfg > 15)
      throw std::runtime_error("DSL configuration");
    S::Config c;
    c.model = static_cast<R::Fpu::Model>(cfg / 4);
    c.mul_bias = (cfg & 2) != 0;
    c.div_nearest = (cfg & 1) != 0;
    c.peers = net[0].integer<std::int64_t>();
    c.local = net[1].integer<std::int64_t>();
    c.network_mode = 1;
    c.seed = net[3].integer<std::uint32_t>();
    c.network_seed = net[4].integer<std::uint32_t>();
    return c;
  }
  std::size_t actor(const D::Value &id) const {
    auto i = id.integer<unsigned>();
    if (i < 1 || i > addresses.size())
      throw std::runtime_error("unknown authored actor ID");
    return i - 1;
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
    if (index == 0)
      ++ready;
    const auto &programs = input.items[6].bounded_array(32);
    if (ready == 0 || ready > programs.size())
      return;
    const auto &phases = programs.at(ready - 1).array(3);
    for (const auto &command : phases[index].bounded_array(64)) {
      const auto &a = command.bounded_array(5);
      if (a.empty())
        throw std::runtime_error("empty task command");
      const auto code = a[0].integer<unsigned>();
      std::optional<std::int16_t> result;
      switch (code) {
      case 0:
        command.array(4);
        result =
            session.turn_toward(a[1].integer<S::HpId>(),
                                a[2].integer<std::int32_t>(), vector3(a[3]));
        break;
      case 1:
        command.array(4);
        session.take_damage(a[1].integer<S::HpId>(), a[2].integer<R::Raw>(), {},
                            a[3].integer<std::uint32_t>());
        break;
      case 2: {
        command.array(4);
        auto i = actor(a[1]);
        auto angle = a[2].integer<std::uint32_t>();
        auto pos = vector3(a[3]);
        session.with_world([&](O::World &) {
          auto *o = retained[i];
          o->angle = angle;
          o->pos = pos;
        });
        break;
      }
      case 3: {
        command.array(2);
        auto i = actor(a[1]);
        session.with_world([&](O::World &w) { w.destroy(*retained[i], 3); });
        break;
      }
      case 4: {
        command.array(2);
        auto pos = vector3(a[1]);
        session.with_world([&](O::World &w) {
          auto *o = w.new_object(0x400, pos);
          o->flags = 32;
          o->tag = 0;
        });
        break;
      }
      case 5:
        command.array(1);
        throw TaskFailure();
      default:
        throw std::runtime_error("unknown task opcode");
      }
      effects.push_back(sequence({text("command"), command.canonical(),
                                  optional_number(result),
                                  n(session.snapshot().world->frame)}));
    }
  }

public:
  Replay(unsigned cfg, D::Value value)
      : input(std::move(value)), session(config(cfg, input)) {
    const auto &a = input.items;
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
    session.enable_objects(a[0].items[2].integer<std::uint32_t>(), b);
    for (unsigned i = 0; i < 3; ++i)
      session.register_object_task(
          0xf1000001 + i,
          std::array<std::string, 3>{"hp_pre", "hp_apply", "hp_post"}[i],
          20 + 10 * i, 0, [this, i](S::Session &) { task(i); });
    const auto &actors = a[2].array(2);
    session.with_world([&](O::World &w) {
      for (unsigned i = 0; i < 2; ++i) {
        const auto &row = actors[i].array(7);
        auto *o = w.new_object(0x400, vector3(row[0]));
        o->flags = row[1].integer<std::uint32_t>();
        o->angle = row[2].integer<std::uint32_t>();
        o->tag = 0;
        addresses[i] = o->id;
        serials[i] = o->serial;
        retained[i] = o;
      }
    });
    session.with_world([&](O::World &w) {
      for (unsigned i = 0; i < 2; ++i)
        if (boolean(actors[i].items[6]))
          w.run_every_frame(*w.objects.at(addresses[i]));
    });
    session.enable_hp();
    for (unsigned i = 0; i < 2; ++i) {
      const auto &row = actors[i].items;
      S::HpInitial initial;
      initial.hp = row[3].integer<R::Raw>();
      initial.cflags = row[4].integer<std::uint64_t>();
      const auto &status = row[5].array(5);
      for (unsigned k = 0; k < 5; ++k)
        initial.status[k] = status[k].integer<R::Raw>();
      session.bind_hp(serials[i], addresses[i], initial);
    }
    for (unsigned i = 0; i < 2; ++i)
      session.seed_hp_backing(i + 1, {{0xc8, 4, 0},
                                      {0x231, 1, 0},
                                      {0x2dc, 4, 0},
                                      {0x2e0, 2, 0},
                                      {0x304, 4, 0},
                                      {0x3b0, 2, 0}});
    for (const auto &row : a[5].bounded_array(256)) {
      const auto &w = row.array(4);
      session.seed_hp_backing(
          w[0].integer<S::HpId>(),
          {{w[1].integer<std::uint32_t>(), w[2].integer<std::size_t>(),
            w[3].integer<std::uint64_t>()}});
    }
    const auto &l = a[3].array(2);
    S::TurnLimits limits;
    limits.bindings = l[0].integer<std::size_t>();
    limits.calls_per_tick = l[1].integer<std::size_t>();
    session.enable_turning(limits);
    for (const auto &row : a[4].bounded_array(64)) {
      const auto &v = row.array(4);
      session.bind_turner(v[0].integer<S::HpId>(),
                          {v[1].integer<R::Raw>(),
                           v[2].integer<std::uint16_t>(),
                           v[3].integer<std::uint8_t>()});
    }
  }
  std::string run() {
    const auto initial = observation(session.snapshot());
    auto previous = parsed(initial);
    std::vector<std::string> steps;
    for (const auto &entry : input.items[7].bounded_array(32)) {
      const auto &v = entry.bounded_array(10);
      if (v.empty())
        throw std::runtime_error("empty submission action");
      std::string result;
      const auto action = v[0].integer<unsigned>();
      if (action == 0) {
        entry.array(1);
        result = "_";
      } else if (action == 1) {
        entry.array(10);
        try {
          session.set_level_pending(boolean(v[8]));
          std::vector<N::ByteVector> inbox;
          for (const auto &p : v[9].bounded_array(16))
            inbox.push_back(byte_list(p, 2048));
          const auto r = session.submit(
              byte_list(v[1], 8), v[2].integer<std::int64_t>(),
              v[3].integer<std::int64_t>(), v[4].integer<std::int64_t>(),
              boolean(v[5]), boolean(v[6]), v[7].integer<std::int64_t>(),
              inbox);
          result = sequence({sends(r.sent), n(r.held), n(r.steps)});
        } catch (const TaskFailure &) {
          result = text("TaskFailure");
        } catch (const S::Error &) {
          result = text("Refusal");
        }
      } else
        throw std::runtime_error("unknown submission opcode");
      const auto current = parsed(observation(session.snapshot()));
      auto patch = delta(previous, current);
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
  delta_anchors();
  Replay replay(config, D::Parser(source).parse());
  return replay.run();
}
inline const std::vector<std::string> &operations() {
  static const std::vector<std::string> names{
      "ready",         "opposite_mode",   "other_mode",    "slow_one",
      "slow_sixty",    "slow_bypass",     "negative_rate", "raw_rate",
      "velocity_wrap", "world_authority", "health_before", "health_after",
      "two_bindings",  "frozen",          "delayed",       "held",
      "no_ready",      "pending",         "no_sim",        "paused",
      "retired",       "reused",          "twice",         "post_failure"};
  return names;
}
} // namespace champions::test::session_turn_trace
