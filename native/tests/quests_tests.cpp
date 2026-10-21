#include "champions/amx.hpp"
#include "champions/level_flow.hpp"
#include "champions/quests.hpp"
#include "champions/script_commands.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace q = champions::simulation::rta::quests;
namespace flow = champions::simulation::rta::level_flow;
namespace sc = champions::simulation::rta::script_commands;
namespace a = champions::simulation::rta::amx;

namespace {
int failed = 0;
int line_no = 0;
void check(bool ok, const std::string &what) {
  if (ok)
    return;
  std::cerr << "FAIL line " << line_no << " " << what << '\n';
  ++failed;
}
std::vector<std::string> fields_of(const std::string &line) {
  std::vector<std::string> fields;
  std::stringstream stream(line);
  std::string field;
  while (std::getline(stream, field, '\t'))
    fields.push_back(field);
  return fields;
}
int i32(const std::string &text) { return std::stoi(text); }
std::uint64_t u64(const std::string &text) { return std::stoull(text); }
double f64(const std::string &text) { return std::stod(text); }
bool near(double left, double right) { return std::fabs(left - right) < 1e-4; }
q::Field field_hex(const std::string &hex) {
  q::Field field{};
  check(hex.size() == 64, "quest field hex width");
  if (hex.size() != 64)
    return field;
  for (std::size_t i = 0; i < field.size(); ++i) {
    field[i] = static_cast<std::uint8_t>(std::stoul(hex.substr(i * 2, 2), nullptr, 16));
  }
  return field;
}
std::vector<std::string> split_bar(const std::string &text) {
  std::vector<std::string> out;
  if (text.empty()) {
    out.emplace_back();
    return out;
  }
  std::stringstream stream(text);
  std::string field;
  while (std::getline(stream, field, '|'))
    out.push_back(field);
  return out;
}
int command_index(const char *name) {
  for (std::size_t i = 0; i < sc::COMMANDS; ++i)
    if (std::strcmp(sc::row(i).name, name) == 0)
      return static_cast<int>(i);
  return -1;
}

struct Built {
  a::ByteVector image;
  std::vector<std::pair<unsigned, std::string>> strings;
};
struct Call {
  std::string native;
  std::vector<a::Cell> args;
};
Built build_calls(const std::vector<Call> &calls,
                  const std::vector<std::pair<unsigned, std::string>> &strings) {
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
  program.push_back(a::Label{"start:"});
  ins("PROC");
  for (const auto &call : calls) {
    for (auto it = call.args.rbegin(); it != call.args.rend(); ++it)
      ins("PUSH.C", {*it});
    ins("PUSH.C", {a::Cell(4 * call.args.size())});
    ins("SYSREQ.C", {native_index(call.native)});
    ins("STACK", {a::Cell(4 * (call.args.size() + 1))});
  }
  ins("RETN");
  const auto code = a::assemble(program);
  a::ImageOptions options;
  options.data.resize(4096);
  options.natives = natives;
  options.stack_cells = 256;
  options.publics.emplace_back("start", a::Cell(code.labels.at("start")));
  Built built;
  built.image = a::build_image(code.cells, options);
  built.strings = strings;
  return built;
}

struct Fixture {
  q::Campaign campaign;
  q::Menu menu;
  q::MenuWorld world;
  std::vector<std::vector<std::string>> acts;
};

void replay(const std::string &path) {
  std::ifstream in(path);
  check(static_cast<bool>(in), "vector file opens");
  Fixture fixture;
  std::string line;
  while (std::getline(in, line)) {
    ++line_no;
    if (line.empty())
      continue;
    const auto fields = fields_of(line);
    if (fields.empty())
      continue;
    const auto &op = fields[0];
    if (op == "commit") {
      check(fields.size() == 2 && fields[1] == "7efd3ce", "research pin");
    } else if (op == "reset") {
      fixture = {};
    } else if (op == "set_players") {
      fixture.campaign.set_num_players(i32(fields.at(1)));
    } else if (op == "set_difficulty") {
      fixture.campaign.set_difficulty(i32(fields.at(1)));
    } else if (op == "quest_add") {
      fixture.campaign.quest_add(fields.at(1), fields.at(2));
    } else if (op == "quest_remove") {
      fixture.campaign.quest_remove(fields.at(1));
    } else if (op == "plant_count") {
      auto record = fixture.campaign.record(i32(fields.at(1)));
      record.quest_count = i32(fields.at(2));
      fixture.campaign.set_record(i32(fields.at(1)), std::move(record));
    } else if (op == "plant_hex") {
      auto record = fixture.campaign.record(i32(fields.at(1)));
      const auto index = static_cast<std::size_t>(i32(fields.at(2)));
      record.quests[index].title = field_hex(fields.at(3));
      record.quests[index].text = field_hex(fields.at(4));
      fixture.campaign.set_record(i32(fields.at(1)), std::move(record));
    } else if (op == "clear_list") {
      const int slot = i32(fields.at(1));
      auto record = fixture.campaign.record(slot);
      auto cleared = q::clear_quest_list(record.quests, record.quest_count);
      record.quests = cleared.first;
      record.quest_count = cleared.second;
      fixture.campaign.set_record(slot, std::move(record));
    } else if (op == "item") {
      auto record = fixture.campaign.record(i32(fields.at(1)));
      record.bags[static_cast<std::size_t>(i32(fields.at(2)))].push_back(
          {fields.at(3), u64(fields.at(4))});
      fixture.campaign.set_record(i32(fields.at(1)), std::move(record));
    } else if (op == "flag39") {
      const int slot = i32(fields.at(1));
      const auto bag = static_cast<std::size_t>(i32(fields.at(2)));
      auto record = fixture.campaign.record(slot);
      record.bags[bag] = q::flag39_remaining(record.bags[bag]);
      fixture.campaign.set_record(slot, std::move(record));
    } else if (op == "expect_count") {
      check(fixture.campaign.record(i32(fields.at(1))).quest_count == i32(fields.at(2)),
            "quest count");
    } else if (op == "expect_line") {
      const auto &line_at =
          fixture.campaign.record(i32(fields.at(1))).quests[static_cast<std::size_t>(i32(fields.at(2)))];
      check(q::field_text(line_at.title) == fields.at(3) && q::field_text(line_at.text) == fields.at(4),
            "quest line");
    } else if (op == "expect_hex") {
      const auto &line_at =
          fixture.campaign.record(i32(fields.at(1))).quests[static_cast<std::size_t>(i32(fields.at(2)))];
      check(line_at.title == field_hex(fields.at(3)) && line_at.text == field_hex(fields.at(4)),
            "quest field bytes");
    } else if (op == "expect_item") {
      const auto &item = fixture.campaign.record(i32(fields.at(1)))
                             .bags[static_cast<std::size_t>(i32(fields.at(2)))]
                                 [static_cast<std::size_t>(i32(fields.at(3)))];
      check(item.name == fields.at(4) && item.flags == u64(fields.at(5)), "kept quest item");
    } else if (op == "expect_items") {
      check(static_cast<int>(fixture.campaign.record(i32(fields.at(1)))
                                 .bags[static_cast<std::size_t>(i32(fields.at(2)))]
                                 .size()) == i32(fields.at(3)),
            "kept item count");
    } else if (op == "exp") {
      check(fields.size() == 98, "experience vector width");
      if (fields.size() != 98)
        continue;
      std::array<std::int32_t, q::XP_LEVELS> table{};
      for (int i = 0; i < q::XP_LEVELS; ++i)
        table[static_cast<std::size_t>(i)] = i32(fields[static_cast<std::size_t>(10 + i)]);
      const auto got = q::give_exp(i32(fields[1]), i32(fields[2]), i32(fields[3]), i32(fields[4]),
                                  static_cast<float>(f64(fields[5])), static_cast<float>(f64(fields[6])),
                                  static_cast<float>(f64(fields[7])), static_cast<float>(f64(fields[8])),
                                  i32(fields[9]), table.data(), q::XP_LEVELS);
      check(got.level == i32(fields[91]) && got.exp == i32(fields[92]) && got.skill == i32(fields[93]) &&
                got.stat == i32(fields[94]) && near(got.hp, f64(fields[95])) &&
                near(got.mana, f64(fields[96])) && got.recalcs == i32(fields[97]),
            "experience result");
    } else if (op == "hub") {
      check(q::hub_name(i32(fields.at(1)), i32(fields.at(2)), i32(fields.at(3)), i32(fields.at(4))) ==
                fields.at(5),
            "hub name");
    } else if (op == "acts") {
      fixture.acts.clear();
      for (std::size_t i = 1; i < fields.size(); ++i)
        fixture.acts.push_back(split_bar(fields[i]));
      fixture.campaign.set_acts(fixture.acts);
    } else if (op == "find_act") {
      check(q::find_act(fields.at(1), i32(fields.at(2)), fixture.acts) == i32(fields.at(3)),
            "act prefix");
    } else if (op == "act_changed") {
      fixture.campaign.set_act(i32(fields.at(2)));
      const auto got = fixture.campaign.act_changed(fields.at(1));
      check(got.first == i32(fields.at(3)) && got.second == i32(fields.at(4)), "act change");
    } else if (op == "mission_on") {
      fixture.campaign.mission_set_available(i32(fields.at(1)), i32(fields.at(2)));
    } else if (op == "mission_done") {
      fixture.campaign.mission_set_complete(i32(fields.at(1)), i32(fields.at(2)));
    } else if (op == "expect_mission") {
      const int slot = i32(fields.at(1));
      const int index = i32(fields.at(2)) * q::MISSIONS + i32(fields.at(3));
      check(fixture.campaign.record(slot).missions[static_cast<std::size_t>(index)] ==
                static_cast<std::uint8_t>(i32(fields.at(4))),
            "mission byte");
    } else if (op == "medal") {
      std::string names;
      fixture.campaign.grant_medal(i32(fields.at(1)), i32(fields.at(2)), [&](const std::string &name) {
        if (!names.empty())
          names.push_back(',');
        names += name;
      });
      check(names == fields.at(3), "medal scripts");
    } else if (op == "expect_medal") {
      check(fixture.campaign.record(i32(fields.at(1))).medals[static_cast<std::size_t>(i32(fields.at(2)))] ==
                static_cast<std::uint8_t>(i32(fields.at(3))),
            "medal byte");
    } else if (op == "evil") {
      fixture.campaign.set_evil_command(i32(fields.at(1)));
    } else if (op == "decision") {
      fixture.campaign.set_decision_command();
    } else if (op == "expect_choice") {
      check(fixture.campaign.record(0).evil == static_cast<std::uint8_t>(i32(fields.at(1))) &&
                fixture.campaign.record(0).decision == static_cast<std::uint8_t>(i32(fields.at(2))) &&
                fixture.campaign.record(1).evil == static_cast<std::uint8_t>(i32(fields.at(3))) &&
                fixture.campaign.record(1).decision == static_cast<std::uint8_t>(i32(fields.at(4))),
            "evil and decision bytes");
    } else if (op == "flag") {
      fixture.campaign.set_player_flag(i32(fields.at(1)), i32(fields.at(2)), i32(fields.at(3)));
    } else if (op == "expect_flag") {
      check(fixture.campaign.record(i32(fields.at(1))).script_flags ==
                static_cast<std::uint8_t>(i32(fields.at(2))),
            "script flag byte");
    } else if (op == "gold") {
      fixture.campaign.give_gold(i32(fields.at(1)));
    } else if (op == "expect_gold") {
      check(fixture.campaign.record(i32(fields.at(1))).gold == i32(fields.at(2)), "gold");
    } else if (op == "var_set") {
      fixture.campaign.var_set(fields.at(1), i32(fields.at(2)));
    } else if (op == "var_get") {
      fixture.campaign.var_get(fields.at(1));
    } else if (op == "vars_clear") {
      fixture.campaign.vars_clear();
    } else if (op == "expect_var") {
      const q::Var *found = nullptr;
      for (const auto &var : fixture.campaign.vars())
        if (var.name == fields.at(1))
          found = &var;
      check(found && found->value == static_cast<std::int8_t>(i32(fields.at(2))), "quest variable");
    } else if (op == "seed") {
      fixture.campaign.seed_new_character(i32(fields.at(1)));
    } else if (op == "menu_reset") {
      fixture.menu = q::new_menu();
      fixture.world = {};
    } else if (op == "menu_row") {
      q::MissionRow row;
      row.story = fields.at(1);
      row.evil = fields.at(2);
      row.medal = fields.at(3);
      row.bonus = fields.at(4);
      row.flag20 = i32(fields.at(5));
      row.flag24 = i32(fields.at(6));
      row.flag28 = i32(fields.at(7));
      fixture.world.rows.push_back(row);
    } else if (op == "menu_avail") {
      fixture.world.available.clear();
      for (std::size_t i = 1; i < fields.size(); ++i)
        fixture.world.available.push_back(i32(fields[i]));
    } else if (op == "menu_done") {
      fixture.world.complete.clear();
      for (std::size_t i = 1; i < fields.size(); ++i)
        fixture.world.complete.push_back(i32(fields[i]));
    } else if (op == "menu_medal") {
      fixture.world.medals[i32(fields.at(1))] = i32(fields.at(2));
    } else if (op == "menu_flags") {
      fixture.world.evil = i32(fields.at(1)) != 0;
      fixture.world.local = i32(fields.at(2)) != 0;
      fixture.world.pause = i32(fields.at(3)) != 0;
      fixture.world.slot_active = i32(fields.at(4)) != 0;
      fixture.world.fade = i32(fields.at(5));
    } else if (op == "menu_place") {
      fixture.menu.row = i32(fields.at(1));
      fixture.menu.col = i32(fields.at(2));
    } else if (op == "menu_button") {
      q::menu_tick(fixture.menu, i32(fields.at(1)), fixture.world);
    } else if (op == "expect_menu") {
      check(fixture.menu.state == i32(fields.at(1)) && fixture.menu.row == i32(fields.at(2)) &&
                fixture.menu.col == i32(fields.at(3)) && fixture.menu.counter == i32(fields.at(4)) &&
                fixture.menu.mark == i32(fields.at(5)) && fixture.menu.paused == i32(fields.at(6)) &&
                fixture.menu.pending == fields.at(7) && fixture.menu.orient == i32(fields.at(8)) &&
                fixture.menu.c0 == i32(fields.at(9)),
            "menu state");
    } else if (op == "helper") {
      q::MissionRow row;
      row.story = fields.at(2);
      row.evil = fields.at(3);
      row.medal = fields.at(4);
      row.bonus = fields.at(5);
      row.flag20 = i32(fields.at(6));
      row.flag24 = i32(fields.at(7));
      row.flag28 = i32(fields.at(8));
      check(q::helper_value(i32(fields.at(1)), row, i32(fields.at(9)) != 0, i32(fields.at(10)) != 0,
                            i32(fields.at(11))) == i32(fields.at(12)),
            "menu helper");
    } else {
      check(false, "unknown vector op " + op);
    }
  }
}

void test_contract(const std::string &path) {
  std::ifstream in(path);
  std::stringstream buffer;
  buffer << in.rdbuf();
  const auto text = buffer.str();
  check(text.find("\"commit\": \"7efd3ce") != std::string::npos, "contract commit");
  check(text.find("\"policy\": \"rta-quests-v1\"") != std::string::npos, "contract policy");
  check(text.find("docs/quests.md") != std::string::npos, "contract lists the quest doc");
  check(text.find("tools/ref_quests.py") != std::string::npos, "contract lists the reference");
}

void test_refusals_and_save() {
  line_no = 0;
  q::Campaign campaign;
  campaign.set_num_players(0);
  campaign.give_gold(5);
  check(campaign.record(0).gold == 0, "zero players do not divide gold");
  bool threw = false;
  try {
    campaign.mission_set_available(-1, 1);
  } catch (const q::Error &) {
    threw = true;
  }
  check(threw, "a negative mission is refused");
  campaign.set_num_players(1);
  campaign.set_difficulty(1);
  campaign.set_act(0);
  campaign.set_acts({{"hub", "blood1"}, {"hub", "fire1"}});
  campaign.quest_add("Alpha", "Body");
  campaign.var_set("Step", 7);
  campaign.mission_set_available(1, 1);
  campaign.set_evil_command(1);
  campaign.set_decision_command();
  auto record = campaign.record(0);
  record.bags[0].push_back({"pickaxe", 1ull << 39});
  campaign.set_record(0, std::move(record));
  const auto bytes = campaign.save();
  const auto loaded = q::Campaign::load(bytes);
  check(loaded.save() == bytes, "campaign image round-trips");
  check(loaded.record(0).gold == 0 && loaded.vars().size() == 1 && loaded.vars()[0].name == "step" &&
            loaded.vars()[0].value == 7 && loaded.act() == 0,
        "loaded campaign keeps the quest variable");
  threw = false;
  try {
    q::Campaign::load({1, 2, 3, 4});
  } catch (const q::Error &) {
    threw = true;
  }
  check(threw, "a short image is refused");

  flow::Acts one_empty = {{{"", 0}}};
  check(flow::find_act(one_empty, "fire1", 0) == 0, "level flow matches an empty prefix");
  check(q::find_act("fire1", 0, {{""}}) == -1, "the quest rule does not match an empty prefix");
}

q::Field raw_title() {
  q::Field field{};
  const char *text = "ABCDEFGHIJKLMNOPQRSTUV";
  for (std::size_t i = 0; text[i]; ++i)
    field[i] = static_cast<std::uint8_t>(text[i]);
  return field;
}

void test_level_change() {
  line_no = 0;
  sc::Host host;
  host.campaign().set_num_players(1);
  host.campaign().set_act(0);
  host.campaign().set_acts({{"hub"}, {""}});
  auto record = host.campaign().record(0);
  record.quest_count = 1;
  record.quests[0].title = q::field_from("Alpha");
  record.quests[0].text = q::field_from("Body");
  host.campaign().set_record(0, record);
  host.level().act = 0;
  host.level().acts = {{{"hub", 0}}, {{"", 0}}};
  host.level().players[0].quests.push_back("Alpha");
  host.level().quest_values["step"] = 4;
  host.level().pending.request("zz");
  const auto disagreed = host.change_level({});
  check(disagreed.reset_act, "level flow clears on an empty prefix");
  check(host.campaign().record(0).quest_count == 1, "the campaign keeps its list");
  check(host.campaign().act() == 0, "the campaign act stays");
  check(host.level().players[0].quests.empty() && host.level().quest_values["step"] == 0,
        "level flow cleared its own quest copy");

  sc::Host matched;
  matched.campaign().set_num_players(1);
  matched.campaign().set_act(0);
  matched.campaign().set_acts({{"hub", "blood1"}, {"hub", "fire1"}});
  record = matched.campaign().record(0);
  record.quest_count = 1;
  record.quests[0].title = raw_title();
  record.quests[0].text = q::field_from("Body");
  record.bags[0].push_back({"zzzz", 1ull << 39});
  record.bags[0].push_back({"pickaxe", 1ull << 39});
  matched.campaign().set_record(0, record);
  matched.campaign().var_set("step", 4);
  matched.level().act = 0;
  matched.level().acts = {{{"hub", 0}, {"blood1", 0}}, {{"hub", 0}, {"fire1", 0}}};
  matched.level().players[0].quests.push_back("Alpha");
  matched.level().pending.request("fire1");
  const auto changed = matched.change_level({});
  check(changed.act_changed && changed.reset_act, "a different act resets");
  check(matched.campaign().act() == 1, "the campaign stores the new act");
  const auto &cleared = matched.campaign().record(0);
  check(cleared.quest_count == 0, "the quest count is cleared");
  const auto title = cleared.quests[0].title;
  const char expect[] = {'N', 'o', 'Q', 'u', 'e', 's', 't', 's', 0, 'J', 'K', 'L', 'M', 'N'};
  bool prefix = true;
  for (std::size_t i = 0; i < sizeof(expect); ++i)
    prefix = prefix && title[i] == static_cast<std::uint8_t>(expect[i]);
  check(prefix, "the placeholder leaves the tail");
  check(cleared.bags[0].size() == 1 && cleared.bags[0][0].name == "pickaxe", "flag 39 items are filtered");
  check(matched.campaign().vars().size() == 1 && matched.campaign().vars()[0].value == 0,
        "quest variable values are cleared");
  check(matched.campaign().events().act_resets == 1 && matched.campaign().events().flag39_removed == 1,
        "the clear is counted");
  check(matched.level().players[0].quests.empty(), "level flow cleared its quest strings");

  sc::Host latched;
  latched.campaign().set_num_players(1);
  latched.campaign().set_act(0);
  latched.campaign().set_acts({{"hub", "blood1"}, {"hub", "fire1"}});
  latched.campaign().quest_add("Alpha", "Body");
  latched.level().act = 0;
  latched.level().acts = {{{"hub", 0}, {"blood1", 0}}, {{"hub", 0}, {"fire1", 0}}};
  latched.level().pending.request("fire1");
  flow::ChangeOptions options;
  options.card_latch = true;
  const auto held = latched.change_level(options);
  check(held.act_changed && !held.reset_act, "a card latch stores the act and keeps the list");
  check(latched.campaign().record(0).quest_count == 1, "the latched campaign is not cleared");

  sc::Host empty;
  empty.campaign().quest_add("Alpha", "Body");
  bool threw = false;
  try {
    empty.change_level({});
  } catch (const flow::Error &) {
    threw = true;
  }
  check(threw && empty.campaign().record(0).quest_count == 0, "no pending level does not clear");
  // quest_add wrote nothing because the player count is still 0. Plant one and retry.
  empty.campaign().set_num_players(1);
  empty.campaign().quest_add("Alpha", "Body");
  threw = false;
  try {
    empty.change_level({});
  } catch (const flow::Error &) {
    threw = true;
  }
  check(threw && empty.campaign().record(0).quest_count == 1, "a missing pending level leaves the list");
}

void test_host() {
  line_no = 0;
  const auto built = build_calls(
      {{"setCFlag", {2000, 0, 1}},
       {"addQuest", {2100, 2200}},
       {"setv", {2300, 7}},
       {"setMissionAvailable", {1, 1}},
       {"grantMission", {1}},
       {"setEvil", {1}},
       {"setMadeDecision", {}},
       {"setPlayerFlag", {-1, 0, 1}},
       {"givePlayerGold", {5}},
       {"activateMissionMenu", {}},
       {"countQuestItems", {2400}},
       {"removePlayerItem", {2400}},
       {"givePlayerItem", {2400}},
       {"givePlayerExp", {10}}},
      {{2000, "boss"}, {2100, "Alpha"}, {2200, "Body"}, {2300, "Step"}, {2400, "nope"}});
  sc::Host host;
  host.add_object("boss", 0x20u, {}, 0);
  check(host.load(built.image), "synthetic quest script loads");
  if (!host.machine())
    return;
  for (const auto &entry : built.strings)
    host.machine()->set_string(entry.first, entry.second, false);
  // Binder words from ref_natives. One player is a fixture for this script,
  // not a claimed boot value. Difficulty 1 selects bit 1.
  host.variables().memory.word(0x4d8ae0u, 1u);
  host.variables().memory.word(0x4f719cu, 1u);
  check(host.begin_level() == sc::Stop::Complete, "the quest script returns");
  check(host.counts().faults == 0, host.last_fault().empty() ? "no fault" : host.last_fault());
  const auto &campaign = host.campaign();
  check(campaign.num_players() == 1 && campaign.record(0).quest_count == 1, "addQuest mirrored");
  check(q::field_text(campaign.record(0).quests[0].title) == "Alpha" &&
            q::field_text(campaign.record(0).quests[0].text) == "Body",
        "quest text mirrored");
  check(campaign.vars().size() == 1 && campaign.vars()[0].name == "step" && campaign.vars()[0].value == 7,
        "setv mirrored");
  check(campaign.record(0).missions[static_cast<std::size_t>(q::MISSIONS + 1)] == 3,
        "mission 1 is available and complete");
  check(campaign.record(0).evil == 2 && campaign.record(0).decision == 2, "difficulty bit 1 is set");
  check(campaign.record(0).script_flags == 1 && campaign.record(0).gold == 5, "flag and gold mirrored");
  check(campaign.events().menu_opens == 1 && campaign.chooser() == 0, "the mission menu opened");
  check(campaign.events().cflag_bit0 == 1 && campaign.events().boss_music == 1, "bit 0 requests boss music");
  const char *refused[] = {"countQuestItems", "removePlayerItem", "givePlayerItem", "givePlayerExp"};
  for (const char *name : refused) {
    const auto index = command_index(name);
    check(index >= 0 && host.counts().refused[static_cast<std::size_t>(index)] == 1 &&
              host.counts().executed[static_cast<std::size_t>(index)] == 0,
          std::string("still refused ") + name);
  }

  sc::Host blocked;
  blocked.add_object("boss", 0x20u, {}, 0);
  const auto menu_only = build_calls({{"activateMissionMenu", {}}}, {});
  check(blocked.load(menu_only.image), "menu script loads");
  blocked.variables().memory.byte(0x41b51du, 1);
  check(blocked.begin_level() == sc::Stop::Complete, "a running recall still returns");
  check(blocked.campaign().events().menu_opens == 0, "recall byte blocks the menu");

  sc::Host experience;
  std::array<std::int32_t, q::XP_LEVELS> table{};
  for (int i = 0; i < 80; ++i)
    table[static_cast<std::size_t>(i)] = 100 * i;
  table[80] = 10000;
  experience.campaign().set_xp_table(table);
  const auto exp_script = build_calls({{"givePlayerExp", {10}}}, {});
  check(experience.load(exp_script.image), "experience script loads");
  experience.variables().memory.word(0x4d8ae0u, 1u);
  check(experience.begin_level() == sc::Stop::Complete, "givePlayerExp returns");
  check(experience.campaign().events().exp_grants == 1, "one experience grant");
  check(experience.campaign().record(0).level == 1 && experience.campaign().record(0).exp == 10 &&
            experience.campaign().record(0).skill == 1,
        "the synthetic table grants one level");
}
} // namespace

int main(int argc, char **argv) {
  if (argc != 3) {
    std::cerr << "usage: champions-quests-tests VECTORS CONTRACT\n";
    return 2;
  }
  replay(argv[1]);
  test_contract(argv[2]);
  test_refusals_and_save();
  test_level_change();
  test_host();
  if (failed) {
    std::cerr << failed << " failed\n";
    return 1;
  }
  std::cout << "quests ok\n";
  return 0;
}
