// Playable integration (wave 11): the effect-class damage entry of native-fx-2's
// GameHooks. Forwards to the executed descriptor glue of combat_runtime_damage.cpp.
#include "combat_runtime_internal.hpp"
namespace champions::simulation::rta::combat_runtime {
int damage_with_descriptor(World::State &, Address, Address, Raw, Raw,
                           const combat::Descriptor &, Raw, Raw);
int World::damage_descriptor(Address victim, Address attacker, Raw attack,
                             Raw flags,
                             const std::vector<std::array<std::int64_t, 4>> &rows,
                             Raw minimum, Raw maximum) {
  if (s_->failed || s_->running)
    throw Error("combat pass failed or reentrant");
  if (rows.size() != 4)
    throw Unsupported("damage descriptor needs four rows");
  combat::Descriptor d;
  for (const auto &row : rows)
    d.push_back(std::vector<std::int64_t>(row.begin(), row.end()));
  return damage_with_descriptor(*s_, victim, attacker, attack, flags, d, minimum,
                                maximum);
}
} // namespace champions::simulation::rta::combat_runtime
