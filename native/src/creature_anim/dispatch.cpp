#include "champions/creature_anim/dispatch.hpp"
namespace champions::simulation::rta::creature_anim {
const DispatchTable &dispatch_table(const std::string &name) {
  const auto &tables = dispatch_tables();
  auto it = tables.find(name);
  auto group = it == tables.end() ? std::string() : it->second.group;
  // 'special' is the ten own-machine classes. 'none' still has no clip rows.
  if (it == tables.end() ||
      (group != "root" && group != "steered" && group != "special"))
    throw Unsupported("partial, no-dispatch or unknown creature class: " +
                      name);
  return it->second;
}
Raw agony_hit_entry(Raw flags) {
  // setState 0x234168, request 0x12, word +0x2dc. Any knockback bit wins.
  if (flags & 0x380000)
    return 0xd;
  if (flags & 0x40000)
    return 9;
  return 0xa;
}
std::optional<Raw> clockwork_idle_entry(Raw form) {
  // Field +0x3c8 is a form index compared for equality, not a mask.
  if (form == 0)
    return 0xa;
  if (form >= 1 && form <= 5)
    return 10 * (form + 1);
  return std::nullopt;
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
    // Agony's pending-damage word is a mask. Exact recorded values already
    // returned above. Other bits of this one field use the same rows.
    if (!key.returned && choice.random == 0 && name == "Agony" &&
        state == 0x12 && key.field == 0x2dc) {
      Raw mapped = agony_hit_entry(key.value);
      Raw want = mapped == 0xd ? 0x80000 : mapped == 9 ? 0x40000 : 0;
      if (want) {
        for (const auto &v : c.conditions)
          if (!v.condition.returned && v.condition.field == 0x2dc &&
              v.condition.value == want)
            return v.selection;
        throw Unsupported("Agony hit row missing");
      }
      return c.baseline;
    }
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
      {"push", 2},  {"named_push", 2},  {"push_w", 6},       {"push_chained", 2},
      {"stop", 2},
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
    else if (kind == "push" || kind == "named_push")
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
CombinedDispatch combined_dispatch(const std::string &cls, Raw state,
                                   const std::vector<FieldValue> &fields) {
  // Both fields set. The winner is the single-field result this matches.
  struct Row {
    const char *cls;
    Raw state;
    FieldValue fields[2];
    AnimOp ops[3];
    unsigned nops;
    std::uint32_t stored;
  };
  auto start = [](std::int64_t entry, std::int64_t flags) {
    return AnimOp{"anim_start", {entry, flags}};
  };
  auto chain = [](std::int64_t entry, std::int64_t flags) {
    return AnimOp{"anim_pushChained", {entry, flags}};
  };
  auto sync = [] { return AnimOp{"anim_syncToSimFrame", {}}; };
  auto slot = [](std::int64_t n) { return AnimOp{"vslot", {n}}; };
  const Row rows[] = {
      {"FemaleDarkElf", 0x12, {{0x2dc, 0x80000}, {0x3c8, 1}},
       {start(4, 0x14), chain(5, 0x24), chain(26, 0x2)}, 3, 0x12},
      {"Horsemen", 0x14, {{0x3c8, 1}, {0x470, 3}}, {}, 0, 0},
      {"VampireLord", 3, {{0x3c8, 1}, {0x3cc, 1}}, {start(16, 0x803), sync()}, 2, 3},
      {"VampireLord", 5, {{0x3c8, 1}, {0x3cc, 1}}, {start(7, 3), sync()}, 2, 5},
      {"VampireLord", 0x11, {{0x3c8, 1}, {0x3cc, 1}}, {start(11, 3)}, 1, 0x11},
      {"Clockwork", 0x14, {{0xc, 2}, {0x3c8, 1}}, {start(16, 0x806), slot(28)}, 2,
       0x14},
      {"Clockwork", 0x14, {{0xc, 2}, {0x3c8, 4}}, {start(46, 0x806), slot(28)}, 2,
       0x14},
  };
  for (const auto &row : rows) {
    if (cls != row.cls || state != row.state || fields.size() != 2)
      continue;
    if (fields[0].field != row.fields[0].field ||
        fields[0].value != row.fields[0].value ||
        fields[1].field != row.fields[1].field ||
        fields[1].value != row.fields[1].value)
      continue;
    CombinedDispatch out;
    out.stored = row.stored;
    for (unsigned i = 0; i < row.nops; ++i)
      out.ops.push_back(row.ops[i]);
    return out;
  }
  throw Unsupported("unrecorded combined field dispatch");
}
} // namespace champions::simulation::rta::creature_anim
