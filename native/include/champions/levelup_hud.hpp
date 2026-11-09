#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <vector>

// Observed level-up and automap producers that the client did not have yet.
// research-levelup 98a8a47 docs/spec/levelup.md
// research-hud-inputs fb24f5e docs/spec/hud-live-producers.md
// Synthetic inputs only. Retail experience words and skill trees are not embedded.
//
// Already present elsewhere and not duplicated here:
//   bars and the green line        ui_layout barClipRight / hudBaseLayers
//   per-tick health and mana regen  player.cpp Tick::regen_health / regen_mana
//   hud_showMessage / tickMessage   strings.hpp Hud::show_message / tick_message
//   message alpha                   hud_dynamic messageAlphaStep / messageFrame
//   save and restore                menus Ledger::save / load
namespace champions::levelup_hud {

struct Refusal : std::runtime_error {
  using std::runtime_error::runtime_error;
};

// player_giveExp asks hud_showMessage for 360 frames and force 0 on each
// level step. hud-live-producers.md:120-122, 204.
inline constexpr std::uint32_t kLevelMessageFrames = 0x168;
inline constexpr std::uint32_t kLevelMessageForce = 0;

struct ExperienceState {
  std::int32_t experience = 0;
  std::int32_t level = 1;        // signed byte +0x59
  std::uint32_t skill_word = 0;  // +0xe8
  std::uint8_t stat_byte = 0;    // +0xec
  std::uint32_t health = 0, mana = 0, max_health = 0, max_mana = 0;
};
struct ExperienceResult {
  ExperienceState state;
  int steps = 0;  // level steps, one hud_showMessage call each
};
// player_giveExp 0x1d3150. The 81-word table is supplied by the caller.
// Acknowledged level +0x58 is not an input and is not written.
// levelup.md:28-62. hud-live-producers.md:86-112.
// recalc runs after each level increment (player_recalcStats 0x1b6458) and
// must leave the new maxima in the state; the refill reads them.
ExperienceResult give_experience(ExperienceState state, std::int32_t amount,
                                 const std::array<std::int32_t, 81> &table,
                                 const std::function<void(ExperienceState &)> &recalc);

// Skill word added for the level being left. levelup.md:33-34, 49-50.
int skill_grant(int level_being_left);

// Cursor position against the row box. Only interior and outside were
// executed; the inclusive edges were not probed. levelup.md:98-103.
enum class Cursor { Inside, Outside, Edge };

struct SkillAsk {
  int slot = 0;
  int class_id = 0;              // record +0x50, selects the tree
  std::int32_t points = 0;       // signed +0xe8
  int acknowledged = 0;          // signed byte +0x58
  std::uint32_t max_mana = 0;    // float +0xc4
  // The caller's tree row (kind, need, ident, prerequisite kind and value).
  int kind = 0;
  int need = 0;
  std::uint64_t ident = 0;
  int pre_kind = -1;
  std::uint64_t pre = 0;
  bool in_table = true;          // the passive-table walk saw a row before END
  bool resource = true;          // first picture lookup at 0x1d47b4 was nonzero
  Cursor cursor = Cursor::Inside;
  std::uint32_t edge = 0;        // pad +0xc
  std::uint32_t confirm_mask = 0x40;  // word at 0x41bfbc
  std::uint32_t held = 0;        // pad +8
  int page = 0;                  // panel +0x50
  std::uint32_t gate = 0;        // word at 0x4f4878 before the call
  int panel30 = 0;               // panel +0x30
  // The float the skill's virtual entry vtable+0x28 returns. The bodies were
  // not run, so a kind-1 row needs this planted. levelup.md:150-158.
  std::uint32_t virtual_float = 0;
  bool virtual_float_planted = false;
  std::function<int(int)> skill_rank;
  std::function<int(std::uint64_t)> passive_rank;
};
struct SkillDecision {
  bool sent = false;    // invmsg_sendSkillPoint was called
  bool recalc = false;  // player_recalcStats at 0x1d5950 was called
  int slot = 0;
  int kind = 0;         // 0 passive mask, 1 skill id
  std::uint32_t value = 0;
};
// skills_screen 0x1d4660 sender. levelup.md:92-164.
SkillDecision skill_screen(const SkillAsk &ask);

// The executed extra unlocks: (class, row kind, row ident) also unlocked by
// rank >= 1 of the skill. levelup.md:121-131.
struct Alternative {
  int class_id;
  int kind;
  std::uint64_t ident;
  int skill;
};
const std::vector<Alternative> &alternatives();

// Fixed automap grid side. tools/ref_hud_inputs.py:10-11.
inline constexpr int kAutomapSide = 216;
// Automap reveal 0x16a4b0 from a world position, fractional cells included.
// origin and world are raw float words. hud-live-producers.md:146-165.
// tools/ref_hud_inputs.py:77-132.
void reveal_world(std::vector<std::uint16_t> &grid, int width, int height,
                  std::uint32_t origin_x, std::uint32_t origin_y,
                  std::uint32_t world_x, std::uint32_t world_y);

} // namespace champions::levelup_hud
