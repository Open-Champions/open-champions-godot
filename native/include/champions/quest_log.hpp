#pragma once
// Return to Arms inventory quest page (page 4, stats_screen mode 1): the text
// calls the original submits for a player's stored quest list. Research
// t3/research-quest-log 965ec52 docs/spec/quest-log.md sections 2 and 6
// (lines 72-77 and 252-265), model tools/ref_quest_log.py lines 7-41 and
// 121-133, executed by tools/difftest_quest_log_state.py lines 235-322.
//
// These are call arguments, not rendered pixel positions. Three things stay
// Unknown and are not modelled here, so this is not pixel parity:
//  - the integer a loaded font's text_drawWrapped returns (the caller supplies
//    each return; y after the first title depends on it, quest-log.md 288);
//  - what 0x1802e0 stores (the probe stubbed it, quest-log.md 75 and 289);
//  - the parchment sprite's displayed crop and the role of its third argument
//    (quest-log.md 74 and 290), so the parchment submission is not emitted.
#include "champions/quests.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace champions::simulation::rta::quest_log {
// quest-log.md 75 and 255: the page passes the word at 0x4f7104 as the font.
// research-ui 3e39392 docs/formats/ui-fonts.md 74-76 names it gamefontsmall.
inline constexpr std::uint32_t FONT_WORD = 0x4f7104;
inline constexpr std::int64_t TEXT_LAYER = 6;
inline constexpr std::uint32_t TITLE_COLOUR = 0x80323c3c;
inline constexpr std::uint32_t BODY_COLOUR = 0x802d2d2d;
inline constexpr std::array<std::int64_t, 4> CLIP = {0, 119, 320, 440};
inline constexpr std::int64_t ROW_TOP = 114;
inline constexpr std::int64_t ROW_STEP = 22;
inline constexpr std::int64_t WRAP_X = 24;
inline constexpr std::array<std::int64_t, 4> WRAP_TAIL = {288, 82, 418, 21};
inline constexpr std::int64_t EMPTY_X = 160;
inline constexpr std::int64_t EMPTY_Y = 224;
inline constexpr std::int64_t EMPTY_COUNT = 1000000;
// The unnamed callee at 0x1802e0. Its arguments are Observed; its stores are
// Unknown (quest-log.md 75).
inline constexpr char INK_CALL[] = "0x1802e0";

struct Error : std::runtime_error {
  using std::runtime_error::runtime_error;
};

// One original call. text_lookup carries the key in `key` and no arguments.
// A text pointer argument is the stand-in `lookup + 1` (1-based index into
// Page::lookups), never a host address.
struct Call {
  std::string name;
  std::vector<std::int64_t> args;
  std::string key;
};
struct Row {
  std::int64_t title_y = 0;
  std::int64_t body_y = 0;
};
struct Page {
  std::vector<Call> calls;
  std::vector<std::string> lookups; // keys in lookup order
  std::vector<Row> rows;            // one per stored entry, storage order
  bool empty = false;
};

// The text_drawWrapped return for the wrap that draws lookup `index` (0-based,
// title then body per entry). The value for a loaded font is Unknown; the
// caller supplies it.
using WrapReturn = std::function<std::int32_t(std::size_t index)>;

// The page's calls for one record's list. `font` is the value text_begin
// receives (the word at FONT_WORD). A count outside 0..8 or a stored field
// with no terminator is refused. With count 0 no wrap return is needed. With
// count > 0 a missing `wrap` is refused.
Page page_calls(const std::array<quests::QuestLine, quests::QUEST_SLOTS> &quests, int count,
                std::uint32_t font, const WrapReturn &wrap);

// ref_quest_log.row_positions: y of each title and body for the given wrap
// returns. The page adds each return, then ROW_STEP after the body.
std::vector<Row> row_positions(const std::vector<std::int32_t> &title_returns,
                               const std::vector<std::int32_t> &body_returns);
} // namespace champions::simulation::rta::quest_log
