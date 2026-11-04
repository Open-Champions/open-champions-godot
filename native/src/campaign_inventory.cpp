#include "champions/campaign_inventory.hpp"
#include "champions/script_items.hpp"
#include <stdexcept>

namespace champions::simulation::rta::campaign_inventory {
namespace {
namespace si = script_items;
using builtins::Variables;

constexpr std::uint32_t QUEST_AT = 0x41d0;
constexpr std::uint32_t QUEST_COUNT = 0x43d0;
constexpr std::uint32_t QUEST_STRIDE = 0x40;
constexpr int QUEST_SLOTS = 8;
constexpr char PLACEHOLDER[] = "NoQuests";

std::uint32_t word_of(const Variables::InventoryResult &result) {
  const auto *word = std::get_if<std::uint32_t>(&result);
  if (!word)
    throw std::runtime_error("inventory service returned no word");
  return *word;
}

Variables::InventoryResult call_inventory(Variables &variables, const std::string &name,
                                          const std::vector<Variables::InventoryValue> &args) {
  if (!variables.inventory_service)
    throw std::runtime_error("inventory service is not bound");
  return variables.inventory_service(name, args);
}

bool flag39(std::uint64_t flags) { return ((flags >> quests::QUEST_ITEM_BIT) & 1u) != 0; }

// Address arithmetic from script_items.hpp. The out-of-line helpers live in
// champions-script-items, which this campaign library does not link.
std::uint32_t player_rec(int slot) {
  return si::G_PLAYERS + static_cast<std::uint32_t>(slot) * si::REC_SIZE;
}
std::uint32_t cell_at(int slot, int bag, int index) {
  return player_rec(slot) + si::P_BAGS + static_cast<std::uint32_t>(bag) * si::BAG_SIZE +
         static_cast<std::uint32_t>(index) * si::ITEM_SIZE;
}
std::uint32_t count_at(int slot, int bag) {
  return player_rec(slot) + si::P_BAGS + static_cast<std::uint32_t>(bag) * si::BAG_SIZE + si::BAG_COUNT;
}

// inventory-reset.md:75-82. The client's inventory_removeItem slides the tail
// and the equipment pointers. A null definition is not a classified cell.
int remove_unkept(Variables &variables, int slot) {
  auto &memory = variables.memory;
  int removed = 0;
  for (int bag = 0; bag < si::BAGS; ++bag) {
    int index = 0;
    for (;;) {
      const auto count = memory.s32(count_at(slot, bag));
      if (index >= count)
        break;
      if (count < 0 || count > si::BAG_SLOTS)
        throw std::runtime_error("bag count is outside 0..64");
      const auto cell = cell_at(slot, bag, index);
      const auto flags = memory.u64(cell + 8);
      if (!flag39(flags)) {
        ++index;
        continue;
      }
      const auto definition = memory.u32(cell);
      if (definition == 0)
        throw std::runtime_error("flag-39 cell has no definition; keep or drop is refused");
      const auto name = memory.cstr(memory.u32(definition));
      if (quests::quest_item_kept(name)) {
        ++index;
        continue;
      }
      word_of(call_inventory(variables, "inventory_removeItem",
                             {static_cast<std::uint32_t>(slot), static_cast<std::uint32_t>(index),
                              static_cast<std::uint32_t>(bag), 0u}));
      ++removed;
    }
  }
  return removed;
}

void overlay_placeholder(builtins::Memory &memory, std::uint32_t field) {
  for (int i = 0; i < 9; ++i)
    memory.byte(field + static_cast<std::uint32_t>(i), static_cast<std::uint8_t>(PLACEHOLDER[i]));
}

void clear_quest_lists(Variables &variables, int players) {
  auto &memory = variables.memory;
  for (int slot = 0; slot < players; ++slot) {
    const auto base = player_rec(slot);
    const auto count = memory.s32(base + QUEST_COUNT);
    if (count < 0 || count > QUEST_SLOTS)
      throw std::runtime_error("quest count is outside 0..8");
    for (int index = 0; index < count; ++index) {
      const auto field = base + QUEST_AT + static_cast<std::uint32_t>(index) * QUEST_STRIDE;
      overlay_placeholder(memory, field);
      overlay_placeholder(memory, field + 0x20);
    }
    memory.w32(base + QUEST_COUNT, 0);
  }
}

std::uint32_t raw_u32(const std::vector<std::uint8_t> &raw, std::size_t offset) {
  return std::uint32_t(raw[offset]) | (std::uint32_t(raw[offset + 1]) << 8) |
         (std::uint32_t(raw[offset + 2]) << 16) | (std::uint32_t(raw[offset + 3]) << 24);
}
std::uint64_t raw_u64(const std::vector<std::uint8_t> &raw, std::size_t offset) {
  return std::uint64_t(raw_u32(raw, offset)) | (std::uint64_t(raw_u32(raw, offset + 4)) << 32);
}
} // namespace

std::vector<std::vector<std::string>> documented_story_acts() {
  // docs/story-route.md:242-255 and :273-278. Mission 1 names and both hubs
  // are act 0. innovat1 and the rest of that chain are act 1. Each later
  // mission row is its own act. Evil names share the story act.
  return {
      {"hub", "hub2", "blood1", "strife", "drundar", "drunarm", "drunare"},
      {"innovat1", "inmine1", "inmine2", "innovat2"},
      {"islc1"},
      {"wat1"},
      {"tormenta"},
      {"swamp1", "swamp2"},
      {"valor"},
      {"fire1"},
      {"storm1"},
      {"fay2", "fay1"},
      {"nit1"},
      {"bossgood", "bossevil"},
  };
}

int clear_act_change_inventory(Variables &variables) {
  auto &memory = variables.memory;
  const auto players = memory.s32(si::G_NUM_PLAYERS);
  if (players < 0 || players > quests::PLAYERS)
    throw std::runtime_error("player count is outside 0..4");
  int removed = 0;
  for (int slot = 0; slot < players; ++slot)
    removed += remove_unkept(variables, slot);
  clear_quest_lists(variables, players);
  variables.quest_clear_all();
  memory.w32(ACT_MARK, 1u);
  return removed;
}

Result apply_campaign_travel(Variables &variables, const std::vector<std::vector<std::string>> &acts,
                             const Request &request) {
  Result result;
  result.act = request.act;
  // inventory-reset.md:59-63. A nonzero frontend word skips save_actChanged.
  // The delay slot makes the clear compare see 0. The word itself is not stored.
  if (request.frontend)
    return result;
  const auto found = quests::find_act(request.pending, request.act, acts);
  if (found >= 0 && found != request.act) {
    result.changed = 1;
    result.act = found;
    variables.memory.w32(ACT_WORD, static_cast<std::uint32_t>(found));
  }
  // inventory-reset.md:86-94. The latch and a running death task store the
  // new act and skip the clear. 0x4f7118 stays 0.
  result.cleared = result.changed != 0 && !request.card_latch && !request.death_task;
  if (result.cleared)
    result.removed = clear_act_change_inventory(variables);
  return result;
}

bool share_copy_unexecuted(const std::vector<std::uint8_t> &record) {
  constexpr std::size_t bag2 = 0x130 + 2 * 0x1208;
  constexpr std::size_t count_at = bag2 + 0x1200;
  if (record.size() < count_at + 4)
    return false;
  const auto count = static_cast<std::int32_t>(raw_u32(record, count_at));
  if (count < 0 || count > 64)
    return true;
  for (int index = 0; index < count; ++index) {
    const auto cell = bag2 + static_cast<std::size_t>(index) * 0x48;
    if (cell + 16 > record.size())
      return true;
    const auto flags = raw_u64(record, cell + 8);
    if (!flag39(flags))
      continue;
    if ((flags & FLAG_STACK) != 0 || raw_u32(record, cell) == 0)
      return true;
  }
  return false;
}

void share_quest_items(Variables &variables, int source_slot) {
  auto &memory = variables.memory;
  const auto players = memory.s32(si::G_NUM_PLAYERS);
  if (players < 0 || players > quests::PLAYERS)
    throw std::runtime_error("player count is outside 0..4");
  if (source_slot < 0 || source_slot >= players)
    throw std::runtime_error("share source is outside the player count");
  struct Captured {
    std::vector<std::uint8_t> bytes;
  };
  std::vector<Captured> captured;
  const auto source_count = memory.s32(count_at(source_slot, 2));
  if (source_count < 0 || source_count > si::BAG_SLOTS)
    throw std::runtime_error("bag count is outside 0..64");
  for (int index = 0; index < source_count; ++index) {
    const auto cell = cell_at(source_slot, 2, index);
    const auto flags = memory.u64(cell + 8);
    if (!flag39(flags))
      continue;
    // inventory-reset.md:109-111. Bit 8's stack rule was not re-run from this caller.
    if ((flags & FLAG_STACK) != 0)
      throw std::runtime_error("inventory_shareQuestItems flag bit 8 on a copied cell is Unknown");
    const auto definition = memory.u32(cell);
    if (definition == 0)
      throw std::runtime_error("shared flag-39 cell has no definition");
    if (memory.s8(definition + 0x1d) != 2)
      throw std::runtime_error("shared flag-39 cell category is not bag 2");
    captured.push_back({memory.read(cell, si::ITEM_SIZE)});
  }
  for (int slot = 0; slot < players; ++slot) {
    if (slot == source_slot)
      continue;
    int index = 0;
    for (;;) {
      const auto count = memory.s32(count_at(slot, 2));
      if (index >= count)
        break;
      if (count < 0 || count > si::BAG_SLOTS)
        throw std::runtime_error("bag count is outside 0..64");
      const auto cell = cell_at(slot, 2, index);
      if (!flag39(memory.u64(cell + 8))) {
        ++index;
        continue;
      }
      word_of(call_inventory(variables, "inventory_removeItem",
                             {static_cast<std::uint32_t>(slot), static_cast<std::uint32_t>(index), 2u, 0u}));
    }
    for (const auto &item : captured) {
      for (std::uint32_t byte = 0; byte < item.bytes.size(); ++byte)
        word_of(call_inventory(variables, "stack_record_store", {byte, std::uint32_t(item.bytes[byte])}));
      // Announce 0. The hint inside inventory_giveItem is gated on a retail
      // name this caller did not execute (inventory-reset.md:96-111).
      const auto index_word =
          word_of(call_inventory(variables, "inventory_giveItem", {static_cast<std::uint32_t>(slot), 0u}));
      if (index_word == 0xffffffffu)
        throw std::runtime_error("inventory_giveItem did not append the shared quest cell");
    }
  }
}
} // namespace champions::simulation::rta::campaign_inventory
