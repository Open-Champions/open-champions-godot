#include "champions/mission2_route.hpp"
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {
namespace route = champions::simulation::rta::mission2_route;
namespace quests = champions::simulation::rta::quests;
namespace flow = champions::simulation::rta::level_flow;
int checks = 0;

void require(bool ok, const char *what) {
  ++checks;
  if (!ok) {
    std::cerr << "fail: " << what << "\n";
    std::exit(1);
  }
}

std::vector<std::uint8_t> pattern(std::size_t size, std::uint32_t seed) {
  std::vector<std::uint8_t> bytes(size);
  for (std::size_t i = 0; i < size; ++i)
    bytes[i] = static_cast<std::uint8_t>((i * 17u + seed) & 0xffu);
  return bytes;
}

route::Session planted(int difficulty, int evil, int decision, int mission1, int mission2,
                       int latch, std::uint32_t seed) {
  // tools/difftest_mission2_compose.py:42-52 stores the difficulty bit, not a
  // boolean, into the two campaign bytes.
  route::Session session;
  session.latch = latch;
  session.bits.difficulty = difficulty;
  session.bits.evil = (evil & 1) << difficulty;
  session.bits.decision = (decision & 1) << difficulty;
  session.bits.mission1 = mission1;
  session.bits.mission2 = mission2;
  session.names = {"keep", "also"};
  session.player_bytes = pattern(route::PLAYER_BYTES, seed);
  session.quest_table = pattern(route::QUEST_TABLE_BYTES, seed ^ 0x5au);
  session.quest_table[0] = 0x15;
  session.seed = seed;
  return session;
}

void expect_unchanged(const route::Session &before, const route::Session &after, const char *what) {
  require(after.player_bytes == before.player_bytes, what);
  require(after.quest_table == before.quest_table, what);
  require(after.seed == before.seed, what);
}

void test_reset() {
  const int seeds[] = {20973, 7};
  for (int seed : seeds) {
    for (int difficulty : {0, 4}) {
      for (int evil : {0, 1}) {
        for (int decision : {0, 1}) {
          for (int complete1 : {0, 1}) {
            for (int complete2 : {0, 1}) {
              for (int latch : {0, 1}) {
                const int mission1 = 1 | (complete1 << 1);
                const int mission2 = 1 | (complete2 << 1);
                auto session = planted(difficulty, evil, decision, mission1, mission2, latch,
                                       static_cast<std::uint32_t>(seed));
                const auto hub = route::resolver_hub(session.bits);
                const auto other = planted(difficulty, evil, decision, mission1, mission2 ^ 2, latch,
                                           static_cast<std::uint32_t>(seed));
                require(route::resolver_hub(other.bits) == hub, "mission 2 completion does not change the hub");
                const auto after = route::reset_destinations(session);
                expect_unchanged(session, after, "reset leaves player bytes, the quest table, and the seed");
                if (latch) {
                  require(!after.resolver_called, "latch 1 does not call the resolver");
                  require(after.names == session.names, "latch 1 leaves the existing entries");
                } else {
                  require(after.resolver_called, "latch 0 calls the resolver");
                  require(after.names.size() == 1 && after.names[0] == hub, "latch 0 stores the resolver hub");
                }
              }
            }
          }
        }
      }
    }
    for (int mission1 : {0, 1, 2, 3}) {
      auto session = planted(0, 1, 1, mission1, 0, 0, static_cast<std::uint32_t>(seed));
      const auto after = route::reset_destinations(session);
      const bool complete = (mission1 & 2) != 0;
      const auto expect = complete ? std::string(quests::HUB_EVIL) : std::string(quests::HUB_GOOD);
      require(after.names.size() == 1 && after.names[0] == expect, "completion bit with the available bit clear");
      expect_unchanged(session, after, "completion-byte reset leaves player bytes");
    }
  }
  auto negative = planted(0, 1, 1, 2, 0, 0, 7);
  negative.bits.difficulty = -1;
  bool threw = false;
  try {
    route::resolver_hub(negative.bits);
  } catch (const quests::Error &) {
    threw = true;
  }
  require(threw, "the existing resolver refuses a negative difficulty");
}

void test_insert_and_route() {
  const std::string H = quests::HUB_GOOD;
  const std::string E = quests::HUB_EVIL;
  const std::string A = "ord-a";
  const std::string B = "ord-b";
  const std::string C = "ord-c";
  const std::string K = "keep";
  struct Sample {
    bool evil;
    std::vector<std::string> start;
    std::string current;
    std::vector<std::string> expect;
    bool resolver;
  };
  // Shapes from tools/difftest_mission2_compose.py:101-116 and the duplicate
  // rows in the audited insert log. Ordinary names are synthetic.
  const Sample samples[] = {
      {false, {}, E, {}, true},
      {false, {}, A, {A}, false},
      {false, {K}, A, {K, A}, false},
      {false, {H, E}, A, {H, A}, true},
      {false, {E, H}, B, {H, B}, true},
      {false, {A, E, H}, C, {A, H, C}, true},
      {false, {E}, H, {H}, true},
      {false, {E}, E, {E}, false},
      {false, {E, A}, A, {E, A}, false},
      {true, {}, H, {}, true},
      {true, {H, E}, A, {E, A}, true},
      {true, {E, H}, B, {E, B}, true},
      {true, {C, H}, E, {C, E}, true},
      {true, {A, E, H}, C, {A, E, C}, true},
      {false, {E, E}, A, {E, A}, true},
      {true, {H, H}, A, {H, A}, true},
      {false, {E, K, E}, A, {K, E, A}, true},
      {false, {E, E}, H, {E, H}, true},
  };
  for (const auto &sample : samples) {
    auto session = planted(0, sample.evil ? 1 : 0, sample.evil ? 1 : 0, sample.evil ? 2 : 0, 3, 0, 20973);
    const auto players = session.player_bytes;
    const auto quests_before = session.quest_table;
    session.names = sample.start;
    const auto after = route::add_current_level(session, sample.current);
    require(after.names == sample.expect, "append then one non-resolver hub removal");
    require(after.resolver_called == sample.resolver, "resolver call matches an already-listed return");
    require(after.player_bytes == players && after.quest_table == quests_before && after.seed == session.seed,
            "insertion leaves player bytes, the quest table, and the seed");
  }
  const std::vector<std::string> levels = {"ord-a", "ord-b", "ord-c", "ord-d"};
  for (int evil : {0, 1}) {
    auto session = planted(evil ? 4 : 0, evil, evil, evil ? 3 : 1, 0, 0, 7);
    session.level = route::resolver_hub(session.bits);
    const auto after = route::arrival_list(session, levels);
    require(after.names.size() == 5, "arrival list has the hub and four ordinary names");
    require(after.names[0] == route::resolver_hub(session.bits), "row 0 is the resolver hub");
    require(std::vector<std::string>(after.names.begin() + 1, after.names.end()) == levels,
            "the four ordinary names stay in insertion order");
    expect_unchanged(session, after, "arrival list leaves player bytes and the quest table");
  }
  auto latched = planted(0, 0, 0, 2, 0, 1, 7);
  bool threw = false;
  try {
    route::arrival_list(latched, levels);
  } catch (const route::Refused &) {
    threw = true;
  }
  require(threw, "the route reset was not executed with the latch set");
}

void test_portal() {
  const std::vector<std::string> levels = {"ord-a", "ord-b", "ord-c", "ord-d"};
  const int plans[][4] = {{0, 0, 0, 1}, {0, 1, 1, 1}, {4, 1, 1, 1}};
  for (int seed : {20973, 7}) {
    for (const auto &plan : plans) {
      for (int gate : {0, 1}) {
        auto session = planted(plan[0], plan[1], plan[2], 1 | (plan[3] << 1), 3, 0,
                               static_cast<std::uint32_t>(seed));
        session.level = route::resolver_hub(session.bits);
        session = route::arrival_list(session, levels);
        session.level = levels.back();
        session.sub.clear();
        session.pending.clear();
        session.orientation = 0;
        const auto hub = session.names[0];
        const auto result = route::portal_row0(session, gate, 0);
        require(result.names == session.names, "the portal leaves the built list");
        require(result.player_bytes == session.player_bytes, "player bytes survive the stop");
        require(result.quest_table == session.quest_table, "the quest table survives the stop");
        require(result.quest_table[0] == 0x15, "the quest-table prefix survives the stop");
        require(result.seed == session.seed, "the seed survives the stop");
        require(!result.loader_entered, "the stop does not enter the loader");
        if (gate == 0) {
          require(!result.accepted && !result.travel_registered, "gate 0 registers no travel task");
          require(result.pending.empty() && result.level == session.level, "gate 0 leaves the empty pending name");
          continue;
        }
        require(result.accepted && result.travel_registered, "gate 1 registers the travel task");
        require(result.found_row == session.names.size() - 1, "the found row is the current level");
        require(result.pending_before_stop == hub, "row 0 writes the hub");
        require(result.orientation_before_stop == flow::ORIENT_NONE, "row 0 writes orientation 255");
        if (result.accepting_ticks != 181) {
          std::cerr << "accepting ticks " << result.accepting_ticks << "\n";
          require(false, "the existing travel counter accepts on tick 181");
        }
        require(true, "the existing travel counter accepts on tick 181");
        require(result.pending.empty(), "the pre-load stop clears the pending name");
        require(result.level == hub && result.sub.empty(), "the stop accepts the hub as the current level");
        require(result.previous == levels.back(), "the saved previous level is the level being left");
        require(result.orientation == flow::ORIENT_CONSUMED, "orientation becomes 254");
        require(result.orientation_copy == flow::ORIENT_NONE, "the portal orientation is stored aside");
      }
    }
  }
  auto session = planted(0, 1, 1, 3, 0, 0, 7);
  session = route::arrival_list(session, levels);
  session.level = levels.back();
  bool missing = false;
  session.level = "not-listed";
  try {
    route::portal_row0(session, 1, 0);
  } catch (const route::Refused &) {
    missing = true;
  }
  require(missing, "discovery growth on this portal was not executed");
  session.level = levels.back();
  bool other_row = false;
  try {
    route::portal_row0(session, 1, 1);
  } catch (const route::Refused &) {
    other_row = true;
  }
  require(other_row, "a pad walk to another row was not executed");
}

void test_refusals() {
  const std::pair<void (*)(), const char *> cases[] = {
      {route::refuse_hub2_dialog, "hub2 dialog choices are Unknown"},
      {route::refuse_helper, "helper 0x18ebb8 is not implemented"},
      {route::refuse_physical_use, "physical NPC, trigger, and portal use are Unknown"},
      {route::refuse_loader_body, "loader body is Unknown"},
      {route::refuse_arrival, "arrival is Unknown"},
  };
  for (const auto &item : cases) {
    bool threw = false;
    try {
      item.first();
    } catch (const route::Refused &error) {
      threw = std::string(error.what()).find(item.second) != std::string::npos;
    }
    require(threw, item.second);
  }
}
} // namespace

int main() {
  test_reset();
  test_insert_and_route();
  test_portal();
  test_refusals();
  std::cout << "mission2 route: " << checks << " checks PASS\n";
  return 0;
}
