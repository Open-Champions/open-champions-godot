#include "champions/amx.hpp"
#include "champions/core_arithmetic.hpp"
#include "champions/core_state.hpp"
#include "champions/native_audio.hpp"
#include "champions/script_commands.hpp"
#include "champions/script_effects.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace sc = champions::simulation::rta::script_commands;
namespace se = champions::simulation::rta::script_effects;
namespace builtins = champions::simulation::rta::builtins;
namespace a = champions::simulation::rta::amx;
namespace audio = champions::audio;
using champions::simulation::rta::SharedRandom;

namespace {
int failed = 0;
void check(bool ok, const std::string &what) {
  if (ok)
    return;
  std::cerr << "FAIL " << what << '\n';
  ++failed;
}
int index_of(const char *name) {
  for (std::size_t i = 0; i < sc::COMMANDS; ++i)
    if (std::strcmp(sc::row(i).name, name) == 0)
      return static_cast<int>(i);
  return -1;
}
const char *effect_name(sc::Effect effect) {
  if (effect == sc::Effect::Documented)
    return "documented";
  if (effect == sc::Effect::Partial)
    return "partly documented";
  return "unknown";
}

struct Built {
  a::ByteVector image;
  std::vector<std::pair<unsigned, std::string>> strings;
};
struct Call {
  std::string native;
  std::vector<a::Cell> args;
};
// A call whose native starts with '.' is one raw instruction with its args as
// operands. Without keep_result an entry ends ZERO.pri, RETN, as a Pawn
// function without a return value does; with it, RETN returns the last PRI.
Built build_calls(const std::vector<std::pair<std::string, std::vector<Call>>> &publics,
                  const std::vector<std::pair<unsigned, std::string>> &strings,
                  bool keep_result = false) {
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
// The list head the object's grid node is linked into: walk back to the cell.
const champions::simulation::rta::Link *grid_cell(const champions::simulation::rta::objects::GameObject *obj) {
  const champions::simulation::rta::Link *link = obj && obj->heads[2].next ? obj->heads[2].next->partner : nullptr;
  while (link && link->kind != champions::simulation::rta::LinkKind::Cell)
    link = link->prev;
  return link;
}
bool off_grid(sc::Host &host, const champions::simulation::rta::objects::GameObject *obj) {
  return grid_cell(obj) == &host.variables().world->off_grid;
}
void plant(sc::Host &host, const Built &built) {
  check(host.load(built.image), "synthetic script loads");
  if (!host.machine())
    return;
  for (const auto &entry : built.strings)
    host.machine()->set_string(entry.first, entry.second, false);
}

// One synthetic objects.ob record named gate. The class string is an authored
// placeholder, not a game class.
std::vector<std::uint8_t> gate_objects(const char *cls = "Stub") {
  std::vector<std::uint8_t> blob;
  auto u16 = [&](std::uint16_t v) {
    blob.push_back(static_cast<std::uint8_t>(v));
    blob.push_back(static_cast<std::uint8_t>(v >> 8));
  };
  auto u32 = [&](std::uint32_t v) {
    for (int shift = 0; shift < 32; shift += 8)
      blob.push_back(static_cast<std::uint8_t>(v >> shift));
  };
  u16(1);
  u16(0);
  u32(40);
  u32(0);
  u16(32);
  u16(0);
  u32(0);
  u32(0);
  u32(0);
  u32(static_cast<std::uint32_t>(std::strlen(cls) + 1));
  u32(0);
  u32(0);
  const char *prop = "name=gate";
  blob.insert(blob.end(), cls, cls + std::strlen(cls) + 1);
  blob.insert(blob.end(), prop, prop + std::strlen(prop) + 1);
  return blob;
}

void test_table(const std::string &path) {
  std::ifstream in(path);
  check(static_cast<bool>(in), "vector file opens");
  std::string header;
  std::getline(in, header);
  check(header == "index\tname\tcalls\tlevels\tstatus\tsubsystem\tnative\tseam", "vector header");
  std::map<std::string, int> documented_by_subsystem;
  int documented = 0, partial = 0, unknown = 0, used = 0;
  long calls = 0;
  int rows = 0;
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty())
      continue;
    std::vector<std::string> fields;
    std::stringstream stream(line);
    std::string field;
    while (std::getline(stream, field, '\t'))
      fields.push_back(field);
    check(fields.size() == 8, "vector row width");
    if (fields.size() != 8 || rows >= static_cast<int>(sc::COMMANDS))
      continue;
    const auto &row = sc::row(static_cast<std::size_t>(rows));
    check(std::to_string(row.index) == fields[0], "vector index");
    check(row.name == fields[1], "vector name");
    check(effect_name(row.effect) == fields[4], "vector status " + fields[1]);
    check(row.subsystem == fields[5], "vector subsystem " + fields[1]);
    check(row.native == fields[6], "vector native " + fields[1]);
    check(row.seam == fields[7], "vector seam " + fields[1]);
    const auto n = std::stol(fields[2]);
    calls += n;
    if (n)
      ++used;
    if (row.effect == sc::Effect::Documented) {
      ++documented;
      documented_by_subsystem[row.subsystem] += 1;
    } else if (row.effect == sc::Effect::Partial) {
      ++partial;
    } else {
      ++unknown;
    }
    ++rows;
  }
  check(rows == 191, "191 commands");
  check(calls == 5660, "5660 published calls");
  check(documented == 81 && partial == 70 && unknown == 40, "status census");
  check(used == 122 && 191 - used == 69, "used and never-called census");
  check(documented_by_subsystem["script-binder"] == 74, "documented binder commands");
  check(documented_by_subsystem["objects"] == 4, "documented object commands");
  check(documented_by_subsystem["session"] == 1, "documented session commands");
  check(documented_by_subsystem["fx"] == 1, "documented fx commands");
  check(documented_by_subsystem["level-flow"] == 1, "documented level-flow commands");
  check(documented_by_subsystem["combat-runtime"] == 0, "no documented combat command");
  check(documented_by_subsystem["creature-ai"] == 0, "no documented creature command");
  check(documented_by_subsystem["audio"] == 0, "no documented audio command");
  check(documented_by_subsystem["hud"] == 0, "no documented hud command");
}

void test_variables_and_seed() {
  const auto setv = index_of("setv");
  const auto getv = index_of("getv");
  const auto rand = index_of("getRand");
  check(setv >= 0 && getv >= 0 && rand >= 0, "binder indexes");
  auto built = build_calls({{"start", {{"setv", {2000, 7}}, {"getv", {2000}}, {"getRand", {}}}}},
                           {{2000, "quest"}}, true);
  sc::Host host;
  SharedRandom live(20973);
  host.bind_seed(&live);
  plant(host, built);
  check(host.run("start") == sc::Stop::Complete, "variable script completes");
  check(host.variables().quest_get("quest") == 7, "setv writes the quest cell");
  // The drawn value and the seed after it are compared with the reference in
  // test_host_vectors (case seed_draw).
  check(host.variables().memory.word(0x404464) == live.state, "seed word matches the shared generator");
  check(host.counts().executed[static_cast<std::size_t>(setv)] == 1, "setv executed");
  check(host.counts().executed[static_cast<std::size_t>(getv)] == 1, "getv executed");
  check(host.counts().executed[static_cast<std::size_t>(rand)] == 1, "getRand executed");
  check(host.counts().faults == 0, "documented variable commands do not fault");
}

void test_seed_refused_without_session_seed() {
  const auto rand = static_cast<std::size_t>(index_of("getRand"));
  auto built = build_calls({{"start", {{"getRand", {}}}}}, {});
  sc::Host host;
  plant(host, built);
  host.run("start");
  check(host.counts().executed[rand] == 0 && host.counts().refused[rand] == 1,
        "getRand is refused while no session seed is bound");
  check(host.variables().world->shared.state == SharedRandom{}.state,
        "no host-private generator is drawn");
  sc::Host stubbed;
  SharedRandom live(20973);
  stubbed.bind_seed(&live);
  plant(stubbed, built);
  const auto blob = gate_objects();
  stubbed.adopt_objects(blob.data(), blob.size());
  stubbed.run("start");
  check(stubbed.counts().executed[rand] == 0 && stubbed.counts().refused[rand] == 1,
        "getRand is refused once an unconstructed object is adopted");
  check(live.state == 20973u, "the refused draw leaves the session seed alone");
}

void test_entrance_grid_and_particle() {
  const auto entrance = index_of("setEntrance");
  const auto hide = index_of("hideMonster");
  const auto show = index_of("showMonster");
  const auto particle = index_of("addParticleOnObject");
  auto built = build_calls(
      {{"start",
        {{"setEntrance", {2000}},
         {"hideMonster", {2200}},
         {"showMonster", {2200}},
         {"hideMonster", {2200}},
         {"addParticleOnObject", {0, 2200, 0, 0, 0, 1000, 1000, 1000}}}}},
      {{2000, "north"}, {2200, "gate"}});
  sc::Host host;
  plant(host, built);
  check(host.run_prestart() == sc::Stop::Missing, "no preStart");
  host.add_object("gate", 0, {}, 0);
  check(host.counts().objects_registered == 1, "one named object");
  check(host.run_start() == sc::Stop::Complete, "entrance script completes");
  check(host.level().entrance == "north", "setEntrance copies the level-flow entrance");
  check(host.variables().memory.cstr(0x4f8468) == "north", "entrance word is the binder buffer");
  auto *gate = host.variables().find_first("gate");
  check(gate && off_grid(host, gate),
        "hideMonster moves the object off the grid");
  check(gate && (host.variables().memory.word(gate->addr + 0xc) & 0x8000u) != 0,
        "hideMonster sets the off-grid flag");
  check(host.spawns().size() == 1, "one particle spawn");
  // Spawn words are compared with the reference in test_host_vectors.
  check(host.effects().missing_definitions == 1, "missing particle definition is counted");
  check(host.counts().executed[static_cast<std::size_t>(entrance)] == 1, "setEntrance executed");
  check(host.counts().executed[static_cast<std::size_t>(hide)] == 2, "hideMonster executed");
  check(host.counts().executed[static_cast<std::size_t>(show)] == 1, "showMonster executed");
  check(host.counts().executed[static_cast<std::size_t>(particle)] == 1, "addParticleOnObject executed");
  check(host.counts().started == 1 && host.counts().reached_complete == 1, "level entry completed");
  check(host.counts().faults == 0, "routed commands do not fault");
}

// Synthetic binder address of a test creature's type-name string (+0x34
// holds a pointer to it). Fixture only, like the generated effects vectors.
constexpr std::uint32_t FIXTURE_TYPE_NAME = 0x210400;
// g_scriptSelectedObj, research tools/ref_natives.py G_SELECTED_OBJ.
constexpr std::uint32_t SELECTED_WORD = 0x4039f8;
std::size_t at(const char *name) { return static_cast<std::size_t>(index_of(name)); }

// Expected values are research's: docs/script-effects.md (the 0x1abcd slot-34
// pattern, the fade steps, the channel formula, the stop marker) or the
// pinned reference (scale: ref_core and ref_script_effects.scale_stats at
// 52984db give 50/100 = 0x3effffff, so 100.0 becomes 49.0 = 0x42440000 and
// armour 10 becomes 4).
void test_routed_commands() {
  auto built = build_calls(
      {{"start",
        {{"setMonsterVal", {2000, 1, 0x1abcd}},
         {"setMonsterVal", {2000, 4, 0x1abcd}},
         {"setMonsterVal", {2200, 1, 99}},
         {"setCreatureParams", {2000, 50, 50, 50, 50, 50}},
         {"charmMonster", {2400}},
         {"hudResetFadeScreen", {1}},
         {"hudFadeScreen", {-16}},
         {"endMusic", {}}}}},
      {{2000, "ogre"}, {2200, "prop"}, {2400, "imp"}});
  sc::Host host;
  host.add_object("ogre", 0x20, {}, 0);
  host.add_object("prop", 0, {}, 0);
  host.add_object("imp", 0x20, {}, 0);
  auto &memory = host.variables().memory;
  auto *ogre = host.variables().find_first("ogre");
  auto *prop = host.variables().find_first("prop");
  auto *imp = host.variables().find_first("imp");
  check(ogre && prop && imp, "routed objects exist");
  if (!ogre || !prop || !imp)
    return;
  memory.w32(ogre->addr + 0x228, 0x42c80000u);
  memory.w16(ogre->addr + 0xe4, 10);
  plant(host, built);
  host.run_prestart();
  check(host.run_start() == sc::Stop::Complete, "routed script completes");
  check(host.counts().faults == 0 && host.counts().callee_refusals == 0, "routed script does not fault");
  check(memory.u16(ogre->addr + 0x3ac) == 0xabcd, "which 1 stores the low half");
  check(memory.u32(ogre->addr + 0x3c0) == 0x472bcd00u, "which 4 stores from_int of the low half");
  check(memory.u16(prop->addr + 0x3ac) == 0, "a non-creature stores nothing");
  check(host.counts().executed[at("setMonsterVal")] == 3 && host.counts().refused[at("setMonsterVal")] == 0,
        "setMonsterVal executed");
  check(memory.u32(ogre->addr + 0x228) == 0x42440000u, "setCreatureParams scales hit points");
  check(memory.u16(ogre->addr + 0xe4) == 4, "setCreatureParams scales armour");
  check(memory.u8(imp->addr + 0x310) == 0 && memory.u16(imp->addr + 0x312) == 0xffff,
        "charmMonster stores team 0 and frames -1");
  const auto &calls = host.callees().calls;
  check(std::any_of(calls.begin(), calls.end(),
                    [&](const se::Call &c) {
                      return c.name == "handle_clear" && c.args == std::vector<std::uint32_t>{imp->addr + 0x234, 0xffffffffu};
                    }),
        "charmMonster clears the null target");
  check(memory.u32(se::G_FADE) == 112, "128 - 16 is 112");
  check(host.callees().rects.size() == 1 && host.callees().rects[0].colour == (112u << 24),
        "a nonzero fade records the 640 by 448 rect");
  check(memory.u32(0x877158) == 1 && host.audio().queued_count(2) == 1, "endMusic appends one stop marker");
  check(host.counts().inferred[at("setMonsterVal")] == 0, "an executed route is not inferred");
}

// Each routed command whose callee research did not execute for this state
// is refused before the binder runs.
void test_callee_refusals() {
  auto built = build_calls(
      {{"start",
        {{"charmMonster", {2000}},
         {"followPlayer", {2000}},
         {"hudFadeScreen", {40}},
         {"startMusic", {2200}},
         {"setCFlag", {2400, 0, 1}}}}},
      {{2000, "imp"}, {2200, "theme"}, {2400, "broon"}});
  sc::Host host;
  host.add_object("imp", 0x20, {}, 0);
  host.add_object("broon", 0x20, {}, 0);
  auto &memory = host.variables().memory;
  auto *imp = host.variables().find_first("imp");
  auto *broon = host.variables().find_first("broon");
  check(imp && broon, "refusal objects exist");
  if (!imp || !broon)
    return;
  memory.w32(imp->addr + 0x234, 0x44);
  memory.string(FIXTURE_TYPE_NAME, "FatCyclops");
  memory.w32(broon->addr + 0x34, FIXTURE_TYPE_NAME);
  plant(host, built);
  host.run_prestart();
  check(host.run_start() == sc::Stop::Complete, "discarded refusals do not stop the entry");
  for (const char *name : {"charmMonster", "followPlayer", "hudFadeScreen", "startMusic", "setCFlag"})
    check(host.counts().refused[at(name)] == 1 && host.counts().executed[at(name)] == 0,
          std::string("callee refusal: ") + name);
  check(host.counts().callee_refusals == 5 && host.counts().stub_refusals == 0, "callee refusals counted");
  check(memory.u16(imp->addr + 0x312) == 0 && memory.u32(imp->addr + 0x234) == 0x44,
        "a live charm target is left alone");
  check((memory.u32(imp->addr + 0xa8) & (1u << 23)) == 0, "followPlayer stores nothing when refused");
  check(memory.u32(se::G_FADE) == 0 && host.callees().calls.empty(), "nothing reached a callee");
  check((memory.u8(broon->addr + 0xa8) & 1u) == 0 && host.audio().queued_count(2) == 0,
        "setCFlag on a boss without a music volume stores nothing");
  check(host.campaign().events().boss_music == 0, "no boss-music request is noted");
}

// With the caller's music volume, startMusic and the boss bit of setCFlag run.
void test_music_with_volume() {
  auto built = build_calls({{"start", {{"startMusic", {2200}}, {"setCFlag", {2400, 0, 1}}}}},
                           {{2200, "theme"}, {2400, "broon"}});
  sc::Host host;
  host.add_object("broon", 0x20, {}, 0);
  auto &memory = host.variables().memory;
  auto *broon = host.variables().find_first("broon");
  check(broon != nullptr, "boss object exists");
  if (!broon)
    return;
  memory.string(FIXTURE_TYPE_NAME, "FatCyclops");
  memory.w32(broon->addr + 0x34, FIXTURE_TYPE_NAME);
  audio::VatEntry row;
  row.name = "mx_ba_broon_l.vag";
  row.length = 16;
  row.declared_rate = 48000;
  host.audio().set_global_vat(std::vector<std::uint8_t>(16, 0), {row}, 0);
  host.set_music_volume(0x3f800000u);
  plant(host, built);
  host.run_prestart();
  check(host.run_start() == sc::Stop::Complete, "music script completes");
  check(host.counts().callee_refusals == 0 && host.counts().faults == 0, "music runs with a volume");
  const auto &calls = host.callees().calls;
  check(calls.size() == 2 && calls[0].name == "audio_setChannelVolume" &&
            calls[0].fargs == std::vector<std::uint32_t>{0x3f480000u},
        "the channel word is scalar * volume * 0x3f480000");
  check(calls.size() == 2 && calls[1].name == "stream_playLevel" && calls[1].args.size() == 3 &&
            calls[1].args[0] == 2 && calls[1].args[2] == 0x23,
        "stream 2 is queued with flags 3 | 0x22");
  check(host.music_channel() == 0x3f480000u, "the channel word is kept");
  check(host.counts().executed[at("setCFlag")] == 1 && host.counts().inferred[at("setCFlag")] == 1,
        "setCFlag's boss route is counted as inferred");
  check(memory.u8(broon->addr + 0xa8) & 1u, "setCFlag sets bit 0");
  check(host.audio().queued_count(2) == 1, "setCFlag queues the boss track");
}

// The five text setters sit behind text_lookup, whose string table the host
// does not have: refused, nothing written to the HUD words.
void test_text_setters_refused() {
  auto built = build_calls({{"start",
                             {{"setInfo", {2000}},
                              {"addHelpMessage", {2000, 2000}},
                              {"addHelpMessageF", {2000, 1, 2000, 2}},
                              {"showBust", {2000, 2000, 1, 2}},
                              {"showCompass", {2000, 2000}}}}},
                           {{2000, "key"}});
  sc::Host host;
  plant(host, built);
  host.run_prestart();
  check(host.run_start() == sc::Stop::Complete, "text setters refuse without stopping");
  for (const char *name : {"setInfo", "addHelpMessage", "addHelpMessageF", "showBust", "showCompass"})
    check(host.counts().refused[at(name)] == 1, std::string("text setter refused: ") + name);
  auto &memory = host.variables().memory;
  check(memory.u16(se::G_MESSAGE_FRAMES) == 0 && memory.u8(se::G_HELP_OPEN) == 0 &&
            memory.u8(se::G_BUST_COUNTER) == 0 && memory.u8(se::G_COMPASS_COUNTER) == 0,
        "no HUD word is written");
  check(memory.u32(se::G_CAMERA) == 0, "no camera is installed");
}

// A select* form on an adopted stub is a stub refusal.
void test_selected_stub() {
  auto built = build_calls({{"start", {{"selectSetMonsterVal", {1, 5}}, {"initScriptSetParams", {50, 50, 50, 50, 50}}}}}, {});
  sc::Host host;
  plant(host, built);
  host.run_prestart();
  const auto blob = gate_objects();
  host.adopt_objects(blob.data(), blob.size());
  auto *gate = host.variables().find_first("gate");
  check(gate != nullptr, "stub adopted");
  if (!gate)
    return;
  host.variables().memory.w32(SELECTED_WORD, gate->addr);
  host.run_start();
  check(host.counts().refused[at("selectSetMonsterVal")] == 1 && host.counts().refused[at("initScriptSetParams")] == 1,
        "select forms refuse on a stub");
  check(host.counts().stub_refusals == 2, "select-form stub refusals counted");
  check(host.variables().memory.u16(gate->addr + 0x3ac) == 0, "the stub is not written");
}

void test_refusals() {
  const char *names[] = {"newWorld", "removePlayerItem", "onePlayerHasItem", "generateMonster",
                         "rootMonster", "followPath", "cutScene", "sound"};
  std::vector<Call> calls;
  for (const char *name : names)
    calls.push_back({name, {2000}});
  auto built = build_calls({{"start", calls}}, {{2000, "nope"}});
  sc::Host host;
  host.add_object("gate", 0, {}, 0);
  plant(host, built);
  const auto before = host.variables().memory.word(0x41b6d0);
  host.run_prestart();
  check(host.run_start() == sc::Stop::Complete, "refused commands still return");
  check(host.counts().faults == 0, "a refusal is not a fault");
  check(host.level().entrance.empty(), "a refusal does not set an entrance");
  check(host.variables().memory.word(0x41b6d0) == before, "a refusal does not request a world");
  check(host.spawns().empty() && host.effects().missing_definitions == 0, "a refusal spawns nothing");
  auto *gate = host.variables().find_first("gate");
  check(gate && grid_cell(gate) && !off_grid(host, gate),
        "a refusal does not move the grid");
  for (const char *name : names) {
    const auto index = index_of(name);
    check(index >= 0, std::string("refusal index ") + name);
    if (index < 0)
      continue;
    check(host.counts().refused[static_cast<std::size_t>(index)] == 1, std::string("refused ") + name);
    check(host.counts().executed[static_cast<std::size_t>(index)] == 0, std::string("not executed ") + name);
    check(sc::row(static_cast<std::size_t>(index)).effect != sc::Effect::Documented, std::string("undocumented ") + name);
  }
}

void test_wait_then_complete() {
  auto built = build_calls({{"start", {{"addDeferedCall", {2000}}}}, {"later", {{"setv", {2200, 1}}}}},
                           {{2000, "later"}, {2200, "flag"}});
  sc::Host host;
  plant(host, built);
  host.run_prestart();
  check(host.run_start() == sc::Stop::Wait, "a queued name is the first wait");
  check(host.counts().started == 1 && host.counts().reached_wait == 1, "wait counted once");
  check(host.counts().reached_complete == 0, "a queued script has not completed");
  check(host.variables().quest_get("flag") == 0, "the deferred body has not run");
  host.on_ready_tick();
  check(!host.waiting() && host.counts().reached_complete == 1,
        "the load-time promotion lets the first tick run the deferred body");
  check(host.variables().quest_get("flag") == 1, "the deferred setv ran");
  check(host.counts().faults == 0, "deferred execution does not fault");
}

void test_prestart_before_objects() {
  auto built = build_calls({{"preStart", {{"hideMonster", {2000}}}}}, {{2000, "gate"}});
  sc::Host host;
  const auto blob = gate_objects();
  plant(host, built);
  host.run_prestart();
  host.adopt_objects(blob.data(), blob.size());
  host.run_start();
  auto *gate = host.variables().find_first("gate");
  check(gate && grid_cell(gate) && !off_grid(host, gate),
        "preStart runs before the objects are loaded and hides nothing");
}

void test_stub_objects() {
  sc::Host empty;
  const auto unnamed = gate_objects("");
  empty.adopt_objects(unnamed.data(), unnamed.size());
  check(empty.counts().objects_refused == 1 && empty.counts().objects_registered == 0,
        "a record without a class string is refused");
  check(empty.variables().find_first("gate") == nullptr, "a refused record is not registered");

  const char *names[] = {"friendly", "getHitPoints", "hideMonster", "increaseEnemyRank"};
  std::vector<Call> calls;
  for (const char *name : names)
    calls.push_back({name, {2000}});
  // setCFlag (a quest command, QUESTS.md) tests creature flag 0x20 on each
  // object it finds, so a stub refuses it too.
  calls.push_back({"setCFlag", {2000, 26, 1}});
  // The 52984db creature routes test the same flag.
  for (const char *name : {"setMonsterVal", "setMonsterRange", "setCreatureParams"})
    calls.push_back({name, {2000, 1, 1, 1, 1, 1}});
  calls.push_back({"charmMonster", {2000}});
  calls.push_back({"followPlayer", {2000}});
  calls.push_back({"addParticleOnObject", {0, 2000, 0, 0, 0, 0, 0, 0}});
  auto built = build_calls({{"start", calls}}, {{2000, "gate"}});
  sc::Host host;
  plant(host, built);
  host.run_prestart();
  const auto blob = gate_objects();
  host.adopt_objects(blob.data(), blob.size());
  auto *gate = host.variables().find_first("gate");
  check(gate && host.is_stub(gate->addr), "an adopted record is an unconstructed stub");
  check(gate && gate->type_name == "Stub", "the stub keeps the record's class string");
  host.run_start();
  for (const char *name : names) {
    const auto index = static_cast<std::size_t>(index_of(name));
    check(host.counts().executed[index] == 0 && host.counts().refused[index] == 1,
          std::string("refused on a stub: ") + name);
  }
  const auto cflag = static_cast<std::size_t>(index_of("setCFlag"));
  check(host.counts().executed[cflag] == 0 && host.counts().refused[cflag] == 1 &&
            host.campaign().events().cflag_bit26 == 0,
        "refused on a stub: setCFlag");
  for (const char *name : {"setMonsterVal", "setMonsterRange", "setCreatureParams", "charmMonster", "followPlayer"})
    check(host.counts().executed[at(name)] == 0 && host.counts().refused[at(name)] == 1,
          std::string("refused on a stub: ") + name);
  check(host.counts().stub_refusals == 10, "stub refusals counted");
  check(gate && grid_cell(gate) && !off_grid(host, gate),
        "a refused hideMonster leaves the stub on the grid");
  check(host.counts().executed[static_cast<std::size_t>(index_of("addParticleOnObject"))] == 1,
        "a command that reads only the stub's position still runs");
}

void test_refused_result_stops_entry() {
  const auto has = static_cast<std::size_t>(index_of("playerHasItem"));
  const auto setv = static_cast<std::size_t>(index_of("setv"));
  // Discarded: the next call's SYSREQ.C overwrites PRI. The entry goes on.
  auto discarded = build_calls({{"start", {{"playerHasItem", {2000}}, {"setv", {2100, 1}}}}},
                               {{2000, "key"}, {2100, "flag"}});
  sc::Host quiet;
  plant(quiet, discarded);
  quiet.run_prestart();
  check(quiet.run_start() == sc::Stop::Complete, "a discarded refusal does not stop the entry");
  check(quiet.counts().refusal_stops == 0 && quiet.counts().executed[setv] == 1,
        "the entry runs on after a discarded refusal");
  // Used: STOR.pri stores the refused answer. The entry stops there.
  auto stored = build_calls({{"start", {{"playerHasItem", {2000}}, {".STOR.pri", {2400}},
                                        {"setv", {2100, 1}}}}},
                            {{2000, "key"}, {2100, "flag"}});
  sc::Host host;
  plant(host, stored);
  host.run_prestart();
  check(host.run_start() == sc::Stop::Refused, "a used refusal stops the entry");
  check(host.counts().refusal_stops == 1 && host.counts().faults == 0,
        "the stop is counted apart from faults");
  check(host.counts().refused[has] == 1 && host.counts().executed[setv] == 0,
        "nothing after the used refusal runs");
  check(host.counts().reached_complete == 0, "a stopped entry has not completed");
  check(host.stopped_on_refusal(), "the host reports the refusal stop");
  // Used: RETN returns PRI as the entry's value.
  auto returned = build_calls({{"start", {{"playerHasItem", {2000}}}}}, {{2000, "key"}}, true);
  sc::Host tail;
  plant(tail, returned);
  check(tail.run("start") == sc::Stop::Refused, "a returned refusal stops the entry");
}

void test_missing_and_bad_image() {
  auto built = build_calls({{"other", {}}}, {});
  sc::Host host;
  plant(host, built);
  check(host.run_prestart() == sc::Stop::Missing && host.run_start() == sc::Stop::Missing,
        "no preStart or start");
  check(host.counts().started == 0, "an unused public does not start the level");
  sc::Host broken;
  check(!broken.load({1, 2, 3, 4}), "a short image is refused");
  check(broken.counts().started == 0 && broken.counts().faults == 0, "a refused image changes no counts");
  check(!broken.adopt_objects(nullptr, 0), "empty objects are refused");
  check(broken.counts().objects_refused == 1, "object refusal is counted");
}
// Replays native/tests/vectors/script_host.txt: research 9649e6b
// tools/ref_natives.py run on synthetic images by
// tools/generate_script_host_vectors.py. Expected values are the reference's.
std::vector<std::string> words_of(const std::string &line) {
  std::vector<std::string> out;
  std::stringstream stream(line);
  std::string word;
  while (stream >> word)
    out.push_back(word);
  return out;
}
std::uint32_t hex(const std::string &text) { return static_cast<std::uint32_t>(std::stoul(text, nullptr, 16)); }

struct HostCase {
  std::string name;
  bool seeded = false;
  std::uint32_t seed = 0;
  a::ByteVector image;
  std::vector<std::vector<std::string>> objects, at, obj, spawn;
  int ticks = 0;
  std::vector<std::string> quests, entry;
  std::uint32_t seed_out = 0;
  std::uint64_t draws = 0;
  std::string entrance;
};

void replay(const HostCase &c) {
  const std::string tag = "vector " + c.name + ": ";
  sc::Host host;
  SharedRandom live(c.seed);
  if (c.seeded)
    host.bind_seed(&live);
  check(host.load(c.image), tag + "reference image loads");
  if (!host.machine())
    return;
  std::vector<std::uint32_t> results;
  const auto prestart = host.run_prestart();
  if (prestart != sc::Stop::Missing)
    results.push_back(static_cast<std::uint32_t>(host.last_value()));
  for (const auto &o : c.objects)
    host.add_object(o[1], hex(o[2]), {hex(o[3]), hex(o[4]), hex(o[5])}, hex(o[6]));
  host.run_start();
  if (host.machine()->find_public("start").first == a::ERR_NONE)
    results.push_back(static_cast<std::uint32_t>(host.last_value()));
  check(results.size() + 1 == c.entry.size(), tag + "entry count");
  for (std::size_t i = 0; i + 1 < c.entry.size() && i < results.size(); ++i)
    check(results[i] == hex(c.entry[i + 1]), tag + "entry value " + std::to_string(i));
  auto snapshot = [&](const std::vector<std::string> &want) {
    const auto queue = host.queue();
    const auto at = want[1];
    check(queue.count == std::stoul(want[2]) && queue.queued == std::stoul(want[3]) &&
              queue.flip == std::stoul(want[4]),
          tag + "queue at tick " + at);
    for (std::size_t k = 5; k < want.size(); ++k) {
      const auto eq = want[k].find('=');
      const auto name = want[k].substr(0, eq);
      check(host.variables().quest_get(name) == std::stol(want[k].substr(eq + 1)),
            tag + "quest " + name + " at tick " + at);
    }
  };
  check(!c.at.empty(), tag + "has a load snapshot");
  if (!c.at.empty())
    snapshot(c.at[0]);
  for (int tick = 1; tick <= c.ticks && tick < static_cast<int>(c.at.size()); ++tick) {
    host.on_ready_tick();
    snapshot(c.at[static_cast<std::size_t>(tick)]);
  }
  if (c.seeded)
    check(live.state == c.seed_out, tag + "session seed after the draws");
  check(host.counts().executed[static_cast<std::size_t>(index_of("getRand"))] == c.draws,
        tag + "shared draws");
  for (const auto &o : c.obj) {
    auto *found = host.variables().find_first(o[1]);
    check(found != nullptr, tag + "object " + o[1]);
    if (!found)
      continue;
    const bool off = off_grid(host, found);
    check(host.variables().memory.word(found->addr + 0xc) == hex(o[2]), tag + "flags of " + o[1]);
    check(host.variables().memory.read(found->addr + 0x231) == hex(o[3]), tag + "hidden byte of " + o[1]);
    check(off == (o[4] == "1"), tag + "grid of " + o[1]);
  }
  check(host.level().entrance == (c.entrance == "-" ? std::string() : c.entrance), tag + "entrance");
  check(host.spawns().size() == c.spawn.size(), tag + "spawn count");
  for (std::size_t i = 0; i < c.spawn.size() && i < host.spawns().size(); ++i) {
    const auto &got = host.spawns()[i];
    const auto &want = c.spawn[i];
    check(got.definition == hex(want[1]) && got.pos[0] == hex(want[2]) && got.pos[1] == hex(want[3]) &&
              got.pos[2] == hex(want[4]) && got.vel[0] == hex(want[5]) && got.vel[1] == hex(want[6]) &&
              got.vel[2] == hex(want[7]) && got.backdate == hex(want[8]),
          tag + "spawn " + std::to_string(i));
  }
  check(host.counts().faults == 0 && host.counts().refusal_stops == 0, tag + "no fault or refusal stop");
}

void test_host_vectors(const std::string &path) {
  std::ifstream in(path);
  check(static_cast<bool>(in), "host vector file opens");
  HostCase current;
  int cases = 0;
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#')
      continue;
    const auto w = words_of(line);
    const auto &key = w[0];
    if (key == "CASE") {
      current = HostCase{};
      current.name = w.size() > 1 ? w[1] : "?";
    } else if (key == "SEED") {
      current.seeded = w[1] != "-";
      if (current.seeded)
        current.seed = hex(w[1]);
    } else if (key == "IMAGE") {
      for (std::size_t i = 0; i + 1 < w[1].size(); i += 2)
        current.image.push_back(static_cast<std::uint8_t>(std::stoul(w[1].substr(i, 2), nullptr, 16)));
    } else if (key == "OBJECT") {
      current.objects.push_back(w);
    } else if (key == "TICKS") {
      current.ticks = std::stoi(w[1]);
    } else if (key == "QUESTNAMES") {
      current.quests.assign(w.begin() + 1, w.end());
    } else if (key == "ENTRY") {
      current.entry = w;
    } else if (key == "AT") {
      current.at.push_back(w);
    } else if (key == "SEED_OUT") {
      current.seed_out = hex(w[1]);
    } else if (key == "DRAWS") {
      current.draws = std::stoull(w[1]);
    } else if (key == "OBJ") {
      current.obj.push_back(w);
    } else if (key == "ENTRANCE") {
      current.entrance = w[1];
    } else if (key == "SPAWN") {
      current.spawn.push_back(w);
    } else if (key == "END") {
      replay(current);
      ++cases;
    } else {
      check(false, "unknown host vector line " + key);
    }
  }
  check(cases == 8, "eight host vector cases");
}
} // namespace

int main(int argc, char **argv) {
  if (argc != 3) {
    std::cerr << "usage: script-commands-tests VECTORS HOST_VECTORS\n";
    return 2;
  }
  test_table(argv[1]);
  test_host_vectors(argv[2]);
  test_variables_and_seed();
  test_seed_refused_without_session_seed();
  test_entrance_grid_and_particle();
  test_routed_commands();
  test_callee_refusals();
  test_music_with_volume();
  test_text_setters_refused();
  test_selected_stub();
  test_refusals();
  test_wait_then_complete();
  test_prestart_before_objects();
  test_stub_objects();
  test_refused_result_stops_entry();
  test_missing_and_bad_image();
  if (failed) {
    std::cerr << failed << " failed\n";
    return 1;
  }
  std::cout << "script-commands ok\n";
  return 0;
}
