#pragma once
// Campaign bag carry between levels.
//
// Observed rules, research-inventory-reset ce2e086
// docs/inventory-reset.md:40-110 (audit research-inventory-reset-r2-fix.md PASS).
// Which act a level belongs to is not decided here: the client reads the act
// tables from the executable (quests::read_act_tables) and level flow's
// begin_change decides an act change; Host::change_level then runs this clear.
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

// Why a live cell cannot be kept or removed in this memory, or "" when every
// cell of every slot below players can. A count outside 0..64 or a zero
// definition word is not a classified item. A flag-39 cell also needs its
// definition's name: the keep decision is made on that name
// (docs/quests.md:163-169, inventory-reset.md:75-81), and no research says
// what happens when it cannot be read, so a zero name pointer or an empty
// name refuses instead of removing the cell.
std::string unclassified_cell(const builtins::Memory &memory, int players);

// The act-change clear, run by Host::change_level when level flow's reset_act
// is set: unkept flag-39 cells through inventory_removeItem, quest-list
// counts, and quest-variable values (names stay), then 1 at 0x4f7118. Checks
// every slot with unclassified_cell before the first edit and refuses then.
// Returns the number of removed cells.
int clear_act_change_inventory(builtins::Variables &variables);

// inventory_shareQuestItems 0x1c4fa8. Bag 2 flag-39 cells only. Copies go
// through inventory_giveItem and are not equipped. Flag bit 8 on a copied
// cell is Unknown (inventory-reset.md:109-111) and refuses before any edit.
void share_quest_items(builtins::Variables &variables, int source_slot);

// Why a saved record's bag 2 has a flag-39 cell this client will not copy,
// or "" when every such cell can be copied. A saved cell holds a definition
// row index (ps2_save.cpp encode_saved_record), so row 0 is a real item; a
// negative row has no definition. Flag bit 8 is Unknown
// (inventory-reset.md:109-111).
std::string share_copy_unexecuted(const std::vector<std::uint8_t> &record);
} // namespace champions::simulation::rta::campaign_inventory
