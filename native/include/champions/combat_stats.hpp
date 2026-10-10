#pragma once
#include "champions/combat.hpp"
#include <functional>
#include <optional>
#include <utility>
#include <vector>
namespace champions::simulation::rta::combat {
// Nonterminating callback-driven level cycles stop after this many completed
// level transitions, retaining prior state and throwing Unsupported.
inline constexpr std::int32_t MAX_LEVEL_TRANSITIONS = 4096;
// Names use Unicode code points, with Python/Unicode16 lowercase semantics.
std::u32string unicode_lower(const std::u32string &);
std::int32_t passive_level(const Tables &, const PlayerRec &, std::int64_t slot,
                           std::uint64_t mask);
std::int32_t boss_rank(const Tables &, bool is_creature,
                       const std::optional<std::u32string> &type_name,
                       const std::u32string &name_prop, std::uint64_t cflags);
Raw level_scale(const Tables &, std::int64_t level_byte,
                std::int64_t difficulty, bool pvp_b,
                const std::u32string &level_name);
struct MonsterStats {
  std::optional<Raw> hp, max_hp;
  std::int32_t armor = 0, dmg_low = 0, dmg_high = 0, attack = 0;
  Raw exp_value = 0;
};
MonsterStats monster_stats(const Tables &, const World &,
                           const std::array<Raw, 5> &pct, std::int64_t rank,
                           const std::optional<std::u32string> &type_name,
                           bool roll_hp, std::uint32_t own_draw = 0,
                           bool dead = false);
std::int64_t exp_cap(const Tables &, std::int64_t level);
std::int32_t
give_exp(const Tables &, World &, std::int64_t slot, std::int64_t amount,
         const std::function<void(std::int64_t)> &on_level_up = {});
std::vector<std::pair<std::int32_t, std::int64_t>>
kill_experience(const Tables &, World &, const Obj &, std::int64_t killer_slot,
                std::uint32_t orb_draw = 0);
std::int64_t item_armor_value(const Item &);
std::int32_t item_attack_rating(const Item &);
std::pair<std::int32_t, std::int32_t> item_damage_range(const Item &,
                                                        bool with_elements);
std::int32_t recalc_armor(const Tables &, World &, std::int64_t slot);
void recalc_stats(const Tables &, World &);
std::array<std::int32_t, 3> recalc_damage(const Tables &, World &, Obj &player,
                                          const Item *weapon,
                                          bool with_elements);
std::int32_t level_for_exp(const Tables &, std::int64_t exp);
} // namespace champions::simulation::rta::combat
