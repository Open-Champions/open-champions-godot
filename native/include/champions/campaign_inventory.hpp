#pragma once
// Campaign bag carry between levels.
//
// Observed rules, research-inventory-reset ce2e086
// docs/inventory-reset.md:40-110 (audit research-inventory-reset-r2-fix.md PASS).
// The act of each documented story name is research-story-route-r3 019a155
// docs/story-route.md:240-278.
//
// An act change removes unkept flag-39 cells through the client's
// inventory_removeItem, so equipment and weapon pointers follow that slide.
// The keep names are the list already executed in docs/quests.md:163-169 and
// cited at inventory-reset.md:76-81: a name containing "bloodstone" or
// "shardof", or exactly spiritrunes, emptybag, fullbag, pickaxe, twistedkey.
// No name outside that list is kept. The arena wipe player_removeAllItems
// 0x1bf600 is not this path: its only jal site is 0x304e9c, and this client
// does not enter that arena init.
#include "champions/quests.hpp"
#include "champions/script_builtins.hpp"
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace champions::simulation::rta::campaign_inventory {
inline constexpr std::uint32_t ACT_WORD = 0x88d3d4;       // stored act
inline constexpr std::uint32_t ACT_MARK = 0x4f7118;       // 1 after the clear
inline constexpr std::uint32_t FRONTEND_WORD = 0x41b510;  // loaded as a word
inline constexpr std::uint32_t CARD_LATCH = 0x4f8f28;
inline constexpr std::uint64_t FLAG_STACK = std::uint64_t(1) << 8;

struct Request {
  std::string pending;
  int act = -1;
  bool frontend = false;
  bool card_latch = false;
  bool death_task = false;
};

struct Result {
  int changed = 0;  // save_actChanged's result; 0 when the frontend skips the call
  int act = -1;     // stored act
  bool cleared = false;
  int removed = 0;
};

// docs/story-route.md:240-278. Prefixes only. hub is a prefix of hub2, and
// both are act 0, so hub2 from inside act 0 is not an act change.
std::vector<std::vector<std::string>> documented_story_acts();

// Same-act travel leaves the bags. An act change, including the first level
// from act -1, clears when the latch and the death task are clear. The
// frontend word skips the act call and the clear, and is not written.
Result apply_campaign_travel(builtins::Variables &variables,
                             const std::vector<std::vector<std::string>> &acts,
                             const Request &request);

// Bags, quest-list counts, and quest-variable values. Names of variables stay.
// Used by apply_campaign_travel and by Host::change_level when level flow's
// reset_act is set.
int clear_act_change_inventory(builtins::Variables &variables);

// inventory_shareQuestItems 0x1c4fa8. Bag 2 flag-39 cells only. Copies go
// through inventory_giveItem and are not equipped. Flag bit 8 on a copied
// cell is Unknown (inventory-reset.md:109-111) and refuses before any edit.
void share_quest_items(builtins::Variables &variables, int source_slot);

// True when a saved record's bag 2 has a flag-39 cell whose copy rule is not
// executed (flag bit 8, or a null definition).
bool share_copy_unexecuted(const std::vector<std::uint8_t> &record);
} // namespace champions::simulation::rta::campaign_inventory
