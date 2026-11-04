#include "champions/combat_damage.hpp"
#include "champions/spell_numerics.hpp"
#include <iostream>
#include <stdexcept>

using namespace champions::simulation::rta;
namespace sn = spell_numeric;
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

void plant(creature_hp::Memory &memory, Raw hp) {
  memory.store(kCreature + 0x228, 4, hp);
  memory.store(kCreature + 0xc8, 4, 0);
  memory.store(kCreature + 0x2dc, 4, 0);
  memory.store(kCreature + 0x232, 2, 0);
}

sn::Delivery hit(World &world, Obj &victim, Obj &attacker, Raw dmin, Raw dmax, std::uint32_t flags,
                std::int64_t no_crit, creature_hp::Memory &memory) {
  return sn::deliver_creature_hit(world, victim, &attacker, dmin, dmax, 100000, flags, nullptr, no_crit,
                                 memory, kCreature, 0);
}
} // namespace

int main() {
  try {
    ScopedFpu ps2{Fpu{Fpu::Model::Ps2, false, false}};
    Obj attacker;
    {
      World seeded(20973, 20973);
      arm(seeded, attacker);
      creature_hp::Memory memory;
      auto victim = creature(kHp);
      plant(memory, kHp);
      const auto flagged = hit(seeded, victim, attacker, kPlain, kPlain, 0x80000, 0, memory);
      check(flagged.handed && flagged.creature_applied && !flagged.player_slot_refused, "flag 0x80000 hands a creature hit");
      check(flagged.amount == kPlain, "equal range 40 with attack 100000");
      check(memory.load(kCreature + 0x228, 4) == sub(kHp, kPlain), "hit points lose the handed amount");
      check(memory.load(kCreature + 0xc8, 4) == kPlain, "pending +0xc8 stores the amount");
      check(memory.load(kCreature + 0x2dc, 4) == flagged.handed_flags, "pending flags +0x2dc store the flag word");
      check(memory.load(kCreature + 0x232, 2) == 0, "delivery does not enter death");

      World other(7, 7);
      arm(other, attacker);
      creature_hp::Memory plain_memory;
      auto plain_victim = creature(kHp);
      plant(plain_memory, kHp);
      const auto plain = hit(other, plain_victim, attacker, kPlain, kPlain, 0, 0, plain_memory);
      check(plain.amount == flagged.amount, "flag 0 hands the same amount");
      check(plain.amount == kPlain, "the amount is stable across seeds 20973 and 7");
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
      const auto before = memory.load(kCreature + 0x228, 4);
      check(combat::flt(sub(kPlain, mul(combat::itof(3000), combat::K_1_30)), mul(kPlain, HALF)),
            "armour 3000 cuts 40 below half");
      const auto armored = hit(world, victim, attacker, kPlain, kPlain, 0, 1, memory);
      check(armored.handed && armored.amount == kHalfPlain, "armour floor hands 20");
      check(armored.player_slot_refused && !armored.creature_applied, "player damage slot is not entered");
      check(memory.load(kCreature + 0x228, 4) == before, "player-flagged victim hit points stay put");
    }
    {
      World world(20973, 20973);
      arm(world, attacker);
      creature_hp::Memory memory;
      auto victim = creature(kHp);
      plant(memory, kHp);
      const auto cold = sn::deliver_cold_hit(world, victim, &attacker, 30, 30, memory, kCreature, 0);
      const auto timer = div(mul(kCold, combat::SIXTY), kHp);
      check(cold.handed && cold.amount == kCold && cold.creature_applied, "cold resistance 0 hands 30");
      check(victim.status.at(1) == timer && le(timer, combat::SIXTY), "cold timer is 30 * 60 / hit points");
      check(memory.load(kCreature + 0x228, 4) == sub(kHp, kCold), "cold amount is subtracted from hit points");
    }
    {
      World world(7, 7);
      arm(world, attacker);
      creature_hp::Memory memory;
      auto victim = creature(0);
      victim.max_hp = kHp;
      plant(memory, 0);
      const auto cold = sn::deliver_cold_hit(world, victim, &attacker, 30, 30, memory, kCreature, 0);
      check(cold.amount == kCold && victim.status.at(1) == combat::SIXTY, "cold timer is 60 at 0 hit points");
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
      creature_hp::Memory memory;
      constexpr creature_hp::Address at = 0x30000;
      const auto frames = sn::bash_frames(7);
      check(frames == 7u * 60u, "bash frames are statistic 2 times 60");
      sn::bash_stun(memory, at, frames, 1, 0);
      check(memory.load(at + 0x31c, 2) == frames, "stun timer is stored at +0x31c");
      check(memory.load(at + 0x232, 2) == 0x13, "stun state is 0x13");
      const unsigned colours[] = {0x3a, 0x9b, 0x9a, 0x48, 0x3a, 0x9b, 0x9a, 0};
      for (unsigned i = 0; i < 8; ++i)
        check(memory.load(at + 0x90 + 2 * i, 2) == colours[i], "flash 1 writes the stun colours");
      creature_hp::Memory quiet;
      sn::bash_stun(quiet, at, frames, 0, 0);
      check(quiet.load(at + 0x31c, 2) == frames && quiet.load(at + 0x90, 2) == 0, "flash 0 leaves colours unchanged");
      creature_hp::Memory boss;
      sn::bash_stun(boss, at, frames, 1, 1);
      check(boss.load(at + 0x31c, 2) == 0 && boss.load(at + 0x232, 2) == 0, "boss rank 1 stores nothing");
      creature_hp::Memory corpse;
      corpse.store(at + 0x232, 2, 0x14);
      sn::bash_stun(corpse, at, frames, 1, 0);
      check(corpse.load(at + 0x31c, 2) == 0 && corpse.load(at + 0x232, 2) == 0x14, "a corpse stores nothing");
    }
    {
      constexpr Raw maximum = kHp;
      constexpr Raw percent = 10;
      const auto product = sn::owner_heal_product(maximum, percent);
      const auto grouped = mul(mul(maximum, 0x3c71aa2eu), mul(from_int(percent), 0x3c23d70au));
      check(product == grouped, "heal product uses the grouped float32 order");
      constexpr Raw current = 0x42480000;
      check(sn::add_cached_heal(current, product) == add(current, product), "one heal add uses the cached product");
    }
    {
      constexpr Raw cost = kPlain;
      constexpr Raw below = cost - 1;
      constexpr std::uint16_t recast_field = 30;
      const auto exact = sn::debit_after_zero_cast(cost, cost, 0, recast_field, false);
      const auto under = sn::debit_after_zero_cast(below, cost, 0, recast_field, false);
      const auto above = sn::debit_after_zero_cast(kAbove, cost, 0, recast_field, false);
      const auto blocked = sn::debit_after_zero_cast(cost, cost, 1, recast_field, false);
      const auto copy = sn::debit_after_zero_cast(cost, cost, 0, recast_field, true);
      check(exact.debited && exact.mana == sub(cost, cost) && exact.recast == 30, "exact mana debits and recast is 30");
      check(under.debited && under.mana == sub(below, cost) && under.recast == 30, "one pattern below the cost still debits");
      check(above.debited && above.mana == sub(kAbove, cost) && above.recast == 30, "mana above the cost debits");
      check(!blocked.debited && blocked.mana == cost && blocked.recast == 1, "a positive recast does not debit");
      check(!copy.debited && copy.mana == cost && copy.recast == 0, "the prediction copy does not debit");
    }
    std::cout << "spell-numerics: " << checks << " synthetic checks PASS\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "spell-numerics: FAIL: " << error.what() << " after " << checks << " checks\n";
    return 1;
  }
}
