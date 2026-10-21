#pragma once
// One level change for the campaign and level flow together. Level flow's
// begin_change makes the act decision (save_actChanged, then the card latch
// and death-task tests of mainloop_loadRequestedWorld). The campaign takes
// that act and, when level flow resets the act, runs its own clear: the
// quest lists, the flag-39 items and the quest-variable values. One decision
// serves both, so they cannot disagree about a change.
#include "champions/level_flow.hpp"
#include "champions/quests.hpp"
#include <functional>
#include <string>

namespace champions::simulation::rta::quests {
level_flow::ChangeResult
change_level(Campaign &, level_flow::State &, const level_flow::ChangeOptions &,
             const std::function<void(const std::string &)> &event = {});
} // namespace champions::simulation::rta::quests
