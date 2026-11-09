#include "champions/story_route.hpp"

#include <algorithm>
#include <cctype>

namespace champions::simulation::rta::story_route {
namespace {
std::string lower(std::string text) {
  for (auto &c : text)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return text;
}
bool evil_mission(int mission) {
  return std::find(std::begin(EVIL_MISSIONS), std::end(EVIL_MISSIONS), mission) != std::end(EVIL_MISSIONS);
}

// docs/story-route.md section 7 (mission 3) and section 8 (missions 4 and 5).
// Evidence strings name the research lines each entry rests on.
const char *const S7_START = "docs/story-route.md:391-398";
const char *const S7_ABSORB = "docs/story-route.md:400-408";
const char *const S7_PRED = "docs/story-route.md:417-422";
const char *const S7_MEET = "docs/story-route.md:424-428";
const char *const S7_TRAVEL = "docs/story-route.md:430-440";
const char *const S7_RACER = "docs/story-route.md:442-450 (static parse)";
const char *const S7_GNOME = "docs/story-route.md:454-463";
const char *const S7_BLESS = "docs/story-route.md:465-474";
const char *const S7_DONE = "docs/story-route.md:476-485";
const char *const S7_CAVE = "docs/story-route.md:489-494";
const char *const S8_WSTART = "docs/story-route.md:599-611";
const char *const S8_WINTRO = "docs/story-route.md:613-621";
const char *const S8_WATER1 = "docs/story-route.md:623-634";
const char *const S8_KEYS = "docs/story-route.md:638-662";
const char *const S8_BACK = "docs/story-route.md:664-670";
const char *const S8_SHARD = "docs/story-route.md:672-726";
const char *const S8_IFF46 = "docs/story-route.md:745-748";
const char *const S8_TSTART = "docs/story-route.md:752-760";
const char *const S8_TPRE = "docs/story-route.md:762-767";
const char *const S8_TCUT = "docs/story-route.md:773-780";
const char *const S8_GOL = "docs/story-route.md:789-796";
const char *const S8_GOLQ = "docs/story-route.md:804-807";
const char *const S8_ENDTOR = "docs/story-route.md:811-830";
const char *const S8_TTRAVEL = "docs/story-route.md:832-848";

std::vector<Entry> build_entries() {
  std::vector<Entry> out = {
      {"islc1", "start", 3, Kind::Start, S7_START},
      {"islc1", "absorb", 3, Kind::Intro, S7_ABSORB},
      {"islc1", "if4257", 3, Kind::Condition, S7_PRED},
      {"islc1", "ifa305", 3, Kind::Condition, S7_PRED},
      {"islc1", "setvdea6", 3, Kind::Action, S7_MEET},
      {"islc1", "lostHope", 3, Kind::Travel, S7_TRAVEL},
      {"islc1", "solitary", 3, Kind::Travel, S7_TRAVEL},
      {"islc1", "regret", 3, Kind::Travel, S7_TRAVEL},
      {"islc1", "gnome", 3, Kind::Travel, S7_TRAVEL},
      {"islc1", "kerriel", 3, Kind::Travel, S7_TRAVEL},
      {"islc1", "postActifb1b", 3, Kind::Wrapper, S7_RACER},
      {"islc1", "postActi6592", 3, Kind::Wrapper, S7_RACER},
      {"islc1", "postActi928c", 3, Kind::Wrapper, S7_RACER},
      {"islc1", "postActifd9a", 3, Kind::Wrapper, S7_RACER},
      {"islcave1", "start", 3, Kind::Start, S7_START},
      {"islcave1", "callScri7351", 3, Kind::Wrapper, S7_CAVE},
      {"islcave1", "steppedOnTrigger", 3, Kind::Completion, S7_CAVE},
      {"islgnome", "entrance1", 3, Kind::Travel, S7_GNOME},
      {"islgnome", "entrance2", 3, Kind::Travel, S7_GNOME},
      {"islgnome", "entrance3", 3, Kind::Travel, S7_GNOME},
      // The eight killMonster calls run in this callee when startedQuest is 1
      // and finishedQuest is absent or 0 (docs/story-route.md:454-463). The
      // client does not port killMonster (Partial, not routed), so with the
      // gnome quest started and not finished the entrance stops at the first
      // call, before islc2 is requested: a disclosed mission 3 play gap.
      {"islgnome", "failQuest", 3, Kind::Action, S7_GNOME},
      {"islc2", "iffbe4", 3, Kind::Condition, S7_BLESS},
      {"islc2", "if9be1", 3, Kind::Condition, S7_BLESS},
      {"islc2", "iffa37", 3, Kind::Condition, S7_BLESS},
      {"islc2", "callScri8a40", 3, Kind::Wrapper, S7_DONE},
      {"islc2", "endMission", 3, Kind::Completion, S7_DONE},
      {"wat1", "start", 4, Kind::Start, S8_WSTART},
      {"wat1", "absorb", 4, Kind::Intro, S8_WINTRO},
      {"wat1", "waterIntro", 4, Kind::Intro, S8_WINTRO},
      {"wat1", "return10c8", 4, Kind::Condition, S8_KEYS},
      {"wat1", "return6198", 4, Kind::Condition, S8_KEYS},
      {"wat1", "newWorld57c0", 4, Kind::Travel, S8_KEYS},
      {"wat1", "newWorld9711", 4, Kind::Travel, S8_KEYS},
      {"wat1", "newWorld7517", 4, Kind::Travel, S8_KEYS},
      {"wat1", "newWorldced", 4, Kind::Travel, S8_KEYS},
      {"wat1", "newWorldf0d", 4, Kind::Travel, S8_KEYS},
      {"wat1", "newWorldd7df", 4, Kind::Travel, S8_KEYS},
      {"water1", "start", 4, Kind::Start, S8_WATER1},
      {"water1", "callScri7351", 4, Kind::Wrapper, S8_WATER1},
      {"water1", "steppedOnTrigger", 4, Kind::Completion, S8_WATER1},
      {"wate2a", "iff46", 4, Kind::Condition, S8_IFF46},
      {"tormenta", "preStart", 5, Kind::PreStart, S8_TPRE},
      {"tormenta", "start", 5, Kind::Start, S8_TSTART},
      {"tormenta", "absorb", 5, Kind::Intro, S8_TSTART},
      {"tormenta", "if87a0", 5, Kind::Intro, S8_TCUT},
      {"tormenta", "if3f82", 5, Kind::Intro, S8_TCUT},
      {"tormenta", "if1b74", 5, Kind::Condition, S8_GOL},
      {"tormenta", "ifb0b8", 5, Kind::Condition, S8_GOL},
      {"tormenta", "ifa33a", 5, Kind::Condition, S8_GOL},
      {"tormenta", "if5184", 5, Kind::Condition, S8_GOL},
      {"tormenta", "if799d", 5, Kind::Condition, S8_GOL},
      {"tormenta", "if4fec", 5, Kind::Condition, S8_GOL},
      {"tormenta", "setv2725", 5, Kind::Action, S8_GOLQ},
      {"tormenta", "callScri8c36", 5, Kind::Completion, S8_ENDTOR},
      {"tormenta", "endtor", 5, Kind::Completion, S8_ENDTOR},
      {"tormenta", "newWorld3aa4", 5, Kind::Travel, S8_TTRAVEL},
      {"tormentb", "newWorld1223", 5, Kind::Travel, S8_TTRAVEL},
      {"tormentb", "newWorld9066", 5, Kind::Travel, S8_TTRAVEL},
      {"torment2", "newWorlda629", 5, Kind::Travel, S8_TTRAVEL},
      {"torment2", "newWorldf608", 5, Kind::Travel, S8_TTRAVEL},
      {"torment2", "newWorld181f", 5, Kind::Travel, S8_TTRAVEL},
      {"torment3", "newWorld6aa", 5, Kind::Travel, S8_TTRAVEL},
      {"torment3", "newWorldbc33", 5, Kind::Travel, S8_TTRAVEL},
  };
  // newWorlde6b4 and shardCheck run on all six side levels.
  for (const char *level : {"wate2a", "wate2b", "wate2c", "watg2a", "watg2b", "watg2c"}) {
    out.push_back({level, "newWorlde6b4", 4, Kind::Travel, S8_BACK});
    out.push_back({level, "shardCheck", 4, Kind::Check, S8_SHARD});
  }
  return out;
}

std::vector<RefusedEntry> build_refused() {
  std::vector<RefusedEntry> out;
  for (const char *level : {"wate2a", "wate2b", "wate2c"})
    out.push_back({level, "brokeFinalWall",
                   "faults at pc 0x146ecc inside addParticleOnObject before its quest removal; the "
                   "statements after the fault are Unknown (docs/story-route.md:728-736)"});
  for (const char *level : {"watg2a", "watg2b", "watg2c"})
    out.push_back({level, "holeCheck",
                   "returns without its completion stores with no plug object; the distance that "
                   "reaches them is Unknown (docs/story-route.md:738-743)"});
  for (const char *level : {"wate2a", "wate2b", "wate2c", "watg2a", "watg2b", "watg2c"})
    out.push_back({level, "start",
                   "start was not run; what it stores into the compared shardCheck variable in play "
                   "is Unknown (docs/story-route.md:676-681, :723-725, :945-948)"});
  out.push_back({"tormenta", "startDia7a53",
                 "an object dialogscript; which use reaches it was not run (docs/story-route.md:769-771)"});
  return out;
}

struct LevelMission {
  const char *level;
  int mission;
};
// Section 7 and 8 levels: those with listed entries, and the travel
// destinations (islsm1, islsm2a, islsm3a) no entry was run in.
const LevelMission STORY_LEVELS[] = {
    {"islc1", 3},    {"islcave1", 3}, {"islgnome", 3}, {"islc2", 3},    {"islsm1", 3},
    {"islsm2a", 3},  {"islsm3a", 3},  {"wat1", 4},     {"water1", 4},   {"wate2a", 4},
    {"wate2b", 4},   {"wate2c", 4},   {"watg2a", 4},   {"watg2b", 4},   {"watg2c", 4},
    {"tormenta", 5}, {"tormentb", 5}, {"torment2", 5}, {"torment3", 5},
};
} // namespace

std::vector<MenuEntry> menu_entries(const std::vector<quests::MissionRow> &rows,
                                    const std::vector<std::vector<std::string>> &acts) {
  if (rows.size() < static_cast<std::size_t>(MENU_MISSIONS))
    throw Refused("the mission table has fewer than the twelve executed menu rows");
  std::vector<MenuEntry> out;
  for (int row = 0; row < MENU_MISSIONS; ++row) {
    const auto &info = rows[static_cast<std::size_t>(row)];
    MenuEntry entry;
    entry.mission = row + 1;
    entry.row = row;
    entry.story = info.story;
    entry.evil = info.evil;
    if (entry.story.empty())
      throw Refused("a menu row has no story level; the executed table has twelve");
    if (!entry.evil.empty() && !evil_mission(entry.mission))
      throw Refused("an evil pointer outside missions 6, 10 and 12 is not the executed table");
    if (entry.evil.empty() && evil_mission(entry.mission))
      throw Refused("missions 6, 10 and 12 have an evil level in the executed table");
    entry.act = quests::find_act(entry.story, -1, acts);
    if (entry.act != row)
      throw Refused("the story level's act differs from the executed acts 0..11 in row order");
    if (!entry.evil.empty() && quests::find_act(entry.evil, -1, acts) != entry.act)
      throw Refused("the evil level is not in its story level's act");
    out.push_back(std::move(entry));
  }
  return out;
}

Request story_request(const quests::Campaign &campaign, int row,
                      const std::vector<std::vector<std::string>> &acts) {
  if (row < 0 || row >= MENU_MISSIONS)
    throw Refused("menu row is outside missions 1..12");
  const auto entries = menu_entries(campaign.rows(), acts);
  const auto &entry = entries[static_cast<std::size_t>(row)];
  const bool available = campaign.mission_available(entry.mission) != 0;
  if (row != 0 && !available)
    throw Refused("the story column refuses a row whose mission is not available");
  Request out;
  out.level = campaign.evil() != 0 && !entry.evil.empty() ? entry.evil : entry.story;
  out.act = entry.act;
  out.offered = available;
  return out;
}

InLevel in_level(int mission) {
  if (mission == 1)
    return InLevel::Mission1Notes;
  if (mission == 2)
    return InLevel::Mission2Route;
  if (mission >= 3 && mission <= 5)
    return InLevel::StoryEntries;
  if (mission >= 6 && mission <= MENU_MISSIONS)
    return InLevel::Refused;
  throw Refused("mission is outside the menu's missions 1..12");
}

std::optional<std::string> in_level_refusal(int mission) {
  if (in_level(mission) != InLevel::Refused)
    return std::nullopt;
  return "mission " + std::to_string(mission) +
         " in-level beats are not established; the table level was requested and is refused "
         "(docs/story-route.md:322-326, :349-354)";
}

int table_mission(const std::vector<quests::MissionRow> &rows, const std::string &requested) {
  const auto base = requested.substr(0, requested.find('_'));
  for (int row = 0; row < MENU_MISSIONS && static_cast<std::size_t>(row) < rows.size(); ++row) {
    const auto &info = rows[static_cast<std::size_t>(row)];
    if ((!info.story.empty() && lower(info.story) == lower(base)) ||
        (!info.evil.empty() && lower(info.evil) == lower(base)))
      return row + 1;
  }
  return 0;
}

TableMatch table_match(const std::vector<quests::MissionRow> &rows, const std::string &requested) {
  const auto base = lower(requested.substr(0, requested.find('_')));
  const auto is = [&](const std::string &name) { return !name.empty() && lower(name) == base; };
  // Story and evil first, as table_mission matches them.
  if (const int mission = table_mission(rows, requested)) {
    const auto &info = rows[static_cast<std::size_t>(mission - 1)];
    return {mission, is(info.story) ? Column::Story : Column::Evil};
  }
  for (int row = 0; row < MENU_MISSIONS && static_cast<std::size_t>(row) < rows.size(); ++row) {
    const auto &info = rows[static_cast<std::size_t>(row)];
    if (is(info.medal))
      return {row + 1, Column::Medal};
    if (is(info.bonus))
      return {row + 1, Column::Bonus};
  }
  return {};
}

std::optional<std::string> load_refusal(const std::vector<quests::MissionRow> &rows,
                                        const std::string &requested) {
  const auto match = table_match(rows, requested);
  if (match.mission < 1)
    return std::nullopt;
  if (match.column == Column::Story || match.column == Column::Evil)
    return in_level_refusal(match.mission);
  if (match.mission < 3)
    return std::nullopt;
  return "mission " + std::to_string(match.mission) + " " +
         (match.column == Column::Medal ? "medal" : "bonus") +
         " level was not run; the medal and bonus columns are not re-run (docs/story-route.md:268, "
         ":562-563)";
}

const std::vector<Entry> &observed_entries() {
  static const std::vector<Entry> entries = build_entries();
  return entries;
}
const std::vector<RefusedEntry> &refused_entries() {
  static const std::vector<RefusedEntry> entries = build_refused();
  return entries;
}

int story_level_mission(const std::string &level) {
  const auto name = lower(level.substr(0, level.find('_')));
  for (const auto &row : STORY_LEVELS)
    if (name == row.level)
      return row.mission;
  return 0;
}
bool story_level(const std::string &level) { return story_level_mission(level) != 0; }

const std::vector<Availability> &availability_entries() {
  // Mission 4: docs/story-route.md:504-510. Missions 5 and 6:
  // docs/story-route.md:861-878 (store calls :868-876).
  static const std::vector<Availability> entries = {
      // Mission 4: store calls Unknown in research; two stop at the client's
      // refused activateStore on the disc (client observation).
      {"hub", "setMissi5f9f", 4, false, false, false, false},
      {"hub", "setMissidce3", 4, false, false, false, true},
      {"hub", "setMissie8e9", 4, true, false, false, false},
      {"hub", "setMissic4", 4, true, false, false, true},
      {"hub", "setMissif825", 5, false, false, true, false}, {"hub", "setMissibbbb", 5, false, true, true, false},
      {"hub", "setMissi9389", 5, false, true, true, false},  {"hub2", "setMissif825", 5, false, false, true, false},
      {"hub2", "setMissibbbb", 5, false, true, true, false}, {"hub2", "setMissi9389", 5, false, true, true, false},
      {"hub", "setMissi95d5", 6, false, false, true, false}, {"hub", "setMissi4082", 6, false, true, true, false},
      {"hub", "setMissicaba", 6, true, false, true, false},  {"hub", "setMissi6d46", 6, true, true, true, false},
      {"hub2", "setMissi95d5", 6, false, false, true, false}, {"hub2", "setMissi4082", 6, false, true, true, false},
      {"hub2", "setMissic3ab", 6, false, true, true, false},
  };
  return entries;
}

std::optional<std::string> entry_refusal(const std::string &level, const std::string &entry,
                                         bool object_use) {
  const int mission = story_level_mission(level);
  if (!mission)
    return std::nullopt;
  const auto name = lower(level.substr(0, level.find('_')));
  if (object_use)
    return "object use or trigger dispatch of " + entry + " was not run in mission " +
           std::to_string(mission) + " levels (docs/story-route.md:549-552, :945-947)";
  for (const auto &row : refused_entries())
    if (name == row.level && entry == row.name)
      return entry + ": " + row.reason;
  for (const auto &row : observed_entries())
    if (name == row.level && entry == row.name)
      return std::nullopt;
  return entry + " is outside the executed mission " + std::to_string(mission) +
         " entries (docs/story-route.md " + (mission == 3 ? "section 7" : "section 8") + ")";
}

std::string dialog_refusal(const std::string &level) {
  const int mission = story_level_mission(level);
  if (!mission)
    return {};
  return "dialog selection in mission " + std::to_string(mission) +
         " levels was not run; node reachability is Inferred at best (docs/story-route.md:412-421, "
         ":442-450, :465-474, :782-802)";
}

std::optional<script_commands::EntryGate> gate_for(const std::string &level) {
  if (!story_level(level))
    return std::nullopt;
  script_commands::EntryGate gate;
  gate.entry = [level](const std::string &entry, bool object_use) {
    return entry_refusal(level, entry, object_use);
  };
  gate.dialog_refusal = dialog_refusal(level);
  return gate;
}

void refuse_menu_draw() {
  throw Refused("missionMenu draw 0x232300 faults on an 8-byte store through the display-list cursor "
                "at 0x44fe10 (pc 0x17b968) with its stubs installed (docs/story-route.md:289-301)");
}
void refuse_input_restorer() {
  throw Refused("who sets the input-ready word 0x8e2f10 back to 1.0 during play is Unknown "
                "(docs/story-route.md:289-301)");
}
void refuse_loader_body() {
  throw Refused("the loader body after a story request was not run (docs/story-route.md:322-326, "
                ":549-552, :945-947)");
}
void refuse_arrival() {
  throw Refused("arrival after a story request is Unknown (docs/story-route.md:322-326, :549-552, "
                ":945-947)");
}
} // namespace champions::simulation::rta::story_route
