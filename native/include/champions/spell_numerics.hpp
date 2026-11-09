#pragma once
#include "champions/combat.hpp"
#include "champions/core_arithmetic.hpp"
#include "champions/creature_hp.hpp"
#include <cstdint>

namespace champions::simulation::rta::spell_numeric {
// Observed spell numerics from research-spells-r3 cf8320e,
// docs/spec/player-spells-numeric-r3.md. The player damage slot, death,
// knock-back, spatial queries and the real cast body stay outside this note
// (lines 63-66 and 129-130). Spells round 5 is not used.

// ID 55 grouped heal product.
// float32(float32(maximum * 0x3c71aa2e) * float32(float32(percent) * 0x3c23d70a))
// research-spells-r3 ran it for the owner amount only: statistic 0 and the
// owner record +0xb0, stored at spell +0x1c (player-spells-numeric-r3.md:81-89).
// skills_b also uses this grouping for the non-owner share (statistic 2 and the
// recipient +0xb0). That share comes from the earlier skills_b contract; this
// note does not cover it.
inline Raw heal_grouped_product(Raw maximum, Raw percent_integer) {
  constexpr Raw kRate = 0x3c71aa2eu;
  constexpr Raw kHundredth = 0x3c23d70au;
  return mul(mul(maximum, kRate), mul(from_int(percent_integer), kHundredth));
}

// One add of a heal amount onto record +0xac (player-spells-numeric-r3.md:81-83).
inline Raw add_heal(Raw current, Raw amount) { return add(current, amount); }

// Player_castSkill 0x1a42b8 payment. Tick::cast_skill (player.cpp) calls these
// four functions; no other copy of the debit exists.
//
// Observed (player-spells-numeric-r3.md:94-108; difftest_spell_numeric_r3.py:
// 310-367): skill kind 0, skill recast 30, skill flag bit 0 clear, the cast slot
// returning 0. Mana exactly the cost, one pattern below it and 1000 all debit;
// recast 1 blocks; the prediction copy (0x20000) blocks.
//
// Recast is the signed halfword at player +0x4ae.
inline bool recast_blocks(std::uint16_t recast_halfword) {
  return static_cast<std::int16_t>(recast_halfword) > 0;
}
// The prediction copy returns only when skill +4 bit 0 is clear.
inline bool copy_blocks(bool prediction_copy, std::uint32_t skill_flags) {
  return prediction_copy && !(skill_flags & 1u);
}

// Payment inputs that research-spells-r3 did not run. Tick::cast_skill still
// pays in these cases. That behaviour comes from the player-tick contract and its
// pinned player vectors, not from this note. The note says the real cast body
// "remain[s] the player-tick contract" (player-spells-numeric-r3.md:110-112).
enum Unprobed : std::uint8_t {
  kObserved = 0,
  // Recast below zero, e.g. 0xffff. The note ran recast 0 and 1 only (:100-103;
  // difftest :362 and :364).
  kRecastNegative = 1u << 0,
  // A prediction copy whose skill has bit 0 set gets past the copy gate. The
  // note ran bit 0 clear only (:95; difftest :354).
  kCopyBit0Set = 1u << 1,
  // The cast slot returned a spell. The note stubbed the slot to return 0 (:96-97).
  // Payment with the real cast body is Unknown (:110).
  kNonZeroCast = 1u << 2,
  // Skill kind other than 0. Kind 1 scales the cost by 0x3c888888; the note ran
  // kind 0 only (:95; difftest :354).
  kKindNotZero = 1u << 3,
};

struct Payment {
  Raw mana = 0;           // New record +0xc0.
  Raw cost = 0;           // The amount subtracted.
  std::uint8_t unprobed = kObserved;
};

// Debit after the cast slot returns. `slot5` is the skill's slot-5 float, `kind`
// is skill +8, `recast_before` the +0x4ae halfword read at entry, `skill_flags`
// skill +4 read at the copy gate. One subtract, with no below-cost refusal
// (player-spells-numeric-r3.md:106).
inline Payment cast_payment(Raw mana, Raw slot5, std::int32_t kind, std::uint16_t recast_before,
                            bool prediction_copy, std::uint32_t skill_flags,
                            std::uint32_t cast_return) {
  Payment out;
  out.cost = kind == 1 ? mul(slot5, 0x3c888888u) : slot5;
  out.mana = sub(mana, out.cost);
  if (static_cast<std::int16_t>(recast_before) < 0)
    out.unprobed |= kRecastNegative;
  if (prediction_copy && (skill_flags & 1u))
    out.unprobed |= kCopyBit0Set;
  if (cast_return != 0)
    out.unprobed |= kNonZeroCast;
  if (kind != 0)
    out.unprobed |= kKindNotZero;
  return out;
}

// Synthetic fixtures below. The client does not call them. They chain the live
// combat::damage_apply and creature_hp::Model::take_damage so that the test
// can compare them with the note.
struct Delivery {
  int result = 0;
  bool handed = false;
  Raw amount = 0;
  std::uint32_t handed_flags = 0;
  bool creature_applied = false;
  bool player_slot_refused = false;
};

// Creature hit-point delivery (player-spells-numeric-r3.md:38-61). `world` keeps
// its events and its take-damage hook: the hook is swapped for the call only.
Delivery deliver_creature_hit(combat::World &world, combat::Obj &victim, combat::Obj *attacker,
                             Raw dmin, Raw dmax, std::int64_t attack, std::uint32_t flags,
                             const combat::Descriptor *descriptor, std::int64_t no_crit,
                             creature_hp::Memory &memory, creature_hp::Address creature,
                             creature_hp::Address attacker_address);

Delivery deliver_cold_hit(combat::World &world, combat::Obj &victim, combat::Obj *attacker,
                         std::int32_t cold_low, std::int32_t cold_high, creature_hp::Memory &memory,
                         creature_hp::Address creature, creature_hp::Address attacker_address);
} // namespace champions::simulation::rta::spell_numeric
