// Synthetic checks of the kill rewards over the shared bytes: player_giveExp
// (levelup.md) and the loot_generateDrops roll (items.md section 8). Authored
// fixtures only: the thresholds, class formulas and item tables are invented
// test data, not game values.
#include "champions/campaign_rewards.hpp"
#include "champions/combat.hpp"
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>

namespace R = champions::simulation::rta;
namespace CR = R::campaign_rewards;
namespace items = R::items;
using R::ONE;
using R::Raw;
namespace {
void check(bool ok, const char *why) {
  if (!ok) throw std::runtime_error(why);
}
struct Bytes {
  std::map<R::combat_runtime::Address, std::uint8_t> bytes;
  CR::Memory view() {
    return {[this](R::combat_runtime::Address a, unsigned n) {
              std::uint64_t v = 0;
              for (unsigned i = 0; i < n; ++i) v |= std::uint64_t(bytes[a + i]) << (8 * i);
              return v;
            },
            [this](R::combat_runtime::Address a, unsigned n, std::uint64_t v) {
              for (unsigned i = 0; i < n; ++i) bytes[a + i] = std::uint8_t(v >> (8 * i));
            }};
  }
};
constexpr R::combat_runtime::Address REC = R::combat_runtime::PLAYERS;
R::player_state::Tables test_tables() {
  R::player_state::Tables t;
  // Invented, strictly increasing thresholds; entry 80 is the far sentinel.
  for (std::size_t n = 0; n < 80; ++n) t.experience[n] = std::int32_t(100 * n * (n + 1) / 2 + (n ? 50 : 0));
  t.experience[80] = 1000000;
  for (auto &f : t.formulas) f = {0x41200000, 0x3f800000, 0x3f000000, 0x40a00000, 0x40000000, 0x3f800000};
  return t;
}
Raw raw(float f) {
  Raw r;
  std::memcpy(&r, &f, 4);
  return r;
}
void seed_record(CR::Memory &m, int level, std::int32_t exp, float hp, float mana) {
  m.write(REC + 0x59, 1, std::uint8_t(level));
  m.write(REC + 0x58, 1, std::uint8_t(level));
  m.write(REC + 0x5c, 4, std::uint32_t(exp));
  m.write(REC + 0x50, 4, 6);
  for (unsigned n = 0; n < 4; ++n) m.write(REC + 0x90 + 4 * n, 4, 20);
  m.write(REC + 0xac, 4, raw(hp));
  m.write(REC + 0xc0, 4, raw(mana));
  m.write(REC + 0xe8, 4, 0);
  m.write(REC + 0xec, 1, 0);
}
// Expected maxima, worked by hand from research-combat data-tables.md 4.3
//   maxHp   = level * hpPerLevel + stamina * hpPerStamina - hpOffset
//   maxMana = (intelligence - manaIntOffset) * manaPerInt + level * manaPerLevel
// with test_tables()'s invented constants (offset 10, 1 per int, 0.5 per level,
// 5 per level, 2 per stamina, hp offset 1) and every statistic 20: not computed
// with player_state::maxima, which the code under test calls.
//   level 2: hp 2*5 + 20*2 - 1 = 49, mana (20-10)*1 + 2*0.5 = 11
//   level 8: hp 8*5 + 20*2 - 1 = 79, mana 10 + 8*0.5 = 14
constexpr float HAND_HP_L2 = 49.f, HAND_MANA_L2 = 11.f, HAND_HP_L8 = 79.f, HAND_MANA_L8 = 14.f;

void exp_one_level() {
  Bytes b;
  auto m = b.view();
  const auto t = test_tables();
  seed_record(m, 1, 0, 5.f, 1.f);
  const auto r = CR::give_exp(t, m, 0, t.experience[1] + 3);
  check(r.levels_gained == 1 && r.level_before == 1 && r.level_after == 2, "one level gained");
  check(m.read(REC + 0x59, 1) == 2 && m.read(REC + 0x58, 1) == 1, "earned level up, acknowledged level untouched");
  check(std::int32_t(m.read(REC + 0x5c, 4)) == t.experience[1] + 3, "experience added");
  check(m.read(REC + 0xe8, 4) == 1 && m.read(REC + 0xec, 1) == 3, "1 skill point (level 1 band), 3 stat points");
  check(m.read(REC + 0xb0, 4) == raw(HAND_HP_L2) && m.read(REC + 0xc4, 4) == raw(HAND_MANA_L2),
        "maxima recomputed for level 2 equal the hand-worked 49 and 11");
  check(m.read(REC + 0xac, 4) == raw(HAND_HP_L2) && m.read(REC + 0xc0, 4) == raw(HAND_MANA_L2),
        "positive hit points refill both currents to the hand-worked maxima");
}
void exp_below_threshold() {
  Bytes b;
  auto m = b.view();
  const auto t = test_tables();
  seed_record(m, 1, 0, 5.f, 1.f);
  const auto r = CR::give_exp(t, m, 0, t.experience[1] - 1);
  check(r.levels_gained == 0 && m.read(REC + 0x59, 1) == 1, "below the threshold: no level");
  check(m.read(REC + 0xac, 4) == raw(5.f), "no refill without a level");
  check(m.read(REC + 0xb0, 4) == 0, "maxima untouched without a level");
}
void exp_many_levels() {
  Bytes b;
  auto m = b.view();
  const auto t = test_tables();
  seed_record(m, 4, t.experience[3], 1.f, 1.f);
  // From level 4 to 8: the skill points follow the band of each level left
  // (4, 5 give 1; 6, 7 give 2), the stat points 3 a level.
  const auto r = CR::give_exp(t, m, 0, t.experience[7] - t.experience[3]);
  check(r.levels_gained == 4 && m.read(REC + 0x59, 1) == 8, "four levels in one award");
  check(m.read(REC + 0xe8, 4) == 1 + 1 + 2 + 2, "skill points by the old level band");
  check(m.read(REC + 0xec, 1) == 12, "three stat points a level");
  check(m.read(REC + 0xb0, 4) == raw(HAND_HP_L8) && m.read(REC + 0xc4, 4) == raw(HAND_MANA_L8),
        "maxima are the hand-worked 79 and 14 of the last level");
}
void exp_dead_record_and_ceiling() {
  Bytes b;
  auto m = b.view();
  const auto t = test_tables();
  seed_record(m, 1, 0, 0.f, 1.f);
  CR::give_exp(t, m, 0, t.experience[1]);
  check(m.read(REC + 0xac, 4) == 0, "a record with no hit points is not refilled");
  check(m.read(REC + 0xb0, 4) == raw(HAND_HP_L2), "its maxima still follow the level (hand-worked 49)");
  seed_record(m, 79, 0, 1.f, 1.f);
  const auto r = CR::give_exp(t, m, 0, 0x7fffff00);
  check(std::int32_t(m.read(REC + 0x5c, 4)) == t.experience[80] - 1, "experience clamps at table[80] - 1");
  check(m.read(REC + 0x59, 1) == 80 && r.levels_gained == 1, "level 80 is the last");
  bool refused = false;
  try { CR::give_exp(t, m, 4, 1); } catch (const CR::Unsupported &) { refused = true; }
  check(refused, "a slot outside 0..3 is refused");
}

// ---- loot ---------------------------------------------------------------------------
items::Tables synthetic_tables() {
  items::Tables t;
  auto def = [&](std::string name, int category, int subtype = 0, int slot = 0,
                 std::uint64_t flags = 0, int level = 1, int price = 100,
                 bool preset = false) {
    auto d = std::make_shared<items::Definition>();
    auto &rows = preset ? t.presets : t.defs;
    d->index = static_cast<std::int32_t>(rows.size());
    d->addr = (preset ? items::A_PRESETS : items::A_DEFS) +
              64 * static_cast<std::uint32_t>(d->index);
    d->preset = preset;
    d->name = std::move(name);
    d->level = level;
    d->weight = ONE;
    d->count = 1;
    d->damage_min = 2;
    d->damage_max = 5;
    d->price = price;
    d->armor = 4;
    d->attack = 3;
    d->slot = slot;
    d->category = category;
    d->subtype = subtype;
    d->damage_bits = subtype == 3 ? 8 : 1;
    d->flags = flags;
    d->picture = "ore";
    d->model_row = 0;
    rows.push_back(d);
  };
  const std::array<int, 3> subtypes{6, 3, 0};
  for (unsigned i = 0; i < 3; ++i)
    def("weapon" + std::to_string(i), 0, subtypes[i], 0, (3u << 11) | 2, 1,
        100 + static_cast<int>(i) * 25);
  const std::array<int, 6> slots{11, 4, 9, 12, 3, 0};
  for (unsigned i = 0; i < 6; ++i) {
    def("armor" + std::to_string(i), 1, 0, slots[i], 2u << 11, 1,
        70 + static_cast<int>(i));
    def("cloth" + std::to_string(i), 1, 0, slots[i], (2u << 11) | 0x80000, 1,
        70 + static_cast<int>(i));
  }
  def("circlet_demo", 1, 0, 11, 0, 1, 75);
  def("other", 2, 0, 0, 3u << 11, 1, 55);
  def("ore_low", 2, 0, 0, (2u << 11) | UINT64_C(0x800000000), 1, 40);
  def("ore_high", 2, 0, 0, UINT64_C(0x800000000) | 0x10000, 38, 90);
  def("figure_low", 2, 0, 0, UINT64_C(0x200000000000), 1, 35);
  def("figure_high", 2, 0, 0, UINT64_C(0x200000000000) | (4u << 11), 38, 95);
  for (const auto &group : {"m", "h"})
    for (int i = 0; i < 12; ++i)
      def(std::string("p_") + group + std::to_string(i), 2, 0, 0, 0, 1, 10 + i);
  def("arrows", 0, 6, 0, 0, 1, 2);
  def("recall", 2, 0, 0, 0, 1, 30);
  for (unsigned i = 0; i < 12; ++i)
    def("preset" + std::to_string(i) + "+boost2", static_cast<int>(i % 3), 0, 0,
        (3u << 11) | (i == 4 ? 0x80000 : 0), i < 9 ? 1 : 40,
        100 + static_cast<int>(i / 3) * 20, true);
  const std::array<int, 4> plevels{1, 50, 1, 2}, slevels{1, 40, 2, 1};
  const std::array<unsigned, 4> masks{0, 1, 2, 0}, damage{0, 0, 8, 8};
  for (unsigned i = 0; i < 4; ++i) {
    items::Prefix p;
    p.index = static_cast<int>(i);
    p.key = "pre" + std::to_string(i);
    p.level = plevels[i];
    p.mask = masks[i];
    p.damage_mask = damage[i];
    p.texts.fill("+boost" + std::to_string(i + 1));
    if (i == 2)
      p.texts[1] = std::nullopt;
    t.prefixes.push_back(p);
    items::Suffix s;
    s.index = static_cast<int>(i);
    s.key = "suf" + std::to_string(i);
    s.level = slevels[i];
    s.texts.fill("+worth" + std::to_string(i * 5));
    if (i == 2)
      s.texts[1] = std::nullopt;
    t.suffixes.push_back(s);
  }
  t.formats = {{"boost%d", 0}, {"worth%d", 0x25}, {"guard%d", 0x0d}};
  t.potion_stem = "p_";
  t.thrown_stem = "thrown_";
  t.circlet_stem = "circlet";
  t.circlet_stem_equip = "circlet";
  t.arrows = "arrows";
  t.recall = "recall";
  t.name_key_format = "%s";
  t.name_space = " ";
  t.of_key = "of";
  t.gold_scale_multiplayer = UINT64_C(0x4003333333333333);
  t.weapon_multipliers.assign(8, ONE);
  for (int i = 0; i < 12; ++i) {
    t.quality_rows.push_back(
        i % 2 == 0 ? std::array<Raw, 3>{0x3e800000, 0x3f000000, 0x3e800000}
                   : std::array<Raw, 3>{0x3d800000, 0x3f400000, 0x3e400000});
    t.mana_potions.push_back("p_m" + std::to_string(i));
    t.heal_potions.push_back("p_h" + std::to_string(i));
  }
  t.mana_potions[10] = t.heal_potions[0];
  return t;
}

std::uint32_t lcg(std::uint32_t seed, std::uint64_t draws) {
  while (draws--) seed = seed * 0x19660du + 0x3c6ef35fu;
  return seed;
}
R::combat_runtime::LootDrop boar_like() {
  R::combat_runtime::LootDrop d;
  d.throw_speed = 0x40000000;           // 2.0
  d.z = 0x42100000;                     // 36.0
  d.gold_scale = 0x40333333;            // 2.8
  d.gold_chance = 0;                    // zero gold
  d.quality_row = 0x40333333;           // 2.8
  d.item_chance = 0;
  return d;
}
void set_game(CR::Memory &m, std::uint32_t seed) {
  m.write(R::combat_runtime::NUM_PLAYERS, 4, 1);
  m.write(REC + 0x50, 4, 0);
  m.write(R::combat_runtime::SHARED_SEED, 4, seed);
}
// The level load's values for the test: monster level 22.0, difficulty 1.
constexpr Raw LEVEL_22 = 0x41b00000;
void bind(CR::Loot &loot, Raw level = LEVEL_22, std::int32_t difficulty = 1) { loot.bind_level_globals(level, difficulty); }
void loot_roll() {
  auto tables = std::make_shared<items::Tables>(synthetic_tables());
  bool saw_empty = false, saw_item = false;
  for (std::uint32_t seed = 1; seed < 4000 && !(saw_empty && saw_item); ++seed) {
    Bytes b;
    auto m = b.view();
    set_game(m, seed);
    CR::Loot loot(tables, m);
    bind(loot);
    const auto out = loot.roll(boar_like(), {});
    check(std::uint32_t(m.read(R::combat_runtime::SHARED_SEED, 4)) == lcg(seed, out.shared_draws),
          "the shared seed advanced by exactly the draws taken");
    if (out.items.empty() && out.ending == "no equipment roll") {
      // Nothing succeeded: quality, gold-roll count, one chance draw per gold
      // roll, then consumable, reagent, figurine and the equipment test.
      check(out.shared_draws >= 7 && out.shared_draws <= 9, "7 to 9 draws when nothing succeeds");
      saw_empty = true;
    }
    if (!out.items.empty()) {
      check(out.items.front().kind == "consumable" || out.items.front().kind == "reagent" ||
                out.items.front().kind == "figurine",
            "a zero item chance leaves only consumables, reagents and figurines");
      check(out.items.front().count > 0, "the record carries a stack count");
      // Same seed, same outcome.
      Bytes b2;
      auto m2 = b2.view();
      set_game(m2, seed);
      CR::Loot again(tables, m2);
      bind(again);
      const auto twin = again.roll(boar_like(), {});
      check(twin.items.size() == out.items.size() && twin.items.front().record == out.items.front().record &&
                twin.shared_draws == out.shared_draws,
            "the roll is deterministic in the seed");
      saw_item = true;
    }
  }
  check(saw_empty, "a seed that rolls nothing exists in the first 4000");
  check(saw_item, "a seed that rolls an item exists in the first 4000");
}
void loot_gold_refused() {
  auto tables = std::make_shared<items::Tables>(synthetic_tables());
  Bytes b;
  auto m = b.view();
  set_game(m, 5);
  CR::Loot loot(tables, m);
  bind(loot);
  auto d = boar_like();
  d.gold_chance = ONE; // chance 65536: every gold roll succeeds
  bool refused = false;
  try { loot.roll(d, {}); } catch (const CR::Unsupported &) { refused = true; }
  check(refused, "a Gold roll is refused, not replaced by an invented object");
  check(std::uint32_t(m.read(R::combat_runtime::SHARED_SEED, 4)) != 5, "the draws taken before the refusal are kept");
}
// The level's monster level and the difficulty are not in the World's bytes in
// the session; the roll takes them from the level load or does not happen.
void loot_needs_level_inputs() {
  auto tables = std::make_shared<items::Tables>(synthetic_tables());
  Bytes b;
  auto m = b.view();
  set_game(m, 77);
  // Plant the World's own defaults (a 1.0 level, zero difficulty): they must not be used.
  m.write(0x4f714c, 4, ONE);
  m.write(0x4f719c, 4, 0);
  CR::Loot loot(tables, m);
  bool refused = false;
  try { loot.roll(boar_like(), {}); } catch (const CR::Unsupported &) { refused = true; }
  check(refused, "an unbound level and difficulty refuse the roll");
  check(std::uint32_t(m.read(R::combat_runtime::SHARED_SEED, 4)) == 77, "a refused roll leaves the shared seed alone");
  for (std::int32_t bad : {-1, 0, 6, 99}) {
    bool rejected = false;
    try { loot.bind_level_globals(LEVEL_22, bad); } catch (const CR::Unsupported &) { rejected = true; }
    check(rejected, "a difficulty outside 1..5 is not bound");
  }
  check(!loot.level_globals_bound(), "nothing was bound by the rejected difficulties");
  // The bound inputs are used, not the bytes: the same seed rolls differently
  // for another level or difficulty (the budget and generators depend on them).
  bool level_matters = false, difficulty_matters = false;
  for (std::uint32_t seed = 1; seed < 400 && !(level_matters && difficulty_matters); ++seed) {
    auto roll = [&](Raw level, std::int32_t difficulty) {
      Bytes bytes;
      auto mem = bytes.view();
      set_game(mem, seed);
      mem.write(0x4f714c, 4, ONE);
      mem.write(0x4f719c, 4, 0);
      CR::Loot l(tables, mem);
      bind(l, level, difficulty);
      return l.roll(boar_like(), {});
    };
    const auto base = roll(LEVEL_22, 1);
    const auto other_level = roll(ONE, 1), other_difficulty = roll(LEVEL_22, 5);
    auto differs = [&](const CR::LootOutcome &a, const CR::LootOutcome &c) {
      return a.shared_draws != c.shared_draws || a.items.size() != c.items.size() ||
             (!a.items.empty() && !c.items.empty() && a.items.front().record != c.items.front().record);
    };
    level_matters = level_matters || differs(base, other_level);
    difficulty_matters = difficulty_matters || differs(base, other_difficulty);
  }
  check(level_matters, "the bound monster level changes the roll");
  check(difficulty_matters, "the bound difficulty changes the roll");
}
void loot_needs_tables() {
  bool refused = false;
  try { CR::Loot loot(nullptr, {}); } catch (const CR::Unsupported &) { refused = true; }
  check(refused, "no tables, no roll");
}
} // namespace
int main() {
  try {
    exp_one_level();
    exp_below_threshold();
    exp_many_levels();
    exp_dead_record_and_ceiling();
    loot_roll();
    loot_gold_refused();
    loot_needs_level_inputs();
    loot_needs_tables();
  } catch (const std::exception &error) {
    std::cerr << "campaign rewards test failed: " << error.what() << "\n";
    return 1;
  }
  std::cout << "campaign rewards tests passed\n";
  return 0;
}
