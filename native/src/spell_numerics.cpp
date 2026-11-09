#include "champions/spell_numerics.hpp"
#include "champions/combat_damage.hpp"
#include "champions/creature_hp.hpp"
#include <cstddef>
#include <utility>

namespace champions::simulation::rta::spell_numeric {
namespace {
using combat::Descriptor;
using combat::Obj;
using combat::World;

Descriptor cold_descriptor(std::int32_t low, std::int32_t high) {
  // Type 0x16, both cold values, no direct damage. player-spells-numeric-r3.md:56-59.
  Descriptor desc = combat::EMPTY_DESC;
  desc[0] = {0x16, low, high, 0};
  return desc;
}

struct Hand {
  bool present = false;
  Raw amount = 0;
  std::uint32_t flags = 0;
};

Hand last_hit(const World &world, std::size_t first) {
  Hand hand;
  for (auto index = first; index < world.events.size(); ++index) {
    const auto &event = world.events[index];
    if (event.empty())
      continue;
    const auto *name = std::get_if<combat::Text>(&event[0].value);
    if (!name || *name != U"take_damage" || event.size() < 5)
      continue;
    const auto *amount = std::get_if<std::uint64_t>(&event[2].value);
    const auto *flags = std::get_if<std::uint64_t>(&event[4].value);
    if (!amount || !flags)
      continue;
    hand.present = true;
    hand.amount = static_cast<Raw>(*amount);
    hand.flags = static_cast<std::uint32_t>(*flags);
  }
  return hand;
}
} // namespace

Delivery deliver_creature_hit(World &world, Obj &victim, Obj *attacker, Raw dmin, Raw dmax,
                             std::int64_t attack, std::uint32_t flags, const Descriptor *descriptor,
                             std::int64_t no_crit, creature_hp::Memory &memory, creature_hp::Address creature,
                             creature_hp::Address attacker_address) {
  // damage_apply 0x1e8230, then Creature_takeDamage 0x116638 when the victim
  // is not carrying the player flag. A player-flagged victim (including 0x60)
  // still receives the handed amount from damage_apply; the player slot is
  // not entered. player-spells-numeric-r3.md:33-36 and :52-55.
  // Death and knock-back are Creature_processDamage 0x110d30 and are not called.
  // The hook is swapped for this call only and restored on every exit. Earlier
  // events stay; only the events this call appends are read.
  struct Hook {
    World &world;
    decltype(World::take_damage_fn) saved;
    ~Hook() { world.take_damage_fn = std::move(saved); }
  } hook{world, std::move(world.take_damage_fn)};
  world.take_damage_fn = [](Obj &, Raw, Obj *, std::uint32_t) {};
  const auto first = world.events.size();
  Delivery out;
  out.result = combat::damage_apply(world, victim, attacker, dmin, dmax, attack, flags, descriptor,
                                    no_crit);
  const auto hand = last_hit(world, first);
  out.handed = hand.present;
  out.amount = hand.amount;
  out.handed_flags = hand.flags;
  if (!hand.present)
    return out;
  if (victim.flags & 0x40) {
    out.player_slot_refused = true;
    return out;
  }
  creature_hp::Model model(memory, creature_hp::CallbackPolicy::ReferenceZero);
  model.take_damage(creature, attacker_address, hand.flags, hand.amount);
  out.creature_applied = true;
  return out;
}

Delivery deliver_cold_hit(World &world, Obj &victim, Obj *attacker, std::int32_t cold_low,
                         std::int32_t cold_high, creature_hp::Memory &memory, creature_hp::Address creature,
                         creature_hp::Address attacker_address) {
  // Direct damage 0. The cold amount is the descriptor, not a guessed range.
  // player-spells-numeric-r3.md:56-59 and :119-120.
  const auto desc = cold_descriptor(cold_low, cold_high);
  return deliver_creature_hit(world, victim, attacker, ZERO, ZERO, 100000, 0, &desc, 0, memory,
                             creature, attacker_address);
}
} // namespace champions::simulation::rta::spell_numeric
