#pragma once
// Copies the binder's player-record campaign fields into the typed campaign
// and back. Bags, the menu and the act tables stay on the campaign: the binder
// does not hold them.
#include "champions/quests.hpp"
#include "champions/script_builtins.hpp"

namespace champions::simulation::rta::quests {
void mirror_from_variables(Campaign &, const champions::simulation::rta::builtins::Variables &);
void mirror_to_variables(const Campaign &, champions::simulation::rta::builtins::Variables &);
void write_exp(const Campaign &, champions::simulation::rta::builtins::Variables &, int slot);
bool route_undocumented(const char *name, const Campaign &);
} // namespace champions::simulation::rta::quests
