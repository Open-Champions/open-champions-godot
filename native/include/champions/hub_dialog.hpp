#pragma once
// Hub and hub2 between-mission NPC services, RTA US v2.00 only.
// Authority: t3/research-hub-npcs @ fbbfe65, docs/hub-services.md (cited below
// as HS:line), model tools/ref_hub_services.py (RHS:line), PASS audit
// cursor-runs/audits/research-hub-services-fix.md (HSF-1..3 LOW respected:
// K.poll counts Script-task runs, never frames; the L-INN restore charge and
// its gold imports stay Unknown).
//
// Only Observed/Executed rows are tabled. Names are never stored: every
// condition, action and variable is resolved from the supplied script by public
// offset and literal call site, as the research difftest does (HS:16-35).
#include "champions/amx.hpp"
#include "champions/dialog.hpp"
#include "champions/script_builtins.hpp"
#include "champions/quests.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace champions::simulation::rta::hub_dialog {

enum class Hub : std::uint8_t { Hub, Hub2 };
// Predicate kinds, HS:85-97 and RHS:20-26.
enum class Kind : std::uint8_t { Active, Offer, Pending, Demo, Intro, Done, NotDone };
enum class Var : std::uint8_t { None, Demo, Intro };
// Action effects, HS:112-125 and RHS:30-35.
struct Effect {
  int avail = 0; // mission whose bit 0 is set; 0 = none
  bool quest = false, store = false;
  Var var = Var::None;
};
inline constexpr int NONE = -1;
struct ChoiceRow {
  int cond = NONE; // choice-condition public offset
  Kind kind = Kind::Offer;
  int mission = 0;
  int action = NONE; // action public offset
  int next = -1;     // -1 closes
};
struct NodeRow {
  int cond;
  Kind kind;
  int mission;
  int count;
  std::array<ChoiceRow, 2> choices;
};
struct ActionRow {
  int offset;
  Effect effect;
};

namespace detail {
constexpr ChoiceRow close(int action) { return {NONE, Kind::Offer, 0, action, -1}; }
constexpr ChoiceRow more(int action, int next) { return {NONE, Kind::Offer, 0, action, next}; }
// SHOP -> store, END -> none (status/reminder node). RHS:42-47.
constexpr NodeRow status(int cond, Kind k, int n, int store) {
  return {cond, k, n, 2, {close(store), close(NONE)}};
}
// END -> end_action, SHOP -> shop_action (offer node). RHS:50-51.
constexpr NodeRow offer(int cond, int n, int end_action, int shop_action) {
  return {cond, Kind::Offer, n, 2, {close(end_action), close(shop_action)}};
}
constexpr NodeRow chain(int cond, Kind k, int n, int next, int action = NONE) {
  return {cond, k, n, 1, {more(action, next), close(NONE)}};
}
constexpr Effect E(int avail = 0, bool quest = false, bool store = false, Var var = Var::None) {
  return {avail, quest, store, var};
}
} // namespace detail

// Node tables HS:160-225 (hub H1.N00..N30, hub2 H2.N00..N25), RHS:58-123.
inline constexpr std::array<NodeRow, 31> HUB_NODES{{
    {0x3F0, Kind::Active, 0, 2, {detail::close(0x1DF8), detail::close(NONE)}},
    detail::chain(0x11C, Kind::Offer, 1, 2),
    detail::chain(0x11C, Kind::Offer, 1, 3),
    {0x198, Kind::Pending, 1, 2, {detail::close(0x1E68), detail::close(0x1EE8)}},
    detail::chain(0x498, Kind::Demo, 0, 5),
    {0x498, Kind::Demo, 0, 2, {detail::close(0x598), detail::close(0x1FC0)}},
    detail::status(0x680, Kind::Demo, 0, 0x1DF8),
    detail::offer(0x734, 2, 0x7B0, 0x2064),
    detail::status(0x898, Kind::Pending, 2, 0x1DF8),
    detail::chain(0x9A0, Kind::Offer, 3, 10),
    detail::offer(0x9A0, 3, 0xA1C, 0x2108),
    detail::status(0xB04, Kind::Pending, 3, 0x1DF8),
    detail::offer(0xC0C, 4, 0x21AC, 0x222C),
    detail::status(0xD70, Kind::Pending, 4, 0x1DF8),
    detail::offer(0xE78, 5, 0xEF4, 0x2304),
    detail::status(0xFDC, Kind::Pending, 5, 0x1DF8),
    detail::chain(0x10E4, Kind::Offer, 6, 17),
    detail::offer(0x10E4, 6, 0x23A8, 0x2428),
    detail::status(0x1248, Kind::Pending, 6, 0x1DF8),
    detail::offer(0x1350, 7, 0x13CC, 0x2500),
    detail::status(0x14B4, Kind::Pending, 7, 0x1DF8),
    detail::status(0x15BC, Kind::Offer, 8, 0x1DF8),
    detail::status(0x1638, Kind::Offer, 9, 0x1DF8),
    detail::chain(0x16B4, Kind::Offer, 10, 24),
    detail::offer(0x16B4, 10, 0x25A4, 0x2624),
    detail::status(0x1818, Kind::Pending, 10, 0x1DF8),
    detail::chain(0x1920, Kind::Offer, 11, 27, 0x26FC),
    detail::status(0x1A84, Kind::Pending, 11, 0x1DF8),
    detail::chain(0x1B8C, Kind::Offer, 12, 29),
    detail::offer(0x1B8C, 12, 0x1C08, 0x277C),
    detail::status(0x1CF0, Kind::Pending, 12, 0x1DF8),
}};
inline constexpr std::array<NodeRow, 26> HUB2_NODES{{
    {0x2AB0, Kind::Active, 0, 2, {detail::close(0x41B4), detail::close(NONE)}},
    // H2.N01: MORE if !C(1) -> N02, MORE if C(1) -> N03 (HS:201).
    {0x28B4, Kind::Intro, 2, 2,
     {ChoiceRow{0x2978, Kind::NotDone, 1, 0x29BC, 2}, ChoiceRow{0x2A08, Kind::Done, 1, 0x29BC, 3}}},
    detail::status(0x28B4, Kind::Intro, 2, 0x41B4),
    detail::offer(0x2AF0, 2, 0x2B6C, 0x4224),
    detail::status(0x2C54, Kind::Pending, 2, 0x41B4),
    detail::offer(0x2D5C, 3, 0x2DD8, 0x42C8),
    detail::status(0x2EC0, Kind::Pending, 3, 0x41B4),
    detail::offer(0x2FC8, 4, 0x436C, 0x43EC),
    detail::status(0x312C, Kind::Pending, 4, 0x41B4),
    detail::offer(0x3234, 5, 0x32B0, 0x44C4),
    detail::status(0x3398, Kind::Pending, 5, 0x41B4),
    detail::offer(0x34A0, 6, 0x351C, 0x4568),
    detail::status(0x3604, Kind::Pending, 6, 0x41B4),
    detail::chain(0x370C, Kind::Offer, 7, 14),
    detail::offer(0x370C, 7, 0x3788, 0x460C),
    detail::status(0x3870, Kind::Pending, 7, 0x41B4),
    detail::status(0x3978, Kind::Offer, 8, 0x41B4),
    detail::status(0x39F4, Kind::Offer, 9, 0x41B4),
    detail::chain(0x3A70, Kind::Offer, 10, 19),
    detail::chain(0x3A70, Kind::Offer, 10, 20, 0x46B0),
    detail::status(0x3BD4, Kind::Pending, 10, 0x41B4),
    detail::offer(0x3CDC, 11, 0x4730, 0x47B0),
    detail::status(0x3E40, Kind::Pending, 11, 0x41B4),
    detail::chain(0x3F48, Kind::Offer, 12, 24),
    detail::offer(0x3F48, 12, 0x3FC4, 0x4888),
    detail::status(0x40AC, Kind::Pending, 12, 0x41B4),
}};
// Action effects, Executed with recorded writes (HS:112-125, 160-225; RHS:125-151).
inline constexpr std::array<ActionRow, 22> HUB_ACTIONS{{
    {0x1DF8, detail::E(0, false, true)}, {0x598, detail::E(0, false, false, Var::Demo)},
    {0x1FC0, detail::E(0, false, true, Var::Demo)},
    {0x1E68, detail::E(1, true)}, {0x1EE8, detail::E(1, true, true)},
    {0x7B0, detail::E(2)}, {0x2064, detail::E(2, false, true)},
    {0xA1C, detail::E(3)}, {0x2108, detail::E(3, false, true)},
    {0x21AC, detail::E(4, true)}, {0x222C, detail::E(4, true, true)},
    {0xEF4, detail::E(5)}, {0x2304, detail::E(5, false, true)},
    {0x23A8, detail::E(6, true)}, {0x2428, detail::E(6, true, true)},
    {0x13CC, detail::E(7)}, {0x2500, detail::E(7, false, true)},
    {0x25A4, detail::E(10, true)}, {0x2624, detail::E(10, true, true)},
    {0x26FC, detail::E(11, true)},
    {0x1C08, detail::E(12)}, {0x277C, detail::E(12, false, true)},
}};
inline constexpr std::array<ActionRow, 19> HUB2_ACTIONS{{
    {0x41B4, detail::E(0, false, true)}, {0x29BC, detail::E(0, false, false, Var::Intro)},
    {0x2B6C, detail::E(2)}, {0x4224, detail::E(2, false, true)},
    {0x2DD8, detail::E(3)}, {0x42C8, detail::E(3, false, true)},
    {0x436C, detail::E(4, true)}, {0x43EC, detail::E(4, true, true)},
    {0x32B0, detail::E(5)}, {0x44C4, detail::E(5, false, true)},
    {0x351C, detail::E(6)}, {0x4568, detail::E(6, false, true)},
    {0x3788, detail::E(7)}, {0x460C, detail::E(7, false, true)},
    {0x46B0, detail::E(10, true)},
    {0x4730, detail::E(11, true)}, {0x47B0, detail::E(11, true, true)},
    {0x3FC4, detail::E(12)}, {0x4888, detail::E(12, false, true)},
}};

// The only natives a live-dialog action executes (HS:115-117; difftest
// ACTION_NATIVES, asserted statically and Executed). None takes an item or gold.
inline bool action_native(const std::string &name) {
  return name == "setMissionAvailable" || name == "addQuest" || name == "setv" ||
         name == "changeSwoop" || name == "activateStore";
}

// PushTrigger record values, Observed and asserted (HS:363-366): nethostonly=1,
// radius 79 (hub) / 78 (hub2). How the original's use query applies the radius
// is not in this research.
inline constexpr int NET_HOST_ONLY = 1;
inline constexpr int push_radius(Hub hub) { return hub == Hub::Hub ? 79 : 78; }
// The deferred duplicate checks queued by start (HS:329-341): tables of 10 and
// 13 cells (bounds limits 9 and 12, Observed); the loops visit 9 and 12 cells
// (Executed), so each table's last cell is never checked.
inline constexpr int SHARD_TABLE = 10, BLOOD_TABLE = 13, SHARD_LIST = 9, BLOOD_LIST = 12;
// K.poll (HS:374-383, RHS:331-349): counted unpaused runs before the menu call.
inline constexpr int WARN_POLL_RUNS = 6;
// Hub public of the one cutScene literal (HS:66-69).
inline constexpr int CUTSCENE_ENTRY = 0x34A4;

// ---- Static view of a loaded script (the difftest's Script, HS:16-21) -------
struct Call {
  amx::Cell site = 0;
  std::string native;
  std::vector<std::optional<amx::Cell>> args; // literal PUSH.C cells, else nullopt
};
struct Static {
  std::map<amx::Cell, std::string> by_offset; // public offset -> name
  std::vector<amx::Cell> offsets;              // sorted
  std::vector<Call> sites;
  std::vector<std::pair<amx::Cell, amx::Cell>> bounds, internal; // (site, value)
  explicit Static(const amx::Machine &m) {
    if (!m.decoded)
      throw dialog::Refused("hub services: script is not relocated");
    for (const auto &[name, offset] : m.publics()) {
      by_offset[offset] = name;
      offsets.push_back(offset);
    }
    std::sort(offsets.begin(), offsets.end());
    const auto names = m.native_names();
    const auto op = [](const char *n) { return amx::Cell(amx::opcode_number(n)); };
    const auto PUSH_C = op("PUSH.C"), PUSH = op("PUSH"), PUSH_PRI = op("PUSH.pri"),
               PUSH_ALT = op("PUSH.alt"), STACK = op("STACK"), SYSREQ_C = op("SYSREQ.C"),
               LINE = op("LINE"), BOUNDS = op("BOUNDS"), CALL = op("CALL");
    std::vector<std::optional<amx::Cell>> pending;
    for (const auto &[at, ins] : *m.decoded) {
      const auto *value = std::get_if<amx::Cell>(&ins.operand);
      if (ins.opcode == LINE)
        continue;
      if (ins.opcode == SYSREQ_C && value) {
        Call c;
        c.site = at;
        if (*value >= 0 && std::size_t(*value) < names.size())
          c.native = names[std::size_t(*value)];
        if (!pending.empty())
          c.args.assign(pending.rbegin() + 1, pending.rend());
        sites.push_back(std::move(c));
        pending.clear();
        continue;
      }
      if (ins.opcode == PUSH_C && value) {
        pending.push_back(*value);
        continue;
      }
      if (ins.opcode == PUSH || ins.opcode == PUSH_PRI || ins.opcode == PUSH_ALT) {
        pending.push_back(std::nullopt);
        continue;
      }
      if (ins.opcode == BOUNDS && value)
        bounds.emplace_back(at, *value);
      if (ins.opcode == CALL && value)
        internal.emplace_back(at, *value);
      pending.clear();
      (void)STACK;
    }
  }
  amx::Cell end_of(amx::Cell offset) const {
    const auto it = std::upper_bound(offsets.begin(), offsets.end(), offset);
    return it == offsets.end() ? amx::Cell(1) << 40 : *it;
  }
  std::vector<Call> calls(amx::Cell offset) const {
    std::vector<Call> out;
    const auto end = end_of(offset);
    for (const auto &c : sites)
      if (c.site >= offset && c.site < end)
        out.push_back(c);
    return out;
  }
  std::vector<amx::Cell> bounds_in(amx::Cell offset) const {
    std::vector<amx::Cell> out;
    const auto end = end_of(offset);
    for (const auto &[site, limit] : bounds)
      if (site >= offset && site < end)
        out.push_back(limit);
    return out;
  }
  // Literal string arguments of one native in one public, in site order.
  std::vector<std::string> literals(const amx::Machine &m, amx::Cell offset, const std::string &native,
                                    std::size_t arg = 0) const {
    std::vector<std::string> out;
    for (const auto &c : calls(offset)) {
      if (c.native != native)
        continue;
      if (c.args.size() <= arg || !c.args[arg] || m.get_addr(*c.args[arg]) != amx::ERR_NONE)
        throw dialog::Refused("hub services: non-literal " + native + " argument");
      out.push_back(m.get_string(*c.args[arg]));
    }
    return out;
  }
};

inline std::optional<amx::Cell> public_offset(const amx::Machine &m, const std::string &name) {
  if (name.empty())
    return std::nullopt;
  const auto found = m.find_public(name);
  if (found.first != amx::ERR_NONE)
    return std::nullopt;
  return m.public_entry(found.second).second;
}

// The one-node cutscene line list: no condition and no choices (HS:64-69,
// Observed). It is not an NPC dialog; its playback is not ported.
inline bool cutscene_line_list(const std::vector<dialog::Node> &nodes) {
  return nodes.size() == 1 && nodes[0].condition.empty() && nodes[0].choices.empty() &&
         nodes[0].raw_choices == 0;
}

// The two deferred duplicate checks `start` queues: their bounds-check limits
// give the table sizes (HS:334-337). nullopt when the script differs.
struct DedupeTables {
  int shard_cells = 0, blood_cells = 0;
};
inline std::optional<DedupeTables> dedupe_tables(const amx::Machine &m) {
  const Static s(m);
  const auto start = public_offset(m, "start");
  if (!start)
    return std::nullopt;
  std::vector<std::string> deferred;
  try {
    deferred = s.literals(m, *start, "addDeferedCall");
  } catch (const dialog::Refused &) {
    return std::nullopt;
  }
  if (deferred.size() != 2)
    return std::nullopt;
  DedupeTables out;
  for (std::size_t i = 0; i < 2; ++i) {
    const auto at = public_offset(m, deferred[i]);
    if (!at)
      return std::nullopt;
    const auto limits = s.bounds_in(*at);
    const amx::Cell want = i == 0 ? SHARD_TABLE - 1 : BLOOD_TABLE - 1;
    if (limits.size() != 2 || limits[0] != want || limits[1] != want)
      return std::nullopt;
    (i == 0 ? out.shard_cells : out.blood_cells) = int(want + 1);
  }
  return out;
}

// ---- The full live-dialog binding ------------------------------------------
struct Binding {
  Hub hub = Hub::Hub;
  struct Predicate {
    Kind kind;
    int mission;
  };
  std::map<std::string, Predicate> predicates; // node and choice condition names
  std::vector<std::string> node_conditions;    // file order
  std::map<std::string, Effect> actions;       // action public name -> effect
  std::array<std::string, 12> started;         // V.started[1..12]
  std::string intro, demo;                     // V.intro (hub2), V.demo (hub)

  // Matches the whole node/choice structure field by field, as the difftest's
  // check_structure does (HS:227-228). nullopt (with a reason) when it differs:
  // the caller then keeps its earlier, narrower admission or refuses.
  static std::optional<Binding> match(const amx::Machine &m, const std::vector<dialog::Node> &nodes,
                                      std::string *why = nullptr) {
    const auto fail = [&](const std::string &reason) -> std::optional<Binding> {
      if (why)
        *why = reason;
      return std::nullopt;
    };
    Binding b;
    const NodeRow *rows = nullptr;
    const ActionRow *acts = nullptr;
    std::size_t count = 0, act_count = 0;
    const auto first = nodes.empty() ? std::nullopt : public_offset(m, nodes[0].condition);
    if (nodes.size() == HUB_NODES.size() && first == HUB_NODES[0].cond) {
      b.hub = Hub::Hub;
      rows = HUB_NODES.data(), count = HUB_NODES.size();
      acts = HUB_ACTIONS.data(), act_count = HUB_ACTIONS.size();
    } else if (nodes.size() == HUB2_NODES.size() && first == HUB2_NODES[0].cond) {
      b.hub = Hub::Hub2;
      rows = HUB2_NODES.data(), count = HUB2_NODES.size();
      acts = HUB2_ACTIONS.data(), act_count = HUB2_ACTIONS.size();
    } else {
      return fail("not a hub or hub2 live dialog");
    }
    const auto effect_at = [&](int offset) -> std::optional<Effect> {
      for (std::size_t i = 0; i < act_count; ++i)
        if (acts[i].offset == offset)
          return acts[i].effect;
      return std::nullopt;
    };
    std::set<int> referenced;
    for (std::size_t i = 0; i < count; ++i) {
      const auto &node = nodes[i];
      const auto &row = rows[i];
      if (public_offset(m, node.condition) != std::optional<amx::Cell>(row.cond))
        return fail("node condition offset differs");
      if (int(node.choices.size()) != row.count || node.raw_choices != row.count)
        return fail("node choice count differs");
      b.predicates[node.condition] = {row.kind, row.mission};
      b.node_conditions.push_back(node.condition);
      for (int j = 0; j < row.count; ++j) {
        const auto &choice = node.choices[std::size_t(j)];
        const auto &want = row.choices[std::size_t(j)];
        const auto cond = public_offset(m, choice.condition);
        if (choice.condition.empty() ? want.cond != NONE : cond != std::optional<amx::Cell>(want.cond))
          return fail("choice condition offset differs");
        if (!choice.condition.empty())
          b.predicates[choice.condition] = {want.kind, want.mission};
        const auto act = public_offset(m, choice.action);
        if (choice.action.empty() ? want.action != NONE : act != std::optional<amx::Cell>(want.action))
          return fail("choice action offset differs");
        if (choice.next != want.next)
          return fail("choice next differs");
        if (!choice.action.empty()) {
          const auto effect = effect_at(want.action);
          if (!effect)
            return fail("action has no researched effect");
          b.actions[choice.action] = *effect;
          referenced.insert(want.action);
        }
      }
    }
    if (referenced.size() != act_count)
      return fail("action set differs");
    // V.started[n]: the twelve getv literals, in order, of the callScript
    // target of node 0's condition (HS:25, difftest started_names).
    try {
      const Static s(m);
      const auto callee = s.literals(m, rows[0].cond, "callScript");
      if (callee.empty())
        return fail("node-0 condition calls no scan entry");
      const auto scan = public_offset(m, callee[0]);
      if (!scan)
        return fail("node-0 scan entry missing");
      const auto names = s.literals(m, *scan, "getv");
      if (names.size() != b.started.size())
        return fail("node-0 scan does not read twelve variables");
      std::copy(names.begin(), names.end(), b.started.begin());
      const auto var = s.literals(m, b.hub == Hub::Hub ? 0x498 : 0x28B4, "getv");
      if (var.empty())
        return fail("demo/intro variable unread");
      (b.hub == Hub::Hub ? b.demo : b.intro) = var[0];
    } catch (const dialog::Refused &error) {
      return fail(error.what());
    }
    return b;
  }

  // HS:85-97 / RHS:200-216. Player 0's mission byte at the current difficulty.
  bool predicate(Kind kind, int n, builtins::Variables &state) const {
    const auto d = amx::s32(state.memory.word(0x4f719c));
    (void)quests::mission_index(d, 1); // keep the campaign's out-of-domain refusal
    const auto a = [&](int m) { return state.mission_available(d, m) != 0; };
    const auto c = [&](int m) { return state.mission_complete(d, m) != 0; };
    const auto v = [&](const std::string &name) { return state.quest_get(name); };
    switch (kind) {
    case Kind::Active:
      for (int m = 1; m <= 12; ++m)
        if (a(m) && !c(m) && v(started[std::size_t(m - 1)]) != 0)
          return true;
      return false;
    case Kind::Offer:
      return !a(n);
    case Kind::Pending:
      return a(n) && !c(n) && v(started[std::size_t(n - 1)]) == 0;
    case Kind::Demo:
      return false; // demoRunning 0x141764 answers 0 (HS:95)
    case Kind::Intro:
      return !a(2) && v(intro) == 0;
    case Kind::Done:
      return c(n);
    case Kind::NotDone:
      return !c(n);
    }
    throw dialog::Refused("unknown hub predicate");
  }
  std::optional<bool> condition(const std::string &name, builtins::Variables &state) const {
    const auto found = predicates.find(name);
    if (found == predicates.end())
      return std::nullopt;
    return predicate(found->second.kind, found->second.mission, state);
  }
  // dialog_onMsg: node 0 first, then file order; node 0 when none (HS:79-83).
  int start(builtins::Variables &state) const {
    for (std::size_t i = 0; i < node_conditions.size(); ++i)
      if (*condition(node_conditions[i], state))
        return int(i);
    return 0;
  }
  const std::string &variable(Var var) const { return var == Var::Demo ? demo : intro; }
};

} // namespace champions::simulation::rta::hub_dialog
