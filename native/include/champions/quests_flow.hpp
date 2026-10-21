#pragma once
// Act change shared by the campaign and level flow. The campaign uses its own
// prefix match (an empty prefix does not match). Level flow keeps its match.
// The clear runs when the campaign's save_actChanged is non-zero, the card
// latch is clear and no death task is running.
#include "champions/level_flow.hpp"
#include "champions/quests.hpp"
#include <functional>
#include <string>

namespace champions::simulation::rta::quests {
level_flow::ChangeResult
change_level(Campaign &, level_flow::State &, const level_flow::ChangeOptions &,
             const std::function<void(const std::string &)> &event = {});
} // namespace champions::simulation::rta::quests
