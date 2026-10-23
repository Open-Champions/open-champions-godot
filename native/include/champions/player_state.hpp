#pragma once

#include "champions/arena_executable.hpp"
#include "champions/core_arithmetic.hpp"
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// One RTA US v2.00 player record. Fields and addresses are from research-menus
// docs/player.md, research-combat docs/data-tables.md and research-saves
// docs/save-format.md. The user's executable supplies every table value at run
// time; this module contains no retail table bytes.
namespace champions::simulation::rta::player_state {
struct Refusal : std::runtime_error { using std::runtime_error::runtime_error; };

struct Tables {
  std::array<std::int32_t, 81> experience{};
  std::array<std::array<Raw, 6>, 7> formulas{};
};
Tables read_tables(const arena_modes::Executable &executable);

struct Record {
  std::string name;
  std::int32_t player_class = 0, model = 0;
  std::array<std::int32_t, 4> base_stats{}; // strength, intelligence, dexterity, stamina
  std::array<std::int32_t, 4> effective_stats{}; // record +0x90, including imported equipment
  std::int32_t level = 0, acknowledged = 0, experience = 0, gold = 0;
  Raw hp = 0, max_hp = 0, mana = 0, max_mana = 0;
  bool imported = false;
  bool current_known = false;
};

// The new-character flow has already read class start stats from the same
// executable and applied the twenty spent points. This function returns the
// record as player_initNewCharacter 0x1b1d70 builds it once the first
// player_recalcStats with a live Player object has run: level 1, experience 0,
// gold 500, effective = base, and the maxima in recalcStats' exact operation
// order. init alone leaves max HP 0 and max mana 10 (see player_state.cpp).
// Current HP/mana remain unknown (current_known false).
// Maximum mana and hit points player_recalcStats 0x1b6458 stores for a class,
// level (record +0x59) and effective statistics (record +0x90: strength,
// intelligence, dexterity, stamina), in the research's exact operation order
// (research-combat docs/data-tables.md 4.3). Item modifiers and figurines add to
// the statistics before this and are not modelled: for a record with neither,
// effective equals base. Used by create() and by a level-up.
struct Maxima { Raw max_hp = 0, max_mana = 0; };
Maxima maxima(const Tables &tables, std::int32_t player_class, std::int32_t level,
              const std::array<std::int32_t, 4> &effective_stats);
Record create(const std::string &name, std::int32_t player_class,
              std::int32_t model, const std::array<std::int32_t, 4> &stats,
              const Tables &tables);
// Imported fields come from the decoded live PS2 record. Item modifiers and
// figurines are carried in its already calculated maxima; recomputing from
// base stats alone would discard them. Refuses a level above 50 or experience
// above table[50]: player_importCharacter clamps both.
Record import(Record record, const Tables &tables);
void validate(const Record &, const Tables &);

// An authored companion for native .ocplay/.ocnsave files. Never a PS2 slot.
// It carries the record only. Loading requires the same selected executable
// for experience thresholds; no table bytes are written to the companion.
std::vector<std::uint8_t> save(const Record &, const Tables &);
Record load(const std::vector<std::uint8_t> &, const Tables &);
} // namespace champions::simulation::rta::player_state
