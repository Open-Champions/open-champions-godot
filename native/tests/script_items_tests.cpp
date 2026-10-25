// Contract tests for the item, gold, skill and monster-builder script
// commands (research-items 49bc578, docs/script-items.md). The vectors come
// from tools/generate_script_items_vectors.py, which runs the research's
// reference models on authored synthetic worlds. No game file is read.
#include "champions/amx.hpp"
#include "champions/core_state.hpp"
#include "champions/monster_build.hpp"
#include "champions/script_commands.hpp"
#include "champions/script_items.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace a = champions::simulation::rta::amx;
namespace b = champions::simulation::rta::builtins;
namespace si = champions::simulation::rta::script_items;
namespace mb = champions::simulation::rta::monster_build;
namespace sc = champions::simulation::rta::script_commands;
using champions::simulation::rta::Raw;

namespace {
int failures = 0;
int checks = 0;
void check(bool ok, const std::string &what) {
  ++checks;
  if (!ok) {
    ++failures;
    if (failures < 40)
      std::cerr << "FAIL " << what << '\n';
  }
}

std::vector<std::string> split(const std::string &line) {
  std::istringstream in(line);
  std::vector<std::string> out;
  std::string word;
  while (in >> word)
    out.push_back(word);
  return out;
}
std::uint64_t hex(const std::string &s) { return std::stoull(s, nullptr, 16); }
std::vector<std::uint8_t> hex_bytes(const std::string &s) {
  std::vector<std::uint8_t> out;
  if (s == "-")
    return out;
  for (std::size_t i = 0; i + 1 < s.size(); i += 2)
    out.push_back(static_cast<std::uint8_t>(std::stoul(s.substr(i, 2), nullptr, 16)));
  return out;
}
std::string hex_text(const std::string &token) {
  const auto bytes = hex_bytes(token);
  return std::string(bytes.begin(), bytes.end());
}

struct Case {
  int id = 0;
  std::string kind;
  std::vector<std::pair<std::uint32_t, std::uint32_t>> objects;
  std::vector<std::pair<std::uint32_t, std::vector<std::uint8_t>>> init;
  std::map<std::string, std::vector<std::string>> answers;
  std::vector<std::string> call;
  bool refused = false, fault = false;
  std::optional<std::int64_t> result;
  std::vector<std::vector<std::string>> log;
  std::vector<std::pair<std::uint32_t, std::vector<std::uint8_t>>> bytes;
};

struct Vectors {
  std::map<std::uint32_t, std::uint8_t> image;
  std::vector<Case> cases;
  std::vector<std::vector<std::string>> limits, fits, cm1_cached, cm1_scan;
  std::vector<std::string> arena_classes;
  std::size_t lines = 0;
};

Vectors read(const std::string &path) {
  std::ifstream in(path);
  Vectors v;
  std::string line;
  Case *open = nullptr;
  while (std::getline(in, line)) {
    ++v.lines;
    if (line.empty() || line[0] == '#')
      continue;
    auto f = split(line);
    const auto &tag = f[0];
    if (tag == "image") {
      const auto at = static_cast<std::uint32_t>(hex(f[1]));
      const auto data = hex_bytes(f[2]);
      for (std::size_t k = 0; k < data.size(); ++k)
        v.image[at + static_cast<std::uint32_t>(k)] = data[k];
    } else if (tag == "case") {
      v.cases.emplace_back();
      open = &v.cases.back();
      open->id = std::stoi(f[1]);
      open->kind = f[2];
    } else if (tag == "objects") {
      for (std::size_t k = 2; k < f.size(); ++k) {
        const auto colon = f[k].find(':');
        open->objects.emplace_back(static_cast<std::uint32_t>(hex(f[k].substr(0, colon))),
                                   static_cast<std::uint32_t>(hex(f[k].substr(colon + 1))));
      }
    } else if (tag == "init") {
      open->init.emplace_back(static_cast<std::uint32_t>(hex(f[1])), hex_bytes(f[2]));
    } else if (tag == "answer") {
      open->answers[f[1]] = std::vector<std::string>(f.begin() + 2, f.end());
    } else if (tag == "call") {
      open->call = std::vector<std::string>(f.begin() + 1, f.end());
    } else if (tag == "expect_refused") {
      open->refused = true;
    } else if (tag == "expect_fault") {
      open->fault = true;
    } else if (tag == "expect_result") {
      open->result = std::stoll(f[1]);
    } else if (tag == "expect_log") {
      open->log.emplace_back(f.begin() + 1, f.end());
    } else if (tag == "expect_bytes") {
      open->bytes.emplace_back(static_cast<std::uint32_t>(hex(f[1])), hex_bytes(f[2]));
    } else if (tag == "end") {
      open = nullptr;
    } else if (tag == "limit") {
      v.limits.push_back(f);
    } else if (tag == "fits") {
      v.fits.push_back(f);
    } else if (tag == "cm1_cached") {
      v.cm1_cached.push_back(f);
    } else if (tag == "cm1_scan") {
      v.cm1_scan.push_back(f);
    } else if (tag == "arena_class") {
      v.arena_classes.push_back(f[2]);
    }
  }
  return v;
}

std::string word_text(std::uint64_t v) {
  std::ostringstream out;
  out << std::hex << (v & 0xffffffffu);
  return out.str();
}
std::vector<std::vector<std::string>> logged(const std::vector<si::Service::Call> &calls) {
  std::vector<std::vector<std::string>> out;
  for (const auto &call : calls) {
    std::vector<std::string> line{call.name};
    for (auto arg : call.args)
      line.push_back(word_text(arg));
    out.push_back(line);
  }
  return out;
}

struct World {
  std::shared_ptr<b::Memory> memory;
  std::unique_ptr<b::Variables> variables;
  std::unique_ptr<si::Service> service;
};
World build(const Vectors &v, const Case &c, const std::shared_ptr<champions::simulation::rta::items::Tables> &tables) {
  World w;
  const auto *image = &v.image;
  w.memory = std::make_shared<b::Memory>([image](std::uint32_t at, std::size_t n) {
    std::vector<std::uint8_t> out(n);
    for (std::size_t k = 0; k < n; ++k) {
      const auto found = image->find(at + static_cast<std::uint32_t>(k));
      out[k] = found == image->end() ? 0 : found->second;
    }
    return out;
  });
  w.variables = std::make_unique<b::Variables>(w.memory);
  w.variables->local_player_slot = [] { return a::Cell(0); };
  w.service = std::make_unique<si::Service>(*w.variables);
  w.service->bind_tables(tables);
  for (const auto &obj : c.objects)
    w.variables->new_object({}, obj.second, {}, 0, "10GameObject", {}, obj.first);
  for (const auto &entry : c.init)
    w.memory->write(entry.first, entry.second);
  w.memory->writes.clear();
  for (const auto *name : {"net_isLocalSlot", "pool_new", "skill_lightQuiverComplement", "skill_axeBandolierComplement",
                           "vars_get", "text_lookup", "player_countPotions", "player_recalcStats",
                           "player_resetIdleAnim", "vars_set", "hud_showHelpMessage", "inventory_selectIdleAnim",
                           "inventory_equipItem", "ItemProp_ctor2"}) {
    // givePlayerRandom's vectors run the ground item itself (one shared and one
    // own draw, the block bytes), so it is not delegated there.
    if (c.kind == "givePlayerRandom" && std::string(name) == "ItemProp_ctor2")
      continue;
    std::uint64_t answer = 0;
    const auto found = c.answers.find(name);
    if (found != c.answers.end())
      answer = std::stoull(found->second.at(0));
    w.service->delegates[name] = [answer](const std::vector<std::uint64_t> &) { return answer; };
  }
  return w;
}

// The binder handler run through an AMX machine whose argument cells point at
// the strings.
std::int64_t dispatch(b::Variables &variables, const std::vector<std::string> &call) {
  a::ImageOptions io;
  io.data.resize(4096);
  io.stack_cells = 128;
  a::Machine machine(a::build_image({0}, io));
  if (machine.load() != 0)
    throw std::runtime_error("machine");
  machine.params = 3000;
  for (std::size_t k = 1; k < call.size(); ++k) {
    const auto &token = call[k];
    const auto cell = 3000 + 4 * static_cast<a::Address>(k);
    if (token[0] == 's') {
      const auto at = static_cast<a::Address>(400 * (k - 1));
      machine.set_string(at, hex_text(token.substr(1)), false);
      machine.set_cell(cell, at);
    } else {
      machine.set_cell(cell, static_cast<std::uint64_t>(static_cast<std::uint32_t>(std::stoll(token.substr(1)))));
    }
  }
  return variables.dispatch(call[0], machine);
}

void compare_bytes(const Case &c, const b::Memory &after, const b::Memory &twin) {
  std::set<std::uint32_t> expected;
  for (const auto &run : c.bytes)
    for (std::size_t k = 0; k < run.second.size(); ++k) {
      const auto at = run.first + static_cast<std::uint32_t>(k);
      expected.insert(at);
      check(after.u8(at) == run.second[k], "case " + std::to_string(c.id) + " " + c.kind + " byte 0x" + word_text(at));
    }
  for (const auto &write : after.writes)
    for (std::size_t k = 0; k < write.second; ++k) {
      const auto at = write.first + static_cast<std::uint32_t>(k);
      if (!expected.count(at))
        check(after.u8(at) == twin.u8(at),
              "case " + std::to_string(c.id) + " " + c.kind + " stray write 0x" + word_text(at));
    }
}

void run_handler(const Vectors &v, const Case &c, const std::shared_ptr<champions::simulation::rta::items::Tables> &tables) {
  auto w = build(v, c, tables);
  auto twin = build(v, c, tables);
  const auto label = "case " + std::to_string(c.id) + " " + c.kind;
  bool refused = false, faulted = false;
  std::int64_t result = 0;
  std::string why;
  try {
    result = dispatch(*w.variables, c.call);
  } catch (const si::Refused &error) {
    refused = true;
    why = error.what();
  } catch (const std::exception &error) {
    faulted = true;
    why = error.what();
  }
  if (c.refused) {
    check(refused, label + " refuses");
    return;
  }
  if (c.fault) {
    check(refused || faulted, label + " faults");
    return;
  }
  check(!refused && !faulted, label + " runs (" + why + ")");
  if (refused || faulted)
    return;
  check(result == c.result.value_or(0), label + " result");
  check(logged(w.service->calls) == c.log, label + " call log");
  compare_bytes(c, *w.memory, *twin.memory);
}

void run_recalc_load(const Vectors &v, const Case &c, const std::shared_ptr<champions::simulation::rta::items::Tables> &tables) {
  auto w = build(v, c, tables);
  std::vector<si::Service::Call> calls;
  si::Complements comp;
  const auto lq = std::stoull(c.answers.at("skill_lightQuiverComplement").at(0));
  const auto ab = std::stoull(c.answers.at("skill_axeBandolierComplement").at(0));
  comp.light_quiver = [&](std::uint32_t obj, std::int32_t rank) {
    calls.push_back({"skill_lightQuiverComplement", {obj, std::uint32_t(rank)}});
    return Raw(lq);
  };
  comp.axe_bandolier = [&](std::uint32_t obj, std::int32_t rank) {
    calls.push_back({"skill_axeBandolierComplement", {obj, std::uint32_t(rank)}});
    return Raw(ab);
  };
  si::BinderView view(*w.memory);
  const auto slot = std::stoi(c.call.at(1).substr(1));
  si::recalc_load(view, tables.get(), comp, slot);
  const auto label = "case " + std::to_string(c.id) + " recalcLoad";
  check(w.memory->word(si::rec(slot) + si::P_LOAD) == static_cast<std::uint32_t>(c.result.value_or(0)), label + " load");
  check(logged(calls) == c.log, label + " complement calls");
}

void run_arena(const Vectors &v, const Case &c) {
  b::Memory mem;
  b::Memory twin;
  for (const auto &entry : c.init) {
    mem.write(entry.first, entry.second);
    twin.write(entry.first, entry.second);
  }
  mem.writes.clear();
  std::vector<si::Service::Call> calls;
  const auto misses = std::stoi(c.answers.at("misses").at(0));
  const auto &spot_words = c.answers.at("spot");
  const mb::Vec3 spot{Raw(hex(spot_words[0])), Raw(hex(spot_words[1])), Raw(hex(spot_words[2]))};
  const auto object = static_cast<std::uint32_t>(std::stoull(c.answers.at("object").at(0)));
  const auto block = static_cast<std::uint32_t>(std::stoull(c.answers.at("block").at(0)));
  int tries = 0;
  mb::ArenaCalls calls_out;
  calls_out.find_free_spot = [&](const mb::Vec3 &point, std::int32_t range, std::int32_t radius, std::int32_t flag) {
    calls.push_back({"world_findFreeSpot", {point[0], point[1], point[2], std::uint32_t(range), std::uint32_t(radius), std::uint32_t(flag)}});
    ++tries;
    return std::make_pair(spot, std::uint32_t(tries <= misses ? 0 : 1));
  };
  std::vector<std::string> created;
  calls_out.create_object = [&](const std::string &name, const mb::Vec3 &at, std::int32_t facing, std::uint32_t props) {
    calls.push_back({"world_createObject", {at[0], at[1], at[2], std::uint32_t(facing), props}});
    created.push_back(name);
    return object;
  };
  calls_out.refused_class = [](std::uint32_t) { return false; };
  calls_out.scale_stats = [&](std::uint32_t obj, const std::array<Raw, 5> &s) {
    calls.push_back({"Creature_scaleStats", {obj, s[0], s[1], s[2], s[3], s[4]}});
  };
  calls_out.clear_status = [&](std::uint32_t obj) { calls.push_back({"Creature_clearStatus", {obj}}); };
  calls_out.pool_new = [&](std::uint32_t size) {
    calls.push_back({"pool_new", {size}});
    return block;
  };
  calls_out.add_spell = [&](std::uint32_t obj, std::uint32_t spell) { calls.push_back({"creature_addSpell", {obj, spell}}); };
  calls_out.virtual_call = [&](int slot, std::uint32_t obj, std::uint32_t value) {
    calls.push_back({"virtual", {std::uint32_t(slot), obj, value}});
  };
  const auto label = "case " + std::to_string(c.id) + " arenaSpawn";
  std::optional<std::string> name;
  std::uint32_t creature = 0;
  if (c.call.at(1)[0] == 's')
    name = hex_text(c.call[1].substr(1));
  else
    creature = static_cast<std::uint32_t>(std::stoll(c.call.at(2).substr(1)));
  bool refused = false;
  std::optional<std::uint32_t> result;
  try {
    result = mb::arena_spawn_monster(mem, calls_out, name, creature, v.arena_classes);
  } catch (const si::Refused &) {
    refused = true;
  }
  if (c.fault) {
    check(refused, label + " refuses the original's fault");
    return;
  }
  check(!refused, label + " runs");
  if (refused)
    return;
  check(result.value_or(0) == static_cast<std::uint32_t>(c.result.value_or(0)), label + " result");
  // The reference logs the class name inside world_createObject's arguments.
  auto expected = c.log;
  for (auto &line : expected)
    if (line[0] == "world_createObject" && line.size() > 1 && line[1][0] == 's') {
      check(!created.empty() && hex_text(line[1].substr(1)) == created.front(), label + " class name");
      line.erase(line.begin() + 1);
    }
  check(logged(calls) == expected, label + " call log");
  compare_bytes(c, mem, twin);
}

void run_generator(const Case &c) {
  auto memory = std::make_shared<b::Memory>();
  b::Variables variables(memory);
  si::Service service(variables);
  b::Memory twin;
  for (const auto &entry : c.init) {
    memory->write(entry.first, entry.second);
    twin.write(entry.first, entry.second);
  }
  memory->writes.clear();
  std::vector<si::Service::Call> calls;
  const auto object = static_cast<std::uint32_t>(std::stoull(c.answers.at("object").at(0)));
  const auto dynamic = c.answers.at("dynamic").at(0);
  std::string created;
  service.create_object = [&](const std::string &name, const std::array<std::uint32_t, 3> &pos, std::int32_t angle,
                              std::uint32_t props) {
    created = name;
    calls.push_back({"world_createObject", {pos[0], pos[1], pos[2], std::uint32_t(angle), props}});
    return object;
  };
  service.dynamic_type = [&](std::uint32_t) { return dynamic; };
  const auto label = "case " + std::to_string(c.id) + " generatorSpawn";
  const auto type_name = hex_text(c.call.at(1).substr(1));
  bool refused = false;
  std::uint32_t result = 0;
  try {
    result = service.generator_spawn(0x01a00000, 0x5d32c8, type_name);
  } catch (const si::Refused &) {
    refused = true;
  }
  if (c.refused) {
    check(refused, label + " refuses the Skeleton rise");
    return;
  }
  check(!refused, label + " runs");
  check(result == static_cast<std::uint32_t>(c.result.value_or(0)), label + " result");
  auto expected = c.log;
  for (auto &line : expected)
    if (line[0] == "world_createObject") {
      check(hex_text(line[1].substr(1)) == created, label + " type name");
      line.erase(line.begin() + 1);
    }
  check(logged(calls) == expected, label + " call log");
  compare_bytes(c, *memory, twin);
}

// generateMonster's interning half, through the binder handler with a
// generator name no object has.
void run_intern(const Case &c) {
  auto memory = std::make_shared<b::Memory>();
  b::Variables variables(memory);
  b::Memory twin;
  for (const auto &entry : c.init) {
    memory->write(entry.first, entry.second);
    twin.write(entry.first, entry.second);
  }
  memory->writes.clear();
  std::vector<std::string> call{"generateMonster", "s" + std::string("6e6f2067656e657261746f72"), c.call.at(1)};
  dispatch(variables, call);
  compare_bytes(c, *memory, twin);
}

// ---- synthetic tests -----------------------------------------------------------------------
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
      if (!call.native.empty() && call.native[0] == '.') {
        std::vector<a::AsmValue> operands(call.args.begin(), call.args.end());
        ins(call.native.substr(1), operands);
        continue;
      }
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
  for (const auto &entry : built.strings)
    host.machine()->set_string(entry.first, entry.second, false);
}
std::size_t index_of(const char *name) {
  for (std::size_t i = 0; i < sc::COMMANDS; ++i)
    if (std::string(sc::row(i).name) == name)
      return i;
  throw std::runtime_error(name);
}

void synthetic_tests() {
  // The executable image's g_difficultyIdx (1) and cm1 word (-1).
  {
    sc::Host host;
    check(host.variables().memory.s32(si::G_DIFFICULTY) == si::DIFFICULTY_IMAGE, "host starts g_difficultyIdx at 1");
    check(host.campaign().difficulty() == si::DIFFICULTY_IMAGE, "campaign starts at difficulty 1");
    check(host.variables().memory.s32(si::G_CM1_WORD) == si::CM1_IMAGE, "cm1 word holds the image's -1");
    check(si::scan_cm1_vars(0, {}, host.variables().memory.word(si::G_CM1_WORD)) == -1, "game_scanCm1Vars(0) reads -1");
  }
  // clearAllSkills reaches only the unused row of difficulty 0 (docs/script-items.md 3).
  {
    sc::Host host;
    auto &v = host.variables();
    v.memory.word(si::G_NUM_PLAYERS, 1);
    v.memory.byte(si::G_MEDAL_PLAYER, 0);
    auto built = build_calls({{"start", {{"clearAllSkills", {}}}}}, {});
    plant(host, built);
    host.run_prestart();
    host.run_start();
    check(host.counts().executed[index_of("clearAllSkills")] == 1, "clearAllSkills executes");
    for (int d = 1; d <= 5; ++d)
      for (int m = 0; m < 13; ++m)
        check(v.mission_available(d, m) == 0 && v.mission_complete(d, m) == 0,
              "difficulty " + std::to_string(d) + " mission " + std::to_string(m) + " untouched by clearAllSkills");
    check(v.mission_available(0, 0) == 1 && v.mission_complete(0, 8) == 1, "row 0 lies on the cleared skill pairs");
    check(v.mission_available(0, 9) == 0, "row 0 past the pairs is not cleared");
    // The binder's mission reads go through g_difficultyIdx, which is 1.
    check(v.memory.s32(si::G_DIFFICULTY) == 1, "difficulty stays 1");
  }
  // The item commands pass the refusal gate; a count with nobody runs.
  {
    sc::Host host;
    host.variables().memory.word(si::G_NUM_PLAYERS, 1);
    auto built = build_calls({{"start", {{"playerHasItem", {2000}}, {"countQuestItems", {2000}},
                                         {"removePlayerItem", {a::Cell(-1), 2000}}, {"removeAllItems", {}},
                                         {"removeAllWeapons", {}}, {"onePlayerHasItem", {0, 2000}}}}},
                             {{2000, "Key"}});
    plant(host, built);
    host.run_prestart();
    host.run_start();
    for (const auto *name : {"playerHasItem", "countQuestItems", "removePlayerItem", "removeAllItems",
                             "removeAllWeapons", "onePlayerHasItem"})
      check(host.counts().executed[index_of(name)] == 1 && host.counts().refused[index_of(name)] == 0,
            std::string(name) + " executes in the host");
    check(!host.stopped_on_refusal(), "item counts do not stop the entry");
  }
  // A give needs a bound seed and the item tables; without them it refuses and,
  // when its answer is used, stops the entry.
  {
    sc::Host host;
    host.variables().memory.word(si::G_NUM_PLAYERS, 1);
    auto built = build_calls({{"start", {{"givePlayerItem", {2000, 0, 0}}}}}, {{2000, "Key"}}, true);
    plant(host, built);
    host.run_prestart();
    const auto stop = host.run_start();
    check(host.counts().refused[index_of("givePlayerItem")] == 1, "a give with no seed refuses");
    check(stop == sc::Stop::Refused, "the used refused answer stops the entry");
  }
  {
    sc::Host host;
    champions::simulation::rta::SharedRandom seed;
    host.bind_seed(&seed);
    host.variables().memory.word(si::G_NUM_PLAYERS, 1);
    auto built = build_calls({{"start", {{"givePlayerItem", {2000, 0, 0}}}}}, {{2000, "Key"}});
    plant(host, built);
    host.run_prestart();
    host.run_start();
    check(host.counts().refused[index_of("givePlayerItem")] == 1, "a give with no item tables refuses");
    check(!host.stopped_on_refusal(), "an unused refused answer does not stop the entry");
  }
  // Service defaults: a ground item without resource and bounds services,
  // a bad equip address and missing complements refuse.
  {
    auto memory = std::make_shared<b::Memory>();
    b::Variables v(memory);
    si::Service s(v);
    bool refused = false;
    try {
      s.call("pool_new", {400u});
    } catch (const si::Refused &) {
      refused = true;
    }
    check(refused, "ground item refuses before pool allocation");
    refused = false;
    try {
      s.call("ItemProp_ctor2", {0x01300000u, std::array<std::uint32_t, 3>{0, 0, 0}});
    } catch (const si::Refused &) {
      refused = true;
    }
    check(refused, "ground item refuses without resource and bounds services");
    refused = false;
    try {
      s.call("inventory_equipItem", {0u, si::bag_entry(0, 0, -1), 0u});
    } catch (const si::Refused &) {
      refused = true;
    }
    check(refused, "equipping the address before the bag refuses");
    refused = false;
    try {
      s.call("item_parseDescriptor", {std::string("Key"), b::Variables::ItemBlank{}, UINT32_MAX, 0u});
    } catch (const si::Refused &) {
      refused = true;
    }
    check(refused, "parse without tables refuses");
    check(si::item_weight(si::BinderView(*memory), nullptr, {}, 0, 0) == champions::simulation::rta::ONE,
          "a null item weighs 1.0");
  }
  // The load-limit walk: the object test, and the passive asked for the object's slot.
  {
    b::Memory mem;
    si::BinderView v(mem);
    mem.word(si::G_NUM_PLAYERS, 2);
    const auto r0 = si::rec(0), r1 = si::rec(1);
    mem.word(r0 + si::P_LOAD_LIMIT, 0x12345678);
    mem.word(r1 + si::P_OBJECT, 0x01200000);
    mem.word(0x01200000 + si::OBJ_SLOT, 0);           // the object names slot 0
    mem.word(r1 + si::P_EFF_STRENGTH, 46);
    mem.word(r0 + si::P_CLASS, 2);
    mem.word(r1 + si::P_CLASS, 3);
    mem.w64(si::G_CLASS_SKILLS + 2 * si::CLASS_SKILLS_SIZE + 4 * si::CLASS_SKILL_ROW, si::PASSIVE_LOAD);
    mem.byte(r0 + si::P_PASSIVES + 4, 2);              // slot 0's level: 2
    mem.word(si::G_LOAD_BONUS, 1); // no writer exists: the walk must not read it
    si::recalc_load_limits(v);
    check(mem.word(r0 + si::P_LOAD_LIMIT) == 0x12345678, "a record with no object keeps its limit");
    check(mem.word(r1 + si::P_LOAD_LIMIT) == si::load_limit(46, 2, 0),
          "the passive is the object's slot's, and the bonus word is the constant 0");
    check(si::LOAD_BONUS_VALUE == 0 && si::load_limit(46, 2, 1) != si::load_limit(46, 2, 0),
          "the bonus branch is kept for parity but never taken");
    check(si::load_limit(46, 0, 0) == 0x432b0000, "171.0");
    check(si::load_limit(9000, 0, 1) == si::F_9999, "+5000 then the 9999 cap");
  }
  // game_set4f7110: -1 rescans this machine's profile.
  {
    b::Memory mem;
    si::set_cm1_word(mem, -1, [](const std::string &n) { return n == "m8cm1" ? 1 : 0; });
    check(mem.s32(si::G_CM1_WORD) == 2, "rescan picks m8cm1, index 2");
    si::set_cm1_word(mem, 5, {});
    check(mem.s32(si::G_CM1_WORD) == 5, "a value is stored as it is");
  }
  // ScriptGenerator_spawn without a factory refuses.
  {
    auto memory = std::make_shared<b::Memory>();
    b::Variables v(memory);
    si::Service s(v);
    bool refused = false;
    try {
      s.generator_spawn(0x01a00000, 0x5d32c8, "Orc");
    } catch (const si::Refused &) {
      refused = true;
    }
    check(refused, "no factory: the spawn refuses");
  }
}

std::string sha_field(const std::string &json, const std::string &key) {
  const auto at = json.find("\"" + key + "\"");
  if (at == std::string::npos)
    return {};
  const auto colon = json.find(':', at);
  const auto end = json.find_first_of(",}", colon);
  auto value = json.substr(colon + 1, end - colon - 1);
  value.erase(std::remove_if(value.begin(), value.end(), [](char ch) { return ch == ' ' || ch == '"' || ch == '\n'; }),
              value.end());
  return value;
}
} // namespace

int main(int argc, char **argv) {
  if (argc < 3) {
    std::cerr << "usage: champions-script-items-tests VECTORS CONTRACT\n";
    return 2;
  }
  const auto v = read(argv[1]);
  std::ifstream contract_in(argv[2]);
  const std::string contract((std::istreambuf_iterator<char>(contract_in)), std::istreambuf_iterator<char>());
  check(sha_field(contract, "policy") == si::POLICY, "contract policy");
  check(sha_field(contract, "lines") == std::to_string(v.lines), "vector line count matches the contract");
  const auto image = v.image;
  auto tables = si::image_tables([&image](std::uint32_t at, std::size_t n) {
    std::vector<std::uint8_t> out(n);
    for (std::size_t k = 0; k < n; ++k) {
      const auto found = image.find(at + static_cast<std::uint32_t>(k));
      out[k] = found == image.end() ? 0 : found->second;
    }
    return out;
  });
  std::map<std::string, int> ran;
  for (const auto &c : v.cases) {
    ran[c.kind] += 1;
    if (c.kind == "recalcLoad")
      run_recalc_load(v, c, tables);
    else if (c.kind == "itemWeightNull")
      check(si::item_weight(si::BinderView(*std::make_shared<b::Memory>()), nullptr, {}, 0, 0) ==
                static_cast<Raw>(c.result.value_or(0)),
            "null item weight");
    else if (c.kind == "arenaSpawn")
      run_arena(v, c);
    else if (c.kind == "generatorSpawn")
      run_generator(c);
    else if (c.kind == "intern")
      run_intern(c);
    else
      run_handler(v, c, tables);
  }
  for (const auto &f : v.limits)
    check(si::load_limit(std::stoi(f[1]), std::stoi(f[2]), static_cast<std::int32_t>(std::stoll(f[3]))) ==
              static_cast<Raw>(std::stoull(f[4])),
          "load limit " + f[1] + " " + f[2] + " " + f[3]);
  for (const auto &f : v.fits) {
    b::Memory mem;
    mem.word(si::rec(0) + si::P_LOAD, static_cast<std::uint32_t>(std::stoull(f[1])));
    mem.word(si::rec(0) + si::P_LOAD_LIMIT, static_cast<std::uint32_t>(std::stoull(f[2])));
    check(si::fits(si::BinderView(mem), 0, static_cast<Raw>(std::stoull(f[3])), f[4] == "1") == (f[5] == "1"), "fits");
  }
  for (const auto &f : v.cm1_cached)
    check(si::scan_cm1_vars(0, {}, static_cast<std::uint32_t>(std::stoull(f[1]))) == std::stoi(f[2]), "cm1 cached");
  for (const auto &f : v.cm1_scan) {
    std::map<std::string, int> values;
    std::istringstream in(f[1]);
    std::string pair;
    while (std::getline(in, pair, ','))
      values[pair.substr(0, pair.find('='))] = std::stoi(pair.substr(pair.find('=') + 1));
    std::string asked;
    const auto index = si::scan_cm1_vars(1, [&](const std::string &n) {
      asked += (asked.empty() ? "" : ",") + n;
      return values.at(n);
    }, 0);
    check(index == std::stoi(f[2]) && asked == f[3], "cm1 scan order and index");
  }
  synthetic_tests();
  if (argc > 3) {
    // The former random-giver vector set remains a research artifact, not a
    // claim that the native handler can execute an unknown stack record.
    auto memory = std::make_shared<b::Memory>();
    b::Variables variables(memory);
    si::Service service(variables);
    bool refused = false;
    try {
      service.call("level_monsterLevelTimesDifficulty", {});
    } catch (const si::Refused &) {
      refused = true;
    }
    check(refused && service.record()[0] == 0, "random giver refuses before record mutation");
  }
  for (const auto &entry : ran)
    std::cout << entry.first << ' ' << entry.second << '\n';
  std::cout << "checks " << checks << " failures " << failures << '\n';
  return failures == 0 ? 0 : 1;
}
