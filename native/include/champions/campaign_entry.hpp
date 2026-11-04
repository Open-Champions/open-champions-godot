#pragma once

#include "champions/campaign_inventory.hpp"
#include "champions/mission_menu_table.hpp"
#include "champions/script_commands.hpp"

namespace champions::simulation::rta::script_commands {
// A live cell can be kept or removed only when its definition pointer is set.
// A count with a zero definition is not a classified item (the planted-count
// refusal). Campaign-model bags still have no equipment pointers.
inline bool live_bags_classified(const builtins::Memory &memory) {
  const auto players = memory.s32(script_items::G_NUM_PLAYERS);
  if (players != 1)
    return false;
  for (int bag = 0; bag < script_items::BAGS; ++bag) {
    const auto count = memory.s32(script_items::bag_count_at(0, bag));
    if (count < 0 || count > script_items::BAG_SLOTS)
      return false;
    for (int index = 0; index < count; ++index)
      if (memory.u32(script_items::bag_entry(0, bag, index)) == 0)
        return false;
  }
  return true;
}

inline void copy_live_bags(Host &next, Host &previous) {
  const auto &old_memory = previous.variables().memory;
  auto &new_memory = next.variables().memory;
  const auto players = old_memory.s32(script_items::G_NUM_PLAYERS);
  for (int slot = 0; slot < players; ++slot) {
    const auto base = script_items::rec(slot);
    for (const auto span : {std::pair<unsigned, unsigned>{0x130, 3 * 0x1208},
                            {0x43f0, 0x50}, {0x4a7c, 1}, {0x4a84, 4}})
      new_memory.write(base + span.first, old_memory.read(base + span.first, span.second));
  }
}

// The client owns one local player. Classified live cells travel and an act
// change removes unkept flag-39 cells through inventory_removeItem
// (inventory-reset.md:65-84). A count with no definition, a campaign-model
// bag, or an imported record still refuses: those have no equipment pointers
// to slide.
inline int require_campaign_act_transfer(Host &previous, const quests::ActTables &tables,
                                         const std::string &destination, bool imported) {
  const auto current = previous.campaign().act();
  if (current < 0 || std::size_t(current) >= tables.names.size())
    throw std::runtime_error("Campaign act is unbound; act transfer is refused.");
  const auto next = quests::find_act(destination, current, tables.names);
  if (next < 0) throw std::runtime_error("Destination has no documented campaign act.");
  if (imported)
    throw std::runtime_error("Imported inventory carry across this act transfer is unverified.");
  const auto &memory = previous.variables().memory;
  if (memory.s32(script_items::G_NUM_PLAYERS) != 1)
    throw std::runtime_error("Act transfer supports one local player.");
  bool campaign_bags = false;
  for (int bag = 0; bag < script_items::BAGS; ++bag)
    if (!previous.campaign().record(0).bags[std::size_t(bag)].empty())
      campaign_bags = true;
  if (campaign_bags || !live_bags_classified(memory))
    throw std::runtime_error("Act transfer with inventory is refused: live bag carry and flag-39 migration are unported.");
  return next;
}

inline std::string mission2_story_destination(const quests::Campaign &campaign) {
  if (campaign.rows().size() != 13)
    throw std::runtime_error("Mission table is unbound.");
  const auto &row = campaign.rows()[1];
  const auto &name = campaign.evil() != 0 && !row.evil.empty() ? row.evil : row.story;
  if (name != "innovat1")
    throw std::runtime_error("Mission-2 alternate story entry is Unknown in the research.");
  return name;
}

// research-quests@7efd3ce docs/quests.md:266-277,285-298. A table
// miss is not an act change; current-act prefix matches take precedence.
inline bool campaign_entry_requires_transfer(const Host &previous,
                                             const quests::ActTables &tables,
                                             const std::string &destination,
                                             bool mission2) {
  const auto source = previous.campaign().act();
  const auto found = quests::find_act(destination, source, tables.names);
  return mission2 || (found >= 0 && found != source);
}

struct CampaignEntry {
  bool changed = false, reset = false;
  int act = -1;
};
// Normal offline level entry. The existing native change_level contract
// clears quests/variable values and preserves mission/medal/player records.
// The newly constructed host has no old world blob; an act reset discards
// that blob anyway. Other carried routes retain their existing policy.
inline CampaignEntry prepare_campaign_entry(Host &next, Host *previous,
                                             const quests::ActTables &tables,
                                             const std::string &level, bool mission2,
                                             bool imported, bool death_task = false) {
  CampaignEntry out;
  if (previous && campaign_entry_requires_transfer(*previous, tables, level, mission2)) {
    const auto destination = require_campaign_act_transfer(*previous, tables, level, imported);
    next.campaign().set_acts(tables.names);
    next.level().acts = tables.flow;
    next.level().act = previous->campaign().act();
    next.level().pending.request(level);
    copy_live_bags(next, *previous);
    next.variables().memory.w32(campaign_inventory::FRONTEND_WORD,
                                previous->variables().memory.u32(campaign_inventory::FRONTEND_WORD));
    next.variables().memory.w8(campaign_inventory::CARD_LATCH,
                               previous->variables().memory.u8(campaign_inventory::CARD_LATCH));
    level_flow::ChangeOptions options;
    options.hub = next.campaign().hub();
    options.death_task = death_task;
    const auto change = next.change_level(options);
    // inventory-reset.md:59-63. A frontend word skips the act call, so the
    // stored act stays the one already held and is not the destination's act.
    if (next.variables().memory.u32(campaign_inventory::FRONTEND_WORD) == 0 &&
        next.campaign().act() != destination)
      throw std::runtime_error("Campaign act lookups disagree.");
    next.level().prev = level_flow::qualified_name(previous->level().level, previous->level().sub);
    out.changed = change.act_changed;
    out.reset = change.reset_act;
  } else {
    next.campaign().set_acts(tables.names);
    next.level().acts = tables.flow;
    int act = previous ? previous->campaign().act() : quests::find_act(level, -1, tables.names);
    // A hub name is shared by acts. An imported character without a carried
    // act cannot establish which one, so leave it unbound for the guard.
    if (!previous && imported && (level == "hub" || level == "hub2")) act = -1;
    next.campaign().set_act(act);
    next.level().act = act;
    const auto request = level_flow::split_request(level);
    next.level().level = request.first;
    next.level().sub = request.second;
  }
  out.act = next.campaign().act();
  return out;
}
} // namespace champions::simulation::rta::script_commands
