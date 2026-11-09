// Quest log: list order and writers, the page's text calls, and the player
// record copy of the quest span. Research t3/research-quest-log 965ec52
// docs/spec/quest-log.md section 6 (lines 205-302) and tools/ref_quest_log.py.
// Synthetic keys only (A, B, Q0, B0 ...); no game text.
#include "champions/amx.hpp"
#include "champions/quest_log.hpp"
#include "champions/quests.hpp"
#include "champions/script_builtins.hpp"
#include "champions/script_commands.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace q = champions::simulation::rta::quests;
namespace ql = champions::simulation::rta::quest_log;
namespace b = champions::simulation::rta::builtins;
namespace sc = champions::simulation::rta::script_commands;
namespace a = champions::simulation::rta::amx;

namespace {
int failed = 0;
void check(bool ok, const std::string &what) {
  if (ok)
    return;
  std::cerr << "FAIL " << what << '\n';
  ++failed;
}
template <class F> bool refuses(F &&f) {
  try {
    f();
  } catch (const std::exception &) {
    return true;
  }
  return false;
}

// Binder layout (quest-log.md 25-29): g_players 0x4d8af0, stride 0x4c30,
// quest pairs +0x41d0 (8 x 0x40), count +0x43d0. g_numPlayers 0x4d8ae0,
// g_difficultyIdx 0x4f719c, mission bytes +0x4bbf + d*13 + m, medals +0x4c0d.
constexpr std::uint32_t PLAYERS_AT = 0x4d8af0u, STRIDE = 0x4c30u, QUEST_AT = 0x41d0u,
                        COUNT_AT = 0x43d0u, NUM_PLAYERS = 0x4d8ae0u, DIFFICULTY = 0x4f719cu,
                        MISSIONS_AT = 0x4bbfu, MEDALS_AT = 0x4c0du, MEDAL_SLOT = 0x4f7148u;
// The span the research compares: eight entries and the count (SPAN 0x204,
// research-quest-log-r2-fix audit, difftest_quest_log_state.py 339-344).
constexpr std::uint32_t SPAN = 0x204u;
std::uint32_t rec(int slot) { return PLAYERS_AT + std::uint32_t(slot) * STRIDE; }
std::vector<std::uint8_t> span(const b::Memory &m, int slot) { return m.read(rec(slot) + QUEST_AT, SPAN); }
void fill_span(b::Memory &m, int slot, std::uint8_t byte, std::int32_t count) {
  m.write(rec(slot) + QUEST_AT, std::vector<std::uint8_t>(8 * 0x40, byte));
  m.word(rec(slot) + COUNT_AT, std::uint32_t(count));
}
void plant(b::Memory &m, int slot, int index, const std::string &title, const std::string &body) {
  m.string(rec(slot) + QUEST_AT + std::uint32_t(index) * 0x40u, title);
  m.string(rec(slot) + QUEST_AT + std::uint32_t(index) * 0x40u + 0x20u, body);
}
std::string title_at(const b::Memory &m, int slot, int index) {
  return m.cstr(rec(slot) + QUEST_AT + std::uint32_t(index) * 0x40u);
}
std::string body_at(const b::Memory &m, int slot, int index) {
  return m.cstr(rec(slot) + QUEST_AT + std::uint32_t(index) * 0x40u + 0x20u);
}
bool tail_is(const b::Memory &m, std::uint32_t field, const std::string &text, std::uint8_t byte) {
  if (m.cstr(field) != text || m.read(field + std::uint32_t(text.size())) != 0)
    return false;
  for (std::uint32_t i = std::uint32_t(text.size()) + 1; i < 0x20u; ++i)
    if (m.read(field + i) != byte)
      return false;
  return true;
}

// Campaign helpers.
q::Field filled(std::uint8_t byte) {
  q::Field f;
  f.fill(byte);
  return f;
}
void fill_record(q::Record &r, std::uint8_t byte) {
  for (auto &line : r.quests) {
    line.title = filled(byte);
    line.text = filled(byte);
  }
}
void put(q::Field &f, const std::string &text) {
  for (std::size_t i = 0; i < text.size(); ++i)
    f[i] = static_cast<std::uint8_t>(text[i]);
  f[text.size()] = 0;
}
bool field_tail(const q::Field &f, const std::string &text, std::uint8_t byte) {
  if (q::field_text(f) != text || f[text.size()] != 0)
    return false;
  for (std::size_t i = text.size() + 1; i < f.size(); ++i)
    if (f[i] != byte)
      return false;
  return true;
}
bool same_lists(const q::Record &left, const q::Record &right) {
  if (left.quest_count != right.quest_count)
    return false;
  for (std::size_t i = 0; i < left.quests.size(); ++i)
    if (left.quests[i].title != right.quests[i].title || left.quests[i].text != right.quests[i].text)
      return false;
  return true;
}

// quest-log.md 213 (Add): append on each player in order; return at the
// first player whose count is 8 or whose stored title matches; a later call
// still runs; bytes past the new terminator stay 0x5a.
void test_add() {
  q::Campaign c;
  c.set_num_players(3);
  for (int s = 0; s < 3; ++s) {
    auto r = c.record(s);
    fill_record(r, 0x5a);
    c.set_record(s, r);
  }
  c.quest_add("A", "BA");
  c.quest_add("B", "BB");
  for (int s = 0; s < 3; ++s) {
    const auto &r = c.record(s);
    check(r.quest_count == 2 && field_tail(r.quests[0].title, "A", 0x5a) &&
              field_tail(r.quests[0].text, "BA", 0x5a) && field_tail(r.quests[1].title, "B", 0x5a) &&
              field_tail(r.quests[1].text, "BB", 0x5a) && r.quests[2].title == filled(0x5a),
          "add: storage order on every player, tail bytes kept, next free field untouched");
  }
  // Player 1 already has the title: player 0 is left as is (it too has it,
  // so the call returns at player 0). Plant a list where only player 1 has C.
  auto r1 = c.record(1);
  put(r1.quests[2].title, "C");
  put(r1.quests[2].text, "BC");
  r1.quest_count = 3;
  c.set_record(1, r1);
  const auto p2_before = c.record(2);
  c.quest_add("C", "BC");
  check(c.record(0).quest_count == 3 && q::field_text(c.record(0).quests[2].title) == "C",
        "add: the player before the duplicate gets the quest");
  check(c.record(1).quest_count == 3, "add: the duplicate player's list is unchanged");
  check(same_lists(c.record(2), p2_before), "add: the call returns, so a later player does not get it");
  // A later separate call still runs.
  c.quest_add("D", "BD");
  check(c.record(0).quest_count == 4 && c.record(1).quest_count == 4 && c.record(2).quest_count == 3 &&
            q::field_text(c.record(2).quests[2].title) == "D",
        "add: a later call still runs on every player");
  // Full list at player 0 returns before any player.
  q::Campaign full;
  full.set_num_players(2);
  for (int i = 0; i < 8; ++i)
    full.quest_add("F" + std::to_string(i), "X");
  auto p1 = full.record(1);
  p1.quest_count = 0;
  full.set_record(1, p1);
  const auto before0 = full.record(0);
  full.quest_add("G", "X");
  check(same_lists(full.record(0), before0) && full.record(1).quest_count == 0,
        "add: a full list stops the whole call");
}

// quest-log.md 214-215: remove covers every match with the nine-byte
// placeholder and slides only the entries after the last match.
void test_remove() {
  q::Campaign c;
  c.set_num_players(1);
  auto r = c.record(0);
  fill_record(r, 0xee);
  put(r.quests[0].title, "A");
  put(r.quests[0].text, "BA");
  put(r.quests[1].title, "B");
  put(r.quests[1].text, "BB");
  put(r.quests[2].title, "A");
  put(r.quests[2].text, "BA");
  r.quest_count = 3;
  c.set_record(0, r);
  c.quest_remove("A");
  const auto &d = c.record(0);
  check(d.quest_count == 2 && field_tail(d.quests[0].title, q::QUEST_PLACEHOLDER, 0xee) &&
            field_tail(d.quests[0].text, q::QUEST_PLACEHOLDER, 0xee) && q::field_text(d.quests[1].title) == "B" &&
            field_tail(d.quests[2].title, q::QUEST_PLACEHOLDER, 0xee),
        "remove: A,B,A becomes visible NoQuests,B; the last match is covered, no slide");

  q::Campaign one;
  one.set_num_players(1);
  for (const char *t : {"A", "B", "C"})
    one.quest_add(t, std::string("B") + t);
  one.quest_remove("B");
  const auto &o = one.record(0);
  check(o.quest_count == 2 && q::field_text(o.quests[0].title) == "A" && q::field_text(o.quests[1].title) == "C" &&
            q::field_text(o.quests[1].text) == "BC" && q::field_text(o.quests[2].title) == "C",
        "remove: A,B,C without B slides C up once; the old slot keeps its bytes");
  // The copied "C\0" leaves the placeholder's remainder under it.
  check(o.quests[1].title[2] == 'Q' && o.quests[1].title[8] == 0, "remove: strcpy slide keeps placeholder tail");
}

// quest-log.md 216: mission_setComplete does not touch the quest list.
void test_complete() {
  q::Campaign c;
  c.set_num_players(1);
  for (int s = 0; s < q::PLAYERS; ++s) {
    auto r = c.record(s);
    fill_record(r, s == 3 ? 0x3c : 0x5a);
    r.quest_count = s;
    r.missions.fill(0xf5);
    c.set_record(s, r);
  }
  std::vector<q::Record> before;
  for (int s = 0; s < q::PLAYERS; ++s)
    before.push_back(c.record(s));
  c.set_difficulty(1);
  c.mission_set_complete(1, -1);
  const auto at = static_cast<std::size_t>(q::mission_index(1, 1));
  for (int s = 0; s < q::PLAYERS; ++s) {
    const auto &r = c.record(s);
    bool others = true;
    for (std::size_t i = 0; i < r.missions.size(); ++i)
      if (i != at && r.missions[i] != 0xf5)
        others = false;
    check(r.missions[at] == 0xf7 && others && same_lists(r, before[static_cast<std::size_t>(s)]),
          "complete -1: bit 1 on all four records, other bits kept, list untouched");
  }
  c.mission_set_complete(1, 0);
  for (int s = 0; s < q::PLAYERS; ++s)
    check(c.record(s).missions[at] == 0xf5 && same_lists(c.record(s), before[static_cast<std::size_t>(s)]),
          "complete 0: bit 1 cleared, list untouched");
  c.mission_set_complete(13, -1);
  for (int s = 0; s < q::PLAYERS; ++s)
    check(c.record(s).missions == before[static_cast<std::size_t>(s)].missions, "complete: mission 13 writes nothing");

  // The binder: no recorded write inside any of the four quest spans.
  b::Variables v;
  auto &m = v.memory;
  m.word(NUM_PLAYERS, 1);
  m.word(DIFFICULTY, 1);
  for (int s = 0; s < 4; ++s) {
    fill_span(m, s, s == 3 ? 0x3c : 0x5a, s);
    m.write(rec(s) + MISSIONS_AT + 13u, std::vector<std::uint8_t>(13 * 5, 0xf5));
  }
  std::vector<std::vector<std::uint8_t>> spans;
  for (int s = 0; s < 4; ++s)
    spans.push_back(span(m, s));
  m.writes.clear();
  v.mission_set_complete(1, 1, -1);
  v.mission_set_complete(1, 1, 0);
  v.mission_set_complete(1, 13, -1);
  bool clean = true;
  for (const auto &w : m.writes)
    for (int s = 0; s < 4; ++s) {
      const auto lo = rec(s) + QUEST_AT, hi = lo + SPAN;
      if (w.first < hi && w.first + w.second > lo)
        clean = false;
    }
  for (int s = 0; s < 4; ++s)
    clean = clean && span(m, s) == spans[static_cast<std::size_t>(s)];
  check(clean && !m.writes.empty(), "binder complete: mission writes only, no store in any quest span");
  check(m.read(rec(2) + MISSIONS_AT + 14u) == 0xf5, "binder complete: 0 restores the seeded byte");
}

// quest-log.md 217: quest_clearAll covers the first g_numPlayers lists only.
void test_clear() {
  q::Campaign c;
  c.set_num_players(3);
  for (const char *t : {"A", "B"})
    c.quest_add(t, "X");
  c.set_num_players(2);
  c.var_set("step", 5);
  const auto third = c.record(2);
  c.quest_clear_all();
  for (int s = 0; s < 2; ++s) {
    const auto &r = c.record(s);
    check(r.quest_count == 0 && q::field_text(r.quests[0].title) == q::QUEST_PLACEHOLDER &&
              q::field_text(r.quests[1].text) == q::QUEST_PLACEHOLDER && q::field_text(r.quests[2].title).empty(),
          "clear: used entries covered, count 0");
  }
  check(same_lists(c.record(2), third) && c.record(2).quest_count == 2, "clear: a later record keeps its list and count");
  check(c.vars().size() == 1 && c.vars()[0].value == 5 && c.events().act_resets == 0,
        "clear: quest_clearAll is the list only; no variable or act reset");
}

struct Built {
  a::ByteVector image;
};
struct Call {
  std::string native;
  std::vector<a::Cell> args;
};
Built build_calls(const std::vector<Call> &calls) {
  std::vector<std::string> natives;
  auto native_index = [&](const std::string &name) {
    auto found = std::find(natives.begin(), natives.end(), name);
    if (found == natives.end()) {
      natives.push_back(name);
      return a::Cell(natives.size() - 1);
    }
    return a::Cell(found - natives.begin());
  };
  std::vector<a::AsmLine> program;
  auto ins = [&](const std::string &name, std::vector<a::AsmValue> args = {}) {
    program.push_back(a::AsmInstruction{name, std::move(args), {}, {}});
  };
  program.push_back(a::Label{"start:"});
  ins("PROC");
  for (const auto &call : calls) {
    for (auto it = call.args.rbegin(); it != call.args.rend(); ++it)
      ins("PUSH.C", {*it});
    ins("PUSH.C", {a::Cell(4 * call.args.size())});
    ins("SYSREQ.C", {native_index(call.native)});
    ins("STACK", {a::Cell(4 * (call.args.size() + 1))});
  }
  ins("ZERO.pri");
  ins("RETN");
  const auto code = a::assemble(program);
  a::ImageOptions options;
  options.data.resize(4096);
  options.natives = natives;
  options.stack_cells = 256;
  options.publics.emplace_back("start", a::Cell(code.labels.at("start")));
  return Built{a::build_image(code.cells, options)};
}

// quest-log.md 225-232: addQuest and removeQuest are the only script
// commands that write a journal entry. Every other campaign command the
// host runs leaves the binder's quest span byte-identical.
void test_writers() {
  const std::vector<Call> others = {
      {"getv", {2000}},           {"setv", {2000, 7}},          {"addv", {2000, 1}},
      {"setMissionAvailable", {1, 1}}, {"getMissionAvailable", {1}}, {"grantMission", {1}},
      {"getMissionComplete", {1}}, {"setEvil", {1}},             {"getEvil", {}},
      {"setMadeDecision", {}},     {"getMadeDecision", {}},      {"getMedal", {1}},
      {"setPlayerFlag", {-1, 0, 1}}, {"getPlayerFlag", {0, 0}},  {"givePlayerGold", {5}},
      {"activateMissionMenu", {}}};
  auto run = [&](const std::vector<Call> &calls, bool &ok) {
    sc::Host host;
    ok = host.load(build_calls(calls).image) && host.machine();
    if (!ok)
      return std::vector<std::uint8_t>{};
    host.machine()->set_string(2000, "Step", false);
    host.machine()->set_string(2100, "A", false);
    host.machine()->set_string(2200, "BA", false);
    host.machine()->set_string(2300, "Z", false);
    host.machine()->set_string(2400, "BZ", false);
    auto &m = host.variables().memory;
    m.word(NUM_PLAYERS, 1);
    m.word(DIFFICULTY, 1);
    fill_span(m, 0, 0x5a, 1);
    plant(m, 0, 0, "A", "BA");
    const auto before = span(m, 0);
    host.run_prestart();
    ok = host.run_start() == sc::Stop::Complete && host.counts().faults == 0;
    const auto after = span(m, 0);
    return std::vector<std::uint8_t>(after.begin(), after.end()) == before ? std::vector<std::uint8_t>{}
                                                                           : after;
  };
  bool ok = false;
  const auto changed = run(others, ok);
  check(ok, "writers: the other campaign commands complete");
  check(changed.empty(), "writers: no other campaign command writes the quest span");
  for (const auto &one : others) {
    bool each = false;
    check(run({one}, each).empty() && each, "writers: " + one.native + " leaves the quest span");
  }
  bool added = false, removed = false;
  check(!run({{"addQuest", {2300, 2400}}}, added).empty() && added, "writers: addQuest writes the span");
  check(!run({{"removeQuest", {2100}}}, removed).empty() && removed, "writers: removeQuest writes the span");
}

// quest-log.md 219-223, 252-265, 72-77: the page's calls.
q::Record page_record(const std::vector<std::string> &titles) {
  q::Record r;
  fill_record(r, 0xee);
  for (std::size_t i = 0; i < titles.size(); ++i) {
    put(r.quests[i].title, titles[i]);
    put(r.quests[i].text, "B" + titles[i].substr(1));
  }
  r.quest_count = int(titles.size());
  return r;
}
bool call_is(const ql::Call &c, const char *name, std::vector<std::int64_t> args, const std::string &key = {}) {
  return c.name == name && c.args == args && c.key == key;
}
void test_page() {
  const std::uint32_t font = 0x11111111u;
  // Empty: begin, lookup NoQuests, drawWideCentered(160, 224, ptr, 1000000), end.
  const auto empty = ql::page_calls(page_record({}).quests, 0, font, {});
  check(empty.empty && empty.calls.size() == 4 && call_is(empty.calls[0], "text_begin", {font, 6, 0x80323c3c, -1}) &&
            call_is(empty.calls[1], "text_lookup", {}, "NoQuests") &&
            call_is(empty.calls[2], "text_drawWideCentered", {160, 224, 1, 1000000}) &&
            call_is(empty.calls[3], "text_end", {}) && empty.lookups == std::vector<std::string>{"NoQuests"},
        "page: empty state, no clip and no wrap");

  // Stored order Q1, Q0 with controlled returns [1, 0, 3, 2].
  const auto rec2 = page_record({"Q1", "Q0"});
  const std::vector<std::int32_t> returns = {1, 0, 3, 2};
  std::vector<std::size_t> asked;
  const auto page = ql::page_calls(rec2.quests, 2, font, [&](std::size_t i) {
    asked.push_back(i);
    return returns[i];
  });
  check(page.lookups == std::vector<std::string>{"Q1", "B1", "Q0", "B0"}, "page: storage order, not sorted");
  check(asked == std::vector<std::size_t>{0, 1, 2, 3}, "page: one wrap return per draw, in order");
  check(page.rows.size() == 2 && page.rows[0].title_y == 114 && page.rows[0].body_y == 115 &&
            page.rows[1].title_y == 137 && page.rows[1].body_y == 140,
        "page: y from 114 plus each return, +22 after the body");
  const auto rows = ql::row_positions({1, 3}, {0, 2});
  check(rows.size() == 2 && rows[0].title_y == 114 && rows[0].body_y == 115 && rows[1].title_y == 137 &&
            rows[1].body_y == 140,
        "page: row_positions matches ref_quest_log");
  check(page.calls.size() == 18, "page: nine calls per entry");
  for (int e = 0; e < 2; ++e) {
    const auto *c = &page.calls[static_cast<std::size_t>(e * 9)];
    const auto ty = page.rows[static_cast<std::size_t>(e)].title_y, by = page.rows[static_cast<std::size_t>(e)].body_y;
    const auto t = std::int64_t(e * 2 + 1), bptr = t + 1;
    check(call_is(c[0], "text_begin", {font, 6, 0x80323c3c, -1}) && call_is(c[1], "text_setClip", {0, 119, 320, 440}) &&
              call_is(c[2], "0x1802e0", {0x80323c3c, -1}) &&
              call_is(c[3], "text_lookup", {}, page.lookups[static_cast<std::size_t>(t - 1)]) &&
              call_is(c[4], "text_drawWrapped", {24, ty, t, 288, 82, 418, 21}) &&
              call_is(c[5], "0x1802e0", {0x802d2d2d, -1}) &&
              call_is(c[6], "text_lookup", {}, page.lookups[static_cast<std::size_t>(bptr - 1)]) &&
              call_is(c[7], "text_drawWrapped", {24, by, bptr, 288, 82, 418, 21}) && call_is(c[8], "text_end", {}),
          "page: entry batch arguments");
  }
  // Zero returns give a shared row (the first probe's stub, quest-log.md 76).
  const auto flat = ql::page_calls(page_record({"Q0"}).quests, 1, ql::FONT_WORD, [](std::size_t) { return 0; });
  check(flat.rows[0].title_y == 114 && flat.rows[0].body_y == 114 && flat.calls[0].args[0] == 0x4f7104,
        "page: returns [0, 0] give (114, 114); the font word is passed through");
  // A full list: eight batches.
  const auto full = ql::page_calls(page_record({"Q0", "Q1", "Q2", "Q3", "Q4", "Q5", "Q6", "Q7"}).quests, 8, font,
                                   [](std::size_t) { return 0; });
  check(full.calls.size() == 72 && full.lookups.size() == 16, "page: eight entries");
  // Completion leaves the rows (quest-log.md 78 and 221): the page reads only
  // the list, so a record with bit 1 set draws the same calls.
  auto done = rec2;
  done.missions.fill(0xf7);
  const auto again = ql::page_calls(done.quests, 2, font, [&](std::size_t i) { return returns[i]; });
  check(again.lookups == page.lookups && again.calls.size() == page.calls.size(), "page: no completed row");
  // Refusals.
  check(refuses([&] { ql::page_calls(rec2.quests, 9, font, {}); }), "page: count 9 refused");
  check(refuses([&] { ql::page_calls(rec2.quests, -1, font, {}); }), "page: negative count refused");
  check(refuses([&] { ql::page_calls(rec2.quests, 2, font, {}); }), "page: unknown wrap return refused");
  auto bad = rec2;
  bad.quests[1].text = filled(0x41);
  check(refuses([&] { ql::page_calls(bad.quests, 2, font, [](std::size_t) { return 0; }); }),
        "page: unterminated field refused");
}

// quest-log.md 267-277: savePlayer copies the medal player's quest span;
// restorePlayer writes it back, keeping the live medal byte.
void test_save_restore() {
  for (int medal_slot = 0; medal_slot < 2; ++medal_slot) {
    b::Variables v;
    auto &m = v.memory;
    m.word(NUM_PLAYERS, 2);
    const int other = 1 - medal_slot;
    fill_span(m, medal_slot, 0xee, 2);
    plant(m, medal_slot, 0, "Q0", "B0");
    plant(m, medal_slot, 1, "Q1", "B1");
    fill_span(m, other, 0x77, 1);
    plant(m, other, 0, "P0", "C0");
    const auto other_span = span(m, other);
    m.byte(MEDAL_SLOT, std::uint8_t(medal_slot));
    m.byte(rec(medal_slot) + MEDALS_AT, 9);
    constexpr std::uint32_t COPY = 0x100000u;
    m.write(COPY, std::vector<std::uint8_t>(STRIDE, 0xa5));
    v.scene_service = [&](const std::string &name, const std::vector<b::Variables::ServiceValue> &args) -> std::uint32_t {
      if (name == "save_playerCopy") {
        check(std::get<std::uint32_t>(args.at(0)) == std::uint32_t(medal_slot), "save: copy for the medal slot");
        return COPY;
      }
      return 0;
    };
    v.save_player_record();
    const auto live = span(m, medal_slot);
    check(m.read(COPY + QUEST_AT, SPAN) == live, "save: the copy span equals the live span, tails included");
    m.write(rec(medal_slot) + QUEST_AT, std::vector<std::uint8_t>(SPAN, 0x5a));
    m.byte(rec(medal_slot) + MEDALS_AT, 4);
    v.restore_player_record();
    check(span(m, medal_slot) == live && title_at(m, medal_slot, 1) == "Q1" && body_at(m, medal_slot, 1) == "B1" &&
              tail_is(m, rec(medal_slot) + QUEST_AT + 0x20u, "B0", 0xee),
          "restore: title, body and tail bytes return");
    check(m.read(rec(medal_slot) + MEDALS_AT) == 4, "restore: the live medal byte stays");
    check(span(m, other) == other_span, "restore: the other player's span is left alone");
  }
}

// The binder's own quest_add/quest_remove, on the same fixtures.
void test_binder_list() {
  b::Variables v;
  auto &m = v.memory;
  m.word(NUM_PLAYERS, 2);
  fill_span(m, 0, 0x5a, 0);
  fill_span(m, 1, 0x5a, 0);
  v.quest_add("A", "BA");
  v.quest_add("B", "BB");
  check(tail_is(m, rec(0) + QUEST_AT, "A", 0x5a) && tail_is(m, rec(1) + QUEST_AT + 0x60u, "BB", 0x5a) &&
            m.word(rec(1) + COUNT_AT) == 2,
        "binder add: order and tails");
  v.quest_add("A", "BA");
  check(m.word(rec(0) + COUNT_AT) == 2 && m.word(rec(1) + COUNT_AT) == 2, "binder add: duplicate returns");
  v.quest_add("C", "BC");
  v.quest_remove("A");
  v.quest_add("A", "BA"); // A,B,C -> B,C -> B,C,A
  plant(m, 0, 0, "A", "BA"); // planted A,C,A
  v.quest_remove("A");
  check(m.word(rec(0) + COUNT_AT) == 2 && title_at(m, 0, 0) == "NoQuests" && title_at(m, 0, 1) == "C",
        "binder remove: every match covered, no slide past the last match");
  check(title_at(m, 1, 0) == "B" && title_at(m, 1, 1) == "C" && m.word(rec(1) + COUNT_AT) == 2,
        "binder remove: a match in the last used entry needs no slide");
}
} // namespace

int main() {
  test_add();
  test_remove();
  test_complete();
  test_clear();
  test_writers();
  test_page();
  test_save_restore();
  test_binder_list();
  if (failed) {
    std::cerr << failed << " failed\n";
    return 1;
  }
  std::cout << "quest-log ok\n";
  return 0;
}
