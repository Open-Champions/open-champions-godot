#pragma once

#include "champions/mission_menu_table.hpp"
#include "champions/script_commands.hpp"

namespace champions::simulation::rta::script_commands {
// The client owns one local player. Quest bridge records do not mirror the
// live item bags (quests_bridge.cpp), so mission-2 carry supports an empty
// modeled inventory only, including same-act entry. Never silently omit
// item carry or flag-39 removal.
inline int require_campaign_act_transfer(Host &previous, const quests::ActTables &tables,
                                         const std::string &destination, bool imported) {
  const auto current = previous.campaign().act();
  if (current < 0 || std::size_t(current) >= tables.names.size())
    throw std::runtime_error("Campaign act is unbound; mission-2 act transfer is refused.");
  const auto next = quests::find_act(destination, current, tables.names);
  if (next < 0) throw std::runtime_error("Destination has no documented campaign act.");
  if (imported)
    throw std::runtime_error("Imported inventory carry into mission 2 is unverified.");
  const auto &memory = previous.variables().memory;
  if (memory.s32(script_items::G_NUM_PLAYERS) != 1)
    throw std::runtime_error("Mission-2 act transfer supports one local player.");
  for (int bag = 0; bag < script_items::BAGS; ++bag)
    if (memory.u32(script_items::bag_count_at(0, bag)) != 0 ||
        !previous.campaign().record(0).bags[std::size_t(bag)].empty())
      throw std::runtime_error("Mission-2 entry with inventory is refused: live bag carry and flag-39 migration are unported.");
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
                                             bool imported) {
  next.campaign().set_acts(tables.names);
  next.level().acts = tables.flow;
  CampaignEntry out;
  if (previous && mission2) {
    const auto destination = require_campaign_act_transfer(*previous, tables, level, imported);
    next.level().act = previous->campaign().act();
    next.level().pending.request(level);
    level_flow::ChangeOptions options;
    options.hub = next.campaign().hub();
    const auto change = next.change_level(options);
    if (next.campaign().act() != destination)
      throw std::runtime_error("Campaign act lookups disagree.");
    next.level().prev = level_flow::qualified_name(previous->level().level, previous->level().sub);
    out.changed = change.act_changed;
    out.reset = change.reset_act;
  } else {
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
