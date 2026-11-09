#include "champions/quest_log.hpp"

#include <algorithm>

namespace champions::simulation::rta::quest_log {
namespace {
// cstr of a stored 32-byte field. A field with no terminator would read on
// into the next field on the console; that read is refused.
std::string stored_key(const quests::Field &field) {
  if (std::find(field.begin(), field.end(), std::uint8_t(0)) == field.end())
    throw Error("quest field has no terminator");
  return quests::field_text(field);
}
Call call(const char *name, std::vector<std::int64_t> args) { return Call{name, std::move(args), {}}; }
// text_begin(font, 6, 0x80323c3c, -1): quest-log.md 75 and 259; the empty
// branch passes the same four arguments (quest-log.md 77 and 263).
Call begin(std::uint32_t font) {
  return call("text_begin", {std::int64_t(font), TEXT_LAYER, std::int64_t(TITLE_COLOUR), -1});
}
std::int64_t lookup(Page &page, const std::string &key) {
  page.lookups.push_back(key);
  page.calls.push_back(Call{"text_lookup", {}, key});
  return std::int64_t(page.lookups.size());
}
Call wrapped(std::int64_t y, std::int64_t pointer) {
  return call("text_drawWrapped",
              {WRAP_X, y, pointer, WRAP_TAIL[0], WRAP_TAIL[1], WRAP_TAIL[2], WRAP_TAIL[3]});
}
} // namespace

Page page_calls(const std::array<quests::QuestLine, quests::QUEST_SLOTS> &quests, int count,
                std::uint32_t font, const WrapReturn &wrap) {
  if (count < 0 || count > quests::QUEST_SLOTS)
    throw Error("quest count is outside 0..8");
  Page page;
  if (count == 0) {
    // Empty list: begin at 0x1d27d0, lookup of the placeholder key at
    // 0x1d27dc, text_drawWideCentered(160, 224, pointer, 1000000) at
    // 0x1d2800, end at 0x1d2808. No clip and no wrap (quest-log.md 77, 263-265).
    page.empty = true;
    page.calls.push_back(begin(font));
    const auto pointer = lookup(page, quests::QUEST_PLACEHOLDER);
    page.calls.push_back(call("text_drawWideCentered", {EMPTY_X, EMPTY_Y, pointer, EMPTY_COUNT}));
    page.calls.push_back(call("text_end", {}));
    return page;
  }
  if (!wrap)
    throw Error("text_drawWrapped return required: a loaded font's return is Unknown");
  // Keys first, so a field with no terminator refuses before any call is kept.
  std::vector<std::pair<std::string, std::string>> keys;
  for (int index = 0; index < count; ++index) {
    const auto &line = quests[static_cast<std::size_t>(index)];
    keys.emplace_back(stored_key(line.title), stored_key(line.text));
  }
  // Storage order, one batch per entry (quest-log.md 75, 219-221, 259-263).
  // y starts at 114, each wrap return is added, then 22 after the body
  // (ref_quest_log.py 121-132).
  std::int64_t y = ROW_TOP;
  for (const auto &entry : keys) {
    Row row;
    page.calls.push_back(begin(font));
    page.calls.push_back(call("text_setClip", {CLIP[0], CLIP[1], CLIP[2], CLIP[3]}));
    page.calls.push_back(call(INK_CALL, {std::int64_t(TITLE_COLOUR), -1}));
    const auto title = lookup(page, entry.first);
    row.title_y = y;
    page.calls.push_back(wrapped(y, title));
    y += wrap(page.lookups.size() - 1);
    page.calls.push_back(call(INK_CALL, {std::int64_t(BODY_COLOUR), -1}));
    const auto body = lookup(page, entry.second);
    row.body_y = y;
    page.calls.push_back(wrapped(y, body));
    y += std::int64_t(wrap(page.lookups.size() - 1)) + ROW_STEP;
    page.calls.push_back(call("text_end", {}));
    page.rows.push_back(row);
  }
  return page;
}

std::vector<Row> row_positions(const std::vector<std::int32_t> &title_returns,
                               const std::vector<std::int32_t> &body_returns) {
  if (title_returns.size() != body_returns.size())
    throw Error("one return pair per entry");
  std::vector<Row> rows;
  std::int64_t y = ROW_TOP;
  for (std::size_t i = 0; i < title_returns.size(); ++i) {
    Row row;
    row.title_y = y;
    y += title_returns[i];
    row.body_y = y;
    y += body_returns[i] + ROW_STEP;
    rows.push_back(row);
  }
  return rows;
}
} // namespace champions::simulation::rta::quest_log
