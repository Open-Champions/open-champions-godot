#include "champions/creature_anim/dispatch.hpp"
#include "champions/own_machines.hpp"
#include "combat_runtime_internal.hpp"
namespace champions::simulation::rta::combat_runtime {
namespace {
struct OwnClass {
  const char *name;
  Raw run, set_state;
};
// Run and setState addresses from the own-state-machine contract.
const OwnClass kOwn[] = {
    {"Critter", 0x255008, 0x255c5c},
    {"Soul", 0x2a9870, 0x2aac38},
    {"NPC", 0x29ea40, 0x29efcc},
    {"Natasla", 0x29dbf0, 0x29dc3c},
    {"Mermaid", 0x285c58, 0x285ddc},
    {"GnomeNavigator", 0x26a1e0, 0x26a3f4},
    {"Skeleton", 0x2a6864, 0x2a7768},
    {"Skelight", 0x2a8b84, 0x2a91b8},
    {"Gnaw", 0x267654, 0x267a0c},
    {"AntQueen", 0x236f78, 0x237918},
};
const OwnClass *find_name(const std::string &name) {
  for (const auto &row : kOwn)
    if (name == row.name)
      return &row;
  return nullptr;
}
const OwnClass *find_run(Raw fn) {
  for (const auto &row : kOwn)
    if (row.run == fn && std::string(row.name) != "Skelight")
      return &row;
  return nullptr;
}
const OwnClass *find_set(Raw fn) {
  for (const auto &row : kOwn)
    if (row.set_state == fn && std::string(row.name) != "Skelight")
      return &row;
  return nullptr;
}
} // namespace
void World::State::apply_own_state(Address a, const char *cls, Raw requested) {
  using namespace creature_anim;
  const auto &table = dispatch_table(cls);
  auto it = table.states.find(requested);
  if (it == table.states.end())
    throw Unsupported("unrecorded creature state");
  const auto &st = it->second;
  std::vector<const ConditionalSelection *> hits;
  for (const auto &cond : st.conditions) {
    if (cond.condition.returned)
      continue;
    if (u(a + cond.condition.field) == cond.condition.value)
      hits.push_back(&cond);
  }
  Selection selected;
  if (hits.size() > 1)
    throw Unsupported("unrecorded combined field dispatch");
  if (hits.size() == 1) {
    Choice choice;
    choice.condition = hits[0]->condition;
    selected = select_dispatch(cls, requested, choice);
  } else {
    if (!st.residues.empty() || !st.points.empty())
      throw Unsupported("clip-choosing draw is not supplied by this run");
    selected = select_dispatch(cls, requested);
  }
  for (const auto &op : selected.ops) {
    if (op.kind != "start" && op.kind != "blend" && op.kind != "vslot" &&
        op.kind != "holder")
      throw Unsupported("combat world has no service for dispatch op: " +
                        op.kind);
    if (!op.args.empty() && op.args[0] < 0)
      throw Unsupported("dispatch uses an unresolved dynamic resource");
  }
  for (const auto &op : selected.ops) {
    if (op.kind == "start")
      start_anim(a, Raw(op.args[0]), Raw(op.args[2]), Raw(op.args[1]), 0);
    else if (op.kind == "blend")
      start_anim(a, Raw(op.args[0]), Raw(op.args[3]), Raw(op.args[1]),
                 Raw(op.args[2]));
    else {
      std::vector<Value> args;
      if (op.args.size() > 1)
        args.push_back(iv(op.args[1]));
      vcall(a, unsigned(op.args[0]), std::move(args));
    }
  }
  w(a + 0x232, 2, selected.stored);
}
void World::State::own_class_run(Address a, const char *cls) {
  auto state = std::uint32_t(h(a + 0x232));
  own_machines::Probe probe;
  probe.clock = u(a + 0x300);
  probe.timer = u(a + 0xf4);
  probe.flight = u(a + 0x3d4);
  probe.queue = b(a + 0x118);
  probe.anim = h(a + 0x11a);
  probe.events = 0;
  probe.cflags = u(a + 0xa8);
  probe.kind = u(a + 0x3d0);
  probe.class_flags = u(a + 0x43c);
  probe.target = u(a + 0x234) != 0;
  probe.owner = u(a + 0x3c8) != 0;
  probe.rooted = h(a + 0x314) != 0;
  probe.ready = h(a + 0x318) == 0;
  probe.stunned = h(a + 0x31c) != 0;
  probe.z = u(a + 0x18);
  probe.stuck = u(a + 0x2c4);
  probe.fid = u(a + 0x410);
  probe.snap = b(a + 0x3d9) != 0;
  probe.idle = u(a + 0x41c) != 0;
  probe.move = u(a + 0x428) != 0;
  probe.fa = u(a + 0x420) != 0;
  probe.fb = u(a + 0x424) != 0;
  probe.flee = u(a + 0x430) != 0;
  probe.frame = u(FRAME);
  probe.serial = u(a + 0x44);
  probe.timer_out = probe.timer;
  probe.flight_out = probe.flight;
  probe.v25_hook = [&] {
    if (std::string(cls) != "Gnaw")
      throw Unsupported(
          "slot 25 force argument is not in this class contract");
    return model.slot25_can_see_target(a, 0) != 0 ? 1 : 0;
  };
  int sights = 0;
  probe.see_hook = [&] {
    if (std::string(cls) != "Gnaw")
      throw Unsupported("sight cone is not in this class contract");
    Raw arc = sights++ == 0 ? 8000 : 0x7fff;
    return model.can_see_target_in(a, s16m(a + 0x38), arc, 0, 0, 0x40c00000,
                                   0) != 0
               ? 1
               : 0;
  };
  probe.roll_hook = []() -> std::uint32_t {
    throw Unsupported("GameObject_rand stream is not in this run");
  };
  probe.side_hook = [&](const std::string &name) {
    auto refuse = [&] {
      throw Unsupported(std::string(cls) +
                        " decision call is not re-executed in the combat "
                        "world: " +
                        name);
    };
    if (name == "away")
      probe.away = model.is_away_from_home(a);
    else if (name == "move_dest" && std::string(cls) == "NPC") {
      auto speed = u(a + 0xb8);
      bool clear = own_machines::root_clears_speed(u(a + 0xbc)) ||
                   own_machines::root_clears_speed(u(a + 0xc0));
      if (clear && !own_machines::speed_word_is_integer(speed))
        throw Unsupported(
            "NPC speed truncation is unmeasured for a non-integer speed");
      if (clear)
        w(a + 0xb8, 0);
      model.move_to_dest(a);
      if (clear)
        w(a + 0xb8, speed);
    } else if (name == "move_dest")
      model.move_to_dest(a);
    else if (name == "turn")
      model.turn_to_target(a);
    else if (name == "move_target") {
      if (!full())
        throw Unsupported("walking chase");
      model.move_to_target(a);
    } else if (name == "unroot")
      probe.rooted = model.is_not_rooted(a) == 0;
    else if (name == "ready")
      probe.ready = model.is_attack_ready(a) != 0;
    else if (name == "unstun")
      probe.stunned = model.is_not_stunned(a) == 0;
    else if (name == "move_by")
      move(a, {0, 0, 0});
    else if (name == "move_to" && std::string(cls) == "AntQueen")
      probe.arrived = model.move_to(a, vec(a + 0x104));
    else if (name == "delete")
      erase_later(a);
    else if (name == "fx")
      note("fx_spawn");
    else if (name == "stop")
      note("obj_stopRunning");
    else if (name == "dead")
      throw Unsupported(
          "deadUpdate hold and size are not re-executed for this class");
    else
      refuse();
  };
  probe.request_hook = [&](std::uint32_t requested) {
    apply_own_state(a, cls, requested);
  };
  auto tick = own_machines::run(cls, state, probe);
  auto word = tick.words.find(0xf4);
  if (word != tick.words.end())
    w(a + 0xf4, word->second);
  word = tick.words.find(0x3d4);
  if (word != tick.words.end())
    w(a + 0x3d4, word->second);
}
bool World::State::own_run(Address a, Raw fn) {
  auto row = find_run(fn);
  if (!row)
    return false;
  own_class_run(a, row->name);
  return true;
}
Address World::State::spawn_class(const std::string &name, const Spawn &v) {
  if (name == "Skelight")
    return spawn(v, false, 0, 0, 0);
  auto info = find_name(name);
  if (!info)
    throw Unsupported("unimplemented actor run table");
  auto a = spawn(v, false, 0, 0, 0);
  auto it = class_tables.find(name);
  Address table;
  if (it == class_tables.end()) {
    table = alloc_fixture(40 * 8);
    std::map<unsigned, Raw> extra{{4, info->run}, {27, info->set_state}};
    write_table(table, &extra);
    class_tables.emplace(name, table);
  } else
    table = it->second;
  w(a + 0x48, table);
  return a;
}
const char *World::State::own_set_class(Raw fn) const {
  auto row = find_set(fn);
  return row ? row->name : nullptr;
}
} // namespace champions::simulation::rta::combat_runtime
