// Story route after the first menu request. Authored fixtures only: synthetic
// mission rows and act lists, and authored AMX images whose entries carry the
// research's entry identifiers and authored strings. No shipped script,
// table payload, level text, quest or help key.
// Research: t3/research-story-route-r3 3c7987c docs/story-route.md sections 6-8.
#include "champions/amx.hpp"
#include "champions/quests.hpp"
#include "champions/script_commands.hpp"
#include "champions/story_route.hpp"

#include <algorithm>
#include <iostream>
#include <iterator>
#include <set>
#include <string>
#include <vector>

namespace sc = champions::simulation::rta::script_commands;
namespace q = champions::simulation::rta::quests;
namespace route = champions::simulation::rta::story_route;
namespace a = champions::simulation::rta::amx;

namespace {
int failed = 0;
int checks = 0;
void check(bool ok, const std::string &what) {
  ++checks;
  if (ok)
    return;
  std::cerr << "FAIL " << what << '\n';
  ++failed;
}
template <class F> bool refuses(F &&fn) {
  try {
    fn();
  } catch (const route::Refused &) {
    return true;
  }
  return false;
}
std::size_t at(const char *name) {
  for (std::size_t i = 0; i < sc::COMMANDS; ++i)
    if (std::string(sc::row(i).name) == name)
      return i;
  return sc::COMMANDS;
}

// Authored menu table: thirteen rows, story names on 1..12, evil names on
// missions 6, 10 and 12 only (the executed shape), and one act per row whose
// prefixes cover that row's names.
// Two-digit names so no act prefix is a prefix of another row's name
// (save_findAct is a case-sensitive prefix match, docs/story-route.md:273-281).
std::string story(int mission) { return "auth-" + std::string(mission < 10 ? "0" : "") + std::to_string(mission) + "-s"; }
std::string evil(int mission) { return "auth-" + std::string(mission < 10 ? "0" : "") + std::to_string(mission) + "-e"; }
std::vector<q::MissionRow> authored_rows() {
  std::vector<q::MissionRow> rows(13);
  for (int row = 0; row < 12; ++row) {
    rows[std::size_t(row)].story = story(row + 1);
    if (row + 1 == 6 || row + 1 == 10 || row + 1 == 12)
      rows[std::size_t(row)].evil = evil(row + 1);
  }
  return rows;
}
std::vector<std::vector<std::string>> authored_acts() {
  std::vector<std::vector<std::string>> acts;
  for (int row = 0; row < 12; ++row) {
    std::vector<std::string> act = {story(row + 1)};
    if (row + 1 == 6 || row + 1 == 10 || row + 1 == 12)
      act.push_back(evil(row + 1));
    acts.push_back(act);
  }
  return acts;
}

void test_menu_table() {
  const auto rows = authored_rows();
  const auto acts = authored_acts();
  const auto entries = route::menu_entries(rows, acts);
  check(entries.size() == 12, "twelve menu rows; mission 0 is not a row");
  int evil_rows = 0;
  for (const auto &entry : entries) {
    check(entry.mission == entry.row + 1, "row r is mission r + 1");
    check(entry.act == entry.row, "the story level's act from -1 is the row index");
    check(entry.story == rows[std::size_t(entry.row)].story, "story name is the caller's table name");
    if (!entry.evil.empty())
      ++evil_rows;
  }
  check(evil_rows == 3 && !entries[5].evil.empty() && !entries[9].evil.empty() && !entries[11].evil.empty(),
        "only missions 6, 10 and 12 carry an evil level");

  auto stray = rows;
  stray[2].evil = "auth-stray";
  check(refuses([&] { route::menu_entries(stray, acts); }), "an evil pointer on mission 3 is refused");
  auto missing = rows;
  missing[9].evil.clear();
  check(refuses([&] { route::menu_entries(missing, acts); }), "mission 10 without its evil level is refused");
  auto swapped = acts;
  std::swap(swapped[3], swapped[4]);
  check(refuses([&] { route::menu_entries(rows, swapped); }), "acts out of row order are refused");
  auto split = acts;
  split[5] = {story(6)};
  split[6].push_back(evil(6));
  check(refuses([&] { route::menu_entries(rows, split); }), "an evil level in another act is refused");
  check(refuses([&] { route::menu_entries(std::vector<q::MissionRow>(11), acts); }),
        "fewer than twelve rows are refused");
  auto empty = rows;
  empty[7].story.clear();
  check(refuses([&] { route::menu_entries(empty, acts); }), "an empty story pointer is refused");
}

// A campaign with one player at the image difficulty and the authored table.
q::Campaign campaign_with(const std::vector<int> &available) {
  q::Campaign campaign;
  campaign.set_num_players(1);
  campaign.set_rows(authored_rows());
  campaign.set_acts(authored_acts());
  for (int mission : available)
    campaign.mission_set_available(mission, 1);
  return campaign;
}
void fade_in(q::Campaign &campaign) {
  for (int tick = 0; tick < 17; ++tick)
    campaign.tick_menu(0);
}
bool confirm(q::Campaign &campaign) {
  campaign.tick_menu(q::BTN_CROSS);
  for (int tick = 0; tick < 8; ++tick) {
    q::MenuInputs inputs;
    inputs.fade = 128;
    if (campaign.tick_menu(0, inputs))
      return true;
  }
  return false;
}

void test_story_request() {
  const auto acts = authored_acts();
  auto cleared = campaign_with({});
  auto request = route::story_request(cleared, 0, acts);
  check(request.level == story(1) && request.act == 0 && request.orientation == 0xFF && !request.offered,
        "row 0 requests mission 1's level with its bit clear, and that is not an offer");
  check(refuses([&] { route::story_request(cleared, 2, acts); }), "a locked later row is refused");
  check(refuses([&] { route::story_request(cleared, 12, acts); }), "row 12 is outside missions 1..12");

  auto six = campaign_with({1, 2, 3, 4, 5, 6});
  check(route::story_request(six, 5, acts).level == story(6), "evil clear: mission 6's story level");
  six.record(0).evil = 1u << q::IMAGE_DIFFICULTY;
  check(route::story_request(six, 5, acts).level == evil(6) && route::story_request(six, 5, acts).act == 5,
        "evil set: mission 6's evil level, same act");
  check(route::story_request(six, 4, acts).level == story(5), "a row without an evil pointer keeps its story level");
  // Difficulty 1's evil bit is bit 1; bit 0 still selects the story level.
  six.record(0).evil = 1u;
  check(route::story_request(six, 5, acts).level == story(6), "bit 0 does not select difficulty 1's evil level");
}

void test_menu_cursor() {
  // State 0 selects the highest available mission in 1..12.
  auto campaign = campaign_with({1, 2, 3});
  campaign.open_menu(0);
  fade_in(campaign);
  check(campaign.menu().state == 2 && campaign.menu().row == 2, "opening row is the highest available mission");
  // Completing the current mission does not move the row.
  campaign.mission_set_complete(3, -1);
  campaign.tick_menu(0);
  check(campaign.menu().row == 2, "completion does not move the cursor");
  // The next mission's bit does not move it until down.
  campaign.mission_set_available(4, 1);
  campaign.tick_menu(0);
  check(campaign.menu().row == 2, "a new available bit does not move the open cursor");
  campaign.tick_menu(q::BTN_DOWN);
  check(campaign.menu().row == 3, "down steps onto the newly available mission");

  // Down will not step onto a locked mission.
  auto gap = campaign_with({1, 2, 4});
  gap.open_menu(0);
  fade_in(gap);
  check(gap.menu().row == 3, "the highest available mission opens above a locked gap");
  gap.tick_menu(q::BTN_UP);
  check(gap.menu().row == 2, "up sits on the locked row");
  gap.tick_menu(q::BTN_DOWN);
  check(gap.menu().row == 3, "down from the locked row steps onto the next available mission");
  gap.tick_menu(q::BTN_UP);
  gap.tick_menu(q::BTN_UP);
  check(gap.menu().row == 1, "up twice reaches row 1");
  gap.tick_menu(q::BTN_DOWN);
  check(gap.menu().row == 1, "down does not step onto locked mission 3");
  gap.tick_menu(q::BTN_CROSS);
  check(gap.menu().state == 5, "an available row confirms");

  // Reopen keeps row, pending and orientation until the state-0 tick.
  auto reopen = campaign_with({1, 2});
  reopen.open_menu(0);
  fade_in(reopen);
  check(confirm(reopen) && reopen.menu().pending == story(2) && reopen.menu().orient == 0xFF,
        "story confirm requests the row's level with orientation 0xff");
  reopen.mission_set_available(3, 1);
  reopen.open_menu(0);
  check(reopen.menu().state == 0 && reopen.menu().row == 1 && reopen.menu().pending == story(2) &&
            reopen.menu().orient == 0xFF,
        "reopen keeps the row and the old request until the state-0 tick");
  reopen.tick_menu(0);
  check(reopen.menu().row == 2 && reopen.menu().pending == story(2),
        "the state-0 tick selects the new highest row and leaves the old request");
}

void test_menu_confirm() {
  // Row 0 accepts with mission 1's bit clear; the byte stays 0.
  auto cleared = campaign_with({});
  cleared.open_menu(0);
  fade_in(cleared);
  check(cleared.menu().row == 0, "no available mission opens on row 0");
  check(confirm(cleared) && cleared.menu().pending == story(1), "row 0 confirms with mission 1's bit clear");
  check(cleared.record(0).missions[std::size_t(q::mission_index(q::IMAGE_DIFFICULTY, 1))] == 0,
        "confirm does not write mission 1's byte");

  // A locked later row does not confirm.
  auto locked = campaign_with({1, 2, 4});
  locked.open_menu(0);
  fade_in(locked);
  locked.tick_menu(q::BTN_UP);
  locked.tick_menu(q::BTN_CROSS);
  check(locked.menu().state == 2 && locked.menu().row == 2, "a locked row's story column does not confirm");

  // Evil rows: missions 6, 10 and 12 use their evil level when evil is set.
  for (int mission : {5, 6, 10, 12}) {
    std::vector<int> open;
    for (int m = 1; m <= mission; ++m)
      open.push_back(m);
    auto chosen = campaign_with(open);
    chosen.record(0).evil = 1u << q::IMAGE_DIFFICULTY;
    const auto before = chosen.record(0).missions;
    chosen.open_menu(0);
    fade_in(chosen);
    check(confirm(chosen), "mission " + std::to_string(mission) + " confirms");
    const bool has_evil = mission == 6 || mission == 10 || mission == 12;
    const auto expected = has_evil ? evil(mission) : story(mission);
    check(chosen.menu().pending == expected, "evil confirm on mission " + std::to_string(mission));
    check(chosen.record(0).missions == before, "mission bytes are unchanged across the menu write");
  }
}

void test_in_level() {
  check(route::in_level(1) == route::InLevel::Mission1Notes && route::in_level(2) == route::InLevel::Mission2Route,
        "missions 1 and 2 keep their own research");
  for (int mission = 3; mission <= 5; ++mission)
    check(route::in_level(mission) == route::InLevel::StoryEntries && !route::in_level_refusal(mission),
          "missions 3-5 follow sections 7-8");
  for (int mission = 6; mission <= 12; ++mission) {
    const auto reason = route::in_level_refusal(mission);
    check(route::in_level(mission) == route::InLevel::Refused && reason &&
              reason->find("in-level beats are not established") != std::string::npos,
          "missions 6-12 request the table level and refuse");
  }
  check(refuses([] { route::in_level(13); }) && refuses([] { route::in_level(0); }), "mission 0 and 13 are not rows");
  const auto rows = authored_rows();
  check(route::table_mission(rows, story(7)) == 7 && route::table_mission(rows, "AUTH-10-E_sub") == 10 &&
            route::table_mission(rows, "auth-other") == 0,
        "a requested base name maps to its table mission");
}

void test_entry_lists() {
  std::set<std::pair<std::string, std::string>> seen;
  for (const auto &entry : route::observed_entries()) {
    check(seen.insert({entry.level, entry.name}).second, "observed entries are unique");
    check(route::story_level_mission(entry.level) == entry.mission, "each listed entry's level has its mission");
    check(!route::entry_refusal(entry.level, entry.name, false), "a listed entry may start");
    check(route::entry_refusal(entry.level, entry.name, true).has_value(), "object use of a listed entry refuses");
  }
  check(seen.size() == 75, "75 executed or parsed entries across missions 3-5");
  for (const auto &entry : route::refused_entries()) {
    check(!seen.count({entry.level, entry.name}), "a refused entry is not also listed");
    const auto reason = route::entry_refusal(entry.level, entry.name, false);
    check(reason && reason->find(entry.reason) != std::string::npos, "a refused entry reports its reason");
  }
  check(route::refused_entries().size() == 13, "thirteen named refusals");
  for (const char *level : {"islsm1", "islsm2a", "islsm3a"})
    check(route::story_level_mission(level) == 3 && route::entry_refusal(level, "start", false),
          "a travel destination with no executed entry refuses start");
  check(!route::story_level("hub") && !route::story_level("hub2") && !route::story_level("innovat1") &&
            !route::gate_for("hub") && !route::entry_refusal("hub", "anything", true),
        "the hubs and mission 2 keep their own policy");
  check(route::story_level_mission("TORMENTB_frontdoor") == 5, "a subname request keeps its level's mission");
  check(route::dialog_refusal("islc1").find("dialog selection") != std::string::npos &&
            route::dialog_refusal("hub").empty(),
        "dialog selection refuses only in mission 3-5 levels");

  int quests = 0, stores = 0;
  std::set<int> missions;
  for (const auto &entry : route::availability_entries()) {
    missions.insert(entry.mission);
    quests += entry.adds_quest;
    stores += entry.store_calls;
    check(std::string(entry.level) == "hub" || std::string(entry.level) == "hub2", "availability is a hub entry");
  }
  check(route::availability_entries().size() == 17 && quests == 4 && stores == 8 &&
            missions == std::set<int>{4, 5, 6},
        "seventeen hub/hub2 availability entries for missions 4-6");
}

// ---- Host fixtures -------------------------------------------------------
struct Built {
  a::ByteVector image;
  std::vector<std::pair<unsigned, std::string>> strings;
};
struct Call {
  std::string native;
  std::vector<a::Cell> args;
};
Built build_calls(const std::vector<std::pair<std::string, std::vector<Call>>> &publics,
                  const std::vector<std::pair<unsigned, std::string>> &strings, bool keep_result = false) {
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
  for (const auto &entry : publics) {
    program.push_back(a::Label{entry.first + ":"});
    ins("PROC");
    for (const auto &call : entry.second) {
      for (auto it = call.args.rbegin(); it != call.args.rend(); ++it)
        ins("PUSH.C", {*it});
      ins("PUSH.C", {a::Cell(4 * call.args.size())});
      ins("SYSREQ.C", {native_index(call.native)});
      ins("STACK", {a::Cell(4 * (call.args.size() + 1))});
    }
    if (!keep_result)
      ins("ZERO.pri");
    ins("RETN");
  }
  const auto code = a::assemble(program);
  a::ImageOptions options;
  options.data.resize(4096);
  options.natives = natives;
  options.stack_cells = 256;
  for (const auto &entry : publics)
    options.publics.emplace_back(entry.first, a::Cell(code.labels.at(entry.first)));
  Built built;
  built.image = a::build_image(code.cells, options);
  built.strings = strings;
  return built;
}
void plant(sc::Host &host, const Built &built) {
  check(host.load(built.image), "synthetic script loads");
  if (!host.machine())
    return;
  for (const auto &entry : built.strings)
    host.machine()->set_string(entry.first, entry.second, false);
}
constexpr std::uint32_t PENDING = 0x41b6d0, ORIENT = 0x41b720, ENTRANCE = 0x4f8468;
constexpr std::uint8_t SENTINEL = 0xA0; // docs/story-route.md:381-383, :565-567

// One player at the image difficulty, missions 3..6 planted at the 0xA0
// sentinel on all four records, through the campaign image.
void plant_missions(sc::Host &host) {
  auto campaign = q::Campaign::load(host.save_campaign());
  for (int slot = 0; slot < q::PLAYERS; ++slot)
    for (int mission = 3; mission <= 6; ++mission)
      campaign.record(slot).missions[std::size_t(q::mission_index(q::IMAGE_DIFFICULTY, mission))] = SENTINEL;
  host.load_campaign(campaign.save());
}
std::uint8_t byte(const sc::Host &host, int slot, int mission) {
  return host.campaign().record(slot).missions[std::size_t(q::mission_index(q::IMAGE_DIFFICULTY, mission))];
}
void gated(sc::Host &host, const std::string &level, const Built &built) {
  plant(host, built);
  host.start_single_player();
  host.require_complete_commands();
  host.set_entry_gate(route::gate_for(level));
  plant_missions(host);
}

void test_gate_on_host() {
  check(route::gate_for("islc1").has_value() && !route::gate_for("innovat1"), "only story levels get a gate");
  // Level start: availability and stores run; the posted intro reaches the
  // refused cut request and stops there.
  auto image = build_calls(
      {{"start", {{"setMissionAvailable", {3, 1}}, {"setv", {2000, 1}}, {"setv", {2100, 3}}, {"postAction", {2200}}}},
       {"absorb", {{"cutScene", {2300}}, {"setv", {2400, 1}}}},
       {"unlisted", {{"grantMission", {3}}}}},
      {{2000, "auth-started"}, {2100, "auth-current"}, {2200, "absorb"}, {2300, "auth-cut"}, {2400, "auth-seen"}});
  {
    sc::Host host;
    gated(host, "islc1", image);
    check(host.run_start() == sc::Stop::Refused, "the posted intro stops at the refused cut request");
    check(byte(host, 0, 3) == 0xA1 && byte(host, 1, 3) == SENTINEL && byte(host, 0, 4) == SENTINEL,
          "start makes mission 3 available on record 0 only and leaves mission 4");
    check(host.last_refusal().find("refused cutScene") == 0 && host.counts().entry_refusals == 0,
          "the cut request is a command refusal, not an entry refusal");
    check(host.counts().executed[at("setv")] == 2, "the intro's later store did not run");
    check(!host.campaign().mission_complete(3), "start does not grant mission 3");
  }
  {
    sc::Host host;
    gated(host, "islc1", image);
    check(host.run("unlisted") == sc::Stop::Refused, "an unlisted entry is refused before it starts");
    check(host.counts().entry_refusals == 1 && host.counts().executed[at("grantMission")] == 0 &&
              byte(host, 0, 3) == SENTINEL,
          "a refused entry runs no command");
    check(host.last_refusal().find("outside the executed mission 3 entries") != std::string::npos,
          "the refusal names the missing evidence");
    check(host.run("start") == sc::Stop::Refused, "the refusal stops the level script");
  }
  {
    // A level whose start was not run refuses at load.
    sc::Host host;
    gated(host, "wate2a", image);
    check(host.run_start() == sc::Stop::Refused && host.counts().entry_refusals == 1 &&
              host.last_refusal().find("compared shardCheck variable") != std::string::npos,
          "a side level's start is refused with its reason");
    check(byte(host, 0, 3) == SENTINEL, "nothing in the refused start ran");
  }
  {
    // The same image outside a story level keeps the existing policy.
    sc::Host host;
    plant(host, image);
    host.start_single_player();
    host.set_entry_gate(route::gate_for("hub"));
    check(!host.has_entry_gate() && host.run("unlisted") == sc::Stop::Complete &&
              host.campaign().mission_complete(3),
          "no gate outside the story levels");
  }
}

void test_nested_entries() {
  auto image = build_calls({{"callScri8a40", {{"callScript", {2000}}}},
                            {"endMission", {{"grantMission", {3}}}},
                            {"callScri7351", {{"callScript", {2100}}}},
                            {"steppedOnTrigger", {{"grantMission", {3}}}},
                            {"notListed", {{"grantMission", {3}}}}},
                           {{2000, "endMission"}, {2100, "notListed"}});
  {
    sc::Host host;
    gated(host, "islc2", image);
    check(host.run("callScri8a40") == sc::Stop::Complete, "the completion wrapper runs its listed callee");
    check(byte(host, 0, 3) == 0xA2 && byte(host, 1, 3) == 0xA2 && byte(host, 2, 3) == 0xA2 &&
              byte(host, 3, 3) == 0xA2 && byte(host, 0, 4) == SENTINEL,
          "the grant sets bit 1 on all four records from 0xA0 and leaves mission 4");
  }
  {
    // islc2 does not list steppedOnTrigger's wrapper, and the wrapper's callee
    // is a name no level lists: both refuse.
    sc::Host host;
    gated(host, "islc2", image);
    check(host.run("callScri7351") == sc::Stop::Refused && host.counts().entry_refusals == 1,
          "an entry another level lists is refused here");
  }
  {
    sc::Host host;
    gated(host, "islcave1", image);
    const auto stop = host.run("callScri7351");
    check(stop == sc::Stop::Refused && host.counts().entry_refusals == 1 &&
              host.counts().executed[at("callScript")] == 1 &&
              host.last_refusal().find("notListed") != std::string::npos,
          "a nested callScript to an unlisted entry is refused at its start (stop " +
              std::to_string(int(stop)) + ", entry refusals " + std::to_string(host.counts().entry_refusals) +
              ", callScript " + std::to_string(host.counts().executed[at("callScript")]) + ", " +
              host.last_refusal() + ")");
    check(byte(host, 0, 3) == SENTINEL, "the refused nested entry granted nothing");
  }
}

void test_object_use_refused() {
  // One authored objects.ob record named gate, the same authored shape as
  // script_commands_tests.cpp gate_objects. The class string is a placeholder.
  std::vector<std::uint8_t> blob;
  auto u16 = [&](std::uint16_t v) {
    blob.push_back(static_cast<std::uint8_t>(v));
    blob.push_back(static_cast<std::uint8_t>(v >> 8));
  };
  auto u32 = [&](std::uint32_t v) {
    for (int shift = 0; shift < 32; shift += 8)
      blob.push_back(static_cast<std::uint8_t>(v >> shift));
  };
  const std::string cls = "Stub", prop = "name=gate";
  u16(1);
  u16(0);
  u32(40);
  u32(0);
  u16(32);
  u16(0);
  u32(0);
  u32(0);
  u32(0);
  u32(static_cast<std::uint32_t>(cls.size() + 1));
  u32(0);
  u32(0);
  blob.insert(blob.end(), cls.begin(), cls.end());
  blob.push_back(0);
  blob.insert(blob.end(), prop.begin(), prop.end());
  blob.push_back(0);
  auto image = build_calls({{"lostHope", {{"newWorld", {2000}}}}}, {{2000, "auth-dest"}});
  sc::Host host;
  gated(host, "islc1", image);
  if (!host.adopt_objects(blob.data(), blob.size())) {
    check(false, "authored object record adopts");
    return;
  }
  check(host.run_object_entry(0, "lostHope") == sc::Stop::Refused &&
            host.last_refusal().find("object use or trigger dispatch") != std::string::npos,
        "object use of a listed travel entry is refused");
  check(host.variables().memory.cstr(PENDING).empty(), "the refused use requests no level");
}

void test_observed_effects() {
  // Availability: record 0 only (docs/story-route.md:391-398, :599-611).
  // Completion: all four records (docs/story-route.md:476-494, :623-634).
  // Quest action: one quest, mission bytes unchanged (:424-428, :804-807).
  // Travel: level, entrance, orientation 0xff with no object (:430-440, :638-670, :832-848).
  // Removal of the last quest leaves NoQuests (:556-560, :708-712, :815-830).
  auto image = build_calls(
      {{"start", {{"setMissionAvailable", {4, 1}}, {"setv", {2000, 1}}, {"setv", {2100, 4}}, {"addQuest", {2200, 2300}}}},
       {"steppedOnTrigger", {{"grantMission", {4}}}},
       {"setv2725", {{"setv", {2400, 1}}, {"addQuest", {2500, 2600}}}},
       {"endtor", {{"grantMission", {5}}, {"setv", {2700, 1}}, {"removeQuest", {2800}}}},
       {"newWorld57c0", {{"newWorld", {2900}}}},
       {"lostHope", {{"setEntrance", {3000}}, {"newWorld", {3100}}}}},
      {{2000, "auth-started"}, {2100, "auth-current"}, {2200, "auth-title-a"}, {2300, "auth-body-a"},
       {2400, "auth-talked"}, {2500, "auth-title-b"}, {2600, "auth-body-b"}, {2700, "auth-finished"},
       {2800, "auth-title-c"}, {2900, "auth-side"}, {3000, "auth-boat"}, {3100, "auth-isle"}});
  {
    sc::Host host;
    gated(host, "wat1", image);
    check(host.run("start") == sc::Stop::Complete, "mission 4 start runs");
    check(byte(host, 0, 4) == 0xA1 && byte(host, 1, 4) == SENTINEL && byte(host, 0, 5) == SENTINEL &&
              byte(host, 0, 6) == SENTINEL,
          "mission 4 available on record 0; missions 5 and 6 unchanged");
    check(host.campaign().record(0).quest_count == 1 &&
              q::field_text(host.campaign().record(0).quests[0].title) == "auth-title-a",
          "the start adds its one quest");
    check(host.run("newWorld57c0") == sc::Stop::Complete, "a listed key action runs");
    check(host.variables().memory.cstr(PENDING) == "auth-side" && host.variables().memory.read(ORIENT) == 0xFF &&
              host.variables().memory.cstr(ENTRANCE).empty(),
          "travel requests the level with orientation 0xff and leaves the entrance");
    check(!host.campaign().mission_complete(4), "travel is not completion");
  }
  {
    sc::Host host;
    gated(host, "water1", image);
    host.variables().memory.string(PENDING, "PENDING-SENTINEL");
    check(host.run("steppedOnTrigger") == sc::Stop::Complete, "the water trigger grant runs");
    check(byte(host, 0, 4) == 0xA2 && byte(host, 3, 4) == 0xA2 && byte(host, 0, 5) == SENTINEL &&
              host.campaign().record(0).quest_count == 0 && host.variables().memory.cstr(PENDING) == "PENDING-SENTINEL",
          "the grant sets all four records, edits no quest and requests no level");
  }
  {
    sc::Host host;
    gated(host, "tormenta", image);
    check(host.run("setv2725") == sc::Stop::Complete && host.campaign().record(0).quest_count == 1 &&
              byte(host, 0, 5) == SENTINEL,
          "the gol action adds one quest and leaves the mission bytes");
  }
  {
    // A planted quest whose title is the removal argument: the last entry
    // leaves the placeholder.
    sc::Host host;
    gated(host, "tormenta", image);
    auto campaign = q::Campaign::load(host.save_campaign());
    campaign.quest_add("auth-title-c", "auth-body-c");
    host.load_campaign(campaign.save());
    check(host.campaign().record(0).quest_count == 1, "the removal fixture plants one quest");
    check(host.run("endtor") == sc::Stop::Complete, "the completion runs");
    check(byte(host, 0, 5) == 0xA2 && byte(host, 3, 5) == 0xA2 && byte(host, 0, 4) == SENTINEL &&
              byte(host, 0, 6) == SENTINEL,
          "endtor grants mission 5 on four records and leaves missions 4 and 6");
    check(host.campaign().record(0).quest_count == 0 &&
              q::field_text(host.campaign().record(0).quests[0].title) == q::QUEST_PLACEHOLDER,
          "removing the last quest leaves the placeholder");
  }
  {
    sc::Host host;
    gated(host, "islc1", image);
    check(host.run("lostHope") == sc::Stop::Complete && host.variables().memory.cstr(PENDING) == "auth-isle" &&
              host.variables().memory.cstr(ENTRANCE) == "auth-boat" && host.variables().memory.read(ORIENT) == 0xFF &&
              byte(host, 0, 3) == SENTINEL,
          "an islc1 travel entry requests its level and entrance, not completion");
  }
}

void test_offset_zero_travel() {
  // Executed travel entries pass data offset 0; research recorded the level
  // they request (docs/story-route.md:638-670, :832-848). The mission-2
  // null-destination refusal stays for ungated levels.
  auto image = build_calls({{"newWorld57c0", {{"newWorld", {0}}}}}, {{0, "auth-zero"}});
  {
    sc::Host host;
    gated(host, "wat1", image);
    check(host.run("newWorld57c0") == sc::Stop::Complete && host.variables().memory.cstr(PENDING) == "auth-zero" &&
              host.variables().memory.read(ORIENT) == 0xFF,
          "a gated story level reads offset 0 as the binder does");
  }
  {
    sc::Host host;
    plant(host, image);
    host.start_single_player();
    host.require_complete_commands();
    check(host.run("newWorld57c0") == sc::Stop::Refused &&
              host.last_refusal().find("null destination is Unknown") != std::string::npos &&
              host.variables().memory.cstr(PENDING).empty(),
          "the mission-2 boundary still refuses offset 0 without the gate");
  }
}

void test_item_count_condition() {
  // The item count is the client's inventory_countItemByName over the bag,
  // not a stub. An empty bag counts 0 (docs/story-route.md:469-474, :686-706).
  auto image = build_calls({{"iffbe4", {{"playerHasItem", {2000}}}}}, {{2000, "auth-item"}}, true);
  sc::Host host;
  gated(host, "islc2", image);
  check(host.run("iffbe4") == sc::Stop::Complete && host.last_value() == 0 &&
            host.counts().executed[at("playerHasItem")] == 1 && host.counts().refused[at("playerHasItem")] == 0,
        "the item condition runs the client's count");
}

void test_hub_availability() {
  // The hub entries set the next mission's bit on record 0 only; caba/6d46
  // add a quest (docs/story-route.md:504-510, :861-878). Authored bodies with
  // the listed native shape; activateStore stays a refused shop call.
  for (const auto &entry : route::availability_entries()) {
    std::vector<Call> calls = {{"setMissionAvailable", {entry.mission, 1}}};
    if (entry.adds_quest)
      calls.push_back({"addQuest", {2000, 2100}});
    if (entry.store_calls) {
      calls.push_back({"changeSwoop", {0}});
      calls.push_back({"activateStore", {}});
    }
    auto image = build_calls({{entry.name, calls}}, {{2000, "auth-hub-title"}, {2100, "auth-hub-body"}});
    sc::Host host;
    plant(host, image);
    host.start_single_player();
    host.require_complete_commands();
    host.set_entry_gate(route::gate_for(entry.level));
    plant_missions(host);
    const auto stop = host.run(entry.name);
    const std::string label = std::string(entry.level) + " " + entry.name;
    check(!host.has_entry_gate(), "no gate on " + label);
    check(byte(host, 0, entry.mission) == 0xA1 && byte(host, 1, entry.mission) == SENTINEL,
          label + " sets its mission on record 0 only");
    for (int other = 3; other <= 6; ++other)
      if (other != entry.mission)
        check(byte(host, 0, other) == SENTINEL, label + " leaves the other missions");
    check(host.campaign().record(0).quest_count == (entry.adds_quest ? 1 : 0), label + " quest count");
    if (entry.store_calls)
      check(stop == sc::Stop::Refused && host.last_refusal() == "refused activateStore",
            label + " stops at the unported shop call");
    else
      check(stop == sc::Stop::Complete, label + " completes");
  }
}

void test_entry_basis() {
  // 71 entries were executed and 4 (the islc1 racer wrappers) were a static
  // parse, not a run (docs/story-route.md:442-450). failQuest, endMission and
  // the two steppedOnTrigger entries ran only inside their callers
  // (:454-463, :476-485, :489-494, :623-634).
  std::set<std::string> parsed;
  int executed = 0;
  for (const auto &entry : route::observed_entries()) {
    if (std::string(entry.evidence).find("static parse") != std::string::npos)
      parsed.insert(entry.name);
    else
      ++executed;
  }
  check(executed == 71 && parsed == std::set<std::string>{"postActifb1b", "postActi6592", "postActi928c",
                                                          "postActifd9a"},
        "71 executed and 4 statically parsed entries");
}

void test_hub_research_table() {
  // A literal table from the research, independent of the list's own flags.
  // store: 1 research names a changeSwoop(0)/activateStore pair, 0 research says
  // the entry has neither (:868-876), -1 research does not say (section 7).
  struct Row {
    const char *level, *name;
    int mission;
    bool quest;
    int store;
  };
  const Row research[] = {
      // Mission 4, docs/story-route.md:504-510: e8e9 and c4 add a quest; store calls not mentioned.
      {"hub", "setMissi5f9f", 4, false, -1},
      {"hub", "setMissidce3", 4, false, -1},
      {"hub", "setMissie8e9", 4, true, -1},
      {"hub", "setMissic4", 4, true, -1},
      // Mission 5, :861-863, no quest; store pairs on bbbb and 9389, :868-871.
      {"hub", "setMissif825", 5, false, 0},
      {"hub", "setMissibbbb", 5, false, 1},
      {"hub", "setMissi9389", 5, false, 1},
      {"hub2", "setMissif825", 5, false, 0},
      {"hub2", "setMissibbbb", 5, false, 1},
      {"hub2", "setMissi9389", 5, false, 1},
      // Mission 6 on hub, :863-867 (Q-hub on caba and 6d46); stores :868-869.
      {"hub", "setMissi95d5", 6, false, 0},
      {"hub", "setMissi4082", 6, false, 1},
      {"hub", "setMissicaba", 6, true, 0},
      {"hub", "setMissi6d46", 6, true, 1},
      // Mission 6 on hub2, :867-868, no quest; stores :870-871.
      {"hub2", "setMissi95d5", 6, false, 0},
      {"hub2", "setMissi4082", 6, false, 1},
      {"hub2", "setMissic3ab", 6, false, 1},
  };
  const auto &list = route::availability_entries();
  check(list.size() == std::size(research), "the hub list has the research's seventeen entries");
  for (const auto &want : research) {
    const std::string label = std::string(want.level) + " " + want.name;
    const auto found = std::find_if(list.begin(), list.end(), [&](const route::Availability &entry) {
      return std::string(entry.level) == want.level && std::string(entry.name) == want.name;
    });
    check(found != list.end(), label + " is listed");
    if (found == list.end())
      continue;
    check(found->mission == want.mission, label + " mission matches research");
    check(found->adds_quest == want.quest, label + " quest matches research");
    check(found->store_known == (want.store >= 0), label + " store basis matches research");
    check(want.store < 0 ? !found->store_calls : found->store_calls == (want.store == 1),
          label + " store calls match research, and Unknown claims none");
    check(!found->client_store_stop || want.store < 0, label + " disc observation only where research is silent");
  }
}

void test_medal_bonus_refused() {
  // Medal and bonus columns were not re-run (docs/story-route.md:268);
  // watmedal and tormedal were not run (:562-563).
  auto rows = authored_rows();
  for (int row = 0; row < 12; ++row) {
    rows[std::size_t(row)].medal = story(row + 1) + "m";
    rows[std::size_t(row)].bonus = story(row + 1) + "b";
  }
  for (int mission = 1; mission <= 12; ++mission) {
    const auto label = " of mission " + std::to_string(mission);
    const auto medal = route::table_match(rows, story(mission) + "m_sub");
    const auto bonus = route::table_match(rows, story(mission) + "b");
    check(medal.mission == mission && medal.column == route::Column::Medal, "medal name maps" + label);
    check(bonus.mission == mission && bonus.column == route::Column::Bonus, "bonus name maps" + label);
    const auto medal_refusal = route::load_refusal(rows, story(mission) + "m");
    const auto bonus_refusal = route::load_refusal(rows, story(mission) + "b");
    if (mission >= 3)
      check(medal_refusal && bonus_refusal && medal_refusal->find(":268") != std::string::npos &&
                medal_refusal->find("medal") != std::string::npos && bonus_refusal->find("bonus") != std::string::npos,
            "medal and bonus loads refuse" + label);
    else
      check(!medal_refusal && !bonus_refusal, "medal and bonus keep their policy" + label);
    const auto story_refusal = route::load_refusal(rows, story(mission));
    check(route::table_match(rows, story(mission)).column == route::Column::Story &&
              bool(story_refusal) == (mission >= 6),
          "story load refuses only for missions 6-12" + label);
  }
  check(route::table_match(rows, evil(10)).column == route::Column::Evil && route::load_refusal(rows, evil(10)),
        "an evil name maps to the story column and refuses");
  check(route::table_match(rows, "auth-other").mission == 0 && !route::load_refusal(rows, "auth-other"),
        "a name outside the table is not refused here");
}

void test_kept_refusals() {
  check(refuses(route::refuse_menu_draw) && refuses(route::refuse_input_restorer) &&
            refuses(route::refuse_loader_body) && refuses(route::refuse_arrival),
        "menu draw, input restorer, loader body and arrival stay refused");
}
} // namespace

int main() {
  test_menu_table();
  test_story_request();
  test_menu_cursor();
  test_menu_confirm();
  test_in_level();
  test_entry_lists();
  test_gate_on_host();
  test_nested_entries();
  test_object_use_refused();
  test_observed_effects();
  test_offset_zero_travel();
  test_item_count_condition();
  test_hub_availability();
  test_entry_basis();
  test_hub_research_table();
  test_medal_bonus_refused();
  test_kept_refusals();
  if (failed) {
    std::cerr << failed << " of " << checks << " checks failed\n";
    return 1;
  }
  std::cout << "story-route " << checks << " checks\n";
  return 0;
}
