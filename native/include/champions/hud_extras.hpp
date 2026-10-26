#pragma once
// HUD extras: status bars with the XP bar and pending rectangles, starting and
// respawn current health/mana, the gold counter chain, potion notices, the
// quick-cast selector input, and the minimap projector's geometry.
//
// Evidence: research branch t3/research-hud-gaps f7c3964 (docs/hud-gaps.md,
// docs/spec/hud-status.md, hud-xp.md, hud-gold.md, hud-quickslots.md,
// hud-minimap.md) and its reference models tools/ref_hud_*.py. Every function
// replays those models; native/tests/vectors/hud_extras_*.tsv hold their
// outputs (tools/hud_extras_vectors.py regenerates them). Floats are raw
// 32-bit R5900 patterns, as in core_arithmetic.hpp.
//
// Refused (research marks them Unknown or Inferred):
//   * minimap tiles: the VU1 program 0x403e20 was read but not executed;
//   * minimap icons: they run icon scripts and vars_get from draw code;
//   * raster parity of the rectangles on the GS;
//   * console frame/tick ratio for the gold floaters;
//   * ammunition, thrown and camera notice starters (not executed);
//   * Player_ctor as a whole (starting current values apply from its recalc).
#include "champions/core_arithmetic.hpp"
#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace champions::hud_extras {
using Raw = simulation::rta::Raw;
struct Refusal : std::runtime_error {
  using std::runtime_error::runtime_error;
};

// ---- status block (hud-status.md, hud-xp.md) ---------------------------
struct StatusInput {
  int slot{}, x{}; // raw slot; promoted HUD origin X
  bool local = true;
  std::uint32_t obj_flags{}; // player object +0xc, bit 0x8 = dead
  Raw health{}, health_max{}, health_pending{};
  Raw mana{}, mana_max{}, mana_pending{};
  std::int32_t tick{};
  std::uint32_t flash{};  // 0x41b930[slot]
  std::int32_t cooldown{}; // s16 object +0x4ae
  std::int32_t ack_level{}, level{}; // s8 record +0x58, +0x59
  std::int32_t exp{}, exp_base{}, exp_next{}; // record +0x5c; T[ack-1], T[ack]
  std::int32_t glow_alpha{};
};
struct StatusDraw {
  bool rect = false; // false: clipped sprite
  std::string resource;
  int x0{}, y0{}, x1{}, y1{}; // sprite: origin in x0,y0; rectangle: corners
  int layer{};
  std::uint32_t tint{}; // rectangle colour or sprite tint
  int clip_left{}, clip_top{}, clip_right{}, clip_bottom{};
};
struct StatusResult {
  std::vector<StatusDraw> draws; // call order
  std::uint32_t flash{};
  std::int32_t glow_alpha{};
};
StatusResult statusDraws(const StatusInput &);
// Gates before any draw (hud-status.md section 1).
bool statusVisible(bool menu_open, int hud_suppressed, bool shop_open,
                   int camera_mode, bool slot_active, bool has_object,
                   std::uint32_t vehicle, std::int32_t free_units);
int animFrame(std::int32_t tick);                 // (tick>>2)%11, C remainder
std::string barName(const char *colour, std::int32_t tick);
std::int32_t xpFillRight(std::int32_t origin, std::int32_t xp,
                         std::int32_t lower, std::int32_t upper);
std::int32_t fillRight(std::int32_t origin, Raw current, Raw maximum);
int pendingCap(int slot);
// Both rectangles need: local, dead bit clear, own pending > 0. No health gate.
bool pendingRectsAllowed(bool local, std::uint32_t obj_flags);
// The clipped drawer's SCISSOR_1 word (0x17b8b0). letterbox is 0x44fd70.
std::uint64_t scissor(std::int32_t left, std::int32_t top, std::int32_t right,
                      std::int32_t bottom, bool interlaced, bool both_fields,
                      int parity, std::int32_t letterbox);
int visibleColumns(int origin, std::int32_t right, int width = 100);

// bar_green%02d.tex shape measured in HUD.LMP (counts and shapes only).
struct BarGreenShape {
  int entries, used_width, used_height, surface_width, surface_height,
      upload_width, upload_height, palette_width, palette_height, rows;
};
BarGreenShape barGreenShape();

// ---- starting and respawn current health/mana (hud-xp.md part B) -------
inline constexpr Raw INIT_CURRENT = 0x4e6e6b27u; // 999999936.0
inline constexpr Raw INIT_MANA_MAX = 0x41200000u; // 10.0
struct InitCurrent {
  Raw hp{}, hp_max{}, mana{}, mana_max{};
};
// What player_initNewCharacter leaves in +0xac/+0xb0/+0xc0/+0xc4.
InitCurrent afterInit();
// 0x1b6a24-0x1b6a48: current = maximum only when maximum < current.
Raw clampCurrent(Raw current, Raw maximum);
bool recalcClamps(int slot, int num_players, bool has_object);
// The first level's HUD frame: clamp the init value to recalculated maxima.
InitCurrent firstLevelCurrent(Raw hp_max, Raw mana_max, Raw current = INIT_CURRENT);
// player_respawn 0x1bd040: health = max (PvP) or max*0.5; mana unchanged.
Raw respawnHp(Raw hp_max, bool pvp_a);

// ---- gold counter chain (hud-gold.md) -----------------------------------
inline constexpr std::int32_t GOLD_CAP = 99999999;
inline constexpr int FLOATER_COUNT = 8;
struct GoldFloater {
  std::array<Raw, 3> pos{}, vel{};
  std::uint32_t start{}, amount{}; // amount 0 = free
  std::int32_t slot{};            // s8
  std::uint8_t offscreen{};       // written, never read
};
// Screen-side start of a floater (hud-gold.md section 6). It needs the camera
// projection; the caller supplies it (or approximates it, it changes no shared
// state). One cosmetic seed draw per filled floater is owed by the caller.
struct FloaterStart {
  std::array<Raw, 3> pos{}, vel{};
  std::uint8_t offscreen{};
};
struct GoldState {
  std::array<GoldFloater, FLOATER_COUNT> floaters{};
  std::array<std::int32_t, 4> pending{}, hold{}, alpha{};
  std::array<std::uint32_t, 4> gold{}; // record +0xe4 words
  std::uint32_t frame{};
  int cosmetic_draws = 0; // filled floaters, i.e. cosmetic draws owed
};
std::uint32_t goldCredit(std::uint32_t word, std::uint32_t amount);
// Returns the floater index, or -1 when all eight were busy (credited at once).
int addGoldFloater(GoldState &, int slot, std::uint32_t amount,
                   const FloaterStart &);
void tickGold(GoldState &);
void flushGold(GoldState &);
void arenaBonusLife(GoldState &, int slot);
std::int32_t goldFloaterAlpha(std::uint32_t frame, std::uint32_t start);
// ui_insertThousandsSeparators with both quirks; language s8 0 -> ',' else '.'.
std::string formatGold(std::int32_t value, int language);
std::string insertSeparators(const std::string &digits, int language);
struct GoldFloaterText {
  int x{}, y{};
  std::string text;
  std::uint32_t colour{};
};
struct GoldCounterText {
  int slot{}, x{}, y{};
  std::string text;
  std::uint32_t colour{};
};
struct GoldCoin {
  int slot{}, x{}, y{};
  std::uint32_t tint{};
};
struct GoldDraw {
  std::vector<GoldFloaterText> floaters;
  std::vector<GoldCounterText> counters;
  std::vector<GoldCoin> coins;
};
// hud_drawGold, once per displayed frame. Steps floaters, hold and alpha.
GoldDraw drawGold(GoldState &, const std::array<bool, 4> &local,
                  const std::function<int(const std::string &)> &text_width,
                  int language, bool hud_off = false);

// ---- quick slots (hud-quickslots.md) ------------------------------------
inline constexpr int NOTICE_SHORT = 120, NOTICE_LONG = 240;
struct Bag2Cell {
  int kind = -1;  // 0 health, 1 mana, -1 anything else
  int size = -1;  // potion size 0..8 within kind
  std::int32_t quantity{}; // s16
};
int countPotions(const std::vector<Bag2Cell> &bag, int kind);
int potionSize(Raw missing_plus_one, const std::array<int, 9> &found);
struct PotionUse {
  bool returned_early = true;
  std::vector<std::pair<int, int>> notices; // (line, frames)
  int size = -2;                           // -2: not reached, -1: none found
  Raw pending{};
  int count = -1;
  std::optional<int> removed_cell;
  bool sent_drop = false;
  std::optional<std::array<Raw, 2>> regen; // R+0xb8, R+0xbc
  std::vector<Bag2Cell> bag;
};
struct PotionInput {
  bool copy = false;
  Raw hp{}, max_hp{}, hp_pending{}, mana{}, max_mana{}, mana_pending{};
  std::vector<Bag2Cell> bag;
  bool local = true, menu_open = false, grid = false;
  int kind = 0;
};
PotionUse usePotion(const PotionInput &);
// Selector (hud_spellSelect) input half.
enum Gate { MenuLockout, Dialog, Camera, Paused, PauseMenu, MemCard, Medal,
            Jumpgate, InputBlock, GateCount };
bool padReadable(const std::array<bool, GateCount> &gates, bool local_menu_open);
struct PadAxes {
  int dx{}, dy{};
  bool any{};
};
PadAxes padAxes(std::uint32_t bits);
int buttonGlyph(std::uint32_t config_mask);
struct SelectorMessage {
  int id = -1;   // 0 quick slot, 1 assign, 13 quick cast; -1 none
  int count = 0; // arguments used
  // quick slot: slot, button (0 A, 1 B), dir; quick cast: slot, dir;
  // assign: slot, dir, kind, value.
  std::array<std::int64_t, 4> args{};
};
struct SelectorButtons {
  std::uint32_t a{}, b{}, use{}, accept{};
};
SelectorMessage selectorMessage(int slot, int dir, bool menu_mode,
                                std::uint32_t pressed, const SelectorButtons &,
                                int kind, int value);
struct MaskEntry {
  std::int32_t frames{};
  std::uint32_t mask{}, third{};
};
// net_maskLocalButtons update when any direction bit is held.
MaskEntry maskLocalButtons(MaskEntry, const SelectorButtons &);
enum class IconSource { Blank, Skill, Table };
IconSource recordIcon(int id, bool newstyle);

// ---- minimap projector 0x168cac (hud-minimap.md) ------------------------
struct MinimapInput {
  bool world_texture = false;
  std::int32_t free_units{};
  std::int32_t yaw{}; // s16 camera yaw
  std::array<Raw, 2> centre{}, origin{}; // camera x,y; cell origin 0x4044a8/ac
  Raw offx{}, offy{}, zoom{}, half_w{}, half_h{};
  int width{}, height{}; // grid W/H 0x4044dc/e0
};
struct MinimapView {
  bool drawn = false; // gate passed and visible cell range non-empty
  bool gate = false;
  int sx0{}, sy0{}, sx1{}, sy1{}; // clamped scissor rectangle
  int col_min{}, row_min{}, col_end{}, row_end{};
  Raw col_x{}, col_y{}, row_x{}, row_y{}; // pixel steps per column and row
  Raw base_x{}, base_y{};                 // screen of cell (0,0) minus (320,224)
  Raw cos_t{}, sin_t{};                   // of minus yaw
};
bool minimapGate(bool world_texture, std::int32_t free_units, Raw half_w);
std::array<Raw, 2> worldToCell(Raw x, Raw y, const std::array<Raw, 2> &origin);
MinimapView minimapView(const MinimapInput &);
struct Blip {
  Raw x{}, y{};
  std::int32_t angle{}; // object +0x38
  std::uint32_t r{}, g{}, b{};
  Raw scale{};
};
struct BlipTriangles {
  std::array<std::array<Raw, 2>, 3> shadow{}, colour{}; // screen x,y
  std::uint32_t colour_word{};
};
// Needs a view that passed the gate and zoom > 0.125 (otherwise no blips).
std::optional<BlipTriangles> blipTriangles(const MinimapInput &, const MinimapView &,
                                           const Blip &, Raw blip_size);
bool blipsEnabled(Raw zoom);
// Alpha of one map icon from its cell's explored nibble (cell high byte).
int iconAlpha(std::uint8_t cell_high_byte);
// Refused. See the header comment.
[[noreturn]] void minimapTiles();
[[noreturn]] void minimapIcons();
} // namespace champions::hud_extras
