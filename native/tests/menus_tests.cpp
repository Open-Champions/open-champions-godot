#include "champions/menus.hpp"
#include "champions/items.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <unordered_map>
#include <vector>

using namespace champions::menus;

namespace champions::menus {
struct LedgerProbe {
  static std::uint32_t word(const Ledger &ledger, std::uint32_t offset) {
    return ledger.machine_->u32(ledger.machine_->u32(ledger.record() + 0x43d8) + offset);
  }
  static void record_word(Ledger &ledger, std::uint32_t offset, std::uint32_t value) {
    ledger.machine_->w32(ledger.record() + offset, value);
  }
  static void object_word(Ledger &ledger, std::uint32_t offset, std::uint32_t value) {
    ledger.machine_->w32(ledger.machine_->u32(ledger.record() + 0x43d8) + offset, value);
  }
  static std::uint32_t byte(const Ledger &ledger, std::uint32_t offset) {
    return ledger.machine_->u8(ledger.machine_->u32(ledger.record() + 0x43d8) + offset);
  }
  static std::uint32_t half(const Ledger &ledger, std::uint32_t offset) {
    return ledger.machine_->u16(ledger.machine_->u32(ledger.record() + 0x43d8) + offset);
  }
};
} // namespace champions::menus

namespace {
int checks = 0;

void expect(bool ok, const char *name) {
  ++checks;
  if (!ok)
    throw std::runtime_error(name);
}

template <class Fn> void reject(Fn fn, const char *name) {
  bool threw = false;
  try {
    fn();
  } catch (const Refusal &) {
    threw = true;
  }
  expect(threw, name);
}

void rank_vectors() {
  expect(rank_cost(0, 10) == 0, "rank 0*11");
  expect(rank_cost(1, 0) == 1, "rank 1*1");
  expect(rank_cost(1, 10) == 11, "rank 1*11");
  expect(rank_cost(4, 10) == 44, "rank 4*11");
  expect(rank_cost(-1, 10) == -11, "rank negative");
  expect(rank_cost(4, 0) == 4, "rank 4*1");
  expect(rank_cost(5, 0) == 5, "rank power 5,0");
  expect(rank_cost(5, 1) == 6, "rank power 5,1");
  expect(rank_cost(5, 2) == 8, "rank power 5,2");
  expect(rank_cost(5, 3) == 10, "rank power 5,3");
  expect(rank_cost(5, 10) == 44, "rank power 5,10");
  expect(rank_cost(8, 2) == 11, "rank power 8,2");
  expect(rank_cost(20, 10) == 59, "rank power 20,10");
}

void save_index_vectors() {
  constexpr std::uint32_t record = 0x100000;
  const Counts counts{{2, 1, 0}};
  for (auto pair : {std::pair<int, int>{0, 0}, {0, 1}, {1, 0}}) {
    const auto cell = record + BAG_ITEMS + static_cast<std::uint32_t>(pair.first) * BAG_STRIDE +
                      static_cast<std::uint32_t>(pair.second) * ITEM_SIZE;
    const auto got = item_ptr_to_index(cell, record, counts);
    expect(got == pair.first * 1000 + pair.second, "save index");
    expect(item_index_to_ptr(got, record, counts) == cell, "save pointer");
  }
  expect(item_ptr_to_index(0, record, counts) == -1, "null pointer");
  expect(item_ptr_to_index(record + BAG_ITEMS + 2 * ITEM_SIZE, record, counts) == -1, "past count");
  expect(item_index_to_ptr(-1, record, counts) == 0, "index -1");
  expect(item_index_to_ptr(2, record, counts) == 0, "unmatched 2");
  expect(item_index_to_ptr(1000, record, counts) == record + BAG_ITEMS + BAG_STRIDE, "bag 1");
  expect(item_index_to_ptr(1001, record, counts) == 0, "past bag 1");
  expect(item_index_to_ptr(2000, record, counts) == 0, "empty bag");
  expect(item_ptr_to_index(record + BAG_ITEMS + 2 * BAG_STRIDE, record, counts) == -1, "bag 2 cell");
}

void cursor_vectors() {
  expect(cursor_cell(0, 17, 100) == std::make_pair(0, 0), "origin cell");
  expect(cursor_cell(0, 49, 100) == std::make_pair(1, 0), "column 1");
  expect(grid_address(0x2000, 1, 0) == 0x2000 + GRID_COL, "grid column");
  expect(grid_address(0x2000, -1, 0) == 0, "grid negative");
  expect(grid_address(0x2000, 9, 0) == 0, "grid past column");
  expect(grid_address(0x2000, 0, 75) == 0, "grid past row");
  expect(grid_address(0x2000, 8, 74) == 0x2000 + 8 * GRID_COL + 74 * GRID_ROW, "grid corner");
  expect(page_grid_address(0x2000, 1, 0, 0) == 0x2000 + PAGE_BYTES, "page-one hover stride");
  expect(page_grid_address(0x2000, 2, 0, 0) == 0x2000 + 2 * PAGE_BYTES, "page-two hover stride");
  expect(first_cell_bag_address(PLAYER, 1) == PLAYER + BAG_ITEMS + BAG_STRIDE,
         "page-one action bag stride");
  expect(first_cell_bag_address(PLAYER, 2) == PLAYER + BAG_ITEMS + 2 * BAG_STRIDE,
         "page-two action bag stride");
  expect(page_grid_address(0x2000, 3, 0, 0) == 0 && first_cell_bag_address(PLAYER, 3) == 0,
         "stat pages have no item cell");
  expect(cursor_cell(80, 80 + 17 + 4 * 32, 100) == std::make_pair(4, 0), "column 4");
  const auto hit = hit_cell(0, 0, 1);
  expect(cursor_cell(0, hit.x0, hit.y0) == std::make_pair(0, 1), "hit cell row");
}

void action_vectors() {
  struct Row {
    int mode;
    std::uint32_t edge;
    bool equipped, holdable;
    Masks masks;
    Action want;
  };
  const Row rows[] = {
      {0, 0x80, false, false, CROSS_CONFIRM, Action::Buy},
      {0, 0x40, false, false, CROSS_CONFIRM, Action::None},
      {1, 0x80, false, false, CROSS_CONFIRM, Action::Sell},
      {1, 0x80, true, false, CROSS_CONFIRM, Action::ConfirmBox},
      {1, 0x40, false, false, CROSS_CONFIRM, Action::Equip},
      {1, 0x40, true, false, CROSS_CONFIRM, Action::Unequip},
      {1, 0x10, false, false, CROSS_CONFIRM, Action::None},
      {2, 0x40, false, false, CROSS_CONFIRM, Action::Equip},
      {2, 0x40, true, false, CROSS_CONFIRM, Action::Unequip},
      {2, 0x40, false, true, CROSS_CONFIRM, Action::Drag},
      {2, 0x10, false, false, CROSS_CONFIRM, Action::Drop},
      {2, 0x50, false, false, CROSS_CONFIRM, Action::Equip},
      {2, 0x80, false, false, CROSS_CONFIRM, Action::None},
      {2, 0x00, false, false, CROSS_CONFIRM, Action::None},
      {3, 0x00, false, false, CROSS_CONFIRM, Action::Workshop},
      {2, 0x20, false, false, CIRCLE_CONFIRM, Action::Equip},
      {2, 0x80, false, false, CIRCLE_CONFIRM, Action::Drop},
      {2, 0x40, false, false, CIRCLE_CONFIRM, Action::None},
      {0, 0x80, false, false, CIRCLE_CONFIRM, Action::None},
  };
  for (const auto &row : rows)
    expect(item_action(row.mode, row.edge, row.equipped, row.holdable, row.masks) == row.want,
           "item action");
  expect(inventory_grid_tint(false, false, false) == 0x20445558u, "free item underlay");
  expect(inventory_grid_tint(false, true, false) == 0x20000080u, "blocked item underlay");
  expect(inventory_grid_tint(false, true, true) == 0x20445558u, "flag-41 blocked underlay");
  expect(inventory_grid_tint(true, true, false) == 0x20008000u, "equipped wins blocked");
}

void layout_vectors() {
  const auto strip = inventory_strip(80);
  expect(strip.x0 == 95 && strip.y0 == 413 && strip.x1 == 385 && strip.y1 == 414 && strip.layer == 4,
         "strip");
  const auto above = inventory_strip_above(80);
  expect(above.y0 == 412 && above.y1 == 413 && above.x0 == 95, "strip step");
  expect(shop_choice_rows() == std::make_pair(203, 243), "shop rows");
  const auto header = stats_header();
  expect(header.x == 160 && header.y == 107 && header.font == 0x4f7104u && header.layer == 5 &&
             header.colour == 0x80323c3cu,
         "stats header");
  const auto lines = stats_lines();
  expect(lines.size() == 8 && lines[0].x == 160 && lines[0].y == 107 && lines[0].centred, "stats lines");
  const auto skill = skill_layout(40);
  expect(skill.clip_x == 40 && skill.clip_y == 98 && skill.header_centre_x == 93 && skill.header_x == 110,
         "skill clip");
  expect(skill.line_calls == 17 && skill.node_x[0] == 126 && skill.line_x[0] == 17, "skill nodes");
  const auto pause = pause_style();
  expect(pause.font == 0x4f7100u && pause.layer == 7 && pause.colour == 0x80464646u, "pause style");
  const auto shop = shop_mark();
  expect(shop.sprite_x == 66 && shop.sprite_y == 210 && shop.text_colour == 0x80323c3cu, "shop mark");
  expect(stored_mask(1, 0x41b970) == 0x80 && stored_mask(1, 0x41b960) == 0x40, "cross masks");
  expect(stored_mask(0, 0x41b960) == 0x20 && stored_mask(0, 0x41bfb8) == 0x140, "circle masks");
  expect(stored_mask(1, 0x41b964) == 0x7e, "wide stored word");
  reject([] { stored_mask(0, 0x41b970); }, "byte 0 leaves the shop word");
  reject([] { read_wide_mask_as_button(0x7e); }, "wide mask");
  reject([] { refuse("item-info-coordinates"); }, "item info");
  bool saw = false;
  for (const auto &gap : gaps())
    if (std::string(gap.id) == "stat-spend-input")
      saw = true;
  expect(!saw, "stat spend sender is now documented");
  expect(skill_points_for_level(0) == 1 && skill_points_for_level(5) == 1, "skill band 1");
  expect(skill_points_for_level(6) == 2 && skill_points_for_level(15) == 2, "skill band 2");
  expect(skill_points_for_level(16) == 3 && skill_points_for_level(25) == 3, "skill band 3");
  expect(skill_points_for_level(26) == 4 && skill_points_for_level(35) == 4, "skill band 4");
  expect(skill_points_for_level(36) == 5 && skill_points_for_level(80) == 5, "skill band 5");
}

void ledger_vectors() {
  {
    Ledger shop;
    const auto initial = shop.bag_count(0);
    expect(shop.press(0, CROSS_CONFIRM.shop) == Action::Buy, "fixture shop buy action");
    expect(shop.gold() == 441 && shop.bag_count(0) == initial + 1,
           "100 value buy truncates to 59 and inserts in player bag");
    const auto sold = hit_cell(0, 0, 2);
    shop.set_cursor(sold.x0, sold.y0);
    expect(shop.press(1, CROSS_CONFIRM.shop) == Action::Sell, "fixture shop sell action");
    expect(shop.gold() == 445 && shop.bag_count(0) == initial,
           "100 value sell credits 4 and removes from player bag");
  }
  {
    Ledger book;
    expect(book.gold() == 500 && book.bag_count(0) == 2 && book.skill_points() == 4, "seed");
    expect(book.stat_points() == 3 && book.level() == 5 && book.acknowledged() == 4, "seed points");
    expect(book.press(2, 0x40) == Action::Equip && book.equipment_index(0) == 0, "equip");
    expect(book.record_word(0x43f0) == PLAYER + BAG_ITEMS, "equip writes player record pointer");
    expect(book.press(2, 0x40) == Action::Unequip && book.equipment_index(0) == -1, "unequip");
    expect(book.record_word(0x43f0) == 0, "unequip clears player record pointer");
  }
  {
    Ledger authored;
    champions::simulation::rta::items::Tables tables;
    for (int i = 0; i < 2; ++i) {
      auto def = std::make_shared<champions::simulation::rta::items::Definition>();
      def->addr = champions::simulation::rta::items::A_DEFS + static_cast<std::uint32_t>(i) * 64;
      def->name = i == 0 ? "blade" : "charm";
      def->price = 10;
      def->slot = i;
      def->category = 0;
      tables.defs.push_back(def);
    }
    const auto page_tables = tables;
    std::unordered_map<std::uint32_t, std::uint8_t> image;
    const auto image_word = [&](std::uint32_t at, std::uint32_t value) {
      for (unsigned byte = 0; byte < 4; ++byte)
        image[at + byte] = static_cast<std::uint8_t>(value >> (byte * 8));
    };
    const auto image_string = [&](std::uint32_t at, const std::string &value) {
      for (std::size_t byte = 0; byte < value.size(); ++byte)
        image[at + static_cast<std::uint32_t>(byte)] = static_cast<std::uint8_t>(value[byte]);
      image[at + static_cast<std::uint32_t>(value.size())] = 0;
    };
    for (int index = 0; index < 2; ++index) {
      const auto at = champions::simulation::rta::items::A_DEFS + static_cast<std::uint32_t>(index) * 64;
      const auto name_at = 0x3f7000u + static_cast<std::uint32_t>(index) * 0x10u;
      image_word(at, name_at);
      image_string(name_at, index == 0 ? "blade" : "charm");
    }
    image[champions::simulation::rta::items::A_DEFS + 64 + 0x1c] = 1;
    image_string(0x5385e0u, "throw");
    image_string(0x538660u, "throw");
    const auto image_byte = [&](std::uint32_t at) -> std::uint8_t {
      const auto found = image.find(at);
      return found == image.end() ? 0 : found->second;
    };
    Ledger copied;
    copied.adopt_record(authored.record_bytes(), std::move(tables), {}, image_byte);
    expect(copied.press(2, 0x40) == Action::Equip &&
           copied.record_word(0x43f0) == PLAYER + BAG_ITEMS, "imported record equip pointer");
    expect(copied.press(2, 0x40) == Action::Unequip &&
           copied.record_word(0x43f0) == 0, "imported record unequip pointer");
    reject([&] { copied.press(0, CROSS_CONFIRM.shop); }, "unknown live stock refuses purchase");
    expect(copied.frame("shop").sprites.size() == 1, "shop submits only the pinned-tint sprite");
    expect(copied.record_word(0xe4) == 500, "refused stock does not debit gold");
    expect(copied.press(1, CROSS_CONFIRM.shop) == Action::Sell &&
           copied.gold() == 501 && copied.bag_count(0) == 1,
           "live sale credits the imported record and removes one item");
    reject([&] { copied.press(2, 0x10); }, "live drop refuses missing world placement");
    auto page_record = authored.record_bytes();
    const auto put_word = [&](std::size_t at, std::uint32_t value) {
      for (int byte = 0; byte < 4; ++byte)
        page_record[at + static_cast<std::size_t>(byte)] = static_cast<std::uint8_t>(value >> (byte * 8));
    };
    for (int bag = 1; bag < 3; ++bag) {
      const auto base = static_cast<std::size_t>(bag) * BAG_STRIDE;
      put_word(BAG_COUNT + base, 1);
      for (std::size_t byte = 0; byte < ITEM_SIZE; ++byte)
        page_record[BAG_ITEMS + base + byte] = page_record[BAG_ITEMS + byte];
    }
    Ledger pages;
    pages.adopt_record(page_record, page_tables, {}, image_byte);
    std::array<std::vector<IconSize>, BAGS> page_sizes;
    for (int bag = 0; bag < BAGS; ++bag)
      page_sizes[bag].assign(static_cast<std::size_t>(pages.bag_count(bag)), {32, 0, true});
    pages.build_display_grid(page_sizes);
    pages.set_page(1);
    const auto page_one = pages.frame("inventory");
    expect(page_one.rects.size() == 67 && page_one.frames.size() == 1 &&
           page_one.selected_texts.empty(), "page-one display copies draw two occupied cells");
    expect(std::none_of(page_one.gaps.begin(), page_one.gaps.end(), [](const Gap &gap) {
      return std::string(gap.id) == "inventory-page-grid" ||
             std::string(gap.id) == "inventory-grid-footprint";
    }), "page-one display grid is ready");
    expect(pages.press(2, 0x40) == Action::Equip &&
           pages.record_word(0x43f0) == first_cell_bag_address(PLAYER, 1),
           "page-one first-cell action uses bag stride");
    expect(pages.press(2, 0x40) == Action::Unequip, "page-one unequip");
    pages.set_page(2);
    const auto page_two = pages.frame("inventory");
    expect(page_two.rects.size() == 67 && page_two.frames.size() == 1,
           "page-two display copies draw two occupied cells");
    expect(pages.press(2, 0x40) == Action::Equip &&
           pages.record_word(0x43f0) == first_cell_bag_address(PLAYER, 2),
           "page-two first-cell action uses bag stride");
    pages.set_cursor(49, 100);
    reject([&] { pages.press(2, 0x40); }, "page-two other action cell stays refused");
    page_sizes[1][0] = {32, 0};
    pages.build_display_grid(page_sizes);
    pages.set_page(1);
    const auto tall = pages.frame("inventory");
    expect(tall.rects.size() == 65 && tall.frames.empty() &&
           std::any_of(tall.gaps.begin(), tall.gaps.end(), [](const Gap &gap) {
             return std::string(gap.id) == "inventory-grid-footprint" &&
                    gap.column == 0 && gap.row == 0;
           }), "unverified runtime footprint is labelled in its cell and not drawn");
    page_sizes[1][0] = {32, 32, true};
    pages.build_display_grid(page_sizes);
    expect(pages.frame("inventory").rects.size() == 65,
           "positive height refuses even if one-row evidence is asserted");
    page_sizes[1][0] = {16, 0, true};
    pages.build_display_grid(page_sizes);
    const auto unusual = pages.frame("inventory");
    expect(unusual.rects.size() == 65 && unusual.icons.empty() &&
           std::any_of(unusual.gaps.begin(), unusual.gaps.end(), [](const Gap &gap) {
             return std::string(gap.id) == "inventory-grid-footprint" &&
                    gap.column == 0 && gap.row == 0;
           }), "unverified intermediate width is refused");
    auto next_row_record = page_record;
    for (int slot = 1; slot < 6; ++slot)
      for (std::size_t byte = 0; byte < ITEM_SIZE; ++byte)
        next_row_record[BAG_ITEMS + BAG_STRIDE + static_cast<std::size_t>(slot) * ITEM_SIZE + byte] =
            next_row_record[BAG_ITEMS + BAG_STRIDE + byte];
    for (int byte = 0; byte < 4; ++byte)
      next_row_record[BAG_COUNT + BAG_STRIDE + byte] = static_cast<std::uint8_t>(6u >> (8 * byte));
    Ledger packed;
    packed.adopt_record(next_row_record, page_tables, {}, image_byte);
    page_sizes[1] = {{0, 0, true}, {32, 0, true}, {0, 0, true},
                     {64, 0, true}, {0, 0, true}, {32, 0, true}};
    packed.build_display_grid(page_sizes);
    packed.set_page(1);
    const auto packed_frame = packed.frame("inventory");
    const auto row_one = hit_cell(80, 0, 1);
    expect(packed_frame.rects.size() == 75 &&
           packed_frame.rects[73].x0 == row_one.x0 &&
           packed_frame.rects[73].y0 == row_one.y0 &&
           packed.display_bag_slot(1, 0, 1) == 5 &&
           packed.display_bag_slot(1, 1, 1) == 5 &&
           packed.display_bag_slot(1, 8, 0) == -1,
           "two-column item moves to next display row");
    page_sizes[1][1] = {32, 32, true};
    packed.build_display_grid(page_sizes);
    const auto partial = packed.frame("inventory");
    expect(packed.display_bag_slot(1, 0, 0) == 0 &&
           packed.display_bag_slot(1, 1, 0) == -1 &&
           partial.rects.size() == 66 &&
           std::any_of(partial.gaps.begin(), partial.gaps.end(), [](const Gap &gap) {
             return std::string(gap.id) == "inventory-grid-footprint" &&
                    gap.column == 1 && gap.row == 0;
           }), "verified preceding cell survives unknown footprint; later cells refuse");
    auto stack_tables = page_tables;
    stack_tables.defs[0]->name = "throwing-dart";
    image_string(0x3f7000u, "throwing-dart");
    auto stack_record = authored.record_bytes();
    stack_record[BAG_ITEMS + 4] = 25;
    stack_record[BAG_ITEMS + 5] = 0;
    Ledger stack;
    stack.adopt_record(stack_record, stack_tables, {}, image_byte);
    // Independent contract: value 25*10=250; the 0x3d4ccccc float factor
    // makes trunc((250*20)/25*factor) = 9, rather than decimal 0.05's 10.
    // The two name-matched stems also make drop remove min(25,20), leaving 5.
    const auto stack_action = stack.press(1, CROSS_CONFIRM.shop);
    expect(stack_action == Action::Sell && stack.gold() == 509 &&
           stack.bag_count(0) == 2 && stack.record_bytes()[BAG_ITEMS + 4] == 5,
           "name-matched stack sale credits nine and removes twenty");
    image[0x538660u] = 0;
    Ledger unsafe;
    reject([&] { unsafe.adopt_record(stack_record, stack_tables, {}, image_byte); },
           "empty sale stem refuses before any live credit");
    expect(unsafe.gold() == 500, "refused live stem never credits gold");
  }
  {
    Ledger book;
    const auto row = hit_cell(0, 0, 1);
    book.set_cursor(row.x0, row.y0);
    expect(book.press(2, 0x40) == Action::Drag && book.dragging() && book.bag_count(0) == 2, "drag");
    const auto drag_frame = book.frame("inventory");
    expect(drag_frame.texts.size() == 2, "drag does not copy");
    expect(drag_frame.held_icons.empty() &&
           std::any_of(drag_frame.gaps.begin(), drag_frame.gaps.end(), [](const Gap &gap) {
             return std::string(gap.id) == "inventory-drag-picture";
           }), "fixture without picture association labels drag refusal");
  }
  {
    Ledger book;
    expect(book.press(2, 0x10) == Action::Drop && book.bag_count(0) == 1 && book.gold() == 500, "drop");
    expect(book.frame("inventory").texts.at(0).text == "charm", "drop slides");
  }
  {
    Ledger book;
    expect(book.press(0, 0x80) == Action::Buy && book.gold() == 441 && book.bag_count(0) == 3, "buy");
    const auto shop_frame = book.frame("shop");
    expect(shop_frame.texts.size() == 3 && shop_frame.texts[0].x == 160 &&
           shop_frame.texts[0].y == 203 && shop_frame.texts[1].y == 243 &&
           shop_frame.texts[2].text == "stock", "shop choice positions and fixture stock cell");
    expect(shop_frame.sprites.size() == 1 && shop_frame.sprites[0].tint == 0x80808080u &&
           shop_frame.draw_order.size() == 3 &&
           !shop_frame.draw_order[0].text && shop_frame.draw_order[0].index == 0 &&
           shop_frame.draw_order[0].layer == 0 && shop_frame.draw_order[0].submission == 0 &&
           shop_frame.draw_order[1].text && shop_frame.draw_order[1].index == 0 &&
           shop_frame.draw_order[1].layer == 0 && shop_frame.draw_order[1].submission == 1 &&
           shop_frame.draw_order[2].text && shop_frame.draw_order[2].index == 1 &&
           shop_frame.draw_order[2].layer == 0 && shop_frame.draw_order[2].submission == 2,
           "research shop submissions retain layer and order");
    bool refused_buy_sell_tint = false, refused_front_tint = false;
    for (const auto &gap : shop_frame.gaps) {
      refused_buy_sell_tint |= std::string(gap.id) == "shop-buysell-tint";
      refused_front_tint |= std::string(gap.id) == "shop-front-tint";
    }
    expect(refused_buy_sell_tint && refused_front_tint, "unpinned shop tints remain refusals");
  }
  {
    Ledger book;
    expect(book.press(1, 0x80) == Action::Sell && book.gold() == 501 && book.bag_count(0) == 1, "sell");
  }
  {
    Ledger book;
    expect(book.press(2, 0x40) == Action::Equip, "equip before sell");
    expect(book.press(1, 0x80) == Action::ConfirmBox && book.gold() == 500 && book.equipment_index(0) == 0,
           "equipped sell");
  }
  {
    Ledger book;
    expect(book.press(3, 0) == Action::Workshop && book.gold() == 500 && book.bag_count(0) == 2, "workshop");
    book.set_cursor(49, 100);
    expect(book.press(2, 0x40) == Action::None && book.bag_count(0) == 2, "empty cell");
  }
  {
    Ledger book;
    book.set_scheme(CIRCLE_CONFIRM);
    expect(book.press(2, 0x20) == Action::Equip && book.equipment_index(0) == 0, "circle equip");
  }
  {
    Ledger book;
    book.set_scheme(CIRCLE_CONFIRM);
    expect(book.press(2, 0x40) == Action::None && book.equipment_index(0) == -1, "circle cross");
    expect(book.press(0, 0x80) == Action::None && book.gold() == 500, "circle shop");
  }
  {
    Ledger book;
    book.set_page(3);
    expect(book.press(2, 0x40) == Action::None && book.stat_points() == 3, "stats page");
    reject([&] { book.spend_stat(0); }, "stat handler needs complete level-up rules");
    expect(book.base_stat(0) == 10 && book.stat_points() == 3, "refused stat handler preserves record");
    expect(book.passive_level_at(1) == 2, "passive");
    reject([&] { book.spend_skill(0, 1); }, "bare spend is not the skills_screen sender");
    expect(book.skill_points() == 4 && book.passive_level_at(1) == 2, "skill points preserved");
    expect(book.frame("skills").texts.at(0).text == "4 cost 8", "displayed cost");
  }
  {
    Ledger book;
    const auto before = book.hit_points();
    book.grant_level();
    expect(book.level() == 6 && book.skill_points() == 5 && book.stat_points() == 6, "level award");
    expect(book.acknowledged() == 4 && book.hit_points() == book.max_hit_points() && before != book.hit_points(),
           "hit points refilled");
    book.open_level_up();
    expect(book.acknowledged() == 6 && book.page() == 5 && book.level() == 6, "level-up page");
    expect(book.press(2, 0x40) == Action::None, "level-up press");
  }
  {
    Ledger book;
    for (int i = 0; i < 42; ++i)
      book.grant_level();
    expect(book.stat_points() == -127 && book.level() == 47 && book.skill_points() == 150, "stat wrap");
  }
  {
    Ledger book;
    auto bytes = book.save();
    expect(bytes.size() > 56, "save size");
    bytes[52] = bytes[53] = bytes[54] = bytes[55] = 0;
    book.load(bytes);
    expect(book.hit_points() == 0, "loaded zero hit points");
    book.grant_level();
    expect(book.hit_points() == 0 && book.level() == 6, "zero hit points stay");
  }
  {
    Ledger book;
    expect(book.press(2, 0x40) == Action::Equip, "equip for save");
    book.set_page(2);
    book.set_cursor(49, 123);
    const auto bytes = book.save();
    Ledger restored;
    restored.load(bytes);
    expect(restored.save() == bytes, "save round trip");
    expect(restored.equipment_index(0) == 0 && restored.gold() == 500 && restored.page() == 2, "restored");
    auto bad = bytes;
    bad[72] = 99;
    bad[73] = bad[74] = bad[75] = 0;
    reject([&] { restored.load(bad); }, "unknown definition");
    reject([&] { restored.load({1, 2, 3, 4}); }, "truncated save");
  }
  {
    Ledger book;
    const auto drawn = book.frame("inventory");
    expect(!drawn.texts.empty() && drawn.texts[0].x == 17 && drawn.texts[0].y == 89 &&
           drawn.texts[0].colour == 0x80464646u, "documented small-font base colour");
    expect(drawn.sprites.size() == 22 && drawn.sprites[0].resource == "inventrybox3.tex" &&
           drawn.sprites[3].resource == "inventryboxend.tex", "bounded zero-state box sequence");
    expect(drawn.sprites[20].resource == "mousecursor.tex" && drawn.sprites[21].resource == "mousecursor.tex" &&
           drawn.sprites[20].layer == 7 && drawn.sprites[21].layer == 7 &&
           drawn.sprites[20].tint == 0x80808080u && drawn.sprites[21].tint == 0x34000000u,
           "two documented cursor copies");
    expect(drawn.rects.size() >= 65 && drawn.rects[0].colour == 0x01000000u &&
           drawn.rects[64].colour == 0x7f000000u, "scrollbar gradient");
    expect(drawn.selected_texts.size() == 1 && drawn.selected_texts[0].colour == 0x805c2913u,
           "selected small-font colour");
    expect(drawn.frames.size() == 1 && drawn.frames[0].x == 135 && drawn.frames[0].y == 230 &&
           drawn.frames[0].width == 223 && drawn.frames[0].height == 77 &&
           drawn.frames[0].style == 6 && drawn.frames[0].tint == 0x80000000u,
           "observed selected weapon detail frame");
    expect(!drawn.rects.empty() && drawn.rects[0].y0 == 413, "scrollbar");
    const auto stats = book.frame("stats");
    expect(stats.texts.at(0).text == "5" && stats.texts.at(0).x == 160 && stats.texts.at(0).centred, "level line");
    expect(book.frame("pause").texts.empty(), "pause has no frame");
  }
}

void levelup_vectors() {
  const auto formula = read_class_formula([](std::uint32_t at) -> std::uint32_t {
    if (at == 0x1b6740u) return 0x3c013f80u; // authored lui r1, 0x3f80
    if (at == 0x1b6744u) return 0x4481b800u; // authored mtc1 r1, f23
    return 0u;
  }, 0);
  expect(formula[0] == 0x3f800000u && formula[1] == 0u,
         "bounded class-chain reader keeps raw formula words");
  XpTable table{};
  for (int i = 0; i < 81; ++i) table[static_cast<std::size_t>(i)] = i * 100;
  std::array<ClassFormula, 8> formulas{};
  for (auto &row : formulas)
    row = {0u, 0x3f800000u, 0x3f800000u, 0x40000000u, 0x40400000u, 0u};
  Ledger book;
  reject([&] { book.give_xp(500); }, "XP requires supplied table");
  auto bad = table;
  bad[6] = bad[5];
  reject([&] { book.set_xp_rules(bad, formulas); }, "falling threshold refused");
  book.set_xp_rules(table, formulas);
  expect(book.next_threshold() == 500 && book.experience() == 0, "level-five threshold");
  expect(book.hud_base_threshold() == 300 && book.hud_next_threshold() == 400,
         "pending acknowledgement indexes the old HUD interval");
  expect(book.give_xp(499) == 0 && book.level() == 5, "below threshold");
  expect(book.give_xp(1) == 1 && book.level() == 6 && book.experience() == 500,
         "exact threshold transition");
  expect(book.base_threshold() == 500 && book.next_threshold() == 600 &&
         book.hud_base_threshold() == 300 && book.hud_next_threshold() == 400,
         "earned threshold advances while pending HUD interval stays acknowledged");
  expect(book.acknowledged() == 4 && book.skill_points() == 5 && book.stat_points() == 6,
         "old-level point band and pending acknowledgement");
  expect(book.max_hit_points() == 0x424c0000u && book.hit_points() == book.max_hit_points(),
         "class formula HP recalculation and alive refill");
  expect(book.max_mana() == 0x41880000u && book.mana() == book.max_mana(),
         "class formula mana recalculation and alive refill");
  const auto level_six = book.save();
  expect(book.give_xp(100) == 1 && book.max_mana() == 0x41900000u,
         "level seven increases maximum mana to eighteen");
  book.load(level_six);
  expect(book.level() == 6 && book.mana() == 0x41880000u &&
         book.max_mana() == 0x41880000u,
         "loading level six restores both mana fields to seventeen");
  expect(book.give_xp(250) == 2 && book.level() == 8 && book.stat_points() == 12 &&
         book.skill_points() == 9, "multi-level award repeats old-level grants");
  book.open_level_up();
  expect(book.hud_base_threshold() == 700 && book.hud_next_threshold() == 800,
         "acknowledgement advances the HUD interval to the earned level");
  StatInput gate{2, 3, 3000, 0x40u, 0x40u, 0, 0};
  const auto menu_saved = book.save();
  const auto saved_stat = book.base_stat(3);
  for (int point = 0; point < 12; ++point)
    expect(book.press_stat(gate), "exhaust points through stat sender");
  expect(book.stat_points() == 0 && ((book.record_word(0xec) >> 16) & 0xff) == 1,
         "exhaustion selects skills half");
  const auto exhausted = book.save();
  auto missing_half = menu_saved;
  missing_half.resize(missing_half.size() - 4u);
  auto oversized_half = menu_saved;
  oversized_half[oversized_half.size() - 4u] = 0;
  oversized_half[oversized_half.size() - 3u] = 1;
  for (const auto &malformed : {missing_half, oversized_half}) {
    book.load(exhausted);
    book.set_page(2);
    const auto before_refusal = book.save();
    const auto record_before_refusal = book.record_bytes();
    reject([&] { book.load(malformed); }, "invalid v4 sender half refuses after exhaustion");
    expect(book.save() == before_refusal && book.record_bytes() == record_before_refusal &&
           book.page() == 2 && ((book.record_word(0xec) >> 16) & 0xff) == 1 &&
           !book.live_record(), "tail refusal preserves exhausted record and ledger metadata");
    expect(book.give_xp(0) == 0 && book.save() == before_refusal,
           "tail refusal preserves effective-state validity and usable XP operation");
    book.load(menu_saved);
    expect(book.press_stat(gate) && book.stat_points() == 11 &&
           book.base_stat(3) == saved_stat + 1,
           "valid restore and stat spend remain usable after tail refusal");
  }
  book.load(menu_saved);
  expect(book.page() == 5 && ((book.record_word(0xec) >> 16) & 0xff) == 0,
         "restore reopens saved stats half");
  Ledger fresh_menu;
  fresh_menu.load(menu_saved);
  fresh_menu.set_xp_rules(table, formulas);
  expect(book.press_stat(gate) && fresh_menu.press_stat(gate) &&
         book.stat_points() == 11 && fresh_menu.stat_points() == 11 &&
         book.base_stat(3) == saved_stat + 1 && fresh_menu.base_stat(3) == saved_stat + 1,
         "existing and fresh restores spend restored point identically");
  fresh_menu.load(exhausted);
  expect(((fresh_menu.record_word(0xec) >> 16) & 0xff) == 1 && !fresh_menu.press_stat(gate),
         "saved exhausted skills half remains blocked");
  auto legacy_menu = menu_saved;
  legacy_menu.resize(legacy_menu.size() - 4u);
  legacy_menu[8] = 3;
  fresh_menu.load(legacy_menu);
  expect(fresh_menu.page() == 0 && !fresh_menu.press_stat(gate),
         "legacy snapshot without sender half closes level-up page");
  book.load(menu_saved);
  auto suppressed = gate;
  suppressed.elapsed = 2999;
  expect(!book.press_stat(suppressed) && book.stat_points() == 12, "timer gate");
  suppressed = gate;
  suppressed.edge = 0x20;
  expect(!book.press_stat(suppressed), "edge gate");
  suppressed = gate;
  suppressed.panel_gate = 1;
  expect(!book.press_stat(suppressed), "panel gate");
  suppressed = gate;
  suppressed.screen_argument = 1;
  expect(!book.press_stat(suppressed), "screen argument gate");
  suppressed = gate;
  suppressed.level_up_half = 1;
  expect(!book.press_stat(suppressed), "level-up half gate");
  suppressed = gate;
  suppressed.selected_row = 4;
  expect(!book.press_stat(suppressed), "row gate");
  expect(book.press_stat(gate) && book.base_stat(3) == 14 && book.stat_points() == 11,
         "row-three message spends one point");
  expect(book.max_hit_points() == 0x42680000u, "stat spend recalculates HP maximum");
  expect(book.record_word(0x124) == 0x43070000u,
         "stat spend writes researched load capacity");
  expect(LedgerProbe::byte(book, 0x474) == 0 && LedgerProbe::byte(book, 0x475) == 6 &&
         LedgerProbe::byte(book, 0x476) == 0 && LedgerProbe::byte(book, 0x477) == 9,
         "stat spend writes object bonus bytes");
  expect(LedgerProbe::half(book, 0xe4) == 6,
         "stat spend recalculates authored object armour");
  gate.selected_row = 0;
  expect(book.press_stat(gate) && book.base_stat(0) == 11 &&
         book.record_word(0x124) == 0x43080000u,
         "strength spending updates load capacity");
  expect(book.give_kill_xp(0x42000000u, 0x41f00000u) == 4,
         "solo kill formula with separate truncations");
  expect(book.experience() == 754, "kill award enters player record");
  reject([&] { book.give_kill_xp(0x7fc00000u, 0x41f00000u); },
         "nonfinite kill source refused before record mutation");
  expect(book.experience() == 754, "invalid kill source leaves XP unchanged");
  Ledger capped_kill;
  capped_kill.set_xp_rules(table, formulas);
  expect(capped_kill.give_kill_xp(0x47c35000u, 0u) == 100 &&
         capped_kill.experience() == 100,
         "authored high kill XP is capped at the next threshold span");
  Ledger empty;
  Ledger::Adopted zero;
  zero.stat_points = 0;
  empty.adopt(zero);
  empty.set_xp_rules(table, formulas);
  empty.open_level_up();
  gate.selected_row = 0;
  expect(empty.press_stat(gate) && empty.base_stat(0) == 0 && empty.stat_points() == 0,
         "zero balance still sends but handler does not spend");
  expect((empty.record_word(0xec) & 0x00ff0000u) != 0 && !empty.press_stat(gate),
         "empty points switch to skills half and suppress later stat sends");
  Ledger ceiling;
  Ledger::Adopted high;
  high.level = 79;
  high.acknowledged = 79;
  ceiling.adopt(high);
  ceiling.set_xp_rules(table, formulas);
  expect(ceiling.give_xp(9000) == 1 && ceiling.level() == 80 &&
         ceiling.experience() == 7999 && ceiling.next_threshold() == 8000,
         "level eighty ceiling prevents an extra transition");
  Ledger pending;
  Ledger::Adopted below;
  below.level = 5;
  below.acknowledged = 5;
  below.experience = 499;
  pending.adopt(below);
  pending.set_xp_rules(table, formulas);
  expect(pending.hud_base_threshold() == 400 && pending.hud_next_threshold() == 500,
         "HUD interval immediately before pending level");
  expect(pending.give_xp(1) == 1 && pending.level() == 6 && pending.acknowledged() == 5 &&
         pending.hud_base_threshold() == 400 && pending.hud_next_threshold() == 500,
         "pending level keeps a full acknowledged-level HUD bar");
  pending.open_level_up();
  expect(pending.hud_base_threshold() == 500 && pending.hud_next_threshold() == 600,
         "opening level-up changes HUD interval only after acknowledgement");
  Ledger refused;
  Ledger::Adopted malformed;
  malformed.stats[3] = -1;
  malformed.stat_points = 3;
  refused.adopt(malformed);
  refused.set_xp_rules(table, formulas);
  const auto before_xp = refused.record_bytes();
  reject([&] { refused.give_xp(100); }, "unsupported XP recalculation refuses");
  expect(refused.record_bytes() == before_xp, "refused XP award preserves every record byte");
  refused.open_level_up();
  const auto before_stat = refused.record_bytes();
  StatInput refused_gate{2, 0, 3000, 0x40u, 0x40u, 0, 0};
  reject([&] { refused.press_stat(refused_gate); }, "unsupported stat recalculation refuses");
  expect(refused.record_bytes() == before_stat, "refused stat spend preserves every record byte");
  Ledger changed_equipment;
  changed_equipment.set_xp_rules(table, formulas);
  expect(changed_equipment.press(2, 0x40) == Action::Equip, "equipment mutation observed");
  reject([&] { changed_equipment.give_xp(500); },
         "level-up refuses stale effective bonuses after equipment change");
  auto equipped_bytes = changed_equipment.save();
  const auto suffix = equipped_bytes.size() - 100u;
  auto put_word = [&](std::size_t at, std::uint32_t value) {
    for (int i = 0; i < 4; ++i)
      equipped_bytes[at + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(value >> (8 * i));
  };
  put_word(suffix, 25u); // authored effective Strength, base 10
  put_word(suffix + 16u, 15u); // authored offset, kept invalid after equipment mutation
  Ledger restored_equipment;
  restored_equipment.load(equipped_bytes);
  restored_equipment.set_xp_rules(table, formulas);
  expect(restored_equipment.record_word(0x90) == 25u && restored_equipment.save() == equipped_bytes,
         "equipped-character round trip preserves effective bonus bytes");
  const auto before_loaded_xp = restored_equipment.record_bytes();
  reject([&] { restored_equipment.give_xp(500); },
         "equipment invalidation survives save and load");
  expect(restored_equipment.record_bytes() == before_loaded_xp,
         "loaded invalid equipment refuses without changing the record");

  Ledger snapshot;
  snapshot.set_xp_rules(table, formulas);
  snapshot.give_xp(500);
  LedgerProbe::record_word(snapshot, 0xac, 0x40a00000u);
  LedgerProbe::record_word(snapshot, 0xc0, 0x40400000u);
  const auto saved = snapshot.save();
  auto check_restored = [&](const Ledger &restored) {
    expect(restored.level() == 6 && restored.base_stat(0) == 10 &&
           restored.record_word(0x90) == 10u, "restored level and Strength");
    expect(restored.hit_points() == 0x40a00000u && restored.max_hit_points() == 0x424c0000u,
           "restore preserves depleted current HP without refill");
    expect(restored.mana() == 0x40400000u && restored.max_mana() == 0x41880000u,
           "restore preserves depleted current mana and saved maximum");
    expect(restored.record_word(0x124) == 0x43070000u, "restore writes saved capacity");
    expect(restored.record_word(0xdc) == 0u, "restore writes saved stagger resistance");
    expect(LedgerProbe::half(restored, 0xe4) == 6u, "restore writes saved armour");
    expect(LedgerProbe::word(restored, 0x474) == 0x08000600u,
           "restore writes all four saved object bonus bytes");
    for (std::uint32_t index = 0; index < 5; ++index)
      expect(LedgerProbe::word(restored, 0x5c + 4u * index) == 0u,
             "restore writes each saved object resistance");
  };
  snapshot.give_xp(100);
  snapshot.spend_stat(0);
  snapshot.spend_stat(2);
  snapshot.spend_stat(2);
  snapshot.spend_stat(3);
  expect(snapshot.record_word(0x124) == 0x43080000u &&
         LedgerProbe::half(snapshot, 0xe4) == 7u &&
         LedgerProbe::word(snapshot, 0x474) == 0x09000600u,
         "later stat spends change capacity, armour and bonuses");
  LedgerProbe::record_word(snapshot, 0xdc, 0x3f000000u);
  for (std::uint32_t index = 0; index < 5; ++index)
    LedgerProbe::object_word(snapshot, 0x5c + 4u * index, 0x3f000000u);
  snapshot.load(saved);
  check_restored(snapshot);
  expect(snapshot.give_xp(100) == 1, "complete snapshot permits later supported XP");
  Ledger fresh;
  fresh.load(saved);
  check_restored(fresh);
  fresh.set_xp_rules(table, formulas);
  fresh.spend_stat(0);
  expect(fresh.mana() == 0x40400000u && fresh.hit_points() == 0x40a00000u &&
         fresh.record_word(0x124) == 0x43080000u,
         "fresh restore supports stat spending without refilling resources");
  for (const auto version : {1u, 2u}) {
    auto legacy = saved;
    legacy.resize(legacy.size() - (version == 1 ? 100u : 56u));
    legacy[8] = static_cast<std::uint8_t>(version);
    Ledger old;
    old.load(legacy);
    old.set_xp_rules(table, formulas);
    const auto before = old.record_bytes();
    reject([&] { old.give_xp(100); }, "incomplete legacy snapshot refuses XP reconstruction");
    reject([&] { old.spend_stat(0); }, "incomplete legacy snapshot refuses stat reconstruction");
    expect(old.record_bytes() == before, "legacy reconstruction refusal preserves record");
  }
  auto missing_object = saved;
  missing_object[missing_object.size() - 36u] = 0;
  Ledger unsupported;
  unsupported.load(missing_object);
  unsupported.set_xp_rules(table, formulas);
  reject([&] { unsupported.give_xp(100); }, "missing object snapshot cannot authorize XP");
  Ledger domain;
  domain.set_xp_rules(table, formulas);
  const auto prior_domain = domain.save();
  Ledger::Adopted invalid;
  invalid.level = 81;
  reject([&] { domain.adopt(invalid); }, "XP rules refuse level-81 adoption");
  expect(domain.save() == prior_domain, "refused adoption preserves ledger");
  auto invalid_save = prior_domain;
  invalid_save[24] = 81;
  reject([&] { domain.load(invalid_save); }, "XP rules refuse level-81 restoration");
  expect(domain.save() == prior_domain, "refused restoration preserves ledger");
  LedgerProbe::record_word(domain, 0x58, 0x00005104u);
  const auto invalid_record = domain.record_bytes();
  reject([&] { domain.give_xp(100); }, "out-of-domain XP refuses before mutation");
  expect(domain.record_bytes() == invalid_record, "out-of-domain XP preserves record");
  Ledger no_rules;
  no_rules.adopt(invalid);
  reject([&] { no_rules.set_xp_rules(table, formulas); }, "invalid earned level refuses XP configuration");
  expect(!no_rules.has_xp_rules(), "refused configuration leaves rules inactive");
  invalid.level = 5;
  no_rules.adopt(invalid);
  Ledger imported;
  // No definitions in this fixture, but live adoption still requires the
  // two executable stack stems. Authored nonempty strings, like the vendor
  // fixture above; these are not captured retail data.
  const auto imported_byte = [](std::uint32_t at) -> std::uint8_t {
    return at == 0x5385e0u || at == 0x538660u ? 'x' : 0;
  };
  imported.adopt_record(no_rules.record_bytes(), {}, {}, imported_byte);
  imported.set_xp_rules(table, formulas);
  const auto before_imported = imported.save();
  invalid.level = 81;
  reject([&] { imported.adopt(invalid); }, "invalid adoption refuses over imported record");
  reject([&] { imported.load(invalid_save); }, "invalid restoration refuses over imported record");
  auto invalid_import = no_rules.record_bytes();
  invalid_import[0x59] = 81;
  reject([&] { imported.adopt_record(invalid_import, {}, {},
                                    [](std::uint32_t) -> std::uint8_t { return 0; }); },
         "invalid imported earned level refuses");
  expect(imported.live_record() && imported.save() == before_imported &&
         imported.next_threshold() == 500,
         "domain refusals preserve imported relationship and usable XP state");
  for (const auto &malformed : {missing_half, oversized_half}) {
    imported.set_page(2);
    const auto imported_before = imported.save();
    const auto imported_record_before = imported.record_bytes();
    reject([&] { imported.load(malformed); }, "v4 tail refuses over imported record");
    expect(imported.live_record() && imported.page() == 2 &&
           imported.save() == imported_before &&
           imported.record_bytes() == imported_record_before && imported.next_threshold() == 500,
           "tail refusal retains imported ownership, metadata and readable thresholds");
    expect(imported.give_xp(0) == 0 && imported.save() == imported_before,
           "supported detached imported XP operation remains usable");
    imported.open_level_up();
    expect(imported.page() == 5 && imported.live_record(),
           "supported imported menu operation works after tail refusal");
  }
}
void observed_level_vectors() {
  XpTable table{};
  for (int i = 0; i < 81; ++i)
    table[static_cast<std::size_t>(i)] = i * 100;
  std::array<ClassFormula, 8> formulas{};
  for (auto &row : formulas)
    row = {0u, 0x3f800000u, 0x3f800000u, 0x40000000u, 0x40400000u, 0u};
  const std::pair<int, int> bands[] = {{5, 1}, {6, 2}, {15, 2}, {16, 3}, {25, 3},
                                        {26, 4}, {35, 4}, {36, 5}, {79, 5}};
  for (const auto [old_level, grant] : bands) {
    Ledger book;
    Ledger::Adopted hero;
    hero.level = old_level;
    hero.acknowledged = 2;
    hero.experience = table[static_cast<std::size_t>(old_level)] - 1;
    hero.skill_points = 41;
    hero.stat_points = 70;
    for (int i = 0; i < 4; ++i)
      hero.stats[i] = 10;
    book.adopt(hero);
    book.set_xp_rules(table, formulas);
    expect(book.give_xp(0) == 0 && book.level() == old_level && book.skill_points() == 41 &&
               book.stat_points() == 70 && book.acknowledged() == 2,
           "one below the exact word does not level");
    expect(book.give_xp(1) == 1 && book.level() == old_level + 1 &&
               book.skill_points() == 41 + grant && book.stat_points() == 73 &&
               book.acknowledged() == 2 && book.level_notice().stored &&
               book.level_notice().calls == 1 && book.level_notice().frames == 0x168 &&
               book.level_notice().force == 0,
           "exact table word grants the old-level skill word and the stat byte");
  }
  {
    Ledger book;
    Ledger::Adopted hero;
    hero.level = 5;
    hero.acknowledged = 4;
    hero.experience = 499;
    hero.stat_points = 254;
    hero.skill_points = 0;
    for (int i = 0; i < 4; ++i)
      hero.stats[i] = 10;
    book.adopt(hero);
    book.set_xp_rules(table, formulas);
    expect(book.give_xp(1) == 1 && book.stat_points() == 1 && book.skill_points() == 1 &&
               book.acknowledged() == 4,
           "stat byte 254 becomes 1 and acknowledged level stays");
  }
  {
    Ledger book;
    Ledger::Adopted hero;
    hero.level = 5;
    hero.acknowledged = 4;
    hero.experience = 499;
    for (int i = 0; i < 4; ++i)
      hero.stats[i] = 10;
    book.adopt(hero);
    book.set_xp_rules(table, formulas);
    LedgerProbe::record_word(book, 0xac, 0);
    LedgerProbe::record_word(book, 0xc0, 0x40400000u);
    expect(book.give_xp(1) == 1 && book.hit_points() == 0 && book.mana() == 0x40400000u,
           "a zero hit-point value leaves mana");
  }
  {
    // A zero force with the local menu open keeps the previous message.
    // hud-live-producers.md:120-122, 174-176.
    Ledger book;
    Ledger::Adopted hero;
    hero.level = 5;
    hero.acknowledged = 4;
    hero.experience = 499;
    for (int i = 0; i < 4; ++i)
      hero.stats[i] = 10;
    book.adopt(hero);
    book.set_xp_rules(table, formulas);
    book.set_local_menu_open(true);
    expect(book.give_xp(1) == 1 && book.level() == 6 && !book.level_notice().stored &&
               book.level_notice().calls == 1,
           "force 0 does not store while the local menu is open");
    book.set_local_menu_open(false);
    expect(book.give_xp(100) == 1 && book.level_notice().stored &&
               book.level_notice().frames == 0x168 && book.level_notice().calls == 2,
           "the next level step stores once the menu is closed");
  }
  {
    Ledger book;
    Ledger::Adopted hero;
    hero.level = 6;
    hero.acknowledged = 4;
    hero.experience = 123456;
    hero.skill_points = 7;
    hero.stat_points = 9;
    for (int i = 0; i < 4; ++i)
      hero.stats[i] = 10;
    book.adopt(hero);
    book.set_xp_rules(table, formulas);
    const auto image = book.save();
    Ledger::Adopted cleared = hero;
    cleared.level = 1;
    cleared.acknowledged = 1;
    cleared.experience = 0;
    cleared.skill_points = 0;
    cleared.stat_points = 0;
    book.adopt(cleared);
    book.load(image);
    expect(book.level() == 6 && book.acknowledged() == 4 && book.experience() == 123456 &&
               book.skill_points() == 7 && book.stat_points() == 9,
           "a pending level survives restore and is not applied");
  }
  {
    Ledger book;
    book.set_page(5);
    champions::levelup_hud::SkillAsk ask;
    ask.kind = 0;
    ask.need = 1;
    ask.ident = 1;
    ask.pre_kind = -1;
    ask.cursor = champions::levelup_hud::Cursor::Inside;
    ask.edge = 0x40;
    ask.confirm_mask = 0x40;
    ask.page = 5;
    ask.resource = true;
    reject([&] { book.skill_screen(ask); }, "the screen's recalc needs the executable rules");
    expect(book.skill_points() == 4, "refused screen leaves the skill word");
    book.set_xp_rules(table, formulas);
    const auto sent = book.skill_screen(ask);
    expect(sent.sent && sent.kind == 0 && sent.value == 1 && book.skill_points() == 3 &&
               book.passive_level_at(1) == 3,
           "passive row sends slot, kind 0, mask");
    ask.cursor = champions::levelup_hud::Cursor::Outside;
    const auto missed = book.skill_screen(ask);
    expect(!missed.sent && !missed.recalc && book.skill_points() == 3, "outside cursor does not send");
    ask.cursor = champions::levelup_hud::Cursor::Edge;
    reject([&] { book.skill_screen(ask); }, "cursor-box edge stays refused");
  }
}

} // namespace

int main() {
  rank_vectors();
  save_index_vectors();
  cursor_vectors();
  action_vectors();
  layout_vectors();
  ledger_vectors();
  levelup_vectors();
  observed_level_vectors();
  std::cout << "PASS menus " << checks << " checks\n";
  return 0;
}
