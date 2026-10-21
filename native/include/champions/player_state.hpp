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
  std::int32_t level = 0, acknowledged = 0, experience = 0, gold = 0;
  Raw hp = 0, max_hp = 0, mana = 0, max_mana = 0;
  bool imported = false;
  bool current_known = false;
};

// The new-character flow has already read class start stats from the same
// executable and applied the twenty spent points. This function derives the
// maxima with player_recalcStats' exact operation order and initializes the
// character's documented level, experience and gold.
Record create(const std::string &name, std::int32_t player_class,
              std::int32_t model, const std::array<std::int32_t, 4> &stats,
              const Tables &tables);
// Imported fields come from the decoded live PS2 record. Item modifiers and
// figurines are carried in its already calculated maxima; recomputing from
// base stats alone would discard them.
Record import(Record record, const Tables &tables);
void validate(const Record &, const Tables &);

// An authored companion for native .ocplay/.ocnsave files. Never a PS2 slot.
// It carries the record only. Loading requires the same selected executable
// for experience thresholds; no table bytes are written to the companion.
std::vector<std::uint8_t> save(const Record &, const Tables &);
Record load(const std::vector<std::uint8_t> &, const Tables &);
} // namespace champions::simulation::rta::player_state
