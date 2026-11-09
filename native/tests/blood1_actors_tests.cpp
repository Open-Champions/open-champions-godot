#include "champions/blood1_actors.hpp"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace R = champions::simulation::rta;
namespace B = R::blood1_actors;
namespace O = R::objects;
namespace {
void check(bool ok, const char *why) {
  if (!ok) throw std::runtime_error(why);
}
void put32(std::vector<std::uint8_t> &b, std::size_t at, std::uint32_t v) {
  for (unsigned i = 0; i < 4; ++i) b.at(at + i) = std::uint8_t(v >> (8 * i));
}
std::vector<std::uint8_t> lump(const std::vector<std::string> &names) {
  std::size_t chars = 0;
  for (const auto &n : names) chars += n.size() + 1;
  const auto table_end = 4 + names.size() * 12;
  std::vector<std::uint8_t> b(table_end + chars + names.size(), 0);
  put32(b, 0, std::uint32_t(names.size()));
  std::size_t name_at = table_end, data_at = table_end + chars;
  for (std::size_t i = 0; i < names.size(); ++i) {
    put32(b, 4 + i * 12, std::uint32_t(name_at));
    put32(b, 8 + i * 12, std::uint32_t(data_at));
    put32(b, 12 + i * 12, 1);
    for (char c : names[i]) b[name_at++] = std::uint8_t(c);
    ++name_at;
    b[data_at++] = std::uint8_t(i + 1);
  }
  return b;
}
template <typename F> void refuses(F fn, const char *why) {
  bool caught = false;
  try { fn(); } catch (const B::Unsupported &) { caught = true; }
  check(caught, why);
}
const std::vector<std::string> kobold{
    "kobold.vif", "kobold.tex", "attack1.anm", "attack2.anm",
    "attack3.anm", "attack4.anm", "attack5.anm", "death1.anm",
    "death2.anm", "death3.anm", "death4.anm", "evade.anm",
    "evade_lft.anm", "evade_ryt.anm", "hitreact.anm",
    "hitreact2.anm", "knockdown.anm", "idle1.anm", "idle2.anm",
    "run.anm", "spell1.anm", "walk.anm"};
const std::vector<std::string> wraith{
    "warwraith.vif", "warwraith.tex", "ataklong.anm",
    "atakswd1.anm", "atakswd2.anm", "death.anm",
    "deathtouch.anm", "fly.anm", "forward.anm",
    "hitreaction.anm", "idle_axcythe.anm", "idleweap.anm",
    "cythe.vif", "cythe.tex", "dhaxe.vif", "dhaxe.tex",
    "dhsword1.vif", "dhsword1.tex"};
void run_seed(std::uint32_t seed) {
  // Raw outputs are from ref_blood1_enemies.balance_factors, seeds 20973/7.
  std::vector<std::uint8_t> balance(2 + 4 * 12, 0);
  balance[0] = 4;
  const std::array<std::uint16_t, 4> hashes{{0xdc8a,0x97fc,0xa0e1,0xdce0}};
  const std::array<std::array<std::uint16_t, 5>, 4> percentages{{
      {{69,100,60,69,80}}, {{250,80,100,179,189}},
      {{179,179,120,139,189}}, {{200,200,160,100,200}}}};
  const std::array<std::array<R::Raw, 5>, 4> factors{{
      {{0x3f30a3d6,0x3f7fffff,0x3f199999,0x3f30a3d6,0x3f4ccccc}},
      {{0x401fffff,0x3f4ccccc,0x3f7fffff,0x3fe51eb7,0x3ff1eb84}},
      {{0x3fe51eb7,0x3fe51eb7,0x3f999999,0x3fb1eb84,0x3ff1eb84}},
      {{0x3fffffff,0x3fffffff,0x3fcccccc,0x3f7fffff,0x3fffffff}}}};
  const std::array<const char *, 4> balance_names{{
      "Kobold","Koboldtough","koboldshaman","WarWraith"}};
  for (std::size_t row = 0; row < 4; ++row) {
    const auto at = 2 + row * 12;
    balance[at] = std::uint8_t(hashes[row]);
    balance[at + 1] = std::uint8_t(hashes[row] >> 8);
    for (unsigned n = 0; n < 5; ++n) {
      balance[at + 2 + 2*n] = std::uint8_t(percentages[row][n]);
      balance[at + 3 + 2*n] = std::uint8_t(percentages[row][n] >> 8);
    }
    check(B::balance_lookup(balance.data(), balance.size(), balance_names[row]) ==
          factors[row], "reference balance factor raw bits");
  }
  refuses([&] { B::balance_lookup(balance.data(), balance.size() - 1, "Kobold"); },
          "truncated balance refuses");
  champions::resources::Manager resources;
  auto k = lump(kobold), w = lump(wraith);
  resources.load_lmp("kobold.lmp", {k.data(), k.size()});
  resources.load_lmp("wwraith.lmp", {w.data(), w.size()});
  O::World world(O::CallbackPolicy::ReferenceBase);
  world.shared.state = seed;
  std::vector<std::string> calls;
  std::vector<std::uint32_t> events, parts, hits, transitions;
  std::vector<B::LootArgs> drops;
  std::vector<std::array<std::uint64_t, 4>> drop_timers;
  B::Runtime *active_runtime = nullptr;
  bool filter_refuse = false;
  B::TickInput input{true, false, false, false, false, 1000, O::Vec3{0,0,0}};
  std::vector<O::Vec3> grid_positions;
  const auto timers_at = [&](std::uint32_t address) {
    const auto &m = active_runtime->at(address).hp.mem;
    return std::array<std::uint64_t, 4>{{
        m.load(address + 0x31c, 2), m.load(address + 0x318, 2),
        m.load(address + 0x314, 2), m.load(address + 0x31a, 2)}};
  };
  B::Services s;
  s.model_init = [&](std::uint32_t) { calls.emplace_back("model"); };
  s.animation_init = [&](std::uint32_t) { calls.emplace_back("animation"); };
  s.creature_init = [&](std::uint32_t) { calls.emplace_back("init"); };
  s.event_setup = [&](std::uint32_t, std::uint32_t e) { events.push_back(e); };
  s.holder_set_part = [&](std::uint32_t, std::uint32_t part) { parts.push_back(part); };
  s.tick_input = [&](std::uint32_t) { return input; };
  s.process_damage = [&](std::uint32_t) { calls.emplace_back("damage"); };
  s.creature_run = [&](std::uint32_t) { calls.emplace_back("run"); };
  s.animation_step = [&](std::uint32_t) { calls.emplace_back("step"); };
  s.knockback = [&](std::uint32_t) { calls.emplace_back("knockback"); };
  s.pick_target = [&](std::uint32_t) { calls.emplace_back("target"); };
  s.pick_ally = [&](std::uint32_t) { calls.emplace_back("ally"); };
  s.holder_update = [&](std::uint32_t) { calls.emplace_back("holder"); };
  s.holder_descriptor = [&](std::uint32_t) { calls.emplace_back("descriptor"); };
  s.turn_target = [&](std::uint32_t) { calls.emplace_back("turn"); };
  s.grid_update = [&](std::uint32_t address) {
    calls.emplace_back("grid");
    grid_positions.push_back(active_runtime->at(address).object->pos);
  };
  s.settle = [&](std::uint32_t) { calls.emplace_back("settle"); };
  s.can_see_target = [&](std::uint32_t) { calls.emplace_back("see"); };
  s.away_home_probe = [&](std::uint32_t) { calls.emplace_back("home"); };
  s.move_by = [&](std::uint32_t) { calls.emplace_back("moveBy"); };
  s.filter_state = [&](std::uint32_t, std::uint32_t state) {
    check(state == 20, "death filter requested state 20");
    calls.emplace_back("filter");
    return filter_refuse;
  };
  s.state_change = [&](std::uint32_t, std::uint32_t state) {
    calls.emplace_back("setState"); transitions.push_back(state);
  };
  s.hit_target_in_arc = [&](std::uint32_t, std::uint32_t flags) {
    calls.emplace_back("hitArc"); hits.push_back(flags);
  };
  s.hit_target_at_reach = [&](std::uint32_t, std::uint32_t flags) {
    calls.emplace_back("hitReach"); hits.push_back(flags);
  };
  s.dead_update = [&](std::uint32_t) { calls.emplace_back("deadUpdate"); };
  s.delete_object = [&](std::uint32_t) { calls.emplace_back("delete"); };
  s.holder_drop = [&](std::uint32_t) { calls.emplace_back("holderDrop"); };
  s.drop_loot = [&](std::uint32_t address, const B::LootArgs &args) {
    drop_timers.push_back(timers_at(address));
    calls.emplace_back("dropLoot"); drops.push_back(args);
  };
  s.loot_generate = [&](std::uint32_t address, const B::LootArgs &args) {
    drop_timers.push_back(timers_at(address));
    calls.emplace_back("generateDrops"); drops.push_back(args);
  };
  s.death_remainder = [&](std::uint32_t) { calls.emplace_back("deathRemainder"); };
  R::Raw drain_result = 0;
  std::uint32_t cold_value = 77;
  std::vector<std::array<std::uint32_t, 2>> drains;
  s.cold_weapons = [&](std::uint32_t) { calls.emplace_back("coldWeapons"); return cold_value; };
  s.drain_at_reach = [&](std::uint32_t, std::uint32_t flags, std::uint32_t cold) {
    calls.emplace_back("drainReach"); drains.push_back({{flags, cold}});
    return drain_result;
  };
  B::Runtime actors(world, resources, s);
  active_runtime = &actors;
  const std::array<B::Kind, 4> kinds{{B::Kind::Kobold, B::Kind::KoboldTough,
                                      B::Kind::KoboldShaman, B::Kind::WarWraith}};
  const std::array<const char *, 4> names{{"Kobold", "Koboldtough", "koboldshaman", "WarWraith"}};
  const std::array<std::uint32_t, 4> radii{{0x41400000,0x41a00000,0x41900000,0x41900000}};
  const std::array<std::uint32_t, 11> ke{{3,4,5,6,7,21,17,8,9,10,11}};
  const std::array<std::uint32_t, 4> we{{3,4,5,7}};
  for (std::size_t i = 0; i < kinds.size(); ++i) {
    events.clear(); calls.clear();
    auto &a = actors.construct(kinds[i], {R::from_int(std::int32_t(i * 2)),0,0},
                               0, {}, R::from_int(100));
    const auto address = a.object->addr;
    check(a.object->class_name == names[i], "exact class identity");
    check(a.object->vtable == (i == 3 ? 0x517f80u : 0x515ef0u), "reference vtable");
    check(a.object->radius == radii[i], "reference radius");
    check(a.object->heads[2].next && a.object->heads[1].next,
          "one World grid and run identity");
    check(a.object->class_def->entries.size() == (i == 3 ? 18u : 22u),
          "reference resource count");
    for (const auto &entry : a.object->class_def->entries)
      check(entry.second != 0, "named res_find result");
    check(events == (i == 3 ? std::vector<std::uint32_t>(we.begin(), we.end()) :
                            std::vector<std::uint32_t>(ke.begin(), ke.end())),
          "reference event setup order");
    check(calls == std::vector<std::string>({"model","animation","init"}),
          "constructor service order");
    if (i < 3) {
      check(a.variant == i && a.hp.mem.load(address + 0x3cc, 1) == i &&
            a.hp.mem.load(address + 0xe8, 4) == i, "Kobold variant fields");
      check(a.hp.mem.load(address + 0xb4, 1) == 9, "Kobold kind");
    } else {
      check(parts.size() == 1 && parts[0] == a.weapon_part &&
            (a.weapon_part == 13 || a.weapon_part == 15 || a.weapon_part == 17),
            "WarWraith one own draw chooses holder part");
      check(a.hp.mem.load(address + 0x50, 4) == address + 0x3d0 &&
            a.hp.mem.load(address + 0x3d0, 4) == 0x50cf40,
            "embedded Weapon1 identity");
      check(a.hp.mem.load(address + 0x3c8, 1) == 100, "initial fade");
    }
    for (auto state : {0u,3u,6u,7u,20u}) {
      a.state = state;
      world.frame = 0;
      input = {true, false, false, false, false, 1000};
      if (i == 3) {
        a.hp.mem.store(address + 0x3c8, 1, 100);
        a.hp.mem.store(address + 0xa8, 8, 0);
        a.object->flags = 0;
      }
      calls.clear(); transitions.clear();
      actors.tick(address);
      std::vector<std::string> expected = i == 3
          ? state == 20 ? std::vector<std::string>{"damage","run","holder","step","knockback","turn"}
                        : std::vector<std::string>{"damage","run","holder","step","target","knockback","turn"}
          : std::vector<std::string>{"damage","run","step","knockback","target","ally"};
      if (state == 0) {
        expected.emplace_back("see");
        if (i != 3) expected.emplace_back("home");
      } else if (state == 3) {
        expected.emplace_back("setState");
      } else if (state == 6 || state == 7) {
        if (i != 3) expected.emplace_back("settle");
      } else if (i == 3) {
        expected.emplace_back("delete");
      } else {
        expected.emplace_back("moveBy");
        expected.emplace_back("deadUpdate");
      }
      check(calls == expected, "bounded tick call order and completion gate");
      check(transitions == (state == 3 ? std::vector<std::uint32_t>{0} :
                            std::vector<std::uint32_t>{}),
            "state request only on documented branch");
      if (i == 3) {
        const auto expected_fade = state == 3 ? 99u :
                                   state == 6 || state == 7 ? 98u : 100u;
        check(a.hp.mem.load(address + 0x3c8, 1) == expected_fade &&
              (a.hp.mem.load(address + 0xa8, 8) & 0x54) == 0x54 &&
              (a.object->flags & 1) == 1 &&
              (a.hp.mem.load(address + 0xc, 4) & 1) == 1,
              "WarWraith fade, creature flags, and object flag after tick");
      }
    }
    std::ifstream vectors(std::filesystem::path(__FILE__).parent_path() /
                          "vectors/blood1_targeted.tsv");
    check(bool(vectors), "original targeted frame vectors available");
    std::uint32_t vector_seed, kind, hit, complete, fade, height, target_height, result;
    std::string expected_calls;
    std::size_t compared = 0;
    while (vectors >> vector_seed >> kind >> hit >> complete >> fade >> height >>
           target_height >> result >> expected_calls) {
      if (vector_seed != seed || kind != i) continue;
      a.state = 6;
      a.object->pos = {R::from_int(12), R::from_int(24), height};
      a.hp.mem.store(address + 0x18, 4, height);
      a.hp.mem.store(address + 0x3c8, 1, fade);
      input.has_target = true;
      input.hit_event = hit != 0;
      input.animation_over = complete != 0;
      input.target_position = O::Vec3{0, 0, target_height};
      calls.clear(); transitions.clear(); hits.clear(); grid_positions.clear();
      actors.tick(address);
      std::string observed_calls;
      for (const auto &call : calls) {
        if (!observed_calls.empty()) observed_calls += ',';
        observed_calls += call;
      }
      check(observed_calls == expected_calls, "original targeted frame call order");
      check(a.object->pos == O::Vec3{R::from_int(12), R::from_int(24), result},
            "original targeted frame position, including equal height and overshoot");
      check(a.hp.mem.load(address + 0x18, 4) == result,
            "targeted height mirrored in actor memory");
      check(grid_positions == (i == 3 ? std::vector<O::Vec3>{a.object->pos} :
                                        std::vector<O::Vec3>{}),
            "original grid observes updated position exactly once");
      check(transitions == (complete ? std::vector<std::uint32_t>{i == 3 ? 3u : 5u} :
                                       std::vector<std::uint32_t>{}),
            "original targeted completion transition");
      check(hits == (hit ? std::vector<std::uint32_t>{i == 1 ? 0x440008u : 8u} :
                          std::vector<std::uint32_t>{}),
            "original event-only hit dispatch");
      ++compared;
    }
    check(vectors.eof() && compared == 40, "all original targeted vectors compared");
    if (i == 3) {
      input.target_position.reset();
      calls.clear(); grid_positions.clear();
      const auto position = a.object->pos;
      const auto prior_fade = a.hp.mem.load(address + 0x3c8, 1);
      refuses([&] { actors.tick(address); }, "missing target position refuses targeted frame");
      check(calls == std::vector<std::string>{"damage", "run"} &&
            grid_positions.empty() && a.object->pos == position &&
            a.hp.mem.load(address + 0x3c8, 1) == prior_fade,
            "target position refusal precedes class attack effects");
    }
    input.target_position = O::Vec3{0,0,0};
    a.state = 6; input.has_target = true; input.hit_event = true;
    input.animation_over = false;
    if (i == 3) a.hp.mem.store(address + 0x3c8, 1, 100);
    calls.clear(); transitions.clear();
    actors.tick(address);
    check(calls.size() == (i == 3 ? 11u : 9u) &&
          calls.back() == (i == 3 ? "grid" : "turn") && transitions.empty(),
          "running attack dispatches hit without completion transition");
    check(!hits.empty() && hits.back() == (i == 1 ? 0x440008u : 8u),
          "targeted hit dispatch flags, no landed damage claim");
    const auto hit_count = hits.size();
    input.animation_over.reset();
    refuses([&] { actors.tick(address); },
            "missing attack completion answer refuses before hit dispatch");
    check(hits.size() == hit_count, "unknown completion does not dispatch another hit");
    input.animation_over = false;
    if (i == 3) {
      check(a.hp.mem.load(address + 0x3c8, 1) == 98 &&
            (a.hp.mem.load(address + 0xa8, 8) & 0x54) == 0x54 &&
            (a.object->flags & 1) == 1,
            "WarWraith attack fades 100 to 98 after pre-fade flag writes");
      input.has_target = false; input.hit_event = false;
      calls.clear();
      actors.tick(address);
      check(a.hp.mem.load(address + 0x3c8, 1) == 96 &&
            (a.object->flags & 1) == 0 &&
            std::find(calls.begin(), calls.end(), "turn") == calls.end(),
            "WarWraith fade below 99 clears flag and skips prefix turn");
      a.hp.mem.store(address + 0x3c8, 1, 21);
      a.hp.mem.store(address + 0xa8, 8, 0x100);
      actors.tick(address);
      check(a.hp.mem.load(address + 0x3c8, 1) == 19 &&
            (a.hp.mem.load(address + 0xa8, 8) & 0x154) == 0x154,
            "fade threshold tests pre-tick value 21 and preserves other flags");
      actors.tick(address);
      check(a.hp.mem.load(address + 0x3c8, 1) == 17 &&
            (a.hp.mem.load(address + 0xa8, 8) & 0x154) == 0x100,
            "fade below 21 clears only the 0x54 creature flags");
      a.state = 3;
      a.hp.mem.store(address + 0x3c8, 1, 59);
      actors.tick(address);
      check(a.hp.mem.load(address + 0x3c8, 1) == 60,
            "WarWraith chase fade rises toward 60");
      a.hp.mem.store(address + 0x3c8, 1, 61);
      actors.tick(address);
      check(a.hp.mem.load(address + 0x3c8, 1) == 60,
            "WarWraith chase fade falls toward 60");
      a.state = 6;
      a.hp.mem.store(address + 0x3c8, 1, 100);
    }
    input.has_target = true; input.hit_event = true; input.animation_over = true;
    calls.clear(); transitions.clear();
    actors.tick(address);
    check(calls.back() == "setState" &&
          transitions == std::vector<std::uint32_t>{i == 3 ? 3u : 5u},
          "completed targeted attack requests documented next state");
    input.has_target = false; input.hit_event = false;
    if (i == 3) a.hp.mem.store(address + 0x3c8, 1, 100);
    calls.clear(); transitions.clear();
    actors.tick(address);
    check(calls.back() == "setState" &&
          transitions == std::vector<std::uint32_t>{i == 3 ? 3u : 5u},
          "completed no-target attack also requests next state");
    a.state = 7;
    input.has_target = true; input.animation_over = false;
    // research-campaign-combat fbb2e1d supersedes the old "state7 targeted
    // branch remains Unknown" refusal: C:267-268 / MOD:109-117 (Kobold 7) and
    // C:289-292 / MOD:194-204 (WarWraith drain) are Executed in every variant.
    if (i != 3) {
      input.second_event.reset();
      refuses([&] { actors.tick(address); }, "Kobold targeted state 7 needs the event bit 1 answer");
    }
    input.second_event = false;
    calls.clear(); transitions.clear(); hits.clear(); drains.clear();
    actors.tick(address);
    check(calls == (i == 3 ?
          std::vector<std::string>{"damage","run","holder","step","target","knockback","turn","grid"} :
          std::vector<std::string>{"damage","run","step","knockback","target","ally","settle","turn"}) &&
          transitions.empty() && hits.empty() && drains.empty(),
          "targeted state 7 without events turns and keeps the clip");
    input.hit_event = true;
    if (i == 3) {
      drain_result = 0;
      calls.clear(); drains.clear();
      actors.tick(address);
      check(calls == std::vector<std::string>{"damage","run","holder","step","target","knockback",
                                              "coldWeapons","drainReach","turn","grid"} &&
            drains == std::vector<std::array<std::uint32_t, 2>>{{{0x404, 77}}},
            "drain asks ColdWeapons, then hitTargetAtReach(0x404) with it (M2:6162-6165)");
      drain_result = 1;
      calls.clear();
      refuses([&] { actors.tick(address); }, "landed drain refuses: max HP / 15 heal not compared");
      check(calls.back() == "drainReach", "nothing runs after a landed drain");
      drain_result = 0;
      auto no_cold = s;
      no_cold.cold_weapons = {};
      O::World cold_world(O::CallbackPolicy::ReferenceBase);
      B::Runtime cold_runtime(cold_world, resources, no_cold);
      auto &cw = cold_runtime.construct(B::Kind::WarWraith, {0,0,0}, 0, {}, R::from_int(100));
      cw.state = 7;
      calls.clear();
      refuses([&] { cold_runtime.tick(cw.object->addr); }, "missing ColdWeapons answer refuses the drain");
      check(calls == std::vector<std::string>{"damage", "run"}, "drain service refusal precedes effects");
    } else {
      input.second_event = true; input.animation_over = true;
      calls.clear(); hits.clear(); transitions.clear();
      actors.tick(address);
      check(hits == std::vector<std::uint32_t>{0x80001u, 1u} && transitions == std::vector<std::uint32_t>{5} &&
            calls.back() == "setState",
            "Kobold targeted 7: ev&1 flags 0x80001, ev&2 flags 1, clip over -> 5 (C:267-268)");
      input.second_event = false; input.animation_over = false;
    }
    input.has_target = false; input.hit_event = false;
    a.state = 0;
    input.idle_deadline.reset();
    refuses([&] { actors.tick(address); }, "missing idle deadline refuses");
    input.idle_deadline = 10;
    world.frame = 10;
    calls.clear(); transitions.clear();
    actors.tick(address);
    check(transitions.empty(), "idle stays before or at deadline");
    world.frame = 11;
    calls.clear(); transitions.clear();
    actors.tick(address);
    check(transitions == std::vector<std::uint32_t>{1} &&
          calls.back() == "setState", "idle requests wander after deadline");
    input.idle_deadline = 1000;
    world.frame = 0;
    a.hp.mem.store(address + 0x228, 4, R::from_int(100));
    const auto prior_frame = a.hp.mem.load(R::creature_hp::G_SIM_FRAME, 4);
    refuses([&] { actors.take_damage(address, 0x1000, 8, R::from_int(25)); },
            "nonzero attacker remains Unknown");
    check(a.health() == R::from_int(100) &&
          a.hp.mem.load(address + 0x2dc, 4) == 0 &&
          a.hp.mem.load(R::creature_hp::G_SIM_FRAME, 4) == prior_frame,
          "attacker refusal leaves HP, flags and frame unchanged");
    actors.take_damage(address, 0, 8, R::from_int(25));
    check(a.health() == R::from_int(75) &&
          a.hp.mem.load(address + 0xc8, 4) == R::from_int(25) &&
          a.hp.mem.load(address + 0x2dc, 4) == 8,
          "reference base damage probe");
    const std::array<std::uint32_t, 4> timer_offsets{{0x31c,0x318,0x314,0x31a}};
    for (std::size_t n = 0; n < timer_offsets.size(); ++n)
      a.hp.mem.store(address + timer_offsets[n], 2, n + 1);
    const auto before_death_state = a.state;
    const auto before_drops = drops.size();
    filter_refuse = true;
    calls.clear();
    actors.set_state(address, 20);
    check(calls == std::vector<std::string>{"filter"} &&
          a.state == before_death_state && !a.death_reported &&
          drops.size() == before_drops,
          "filter refusal prevents death state and drops");
    for (std::size_t n = 0; n < timer_offsets.size(); ++n)
      check(a.hp.mem.load(address + timer_offsets[n], 2) == n + 1,
            "filter refusal preserves timers");
    filter_refuse = false;
    calls.clear();
    actors.set_state(address, 20);
    check(a.state == 20 && a.hp.mem.load(address + 0x232, 2) == 20 &&
          a.death_reported && drops.size() == i + 1,
          "reference death dispatch");
    check(calls == (i == 3 ?
          std::vector<std::string>{"filter","holderDrop","generateDrops","deathRemainder"} :
          std::vector<std::string>{"filter","dropLoot","deathRemainder"}),
          "reference death call order");
    check(drop_timers.back() == std::array<std::uint64_t, 4>{{0,0,0,0}},
          "all four death timers clear before drop dispatch");
    const auto &d = drops.back();
    check(d.z == 0x42100000 && d.throw_speed == 0x40000000 &&
          d.gold_scale == (i == 3 ? 0x40333333u : 0x3fccccccu) &&
          d.gold_chance == (i == 3 ? 0u : 0x3e999999u) &&
          d.quality_row == (i == 3 ? 0x40400000u : 0x3fb33333u) &&
          d.item_chance == (i == 3 ? 0x3e800000u : 0x3df5c28fu),
          "reference drop call arguments");
    refuses([&] { actors.set_state(address, 8); }, "unprobed state refusal");
    if (i == 3) {
      a.hp.mem.store(address + 0x3c8, 1, 99);
      refuses([&] { actors.tick(address); }, "unprobed Wraith fade path refusal");
    }
  }
  // The reference's weapon probe starts from a fresh World; the four-class
  // loop above consumes shared draws before it reaches WarWraith.
  O::World fresh(O::CallbackPolicy::ReferenceBase);
  fresh.shared.state = seed;
  parts.clear();
  B::Runtime fresh_actors(fresh, resources, s);
  auto &fresh_wraith = fresh_actors.construct(B::Kind::WarWraith,
                                               {0,0,0}, 0, {}, R::from_int(100));
  const auto reference_part = seed == 20973 ? 15u : 13u;
  check(fresh_wraith.weapon_part == reference_part &&
        parts == std::vector<std::uint32_t>{reference_part},
        "fresh-object reference weapon part for seed");
  O::World recycled_world(O::CallbackPolicy::ReferenceBase);
  B::Runtime recycled(recycled_world, resources, s);
  auto &first = recycled.construct(B::Kind::Kobold, {0,0,0}, 0,
                                   {}, R::from_int(100));
  auto *old_object = first.object;
  const auto reused_address = old_object->addr;
  const auto old_serial = old_object->serial;
  recycled_world.destroy(*old_object);
  auto &replacement = recycled.construct(B::Kind::KoboldTough, {0,0,0}, 0,
                                         {}, R::from_int(100));
  check(replacement.object->addr == reused_address &&
        replacement.object != old_object &&
        replacement.object_serial != old_serial &&
        &recycled.at(reused_address) == &replacement,
        "destroy/reconstruct retires stale entry at recycled address");
  input.active = false;
  calls.clear();
  recycled.tick(reused_address);
  check(calls == std::vector<std::string>{"damage","run"},
        "replacement actor ticks safely");
  recycled_world.destroy(*replacement.object);
  refuses([&] { recycled.at(reused_address); },
          "dead replacement identity is not returned");
  auto &callback_actor = recycled.construct(B::Kind::Kobold, {0,0,0}, 0,
                                             {}, R::from_int(100));
  check(callback_actor.object->addr == reused_address &&
        bool(recycled_world.on_destroy),
        "World death dispatch covers recycled actor");
  recycled_world.on_destroy(*callback_actor.object, 3);
  refuses([&] { recycled.at(reused_address); },
          "World death dispatch retires actor identity");
  input.active = true;
  O::World missing_filter_world(O::CallbackPolicy::ReferenceBase);
  auto no_filter = s;
  no_filter.filter_state = {};
  B::Runtime missing_filter(missing_filter_world, resources, no_filter);
  auto &unfiltered = missing_filter.construct(B::Kind::Kobold,
                                               {0,0,0}, 0, {}, R::from_int(100));
  const auto unfiltered_addr = unfiltered.object->addr;
  unfiltered.hp.mem.store(unfiltered_addr + 0x31c, 2, 7);
  refuses([&] { missing_filter.set_state(unfiltered_addr, 20); },
          "missing filter answer refuses death");
  check(unfiltered.state == 0 &&
        unfiltered.hp.mem.load(unfiltered_addr + 0x31c, 2) == 7,
        "missing filter leaves state and timers untouched");
  {
    // research-campaign-combat fbb2e1d MOD:1165: live Kobold setState(8)
    // stores +0x3c8 = 8 (rig in state 3, variant 0, DT:410). The clip start and
    // the rest of setState stay refused; tough/shaman (variants 1/2) never ran
    // setState, and WarWraith has no 8 row (CAI13-3).
    O::World w8(O::CallbackPolicy::ReferenceBase);
    auto s8 = s;
    std::vector<std::uint32_t> filtered;
    bool refuse8 = false;
    s8.filter_state = [&](std::uint32_t, std::uint32_t st) { filtered.push_back(st); return refuse8; };
    B::Runtime r8(w8, resources, s8);
    for (auto kind : kinds) {
      auto &k = r8.construct(kind, {0,0,0}, 0, {}, R::from_int(100));
      const auto at = k.object->addr;
      k.state = 3;
      k.hp.mem.store(at + 0x232, 2, 3);
      k.hp.mem.store(at + 0x234, 4, 0x1234);
      if (kind != B::Kind::WarWraith) k.hp.mem.store(at + 0x3c8, 4, 6);
      filtered.clear();
      refuses([&] { r8.set_state(at, 8); }, "setState(8) refuses its unexecuted remainder or row");
      if (kind == B::Kind::Kobold)
        check(k.state == 8 && k.hp.mem.load(at + 0x232, 2) == 8 && k.hp.mem.load(at + 0x3c8, 4) == 8 &&
              filtered == std::vector<std::uint32_t>{8}, "live Kobold setState(8) stores 8 and +0x3c8 = 8");
      else
        check(k.state == 3 && k.hp.mem.load(at + 0x232, 2) == 3 && filtered.empty(),
              "tough, shaman and WarWraith setState(8) store nothing");
      if (kind != B::Kind::Kobold) continue;
      auto &g = r8.construct(B::Kind::Kobold, {0,0,0}, 0, {}, R::from_int(100));
      const auto ga = g.object->addr;
      g.state = 3; g.hp.mem.store(ga + 0x232, 2, 3);
      refuse8 = true;
      r8.set_state(ga, 8);
      check(g.state == 3 && g.hp.mem.load(ga + 0x232, 2) == 3, "global filter refusal leaves the state");
      refuse8 = false;
      g.hp.mem.store(ga + 0x31c, 2, 9);  // stun: the filter would redirect to 0x13
      refuses([&] { r8.set_state(ga, 8); }, "filter redirect of setState(8) refuses");
      check(g.state == 3 && g.hp.mem.load(ga + 0x3c8, 4) == 0, "redirect stores nothing");
    }
  }
  const auto count = world.objects.size();
  resources.unload("kobold.lmp");
  refuses([&] {
    actors.construct(B::Kind::Kobold, {0,0,0}, 0, {}, R::from_int(100));
  }, "missing named resource refusal");
  check(world.objects.size() == count, "resource refusal leaves no partial World actor");
}
} // namespace

int main() {
  run_seed(20973);
  run_seed(7);
}
