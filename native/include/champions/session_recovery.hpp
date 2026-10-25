#pragma once
#include "champions/player.hpp"

namespace champions::simulation::rta::session_recovery {
// Closed authored domain, not a Player constructor or an inventory action.
inline constexpr std::uint32_t RECORD = 0x4d8af0, RECORD_SIZE = 0x4c30;
inline constexpr const char *POLICY = "rta-session-recovery-v1";
enum class SpellPolicy : std::uint8_t { NoActiveSpells };
struct Initial {
  std::uint32_t slot = 0, record = RECORD;
  std::int32_t player_class = 1;
  Raw health = 0x42480000, max_health = 0x42c80000;
  Raw mana = 0x41a00000, max_mana = 0x42480000;
  Raw pending_health = 0x41200000, health_rate = 0x3f800000;
  Raw pending_mana = 0x40c00000, mana_rate = 0x3f000000;
  std::int8_t health_bonus = 0, mana_bonus = 0;
  // passive_level scans exactly48 rows, not a sentinel list. Explicit unused
  // rows are zero. skill_rank has the separately supplied -1 terminator.
  std::array<std::uint64_t, 48> passive_masks{};
  std::array<std::int8_t, 48> passive_levels{};
  std::vector<std::pair<std::int16_t, std::int16_t>> ranks{{-1, 0}};
  std::array<std::uint32_t, 17> equipment{};
  SpellPolicy spells = SpellPolicy::NoActiveSpells;
  // Opaque authored query token; no original RTTI or spell implementation.
  std::uint32_t blessed_type_token = 0xf5000001;
  bool copy_at_setup =
      false; // Authored World spawn input; flags remain World-owned.
};
void validate(const Initial &);
std::uint32_t passive_table(const Initial &);
std::vector<creature_hp::Write> setup(std::uint32_t actor, const Initial &);
void bind_services(player::Tick &, const Initial &);
} // namespace champions::simulation::rta::session_recovery
