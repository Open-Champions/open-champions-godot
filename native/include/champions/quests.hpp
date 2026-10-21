#pragma once
// Return to Arms campaign state. Quest-list, mission, medal, evil, decision,
// script-flag and gold rules are the ones already executed in ref_natives.
// The menu, hub name, act prefix, experience body, quest-list clear and the
// flag-39 keep list are ref_quests.py at research t3/research-quests 7efd3ce.
// The experience thresholds are caller-supplied: the reference does not carry
// the 81 words at 0x41b558. A negative mission index is not written here; that
// store lands before the mission row, and this model does not invent the
// neighbouring field.
#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace champions::simulation::rta::quests {
inline constexpr const char *POLICY = "rta-quests-v1";
inline constexpr int PLAYERS = 4;
inline constexpr int QUEST_SLOTS = 8;
inline constexpr int QUEST_FIELD = 32;
inline constexpr int QUEST_STRIDE = 0x40;
inline constexpr int MISSIONS = 13;
inline constexpr int DIFFICULTIES = 5;
// The mission rows are difficulties 1..5: +0x4bbf + d * 13 + m, that is
// +0x4bcc..+0x4c0c, and the medal bytes start at +0x4c0d. Row 0 would lie on
// the skill pairs and the countdown bytes. player_initNewCharacter marks rows
// 1..5, and g_difficultyIdx is 1 in the executable image (research-items
// 49bc578 docs/script-items.md section 3).
inline constexpr int FIRST_DIFFICULTY = 1;
inline constexpr int LAST_DIFFICULTY = FIRST_DIFFICULTY + DIFFICULTIES - 1;
inline constexpr int IMAGE_DIFFICULTY = FIRST_DIFFICULTY;
inline constexpr int BAGS = 3;
inline constexpr int XP_LEVELS = 81;
inline constexpr int MENU_ROWS = 12;
inline constexpr int MAX_GOLD = 99999999;
inline constexpr int QUEST_ITEM_BIT = 39;
inline constexpr char QUEST_PLACEHOLDER[] = "NoQuests";
inline constexpr char HUB_GOOD[] = "hub";
inline constexpr char HUB_EVIL[] = "hub2";

inline constexpr int BTN_TRIANGLE = 0x10;
inline constexpr int BTN_CIRCLE = 0x20;
inline constexpr int BTN_CROSS = 0x40;
inline constexpr int BTN_SQUARE = 0x80;
inline constexpr int BTN_SELECT = 0x100;
inline constexpr int BTN_START = 0x800;
inline constexpr int BTN_UP = 0x1000;
inline constexpr int BTN_RIGHT = 0x2000;
inline constexpr int BTN_DOWN = 0x4000;
inline constexpr int BTN_LEFT = 0x8000;
inline constexpr int LAUNCH_ORIENT = 0xFF;

struct Error : std::runtime_error {
  using std::runtime_error::runtime_error;
};

using Field = std::array<std::uint8_t, QUEST_FIELD>;
struct QuestLine {
  Field title{};
  Field text{};
};
struct Item {
  std::string name;
  std::uint64_t flags = 0;
};
struct Record {
  std::array<QuestLine, QUEST_SLOTS> quests{};
  std::int32_t quest_count = 0;
  // missions[mission_index(d, m)]: difficulty d 1..5, record +0x4bcc onward.
  std::array<std::uint8_t, DIFFICULTIES * MISSIONS> missions{};
  std::array<std::uint8_t, MISSIONS> medals{};
  std::uint8_t evil = 0;
  std::uint8_t decision = 0;
  std::uint8_t script_flags = 0;
  std::int32_t level = 0;
  std::int32_t exp = 0;
  std::int32_t skill = 0;
  std::uint8_t stat = 0;
  float hp = 0;
  float max_hp = 0;
  float mana = 0;
  float max_mana = 0;
  std::int32_t gold = 0;
  std::array<std::vector<Item>, BAGS> bags{};
};
struct MissionRow {
  std::string story, evil, medal, bonus;
  std::int32_t flag20 = 0;
  std::int32_t flag24 = 0;
  std::int32_t flag28 = 0;
};
struct Menu {
  int state = 0;
  int row = 0;
  int col = 0;
  int counter = 0;
  int mark = 0;
  int c0 = 0;
  int paused = 0;
  std::string pending;
  int orient = 0;
};
struct MenuWorld {
  std::vector<MissionRow> rows;
  std::vector<int> available;
  std::vector<int> complete;
  std::map<int, int> medals;
  bool evil = false;
  bool local = false;
  bool pause = false;
  bool slot_active = true;
  int fade = 0;
  bool mission_available(int mission) const;
  bool mission_complete(int mission) const;
  int mission_medal(int mission) const;
};
// The calls missionMenu_run makes that this module does not own: whether the
// chooser is local with the pause menu open, net_isSlotActive(chooser), and
// hud_stepFade(4). The executed menu cases ran offline with these defaults.
struct MenuInputs {
  bool local = false;
  bool pause = false;
  bool slot_active = true;
  int fade = 0;
};
struct Var {
  std::string name;
  std::int8_t value = 0;
};
struct ExpResult {
  int level = 0;
  int exp = 0;
  int skill = 0;
  int stat = 0;
  float hp = 0;
  float mana = 0;
  int recalcs = 0;
};
struct Events {
  std::uint64_t menu_opens = 0;
  std::uint64_t cflag_bit0 = 0;
  std::uint64_t cflag_bit26 = 0;
  std::uint64_t cflag_other = 0;
  std::uint64_t boss_music = 0;
  std::uint64_t act_resets = 0;
  std::uint64_t flag39_removed = 0;
  std::uint64_t exp_grants = 0;
  std::uint64_t menu_launches = 0;
};

// The index of difficulty d's mission m in Record::missions. A difficulty
// outside 1..5 or a mission outside 0..12 is refused: the campaign does not
// hold row 0 or the bytes after row 5.
int mission_index(int difficulty, int mission);

std::string field_text(const Field &);
Field field_from(const std::string &);
void overlay_placeholder(Field &);

int skill_points_for(int level);
ExpResult give_exp(int level, int exp, int skill, int stat, float hp, float max_hp,
                   float mana, float max_mana, int amount, const std::int32_t *table,
                   int count);
std::string hub_name(int evil, int decision, int mission1, int difficulty);
int find_act(const std::string &name, int current,
             const std::vector<std::vector<std::string>> &acts);
bool quest_item_kept(const std::string &name);
std::vector<Item> flag39_remaining(const std::vector<Item> &);
std::pair<std::array<QuestLine, QUEST_SLOTS>, int>
clear_quest_list(const std::array<QuestLine, QUEST_SLOTS> &, int count);

Menu new_menu();
int initial_row(const std::vector<int> &available);
int helper_value(int index, const MissionRow &, bool available, bool complete, int medal);
void menu_tick(Menu &, int button, const MenuWorld &);

// One campaign. Four records. The act tables and the mission rows are supplied
// by the caller; this module does not carry the retail tables.
class Campaign {
public:
  void set_num_players(int);
  int num_players() const { return num_players_; }
  void set_difficulty(int difficulty) { difficulty_ = difficulty; }
  int difficulty() const { return difficulty_; }
  void set_act(int act) { act_ = act; }
  int act() const { return act_; }
  void set_acts(std::vector<std::vector<std::string>> acts) { acts_ = std::move(acts); }
  const std::vector<std::vector<std::string>> &acts() const { return acts_; }
  void set_rows(std::vector<MissionRow> rows) { rows_ = std::move(rows); }
  const std::vector<MissionRow> &rows() const { return rows_; }
  void set_xp_table(std::array<std::int32_t, XP_LEVELS> table);
  bool has_xp_table() const { return xp_ready_; }
  void set_network(bool on) { network_ = on; }
  bool network() const { return network_; }
  void set_local_slot(std::function<int()> slot) { local_slot_ = std::move(slot); }

  Record &record(int slot);
  const Record &record(int slot) const;
  void set_record(int slot, Record record);

  void seed_new_character(int slot);
  void quest_add(const std::string &title, const std::string &text);
  void quest_remove(const std::string &title);
  void clear_for_act_change();

  int mission_available(int mission) const;
  int mission_complete(int mission) const;
  void mission_set_available(int mission, int on);
  void mission_set_complete(int mission, int value);
  void grant_medal(int mission, int level, const std::function<void(const std::string &)> &run);
  int medal(int mission, int level, int flag) const;

  int evil() const;
  int decision() const;
  void set_evil_command(int on);
  void set_decision_command();

  void set_player_flag(int slot, int bit, int on);
  int player_flag(int slot, int bit) const;

  void give_gold(int amount);
  void give_exp_to(int slot, std::int32_t amount);

  std::int8_t var_get(const std::string &name);
  void var_set(const std::string &name, int value);
  void vars_clear();
  const std::vector<Var> &vars() const { return vars_; }
  void set_vars(std::vector<Var> vars) { vars_ = std::move(vars); }

  // missionMenu_open stores the chooser and leaves the run state at 0. Only
  // those two are written: the research does not say the open resets the
  // other menu words, and state 0 rewrites row, column, pause and counter.
  // The draw is not this function.
  void open_menu(int slot);
  bool menu_open() const { return menu_open_; }
  int chooser() const { return chooser_; }
  Menu &menu() { return menu_; }
  const Menu &menu() const { return menu_; }
  // One missionMenu_run call. Returns true on the tick that runs state 8,
  // which copies the chosen level name into menu().pending with orientation
  // 0xff. The medal the helpers read is record 0's: every executed menu case
  // had the local slot at 0.
  bool tick_menu(int button, const MenuInputs &inputs = {});

  // save_actChanged: stores the new act when a different one matches.
  // Standalone model of ref_quests.act_changed. The level change in
  // quests_flow.hpp takes the decision from level flow instead, so the
  // campaign and level flow can never disagree about one change.
  std::pair<int, int> act_changed(const std::string &name);
  std::string hub() const;

  void note_cflag(int bit, bool on);
  void note_boss_music();
  const Events &events() const { return events_; }

  // Typed campaign image. Not the PS2 card image: save_build was not re-run.
  // Version 2 holds mission rows 1..5. Version 1 held rows 0..4 and is
  // refused: it did not keep row 5.
  std::vector<std::uint8_t> save() const;
  static Campaign load(const std::vector<std::uint8_t> &);

private:
  void check_slot(int slot) const;
  void choice_set(std::uint8_t Record::*field, int on);
  int choice_get(std::uint8_t Record::*field) const;

  int num_players_ = 0;
  int difficulty_ = IMAGE_DIFFICULTY;
  int act_ = -1;
  bool network_ = false;
  bool xp_ready_ = false;
  bool menu_open_ = false;
  int chooser_ = 0;
  int reward_player_ = 0;
  std::array<std::int32_t, XP_LEVELS> xp_{};
  std::array<Record, PLAYERS> players_{};
  std::vector<std::vector<std::string>> acts_;
  std::vector<MissionRow> rows_;
  std::vector<Var> vars_;
  Menu menu_{};
  Events events_{};
  std::function<int()> local_slot_;
};
} // namespace champions::simulation::rta::quests
