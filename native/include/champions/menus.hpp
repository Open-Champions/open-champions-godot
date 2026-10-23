#pragma once

#include "champions/messages.hpp"
#include "champions/inventory_icon_assoc.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// Screen in front of the existing inventory, shop, stat and skill handlers.
// docs/menus.md and tools/ref_menus.py at research t3/research-menus 099900e.
// Addresses are the US v2.00 ones. Nothing here is copied from the game.
namespace champions::menus {

struct Refusal : std::runtime_error {
  using std::runtime_error::runtime_error;
};

inline constexpr std::uint32_t RANK_EXPONENT = 0x3fccccccu;
inline constexpr int BAGS = 3;
inline constexpr std::uint32_t BAG_STRIDE = 0x1208;
inline constexpr std::uint32_t ITEM_SIZE = 0x48;
inline constexpr std::uint32_t BAG_ITEMS = 0x130;
inline constexpr std::uint32_t BAG_COUNT = 0x1330;
inline constexpr int INDEX_BASE = 1000;
inline constexpr int GRID_COLS = 9;
inline constexpr int GRID_ROWS = 75;
inline constexpr std::uint32_t GRID_COL = 0x1518;
inline constexpr std::uint32_t GRID_ROW = 0x48;
inline constexpr std::uint32_t PAGE_BYTES = 0xbdd8;
inline constexpr int CURSOR_X_BIAS = 17;
inline constexpr int CURSOR_COL = 32;
inline constexpr int CURSOR_ROW = 34;

// Player record and the shop stock list from the UI notes. The ledger keeps
// its items in that record so equip, gold and saves use the existing handlers.
inline constexpr std::uint32_t PLAYER = 0x4d8af0;
inline constexpr std::uint32_t PANEL_ADDR = 0x4f4040;
inline constexpr std::uint32_t STOCK = 0x866e78;
inline constexpr std::uint32_t CLASS_ROW = 0x4ef530;
inline constexpr std::uint32_t CLASS_STRIDE = 0xa88;
inline constexpr std::uint32_t SKILL_ROW = 0x38;

struct Masks {
  std::uint32_t shop = 0, equip = 0, drop = 0, confirm = 0, cancel = 0;
};
// game_initCamera stores these from the byte at 0x41b69c. Who writes that byte
// is not known. Cross-confirm is the only set that stores a shop button.
inline constexpr Masks CROSS_CONFIRM{0x80, 0x40, 0x10, 0x40, 0x20};
inline constexpr Masks CIRCLE_CONFIRM{0x00, 0x20, 0x80, 0x20, 0x140};

enum class Action {
  None, Buy, Sell, ConfirmBox, Equip, Unequip, Drop, Drag, Workshop
};
const char *action_name(Action);

struct Counts {
  std::int32_t n[3] = {};
};

std::int32_t rank_cost(int param, int level);
std::int32_t item_ptr_to_index(std::uint32_t item, std::uint32_t record, Counts counts);
std::uint32_t item_index_to_ptr(std::int32_t index, std::uint32_t record, Counts counts);
std::pair<int, int> cursor_cell(int panel_x, int cursor_x, int cursor_y,
                                int tex_height = 0, int scroll = 0);
std::uint32_t grid_address(std::uint32_t base, int column, int row);
std::uint32_t page_grid_address(std::uint32_t base, int page, int column, int row);
std::uint32_t first_cell_bag_address(std::uint32_t record, int page);
Action item_action(int mode, std::uint32_t edge, bool equipped, bool holdable,
                   Masks masks);
std::uint32_t inventory_grid_tint(bool equipped, bool blocked, bool flag41);

struct Strip {
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0, layer = 4;
};
Strip inventory_strip(int panel_x);
Strip inventory_strip_above(int panel_x);
std::pair<int, int> shop_choice_rows();

struct StatsHeader {
  int x = 160, y = 107;
  std::uint32_t font = 0x4f7104;
  int layer = 5;
  std::uint32_t colour = 0x80323c3c;
  int icon_alpha = -1;
};
StatsHeader stats_header();

struct TextPos {
  int x = 0, y = 0;
  bool centred = false, wide = false, ascii = false;
};
// stats_screen(0) submission positions. The frame is 0x124 by 0x197; its origin
// was not recovered.
std::vector<TextPos> stats_lines();
inline constexpr int STATS_FRAME_W = 0x124;
inline constexpr int STATS_FRAME_H = 0x197;

struct SkillLayout {
  int header_centre_x = 0, header_x = 0, header_y = 106;
  int clip_x = 0, clip_y = 98, clip_layer = 5;
  int clip2_x = 0, clip2_y = 158, clip2_layer = 3;
  std::uint32_t colour = 0x801e6464;
  int text_layer = 5;
  std::uint32_t font = 0x4f7104;
  int icon_alpha = -1;
  int node_x[5] = {86, 137, 196, 247, 306};
  int node_y[7] = {164, 261, 349, 446, 534, 622, 710};
  int line_x[5] = {17, 68, 127, 178, 237};
  int line_y[7] = {24, 121, 209, 306, 394, 482, 570};
  int line_calls = 17;
};
// Class 0, rank 0 only. Other classes were not walked, and the 17 line calls
// were not paired.
SkillLayout skill_layout(int origin);

struct PauseStyle {
  std::uint32_t font = 0x4f7100;
  int layer = 7;
  std::uint32_t colour = 0x80464646;
};
PauseStyle pause_style();

struct ShopMark {
  int sprite_x = 66, sprite_y = 210, sprite_layer = 0;
  std::uint32_t sprite_tint = 0x80808080;
  int text_layer = 0;
  std::uint32_t text_colour = 0x80323c3c;
  int icon_alpha = -1;
  std::uint32_t font = 0x44fdd8;
  int row_y[2] = {203, 243};
};
ShopMark shop_mark();

// Word game_initCamera stores for the byte at 0x41b69c. Byte 0 does not write
// 0x41b970 or 0x41b974. The stored 0x7b..0x7e words are not pad bits.
std::uint32_t stored_mask(int scheme_byte, std::uint32_t address);
void read_wide_mask_as_button(std::uint32_t word);

// Hit cell of the executed cursor division. Column c covers x
// [panel+17+c*32, panel+17+(c+1)*32). Row r covers the y values whose
// row_top falls in [r*34, (r+1)*34).
struct Hit {
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
};
Hit hit_cell(int panel_x, int column, int row, int tex_height = 0, int scroll = 0);

// Skill points added when a level is gained, from the level before the gain.
// +3 stat points is separate. The experience thresholds at 0x41b558 are not in
// this contract.
int skill_points_for_level(int level_before);

struct Gap {
  const char *id;
  std::string reason;
  int column = -1, row = -1;
};
// Stable refusals. Asking for a missing coordinate throws Refusal.
std::vector<Gap> gaps();
[[noreturn]] void refuse(const char *what);

struct DrawText {
  int x = 0, y = 0;
  std::uint32_t colour = 0;
  bool centred = false;
  std::string text;
};
struct DrawRect {
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  std::uint32_t colour = 0;
  int layer = 0;
};
struct DrawSprite {
  int x = 0, y = 0, layer = 0;
  std::uint32_t tint = 0;
  std::string lump, resource;
};
struct DrawIcon {
  int column = 0, row = 0;
  int category = 0, slot = 0;
  std::uint32_t tint = 0x80808080u;
  std::string picture;
};
struct HeldIcon {
  int x = 0, y = 0, layer = 7;
  std::uint32_t tint = 0;
  std::string picture;
};
struct IconSize {
  int width = -1, height = -1;
  bool verified_one_row = false;
};
struct DrawFrame {
  int x = 0, y = 0, width = 0, height = 0, style = 0;
  std::uint32_t tint = 0;
};
struct Frame {
  std::string screen;
  std::vector<DrawText> texts;
  std::vector<DrawText> selected_texts;
  std::vector<DrawRect> rects;
  std::vector<DrawSprite> sprites;
  std::vector<DrawIcon> icons;
  std::vector<HeldIcon> held_icons;
  std::vector<DrawFrame> frames;
  std::vector<Gap> gaps;
};

// One local player's bags, equipment, gold and points, mutated only by the
// existing message handlers. The memory-card browser is not drawn.
class Ledger {
public:
  Ledger();
  Ledger(const Ledger &) = delete;
  Ledger &operator=(const Ledger &) = delete;

  void set_scheme(Masks masks);
  Masks scheme() const { return masks_; }
  void set_cursor(int x, int y);
  void set_page(int page);
  int page() const;

  // mode 0 shop stock, 1 shop bag, 2 inventory, 3 workshop.
  // Page 3, 4 and 5 draw the stats sheet and do not take a cell.
  Action press(int mode, std::uint32_t edge);

  // Handlers. The stat and skill screens' pad bits were not executed, so the
  // play scene does not bind these to a button.
  void spend_stat(int stat);
  void spend_skill(int is_skill, std::uint32_t value);
  void grant_level();
  void open_level_up();

  std::int32_t gold() const;
  std::int32_t skill_points() const;
  std::int32_t stat_points() const;
  std::int32_t level() const;
  std::int32_t acknowledged() const;
  std::int32_t base_stat(int index) const;
  std::uint32_t hit_points() const;
  std::uint32_t max_hit_points() const;
  int bag_count(int bag) const;
  std::uint32_t equipment(int slot) const;
  std::int32_t equipment_index(int slot) const;
  std::uint32_t record_word(std::uint32_t offset) const;
  bool dragging() const { return dragging_ != 0; }
  bool live_record() const { return live_record_; }
  std::uint32_t drag_cell() const { return drag_cell_; }
  int passive_level_at(std::uint64_t mask);

  // A character from the front end (player_initNewCharacter fields) or a PS2
  // import: gold +0xe4, level +0x59, acknowledged +0x58, experience +0x5c, base
  // stats +0x78, skill points +0xe8, stat points +0xec. The bags and the
  // equipment pointers are emptied: neither path supplies item definitions here.
  struct Adopted {
    std::int32_t gold = 0, level = 1, acknowledged = 1, experience = 0;
    std::int32_t skill_points = 0, stat_points = 0;
    std::int32_t stats[4] = {0, 0, 0, 0};
  };
  void adopt(const Adopted &character);
  // Copy the selected imported player's 0x4c30-byte record into this menu
  // machine. The caller supplies definitions and an exact executable reader.
  void adopt_record(const std::vector<std::uint8_t> &bytes,
                    champions::simulation::rta::items::Tables tables,
                    champions::inventory_icons::PictureTables pictures,
                    const std::function<std::uint8_t(std::uint32_t)> &image_byte);
  std::array<std::vector<std::string>, BAGS> bag_picture_names();
  void build_display_grid(const std::array<std::vector<IconSize>, BAGS> &sizes);
  int display_bag_slot(int page, int column, int row) const;
  std::vector<std::uint8_t> record_bytes() const;

  Frame frame(const std::string &screen);
  std::vector<std::uint8_t> save() const;
  void load(const std::vector<std::uint8_t> &bytes);

private:
  using Machine = champions::messages::rta::Machine;
  champions::simulation::rta::items::Tables tables_;
  champions::inventory_icons::PictureTables pictures_;
  champions::messages::rta::Memory memory_;
  champions::messages::rta::Callbacks callbacks_;
  std::unique_ptr<Machine> machine_;
  Masks masks_ = CROSS_CONFIRM;
  int cursor_x_ = 17, cursor_y_ = 100;
  int dragging_ = 0;
  std::uint32_t drag_cell_ = 0;
  int drag_column_ = 0, drag_row_ = 0;
  bool live_record_ = false;
  std::uint32_t stock_addr_ = STOCK;
  struct DisplayCell {
    int slot = -1;
    std::array<std::uint8_t, ITEM_SIZE> bytes{};
  };
  std::array<std::vector<DisplayCell>, BAGS> display_cells_;
  std::array<std::string, BAGS> display_refusals_;
  std::array<std::pair<int, int>, BAGS> display_refusal_cells_{};

  std::uint32_t record() const { return PLAYER; }
  void seed();
  void write_item(std::uint32_t addr, std::uint32_t definition, int count,
                  std::uint64_t flags);
  Counts counts() const;
  std::uint32_t cell_address(int mode, int column, int row) const;
  bool bag_index(std::uint32_t addr, int &bag, int &index) const;
  std::string picture_name(std::uint32_t addr);
};

} // namespace champions::menus
