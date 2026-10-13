#pragma once
#include "champions/creature_hp.hpp"
#include <set>

namespace champions::simulation::rta::skills_a {
using Address = creature_hp::Address;
using Memory = creature_hp::Memory;
using Values = std::vector<Raw>;
struct Unsupported : std::runtime_error { using std::runtime_error::runtime_error; };
inline constexpr Raw G_RAND_SEED = 0x404464, G_SKILL_TABLE = 0x41b170;
struct Call { unsigned slot; Values args; };
// RTA shared Skill foundation only. No individual skill/spell/object lifecycle.
// Memory and history caps are inherited from the standalone raw32 Memory.
class Model {
  std::size_t depth_ = 0;
  void write(Address, std::size_t, Raw);
  Raw scalar(Raw skill, unsigned slot, Values args);
public:
  Memory mem;
  std::vector<creature_hp::Write> writes;
  std::vector<Call> calls;
  std::uint64_t draws = 0, hidden_draws = 0;
  std::set<unsigned> external;
  using Callback = std::function<Values(Model &, const Values &)>;
  std::map<unsigned, Callback> callbacks;
  Model() = default;
  Model(const Model &) = delete;
  Model &operator=(const Model &) = delete;
  // Callback receives adjusted this followed by low32 arguments. Copying the
  // callable supports replacement during execution. Exceptions preserve state.
  // Missing/unknown derived bodies refuse; no fabricated zero callback result.
  Values skill_call(Raw skill, unsigned slot, const Values &args);
  Raw skill_by_id(Raw id) const;
  Raw skill_can_use_base(Raw skill, Raw caster = 0) const;
  Raw skill_cast_base(Raw skill, Raw level = 0, Raw caster = 0) const;
  Raw skill_mana_base(Raw skill, Raw level = 0) const;
  void skill_init_stats_base(Raw skill);
  Raw skill_stat(Raw skill, Raw stat, Raw level, Raw unused = 0) const;
  Raw skill_stat_tier2(Raw skill, Raw stat, Raw level, Raw cap) const;
  Raw skill_stat_for(Raw skill, Raw stat, Raw level, Raw caster, Raw cap);
  Raw skill_stat_for_handle(Raw skill, Raw stat, Raw level, Raw handle, Raw cap);
  Raw skill_adjust_level(Raw skill, Raw level, Raw n = 0) const;
  Raw skill_mana_from_stat4(Raw skill, Raw level, Raw n);
  Values skill_stat_range(Raw skill, Raw stat, Raw level, Raw randomise, Raw spread);
  Raw passive_cast(Raw skill, Raw level = 0, Raw caster = 0) const;
  Raw dodge_fraction(Raw skill, Raw rank);
  Raw regeneration_stat0(Raw skill, Raw level);
  Raw mana_cost_at(Raw function, Raw skill, Raw level);
  Raw draw();
};
} // namespace champions::simulation::rta::skills_a
