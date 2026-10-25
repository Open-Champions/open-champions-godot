#include "champions/blood1_actors.hpp"
#include <algorithm>
#include <array>
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
  std::vector<std::uint32_t> events, parts, hits;
  std::vector<B::LootArgs> drops;
  B::TickInput input{true, false, false, false, false};
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
  s.grid_update = [&](std::uint32_t) { calls.emplace_back("grid"); };
  s.settle = [&](std::uint32_t) { calls.emplace_back("settle"); };
  s.can_see_target = [&](std::uint32_t) { calls.emplace_back("see"); };
  s.away_home_probe = [&](std::uint32_t) { calls.emplace_back("home"); };
  s.move_by = [&](std::uint32_t) { calls.emplace_back("moveBy"); };
  s.state_change = [&](std::uint32_t) { calls.emplace_back("setState"); };
  s.hit_target_in_arc = [&](std::uint32_t, std::uint32_t flags) {
    calls.emplace_back("hitArc"); hits.push_back(flags);
  };
  s.hit_target_at_reach = [&](std::uint32_t, std::uint32_t flags) {
    calls.emplace_back("hitReach"); hits.push_back(flags);
  };
  s.dead_update = [&](std::uint32_t) { calls.emplace_back("deadUpdate"); };
  s.delete_object = [&](std::uint32_t) { calls.emplace_back("delete"); };
  s.holder_drop = [&](std::uint32_t) { calls.emplace_back("holderDrop"); };
  s.drop_loot = [&](std::uint32_t, const B::LootArgs &args) {
    calls.emplace_back("dropLoot"); drops.push_back(args);
  };
  s.loot_generate = [&](std::uint32_t, const B::LootArgs &args) {
    calls.emplace_back("generateDrops"); drops.push_back(args);
  };
  s.death_remainder = [&](std::uint32_t) { calls.emplace_back("deathRemainder"); };
  B::Runtime actors(world, resources, s);
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
      calls.clear();
      actors.tick(address);
      const std::vector<std::string> prefix = i == 3
          ? state == 20 ? std::vector<std::string>{"damage","run","holder","step","knockback","turn"}
                        : std::vector<std::string>{"damage","run","holder","step","target","knockback","turn"}
          : std::vector<std::string>{"damage","run","step","knockback","target","ally"};
      check(calls.size() == prefix.size() + (state == 0 ? (i == 3 ? 1u : 2u) :
            state == 3 ? 1u : state == 6 || state == 7 ? (i == 3 ? 1u : 2u) :
            i == 3 ? 1u : 2u),
            "bounded tick call count");
      check(std::equal(prefix.begin(), prefix.end(), calls.begin()),
            "reference tick prefix order");
      check(calls.back() == (state == 0 ? (i == 3 ? "see" : "home") : state == 3 || state == 6 || state == 7
            ? "setState" : i == 3 ? "delete" : "deadUpdate"),
            "reference tick tail");
    }
    a.state = 6; input.has_target = true; input.hit_event = true;
    calls.clear();
    actors.tick(address);
    check(calls.size() == (i == 3 ? 12u : 10u) &&
          calls.back() == "setState", "reference targeted dispatch order");
    check(!hits.empty() && hits.back() == (i == 1 ? 0x440008u : 8u),
          "targeted hit dispatch flags, no landed damage claim");
    a.state = 7;
    refuses([&] { actors.tick(address); }, "state7 targeted branch remains Unknown");
    input.has_target = false; input.hit_event = false;
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
    calls.clear();
    actors.set_state(address, 20);
    check(a.state == 20 && a.hp.mem.load(address + 0x232, 2) == 20 &&
          a.death_reported && drops.size() == i + 1,
          "reference death dispatch");
    check(calls == (i == 3 ?
          std::vector<std::string>{"holderDrop","generateDrops","deathRemainder"} :
          std::vector<std::string>{"dropLoot","deathRemainder"}),
          "reference death call order");
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
