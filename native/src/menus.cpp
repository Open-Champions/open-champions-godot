#include "champions/menus.hpp"

#include "champions/items.hpp"

#include <algorithm>
#include <cstring>
#include <variant>

namespace champions::menus {
namespace items = champions::simulation::rta::items;
namespace msg = champions::messages::rta;
namespace {

std::int32_t trunc_div(int value, int divisor) {
  if (value < 0)
    return -((-value) / divisor);
  return value / divisor;
}

void put_u32(std::vector<std::uint8_t> &out, std::uint32_t value) {
  for (int i = 0; i < 4; ++i)
    out.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
}
void put_s32(std::vector<std::uint8_t> &out, std::int32_t value) {
  put_u32(out, static_cast<std::uint32_t>(value));
}
std::uint32_t take_u32(const std::vector<std::uint8_t> &in, std::size_t &at) {
  if (at + 4 > in.size())
    throw Refusal("menu save is truncated");
  std::uint32_t value = 0;
  for (int i = 0; i < 4; ++i)
    value |= static_cast<std::uint32_t>(in[at + static_cast<std::size_t>(i)]) << (8 * i);
  at += 4;
  return value;
}
std::int32_t take_s32(const std::vector<std::uint8_t> &in, std::size_t &at) {
  return items::s32(take_u32(in, at));
}

constexpr std::uint32_t NAME_BLADE = 0x3f7000;
constexpr std::uint32_t NAME_CHARM = 0x3f7010;
constexpr std::uint32_t NAME_STOCK = 0x3f7020;
constexpr std::uint32_t OBJECT = 0x700000;
constexpr int EQUIP_SLOTS = 20;
} // namespace

const char *action_name(Action action) {
  switch (action) {
  case Action::Buy: return "buy";
  case Action::Sell: return "sell";
  case Action::ConfirmBox: return "confirm";
  case Action::Equip: return "equip";
  case Action::Unequip: return "unequip";
  case Action::Drop: return "drop";
  case Action::Drag: return "drag";
  case Action::Workshop: return "workshop";
  case Action::None: return "none";
  }
  return "none";
}

std::int32_t rank_cost(int param, int level) {
  const auto p = items::s8(static_cast<std::uint32_t>(param));
  const auto lv = items::s32(static_cast<std::uint32_t>(level));
  if (p < 5) {
    const auto wide = static_cast<std::int64_t>(p) * (static_cast<std::int64_t>(lv) + 1);
    return items::s32(static_cast<std::uint32_t>(wide));
  }
  const auto powered = items::powf(items::fint(static_cast<std::uint32_t>(lv)), RANK_EXPONENT);
  const auto sum = static_cast<std::int64_t>(p) + items::ftrunc(powered);
  return items::s32(static_cast<std::uint32_t>(sum));
}

std::uint32_t cell_of(std::uint32_t record, int bag, int index) {
  return (record + BAG_ITEMS + static_cast<std::uint32_t>(bag) * BAG_STRIDE +
          static_cast<std::uint32_t>(index) * ITEM_SIZE) & 0xffffffffu;
}

std::int32_t item_ptr_to_index(std::uint32_t item, std::uint32_t record, Counts counts) {
  item &= 0xffffffffu;
  if (item == 0)
    return -1;
  for (int bag = 0; bag < BAGS; ++bag) {
    if (counts.n[bag] <= 0)
      continue;
    for (int index = 0; index < counts.n[bag]; ++index)
      if (cell_of(record, bag, index) == item)
        return bag * INDEX_BASE + index;
  }
  return -1;
}

std::uint32_t item_index_to_ptr(std::int32_t index, std::uint32_t record, Counts counts) {
  index = items::s32(static_cast<std::uint32_t>(index));
  if (index == -1)
    return 0;
  for (int bag = 0; bag < BAGS; ++bag) {
    if (counts.n[bag] <= 0)
      continue;
    for (int i = 0; i < counts.n[bag]; ++i)
      if (bag * INDEX_BASE + i == index)
        return cell_of(record, bag, i);
  }
  return 0;
}

std::pair<int, int> cursor_cell(int panel_x, int cursor_x, int cursor_y, int tex_height,
                                int scroll) {
  const int column = trunc_div(cursor_x - panel_x - CURSOR_X_BIAS, CURSOR_COL);
  const int half = tex_height / 2;
  const int row_top = cursor_y + (-half + 0x8c) + scroll - 229;
  return {column, trunc_div(row_top, CURSOR_ROW)};
}

std::uint32_t grid_address(std::uint32_t base, int column, int row) {
  if (column < 0 || row < 0 || column >= GRID_COLS || row >= GRID_ROWS)
    return 0;
  return (base + static_cast<std::uint32_t>(column) * GRID_COL +
          static_cast<std::uint32_t>(row) * GRID_ROW) &
         0xffffffffu;
}
std::uint32_t page_grid_address(std::uint32_t base, int page, int column, int row) {
  if (page < 0 || page >= BAGS) return 0;
  return grid_address(base + static_cast<std::uint32_t>(page) * PAGE_BYTES, column, row);
}
std::uint32_t first_cell_bag_address(std::uint32_t record, int page) {
  if (page < 0 || page >= BAGS) return 0;
  return record + BAG_ITEMS + static_cast<std::uint32_t>(page) * BAG_STRIDE;
}

Action item_action(int mode, std::uint32_t edge, bool equipped, bool holdable, Masks masks) {
  const bool shop = (edge & masks.shop) != 0;
  const bool equip = (edge & masks.equip) != 0;
  const bool drop = (edge & masks.drop) != 0;
  if (mode == 3)
    return Action::Workshop;
  if (mode == 0)
    return shop ? Action::Buy : Action::None;
  if (mode == 1) {
    if (shop)
      return equipped ? Action::ConfirmBox : Action::Sell;
    if (equip)
      return equipped ? Action::Unequip : Action::Equip;
    return Action::None;
  }
  if (mode == 2) {
    if (equip) {
      if (holdable)
        return Action::Drag;
      return equipped ? Action::Unequip : Action::Equip;
    }
    if (drop)
      return Action::Drop;
    return Action::None;
  }
  return Action::None;
}

std::uint32_t inventory_grid_tint(bool equipped, bool blocked, bool flag41) {
  if (equipped) return 0x20008000u;
  if (blocked && !flag41) return 0x20000080u;
  return 0x20445558u;
}

Strip inventory_strip(int panel_x) {
  return {panel_x + 15, 413, panel_x + 305, 414, 4};
}
Strip inventory_strip_above(int panel_x) {
  auto strip = inventory_strip(panel_x);
  strip.y0 -= 1;
  strip.y1 -= 1;
  return strip;
}
std::pair<int, int> shop_choice_rows() { return {203, 243}; }
StatsHeader stats_header() { return {}; }

std::vector<TextPos> stats_lines() {
  return {{160, 107, true, false, true}, {160, 129, true, true, false},
          {81, 208, false, true, false}, {143, 208, true, true, false},
          {81, 229, false, true, false}, {143, 229, true, true, false},
          {85, 306, false, true, false}, {163, 306, true, false, true}};
}

SkillLayout skill_layout(int origin) {
  SkillLayout layout;
  layout.header_centre_x = origin + 53;
  layout.header_x = origin + 70;
  layout.clip_x = origin;
  layout.clip2_x = origin;
  for (int &x : layout.node_x)
    x += origin;
  return layout;
}

PauseStyle pause_style() { return {}; }
ShopMark shop_mark() { return {}; }

std::uint32_t stored_mask(int scheme_byte, std::uint32_t address) {
  struct Word { std::uint32_t address, byte1, byte0; bool byte0_writes; };
  static constexpr Word words[] = {
      {0x41b960, 0x40, 0x20, true},   {0x41b964, 0x7e, 0x7b, true},
      {0x41b968, 0x10, 0x80, true},   {0x41b96c, 0x7c, 0x7c, true},
      {0x41b970, 0x80, 0, false},     {0x41b974, 0x7d, 0, false},
      {0x41b978, 0x7e, 0x7b, true},   {0x41b97c, 0x7b, 0x7e, true},
      {0x41bfb8, 0x20, 0x140, true},  {0x41bfbc, 0x40, 0x20, true},
  };
  // Byte 0's init leaves 0x41b974 at whatever was already there. It is not in
  // the executed byte-0 expectation, so it is not reported as a stored value.
  if (scheme_byte != 0 && scheme_byte != 1)
    throw Refusal("button scheme byte is 0 or 1");
  for (const auto &word : words) {
    if (word.address != address)
      continue;
    if (scheme_byte == 0 && !word.byte0_writes)
      throw Refusal("button-scheme-writer: this word is not stored for scheme byte 0");
    return scheme_byte == 1 ? word.byte1 : word.byte0;
  }
  throw Refusal("button mask address is not one of the ten init words");
}

void read_wide_mask_as_button(std::uint32_t word) {
  if (word >= 0x7b && word <= 0x7e)
    throw Refusal("wide-mask-words: 0x7b through 0x7e are wider than a pad bit and have no reader here");
}

Hit hit_cell(int panel_x, int column, int row, int tex_height, int scroll) {
  const int half = tex_height / 2;
  const int y0 = row * CURSOR_ROW - (-half + 0x8c) - scroll + 229;
  const int x0 = panel_x + CURSOR_X_BIAS + column * CURSOR_COL;
  return {x0, y0, x0 + CURSOR_COL, y0 + CURSOR_ROW};
}

int skill_points_for_level(int level_before) {
  if (level_before < 6)
    return 1;
  if (level_before < 16)
    return 2;
  if (level_before < 26)
    return 3;
  if (level_before < 36)
    return 4;
  return 5;
}

std::vector<Gap> gaps() {
  return {
      {"item-info-frame",
       "One category-dependent frame submission was executed; its second call site and full localized fields remain unexecuted."},
      {"stat-spend-input",
       "The edge bit and cursor value that reach the four stat-point sends were not executed."},
      {"skill-confirm-input",
       "The skill screen was not executed through to invmsg_sendSkillPoint. The handler spends one point and ignores the displayed cost."},
      {"pause-frames", "Pause-menu frame positions were not executed."},
      {"memory-card-frames", "Memory-card browser frame positions were not executed."},
      {"button-scheme-writer", "The writer of the byte at 0x41b69c was not found."},
      {"wide-mask-words", "The 0x7b-0x7e words stored beside the button masks have no reader here."},
      {"drop-seed-draw",
       "Whether 0x14a928 draws g_randSeed on the way to a drop was not executed. The prop search stays the handler's world_findFreeSpot."},
      {"drag-copy", "A dragged cell is not copied onto another bag cell."},
      {"skill-tree-other-classes", "Skill-tree coordinates are the class-0 rank-0 walk only."},
      {"skill-tree-node-pairs", "The 17 line calls were not recorded as endpoint pairs."},
      {"hotkey-from-skills", "This screen does not send the HUD spell-assign message."},
      {"shop-stock-generation", "Stock generation was not run again in the menu research."},
      {"shop-choice-centre-x", "Shop choice rows have a baseline y. Their centre x was not in the checked arguments."},
      {"workshop-body", "Mode 3 hands the cell to workshop_input. That body is not in this contract."},
      {"confirm-box-geometry", "An equipped sell opens hud_confirmBox. Its frame was not executed."},
      {"exp-threshold-table", "Experience thresholds at 0x41b558 are not in this contract. Level award applies the documented point bands only."},
      {"mana-refill-offset", "The mana field used by level-up refill is marked inferred, so grant_level refills hit points only."},
      {"stats-frame-origin", "The stats frame is 0x124 by 0x197. Its x and y were not recovered."},
      {"analog-cursor-step", "The pointer writes the free-cursor position. itemList_analogCursor's step was not in this contract."},
      {"menu-open-start-bit", "Opening the inventory is start-without-select. The start bit value was not in this contract."},
      {"page-bag-stride", "A page adds 0xbdd8 to the grid. That is not the bag stride 0x1208, so another page is not treated as another bag."},
      {"inventory-underbar", "INV-F1: the layer-5 texture argument is not established; the underbar sprite is not drawn."},
      {"inventory-box-variants", "Only the reset zero-state 15-piece sequence is executed; other cosmetic states are refused."},
      {"inventory-localized-fields", "The original text content and modifier/gem/requirement fields are unknown."},
      {"inventory-icon-pulse", "The selected icon uses the observed frame-zero alpha; the running frame source is not wired."},
      {"inventory-stat-input", "The stat-point edge and cursor gate are unknown."},
      {"inventory-live-parity", "No original running-game pixel comparison has been made."},
      {"inventory-live-drop", "World placement and item prop construction are not wired to the imported player record."},
  };
}

void refuse(const char *what) { throw Refusal(what); }

Ledger::Ledger() {
  auto add = [&](std::uint32_t addr, const char *name, int price, int slot) {
    auto def = std::make_shared<items::Definition>();
    def->addr = addr;
    def->name = name;
    def->price = price;
    def->slot = slot;
    def->category = 0;
    tables_.defs.push_back(def);
  };
  add(items::A_DEFS, "blade", 10, 0);
  add(items::A_DEFS + 64, "charm", 10, 1);
  add(items::A_DEFS + 128, "stock", 100, 0);
  tables_.circlet_stem_equip = "authored-circlet";
  callbacks_.result_service = [](const std::string &, const std::vector<msg::Argument> &)
      -> std::variant<std::uint64_t, msg::Position> {
    return msg::Position{1, 2, 3};
  };
  callbacks_.typed_service = [](const std::string &name, const std::vector<msg::Argument> &) -> std::uint64_t {
    if (name == "pool_new")
      return 0x600000;
    return 0;
  };
  machine_ = std::make_unique<Machine>(memory_, callbacks_, &tables_);
  machine_->register_all();
  seed();
}

void Ledger::write_item(std::uint32_t addr, std::uint32_t definition, int count,
                        std::uint64_t flags) {
  machine_->w32(addr, definition);
  machine_->w16(addr + 4, static_cast<std::uint64_t>(count));
  machine_->w64(addr + 8, flags);
  for (unsigned i = 0; i < 4; ++i)
    machine_->w16(addr + 0x24 + 8 * i, 0xffff);
}

void Ledger::seed() {
  auto &m = *machine_;
  const auto r = record();
  auto bytes = [](std::uint32_t addr, const char *text, Machine &machine) {
    for (std::size_t i = 0; text[i]; ++i)
      machine.w8(addr + static_cast<std::uint32_t>(i), static_cast<unsigned char>(text[i]));
    machine.w8(addr + static_cast<std::uint32_t>(std::strlen(text)), 0);
  };
  bytes(NAME_BLADE, "blade", m);
  bytes(NAME_CHARM, "charm", m);
  bytes(NAME_STOCK, "stock", m);
  m.w32(items::A_DEFS, NAME_BLADE);
  m.w8(items::A_DEFS + 0x1c, 0);
  m.w8(items::A_DEFS + 0x1d, 0);
  m.w32(items::A_DEFS + 64, NAME_CHARM);
  m.w8(items::A_DEFS + 64 + 0x1c, 1);
  m.w8(items::A_DEFS + 64 + 0x1d, 0);
  m.w32(items::A_DEFS + 128, NAME_STOCK);
  m.w8(items::A_DEFS + 128 + 0x1c, 0);
  m.w8(items::A_DEFS + 128 + 0x1d, 0);
  // Non-empty sentinels so an empty retail stem does not match every name.
  m.w8(0x5385e0, 'x');
  m.w8(0x538660, 'x');
  m.w32(0x4d8ae0, 1);
  m.w32(r + 0x43d8, OBJECT);
  m.w16(r + 0x4b08, 0xffff);
  m.w32(r + 0xe4, 500);
  m.w32(r + 0xe8, 4);
  m.w8(r + 0xec, 3);
  m.w8(r + 0x59, 5);
  m.w8(r + 0x58, 4);
  m.w32(r + 0xac, 0x3f800000);
  m.w32(r + 0xb0, 0x40000000);
  for (int i = 0; i < 4; ++i)
    m.w32(r + 0x78 + static_cast<std::uint32_t>(i) * 4, static_cast<std::uint32_t>(10 + i));
  m.w32(r + BAG_COUNT, 2);
  write_item(r + BAG_ITEMS, items::A_DEFS, 1, 0);
  write_item(r + BAG_ITEMS + ITEM_SIZE, items::A_DEFS + 64, 1, 1ull << 35);
  write_item(stock_addr_, items::A_DEFS + 128, 1, 0);
  m.w32(0x868078, 1);
  // Class-0 row 0: param 5, passive mask 1, current passive level 2.
  m.w8(CLASS_ROW + 2, 5);
  m.w64(CLASS_ROW + 8, 1);
  m.w8(r + 0xef, 2);
  m.w32(PANEL_ADDR + 0x34, 0);
}

void Ledger::set_scheme(Masks masks) { masks_ = masks; }
void Ledger::set_cursor(int x, int y) {
  cursor_x_ = x;
  cursor_y_ = y;
}
void Ledger::set_page(int page) {
  if (page < 0 || page > 5)
    throw Refusal("item panel page is 0..5");
  machine_->w32(PANEL_ADDR + 0x50, static_cast<std::uint32_t>(page));
}
int Ledger::page() const { return machine_->s32(PANEL_ADDR + 0x50); }

Counts Ledger::counts() const {
  Counts out;
  for (int bag = 0; bag < BAGS; ++bag)
    out.n[bag] = machine_->s32(record() + BAG_COUNT + static_cast<std::uint32_t>(bag) * BAG_STRIDE);
  return out;
}

std::string Ledger::picture_name(std::uint32_t addr) {
  const auto definition = tables_.by_addr(machine_->u32(addr));
  if (!definition) throw Refusal("item picture definition is absent");
  champions::inventory_icons::PictureItem picture;
  picture.definition.name = definition->name;
  picture.definition.category = definition->category;
  picture.definition.slot = definition->slot;
  picture.definition.subtype = definition->subtype;
  picture.definition.damage_min = definition->damage_min;
  picture.definition.damage_max = definition->damage_max;
  picture.definition.picture = definition->picture;
  picture.definition.model_row = static_cast<std::size_t>(definition->model_row);
  picture.flags = machine_->u64(addr + 8);
  picture.prefix = machine_->u8(addr + 0x20);
  std::optional<std::int32_t> value;
  if (definition->category == 1 && definition->name.find('+') != std::string::npos)
    value = machine_->item_value(addr);
  return champions::inventory_icons::picture_filename(
      picture, machine_->s32(record() + 0x54), pictures_, value);
}

std::array<std::vector<std::string>, BAGS> Ledger::bag_picture_names() {
  std::array<std::vector<std::string>, BAGS> names;
  const auto counted = counts();
  for (int bag = 0; bag < BAGS; ++bag) {
    if (counted.n[bag] < 0 || counted.n[bag] > 64)
      throw Refusal("bag count is outside the bounded display builder");
    for (int slot = 0; slot < counted.n[bag]; ++slot) {
      try {
        names[bag].push_back(picture_name(cell_of(record(), bag, slot)));
      } catch (const std::exception &) {
        names[bag].emplace_back();
      }
    }
  }
  return names;
}

void Ledger::build_display_grid(const std::array<std::vector<IconSize>, BAGS> &sizes) {
  const auto counted = counts();
  for (int bag = 0; bag < BAGS; ++bag) {
    auto &cells = display_cells_[bag];
    cells.assign(GRID_COLS * GRID_ROWS, {});
    auto &refusal = display_refusals_[bag];
    refusal.clear();
    display_refusal_cells_[bag] = {-1, -1};
    if (counted.n[bag] < 0 || counted.n[bag] > 64 ||
        sizes[bag].size() != static_cast<std::size_t>(counted.n[bag])) {
      refusal = "Display-grid dimensions are unavailable.";
      continue;
    }
    for (int slot = 0; slot < counted.n[bag]; ++slot) {
      const auto size = sizes[bag][static_cast<std::size_t>(slot)];
      // The executed builder fixture supplied only widths 0/32/64, height 0,
      // and a forced one-row rounding result. A runtime positive height has
      // no verified dimension-to-row conversion, even when it looks small.
      if (!size.verified_one_row ||
          (size.width != 0 && size.width != CURSOR_COL &&
           size.width != 2 * CURSOR_COL) || size.height != 0) {
        refusal = "Unverified item footprint refused; later placements depend on it.";
        break;
      }
      const int columns = size.width / CURSOR_COL + 1;
      if (columns < 1 || columns > GRID_COLS) {
        refusal = "Oversized item footprint refused; later placements depend on it.";
        break;
      }
      bool placed = false;
      for (int row = 0; row < GRID_ROWS && !placed; ++row) {
        for (int column = 0; column + columns <= GRID_COLS; ++column) {
          bool vacant = true;
          for (int offset = 0; offset < columns; ++offset)
            vacant &= cells[static_cast<std::size_t>((column + offset) * GRID_ROWS + row)].slot < 0;
          if (!vacant) continue;
          const auto addr = cell_of(record(), bag, slot);
          DisplayCell copy;
          copy.slot = slot;
          for (std::uint32_t byte = 0; byte < ITEM_SIZE; ++byte)
            copy.bytes[byte] = static_cast<std::uint8_t>(machine_->u8(addr + byte));
          copy.bytes[0x22] = static_cast<std::uint8_t>(slot);
          copy.bytes[0x23] = static_cast<std::uint8_t>(slot >> 8);
          for (int offset = 0; offset < columns; ++offset)
            cells[static_cast<std::size_t>((column + offset) * GRID_ROWS + row)] = copy;
          placed = true;
          break;
        }
      }
      if (!placed) {
        refusal = "Overflowing item footprint refused; later placements depend on it.";
        break;
      }
    }
    if (!refusal.empty()) {
      // This is a refusal marker in the first unused cell, not a claim that
      // the unknown item would have been placed at that coordinate.
      for (int row = 0; row < GRID_ROWS && display_refusal_cells_[bag].first < 0; ++row)
        for (int column = 0; column < GRID_COLS; ++column)
          if (cells[static_cast<std::size_t>(column * GRID_ROWS + row)].slot < 0) {
            display_refusal_cells_[bag] = {column, row};
            break;
          }
    }
  }
}

int Ledger::display_bag_slot(int page, int column, int row) const {
  if (page < 0 || page >= BAGS || column < 0 || column >= GRID_COLS ||
      row < 0 || row >= GRID_ROWS || display_cells_[page].empty())
    return -1;
  const auto &cell = display_cells_[page][static_cast<std::size_t>(column * GRID_ROWS + row)];
  if (cell.slot < 0) return -1;
  return int(cell.bytes[0x22]) | (int(cell.bytes[0x23]) << 8);
}

std::uint32_t Ledger::cell_address(int mode, int column, int row) const {
  if (page() >= 3)
    return 0;
  const auto base = mode == 0 ? stock_addr_ : record() + BAG_ITEMS;
  return page_grid_address(base, page(), column, row);
}

bool Ledger::bag_index(std::uint32_t addr, int &bag, int &index) const {
  const auto found = item_ptr_to_index(addr, record(), counts());
  if (found < 0)
    return false;
  bag = found / INDEX_BASE;
  index = found % INDEX_BASE;
  return true;
}

Action Ledger::press(int mode, std::uint32_t edge) {
  if (live_record_ && mode != 2)
    throw Refusal("live imported record supports inventory actions only");
  if (page() >= 3)
    return Action::None;
  const auto cell = cursor_cell(0, cursor_x_, cursor_y_);
  const auto hovered = cell_address(mode, cell.first, cell.second);
  auto addr = hovered;
  if (mode == 2 && page() > 0) {
    // The executed first-cell sender reads the counted bag at +0x1208 per
    // page, while hover reads the display grid at +0xbdd8 per page. Other
    // page-1/2 sender cells have not been executed.
    if (cell.first != 0 || cell.second != 0)
      throw Refusal("page-1/2 action is established only for the first cell");
    addr = first_cell_bag_address(record(), page());
  }
  const bool filled = addr != 0 && machine_->u32(addr) != 0;
  bool equipped = false;
  bool holdable = false;
  if (filled) {
    holdable = (machine_->u64(addr + 8) & (1ull << 35)) != 0;
    for (int slot = 0; slot < 17 && !equipped; ++slot)
      equipped = machine_->u32(record() + 0x43f0 + static_cast<std::uint32_t>(slot) * 4) == addr;
  }
  const auto action = item_action(mode, edge, equipped, holdable, masks_);
  if (!filled)
    return mode == 3 ? Action::Workshop : Action::None;
  if (action == Action::None || action == Action::Workshop || action == Action::ConfirmBox)
    return action;
  if (live_record_ && action == Action::Drop)
    throw Refusal("live drop needs the documented world placement and item prop service");
  if (action == Action::Drag) {
    dragging_ = 1;
    drag_cell_ = addr;
    drag_column_ = cell.first;
    drag_row_ = cell.second;
    machine_->w32(PANEL_ADDR + 0x58, 1);
    machine_->w32(PANEL_ADDR + 0x64, addr);
    machine_->w32(PANEL_ADDR + 0x68, static_cast<std::uint32_t>(cell.first));
    machine_->w32(PANEL_ADDR + 0x6c, static_cast<std::uint32_t>(cell.second));
    return action;
  }
  if (action == Action::Buy) {
    machine_->shop_buy(0, addr);
    return action;
  }
  int bag = 0, index = 0;
  if (!bag_index(addr, bag, index))
    throw Refusal("page-bag-stride: this grid cell is not a counted bag record");
  if (action == Action::Equip)
    machine_->equip_item(0, addr, 0);
  else if (action == Action::Unequip)
    machine_->unequip_item(0, addr);
  else if (action == Action::Drop)
    machine_->drop_item(0, index, bag, cell.first, cell.second, 0, 0);
  else if (action == Action::Sell)
    machine_->shop_sell(0, index, bag, cell.first, cell.second, 0, 1);
  return action;
}

void Ledger::spend_stat(int stat) { machine_->on_stat_point(0, stat); }

void Ledger::spend_skill(int is_skill, std::uint32_t value) {
  machine_->on_skill_point(0, is_skill, static_cast<std::int64_t>(value));
}

void Ledger::grant_level() {
  const auto r = record();
  const auto level = machine_->s8(static_cast<std::uint64_t>(r + 0x59));
  const auto points = skill_points_for_level(level);
  machine_->w32(r + 0xe8, static_cast<std::uint32_t>(machine_->s32(r + 0xe8) + points));
  const auto stat = items::s8(static_cast<std::uint32_t>(machine_->u8(r + 0xec)) + 3u);
  machine_->w8(r + 0xec, static_cast<std::uint8_t>(stat));
  machine_->w8(r + 0x59, static_cast<std::uint32_t>(items::s8(static_cast<std::uint32_t>(level + 1))));
  const auto hp = machine_->u32(r + 0xac);
  if (items::flt(0, hp))
    machine_->w32(r + 0xac, machine_->u32(r + 0xb0));
}

void Ledger::open_level_up() { machine_->on_join_panel(0, 0, 1); }

std::int32_t Ledger::gold() const { return machine_->s32(record() + 0xe4); }
std::int32_t Ledger::skill_points() const { return machine_->s32(record() + 0xe8); }
std::int32_t Ledger::stat_points() const { return machine_->s8(record() + 0xec); }
std::int32_t Ledger::level() const { return machine_->s8(record() + 0x59); }
std::int32_t Ledger::acknowledged() const { return machine_->s8(record() + 0x58); }
std::int32_t Ledger::base_stat(int index) const {
  if (index < 0 || index > 3)
    throw Refusal("stat index is 0..3");
  return machine_->s32(record() + 0x78 + static_cast<std::uint32_t>(index) * 4);
}
std::uint32_t Ledger::hit_points() const { return machine_->u32(record() + 0xac); }
std::uint32_t Ledger::max_hit_points() const { return machine_->u32(record() + 0xb0); }
int Ledger::bag_count(int bag) const {
  if (bag < 0 || bag >= BAGS)
    throw Refusal("bag is 0..2");
  return counts().n[bag];
}
std::uint32_t Ledger::equipment(int slot) const {
  if (slot < 0 || slot >= EQUIP_SLOTS)
    throw Refusal("equipment slot is outside the saved pointer set");
  const auto offset = slot < 17 ? 0x43f0 + slot * 4 : 0x4434 + (slot - 17) * 4;
  return machine_->u32(record() + static_cast<std::uint32_t>(offset));
}
std::uint32_t Ledger::record_word(std::uint32_t offset) const {
  if (offset > 0x4c30u - 4u || (offset & 3u) != 0)
    throw Refusal("player record word is outside the aligned record");
  return machine_->u32(record() + offset);
}
std::int32_t Ledger::equipment_index(int slot) const {
  return item_ptr_to_index(equipment(slot), record(), counts());
}
int Ledger::passive_level_at(std::uint64_t mask) {
  return machine_->passive_level(0, mask);
}

Frame Ledger::frame(const std::string &screen) {
  Frame out;
  out.screen = screen;
  out.gaps = gaps();
  const auto header = stats_header();
  const auto lines = stats_lines();
  auto text = [&](int x, int y, std::uint32_t colour, bool centred, std::string value) {
    out.texts.push_back({x, y, colour, centred, std::move(value)});
  };
  auto rect = [&](Hit hit, std::uint32_t colour, int layer) {
    out.rects.push_back({hit.x0, hit.y0, hit.x1, hit.y1, colour, layer});
  };
  if (screen == "inventory" && page() < 3) {
    // Bounded zero-state draw of itemList_draw. The general box variant
    // recurrence is not established. The underbar lookup is established, but
    // the audit did not establish its texture argument at the layer-5 draw.
    constexpr int panel_x = 80;
    constexpr int variants[15] = {3, 3, 2, 2, 1, 3, 3, 2, 1, 1, 1, 1, 3, 3, 2};
    for (int row = 0; row < 5; ++row) {
      for (int column = 0; column < 3; ++column)
        out.sprites.push_back({panel_x + 15 + 96 * column, 116 + 68 * row, 0,
                               0x80808080u, "inventry.lmp",
                               "inventrybox" + std::to_string(variants[row * 3 + column]) + ".tex"});
      out.sprites.push_back({panel_x + 303, 116 + 68 * row, 0, 0x80808080u,
                             "inventry.lmp", "inventryboxend.tex"});
    }
    for (int n = 0; n < 65; ++n) {
      const auto alpha = static_cast<std::uint32_t>(std::min(((2 * n + 1) * 127 + 64) / 128, 127));
      out.rects.push_back({panel_x + 15, 413 + n, panel_x + 305, 414 + n,
                           alpha << 24, 4});
    }
    // Pages 1/2 draw the retained display copies built from dimension-indexed
    // bag records. Their action pointer remains the separate counted bag cell.
    const auto selected = cursor_cell(0, cursor_x_, cursor_y_);
    if (page() > 0 && display_cells_[page()].empty())
      out.gaps.push_back({"inventory-page-grid", "Display-grid dimensions are unavailable."});
    if (page() > 0 && !display_refusals_[page()].empty())
      out.gaps.push_back({"inventory-grid-footprint", display_refusals_[page()],
                          display_refusal_cells_[page()].first,
                          display_refusal_cells_[page()].second});
    for (int row = 0; row < 10; ++row) {
      for (int column = 0; column < GRID_COLS; ++column) {
      const DisplayCell *copy = nullptr;
      if (page() > 0 && !display_cells_[page()].empty()) {
        const auto &cell = display_cells_[page()][static_cast<std::size_t>(column * GRID_ROWS + row)];
        if (cell.slot >= 0) copy = &cell;
      }
      const auto addr = page() == 0 ? cell_address(2, column, row)
                                    : (copy ? cell_of(record(), page(), copy->slot) : 0);
      int bag = 0, index = 0;
      if (!bag_index(addr, bag, index)) continue;
      const auto copy_word = [&](std::size_t offset) {
        std::uint32_t value = 0;
        for (int byte = 0; byte < 4; ++byte)
          value |= std::uint32_t(copy->bytes[offset + static_cast<std::size_t>(byte)]) << (8 * byte);
        return value;
      };
      const auto definition_word = copy ? copy_word(0) : machine_->u32(addr);
      if (definition_word == 0) continue;
      bool equipped = false;
      for (int slot = 0; slot < 17 && !equipped; ++slot)
        equipped = equipment(slot) == addr;
      const bool blocked = live_record_ && !equipped && machine_->check_equip_allowed(0, addr) != 0;
      const auto flags = copy ? std::uint64_t(copy_word(8)) | (std::uint64_t(copy_word(12)) << 32)
                              : machine_->u64(addr + 8);
      const bool flag41 = (flags & (1ull << 41)) != 0;
      const auto colour = inventory_grid_tint(equipped, blocked, flag41);
      const auto hit = hit_cell(panel_x, column, row);
      rect(hit, colour, 5);
      if (selected.first == column && selected.second == row) {
        const auto selected_definition = tables_.by_addr(definition_word);
        if (selected_definition && selected_definition->category >= 0 && selected_definition->category <= 2)
          out.frames.push_back({135, 230, 223, selected_definition->category == 0 ? 77 : 57,
                                6, 0x80000000u});
      }
      if (live_record_) {
        const auto definition = tables_.by_addr(definition_word);
        if (!definition) throw Refusal("live icon definition is absent");
        champions::inventory_icons::PictureItem picture;
        picture.definition.name = definition->name;
        picture.definition.category = definition->category;
        picture.definition.slot = definition->slot;
        picture.definition.subtype = definition->subtype;
        picture.definition.damage_min = definition->damage_min;
        picture.definition.damage_max = definition->damage_max;
        picture.definition.picture = definition->picture;
        picture.definition.model_row = static_cast<std::size_t>(definition->model_row);
        picture.flags = flags;
        picture.prefix = copy ? copy->bytes[0x20] : machine_->u8(addr + 0x20);
        const auto body = machine_->s32(record() + 0x54);
        try {
          std::optional<std::int32_t> value;
          if (definition->category == 1 && definition->name.find('+') != std::string::npos)
            value = machine_->item_value(addr);
          out.icons.push_back({column, row, definition->category, definition->slot,
                               selected.first == column && selected.second == row ? 0x8c8c8c8cu : 0x80808080u,
                               champions::inventory_icons::picture_filename(
                                   picture, body, pictures_, value)});
        } catch (const std::exception &) {
          out.gaps.push_back({"inventory-icon", "Picture association refused for an imported item."});
        }
      }
      std::string name = "item";
      const auto def = definition_word;
      for (const auto &row : tables_.defs)
        if (row && row->addr == def) name = row->name;
      const auto old_hit = hit_cell(0, column, row);
      if (!live_record_) {
        text(old_hit.x0, old_hit.y0, 0x80464646u, false, name);
        if (selected.first == column && selected.second == row)
          out.selected_texts.push_back({244, 229, 0x805c2913u, true, name});
      }
      }
    }
    // Both cursor copies use the observed resource, layer and tint. The host
    // cursor is relative to the bounded panel-x=80 fixture above.
    out.sprites.push_back({panel_x + cursor_x_, cursor_y_, 7, 0x80808080u,
                           "inventry.lmp", "mousecursor.tex"});
    out.sprites.push_back({panel_x + cursor_x_, cursor_y_, 7, 0x34000000u,
                           "inventry.lmp", "mousecursor.tex"});
    if (dragging_) {
      const auto definition = tables_.by_addr(machine_->u32(drag_cell_));
      if (live_record_ && definition) {
        champions::inventory_icons::PictureItem picture;
        picture.definition.name = definition->name;
        picture.definition.category = definition->category;
        picture.definition.slot = definition->slot;
        picture.definition.subtype = definition->subtype;
        picture.definition.damage_min = definition->damage_min;
        picture.definition.damage_max = definition->damage_max;
        picture.definition.picture = definition->picture;
        picture.definition.model_row = static_cast<std::size_t>(definition->model_row);
        picture.flags = machine_->u64(drag_cell_ + 8);
        picture.prefix = machine_->u8(drag_cell_ + 0x20);
        try {
          std::optional<std::int32_t> value;
          if (definition->category == 1 && definition->name.find('+') != std::string::npos)
            value = machine_->item_value(drag_cell_);
          const auto filename = champions::inventory_icons::picture_filename(
              picture, machine_->s32(record() + 0x54), pictures_, value);
          out.held_icons.push_back({panel_x + cursor_x_ + 22, cursor_y_ + 22,
                                    7, 0x80808080u, filename});
          out.held_icons.push_back({panel_x + cursor_x_ + 22, cursor_y_ + 25,
                                    7, 0x34000000u, filename});
        } catch (const std::exception &) {
          out.gaps.push_back({"inventory-drag-picture", "Held-item picture association refused."});
        }
      } else {
        out.gaps.push_back({"inventory-drag-picture", "Held-item picture is unavailable for this drag state."});
      }
    }
    return out;
  }
  if (screen == "shop") {
    const auto strip = inventory_strip(0);
    const auto above = inventory_strip_above(0);
    out.rects.push_back({strip.x0, strip.y0, strip.x1, strip.y1, 0x80808080u, strip.layer});
    out.rects.push_back({above.x0, above.y0, above.x1, above.y1, 0x80808080u, above.layer});
    const int mode = screen == "shop" ? 0 : 2;
    const auto listed = screen == "shop" ? 1 : bag_count(0);
    for (int index = 0; index < listed; ++index) {
      const auto addr = cell_address(mode, 0, index);
      if (addr == 0 || machine_->u32(addr) == 0)
        continue;
      const auto hit = hit_cell(0, 0, index);
      rect(hit, 0x80808080u, 7);
      std::string name = "item";
      const auto def = machine_->u32(addr);
      for (const auto &row : tables_.defs)
        if (row && row->addr == def)
          name = row->name;
      if (equipment_index(0) == index && screen == "inventory")
        name += " eq";
      text(hit.x0, hit.y0, header.colour, false, name);
    }
    if (dragging_) {
      const Hit drag{cursor_x_, cursor_y_, cursor_x_ + CURSOR_COL, cursor_y_ + CURSOR_ROW};
      rect(drag, 0x34000000u, 7);
    }
  }
  if (screen == "stats" || screen == "level-up" || (screen == "inventory" && page() >= 3)) {
    // Eight executed submissions, in that order. The original strings were not
    // captured, so these are the live level, gold and stat indices 0..2.
    const std::string shown[] = {
        std::to_string(level()),        std::to_string(gold()),
        "0",                            std::to_string(base_stat(0)),
        "1",                            std::to_string(base_stat(1)),
        "2",                            std::to_string(base_stat(2)),
    };
    for (std::size_t i = 0; i < lines.size(); ++i)
      text(lines[i].x, lines[i].y, header.colour, lines[i].centred, shown[i]);
  }
  if (screen == "skills") {
    const auto layout = skill_layout(0);
    const auto param = machine_->s8(CLASS_ROW + 2);
    const auto cost = rank_cost(param, passive_level_at(1));
    text(layout.header_x, layout.header_y, layout.colour, false,
         std::to_string(skill_points()) + " cost " + std::to_string(cost));
  }
  if (screen == "save")
    text(17, 89, header.colour, false, "index " + std::to_string(equipment_index(0)));
  return out;
}

void Ledger::adopt(const Adopted &c) {
  if (c.level < 1 || c.level > 127 || c.acknowledged < 0 || c.acknowledged > 127)
    throw Refusal("level does not fit the s8 level bytes");
  if (c.stat_points < 0 || c.stat_points > 255)
    throw Refusal("stat points do not fit byte +0xec");
  const auto r = record();
  for (int bag = 0; bag < BAGS; ++bag)
    machine_->w32(r + BAG_COUNT + static_cast<std::uint32_t>(bag) * BAG_STRIDE, 0);
  for (int slot = 0; slot < EQUIP_SLOTS; ++slot) {
    const auto offset = slot < 17 ? 0x43f0 + slot * 4 : 0x4434 + (slot - 17) * 4;
    machine_->w32(r + static_cast<std::uint32_t>(offset), 0);
  }
  dragging_ = 0;
  drag_cell_ = 0;
  live_record_ = false;
  for (auto &cells : display_cells_) cells.clear();
  for (auto &reason : display_refusals_) reason.clear();
  machine_->w32(r + 0xe4, static_cast<std::uint32_t>(c.gold));
  machine_->w32(r + 0xe8, static_cast<std::uint32_t>(c.skill_points));
  machine_->w8(r + 0xec, static_cast<std::uint32_t>(c.stat_points));
  machine_->w8(r + 0x59, static_cast<std::uint32_t>(c.level));
  machine_->w8(r + 0x58, static_cast<std::uint32_t>(c.acknowledged));
  machine_->w32(r + 0x5c, static_cast<std::uint32_t>(c.experience));
  for (int i = 0; i < 4; ++i)
    machine_->w32(r + 0x78 + static_cast<std::uint32_t>(i) * 4, static_cast<std::uint32_t>(c.stats[i]));
}

void Ledger::adopt_record(const std::vector<std::uint8_t> &bytes, items::Tables tables,
                          champions::inventory_icons::PictureTables pictures,
                          const std::function<std::uint8_t(std::uint32_t)> &image_byte) {
  if (bytes.size() != 0x4c30u || !image_byte)
    throw Refusal("live player record must be exactly 0x4c30 bytes with an executable reader");
  auto word = [&](std::size_t at) {
    return std::uint32_t(bytes[at]) | std::uint32_t(bytes[at + 1]) << 8 |
           std::uint32_t(bytes[at + 2]) << 16 | std::uint32_t(bytes[at + 3]) << 24;
  };
  for (int bag = 0; bag < BAGS; ++bag) {
    const auto count = word(BAG_COUNT + static_cast<std::size_t>(bag) * BAG_STRIDE);
    if (count > 64) throw Refusal("live bag count exceeds 64 records");
    for (std::uint32_t i = 0; i < count; ++i) {
      const auto at = BAG_ITEMS + static_cast<std::size_t>(bag) * BAG_STRIDE + i * ITEM_SIZE;
      if (!tables.by_addr(word(at)))
        throw Refusal("live item definition is absent from the selected executable");
    }
  }
  // Collect file-backed definition rows before changing the live ledger.
  std::vector<std::pair<std::uint32_t, std::vector<std::uint8_t>>> image_rows;
  for (const auto *rows : {&tables.defs, &tables.presets}) {
    for (const auto &definition : *rows) {
      if (!definition) continue;
      std::vector<std::uint8_t> row(0x40);
      for (std::uint32_t i = 0; i < 0x40; ++i) row[i] = image_byte(definition->addr + i);
      image_rows.emplace_back(definition->addr, std::move(row));
    }
  }
  tables_ = std::move(tables);
  pictures_ = std::move(pictures);
  for (const auto &[at, row] : image_rows)
    for (std::uint32_t i = 0; i < row.size(); ++i) machine_->w8(at + i, row[i]);
  // The existing passive-rank reader consults the file-backed class rows.
  for (std::uint32_t i = 0; i < CLASS_STRIDE * 7u; ++i)
    machine_->w8(CLASS_ROW + i, image_byte(CLASS_ROW + i));
  for (std::uint32_t i = 0; i < bytes.size(); ++i) machine_->w8(record() + i, bytes[i]);
  dragging_ = 0;
  drag_cell_ = 0;
  live_record_ = true;
  for (auto &cells : display_cells_) cells.clear();
  for (auto &reason : display_refusals_) reason.clear();
  set_page(0);
}

std::vector<std::uint8_t> Ledger::record_bytes() const {
  std::vector<std::uint8_t> out(0x4c30);
  for (std::uint32_t i = 0; i < out.size(); ++i)
    out[i] = static_cast<std::uint8_t>(machine_->u8(record() + i));
  return out;
}

std::vector<std::uint8_t> Ledger::save() const {
  std::vector<std::uint8_t> out;
  const char magic[] = "OCMENU1";
  out.insert(out.end(), magic, magic + 8);
  put_u32(out, 1);
  const auto r = record();
  put_s32(out, gold());
  put_s32(out, skill_points());
  put_s32(out, stat_points());
  put_s32(out, level());
  put_s32(out, acknowledged());
  put_s32(out, machine_->s32(r + 0x5c));
  for (int i = 0; i < 4; ++i)
    put_s32(out, base_stat(i));
  put_u32(out, hit_points());
  put_u32(out, max_hit_points());
  const auto counted = counts();
  for (int bag = 0; bag < BAGS; ++bag) {
    if (counted.n[bag] < 0 || counted.n[bag] > 64)
      throw Refusal("bag count is outside the 64-record bag");
    put_s32(out, counted.n[bag]);
  }
  for (int bag = 0; bag < BAGS; ++bag) {
    for (int index = 0; index < counted.n[bag]; ++index) {
      const auto addr = cell_of(r, bag, index);
      const auto item = machine_->item_object(addr);
      put_s32(out, items::encode_definition_index(tables_, item));
      for (unsigned b = 0; b < ITEM_SIZE; ++b)
        out.push_back(static_cast<std::uint8_t>(machine_->u32(addr + b) & 0xff));
    }
  }
  for (int slot = 0; slot < EQUIP_SLOTS; ++slot)
    put_s32(out, equipment_index(slot));
  put_s32(out, page());
  put_s32(out, cursor_x_);
  put_s32(out, cursor_y_);
  put_s32(out, dragging_);
  put_s32(out, dragging_ ? item_ptr_to_index(drag_cell_, r, counted) : -1);
  return out;
}

void Ledger::load(const std::vector<std::uint8_t> &bytes) {
  std::size_t at = 0;
  if (bytes.size() < 12 || std::memcmp(bytes.data(), "OCMENU1", 7) != 0)
    throw Refusal("menu save magic is not OCMENU1");
  at = 8;
  if (take_u32(bytes, at) != 1)
    throw Refusal("menu save version is not 1");
  const auto r = record();
  const auto gold_value = take_s32(bytes, at);
  const auto skills = take_s32(bytes, at);
  const auto stats = take_s32(bytes, at);
  const auto level_value = take_s32(bytes, at);
  const auto ack = take_s32(bytes, at);
  const auto exp = take_s32(bytes, at);
  std::int32_t bases[4];
  for (int i = 0; i < 4; ++i)
    bases[i] = take_s32(bytes, at);
  const auto hp = take_u32(bytes, at);
  const auto max_hp = take_u32(bytes, at);
  Counts counted;
  for (int bag = 0; bag < BAGS; ++bag) {
    counted.n[bag] = take_s32(bytes, at);
    if (counted.n[bag] < 0 || counted.n[bag] > 64)
      throw Refusal("bag count is outside the 64-record bag");
  }
  for (int bag = 0; bag < BAGS; ++bag)
    machine_->w32(r + BAG_COUNT + static_cast<std::uint32_t>(bag) * BAG_STRIDE,
                  static_cast<std::uint32_t>(counted.n[bag]));
  for (int bag = 0; bag < BAGS; ++bag) {
    for (int index = 0; index < counted.n[bag]; ++index) {
      const auto def_index = take_s32(bytes, at);
      const auto addr = cell_of(r, bag, index);
      if (at + ITEM_SIZE > bytes.size())
        throw Refusal("menu save is truncated");
      for (unsigned b = 0; b < ITEM_SIZE; ++b)
        machine_->w8(addr + b, bytes[at + b]);
      at += ITEM_SIZE;
      const auto def = items::decode_definition_index(tables_, def_index);
      if (!def)
        throw Refusal("saved definition index is not in the supplied tables");
      machine_->w32(addr, def->addr);
    }
  }
  for (int slot = 0; slot < EQUIP_SLOTS; ++slot) {
    const auto index = take_s32(bytes, at);
    const auto ptr = item_index_to_ptr(index, r, counted);
    const auto offset = slot < 17 ? 0x43f0 + slot * 4 : 0x4434 + (slot - 17) * 4;
    machine_->w32(r + static_cast<std::uint32_t>(offset), ptr);
  }
  set_page(take_s32(bytes, at));
  cursor_x_ = take_s32(bytes, at);
  cursor_y_ = take_s32(bytes, at);
  dragging_ = take_s32(bytes, at);
  drag_cell_ = item_index_to_ptr(take_s32(bytes, at), r, counted);
  machine_->w32(r + 0xe4, static_cast<std::uint32_t>(gold_value));
  machine_->w32(r + 0xe8, static_cast<std::uint32_t>(skills));
  machine_->w8(r + 0xec, static_cast<std::uint32_t>(stats));
  machine_->w8(r + 0x59, static_cast<std::uint32_t>(level_value));
  machine_->w8(r + 0x58, static_cast<std::uint32_t>(ack));
  machine_->w32(r + 0x5c, static_cast<std::uint32_t>(exp));
  for (int i = 0; i < 4; ++i)
    machine_->w32(r + 0x78 + static_cast<std::uint32_t>(i) * 4, static_cast<std::uint32_t>(bases[i]));
  machine_->w32(r + 0xac, hp);
  machine_->w32(r + 0xb0, max_hp);
  live_record_ = false;
  for (auto &cells : display_cells_) cells.clear();
  for (auto &reason : display_refusals_) reason.clear();
}

} // namespace champions::menus
