#pragma once
// Copies the binder's player-record campaign fields into the typed campaign
// and back. Bags, the menu and the act tables stay on the campaign: the binder
// does not hold them.
#include "champions/quests.hpp"
#include "champions/script_builtins.hpp"

namespace champions::simulation::rta::quests {
void mirror_from_variables(Campaign &, const champions::simulation::rta::builtins::Variables &,
                           bool current_known = true);
// Writes the record fields back. Quest-variable values are written for the
// names already in the binder's table, in order.
void mirror_to_variables(const Campaign &, champions::simulation::rta::builtins::Variables &,
                         bool current_known = true);
// mirror_to_variables, then the whole quest-variable table from the campaign
// (names and values). Used when a saved campaign is loaded into a host.
void restore_variables(const Campaign &, champions::simulation::rta::builtins::Variables &,
                       bool current_known = true);
void write_exp(const Campaign &, champions::simulation::rta::builtins::Variables &, int slot);
bool route_undocumented(const char *name, const Campaign &);
} // namespace champions::simulation::rta::quests
