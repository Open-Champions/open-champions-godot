// Values come from research-spells-r3 cf8320e, docs/spec/player-spells-numeric-r3.md
// ("spec") and tools/difftest_spell_numeric_r3.py ("difftest"). Float literals
// that the note states as expressions were computed with that commit's
// tools/ref_core.py (ps2 model) and are pinned here as bit patterns.
#include "champions/combat_damage.hpp"
#include "champions/monsters_1.hpp"
#include "champions/player.hpp"
#include "champions/spell_numerics.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace champions::simulation::rta;
namespace sn = spell_numeric;
namespace PL = player;
namespace M1 = monsters_1;
using combat::Obj;
using combat::PlayerRec;
using combat::World;

namespace {
int checks = 0;
void check(bool ok, const char *label) {
  ++checks;
  if (!ok)
    throw std::runtime_error(label);
}

constexpr Raw kPlain = 0x42200000;      // 40
constexpr Raw kHalfPlain = 0x41a00000;  // 20
constexpr Raw kCold = 0x41f00000;       // 30
constexpr Raw kHp = 0x42c80000;         // 100
constexpr Raw kAbove = 0x447a0000;      // 1000
constexpr Raw kSixty = 0x42700000;      // 60
constexpr creature_hp::Address kCreature = 0x10000;

void arm(World &world, Obj &attacker) {
  world.numbers_option = 0;
  world.network_mode = 0;
  world.cutscene = 1;
  world.alive.clear();
  attacker.flags = 0x40;
  attacker.slot = 0;
  attacker.tag = U"caster";
  attacker.pos = {ZERO, ZERO, ZERO};
  PlayerRec rec;
  rec.obj = &attacker;
  world.players.clear();
  world.players.push_back(rec);
}

Obj creature(Raw hp) {
  Obj victim;
  victim.tag = U"victim";
  victim.flags = 0x20;
  victim.cls = U"Creature";
  victim.hp = victim.max_hp = hp;
  victim.pos = {ZERO, ZERO, ZERO};
  return victim;
}

void plant(creature_hp::Memory &memory, Raw hp, Raw pending = 0, Raw pending_flags = 0) {
  memory.store(kCreature + 0x228, 4, hp);
  memory.store(kCreature + 0xc8, 4, pending);
  memory.store(kCreature + 0x2dc, 4, pending_flags);
  memory.store(kCreature + 0x232, 2, 0);
}

sn::Delivery hit(World &world, Obj &victim, Obj &attacker, Raw dmin, Raw dmax, std::uint32_t flags,
                std::int64_t no_crit, creature_hp::Memory &memory) {
  return sn::deliver_creature_hit(world, victim, &attacker, dmin, dmax, 100000, flags, nullptr, no_crit,
                                 memory, kCreature, 0);
}

// Live Creature_stun: monsters_1 slot 30 (0x118a58) with the base state body
// 0x3c2688 in slot 27, the configuration the note ran (spec :70-75).
struct Stun {
  M1::Memory mem;
  static constexpr unsigned C = 0x10000, D = 0x20000, T = 0x30000, O = 0x50000;
  Stun(std::uint64_t boss_rank, std::uint32_t state, bool flash) {
    mem.poke(C + 0x24, 4, D);
    mem.poke(C + 0x48, 4, T);
    mem.poke(C + 0x44, 4, 0x12345678);
    mem.poke(O + 12, 4, 0x40);
    mem.poke(T + 30 * 8 + 4, 4, 0x118a58);
    mem.poke(T + 27 * 8 + 4, 4, 0x3c2688);
    mem.poke(C + 0x232, 2, state);
    M1::Model model(mem);
    model.callbacks["creature_bossRank"] = [boss_rank](auto &, const auto &) -> std::uint64_t {
      return boss_rank;
    };
    model.vcall(C, 30, {7u * 60u, flash ? 1u : 0u});
  }
  Raw at(unsigned offset) const { return static_cast<Raw>(mem.load(C + offset, 2)); }
};

// Live Player_castSkill: player::Tick::cast_skill. Layout as the note's rig
// (difftest :310-349): player +0xc 0x40 or 0x20060, recast halfword +0x4ae,
// slot 0 record +0xc0, skill kind +8, flags +4, recast +0xc = 30.
struct Cast {
  static constexpr std::uint32_t P = 0x10000, A = 0x70000, S = 0x71000;
  static constexpr PL::Address R = 0x4d8af0;
  PL::Memory mem;
  std::size_t writes = 0;
  Cast(Raw mana, std::uint16_t recast, bool copy, Raw cost, std::uint32_t cast_return = 0,
       std::uint32_t skill_flags = 0, std::uint32_t kind = 0) {
    mem.poke(A, 4, 0x00007871);
    mem.poke(0x535e30, 4, 0x00007871);
    mem.poke(A + 0x20, 2, 2);
    mem.poke(0x41b170 + 8, 4, S);
    mem.poke(S + 4, 4, skill_flags);
    mem.poke(S + 8, 4, kind);
    mem.poke(S + 0xc, 2, 30);
    mem.poke(P + 0xc, 4, copy ? 0x20060 : 0x40);
    mem.poke(P + 0x4ae, 2, recast);
    mem.poke(P + 0x3c8, 4, 0);
    mem.poke(R + 0xc0, 4, mana);
    PL::Tick tick(mem, P, PL::Tick::Policy::ReferenceZero);
    tick.callbacks[PL::Service::SkillCanUse] = [](PL::Tick &, const auto &) -> PL::Result { return 1u; };
    tick.callbacks[PL::Service::CollectArc] = [](PL::Tick &, const auto &) -> PL::Result {
      return std::vector<std::uint32_t>{};
    };
    tick.callbacks[PL::Service::SkillCast] = [cast_return](PL::Tick &, const auto &) -> PL::Result {
      return cast_return;
    };
    tick.callbacks[PL::Service::SkillCost] = [cost](PL::Tick &, const auto &) -> PL::Result { return cost; };
    tick.cast_skill(P, A);
    writes = tick.writes.size();
  }
  Raw mana() const { return static_cast<Raw>(mem.load(R + 0xc0, 4)); }
  std::uint32_t recast() const { return static_cast<std::uint32_t>(mem.load(P + 0x4ae, 2)); }
};
} // namespace

int main() {
  try {
    ScopedFpu ps2{Fpu{Fpu::Model::Ps2, false, false}};
    Obj attacker;
    {
      World seeded(20973, 20973);
      arm(seeded, attacker);
      int hook_calls = 0;
      seeded.take_damage_fn = [&hook_calls](Obj &, Raw, Obj *, std::uint32_t) { ++hook_calls; };
      seeded.events.emplace_back();
      creature_hp::Memory memory;
      auto victim = creature(kHp);
      plant(memory, kHp);
      const auto flagged = hit(seeded, victim, attacker, kPlain, kPlain, 0x80000, 0, memory);
      check(flagged.handed && flagged.creature_applied && !flagged.player_slot_refused, "flag 0x80000 hands a creature hit");
      // spec :49-51: equal range 40, attack 100000, armour 0 hands 40.
      check(flagged.amount == kPlain, "equal range 40 with attack 100000 hands 40");
      // The caster is a player, so damage_apply adds no 0x1000000 and hands
      // 0x80000 unchanged (ref_combat.py:1154-1155 and :1389; difftest :213).
      check(flagged.handed_flags == 0x80000u, "the handed flag word is 0x80000");
      // spec :43-45 from a zero start (difftest :231-234).
      check(memory.load(kCreature + 0x228, 4) == 0x42700000u, "hit points 100 become 60");
      check(memory.load(kCreature + 0xc8, 4) == kPlain, "pending +0xc8 from 0 is 40");
      check(memory.load(kCreature + 0x2dc, 4) == 0x80000u, "pending flags +0x2dc from 0 are 0x80000");
      check(memory.load(kCreature + 0x232, 2) == 0, "delivery does not enter death");
      check(seeded.events.size() > 1 && seeded.events.front().empty(), "earlier world events are kept");
      seeded.take_damage_fn(victim, ZERO, nullptr, 0);
      check(hook_calls == 1, "the world's take-damage hook is restored after the call");

      // Nonzero start. The note ran a zero start only; creature_hp adds to +0xc8
      // and ORs +0x2dc (creature_hp.cpp take_damage). Not a research-spells-r3 claim.
      creature_hp::Memory busy;
      auto busy_victim = creature(kHp);
      plant(busy, kHp, 0x3f800000, 0x11);
      World busy_world(20973, 20973);
      arm(busy_world, attacker);
      hit(busy_world, busy_victim, attacker, kPlain, kPlain, 0x80000, 0, busy);
      check(busy.load(kCreature + 0xc8, 4) == 0x42240000u, "pending +0xc8 adds 40 to 1 (41)");
      check(busy.load(kCreature + 0x2dc, 4) == 0x80011u, "pending flags +0x2dc OR 0x80000 into 0x11");

      World other(7, 7);
      arm(other, attacker);
      creature_hp::Memory plain_memory;
      auto plain_victim = creature(kHp);
      plant(plain_memory, kHp);
      const auto plain = hit(other, plain_victim, attacker, kPlain, kPlain, 0, 0, plain_memory);
      check(plain.amount == kPlain && plain.handed_flags == 0, "flag 0 on seed 7 hands the same 40");
      check(plain_memory.load(kCreature + 0x2dc, 4) == 0, "flag 0 leaves +0x2dc 0");
    }
    {
      World world(20973, 20973);
      arm(world, attacker);
      creature_hp::Memory memory;
      auto victim = creature(kHp);
      victim.flags = 0x60;
      victim.armor = 3000;
      victim.slot = 0;
      plant(memory, kHp);
      check(combat::flt(sub(kPlain, mul(combat::itof(3000), combat::K_1_30)), mul(kPlain, HALF)),
            "armour 3000 cuts 40 below half");
      const auto armored = hit(world, victim, attacker, kPlain, kPlain, 0, 1, memory);
      check(armored.handed && armored.amount == kHalfPlain, "armour floor hands 20");
      check(armored.player_slot_refused && !armored.creature_applied, "player damage slot is not entered");
      check(memory.load(kCreature + 0x228, 4) == kHp, "player-flagged victim hit points stay put");
    }
    {
      World world(20973, 20973);
      arm(world, attacker);
      creature_hp::Memory memory;
      auto victim = creature(kHp);
      plant(memory, kHp);
      const auto cold = sn::deliver_cold_hit(world, victim, &attacker, 30, 30, memory, kCreature, 0);
      check(cold.handed && cold.amount == kCold && cold.creature_applied, "cold resistance 0 hands 30");
      // spec :56-59: 30 * 60 / 100 = 18.
      check(victim.status.at(1) == 0x41900000u, "cold timer at 100 hit points is 18");
      check(memory.load(kCreature + 0x228, 4) == 0x428c0000u, "hit points 100 become 70");
    }
    {
      World world(7, 7);
      arm(world, attacker);
      creature_hp::Memory memory;
      auto victim = creature(0);
      victim.max_hp = kHp;
      plant(memory, 0);
      const auto cold = sn::deliver_cold_hit(world, victim, &attacker, 30, 30, memory, kCreature, 0);
      check(cold.amount == kCold && victim.status.at(1) == kSixty, "cold timer is 60 at 0 hit points");
    }
    {
      World world(20973, 20973);
      arm(world, attacker);
      creature_hp::Memory memory;
      auto victim = creature(kHp);
      victim.res.at(1) = ONE;
      plant(memory, kHp);
      const auto resisted = sn::deliver_cold_hit(world, victim, &attacker, 30, 30, memory, kCreature, 0);
      check(resisted.handed && resisted.amount == ZERO && victim.status.at(1) == ZERO, "cold resistance 1 hands 0");
      check(memory.load(kCreature + 0x228, 4) == kHp, "a zero cold amount does not move hit points");
    }
    {
      World world(20973, 20973);
      arm(world, attacker);
      creature_hp::Memory memory;
      auto victim = creature(kHp);
      victim.cflags = combat::CF_INVULNERABLE;
      plant(memory, kHp);
      const auto missed = hit(world, victim, attacker, kPlain, kPlain, 0x40, 1, memory);
      check(missed.result == 0 && !missed.handed, "invulnerability returns 0");
      check(memory.load(kCreature + 0x228, 4) == kHp && memory.load(kCreature + 0xc8, 4) == 0,
            "invulnerability does not change hit points");
    }
    {
      World world(7, 7);
      arm(world, attacker);
      creature_hp::Memory memory;
      auto victim = creature(kPlain);
      victim.max_hp = kHp;
      plant(memory, kPlain);
      const auto lethal = hit(world, victim, attacker, kPlain, kPlain, 0, 1, memory);
      check(lethal.amount == kPlain && memory.load(kCreature + 0x228, 4) == ZERO, "hit points equal to the amount become 0");
      check(memory.load(kCreature + 0x232, 2) == 0, "reaching 0 does not run death or knock-back");
    }
    {
      // Live slot 30. Colours: spec :72-74; ref_creature.py:2249-2260;
      // difftest :268-272 (all eight halfwords from +0x90).
      constexpr Raw flash_colours[8] = {0x3a, 0x9b, 0x9a, 0x48, 0x3a, 0x9b, 0x9a, 0};
      Stun lit(0, 0, true);
      check(lit.at(0x31c) == 420u, "stun timer +0x31c is statistic 7 times 60");
      check(lit.at(0x232) == 0x13u, "stun state is 0x13 through 0x3c2688");
      for (unsigned i = 0; i < 8; ++i)
        check(lit.at(0x90 + 2 * i) == flash_colours[i], "flash 1 writes the eight stun halfwords");
      Stun quiet(0, 0, false);
      check(quiet.at(0x31c) == 420u && quiet.at(0x232) == 0x13u, "flash 0 still stores timer and state");
      for (unsigned i = 0; i < 8; ++i)
        check(quiet.at(0x90 + 2 * i) == 0, "flash 0 leaves all eight halfwords 0");
      Stun boss(1, 0, true);
      check(boss.at(0x31c) == 0 && boss.at(0x232) == 0 && boss.at(0x90) == 0, "boss rank 1 stores nothing");
      Stun corpse(0, 0x14, true);
      check(corpse.at(0x31c) == 0 && corpse.at(0x232) == 0x14u && corpse.at(0x90) == 0, "a corpse stores nothing");
    }
    {
      // spec :85 grouping for maximum 100 and percent 10, computed with
      // ref_core.py at cf8320e: 0x3e170a5b; one add onto 50 gives 0x4248970a.
      const auto product = sn::heal_grouped_product(kHp, 10);
      check(product == 0x3e170a5bu, "grouped heal product for 100 and 10 is 0x3e170a5b");
      check(sn::add_heal(0x42480000u, product) == 0x4248970au, "one heal add onto 50 is 0x4248970a");
    }
    {
      // spec :94-108 through the live Tick::cast_skill; the cast slot returns 0.
      constexpr Raw cost = kPlain;
      Cast exact(cost, 0, false, cost);
      check(exact.mana() == ZERO && exact.recast() == 30, "exact mana debits to 0 and recast is 30");
      Cast under(cost - 1, 0, false, cost);
      check(under.mana() == 0xb6800000u && under.recast() == 30, "one pattern below the cost debits below 0");
      Cast above(kAbove, 0, false, cost);
      check(above.mana() == 0x44700000u && above.recast() == 30, "1000 debits to 960");
      Cast blocked(cost, 1, false, cost);
      check(blocked.mana() == cost && blocked.recast() == 1 && blocked.writes == 0, "recast 1 does not cast or debit");
      Cast copy(cost, 0, true, cost);
      check(copy.mana() == cost && copy.recast() == 0 && copy.writes == 0, "the copy with bit 0 clear does not debit");
      check(sn::recast_blocks(1) && sn::recast_blocks(30) && !sn::recast_blocks(0), "recast gate: positive blocks");
      check(sn::copy_blocks(true, 0) && !sn::copy_blocks(false, 0), "copy gate: bit 0 clear blocks the copy");
      check(sn::cast_payment(cost, cost, 0, 0, false, 0, 0).unprobed == sn::kObserved, "the observed case is unflagged");
      // Outside research-spells-r3. Tick::cast_skill still pays here (player-tick
      // contract); the helper names each case instead of claiming it.
      check(sn::cast_payment(cost, cost, 0, 0xffff, false, 0, 0).unprobed == sn::kRecastNegative,
            "recast -1 is outside the note (recast 0 and 1 only)");
      check(sn::cast_payment(cost, cost, 0, 0, true, 1, 0).unprobed == sn::kCopyBit0Set,
            "a copy with skill bit 0 set is outside the note (bit 0 clear only)");
      check(sn::cast_payment(cost, cost, 0, 0, false, 0, 0x73000).unprobed == sn::kNonZeroCast,
            "a debit after a returned spell is outside the note (real cast body Unknown)");
      check(sn::cast_payment(cost, cost, 1, 0, false, 0, 0).unprobed == sn::kKindNotZero,
            "skill kind 1 is outside the note (kind 0 only)");
    }
    std::cout << "spell-numerics: " << checks << " checks PASS\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "spell-numerics: FAIL: " << error.what() << " after " << checks << " checks\n";
    return 1;
  }
}
