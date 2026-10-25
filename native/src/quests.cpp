#include "champions/quests.hpp"
#include <climits>
#include <algorithm>
#include <cstring>
#include <limits>

namespace champions::simulation::rta::quests {
namespace {
constexpr std::uint8_t PLACEHOLDER[9] = {'N', 'o', 'Q', 'u', 'e', 's', 't', 's', 0};

void need_slot(int slot) {
  if (slot < 0 || slot >= PLAYERS)
    throw Error("player slot is outside the four records");
}
void need_mission(int mission) {
  if (mission < 0)
    throw Error("mission index is before the mission row");
}
// ref_natives._lower: ASCII upper case + 0x20. A byte of 0x80 or more indexes
// the original's character table with a negative number, so it is refused.
std::string lower_name(std::string name) {
  for (auto &c : name) {
    if (static_cast<unsigned char>(c) >= 0x80)
      throw Error("non-ASCII quest variable name");
    if (c >= 'A' && c <= 'Z')
      c = static_cast<char>(c - 'A' + 'a');
  }
  return name;
}
// strcpy into a 32-byte field: the string and its terminator, and the bytes
// after the terminator stay. A string that would run past the field is
// refused rather than written into the neighbouring field.
void copy_into(Field &field, const std::string &text) {
  if (text.size() >= QUEST_FIELD || text.find('\0') != std::string::npos)
    throw Error("quest string does not fit the 32-byte field");
  for (std::size_t i = 0; i < text.size(); ++i)
    field[i] = static_cast<std::uint8_t>(text[i]);
  field[text.size()] = 0;
}
// cstr of a stored field. A field with no terminator would read on into the
// next field on the console; that read is refused.
std::string stored_text(const Field &field) {
  if (std::find(field.begin(), field.end(), std::uint8_t(0)) == field.end())
    throw Error("quest field has no terminator");
  return field_text(field);
}
int clamp_byte(int value) {
  if (value < -128)
    return -128;
  if (value > 127)
    return 127;
  return value;
}
void put_u32(std::vector<std::uint8_t> &out, std::uint32_t value) {
  for (int shift = 0; shift < 32; shift += 8)
    out.push_back(static_cast<std::uint8_t>(value >> shift));
}
void put_i32(std::vector<std::uint8_t> &out, std::int32_t value) {
  put_u32(out, static_cast<std::uint32_t>(value));
}
void put_str(std::vector<std::uint8_t> &out, const std::string &text) {
  if (text.size() > 0xffffu)
    throw Error("saved string exceeds 65535 bytes");
  out.push_back(static_cast<std::uint8_t>(text.size()));
  out.push_back(static_cast<std::uint8_t>(text.size() >> 8));
  out.insert(out.end(), text.begin(), text.end());
}
struct Reader {
  const std::vector<std::uint8_t> &bytes;
  std::size_t at = 0;
  void need(std::size_t n) const {
    if (at + n > bytes.size())
      throw Error("campaign save is truncated");
  }
  std::uint32_t u32() {
    need(4);
    std::uint32_t value = 0;
    for (int i = 0; i < 4; ++i)
      value |= std::uint32_t(bytes[at + static_cast<std::size_t>(i)]) << (8 * i);
    at += 4;
    return value;
  }
  std::int32_t i32() { return static_cast<std::int32_t>(u32()); }
  std::uint8_t u8() {
    need(1);
    return bytes[at++];
  }
  std::string str() {
    need(2);
    const std::size_t n = std::size_t(bytes[at]) | (std::size_t(bytes[at + 1]) << 8);
    at += 2;
    need(n);
    std::string text(bytes.begin() + static_cast<std::ptrdiff_t>(at),
                     bytes.begin() + static_cast<std::ptrdiff_t>(at + n));
    at += n;
    return text;
  }
};
bool listed(const std::vector<int> &values, int mission) {
  return std::find(values.begin(), values.end(), mission) != values.end();
}
} // namespace

int mission_index(int difficulty, int mission) {
  if (difficulty < FIRST_DIFFICULTY || difficulty > LAST_DIFFICULTY)
    throw Error("difficulty is outside mission rows 1..5");
  if (mission < 0 || mission >= MISSIONS)
    throw Error("mission is outside the 13 of a row");
  return (difficulty - FIRST_DIFFICULTY) * MISSIONS + mission;
}

std::string field_text(const Field &field) {
  std::string text;
  for (auto byte : field) {
    if (!byte)
      break;
    text.push_back(static_cast<char>(byte));
  }
  return text;
}
Field field_from(const std::string &text) {
  if (text.size() >= QUEST_FIELD || text.find('\0') != std::string::npos)
    throw Error("quest string does not fit the 32-byte field");
  Field field{};
  for (std::size_t i = 0; i < text.size(); ++i)
    field[i] = static_cast<std::uint8_t>(text[i]);
  return field;
}
void overlay_placeholder(Field &field) {
  for (int i = 0; i < 9; ++i)
    field[static_cast<std::size_t>(i)] = PLACEHOLDER[i];
}

int skill_points_for(int level) {
  if (level < 6)
    return 1;
  if (level < 16)
    return 2;
  if (level < 26)
    return 3;
  if (level < 36)
    return 4;
  return 5;
}
ExpResult give_exp(int level, int exp, int skill, int stat, float hp, float max_hp,
                   float mana, float max_mana, int amount, const std::int32_t *table,
                   int count) {
  if (!table || count != XP_LEVELS)
    throw Error("the experience table has 81 entries");
  const std::int64_t cap = std::int64_t(table[80]) - 1;
  std::int64_t next = std::int64_t(exp) + amount;
  if (next > cap)
    next = cap;
  if (next < std::numeric_limits<std::int32_t>::min() ||
      next > std::numeric_limits<std::int32_t>::max())
    throw Error("experience does not fit a signed 32-bit word");
  int recalcs = 0;
  while (level < 80 && std::int64_t(table[level]) <= next) {
    skill += skill_points_for(level);
    stat = (stat + 3) & 0xFF;
    ++level;
    ++recalcs;
    if (hp > 0.f) {
      hp = max_hp;
      mana = max_mana;
    }
  }
  return {level, static_cast<int>(next), skill, stat & 0xFF, hp, mana, recalcs};
}
std::string hub_name(int evil, int decision, int mission1, int difficulty) {
  if (difficulty < 0)
    throw Error("difficulty bit is negative");
  // Bits above the evil byte cannot be set, so the name stays hub.
  if (difficulty > 7)
    return HUB_GOOD;
  const int bit = 1 << difficulty;
  if ((evil & bit) && (decision & bit) && (mission1 & 2))
    return HUB_EVIL;
  return HUB_GOOD;
}
int find_act(const std::string &name, int current,
             const std::vector<std::vector<std::string>> &acts) {
  std::vector<int> order;
  if (current >= 0 && static_cast<std::size_t>(current) < acts.size())
    order.push_back(current);
  for (std::size_t act = 0; act < acts.size(); ++act)
    order.push_back(static_cast<int>(act));
  for (int act : order) {
    for (const auto &prefix : acts[static_cast<std::size_t>(act)]) {
      if (!prefix.empty() && name.compare(0, prefix.size(), prefix) == 0)
        return act;
    }
  }
  return -1;
}
bool quest_item_kept(const std::string &name) {
  if (name.find("bloodstone") != std::string::npos || name.find("shardof") != std::string::npos)
    return true;
  return name == "spiritrunes" || name == "emptybag" || name == "fullbag" || name == "pickaxe" ||
         name == "twistedkey";
}
std::vector<Item> flag39_remaining(const std::vector<Item> &items) {
  std::vector<Item> kept;
  for (const auto &item : items) {
    if (((item.flags >> QUEST_ITEM_BIT) & 1u) && !quest_item_kept(item.name))
      continue;
    kept.push_back(item);
  }
  return kept;
}
std::pair<std::array<QuestLine, QUEST_SLOTS>, int>
clear_quest_list(const std::array<QuestLine, QUEST_SLOTS> &entries, int count) {
  if (count < 0 || count > QUEST_SLOTS)
    throw Error("quest count is outside 0..8");
  std::array<QuestLine, QUEST_SLOTS> out = entries;
  for (int index = 0; index < count; ++index) {
    overlay_placeholder(out[static_cast<std::size_t>(index)].title);
    overlay_placeholder(out[static_cast<std::size_t>(index)].text);
  }
  return {out, 0};
}

bool MenuWorld::mission_available(int mission) const { return listed(available, mission); }
bool MenuWorld::mission_complete(int mission) const { return listed(complete, mission); }
int MenuWorld::mission_medal(int mission) const {
  const auto found = medals.find(mission);
  return found == medals.end() ? 0 : found->second;
}
Menu new_menu() { return {}; }
int initial_row(const std::vector<int> &available) {
  int row = -1;
  for (int mission : available) {
    if (mission >= 1 && mission <= MENU_ROWS)
      row = std::max(row, mission - 1);
  }
  return row < 0 ? 0 : row;
}
int helper_value(int index, const MissionRow &row, bool available, bool complete, int medal) {
  if (index <= 2)
    return available ? 1 : 0;
  if (index == 3)
    return medal >= 1 && row.flag24 ? 1 : 0;
  if (index == 4)
    return medal >= 2 && row.flag24 ? 1 : 0;
  if (index == 5)
    return medal >= 3 && row.flag24 ? 1 : 0;
  if (index == 6)
    return complete && row.flag24 && !row.medal.empty() ? 1 : 0;
  if (index == 7)
    return complete && !row.medal.empty() ? 1 : 0;
  if (index == 8)
    return complete && row.flag20 ? 1 : 0;
  if (index >= 9 && index <= 11)
    return medal >= 3 && row.flag28 ? 1 : 0;
  throw Error("menu helper index is outside 0..11");
}
namespace {
void toward_side(Menu &state, const MissionRow &info, bool available, bool complete, int medal) {
  if (state.col == 0) {
    if (helper_value(6, info, available, complete, medal))
      state.col = 1;
    else if (helper_value(10, info, available, complete, medal))
      state.col = 2;
  } else if (state.col == 1 && helper_value(10, info, available, complete, medal)) {
    state.col = 2;
  }
}
void toward_story(Menu &state, const MissionRow &info, bool available, bool complete, int medal) {
  if (state.col == 2)
    state.col = helper_value(6, info, available, complete, medal) ? 1 : 0;
  else if (state.col == 1)
    state.col = 0;
}
} // namespace
void menu_tick(Menu &state, int button, const MenuWorld &world) {
  // missionMenu_run also reads these two lockstep pad aliases in the executed
  // mission-menu input probes (mission-menu-art.md, 2026-10-23).
  if (button & 0x200000) button |= BTN_DOWN;
  if (button & 0x400000) button |= BTN_LEFT;
  button &= 0xFFFF;
  const int st = state.state;
  if (st == 0) {
    state.row = initial_row(world.available);
    state.col = 0;
    state.paused = 1;
    state.counter = 0;
    state.state = 1;
    return;
  }
  if (st == 1) {
    state.counter += 8;
    if (state.counter >= 128) {
      state.counter = 127;
      state.state = 2;
    }
    return;
  }
  if (st == 2) {
    state.mark += 8;
    if (world.local && world.pause)
      return;
    if (!world.slot_active) {
      state.state = 4;
      return;
    }
    if (state.row < 0 || static_cast<std::size_t>(state.row) >= world.rows.size())
      throw Error("menu row is outside the mission table");
    const auto &info = world.rows[static_cast<std::size_t>(state.row)];
    const int mission = state.row + 1;
    const bool available = world.mission_available(mission);
    const bool complete = world.mission_complete(mission);
    const int medal = world.mission_medal(mission);
    if (button & BTN_CROSS) {
      bool accepted = false;
      if (state.col == 0 && helper_value(0, info, available, complete, medal))
        accepted = true;
      else if (state.col == 1 && helper_value(7, info, available, complete, medal))
        accepted = true;
      else if (state.col == 2 && helper_value(11, info, available, complete, medal))
        accepted = true;
      if (accepted) {
        state.state = 5;
        state.c0 = 1;
      }
      return;
    }
    if (button & (BTN_CIRCLE | BTN_SELECT)) {
      state.state = 4;
      return;
    }
    if (button & BTN_UP) {
      if (state.row > 0)
        state.row -= 1;
      return;
    }
    if (button & BTN_DOWN) {
      const int next = state.row + 1;
      if (next <= 11 && world.mission_available(next + 1)) {
        state.row = next;
        state.col = 0;
      }
      return;
    }
    const int toward = (state.row & 1) ? BTN_RIGHT : BTN_LEFT;
    const int back = (state.row & 1) ? BTN_LEFT : BTN_RIGHT;
    if (button & toward) {
      toward_side(state, info, available, complete, medal);
      return;
    }
    if (button & back)
      toward_story(state, info, available, complete, medal);
    return;
  }
  if (st == 3) {
    state.state = 2;
    return;
  }
  if (st == 4) {
    state.counter -= 16;
    return;
  }
  if (st == 5) {
    state.state = 6;
    return;
  }
  if (st == 6) {
    if (world.fade >= 128)
      state.state = 7;
    else
      state.c0 += 1;
    return;
  }
  if (st == 7) {
    state.state = 8;
    return;
  }
  if (st == 8) {
    if (state.row < 0 || static_cast<std::size_t>(state.row) >= world.rows.size())
      throw Error("menu row is outside the mission table");
    const auto &info = world.rows[static_cast<std::size_t>(state.row)];
    if (state.col == 1)
      state.pending = info.medal;
    else if (state.col == 2)
      state.pending = info.bonus;
    else if (world.evil && !info.evil.empty())
      state.pending = info.evil;
    else
      state.pending = info.story;
    state.orient = LAUNCH_ORIENT;
    state.c0 = 0;
  }
}

void Campaign::set_num_players(int count) {
  if (count < 0 || count > PLAYERS)
    throw Error("player count is outside 0..4");
  num_players_ = count;
}
void Campaign::set_xp_table(std::array<std::int32_t, XP_LEVELS> table) {
  xp_ = table;
  xp_ready_ = true;
}
void Campaign::check_slot(int slot) const { need_slot(slot); }
Record &Campaign::record(int slot) {
  check_slot(slot);
  return players_[static_cast<std::size_t>(slot)];
}
const Record &Campaign::record(int slot) const {
  check_slot(slot);
  return players_[static_cast<std::size_t>(slot)];
}
void Campaign::set_record(int slot, Record record) {
  check_slot(slot);
  players_[static_cast<std::size_t>(slot)] = std::move(record);
}
void Campaign::seed_new_character(int slot) {
  check_slot(slot);
  auto &missions = players_[static_cast<std::size_t>(slot)].missions;
  for (int difficulty = FIRST_DIFFICULTY; difficulty <= LAST_DIFFICULTY; ++difficulty) {
    auto &byte = missions[static_cast<std::size_t>(mission_index(difficulty, 0))];
    byte = static_cast<std::uint8_t>(byte | 3u);
  }
}
void Campaign::quest_add(const std::string &title, const std::string &text) {
  for (int slot = 0; slot < num_players_; ++slot) {
    auto &rec = players_[static_cast<std::size_t>(slot)];
    if (rec.quest_count > 7)
      return;
    if (rec.quest_count < 0)
      throw Error("quest count is negative");
    for (int index = 0; index < rec.quest_count; ++index) {
      if (stored_text(rec.quests[static_cast<std::size_t>(index)].title) == title)
        return;
    }
    copy_into(rec.quests[static_cast<std::size_t>(rec.quest_count)].title, title);
    copy_into(rec.quests[static_cast<std::size_t>(rec.quest_count)].text, text);
    ++rec.quest_count;
  }
}
void Campaign::quest_remove(const std::string &title) {
  for (int slot = 0; slot < num_players_; ++slot) {
    auto &rec = players_[static_cast<std::size_t>(slot)];
    if (rec.quest_count < 0 || rec.quest_count > QUEST_SLOTS)
      throw Error("quest count is outside 0..8");
    int last = -1;
    for (int index = 0; index < rec.quest_count; ++index) {
      auto &line = rec.quests[static_cast<std::size_t>(index)];
      if (stored_text(line.title) == title) {
        overlay_placeholder(line.title);
        overlay_placeholder(line.text);
        last = index;
      }
    }
    if (last < 0)
      continue;
    while (last < rec.quest_count - 1) {
      const auto &next = rec.quests[static_cast<std::size_t>(last + 1)];
      auto &line = rec.quests[static_cast<std::size_t>(last)];
      copy_into(line.title, stored_text(next.title));
      copy_into(line.text, stored_text(next.text));
      ++last;
    }
    --rec.quest_count;
  }
}
void Campaign::clear_for_act_change() {
  for (int slot = 0; slot < num_players_; ++slot) {
    auto &rec = players_[static_cast<std::size_t>(slot)];
    auto cleared = clear_quest_list(rec.quests, rec.quest_count);
    rec.quests = cleared.first;
    rec.quest_count = cleared.second;
    for (auto &bag : rec.bags) {
      const auto before = bag.size();
      bag = flag39_remaining(bag);
      events_.flag39_removed += before - bag.size();
    }
  }
  for (auto &var : vars_)
    var.value = 0;
  events_.act_resets += 1;
}
int Campaign::mission_available(int mission) const {
  need_mission(mission);
  if (mission >= MISSIONS)
    return 0;
  return players_[0].missions[static_cast<std::size_t>(mission_index(difficulty_, mission))] & 1;
}
int Campaign::mission_complete(int mission) const {
  need_mission(mission);
  if (mission >= MISSIONS)
    return 0;
  return (players_[0].missions[static_cast<std::size_t>(mission_index(difficulty_, mission))] >> 1) & 1;
}
void Campaign::mission_set_available(int mission, int on) {
  need_mission(mission);
  if (mission >= MISSIONS)
    return;
  const auto index = static_cast<std::size_t>(mission_index(difficulty_, mission));
  for (int slot = 0; slot < num_players_; ++slot) {
    auto &byte = players_[static_cast<std::size_t>(slot)].missions[index];
    byte = static_cast<std::uint8_t>((byte & 0xFEu) | (static_cast<unsigned>(on) & 1u));
  }
}
void Campaign::mission_set_complete(int mission, int value) {
  need_mission(mission);
  if (mission >= MISSIONS)
    return;
  const auto index = static_cast<std::size_t>(mission_index(difficulty_, mission));
  for (int slot = 0; slot < PLAYERS; ++slot) {
    auto &byte = players_[static_cast<std::size_t>(slot)].missions[index];
    if (value == -1)
      byte = static_cast<std::uint8_t>(byte | 2u);
    else
      byte = static_cast<std::uint8_t>((byte & 0xFDu) | ((static_cast<unsigned>(value) & 1u) << 1));
  }
}
void Campaign::grant_medal(int mission, int level, const std::function<void(const std::string &)> &run) {
  need_mission(mission);
  if (mission >= MISSIONS)
    return;
  for (int slot = 0; slot < num_players_; ++slot) {
    auto &byte = players_[static_cast<std::size_t>(slot)].medals[static_cast<std::size_t>(mission)];
    int have = byte >= 128 ? int(byte) - 256 : int(byte);
    if (have >= level)
      continue;
    reward_player_ = slot;
    auto callback = run;
    while (have < level) {
      if (!callback)
        throw Error("medal grant requires the reward script");
      if (have == 0) {
        callback("firstBronze");
        have = 1;
      } else if (have == 1) {
        callback("firstSilver");
        have = 2;
      } else {
        callback("firstGold");
        ++have;
      }
    }
    byte = static_cast<std::uint8_t>(level);
  }
}
int Campaign::medal(int mission, int level, int flag) const {
  need_mission(mission);
  int slot = 0;
  if (flag == 0 || !network_) {
    if (!local_slot_)
      throw Error("net_getLocalPlayerSlot callback required");
    slot = local_slot_();
  }
  check_slot(slot);
  if (mission >= MISSIONS)
    return 0;
  const auto byte = players_[static_cast<std::size_t>(slot)].medals[static_cast<std::size_t>(mission)];
  const int have = byte >= 128 ? int(byte) - 256 : int(byte);
  return level == -1 ? have : have >= level;
}
namespace {
// ref_natives._difficulty_bit, then the byte mask in _set_choice. A shift
// that does not land in the campaign byte leaves the byte unchanged.
unsigned choice_mask(int difficulty) {
  return (1u << (static_cast<unsigned>(difficulty) & 31u)) & 0xFFu;
}
} // namespace
void Campaign::choice_set(std::uint8_t Record::*field, int on) {
  const auto mask = static_cast<std::uint8_t>(choice_mask(difficulty_));
  for (int slot = 0; slot < num_players_; ++slot) {
    auto &byte = players_[static_cast<std::size_t>(slot)].*field;
    byte = on ? static_cast<std::uint8_t>(byte | mask) : static_cast<std::uint8_t>(byte & ~mask);
  }
}
int Campaign::choice_get(std::uint8_t Record::*field) const {
  return players_[0].*field & choice_mask(difficulty_);
}
int Campaign::evil() const { return choice_get(&Record::evil); }
int Campaign::decision() const { return choice_get(&Record::decision); }
void Campaign::set_evil_command(int on) {
  if (on)
    choice_set(&Record::evil, on);
}
void Campaign::set_decision_command() { choice_set(&Record::decision, 1); }
void Campaign::set_player_flag(int slot, int bit, int on) {
  const auto mask = static_cast<std::uint8_t>((1u << (static_cast<unsigned>(bit) & 31u)) & 0xFFu);
  auto apply = [&](int index) {
    check_slot(index);
    auto &byte = players_[static_cast<std::size_t>(index)].script_flags;
    byte = on ? static_cast<std::uint8_t>(byte | mask) : static_cast<std::uint8_t>(byte & ~mask);
  };
  if (slot == -1) {
    for (int index = 0; index < num_players_; ++index)
      apply(index);
  } else {
    apply(slot);
  }
}
int Campaign::player_flag(int slot, int bit) const {
  check_slot(slot);
  return players_[static_cast<std::size_t>(slot)].script_flags &
         ((1u << (static_cast<unsigned>(bit) & 31u)) & 0xFFu);
}
void Campaign::give_gold(int amount) {
  // n_givePlayerGold does not enter the loop when the player count is 0,
  // so the division is not performed. The script binder still faults that case.
  if (num_players_ <= 0)
    return;
  // C division toward zero; INT_MIN / -1 cannot occur because the count is 1..4.
  const int share = amount / num_players_;
  for (int slot = 0; slot < num_players_; ++slot) {
    auto &gold = players_[static_cast<std::size_t>(slot)].gold;
    // ref_natives n_givePlayerGold: s32(gold + share), then the cap.
    gold = static_cast<std::int32_t>(static_cast<std::uint32_t>(gold) + static_cast<std::uint32_t>(share));
    if (gold > MAX_GOLD)
      gold = MAX_GOLD;
  }
}
void Campaign::give_exp_to(int slot, std::int32_t amount) {
  check_slot(slot);
  if (!xp_ready_)
    throw Error("experience table was not supplied");
  auto &rec = players_[static_cast<std::size_t>(slot)];
  const auto result = give_exp(rec.level, rec.exp, rec.skill, rec.stat, rec.hp, rec.max_hp, rec.mana,
                               rec.max_mana, amount, xp_.data(), XP_LEVELS);
  rec.level = result.level;
  rec.exp = result.exp;
  rec.skill = result.skill;
  rec.stat = static_cast<std::uint8_t>(result.stat);
  rec.hp = result.hp;
  rec.mana = result.mana;
  events_.exp_grants += 1;
}
std::int8_t Campaign::var_get(const std::string &name) {
  const auto key = lower_name(name);
  for (const auto &var : vars_)
    if (var.name == key)
      return var.value;
  vars_.push_back({key, 0});
  return 0;
}
void Campaign::var_set(const std::string &name, int value) {
  const auto key = lower_name(name);
  const auto stored = static_cast<std::int8_t>(clamp_byte(value));
  for (auto &var : vars_) {
    if (var.name == key) {
      var.value = stored;
      return;
    }
  }
  vars_.push_back({key, stored});
}
void Campaign::vars_clear() {
  for (auto &var : vars_)
    var.value = 0;
}
void Campaign::open_menu(int slot) {
  chooser_ = slot;
  menu_.state = 0;
  menu_open_ = true;
  events_.menu_opens += 1;
}
bool Campaign::tick_menu(int button, const MenuInputs &inputs) {
  if (!menu_open_)
    throw Error("the mission menu is not open");
  MenuWorld world;
  world.rows = rows_;
  world.evil = evil() != 0;
  world.local = inputs.local;
  world.pause = inputs.pause;
  world.slot_active = inputs.slot_active;
  world.fade = inputs.fade;
  for (int mission = 0; mission < MISSIONS; ++mission) {
    const auto byte = players_[0].missions[static_cast<std::size_t>(mission_index(difficulty_, mission))];
    if (byte & 1u)
      world.available.push_back(mission);
    if (byte & 2u)
      world.complete.push_back(mission);
  }
  for (int mission = 0; mission < MISSIONS; ++mission) {
    const auto byte = players_[0].medals[static_cast<std::size_t>(mission)];
    const int have = byte >= 128 ? int(byte) - 256 : int(byte);
    if (have)
      world.medals[mission] = have;
  }
  const bool launch = menu_.state == 8;
  menu_tick(menu_, button, world);
  if (launch)
    events_.menu_launches += 1;
  return launch;
}
std::pair<int, int> Campaign::act_changed(const std::string &name) {
  const int found = find_act(name, act_, acts_);
  if (found >= 0 && found != act_) {
    act_ = found;
    return {1, found};
  }
  return {0, act_};
}
std::string Campaign::hub() const {
  const int mission = players_[0].missions[static_cast<std::size_t>(mission_index(difficulty_, 1))];
  return hub_name(players_[0].evil, players_[0].decision, mission, difficulty_);
}
void Campaign::note_cflag(int bit, bool) {
  if (bit == 0)
    events_.cflag_bit0 += 1;
  else if (bit == 26)
    events_.cflag_bit26 += 1;
  else
    events_.cflag_other += 1;
}
void Campaign::note_boss_music() { events_.boss_music += 1; }

namespace {
void put_field(std::vector<std::uint8_t> &out, const Field &field) {
  out.insert(out.end(), field.begin(), field.end());
}
// Version 2: Record::missions holds rows 1..5 (version 1 held rows 0..4).
constexpr std::uint32_t SAVE_VERSION = 2;
void put_float(std::vector<std::uint8_t> &out, float value) {
  std::uint32_t bits = 0;
  static_assert(sizeof(float) == 4, "float is 32 bits");
  std::memcpy(&bits, &value, 4);
  put_u32(out, bits);
}
} // namespace
std::vector<std::uint8_t> Campaign::save() const {
  std::vector<std::uint8_t> out;
  const char magic[] = {'O', 'C', 'Q', 'U', 'E', 'S', 'T', '1'};
  out.insert(out.end(), magic, magic + 8);
  put_u32(out, SAVE_VERSION);
  put_i32(out, num_players_);
  put_i32(out, difficulty_);
  put_i32(out, act_);
  out.push_back(network_ ? 1 : 0);
  out.push_back(xp_ready_ ? 1 : 0);
  out.push_back(static_cast<std::uint8_t>(reward_player_));
  out.push_back(static_cast<std::uint8_t>(chooser_));
  out.push_back(menu_open_ ? 1 : 0);
  for (auto value : xp_)
    put_i32(out, value);
  for (const auto &rec : players_) {
    for (const auto &line : rec.quests) {
      put_field(out, line.title);
      put_field(out, line.text);
    }
    put_i32(out, rec.quest_count);
    out.insert(out.end(), rec.missions.begin(), rec.missions.end());
    out.insert(out.end(), rec.medals.begin(), rec.medals.end());
    out.push_back(rec.evil);
    out.push_back(rec.decision);
    out.push_back(rec.script_flags);
    put_i32(out, rec.level);
    put_i32(out, rec.exp);
    put_i32(out, rec.skill);
    out.push_back(rec.stat);
    put_float(out, rec.hp);
    put_float(out, rec.max_hp);
    put_float(out, rec.mana);
    put_float(out, rec.max_mana);
    put_i32(out, rec.gold);
    for (const auto &bag : rec.bags) {
      put_u32(out, static_cast<std::uint32_t>(bag.size()));
      for (const auto &item : bag) {
        put_str(out, item.name);
        put_u32(out, static_cast<std::uint32_t>(item.flags));
        put_u32(out, static_cast<std::uint32_t>(item.flags >> 32));
      }
    }
  }
  put_u32(out, static_cast<std::uint32_t>(vars_.size()));
  for (const auto &var : vars_) {
    put_str(out, var.name);
    out.push_back(static_cast<std::uint8_t>(var.value));
  }
  put_u32(out, static_cast<std::uint32_t>(acts_.size()));
  for (const auto &act : acts_) {
    put_u32(out, static_cast<std::uint32_t>(act.size()));
    for (const auto &name : act)
      put_str(out, name);
  }
  put_u32(out, static_cast<std::uint32_t>(rows_.size()));
  for (const auto &row : rows_) {
    put_str(out, row.story);
    put_str(out, row.evil);
    put_str(out, row.medal);
    put_str(out, row.bonus);
    put_i32(out, row.flag20);
    put_i32(out, row.flag24);
    put_i32(out, row.flag28);
  }
  put_i32(out, menu_.state);
  put_i32(out, menu_.row);
  put_i32(out, menu_.col);
  put_i32(out, menu_.counter);
  put_i32(out, menu_.mark);
  put_i32(out, menu_.c0);
  put_i32(out, menu_.paused);
  put_i32(out, menu_.orient);
  put_str(out, menu_.pending);
  return out;
}
Campaign Campaign::load(const std::vector<std::uint8_t> &bytes) {
  Reader in{bytes, 0};
  in.need(8);
  const char magic[] = {'O', 'C', 'Q', 'U', 'E', 'S', 'T', '1'};
  if (!std::equal(magic, magic + 8, bytes.begin()))
    throw Error("campaign save magic");
  in.at = 8;
  const auto version = in.u32();
  if (version == 1)
    throw Error("campaign save version 1 holds mission rows 0..4 and not row 5");
  if (version != SAVE_VERSION)
    throw Error("campaign save version");
  Campaign out;
  out.num_players_ = in.i32();
  out.difficulty_ = in.i32();
  out.act_ = in.i32();
  if (out.num_players_ < 0 || out.num_players_ > PLAYERS)
    throw Error("campaign save player count");
  out.network_ = in.u8() != 0;
  out.xp_ready_ = in.u8() != 0;
  out.reward_player_ = in.u8();
  out.chooser_ = in.u8();
  out.menu_open_ = in.u8() != 0;
  for (auto &value : out.xp_)
    value = in.i32();
  for (auto &rec : out.players_) {
    for (auto &line : rec.quests) {
      in.need(64);
      std::copy(bytes.begin() + static_cast<std::ptrdiff_t>(in.at),
                bytes.begin() + static_cast<std::ptrdiff_t>(in.at + 32), line.title.begin());
      std::copy(bytes.begin() + static_cast<std::ptrdiff_t>(in.at + 32),
                bytes.begin() + static_cast<std::ptrdiff_t>(in.at + 64), line.text.begin());
      in.at += 64;
    }
    rec.quest_count = in.i32();
    in.need(rec.missions.size());
    std::copy(bytes.begin() + static_cast<std::ptrdiff_t>(in.at),
              bytes.begin() + static_cast<std::ptrdiff_t>(in.at + rec.missions.size()),
              rec.missions.begin());
    in.at += rec.missions.size();
    in.need(rec.medals.size());
    std::copy(bytes.begin() + static_cast<std::ptrdiff_t>(in.at),
              bytes.begin() + static_cast<std::ptrdiff_t>(in.at + rec.medals.size()),
              rec.medals.begin());
    in.at += rec.medals.size();
    rec.evil = in.u8();
    rec.decision = in.u8();
    rec.script_flags = in.u8();
    rec.level = in.i32();
    rec.exp = in.i32();
    rec.skill = in.i32();
    rec.stat = in.u8();
    auto take_float = [&]() {
      const auto bits = in.u32();
      float value = 0;
      std::memcpy(&value, &bits, 4);
      return value;
    };
    rec.hp = take_float();
    rec.max_hp = take_float();
    rec.mana = take_float();
    rec.max_mana = take_float();
    rec.gold = in.i32();
    for (auto &bag : rec.bags) {
      const auto count = in.u32();
      if (count > 64)
        throw Error("campaign save bag is larger than 64");
      for (std::uint32_t n = 0; n < count; ++n) {
        Item item;
        item.name = in.str();
        const auto low = in.u32();
        const auto high = in.u32();
        item.flags = std::uint64_t(low) | (std::uint64_t(high) << 32);
        bag.push_back(std::move(item));
      }
    }
  }
  const auto vars = in.u32();
  // Authored overrun guards. save_build was not re-run, so these counts are
  // not original table capacities. A bag of more than 64 is the inventory
  // slot count documented for the player record.
  if (vars > 100000u)
    throw Error("campaign save variable count");
  for (std::uint32_t n = 0; n < vars; ++n)
    out.vars_.push_back({in.str(), static_cast<std::int8_t>(in.u8())});
  const auto acts = in.u32();
  if (acts > 64u)
    throw Error("campaign save act count");
  for (std::uint32_t act = 0; act < acts; ++act) {
    const auto names = in.u32();
    if (names > 256u)
      throw Error("campaign save act row count");
    std::vector<std::string> row;
    for (std::uint32_t n = 0; n < names; ++n)
      row.push_back(in.str());
    out.acts_.push_back(std::move(row));
  }
  const auto rows = in.u32();
  if (rows > 64u)
    throw Error("campaign save mission row count");
  for (std::uint32_t n = 0; n < rows; ++n) {
    MissionRow row;
    row.story = in.str();
    row.evil = in.str();
    row.medal = in.str();
    row.bonus = in.str();
    row.flag20 = in.i32();
    row.flag24 = in.i32();
    row.flag28 = in.i32();
    out.rows_.push_back(std::move(row));
  }
  out.menu_.state = in.i32();
  out.menu_.row = in.i32();
  out.menu_.col = in.i32();
  out.menu_.counter = in.i32();
  out.menu_.mark = in.i32();
  out.menu_.c0 = in.i32();
  out.menu_.paused = in.i32();
  out.menu_.orient = in.i32();
  out.menu_.pending = in.str();
  if (in.at != bytes.size())
    throw Error("campaign save has trailing bytes");
  return out;
}
} // namespace champions::simulation::rta::quests
