#include "champions/quests_flow.hpp"

namespace champions::simulation::rta::quests {
level_flow::ChangeResult
change_level(Campaign &campaign, level_flow::State &state, const level_flow::ChangeOptions &options,
             const std::function<void(const std::string &)> &event) {
  const auto pending = state.pending.name;
  const auto decision = pending.empty() ? std::pair<int, int>{0, campaign.act()}
                                        : campaign.act_changed(pending);
  auto result = level_flow::begin_change(state, options, event);
  if (decision.first && !options.card_latch && !options.death_task)
    campaign.clear_for_act_change();
  return result;
}
} // namespace champions::simulation::rta::quests
