#include "champions/quests_flow.hpp"

namespace champions::simulation::rta::quests {
level_flow::ChangeResult
change_level(Campaign &campaign, level_flow::State &state, const level_flow::ChangeOptions &options,
             const std::function<void(const std::string &)> &event) {
  auto result = level_flow::begin_change(state, options, event);
  campaign.set_act(state.act);
  if (result.reset_act)
    campaign.clear_for_act_change();
  return result;
}
} // namespace champions::simulation::rta::quests
