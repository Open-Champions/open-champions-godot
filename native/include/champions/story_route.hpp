#pragma once
// Story route after the first menu request: the twelve-row mission menu, the
// act each row's level is in, and the executed entries of missions 3, 4 and 5.
//
// Research pin: t3/research-story-route-r3 3c7987c, docs/story-route.md
// sections 6-8 (audits research-story-route-r4.md, research-story-route-r5r6.md
// and research-story-route-r6-fix.md, PASS). Mission 1's completion writers
// are research-mission-unlocks 7b924e1, docs/spec/mission-unlocks.md:279-523,
// limited to the claims research-mission1-complete-fix2.md closed.
//
// Level and script-entry identifiers are the research's identifiers. Mission
// table names are read from the owner's executable at run time
// (mission_menu_table.hpp); quest and help text keys are never carried here.
#include "champions/quests.hpp"
#include "champions/script_commands.hpp"
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace champions::simulation::rta::story_route {
struct Refused : std::runtime_error {
  using std::runtime_error::runtime_error;
};

// missionMenu_run reads missions 1..12; mission 0 is not a row.
// docs/story-route.md:228-236. tools/ref_story_route.py:80-91.
inline constexpr int MENU_MISSIONS = 12;
// The only three rows with an evil pointer: missions 6, 10 and 12.
// docs/story-route.md:237-262 (SR4-1 asserted the twelve tuples).
inline constexpr int EVIL_MISSIONS[] = {6, 10, 12};
// State 8 stores orientation 0xff. docs/story-route.md:263.
inline constexpr int REQUEST_ORIENT = 0xFF;

struct MenuEntry {
  int mission = 0;
  int row = 0;
  std::string story;
  std::string evil; // empty unless the row has an evil pointer
  int act = -1;     // save_findAct from current act -1
};

// The twelve story rows and their acts. Refuses a table that differs from the
// executed one: fewer than twelve rows, an empty story pointer, an evil
// pointer outside missions 6, 10 and 12, an evil name in another act than its
// story name, or a story act other than the row index (acts 0..11 in order).
// docs/story-route.md:237-268. The names are the caller's, read at run time.
std::vector<MenuEntry> menu_entries(const std::vector<quests::MissionRow> &rows,
                                    const std::vector<std::vector<std::string>> &acts);

// What column 0 of state 8 would request for a menu row. Row 0 is accepted
// even when mission 1's available bit is clear (0x231078(0) returns 1); later
// rows only when that mission is available, otherwise this refuses. The evil
// name is used when game_getEvil is set and the row has one.
// docs/story-route.md:237-268; tools/ref_story_route.py:94-98,112-117.
struct Request {
  std::string level;
  int act = -1;
  int orientation = REQUEST_ORIENT;
  // False for row 0 with mission 1's bit clear: not a hub offer
  // (docs/story-route.md:338-340).
  bool offered = true;
};
Request story_request(const quests::Campaign &campaign, int row,
                      const std::vector<std::vector<std::string>> &acts);

// Which research covers a mission's in-level beats once its table level is
// requested (docs/story-route.md:313-326 and :348-356).
enum class InLevel : std::uint8_t {
  Mission1Notes,  // research-mission-unlocks 7b924e1 and research-strife-drundar
  Mission2Route,  // research-mission2-route b8e2d11 (mission2_route.hpp)
  StoryEntries,   // sections 7-8 here
  Refused,        // missions 6-12
};
InLevel in_level(int mission);
// Missions 6-12: the menu may request the table level; the client then
// refuses the level. Returns the refusal reason, or nullopt for missions 1-5.
std::optional<std::string> in_level_refusal(int mission);
// The mission whose story or evil table name equals the requested base name
// (before any '_' subname), or 0.
int table_mission(const std::vector<quests::MissionRow> &rows, const std::string &requested);

// Executed entries of one mission 3-5 level.
enum class Kind : std::uint8_t {
  PreStart,   // writes the entrance
  Start,      // availability, setv, and the level's post or quest
  Intro,      // posted cut request; cutScene stays refused by the host
  Condition,  // dialog or key predicate, executed for its 0/1 return
  Action,     // dialog action: variable store and one quest
  Wrapper,    // static parse: its only call is postAction/callScript of a listed entry
  Travel,     // newWorld, with or without an entrance
  Check,      // shardCheck: 25-call count, then the item count
  Completion, // a grantMission writer
};
struct Entry {
  const char *level;
  const char *name;
  int mission;
  Kind kind;
  const char *evidence;
};
// Entries of missions 3-5 levels that research ran (or, for Wrapper, parsed)
// and the client may start. Section 7: docs/story-route.md:389-510;
// section 8: :597-850.
const std::vector<Entry> &observed_entries();
// Entries research names in these levels and that stay refused, with reasons.
struct RefusedEntry {
  const char *level;
  const char *name;
  const char *reason;
};
const std::vector<RefusedEntry> &refused_entries();
// Story levels of missions 3-5 named by sections 7-8: the levels whose
// entries are listed, and the travel destinations no entry was run in.
int story_level_mission(const std::string &level);
bool story_level(const std::string &level);

// Hub and hub2 entries that set the next mission's available bit
// (docs/story-route.md:504-510 and :852-880). The hub scripts keep their
// existing host policy; this list is the contract the tests replay.
struct Availability {
  const char *level;
  const char *name;
  int mission;      // the bit the entry sets on record 0
  bool adds_quest;  // one addQuest
  bool store_calls; // changeSwoop(0) then activateStore
};
const std::vector<Availability> &availability_entries();

// Why an entry may not start in a mission 3-5 level, or nullopt.
// object_use: started from an object record (physical use/trigger dispatch),
// which research did not run in these levels (docs/story-route.md:549-552,
// :945-947).
std::optional<std::string> entry_refusal(const std::string &level, const std::string &entry,
                                         bool object_use);
// Dialog selection in these levels: the selector was not run, and the node
// reachability is Inferred at best (docs/story-route.md:412-421, :442-450,
// :465-475, :782-802).
std::string dialog_refusal(const std::string &level);
// The host gate for a mission 3-5 story level, or nullopt for other levels.
std::optional<script_commands::EntryGate> gate_for(const std::string &level);

// Kept refused (docs/story-route.md:289-301, :322-326, :549-552, :945-953).
[[noreturn]] void refuse_menu_draw();
[[noreturn]] void refuse_input_restorer();
[[noreturn]] void refuse_loader_body();
[[noreturn]] void refuse_arrival();
} // namespace champions::simulation::rta::story_route
