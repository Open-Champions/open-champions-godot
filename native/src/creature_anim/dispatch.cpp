#include "champions/creature_anim/dispatch.hpp"
namespace champions::simulation::rta::creature_anim {
const DispatchTable &dispatch_table(const std::string &name) {
  const auto &tables = dispatch_tables();
  auto it = tables.find(name);
  if (it == tables.end() ||
      (it->second.group != "root" && it->second.group != "steered"))
    throw Unsupported("partial, no-dispatch or unknown creature class: " +
                      name);
  return it->second;
}
Selection select_dispatch(const std::string &name, Raw state, Choice choice) {
  const auto &states = dispatch_table(name).states;
  auto it = states.find(state);
  if (it == states.end())
    throw Unsupported("unrecorded creature state");
  const auto &c = it->second;
  if (choice.condition) {
    const auto &key = *choice.condition;
    if (choice.random != 0 && (key.returned || key.value != 0))
      throw Unsupported("combined random and condition probes are unknown");
    for (const auto &v : c.conditions)
      if (key.returned == v.condition.returned &&
          key.field == v.condition.field && key.value == v.condition.value)
        return v.selection;
    if (key.returned || key.value != 0)
      throw Unsupported("unrecorded dispatch condition");
  }
  if (!c.residues.empty())
    return c.residues[choice.random % c.residues.size()];
  if (!c.points.empty()) {
    auto p = c.points.find(choice.random);
    if (p == c.points.end())
      throw Unsupported("unrecorded nonperiodic random choice");
    return p->second;
  }
  return c.baseline;
}
void apply_dispatch(DispatchContext &c, const Selection &s) {
  // Validate the entire operation vocabulary and necessary handlers before
  // mutation. Resources are resolved at each original call, not prefetched.
  if (s.ops.size() > 256)
    throw ResourceLimit("dispatch operation budget");
  static const std::map<std::string, std::size_t> arity{
      {"start", 3}, {"named_start", 3}, {"blend", 4},        {"named_blend", 4},
      {"push", 2},  {"push_w", 6},      {"push_chained", 2}, {"stop", 2},
      {"sync", 0},  {"jump_end", 0},    {"snap_end", 0},     {"randomize", 0},
      {"clear", 0}, {"drop", 0}};
  for (const auto &o : s.ops) {
    bool external = o.kind == "vslot" || o.kind == "holder";
    if (external) {
      if (!c.external)
        throw Unsupported("dispatch requires ordered virtual/holder handler");
      if (o.args.size() < 1 || o.args.size() > 2)
        throw ValueError("invalid virtual operation");
    } else {
      auto i = arity.find(o.kind);
      if (i == arity.end())
        throw Unsupported("unimplemented dispatch operation: " + o.kind);
      if (o.args.size() != i->second)
        throw ValueError("invalid animation operation arity");
      if (!o.args.empty() && o.kind.rfind("named_", 0) != 0 && !c.entry)
        throw Unsupported("entry resolver required");
    }
    if (!o.args.empty() && o.args[0] < 0)
      throw Unsupported("dispatch uses an unresolved dynamic resource");
  }
  auto &r = c.runtime;
  auto st = c.state;
  for (const auto &o : s.ops) {
    const auto &a = o.args;
    auto kind = o.kind;
    auto clip = [&] {
      return kind.rfind("named_", 0) == 0 ? ClipId(a.at(0))
                                          : c.entry(Raw(a.at(0)));
    };
    if (kind == "start" || kind == "named_start")
      r.anim_start(a.at(1), st, clip(), a.at(2));
    else if (kind == "blend" || kind == "named_blend")
      r.anim_start_blend_in_out(a.at(1), a.at(2), st, clip(), a.at(3));
    else if (kind == "push")
      r.anim_push(st, clip(), a.at(1));
    else if (kind == "push_w")
      r.anim_push_with_weights(st, clip(), a.at(1), a.at(2), a.at(3), a.at(4),
                               a.at(5));
    else if (kind == "push_chained")
      r.anim_push_chained(st, clip(), a.at(1));
    else if (kind == "stop")
      r.anim_stop(a.at(1), st, clip());
    else if (kind == "sync")
      r.anim_sync_to_sim_frame(st);
    else if (kind == "jump_end")
      r.anim_jump_top_to_end(st);
    else if (kind == "snap_end")
      r.anim_snap_top_to_end(st);
    else if (kind == "randomize")
      r.anim_randomize_top_time(st);
    else if (kind == "clear")
      r.anim_clear(st);
    else if (kind == "drop")
      r.anim_drop_non_persistent(st);
    else if (kind == "vslot" || kind == "holder")
      c.external(o);
    else
      throw Unsupported("unimplemented dispatch operation: " + kind);
  }
}
void install_dispatch_events(DispatchContext &c, const std::string &name,
                             const std::string &variant) {
  const auto &t = dispatch_table(name);
  const auto *events = &t.events;
  if (!variant.empty()) {
    auto it = t.event_variants.find(variant);
    if (it == t.event_variants.end())
      throw Unsupported("unrecorded constructor event variant");
    events = &it->second;
  }
  for (const auto &[entry, slots] : *events) {
    auto id = c.entry(entry);
    if (!id)
      continue; // Reference install_events skips missing clips.
    for (unsigned i = 0; i < 4; ++i)
      c.runtime.clip(id).events[i] = {slots[i].first, slots[i].second};
  }
}
std::vector<DispatchEvent> dispatch_events(const std::string &name, Raw state,
                                           std::int32_t word) {
  const auto &t = dispatch_table(name);
  if (!t.states.count(state))
    throw Unsupported("unrecorded creature event state");
  std::vector<DispatchEvent> out;
  auto it = t.consumers.find(state);
  if (it == t.consumers.end())
    return out;
  for (const auto &[mask, names] : it->second) {
    if (mask != -1 && !(mask == 0 ? word != 0 : (Raw(word) & Raw(mask)) != 0))
      continue;
    for (const auto &n : names) {
      if (n == "000000")
        continue; // Null stub artifact, not an engine service.
      std::string kind = "consumer";
      if (n.rfind("Creature_hit", 0) == 0)
        kind = "hit_test";
      else if (n == "Creature_deadUpdate")
        kind = "corpse";
      else if ((mask == 4 || mask == 8) &&
               (n == "fx_spawn" || n.find("Dust") != std::string::npos))
        kind = "footstep";
      out.push_back({kind, n, state, Raw(word), mask});
    }
  }
  return out;
}
std::vector<Raw> dispatch_transition_probe(const std::string &name, Raw state,
                                           Raw flags) {
  const auto &t = dispatch_table(name);
  if (!t.states.count(state))
    throw Unsupported("unrecorded creature transition state");
  if (flags != 0 && flags != 0xffff)
    throw Unsupported("only zero/all-set transition probes are contracted");
  auto i = t.transitions.find(state);
  if (i == t.transitions.end())
    return {};
  return flags == 0 ? i->second.first : i->second.second;
}
} // namespace champions::simulation::rta::creature_anim
