#include "champions/quests_bridge.hpp"
#include <cstring>

namespace champions::simulation::rta::quests {
namespace builtins = champions::simulation::rta::builtins;
namespace {
constexpr std::uint32_t PLAYERS_AT = 0x4d8af0;
constexpr std::uint32_t RECORD = 0x4c30;
constexpr std::uint32_t NUM_PLAYERS = 0x4d8ae0;
constexpr std::uint32_t DIFFICULTY = 0x4f719c;
constexpr std::uint32_t QUEST_VARS = 0x86da80;
constexpr std::uint32_t QUEST_AT = 0x41d0;
constexpr std::uint32_t QUEST_COUNT = 0x43d0;
// Mission rows 1..5: +0x4bbf + 1 * 13 = +0x4bcc through +0x4c0c. Row 0
// (+0x4bbf..+0x4bcb) is not a mission row: clearAllSkills' pairs and the
// countdowns live there (research-items 49bc578 docs/script-items.md 3).
constexpr std::uint32_t MISSION_AT = 0x4bbf + FIRST_DIFFICULTY * MISSIONS;
static_assert(MISSION_AT == 0x4bcc && MISSION_AT + DIFFICULTIES * MISSIONS == 0x4c0d,
              "five mission rows end where the medal bytes start");
constexpr std::uint32_t MEDAL_AT = 0x4c0d;
constexpr std::uint32_t EVIL_AT = 0x4c1a;
constexpr std::uint32_t DECISION_AT = 0x4c1b;
constexpr std::uint32_t FLAG_AT = 0x4c28;
constexpr std::uint32_t LEVEL_AT = 0x59;
constexpr std::uint32_t EXP_AT = 0x5c;
constexpr std::uint32_t SKILL_AT = 0xe8;
constexpr std::uint32_t STAT_AT = 0xec;
// HP, max HP, mana and max mana are the float words difftest_quests plants
// for player_giveExp: +0xac, +0xb0, +0xc0, +0xc4.
constexpr std::uint32_t HP_AT = 0xac;
constexpr std::uint32_t MAX_HP_AT = 0xb0;
constexpr std::uint32_t MANA_AT = 0xc0;
constexpr std::uint32_t MAX_MANA_AT = 0xc4;
constexpr std::uint32_t GOLD_AT = 0xe4;

std::uint32_t rec(int slot) { return PLAYERS_AT + static_cast<std::uint32_t>(slot) * RECORD; }
float float_of(std::uint32_t bits) {
  float value = 0;
  static_assert(sizeof(float) == 4, "float is 32 bits");
  std::memcpy(&value, &bits, 4);
  return value;
}
std::uint32_t bits_of(float value) {
  std::uint32_t bits = 0;
  std::memcpy(&bits, &value, 4);
  return bits;
}
Field take_field(const builtins::Memory &memory, std::uint32_t address) {
  Field field{};
  const auto bytes = memory.read(address, QUEST_FIELD);
  for (std::size_t i = 0; i < bytes.size() && i < field.size(); ++i)
    field[i] = bytes[i];
  return field;
}
} // namespace

void mirror_from_variables(Campaign &campaign, const builtins::Variables &variables,
                           bool current_known) {
  const auto &memory = variables.memory;
  int players = memory.s32(NUM_PLAYERS);
  if (players < 0)
    players = 0;
  if (players > PLAYERS)
    players = PLAYERS;
  campaign.set_num_players(players);
  campaign.set_difficulty(memory.s32(DIFFICULTY));
  for (int slot = 0; slot < PLAYERS; ++slot) {
    auto record = campaign.record(slot);
    const auto base = rec(slot);
    record.quest_count = memory.s32(base + QUEST_COUNT);
    if (record.quest_count < 0)
      record.quest_count = 0;
    if (record.quest_count > QUEST_SLOTS)
      record.quest_count = QUEST_SLOTS;
    for (int index = 0; index < QUEST_SLOTS; ++index) {
      record.quests[static_cast<std::size_t>(index)].title =
          take_field(memory, base + QUEST_AT + static_cast<std::uint32_t>(index) * QUEST_STRIDE);
      record.quests[static_cast<std::size_t>(index)].text = take_field(
          memory, base + QUEST_AT + static_cast<std::uint32_t>(index) * QUEST_STRIDE + QUEST_FIELD);
    }
    for (std::size_t i = 0; i < record.missions.size(); ++i)
      record.missions[i] = memory.read(base + MISSION_AT + static_cast<std::uint32_t>(i));
    for (std::size_t i = 0; i < record.medals.size(); ++i)
      record.medals[i] = memory.read(base + MEDAL_AT + static_cast<std::uint32_t>(i));
    record.evil = memory.read(base + EVIL_AT);
    record.decision = memory.read(base + DECISION_AT);
    record.script_flags = memory.read(base + FLAG_AT);
    record.level = memory.s8(base + LEVEL_AT);
    record.exp = memory.s32(base + EXP_AT);
    record.skill = memory.s32(base + SKILL_AT);
    record.stat = memory.read(base + STAT_AT);
    if (current_known || slot != 0) record.hp = float_of(memory.word(base + HP_AT));
    record.max_hp = float_of(memory.word(base + MAX_HP_AT));
    if (current_known || slot != 0) record.mana = float_of(memory.word(base + MANA_AT));
    record.max_mana = float_of(memory.word(base + MAX_MANA_AT));
    record.gold = memory.s32(base + GOLD_AT);
    campaign.set_record(slot, std::move(record));
  }
  std::vector<Var> vars;
  auto address = QUEST_VARS;
  for (int guard = 0; guard < 100000; ++guard) {
    if (!memory.read(address))
      break;
    const auto name = memory.cstr(address);
    const auto value = static_cast<std::int8_t>(memory.s8(address + static_cast<std::uint32_t>(name.size()) + 1));
    vars.push_back({name, value});
    address += static_cast<std::uint32_t>(name.size() + 2);
  }
  campaign.set_vars(std::move(vars));
}

void mirror_to_variables(const Campaign &campaign, builtins::Variables &variables,
                         bool current_known) {
  auto &memory = variables.memory;
  memory.word(NUM_PLAYERS, static_cast<std::uint32_t>(campaign.num_players()));
  memory.word(DIFFICULTY, static_cast<std::uint32_t>(campaign.difficulty()));
  for (int slot = 0; slot < PLAYERS; ++slot) {
    const auto &record = campaign.record(slot);
    const auto base = rec(slot);
    for (int index = 0; index < QUEST_SLOTS; ++index) {
      const auto at = base + QUEST_AT + static_cast<std::uint32_t>(index) * QUEST_STRIDE;
      memory.write(at, std::vector<std::uint8_t>(record.quests[static_cast<std::size_t>(index)].title.begin(),
                                                record.quests[static_cast<std::size_t>(index)].title.end()));
      memory.write(at + QUEST_FIELD,
                   std::vector<std::uint8_t>(record.quests[static_cast<std::size_t>(index)].text.begin(),
                                            record.quests[static_cast<std::size_t>(index)].text.end()));
    }
    memory.word(base + QUEST_COUNT, static_cast<std::uint32_t>(record.quest_count));
    for (std::size_t i = 0; i < record.missions.size(); ++i)
      memory.byte(base + MISSION_AT + static_cast<std::uint32_t>(i), record.missions[i]);
    for (std::size_t i = 0; i < record.medals.size(); ++i)
      memory.byte(base + MEDAL_AT + static_cast<std::uint32_t>(i), record.medals[i]);
    memory.byte(base + EVIL_AT, record.evil);
    memory.byte(base + DECISION_AT, record.decision);
    memory.byte(base + FLAG_AT, record.script_flags);
    memory.byte(base + LEVEL_AT, static_cast<std::uint8_t>(record.level));
    memory.word(base + EXP_AT, static_cast<std::uint32_t>(record.exp));
    memory.word(base + SKILL_AT, static_cast<std::uint32_t>(record.skill));
    memory.byte(base + STAT_AT, record.stat);
    if (current_known || slot != 0) memory.word(base + HP_AT, bits_of(record.hp));
    memory.word(base + MAX_HP_AT, bits_of(record.max_hp));
    if (current_known || slot != 0) memory.word(base + MANA_AT, bits_of(record.mana));
    memory.word(base + MAX_MANA_AT, bits_of(record.max_mana));
    memory.word(base + GOLD_AT, static_cast<std::uint32_t>(record.gold));
  }
  // Values only. Names stay where the binder wrote them.
  auto address = QUEST_VARS;
  for (const auto &var : campaign.vars()) {
    if (!memory.read(address))
      break;
    const auto name = memory.cstr(address);
    if (name == var.name)
      memory.byte(address + static_cast<std::uint32_t>(name.size()) + 1, static_cast<std::uint8_t>(var.value));
    address += static_cast<std::uint32_t>(name.size() + 2);
  }
}

void restore_variables(const Campaign &campaign, builtins::Variables &variables,
                       bool current_known) {
  mirror_to_variables(campaign, variables, current_known);
  // The packed g_questVars table, rewritten whole: name, 0, value byte, and
  // an empty name after the last entry (ref_natives vars_*).
  auto &memory = variables.memory;
  auto address = QUEST_VARS;
  for (const auto &var : campaign.vars()) {
    if (var.name.empty() || var.name.find('\0') != std::string::npos)
      throw Error("saved quest variable name is empty or holds a zero byte");
    memory.string(address, var.name);
    memory.byte(address + static_cast<std::uint32_t>(var.name.size()) + 1, static_cast<std::uint8_t>(var.value));
    address += static_cast<std::uint32_t>(var.name.size() + 2);
  }
  memory.byte(address, 0);
}

void write_exp(const Campaign &campaign, builtins::Variables &variables, int slot) {
  if (slot < 0 || slot >= PLAYERS)
    throw Error("player slot is outside the four records");
  const auto &record = campaign.record(slot);
  const auto base = rec(slot);
  variables.memory.byte(base + LEVEL_AT, static_cast<std::uint8_t>(record.level));
  variables.memory.word(base + EXP_AT, static_cast<std::uint32_t>(record.exp));
  variables.memory.word(base + SKILL_AT, static_cast<std::uint32_t>(record.skill));
  variables.memory.byte(base + STAT_AT, record.stat);
  variables.memory.word(base + HP_AT, bits_of(record.hp));
  variables.memory.word(base + MANA_AT, bits_of(record.mana));
}

bool route_undocumented(const char *name, const Campaign &campaign) {
  if (!name)
    return false;
  if (std::strcmp(name, "setCFlag") == 0 || std::strcmp(name, "activateMissionMenu") == 0)
    return true;
  return std::strcmp(name, "givePlayerExp") == 0 && campaign.has_xp_table();
}
} // namespace champions::simulation::rta::quests
