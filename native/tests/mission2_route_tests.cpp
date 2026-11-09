#include "champions/mission2_route.hpp"
#include "champions/mission2_portal.hpp"
#include "champions/ps2_save.hpp"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <tuple>
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

// ---- Round 6: the portal menu to arrival -----------------------------------
// research-mission2-route 3c10c07 docs/mission2-round6.md. Every name below is
// authored; no level data is used.
namespace portal = route::portal;

template <class F> bool refused(F f) {
  try {
    f();
  } catch (const route::Refused &) {
    return true;
  }
  return false;
}

// A deterministic authored stream for picking moves and list sizes.
struct Pick {
  std::uint32_t state;
  explicit Pick(std::uint32_t seed) : state(seed * 2654435761u + 1u) {}
  std::uint32_t next(std::uint32_t n) {
    state = state * 1103515245u + 12345u;
    return (state >> 8) % n;
  }
};

// Up/down model, as tools/difftest_mission2_r6.py:457-464.
std::size_t model_select(const std::vector<int> &moves, std::size_t count) {
  std::size_t selected = 0;
  for (int move : moves) {
    if (move < 0 && selected != 0)
      --selected;
    else if (move > 0 && selected + 1 < count)
      ++selected;
  }
  return selected;
}

// State 0, then the 16 fade-in frames (docs/mission2-round6.md:212-216).
void to_input(portal::Menu &menu, flow::State &world) {
  require(portal::frame(menu, {}, world) == 1, "state 0 moves to the fade-in");
  require(menu.current && *menu.current == menu.names.size() - 1, "state 0 stores the current index, the last entry");
  require(menu.selection == 0 && menu.previous == 0, "state 0 clears the selection and the previous selection");
  require(menu.pause_word == 1 && menu.pause_byte == 1, "offline the menu pauses (0x44fa80, 0x41b4f4)");
  require(menu.word_41b4fc == 1 && menu.word_41b500 == 1, "state 0 sets 0x41b4fc and 0x41b500");
  require(world.travel.ticks == 0, "opening leaves travel 0");
  int fades = 0;
  std::uint8_t state = 1;
  while (state == 1) {
    ++fades;
    state = portal::frame(menu, {}, world);
  }
  require(state == 2 && fades == 16 && menu.fade == portal::FADE_HELD, "fade-in takes 16 frames and holds 0x7f");
}

std::vector<std::string> prior_names(std::uint32_t seed) {
  Pick pick(seed);
  std::vector<std::string> names;
  const auto count = 2 + pick.next(7);
  for (std::uint32_t i = 0; i < count; ++i)
    names.push_back("zm" + std::to_string(10000 + pick.next(80000)));
  return names;
}

void test_portal_menu() {
  for (std::uint32_t seed : {20973u, 7u}) {
    flow::State world;
    world.level = "ord-p";
    auto names = prior_names(seed);
    names.push_back(world.level);
    auto menu = portal::open(names, world);
    to_input(menu, world);
    Pick pick(seed ^ 0x6d6f7665u);
    std::vector<int> moves;
    const auto count = 4 + pick.next(8);
    for (std::uint32_t i = 0; i < count; ++i)
      moves.push_back(pick.next(3) == 0 ? -1 : 1);
    for (int move : moves) {
      portal::Pad pad;
      pad.moved = move < 0 ? 0x1000u : 0x4000u;
      require(portal::frame(menu, pad, world) == 2, "a move stays in state 2");
    }
    auto selected = model_select(moves, names.size());
    require(menu.selection == selected, "up/down ends where the model says (:218-220)");
    portal::Pad confirm;
    confirm.pressed = 0x40;
    if (selected == names.size() - 1) {
      require(portal::frame(menu, confirm, world) == 2 && world.travel.ticks == 0,
              "confirm on the current level is refused (0x27dfa0)");
      portal::Pad up;
      up.moved = 0x1000;
      require(portal::frame(menu, up, world) == 2 && menu.selection == selected - 1, "one more up press");
      --selected;
    }
    // Both pad words at a non-mask bit leave the selection and travel alone.
    portal::Pad other;
    other.pressed = 0x10;
    other.moved = 0x10;
    require(portal::frame(menu, other, world) == 2 && menu.selection == selected && world.travel.ticks == 0,
            "bits outside the masks do nothing");
    portal::Pad chord;
    chord.moved = 0x1000 | 0x4000;
    require(refused([&] { portal::frame(menu, chord, world); }), "up and down together were not executed");
    world.pending.name.clear();
    world.pending.orientation = 0x5a;
    require(portal::frame(menu, confirm, world) == 3 && world.travel.ticks == 1, "confirm stores travel 1 (0x27e00c)");
    int fades = 0;
    std::uint8_t state = 3;
    while (state == 3) {
      ++fades;
      state = portal::frame(menu, {}, world);
    }
    require(state == 4 && fades == 8 && menu.pause_menu_closes == 1, "fade-out takes 8 frames and closes the pause menu");
    require(menu.pause_word == 0 && menu.pause_byte == 0 && !world.travel.paused, "fade-out clears both pause words");
    require(portal::frame(menu, {}, world) == 5 && menu.travel_begins == 1, "state 4 calls jumpgate_beginTravel");
    unsigned counted = 0;
    bool early = false;
    while (menu.state == 5) {
      ++counted;
      portal::frame(menu, {}, world);
      if (menu.state == 5 && (!world.pending.name.empty() || world.pending.orientation != 0x5a))
        early = true;
    }
    require(counted == 181 && world.travel.ticks == 181, "state 5 runs 181 frames and travel ends at 181");
    require(!early, "the first 180 state-5 frames write nothing for the next world");
    require(world.pending.name == names[selected], "the last frame writes the selected name");
    require(world.pending.orientation == 0xff, "the orientation byte goes from the sentinel to 0xff");
    require(menu.state == 6 && menu.tasks, "state 5 ends in state 6 with the tasks still registered");
    require(refused([&] { portal::frame(menu, {}, world); }), "state 6 is not run: its order is Inferred");
  }
  // Refusals at open.
  flow::State world;
  world.level = "ord-p";
  require(refused([&] { portal::open({"hub", "ord-p"}, world, true); }), "the net-game menu was not executed");
  require(refused([&] { portal::open({"hub", "ord-p"}, world, false, true); }), "the fixed destination was not executed");
  require(refused([&] { portal::open({"hub", std::string(32, 'x')}, world); }), "a name must fit 32 bytes with its terminator");
  require(refused([&] { portal::open(std::vector<std::string>(16, "zz"), world); }), "16 names do not fit");
  auto lost = portal::open({"hub", "ord-q"}, world);
  require(refused([&] { portal::frame(lost, {}, world); }), "an unlisted current level was not executed");
  world.travel.ticks = 3;
  require(refused([&] { portal::open({"hub", "ord-p"}, world); }), "the executed menu opened with travel 0");
}

void test_portal_cancel() {
  for (std::uint32_t seed : {20973u, 7u}) {
    flow::State world;
    world.level = "ord-p";
    auto names = prior_names(seed);
    names.push_back(world.level);
    world.pending.name = "zcancel";
    world.pending.orientation = 0x5a;
    auto menu = portal::open(names, world);
    to_input(menu, world);
    portal::Pad cancel;
    cancel.pressed = 0x20;
    require(portal::frame(menu, cancel, world) == 3 && world.travel.ticks == 0 && menu.accept_sounds == 1,
            "cancel goes to state 3 with travel 0");
    for (int i = 0; i < 8; ++i) {
      require(menu.tasks && !menu.input_lock, "the tasks stay until the last fade frame");
      require(portal::frame(menu, {}, world) == 3, "each cancel fade frame stays in state 3");
    }
    require(!menu.tasks && menu.input_lock == portal::CANCEL_INPUT_LOCK, "cancel unlocks input (5) and removes both tasks");
    require(world.pending.name == "zcancel" && world.pending.orientation == 0x5a, "cancel writes nothing for a next world");
    require(world.travel.ticks == 0 && !world.travel.menu_open && !world.travel.paused, "cancel leaves no travel and no pause");
    require(refused([&] { portal::frame(menu, {}, world); }), "a removed menu does not run");
  }
}

// Authored level images, the shapes level_flow_tests.cpp uses.
using Blob = flow::Blob;
champions::simulation::rta::Raw fl(float v) {
  champions::simulation::rta::Raw r;
  std::memcpy(&r, &v, 4);
  return r;
}
flow::Vec3 vec(float x, float y, float z) { return {fl(x), fl(y), fl(z)}; }
void put(Blob &b, std::size_t p, std::uint32_t x, unsigned n = 4) {
  if (b.size() < p + n)
    b.resize(p + n);
  for (unsigned i = 0; i < n; ++i)
    b[p + i] = std::uint8_t(x >> (i * 8));
}
Blob objects(const std::vector<flow::Record> &rs) {
  Blob b(8), strings{0};
  put(b, 0, std::uint32_t(rs.size()), 2);
  auto intern = [&](const std::string &s) {
    auto p = std::uint32_t(strings.size());
    strings.insert(strings.end(), s.begin(), s.end());
    strings.push_back(0);
    return p;
  };
  for (auto &r : rs) {
    auto p = b.size();
    b.resize(p + 24 + r.props.size() * 4);
    put(b, p, intern(r.cls));
    put(b, p + 4, std::uint32_t(24 + r.props.size() * 4), 2);
    put(b, p + 6, r.angle, 2);
    for (unsigned i = 0; i < 3; ++i)
      put(b, p + 8 + 4 * i, r.position[i]);
    for (std::size_t i = 0; i < r.props.size(); ++i)
      put(b, p + 20 + 4 * i, intern(r.props[i]));
  }
  put(b, 4, std::uint32_t(b.size()));
  b.insert(b.end(), strings.begin(), strings.end());
  return b;
}
Blob lump(const std::vector<std::pair<std::string, Blob>> &entries) {
  Blob b(4 + 12 * entries.size());
  put(b, 0, std::uint32_t(entries.size()));
  for (std::size_t i = 0; i < entries.size(); ++i) {
    put(b, 4 + 12 * i, std::uint32_t(b.size()));
    b.insert(b.end(), entries[i].first.begin(), entries[i].first.end());
    b.push_back(0);
    put(b, 8 + 12 * i, std::uint32_t(b.size()));
    put(b, 12 + 12 * i, std::uint32_t(entries[i].second.size()));
    b.insert(b.end(), entries[i].second.begin(), entries[i].second.end());
  }
  return b;
}
Blob gob(const std::vector<std::pair<std::string, Blob>> &entries) {
  Blob b(0x800);
  for (std::size_t i = 0; i < entries.size(); ++i) {
    std::copy(entries[i].first.begin(), entries[i].first.end(), b.begin() + 40 * i);
    put(b, 40 * i + 32, 0xdeadbeef);
    put(b, 40 * i + 36, std::uint32_t(entries[i].second.size()));
    b.insert(b.end(), entries[i].second.begin(), entries[i].second.end());
    b.resize((b.size() + 0x7ff) & ~std::size_t(0x7ff));
  }
  return b;
}

void test_portal_arrival() {
  for (std::uint32_t seed : {20973u, 7u}) {
    std::map<std::string, Blob> files;
    const auto empty = lump({});
    for (auto n : {"FX.LMP;1", "HUD.LMP;1", "MAPICONS.LMP;1", "ICONS.LMP;1", "LOADMISC.LMP;1", "GOODDOG.LMP;1",
                   "SKELE_B.LMP;1", "MSEL.LMP;1"}) // the loader's hub-only lump (step 26)
      files[n] = empty;
    const auto here_gate = vec(20, 30, 40), hub_gate = vec(-70, 80, 9);
    auto level = [&](const std::string &name, std::vector<flow::Record> records) {
      auto main = lump({{"synthetic.world", {1, 2}}, {"objects.ob", objects(records)}, {"script.scr", {1}}});
      files[flow::cd_file_name(name + ".gob")] =
          gob({{name + ".lmp", main}, {"waterenv.lmp", lump({{"water.tex", {0}}})}});
    };
    level("ord-p", {{"Player1", 0, vec(1, 2, 3), {}}, {"Portal", 0, here_gate, {}}});
    // The destination's own gate, and one other authored object.
    level("hub", {{"Decor", 0, vec(5, 5, 5), {}}, {"Portal", 0, hub_gate, {}}});
    std::vector<std::string> io;
    flow::LoadServices services;
    services.disc = [&](const std::string &n) -> std::optional<Blob> {
      io.push_back(n);
      auto i = files.find(n);
      return i == files.end() ? std::nullopt : std::optional<Blob>(i->second);
    };
    services.range = [](const std::string &, std::size_t, std::size_t size) { return Blob(size); };
    services.player_lumps = [] { return false; };
    services.model_lumps = [](auto) { return std::vector<std::string>{}; };
    services.event = [](const std::string &) {};
    flow::Loader loader(services);
    flow::State state;
    state.acts = {{{"hub", 100}, {"ord-p", 60}, {"ord-q", 40}}};
    state.players[0].active = true;
    state.players[0].hp = state.players[0].max_hp = 10;
    // g_randSeed and its step (docs/mission2-round6.md:342-345).
    std::uint32_t rand_seed = seed;
    std::vector<unsigned> travel_at_gate;
    std::vector<std::tuple<flow::Vec3, champions::simulation::rta::Raw, champions::simulation::rta::Raw, bool>> spots;
    std::vector<std::tuple<unsigned, flow::Vec3, std::uint16_t>> players;
    const auto ground = fl(-49);
    flow::SessionHooks hooks;
    hooks.find_free_spot = [&](const flow::Vec3 &p, champions::simulation::rta::Raw spread, champions::simulation::rta::Raw radius, bool any) {
      spots.emplace_back(p, spread, radius, any);
      // The free-spot geometry is the caller's: an authored spot beside it.
      return flow::Vec3{fl(-67), fl(84), p[2]};
    };
    hooks.shared_draw = [&] {
      rand_seed = rand_seed * 0x19660du + 0x3c6ef35fu;
      return std::uint16_t(rand_seed >> 16);
    };
    hooks.terrain_height = [&](champions::simulation::rta::Raw, const flow::Vec3 &) { return ground; };
    hooks.event = [](const std::string &) {};
    hooks.script = [](const std::string &, const Blob &) {};
    hooks.construct = [&](const flow::Record &r) {
      if (r.cls == "Portal")
        travel_at_gate.push_back(state.travel.ticks);
    };
    hooks.restore_level_state = [](const Blob &) { return std::vector<flow::Record>{}; };
    hooks.construct_player = [&](unsigned slot, const flow::Vec3 &p, std::uint16_t facing) {
      players.emplace_back(slot, p, facing);
    };
    hooks.remove_flag39_items = [] {};
    hooks.disable_unclaimed_pieces = [] {};
    flow::SessionAdapter adapter(state, loader, hooks);
    flow::Placement placement;
    placement.players = 1;
    require(adapter.request_script("ord-p", {}, false, 0), "the authored start request");
    adapter.change({}, {}, placement);
    require(state.level == "ord-p" && state.gates.names == std::vector<std::string>({"hub"}),
            "the act entry resets the list to the hub");
    // Round 5's portal use: the first prompt appends the current level, then
    // the use opens the menu with two names (docs/mission2-round5.md:225-233).
    bool found = false;
    require(adapter.prompt_gate(found, true, {}, "hub").usable, "two destinations make the prompt usable");
    require(state.gates.names == std::vector<std::string>({"hub", "ord-p"}), "hub listed before the current level");
    auto menu = portal::open(state.gates.names, state);
    to_input(menu, state);
    // Index 0, the first destination (difftest_mission2_r6.py:607).
    portal::Pad confirm;
    confirm.pressed = 0x40;
    require(portal::frame(menu, confirm, state) == 3 && state.travel.ticks == 1, "confirm row 0");
    while (menu.state != 6)
      portal::frame(menu, {}, state);
    require(state.pending.name == "hub" && state.travel.ticks == 181, "the gate requested the hub with travel 181");
    // Sentinels where the handoff is asserted to store (:313-318).
    state.orientation_copy = 0x5a;
    state.prev = "zprev";
    travel_at_gate.clear();
    spots.clear();
    players.clear();
    io.clear();
    // The handoff takes only the request state 5 wrote; it refuses before
    // the adapter is called.
    state.pending.orientation = 0;
    require(refused([&] { portal::handoff(adapter, state, {}, {}, placement); }) && state.level == "ord-p" && io.empty(),
            "a request without orientation 0xff is not the gate's");
    state.pending.orientation = 0xff;
    const auto rand_before = rand_seed;
    const auto done = portal::handoff(adapter, state, {}, {}, placement);
    require(done.stored == "ord-p" && done.departure_entry, "the departure store gets the level being left");
    require(done.loaded == "hub" && std::count(io.begin(), io.end(), flow::cd_file_name("hub.gob")) == 1,
            "the loader gets the hub");
    require(done.level == "hub" && done.sub.empty() && done.previous == "ord-p" && done.pending.empty(),
            "the name splits to the hub with an empty subname; previous is the level left");
    require(done.orientation_copy == 0xff && done.orientation == 0xfe, "0x41b7c1 holds 0xff and 0x41b720 holds 0xfe");
    // Arrival at the destination's own gate (:334-363).
    require(travel_at_gate == std::vector<unsigned>({181}), "the hub's gate is built once and sees travel 181");
    require(spots.size() == 1, "one free-spot search for one player");
    const auto &spot = spots[0];
    require(std::get<0>(spot) == hub_gate && std::get<1>(spot) == fl(36) && std::get<2>(spot) == fl(24) &&
                std::get<3>(spot),
            "the search is near the destination gate with 0x24 and 0x18");
    const auto facing = std::uint16_t((rand_before * 0x19660du + 0x3c6ef35fu) >> 16);
    require(rand_seed == rand_before * 0x19660du + 0x3c6ef35fu, "the gate steps g_randSeed once for one player");
    require(state.spawn.rank == 3, "the gate stores 3 at 0x4f7108");
    require(players.size() == 1, "Player_ctor is called once");
    const auto &[slot, at, heading] = players[0];
    require(slot == 0, "player 0 is constructed");
    require(at[0] == fl(-67) && at[1] == fl(84) && at[2] == ground,
            "player 0 is at the free spot beside the gate, on the terrain height");
    require(heading == facing, "the facing is g_randSeed's high half");
    require(static_cast<std::int32_t>(static_cast<std::int16_t>(heading)) ==
                static_cast<std::int32_t>(static_cast<std::int16_t>(facing)),
            "the sign-extended heading");
    require(menu.state == 6 && refused([&] { portal::frame(menu, {}, state); }),
            "the menu is left in state 6: state 6 and 7 are not run");
  }
}

void test_destinations() {
  for (std::uint32_t seed : {20973u, 7u}) {
    auto names = prior_names(seed);
    names.push_back("ord-p");
    const auto area = portal::from_names(names);
    const auto saved = portal::save_destinations(area);
    require(saved.ret == 0x1e4 && saved.block.size() == 0x1e4, "the save returns 0x1e4 and writes 0x1e4 bytes");
    const std::uint32_t word = std::uint32_t(saved.block[0]) | std::uint32_t(saved.block[1]) << 8 |
                               std::uint32_t(saved.block[2]) << 16 | std::uint32_t(saved.block[3]) << 24;
    require(word == names.size(), "the count word equals the list's count");
    require(std::equal(area.bytes.begin(), area.bytes.end(), saved.block.begin() + 4), "the 480 bytes equal the list");
    namespace save = champions::simulation::rta::ps2_save;
    require(saved.block == save::pack_jump(area.count, save::Bytes(area.bytes.begin(), area.bytes.end())),
            "the block is the +0x134 row the save writer packs");
    require(portal::SAVE_OFFSET == 0x134 && save::JUMP_SIZE == portal::DESTINATION_BLOCK, "the +0x134 row is 0x1e4 bytes");
    const auto loaded = portal::load_destinations(saved.block);
    require(loaded.ret == 0x1e4 && loaded.destinations.count == area.count && loaded.destinations.bytes == area.bytes,
            "the load restores the count and the list exactly");
    require(portal::to_names(loaded.destinations) == names, "the names come back in order");
  }
  // Sign extension on save and the low byte on load (:260-264).
  portal::Destinations high;
  high.count = 0x85;
  const auto saved = portal::save_destinations(high);
  require(saved.block[0] == 0x85 && saved.block[1] == 0xff && saved.block[2] == 0xff && saved.block[3] == 0xff,
          "the count byte is sign-extended");
  auto block = portal::save_destinations(portal::from_names({"hub"})).block;
  block[1] = 0x01;
  block[3] = 0xff;
  require(portal::load_destinations(block).destinations.count == 1, "the load keeps only the word's low byte");
  require(refused([&] { portal::to_names(high); }), "a count above 15 reads past the list");
  require(refused([&] { portal::load_destinations(Blob(0x1e3)); }), "a short block refuses");
  require(refused([&] { portal::from_names(std::vector<std::string>(16, "zz")); }), "16 names do not fit");
}
} // namespace

int main() {
  test_reset();
  test_insert_and_route();
  test_portal();
  test_refusals();
  test_portal_menu();
  test_portal_cancel();
  test_portal_arrival();
  test_destinations();
  std::cout << "mission2 route: " << checks << " checks PASS\n";
  return 0;
}
