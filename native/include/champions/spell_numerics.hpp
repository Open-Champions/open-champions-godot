#pragma once
#include "champions/combat.hpp"
#include "champions/core_arithmetic.hpp"
#include "champions/creature_hp.hpp"
#include <cstdint>

namespace champions::simulation::rta::spell_numeric {
// Observed spell numerics from research-spells-r3 cf8320e,
// docs/spec/player-spells-numeric-r3.md. Synthetic inputs only.
// The player damage slot, death, knock-back, spatial queries, and the real
// cast body stay outside this entry (same note, lines 63-66 and 129-130).
// Spells round 5 (player damage / knock-back) is not used.

// ID 55 owner amount. player-spells-numeric-r3.md:81-88.
// float32(float32(maximum * 0x3c71aa2e) * float32(float32(percent) * 0x3c23d70a))
// `percent` is the integer statistic, converted with from_int before the
// hundredth multiply. maximum is record +0xb0 for a player owner.
inline Raw owner_heal_product(Raw maximum, Raw percent_integer) {
  constexpr Raw kRate = 0x3c71aa2eu;
  constexpr Raw kHundredth = 0x3c23d70au;
  return mul(mul(maximum, kRate), mul(from_int(percent_integer), kHundredth));
}

// One add of that cached amount onto record +0xac. The cap that decides
// whether the run adds is the earlier healing body, not this product.
inline Raw add_cached_heal(Raw current, Raw product) { return add(current, product); }

// Post-cast debit after a zero cast return. player-spells-numeric-r3.md:97-107
// and :125-127. Recast already above zero, and the prediction copy, do not
// debit. One pattern below the cost still subtracts; the helper does not
// refuse it. `recast_field` is the skill halfword (30 for the probed records).
struct Payment {
  Raw mana = 0;
  std::uint16_t recast = 0;
  bool debited = false;
};
inline Payment debit_after_zero_cast(Raw mana, Raw cost, std::uint16_t recast_now,
                                    std::uint16_t recast_field, bool prediction_copy) {
  if (recast_now > 0 || prediction_copy)
    return Payment{mana, recast_now, false};
  return Payment{sub(mana, cost), recast_field, true};
}

// ID 49. frames is statistic 2 times 60 (player-spells-numeric-r3.md:70-75).
// The probed boss case is rank 1. The body skips every nonzero boss-rank
// return (ref_creature.py slot30_stun). A derived slot 27 stays unknown;
// this store is the base state body 0x3c2688.
inline std::uint16_t bash_frames(std::int32_t statistic2) {
  const auto product = std::int64_t(statistic2) * 60;
  return static_cast<std::uint16_t>(static_cast<std::uint32_t>(product) & 0xffffu);
}

struct Delivery {
  int result = 0;
  bool handed = false;
  Raw amount = 0;
  std::uint32_t handed_flags = 0;
  bool creature_applied = false;
  bool player_slot_refused = false;
};

// Creature hit-point delivery. +0xc8 and +0x2dc are written by
// Creature_takeDamage, not by a second guess. player-spells-numeric-r3.md:43-61.
Delivery deliver_creature_hit(combat::World &world, combat::Obj &victim, combat::Obj *attacker,
                             Raw dmin, Raw dmax, std::int64_t attack, std::uint32_t flags,
                             const combat::Descriptor *descriptor, std::int64_t no_crit,
                             creature_hp::Memory &memory, creature_hp::Address creature,
                             creature_hp::Address attacker_address);

Delivery deliver_cold_hit(combat::World &world, combat::Obj &victim, combat::Obj *attacker,
                         std::int32_t cold_low, std::int32_t cold_high, creature_hp::Memory &memory,
                         creature_hp::Address creature, creature_hp::Address attacker_address);

void bash_stun(creature_hp::Memory &memory, creature_hp::Address creature, std::uint16_t frames,
               int flash, int boss_rank);
} // namespace champions::simulation::rta::spell_numeric
