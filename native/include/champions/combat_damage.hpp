#pragma once
#include "champions/combat.hpp"

namespace champions::simulation::rta::combat {
// RTA reference damage rules. Pointees must remain alive and stable through all
// callbacks. Arithmetic uses the caller's selected FPU; no hardware float.
Raw base_roll(Raw minimum, Raw maximum, std::uint32_t draw);
void set_status(World &, Obj &, std::int64_t which, Raw value, Obj *source);
Raw proc_amount(World &, std::int32_t low, std::int32_t high);
void fx(World &, std::uint32_t name, const Obj &);
Raw bloodlust_armor_factor(World &, Spell &);
bool spirit_shield_absorbs(World &, Spell &);
Raw frost_armor_on_hit(World &, Spell &, Raw damage, Obj *attacker);
void thorns(World &, std::int64_t skill, Obj &victim, Obj &attacker,
            std::int64_t level);
Raw crit_bonus(World &, std::int64_t skill, Obj &attacker, std::int64_t rank);
std::int32_t stun_chance(World &, Obj &, std::int64_t rank);
void stun_effect(World &, Obj &);
Raw retribution(World &, Spell &, Raw damage);
int colour_class(const Obj &, std::uint32_t flags);
bool facing_front(const Obj &victim, const Obj &attacker);
// The reference may recurse forever. At 128 simultaneous damage calls this
// port throws Unsupported, preserving mutations/draws already performed; this
// safety limit is not a successful gameplay result or a console stack model.
int damage_apply(World &, Obj &victim, Obj *attacker, Raw minimum, Raw maximum,
                 std::int64_t attack, std::uint32_t flags,
                 const Descriptor *descriptor = nullptr,
                 std::int64_t no_crit = 0);
} // namespace champions::simulation::rta::combat
