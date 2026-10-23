#include "champions/campaign_actors.hpp"
#include "champions/item_generation.hpp"
#include <array>
#include <stdexcept>
#include <string>
#include <vector>

namespace R = champions::simulation::rta;
namespace C = R::campaign_actors;
namespace O = R::objects;
namespace I = R::items;
namespace {
void check(bool ok, const char *why) {
  if (!ok) throw std::runtime_error(why);
}
void put(std::vector<std::uint8_t> &b, std::size_t at, std::uint32_t v) {
  for (unsigned i = 0; i != 4; ++i) b.at(at + i) = std::uint8_t(v >> (i * 8));
}
std::vector<std::uint8_t> lump(const std::vector<std::string> &names) {
  std::size_t chars = 0;
  for (const auto &n : names) chars += n.size() + 1;
  const auto table_end = 4 + names.size() * 12;
  std::vector<std::uint8_t> b(table_end + chars + names.size(), 0);
  put(b, 0, std::uint32_t(names.size()));
  std::size_t name_at = table_end, data_at = table_end + chars;
  for (std::size_t i = 0; i < names.size(); ++i) {
    put(b, 4 + i * 12, std::uint32_t(name_at));
    put(b, 8 + i * 12, std::uint32_t(data_at));
    put(b, 12 + i * 12, 1);
    for (char c : names[i]) b[name_at++] = std::uint8_t(c);
    ++name_at;
    b[data_at++] = std::uint8_t(i + 1); // authored synthetic payload
  }
  return b;
}
template <class F> void refuses(F fn, const char *why) {
  bool caught = false;
  try { fn(); } catch (const C::Unsupported &) { caught = true; }
  check(caught, why);
}
std::pair<std::vector<std::string>, std::uint64_t>
seeded_loot(const C::LootArgs &args, std::uint32_t seed) {
  I::Tables tables;
  tables.quality_rows.assign(12, {0x3e800000, 0x3f000000, 0x3e800000});
  auto def = [&](const std::string &name) {
    auto d = std::make_shared<I::Definition>();
    d->name = name;
    d->index = std::int32_t(tables.defs.size());
    d->addr = I::A_DEFS + 64 * std::uint32_t(d->index);
    d->category = 2;
    d->count = 1;
    tables.defs.push_back(d);
  };
  def("arrows"); def("recall");
  tables.arrows = "arrows"; tables.recall = "recall";
  for (int i = 0; i < 12; ++i) {
    auto mana = "p_m" + std::to_string(i);
    auto heal = "p_h" + std::to_string(i);
    def(mana); def(heal);
    tables.mana_potions.push_back(mana);
    tables.heal_potions.push_back(heal);
  }
  I::Game game;
  game.num_players = 1; game.difficulty = 1;
  game.level = R::from_int(22);
  I::Rng rng(seed);
  I::World world;
  I::loot_generate_drops(tables, game, rng, world,
      args.gold_scale, args.gold_chance, args.quality_row, args.item_chance);
  std::vector<std::string> kinds;
  for (const auto &drop : world.drops) kinds.push_back(drop.kind);
  return {kinds, rng.draws};
}
}
int main() {
  // Definition slot order is the round-5 reference model. No retail data.
  const std::vector<std::string> boar{
      "warboar.vif", "warboar.tex", "warboar_b.tex", "atak_mid.anm",
      "atak_left_close.anm", "atak_front.anm", "atak_right_close.anm",
      "backup.anm", "charge.anm", "chargetak.anm", "death1.anm",
      "death2.anm", "hit2front.anm", "hit2left.anm", "hit2right.anm",
      "idle1.anm", "roar.anm", "walk.anm", "walk_trn_lft.anm",
      "walk_trn_ryt.anm"};
  const std::vector<std::string> minotaur{
      "minotaur.vif", "minotaur.tex", "atak_180.anm", "atak_back.anm",
      "atak_f1.anm", "atak_f2.anm", "atak_f3.anm", "atak_left.anm",
      "atak_right.anm", "block.anm", "death.anm", "death2.anm",
      "gore.anm", "hitreaction.anm", "idle1.anm", "idle2.anm",
      "run.anm", "taunt.anm", "taunt2.anm", "walk.anm", "axe.vif",
      "axe.tex", "shield.vif", "shield.tex", "shield2.vif", "shield2.tex"};
  champions::resources::Manager resources;
  auto b = lump(boar), m = lump(minotaur);
  resources.load_lmp("warboar.lmp", {b.data(), b.size()});
  resources.load_lmp("minotaur.lmp", {m.data(), m.size()});
  std::vector<std::uint8_t> balance(2 + 3 * 12, 0);
  put(balance, 0, 3);
  const std::array<std::uint16_t, 3> hashes{{0xfe90, 0x270c, 0xec70}};
  for (std::size_t row = 0; row < hashes.size(); ++row) {
    const auto at = 2 + row * 12;
    balance[at] = std::uint8_t(hashes[row]);
    balance[at + 1] = std::uint8_t(hashes[row] >> 8);
    for (unsigned i = 0; i < 5; ++i) {
      balance[at + 2 + i * 2] = 100;
      balance[at + 3 + i * 2] = 0;
    }
  }
  const auto factors = C::balance_lookup(balance.data(), balance.size(), "WarBoarSmall");
  check(factors[0] == R::mul(R::from_int(100), 0x3c23d70a),
        "round-2 balance percent f32");
  check(C::balance_lookup(balance.data(), balance.size(), "AbsentType") ==
        std::array<R::Raw,5>{R::ONE,R::ONE,R::ONE,R::ONE,R::ONE},
        "Creature_ctor missing balance row defaults to five 1.0 factors");
  refuses([&] { C::balance_lookup(balance.data(), balance.size() - 1, "WarBoar"); },
          "truncated balance refusal");
  O::World world(O::CallbackPolicy::ReferenceBase);
  world.shared.state = 20973;
  std::vector<std::string> calls;
  std::vector<C::LootArgs> drops;
  C::TickInput input{true, false, false, false, false, false, 0};
  C::Services services;
  services.tick_input = [&](std::uint32_t) { return input; };
  services.process_damage = [&](std::uint32_t) { calls.emplace_back("damage"); };
  services.creature_run = [&](std::uint32_t) { calls.emplace_back("run"); };
  services.animation_step = [&](std::uint32_t) { calls.emplace_back("anim"); };
  services.knockback = [&](std::uint32_t) { calls.emplace_back("knockback"); };
  services.pick_target = [&](std::uint32_t) { calls.emplace_back("target"); };
  services.dead_update = [&](std::uint32_t) { calls.emplace_back("dead"); };
  services.hit_all_in_arc = [&](std::uint32_t, std::uint32_t flags) {
    check(flags == 8, "round-2 boar attack flags"); calls.emplace_back("hit");
  };
  services.model_hide_mask = [&](std::uint32_t model, std::uint32_t variant) {
    check(model != 0, "r5 hide mask receives resolved model slot");
    check(variant == 0 || variant == 1, "bounded hide variant");
    calls.emplace_back("mask");
    return UINT64_C(0x1234); // synthetic service result, not a game value
  };
  services.minotaur_variant = [&](std::uint32_t, std::uint32_t) {
    calls.emplace_back("variant"); return 1u; // r5 stub answer
  };
  services.minotaur_base_flags = [](std::uint32_t) {
    return UINT64_C(0x80); // synthetic base Creature_ctor answer
  };
  services.animation_init = [&](std::uint32_t, bool push) {
    calls.emplace_back(push ? "animPush" : "animClear");
  };
  std::vector<C::EventParams> events;
  std::vector<std::array<std::uint32_t, 4>> hands;
  services.set_def_entry_params = [&](std::uint32_t, const C::EventParams &p) {
    events.push_back(p);
  };
  services.creature_init = [&](std::uint32_t) { calls.emplace_back("init"); };
  services.weapon_shield = [&](std::uint32_t holder, std::uint32_t hand,
                               std::uint32_t part, std::uint32_t size) {
    check((hand == 1 && part == 0x15) || (hand == 2 && part == 0x19),
          "r5 Minotaur hands");
    check(size == 0x20, "r5 Minotaur holder size");
    hands.push_back({holder, hand, part, size});
    calls.emplace_back("hand");
  };
  services.minotaur_initial_state = [&](std::uint32_t) { calls.emplace_back("state0"); };
  services.creature_drop_loot = [&](std::uint32_t, const C::LootArgs &a) {
    // Synthetic answer for the rank-0 untinted ordinary call only; the runtime
    // must refuse every other case before reaching it.
    calls.emplace_back("dropLoot");
    calls.emplace_back("generateDrops"); drops.push_back(a);
  };
  services.on_death = [&](std::uint32_t) { calls.emplace_back("onDeath"); };
  services.death_animation = [&](std::uint32_t) { calls.emplace_back("deathAnim"); };
  services.minotaur_death_remainder = [&](std::uint32_t) {
    calls.emplace_back("minoDeathUnknown");
  };
  services.filter_state = [&](std::uint32_t, std::uint32_t requested) {
    check(requested == 6 || requested == 0x14, "r2 state filter");
    calls.emplace_back("filter"); return true;
  };
  services.animation_blend = [&](std::uint32_t, std::uint32_t) {
    calls.emplace_back("blend");
  };
  {
    O::World rejected(O::CallbackPolicy::ReferenceBase);
    rejected.shared.state = 7;
    auto unavailable = services;
    unavailable.model_hide_mask = [](std::uint32_t, std::uint32_t) -> std::uint64_t {
      throw C::Unsupported("model_buildHideMask: result Unknown");
    };
    C::Runtime failed(rejected, resources, unavailable);
    refuses([&] {
      failed.construct(C::Kind::WarBoar, {0, 0, 0}, 0, {}, R::from_int(100));
    }, "unknown model service refusal");
    check(rejected.objects.empty(), "constructor refusal leaves no live World actor");
  }
  {
    O::World rejected(O::CallbackPolicy::ReferenceBase);
    auto unavailable = services;
    unavailable.minotaur_base_flags = {};
    C::Runtime failed(rejected, resources, unavailable);
    refuses([&] { failed.construct(C::Kind::Minotaur, {0, 0, 0}, 0, {},
                                    R::from_int(100)); },
            "missing base creature flags refuse Minotaur construction");
    check(rejected.objects.empty(), "Minotaur base refusal consumes no World block");
  }
  C::Runtime actors(world, resources, services);
  refuses([&] {
    actors.construct(C::Kind::WarBoar, {0, 0, 0}, 0,
        std::make_shared<std::vector<std::string>>(1, "hide=1"),
        R::from_int(100));
  }, "Creature_ctor property effects Unknown refusal");
  auto &large = actors.construct(C::Kind::WarBoar, {0, 0, 0}, 0, {}, R::from_int(100), 0);
  auto &small = actors.construct(C::Kind::WarBoarSmall, {R::from_int(2), 0, 0}, 0, {}, R::from_int(80), 0);
  auto &mino = actors.construct(C::Kind::Minotaur, {R::from_int(4), 0, 0}, 0, {}, R::from_int(200), 0);
  check(dynamic_cast<C::WarBoar *>(&large) &&
        dynamic_cast<C::WarBoarSmall *>(&small) &&
        dynamic_cast<C::Minotaur *>(&mino),
        "native actor class identities");
  check(hands == std::vector<std::array<std::uint32_t, 4>>({
        {mino.object->addr + 0x3c8, 1, 0x15, 0x20},
        {mino.object->addr + 0x3c8, 2, 0x19, 0x20}}),
        "r5 Minotaur holder and ordered hand setup");
  check(large.object->vtable == 0x517cf0 && mino.object->vtable == 0x5165f0,
        "round-7 vtable");
  check(large.object->heads[2].next && large.object->heads[1].next &&
        mino.object->heads[2].next && mino.object->heads[1].next,
        "round-7 same World grid/run links");
  check(large.object->radius == 0x41f00000 && small.object->radius == 0x41f00000,
        "r2 boar radius");
  check(mino.object->radius == 0x41c80000 &&
        mino.hp.mem.load(mino.object->addr + 0x1c, 4) == 0x41c80000 &&
        mino.hp.mem.load(mino.object->addr + 0xb8, 4) == 0 &&
        mino.hp.mem.load(mino.object->addr + 0xb0, 4) == 0x3f999999 &&
        mino.hp.mem.load(mino.object->addr + 0xb4, 1) == 0xb &&
        mino.hp.mem.load(mino.object->addr + 0xf4, 4) == 300 &&
        mino.hp.mem.load(mino.object->addr + 0xa8, 8) == UINT64_C(0x200040080) &&
        mino.hp.mem.load(mino.object->addr + 0x46c, 4) == 0,
        "Minotaur_ctor documented radius, speed, turn, kind, age, flags and charge fields");
  check(large.forward_offset == 0xc1700000 && small.forward_offset == 0xc1200000 &&
        large.scale == 0x3f400000 && small.scale == 0x3f199999 && small.small_marker == 1,
        "round-2 boar variants");
  check(large.object->class_def == small.object->class_def &&
        large.object->class_def->entries.size() == 20 &&
        mino.object->class_def->entries.size() == 26,
        "round-5 definition cache and 46 names");
  for (const auto &entry : large.object->class_def->entries)
    check(entry.second != 0, "round-7 boar named res_find result");
  for (const auto &entry : mino.object->class_def->entries)
    check(entry.second != 0, "round-7 Minotaur named res_find result");
  check(world.objects.at(large.object->addr) == large.object &&
        world.objects.at(mino.object->addr) == mino.object,
        "round-7 one World identity");
  check(large.health() == R::from_int(100) && mino.health() == R::from_int(200),
        "HP offset 0x228");
  check(large.hp.mem.load(large.object->addr + 0x48, 4) == 0x517cf0 &&
        large.hp.mem.load(large.object->addr + 0xe0, 4) == 0xc1700000 &&
        small.hp.mem.load(small.object->addr + 0x3d0, 1) == 1 &&
        mino.hp.mem.load(mino.object->addr + 0x48, 4) == 0x5165f0,
        "r2/r7 actor memory offsets");
  check(events.size() == 22 && events[0].entry == 4 &&
        events[0].masks == std::array<std::uint32_t,4>{1,2,0,0} &&
        events[0].times == std::array<R::Raw,4>{0x3e999999,0x3f333333,0,0} &&
        events[4].entry == 10 && events[4].times[1] == 0x3f666666 &&
        events[6].entry == 12 && events[6].masks[0] == 0x84 &&
        events[14].entry == 5 && events[15].entry == 6 &&
        events[15].times[1] == 0x3f199999 &&
        events[21].entry == 4 && events[21].times[0] == 0x3f199999,
        "r5 entry event arguments and order");
  check(mino.hidden_parts == 0x1234, "Unknown mask comes only from service");
  // First constructor is linked into run60 slot 1. The scheduler invokes
  // the same address held by the grid and HP storage.
  world.frame = 1;
  calls.clear(); world.run_objects();
  check(calls == std::vector<std::string>({"damage", "run", "anim", "knockback", "target"}),
        "round-2 bounded tick order");
  // WarBoar_run state 0 (docs/monsters-2.md "0 idle"): clock > +0xf4, which is
  // entry age + 180 (fixed), leaves for state 1. The entry age is a caller input.
  input.state_entry_frame.reset();
  refuses([&] { actors.tick(large.object->addr); },
          "state0 requires the entry age behind +0xf4");
  for (const std::int64_t entry : {0, 100}) {
    input.state_entry_frame = entry;
    world.frame = entry + 180;
    actors.tick(large.object->addr);
    check(large.state == 0, "state0 stays through the 180th frame after entry");
    world.frame = entry + 181;
    refuses([&] { actors.tick(large.object->addr); },
            "state0 refuses unported state-1 transition once clock > entry + 180");
    check(large.state == 0, "idle transition refusal preserves state");
  }
  input.state_entry_frame = 0;
  world.frame = 1;
  // Audit VF-2: a host that cannot cite WarBoar's slot-25 `force` argument
  // refuses state 0 instead of passing a guessed value.
  input.sight_refused = true;
  refuses([&] { actors.tick(large.object->addr); }, "state0 refuses an uncited sight call");
  input.sight_refused = false;
  input.has_target = true;
  refuses([&] { actors.tick(large.object->addr); }, "state0 target Unknown refusal");
  large.state = 3; // reference state's explicit test input
  large.hp.mem.store(large.object->addr + 0x232, 2, 3);
  refuses([&] { actors.tick(large.object->addr); }, "state3 Unknown refusal");
  input.has_target = false;
  large.state = 0;
  large.hp.mem.store(large.object->addr + 0x232, 2, 0);
  actors.set_state(large.object->addr, 6);
  check(large.state == 6 && large.hit_frame == 0 && large.last_attack == 6 &&
        large.hp.mem.load(large.object->addr + 0x232, 2) == 6 &&
        large.hp.mem.load(large.object->addr + 0x3c8, 4) == 0 &&
        large.hp.mem.load(large.object->addr + 0x3cc, 4) == 6,
        "r2 attack state initialization");
  calls.clear(); input.hit_event = true;
  actors.tick(large.object->addr);
  check(large.hit_frame == 1 && calls.back() == "hit" &&
        large.hp.mem.load(large.object->addr + 0x3c8, 4) == 1,
        "r2 attack hit event");
  actors.tick(large.object->addr);
  check(large.hit_frame == 1, "one hit per attack state");
  input.hit_event = false;
  const auto hp_before_attacker = large.health();
  const auto hit_angle_before = large.hp.mem.load(large.object->addr + 0x2e0, 2);
  const auto owner_before = large.hp.mem.load(large.object->addr + 0x2e4, 4);
  refuses([&] { actors.take_damage(large.object->addr, small.object->addr, 1,
                                   R::from_int(5)); },
          "nonzero attacker requires shared World HP binding");
  check(large.health() == hp_before_attacker &&
        large.hp.mem.load(large.object->addr + 0x2e0, 2) == hit_angle_before &&
        large.hp.mem.load(large.object->addr + 0x2e4, 4) == owner_before,
        "refusal does not fabricate hit angle or owner attribution");
  actors.take_damage(large.object->addr, 0, 0x42000, R::ZERO);
  check(large.health() == R::from_int(100) && !large.dead(),
        "r7 zero-damage miss leaves HP unchanged");
  actors.take_damage(large.object->addr, 0, 1, R::from_int(101));
  check(large.dead() && large.health() == R::from_int(-1), "Creature_takeDamage HP subtraction");
  // creature-tick spec 12.1 with attacker 0: frame stamp, flag OR, pending
  // damage; the hit angle and owner handle are written only for an attacker.
  check(large.hp.mem.load(large.object->addr + 0x304, 4) == world.frame &&
        (large.hp.mem.load(large.object->addr + 0x2dc, 4) & 1) == 1 &&
        large.hp.mem.load(large.object->addr + 0xc8, 4) == R::from_int(101) &&
        large.hp.mem.load(large.object->addr + 0x2e0, 2) == hit_angle_before &&
        large.hp.mem.load(large.object->addr + 0x2e4, 4) == owner_before,
        "slot-2 attacker-0 effects match spec 12.1");
  const auto before_death_seed = large.object->seed;
  calls.clear();
  actors.tick(large.object->addr);
  check(large.state == 0x14 && large.death_reported && drops.size() == 1,
        "next-tick boar death and one loot call");
  check(large.hp.mem.load(large.object->addr + 0x232, 2) == 0x14,
        "r8 boar death state offset");
  check(calls == std::vector<std::string>({"damage", "run", "filter", "dropLoot",
                                         "generateDrops", "deathAnim", "onDeath"}) &&
        large.object->seed == before_death_seed * 0x19660dU + 0x3c6ef35fU,
        "r2/r8 death order and one own animation draw");
  check(drops[0].gold_chance == 0 && drops[0].z == 0x42100000 &&
        drops[0].gold_scale == 0x40333333 &&
        drops[0].quality_row == 0x40400000, "round-3/8 zero Gold boar call");
  actors.tick(large.object->addr);
  check(drops.size() == 1, "death loot once");
  actors.take_damage(mino.object->addr, 0, 1, R::from_int(201));
  refuses([&] { actors.tick(mino.object->addr); }, "Minotaur run Unknown refusal");
  actors.set_state(mino.object->addr, 0x14);
  check(mino.dead() && mino.state == 0x14 && drops.size() == 2 &&
        drops[1].gold_chance == 0x3f199999 &&
        mino.hp.mem.load(mino.object->addr + 0x232, 2) == 0x14,
        "round-4 Minotaur Gold-capable death");
  check(seeded_loot(drops[0], 20973) ==
            std::make_pair(std::vector<std::string>{"consumable"}, UINT64_C(10)) &&
        seeded_loot(drops[0], 7) ==
            std::make_pair(std::vector<std::string>{}, UINT64_C(7)),
        "r3 boar fresh-seed generator kinds and draws");
  check(seeded_loot(drops[1], 20973).first == std::vector<std::string>{"gold"} &&
        seeded_loot(drops[1], 7).first.empty(),
        "r4 Minotaur fresh-seed Gold request, not credited Gold");
  calls.clear(); actors.tick(small.object->addr);
  check(calls == std::vector<std::string>({"damage", "run", "anim", "knockback", "target"}),
        "WarBoarSmall shares r2 bounded active tick");
  actors.take_damage(small.object->addr, 0, 1, R::from_int(81));
  check(small.health() == R::from_int(-1), "WarBoarSmall slot-2 HP write");
  actors.tick(small.object->addr);
  check(small.dead() && small.death_reported && drops.size() == 3 &&
        drops.back().gold_chance == 0,
        "WarBoarSmall next-tick death uses boar zero-Gold call");
  const auto cached_model = large.object->class_def->entries.front().second;
  world.lump_generation = 2;
  auto &reload = actors.construct(C::Kind::WarBoar,
      {R::from_int(6), 0, 0}, 0, {}, R::from_int(90));
  check(reload.object->class_def == large.object->class_def &&
        reload.object->class_def->generation == 2 &&
        reload.object->class_def->entries.front().second != cached_model,
        "r5 named resource re-resolution on lump generation change");
  // The second deterministic seed is a separate constructor stream control.
  O::World seed7(O::CallbackPolicy::ReferenceBase);
  seed7.shared.state = 7;
  champions::resources::Manager resources7;
  resources7.load_lmp("warboar.lmp", {b.data(), b.size()});
  resources7.load_lmp("minotaur.lmp", {m.data(), m.size()});
  C::Runtime actors7(seed7, resources7, services);
  auto &on7 = actors7.construct(C::Kind::WarBoar, {0, 0, 0}, 0, {}, R::from_int(100));
  check(on7.object->seed != large.object->seed, "seed 7 own stream differs");
  seed7.frame = 7;
  auto &mino7 = actors7.construct(C::Kind::Minotaur,
      {R::from_int(4), 0, 0}, 0, {}, R::from_int(200));
  check(mino7.hp.mem.load(mino7.object->addr + 0xf4, 4) == 307 &&
        mino7.hp.mem.load(mino7.object->addr + 0xe8, 4) ==
            (1u << ((mino7.object->seed >> 16) & 1u)),
        "seed-7 Minotaur frame threshold and own-draw hidden part");
  R::combat::Tables tables;
  R::combat::World combat_world;
  combat_world.num_players = 1;
  combat_world.monster_level = R::from_int(22);
  combat_world.level_name = U"valor";
  auto &balanced = actors7.construct_balanced(C::Kind::WarBoarSmall,
      {R::from_int(10), 0, 0}, 0, {}, balance.data(), balance.size(),
      tables, combat_world, 0);
  check(balanced.stats.hp && balanced.health() == *balanced.stats.hp &&
        balanced.stats.max_hp && balanced.health() == *balanced.stats.max_hp,
        "Creature_setStats uses own draw and balance row");
  check(balanced.hp.mem.load(balanced.object->addr + 0xe4, 2) ==
            std::uint16_t(balanced.stats.armor) &&
        balanced.hp.mem.load(balanced.object->addr + 0xd0, 2) ==
            std::uint16_t(balanced.stats.dmg_low) &&
        balanced.hp.mem.load(balanced.object->addr + 0xd2, 2) ==
            std::uint16_t(balanced.stats.dmg_high) &&
        balanced.hp.mem.load(balanced.object->addr + 0xd4, 2) ==
            std::uint16_t(balanced.stats.attack) &&
        balanced.hp.mem.load(balanced.object->addr + 0xd8, 4) ==
            balanced.stats.exp_value,
        "Creature_setStats documented field offsets");
  // Idle timer under the second reference seed's World.
  input.state_entry_frame = 7;
  seed7.frame = 187;
  actors7.tick(on7.object->addr);
  seed7.frame = 188;
  refuses([&] { actors7.tick(on7.object->addr); },
          "seed-7 state0 refuses after entry + 180");
  seed7.frame = 7;
  // Death loot: only the ordinary rank-0, untinted call is modeled.
  for (const auto rank : {1, 2}) {
    auto &ranked = actors7.construct_balanced(C::Kind::WarBoar,
        {R::from_int(20 + rank), 0, 0}, 0, {}, balance.data(), balance.size(),
        tables, combat_world, rank);
    const auto drop_count = drops.size();
    calls.clear();
    refuses([&] { actors7.set_state(ranked.object->addr, 0x14); },
            "rank 1/2 Creature_dropLoot extra calls refuse");
    check(ranked.state == 0 && !ranked.death_reported && drops.size() == drop_count &&
          calls.empty() && ranked.hp.mem.load(ranked.object->addr + 0x232, 2) == 0,
          "ranked loot refusal leaves state, seed-bound services and loot untouched");
  }
  {
    auto &unknown_rank = actors7.construct(C::Kind::WarBoar,
        {R::from_int(30), 0, 0}, 0, {}, R::from_int(100));
    calls.clear();
    refuses([&] { actors7.set_state(unknown_rank.object->addr, 0x14); },
            "unknown creature_bossRank refuses death loot");
    check(unknown_rank.state == 0 && calls.empty(), "unknown rank leaves actor untouched");
    auto &tinted = actors7.construct(C::Kind::WarBoar,
        {R::from_int(31), 0, 0}, 0, {}, R::from_int(100), 0);
    tinted.hp.mem.store(tinted.object->addr + 0xa0, 1, 6);
    calls.clear();
    refuses([&] { actors7.set_state(tinted.object->addr, 0x14); },
            "tint preset scaling refuses death loot");
    check(tinted.state == 0 && calls.empty(), "tint refusal leaves actor untouched");
    const auto before = drops.size();
    auto &plain = actors7.construct_balanced(C::Kind::WarBoar,
        {R::from_int(32), 0, 0}, 0, {}, balance.data(), balance.size(),
        tables, combat_world, 0);
    actors7.set_state(plain.object->addr, 0x14);
    check(drops.size() == before + 1 &&
          seeded_loot(drops.back(), 7) ==
              std::make_pair(std::vector<std::string>{}, UINT64_C(7)) &&
          seeded_loot(drops.back(), 20973) ==
              std::make_pair(std::vector<std::string>{"consumable"}, UINT64_C(10)),
          "rank-0 balanced boar death makes the one pinned ordinary call");
  }
  // Minotaur_ctor (docs/monsters-2.md 4871-4882) against an independent model,
  // fresh World per reference seed. Shared draw -> object seed (16-bit), the
  // base setStats draw (balanced path only), then the class's own draw.
  const auto lcg = [](std::uint32_t x) { return x * 0x19660dU + 0x3c6ef35fU; };
  for (const std::uint32_t seed : {20973u, 7u})
    for (const bool balanced : {false, true}) {
      O::World fresh(O::CallbackPolicy::ReferenceBase);
      fresh.shared.state = seed;
      fresh.frame = 40 + seed % 11;
      champions::resources::Manager res;
      res.load_lmp("warboar.lmp", {b.data(), b.size()});
      res.load_lmp("minotaur.lmp", {m.data(), m.size()});
      auto svc = services;
      std::vector<std::uint32_t> variant_draws;
      std::vector<std::string> order;
      svc.minotaur_variant = [&](std::uint32_t, std::uint32_t draw) {
        variant_draws.push_back(draw); order.emplace_back("variant"); return draw & 1u;
      };
      svc.weapon_shield = [&](std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t) {
        order.emplace_back("hand");
      };
      svc.minotaur_initial_state = [&](std::uint32_t) { order.emplace_back("state0"); };
      svc.creature_init = [&](std::uint32_t) { order.emplace_back("init"); };
      svc.minotaur_base_flags = [](std::uint32_t) { return UINT64_C(0x1); };
      C::Runtime rt(fresh, res, svc);
      auto &mi = balanced
          ? rt.construct_balanced(C::Kind::Minotaur, {0, 0, 0}, 0, {}, balance.data(),
                                  balance.size(), tables, combat_world, 0)
          : rt.construct(C::Kind::Minotaur, {0, 0, 0}, 0, {}, R::from_int(200), 0);
      auto word = lcg(seed) >> 16;                 // GameObject seed
      if (balanced) word = lcg(word);              // setStats consumes one draw
      word = lcg(word);                            // class draw
      const auto own = word >> 16;
      const auto at = mi.object->addr;
      check(variant_draws == std::vector<std::uint32_t>{own} &&
            mi.object->seed == word,
            "Minotaur ctor makes exactly one own draw and hands it to the hide variant");
      check(mi.object->radius == 0x41c80000 && mi.hp.mem.load(at + 0x1c, 4) == 0x41c80000 &&
            mi.hp.mem.load(at + 0xb8, 4) == 0 &&
            mi.hp.mem.load(at + 0xb0, 4) == 0x3f999999 &&
            mi.hp.mem.load(at + 0xb4, 1) == 0xb &&
            mi.hp.mem.load(at + 0xf4, 4) == std::uint32_t(fresh.frame + 300) &&
            mi.hp.mem.load(at + 0xa8, 8) == (UINT64_C(0x1) | UINT64_C(0x200040000)) &&
            mi.hp.mem.load(at + 0x46c, 4) == 0 &&
            mi.hp.mem.load(at + 0xe8, 4) == (1u << (own & 1u)) &&
            mi.hp.mem.load(at + 0x48, 4) == 0x5165f0,
            "Minotaur_ctor radius 25, speed 0, turn 1.2, kind 0xb, age+300, flags, +0x46c, own-draw word");
      check(order == std::vector<std::string>({"hand", "hand", "variant", "state0", "init"}),
            "Minotaur ctor hand, own draw/hide mask, setState(0), init order");
    }
  return 0;
}
