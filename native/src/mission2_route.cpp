#include "champions/mission2_route.hpp"
#include <algorithm>
#include <utility>

namespace champions::simulation::rta::mission2_route {
namespace {
bool hub_entry(const std::string &name) {
  // The walk calls the resolver for an entry that is one of the two hub
  // names. docs/mission2-compose.md:32-34. tools/ref_mission2_compose.py:37.
  return name == quests::HUB_GOOD || name == quests::HUB_EVIL;
}

std::string qualify(const std::string &level, const std::string &sub) {
  // tools/ref_mission2_travel.py:56.
  if (sub.empty())
    return level;
  return level + "_" + sub;
}
} // namespace

std::string resolver_hub(const HubBits &bits) {
  // Mission 2's completion byte is not an input. docs/mission2-compose.md:22.
  return quests::hub_name(bits.evil, bits.decision, bits.mission1, bits.difficulty);
}

Session reset_destinations(Session session) {
  // docs/mission2-compose.md:16-23. tools/ref_mission2_compose.py:17-21.
  if (session.latch) {
    session.resolver_called = false;
    return session;
  }
  session.names = {resolver_hub(session.bits)};
  session.resolver_called = true;
  return session;
}

Session add_current_level(Session session, const std::string &current) {
  // docs/mission2-compose.md:30-35. tools/ref_mission2_compose.py:33-41.
  session.resolver_called = false;
  if (std::find(session.names.begin(), session.names.end(), current) != session.names.end())
    return session;
  session.names.push_back(current);
  const auto hub = resolver_hub(session.bits);
  for (std::size_t index = 0; index < session.names.size(); ++index) {
    if (!hub_entry(session.names[index]))
      continue;
    session.resolver_called = true;
    if (session.names[index] != hub) {
      session.names.erase(session.names.begin() + static_cast<std::ptrdiff_t>(index));
      break;
    }
  }
  return session;
}

Session arrival_list(Session session, const std::vector<std::string> &levels) {
  // The executed route resets with the latch clear, then inserts each
  // ordinary name. docs/mission2-compose.md:42-46.
  if (session.latch)
    throw Refused("the four-level route reset was executed with the card latch clear");
  session = reset_destinations(std::move(session));
  for (const auto &level : levels)
    session = add_current_level(std::move(session), level);
  return session;
}

PortalResult portal_row0(const Session &session, int gate, int row) {
  // docs/mission2-compose.md:54-70. tools/difftest_mission2_compose.py:191-249.
  if (gate != 0 && gate != 1)
    throw Refused("only portal gates 0 and 1 were executed");
  PortalResult out;
  out.names = session.names;
  out.player_bytes = session.player_bytes;
  out.quest_table = session.quest_table;
  out.seed = session.seed;
  out.loader_entered = false;
  out.level = session.level;
  out.sub = session.sub;
  out.previous = session.previous;
  out.pending = session.pending;
  out.orientation = session.orientation;
  out.orientation_copy = session.orientation_copy;
  out.pending_before_stop = session.pending;
  out.orientation_before_stop = session.orientation;
  if (gate == 0) {
    // The refused prompt returns before discovery and registers no travel task.
    // tools/ref_mission2_travel.py:24-26. tools/difftest_mission2_compose.py:194-197.
    out.accepted = false;
    out.travel_registered = false;
    return out;
  }
  if (std::find(session.names.begin(), session.names.end(), session.level) == session.names.end())
    throw Refused("portal discovery growth was not executed; the current level is already listed");
  if (row != 0)
    throw Refused("only an explicit selection of row 0 was executed; pad movement is Unknown");
  if (session.names.empty())
    throw Refused("row 0 is absent");
  // Discovery of a name that is already listed does not call the resolver and
  // does not grow the list. docs/mission2-compose.md:58-59.
  const auto discovered = add_current_level(session, session.level);
  out.names = discovered.names;
  out.found_row = static_cast<std::size_t>(
      std::find(out.names.begin(), out.names.end(), session.level) - out.names.begin());
  level_flow::GateList list;
  list.names = out.names;
  level_flow::GateTravel travel;
  level_flow::Pending pending;
  travel.open(false);
  if (!travel.confirm(out.names[0], session.level, list, false))
    throw Refused("row 0 was not accepted by the existing travel counter");
  while (!travel.tick(pending)) {
    ++out.accepting_ticks;
    if (out.accepting_ticks > 181)
      throw Refused("the travel counter did not accept on the executed schedule");
  }
  ++out.accepting_ticks;
  out.travel_registered = true;
  out.pending_before_stop = pending.name;
  out.orientation_before_stop = pending.orientation;
  // Same pre-load stop as request_names: pending cleared, current becomes the
  // requested level, previous is the level being left, orientation becomes
  // 254, and the portal orientation is stored aside. The loader is not entered.
  // tools/ref_mission2_travel.py:54-58. docs/mission2-compose.md:65-70,92-96.
  out.previous = qualify(session.level, session.sub);
  const auto split = pending.name.find('_');
  out.level = split == std::string::npos ? pending.name : pending.name.substr(0, split);
  out.sub = split == std::string::npos ? std::string() : pending.name.substr(split + 1);
  out.pending.clear();
  out.orientation = level_flow::ORIENT_CONSUMED;
  out.orientation_copy = pending.orientation;
  out.accepted = true;
  out.loader_entered = false;
  return out;
}

[[noreturn]] void refuse_hub2_dialog() {
  // docs/mission2-compose.md:124-130. The dialog was not opened.
  throw Refused("hub2 dialog choices are Unknown: the dialog was not opened");
}
[[noreturn]] void refuse_helper() {
  // docs/mission2-compose.md:133-138. The compared menu path stubs this call.
  // Its other effects are Unknown. Entering it is not implemented.
  throw Refused("helper 0x18ebb8 is not implemented: the compared menu path stubs it and its other effects are Unknown");
}
[[noreturn]] void refuse_physical_use() {
  // docs/mission2-compose.md:141-145.
  throw Refused("physical NPC, trigger, and portal use are Unknown");
}
[[noreturn]] void refuse_loader_body() {
  // docs/mission2-compose.md:65-70. The stop is before any loader instruction.
  throw Refused("loader body is Unknown: stop before any loader instruction at 0x1e55c0");
}
[[noreturn]] void refuse_arrival() {
  // docs/mission2-compose.md:72-74.
  throw Refused("arrival is Unknown");
}
} // namespace champions::simulation::rta::mission2_route
