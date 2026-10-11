#pragma once
// Complete creature extension observations for authored test replays.
#include "session_snapshot_trace.hpp"
namespace champions::test::creature_trace {
using namespace champions::test::session_trace;
namespace M = R::creature_movement;
using V = M::Value;
inline std::string value(const V &v) {
  switch (v.kind) {
  case M::ValueKind::None:
    return "_";
  case M::ValueKind::Integer:
    return n(v.scalar);
  case M::ValueKind::Vector:
    return sequence({text("tuple"), integers(v.vec)});
  case M::ValueKind::Buffer:
    return sequence({text("tuple"), sequence({text("buf"), bytes(v.bytes)})});
  case M::ValueKind::List:
  case M::ValueKind::Tuple: {
    std::vector<std::string> out;
    for (const auto &item : v.items)
      out.push_back(value(item));
    return sequence(
        {text(v.kind == M::ValueKind::List ? "list" : "tuple"), sequence(out)});
  }
  }
  throw std::runtime_error("unknown callback value");
}
inline void value_anchors() {
  const auto tuple = V::tuple({V::integer(1), V::integer(2), V::integer(3)});
  const auto list = V::list(tuple.items);
  if (value(tuple) != value(V::vector({1, 2, 3})) ||
      value(tuple) == value(list) ||
      value(V::list({})) == value(V::tuple({})) ||
      value(V::buffer(std::string("\0\xff", 2))) !=
          sequence({text("tuple"), sequence({text("buf"), "b00ff"})}))
    throw std::runtime_error("callback shape observation anchor failed");
}
inline std::string values(const std::vector<V> &vs) {
  std::vector<std::string> out;
  for (const auto &v : vs)
    out.push_back(value(v));
  return sequence(out);
}

inline std::string movement_call(const M::Call &v) {
  return sequence({text(v.name), values(v.args)});
}
inline std::string movement(const S::CreatureMovementSnapshot &c) {
  std::vector<std::string> callbacks, external, writes, calls, events, results;
  for (const auto &v : c.callbacks)
    callbacks.push_back(sequence({text(v.kind), text(v.name), n(v.present)}));
  for (const auto &v : c.external)
    external.push_back(text(v));
  for (const auto &v : c.writes)
    writes.push_back(write(v));
  for (const auto &v : c.calls)
    calls.push_back(movement_call(v));
  for (const auto &v : c.events)
    events.push_back(
        sequence({n(unsigned(v.kind)), write(v.write), movement_call(v.call)}));
  for (const auto &v : c.callback_results)
    results.push_back(sequence({n(v.call), value(v.value)}));
  auto binding =
      c.binding
          ? sequence({n(c.binding->id), n(c.binding->world_serial),
                      n(c.binding->address), n(c.binding->adapter_generation)})
          : "_";
  const auto &i = c.initial;
  const auto &l = c.limits;
  return sequence({binding,
                   sequence({integers(i.root), n(i.root_yaw), n(i.root_timer),
                             n(i.phase), n(i.inert), n(i.base_table)}),
                   text(c.policy),
                   text(c.phase),
                   text(c.publication_error),
                   text(c.memory_owner),
                   n(c.depth),
                   n(c.calls_this_tick),
                   c.last_velocity ? integers(*c.last_velocity) : "_",
                   c.last_bits ? n(*c.last_bits) : "_",
                   sequence({n(l.memory_bytes), n(l.history_entries),
                             n(l.callback_entries), n(l.depth)}),
                   n(unsigned(c.callback_policy)),
                   n(c.borrowed_memory),
                   sequence(callbacks),
                   sequence(external),
                   sequence(writes),
                   sequence(calls),
                   sequence(events),
                   sequence(results),
                   n(c.draws),
                   n(c.object_run_completed),
                   c.task_priority ? n(*c.task_priority) : "_"});
}
inline std::string observation(const S::Snapshot &s) {
  return sequence({snapshot_base_fields(s),
                   s.creature_movement ? movement(*s.creature_movement) : "_"});
}
} // namespace champions::test::creature_trace
