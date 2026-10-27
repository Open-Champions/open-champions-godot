#pragma once
// Addressed, static script contracts, research-strife-drundar 0d42d46;
// door predicate: 3216e7b0 tools/difftest_strife_drundar.py:399-402.
// Identifiers live only in the selected script/object images. No constructor,
// continuous script task, interaction or reachability is established here.
#include "champions/amx.hpp"
#include "champions/level_factory.hpp"
#include "champions/level_flow.hpp"
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace champions::simulation::rta::mission1_scripts {
enum class Level { Strife, Drundar };
struct Site {
  std::uint32_t offset = 0;
  std::string command;
  std::vector<amx::Cell> arguments;
  std::map<unsigned, std::string> strings;
};
struct Switch {
  std::uint32_t key_entry = 0, action_entry = 0;
  std::string persistent, effect;
};
struct Census {
  std::size_t records = 0, selected = 0, not_created = 0, hostiles = 0;
  std::size_t stage1 = 0, stage2 = 0, catapults = 0, switches = 0;
  std::map<std::uint32_t, std::size_t> constructors;
};
class Contract {
public:
  // A different script is not a candidate. A candidate with changed addressed
  // cells throws: it must never silently fall back to unrestricted execution.
  static std::optional<Contract> inspect(const amx::Machine &);
  Level level;
  std::map<std::uint32_t, Site> sites;
  std::map<std::uint32_t, std::string> entries;
  std::vector<std::string> stage_keys, scouts, bridges;
  std::vector<Switch> switches;
  std::map<std::uint32_t, std::string> restore_keys;
  std::string counter_key, door_flag;
  Census census;
  bool objects_validated = false;

  void validate_objects(const std::vector<level_flow::Record> &, const level_factory::Executable &);
  // Validate the actual VM call (including dynamic counter setv) before the
  // existing binder is allowed to run it. The caller still refuses unported
  // callees/objects; a static call is not proof of an implemented effect.
  bool allows(const amx::Machine &, const std::string &, const std::vector<amx::Cell> &) const;
  std::optional<std::uint32_t> offset_of(const std::string &) const;
private:
  explicit Contract(Level which) : level(which) {}
  std::string stage1_init_, stage1_death_, stage2_init_, stage2_death_, catapult_death_;
};
} // namespace champions::simulation::rta::mission1_scripts
