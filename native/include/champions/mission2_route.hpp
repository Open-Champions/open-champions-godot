#pragma once
// Mission 2 hub-return list and the pre-load stop that accepts it.
// Research pin: research-mission2-route b8e2d11, docs/mission2-compose.md.
// The play caller of these two list writers was not re-established
// (docs/mission2-compose.md:48-51), so nothing in the session calls this
// module. Mission-menu run-task admission stays on codex/play-progression
// round 3 (code d5889ae9, report c583578a) and is not copied here.
#include "champions/level_flow.hpp"
#include "champions/quests.hpp"
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace champions::simulation::rta::mission2_route {
struct Refused : std::runtime_error {
  using std::runtime_error::runtime_error;
};

// Spans the compose probe compares and then leaves unchanged.
// tools/difftest_mission2_compose.py:44 and docs/mission2-compose.md:86-96.
inline constexpr std::size_t PLAYER_BYTES = 4u * 0x4C30u;
inline constexpr std::size_t QUEST_TABLE_BYTES = 0x2000u;

// Campaign bytes game_getHubName reads. mission2 is retained so a caller can
// show that byte is not an input; reset does not read it.
// docs/mission2-compose.md:19-22. tools/difftest_mission2_compose.py:42-52.
struct HubBits {
  int evil = 0;
  int decision = 0;
  int mission1 = 0;
  int difficulty = 0;
  int mission2 = 0;
};

struct Session {
  int latch = 0;
  HubBits bits{};
  std::vector<std::string> names;
  std::string pending;
  std::uint8_t orientation = level_flow::ORIENT_NONE;
  std::string level;
  std::string sub;
  std::string previous;
  std::uint8_t orientation_copy = 0;
  std::vector<std::uint8_t> player_bytes;
  std::vector<std::uint8_t> quest_table;
  std::uint32_t seed = 0;
  bool resolver_called = false;
  bool travel_registered = false;
  bool loader_entered = false;
};

// game_getHubName 0x1ec300. tools/ref_mission2_compose.py:10-14.
std::string resolver_hub(const HubBits &bits);

// jumpgate_resetDestinations 0x27db00.
// Latch 0 stores the resolver hub. A nonzero latch leaves the list and does
// not call the resolver. Player bytes, the quest table, and the seed are not
// written. docs/mission2-compose.md:16-24. tools/ref_mission2_compose.py:17-21.
Session reset_destinations(Session session);

// jumpgate_addCurrentLevel 0x27db60.
// An already listed name returns with the resolver uncalled. Otherwise the
// name is appended, then the first hub entry other than the resolver result
// is removed and the function returns. A second copy of that other hub stays.
// docs/mission2-compose.md:28-40. tools/ref_mission2_compose.py:24-41.
Session add_current_level(Session session, const std::string &current);

// Latch-clear reset, then one insertion per ordinary name.
// docs/mission2-compose.md:42-46. tools/ref_mission2_compose.py:53-58.
Session arrival_list(Session session, const std::vector<std::string> &levels);

struct PortalResult {
  bool accepted = false;
  std::vector<std::string> names;
  std::string pending_before_stop;
  std::uint8_t orientation_before_stop = 0;
  bool travel_registered = false;
  std::size_t found_row = 0;
  int accepting_ticks = 0;
  std::string pending;
  std::string level;
  std::string sub;
  std::string previous;
  std::uint8_t orientation = 0;
  std::uint8_t orientation_copy = 0;
  bool loader_entered = false;
  std::vector<std::uint8_t> player_bytes;
  std::vector<std::uint8_t> quest_table;
  std::uint32_t seed = 0;
};

// Gate 0 refuses before discovery. Gate 1, with the current level already
// listed, does not grow the list. Explicit row 0 writes the hub and
// orientation 255 through the existing travel counter, then the pre-load
// stop accepts that pending name and does not enter the loader.
// docs/mission2-compose.md:54-70 and :92-96.
// tools/ref_mission2_travel.py:36-40 and :54-58.
// tools/difftest_mission2_compose.py:191-249.
PortalResult portal_row0(const Session &session, int gate, int row = 0);

// docs/mission2-compose.md:124-145. These stay unimplemented.
[[noreturn]] void refuse_hub2_dialog();
[[noreturn]] void refuse_helper();
[[noreturn]] void refuse_physical_use();
[[noreturn]] void refuse_loader_body();
[[noreturn]] void refuse_arrival();
} // namespace champions::simulation::rta::mission2_route
