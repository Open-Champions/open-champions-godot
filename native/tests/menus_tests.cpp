#include "champions/menus.hpp"
#include "champions/items.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace champions::menus;

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
  expect(saw, "stat spend gap");
  expect(skill_points_for_level(0) == 1 && skill_points_for_level(5) == 1, "skill band 1");
  expect(skill_points_for_level(6) == 2 && skill_points_for_level(15) == 2, "skill band 2");
  expect(skill_points_for_level(16) == 3 && skill_points_for_level(25) == 3, "skill band 3");
  expect(skill_points_for_level(26) == 4 && skill_points_for_level(35) == 4, "skill band 4");
  expect(skill_points_for_level(36) == 5 && skill_points_for_level(80) == 5, "skill band 5");
}

void ledger_vectors() {
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
      def->slot = i;
      def->category = 0;
      tables.defs.push_back(def);
    }
    const auto page_tables = tables;
    Ledger copied;
    copied.adopt_record(authored.record_bytes(), std::move(tables), {},
                        [](std::uint32_t at) -> std::uint8_t {
                          return at == champions::simulation::rta::items::A_DEFS + 64 + 0x1c ? 1 : 0;
                        });
    expect(copied.press(2, 0x40) == Action::Equip &&
           copied.record_word(0x43f0) == PLAYER + BAG_ITEMS, "imported record equip pointer");
    expect(copied.press(2, 0x40) == Action::Unequip &&
           copied.record_word(0x43f0) == 0, "imported record unequip pointer");
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
    pages.adopt_record(page_record, page_tables, {}, [](std::uint32_t) -> std::uint8_t { return 0; });
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
    packed.adopt_record(next_row_record, page_tables, {}, [](std::uint32_t) -> std::uint8_t { return 0; });
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
    expect(book.frame("shop").texts.at(0).text == "stock", "stock cell");
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
    book.spend_stat(0);
    expect(book.base_stat(0) == 11 && book.stat_points() == 2, "stat handler");
    expect(book.passive_level_at(1) == 2, "passive");
    book.spend_skill(0, 1);
    expect(book.skill_points() == 3 && book.passive_level_at(1) == 3, "skill handler");
    expect(book.frame("skills").texts.at(0).text == "3 cost 10", "displayed cost");
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
} // namespace

int main() {
  rank_vectors();
  save_index_vectors();
  cursor_vectors();
  action_vectors();
  layout_vectors();
  ledger_vectors();
  std::cout << "PASS menus " << checks << " checks\n";
  return 0;
}
