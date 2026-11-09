#pragma once

#include "champions/campaign_inventory.hpp"
#include "champions/mission_menu_table.hpp"
#include "champions/script_commands.hpp"

namespace champions::simulation::rta::script_commands {
// The session's bag-carry guard (session_controller.cpp enable_level_script).
// The previous host's bags are this character's bags only while a player
// record is held, no starting items are pending, and the inventory was last
// written for the current player record revision. Creating a character, or
// loading or clearing a player companion, bumps the record revision without
// writing the bags (bind_player_to_scripts), so the old cells stay in the
// previous host and must not travel.
struct BagCarryGuard {
  bool player_record = false;
  bool starting_items_pending = false;
  std::uint64_t inventory_revision = 0, player_revision = 0;
  bool holds() const {
    return player_record && !starting_items_pending && inventory_revision == player_revision;
  }
};

// What an act transfer needs from the session.
struct ActCarry {
  // BagCarryGuard::holds() for this entry. Only then did carry_live_bags run.
  bool bags_carried = false;
  // The front end's save latch, Flow::save_latch_ (frontend.hpp, 0x4f8f28),
  // forwarded by the session; -1 when the session has no reading.
  int card_latch = -1;
};

// The one record copy across level hosts, run by the session under
// BagCarryGuard: slot 0's raw bags, in-record equipment pointers, the
// weapon-set byte and the weapon words. The campaign's name/flag projection
// cannot reconstruct them.
inline void carry_live_bags(Host &next, Host &previous) {
  const auto &old_memory = previous.variables().memory;
  auto &new_memory = next.variables().memory;
  const auto base = script_items::rec(0);
  for (const auto span : {std::pair<unsigned, unsigned>{0x130, 3 * 0x1208},
                          {0x43f0, 0x50}, {0x4a7c, 1}, {0x4a84, 4}})
    new_memory.write(base + span.first, old_memory.read(base + span.first, span.second));
}

// Live cells in slot 0's three bags; -1 when a count is outside 0..64.
inline int live_cell_count(const builtins::Memory &memory) {
  int cells = 0;
  for (int bag = 0; bag < script_items::BAGS; ++bag) {
    const auto count = memory.s32(script_items::bag_count_at(0, bag));
    if (count < 0 || count > script_items::BAG_SLOTS)
      return -1;
    cells += count;
  }
  return cells;
}

// The client owns one local player. Carried live cells travel and an act
// change removes unkept flag-39 cells through inventory_removeItem
// (inventory-reset.md:65-84). Without the session's bag-carry guard the
// previous host's cells are not this character's, so any live cell refuses
// exactly as wave 37 did. With it, the cells are classified in `bags` (the
// destination memory after carry_live_bags; the previous host's memory for a
// preflight) and an unclassified cell refuses. Campaign-model bags, an
// imported record, more than one local player, and a missing latch reading
// also refuse.
inline int require_campaign_act_transfer(Host &previous, const quests::ActTables &tables,
                                         const std::string &destination, bool imported,
                                         const ActCarry &carry,
                                         const builtins::Memory *bags = nullptr) {
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
  if (carry.card_latch != 0 && carry.card_latch != 1)
    throw std::runtime_error("Act transfer is refused: the front end's card latch 0x4f8f28 has no reading, so whether the act-change clear runs is unknown.");
  for (int bag = 0; bag < script_items::BAGS; ++bag)
    if (!previous.campaign().record(0).bags[std::size_t(bag)].empty())
      throw std::runtime_error("Act transfer with campaign-model bags is refused: they have no equipment pointers to slide.");
  if (!carry.bags_carried) {
    if (live_cell_count(memory) != 0)
      throw std::runtime_error("Act transfer with inventory is refused: live bag carry and flag-39 migration are unported.");
    return next;
  }
  const auto unclassified = campaign_inventory::unclassified_cell(bags ? *bags : memory, 1);
  if (!unclassified.empty())
    throw std::runtime_error("Act transfer with inventory is refused: " + unclassified + ".");
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
  int carried_cells = 0;  // slot 0's live cells in the destination before the change
  int removed = 0;        // unkept flag-39 cells the act-change clear removed
  std::uint32_t mark = 0; // the destination's word at 0x4f7118 after entry
  bool items_crossed_act = false;
};
// Normal offline level entry. The existing native change_level contract
// clears quests/variable values and preserves mission/medal/player records.
// The newly constructed host has no old world blob; an act reset discards
// that blob anyway. Other carried routes retain their existing policy.
// The session has already run carry_live_bags when carry.bags_carried.
inline CampaignEntry prepare_campaign_entry(Host &next, Host *previous,
                                             const quests::ActTables &tables,
                                             const std::string &level, bool mission2,
                                             bool imported, const ActCarry &carry,
                                             bool death_task = false) {
  CampaignEntry out;
  if (previous && campaign_entry_requires_transfer(*previous, tables, level, mission2)) {
    const auto destination =
        require_campaign_act_transfer(*previous, tables, level, imported, carry, &next.variables().memory);
    out.carried_cells = carry.bags_carried ? live_cell_count(next.variables().memory) : 0;
    next.campaign().set_acts(tables.names);
    next.level().acts = tables.flow;
    next.level().act = previous->campaign().act();
    next.level().pending.request(level);
    next.variables().memory.w32(campaign_inventory::FRONTEND_WORD,
                                previous->variables().memory.u32(campaign_inventory::FRONTEND_WORD));
    // inventory-reset.md:87-90 reads the latch at 0x4f8f28. The client's
    // latch is the front end's Flow::save_latch_; mirror it there.
    next.variables().memory.w8(campaign_inventory::CARD_LATCH, std::uint8_t(carry.card_latch));
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
    out.removed = next.act_change_removed();
    next.set_items_crossed_act(previous->items_crossed_act() || (change.act_changed && out.carried_cells > 0));
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
    // A carried same-act entry stays on the chain an act transfer with items
    // started; an entry without a previous host starts none.
    next.set_items_crossed_act(previous && previous->items_crossed_act());
  }
  out.act = next.campaign().act();
  out.mark = next.variables().memory.u32(campaign_inventory::ACT_MARK);
  out.items_crossed_act = next.items_crossed_act();
  return out;
}
} // namespace champions::simulation::rta::script_commands
