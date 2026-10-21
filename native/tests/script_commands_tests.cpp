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
using champions::simulation::rta::Fpu;
using champions::simulation::rta::SharedRandom;
using champions::simulation::rta::Lcg;

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
Built build_calls(const std::vector<std::pair<std::string, std::vector<Call>>> &publics,
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

std::vector<std::uint8_t> gate_objects() {
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
  u32(13);
  u32(0);
  u32(0);
  const char *cls = "10GameObject";
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
                           {{2000, "quest"}});
  sc::Host host;
  SharedRandom live(20973);
  Lcg expected(20973);
  const auto drawn = expected.draw();
  host.bind_seed(&live);
  plant(host, built);
  check(host.run("start") == sc::Stop::Complete, "variable script completes");
  check(host.variables().quest_get("quest") == 7, "setv writes the quest cell");
  check(static_cast<std::uint32_t>(host.last_value()) == drawn, "getRand matches one shared draw");
  check(live.state == expected.state, "getRand advances the shared seed once");
  check(host.variables().memory.word(0x404464) == live.state, "seed word matches the shared generator");
  check(host.counts().executed[static_cast<std::size_t>(setv)] == 1, "setv executed");
  check(host.counts().executed[static_cast<std::size_t>(getv)] == 1, "getv executed");
  check(host.counts().executed[static_cast<std::size_t>(rand)] == 1, "getRand executed");
  check(host.counts().faults == 0, "documented variable commands do not fault");
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
  const auto blob = gate_objects();
  check(host.adopt_objects(blob.data(), blob.size()), "synthetic objects.ob");
  check(host.counts().objects_registered == 1, "one named object");
  plant(host, built);
  check(host.begin_level() == sc::Stop::Complete, "entrance script completes");
  check(host.level().entrance == "north", "setEntrance copies the level-flow entrance");
  check(host.variables().memory.cstr(0x4f8468) == "north", "entrance word is the binder buffer");
  auto *gate = host.variables().find_first("gate");
  auto *link = gate && gate->heads[2].next ? gate->heads[2].next->partner : nullptr;
  check(gate && link && link->prev == &host.variables().world->off_grid,
        "hideMonster moves the object off the grid");
  check(gate && (host.variables().memory.word(gate->addr + 0xc) & 0x8000u) != 0,
        "hideMonster sets the off-grid flag");
  check(host.spawns().size() == 1, "one particle spawn");
  if (!host.spawns().empty()) {
    Fpu fpu;
    const auto scale = fpu.mul(fpu.from_int(1000), 0x3a83126eu);
    check(host.spawns()[0].definition == 0x5d3188u, "slot 0 particle definition");
    check(host.spawns()[0].vel[0] == scale && host.spawns()[0].vel[1] == scale &&
              host.spawns()[0].vel[2] == scale,
          "particle velocity is the documented thousandth");
    check(host.spawns()[0].backdate == 0, "particle backdate is zero");
  }
  check(host.effects().missing_definitions == 1, "missing particle definition is counted");
  check(host.counts().executed[static_cast<std::size_t>(entrance)] == 1, "setEntrance executed");
  check(host.counts().executed[static_cast<std::size_t>(hide)] == 2, "hideMonster executed");
  check(host.counts().executed[static_cast<std::size_t>(show)] == 1, "showMonster executed");
  check(host.counts().executed[static_cast<std::size_t>(particle)] == 1, "addParticleOnObject executed");
  check(host.counts().started == 1 && host.counts().reached_complete == 1, "level entry completed");
  check(host.counts().faults == 0, "routed commands do not fault");
}

std::string wide_at(const builtins::Memory &memory, std::uint32_t address) {
  std::string out;
  for (std::uint32_t i = 0; i < 64; ++i) {
    const auto unit = memory.u16(address + i * 2u);
    if (unit == 0)
      return out;
    out.push_back(static_cast<char>(unit));
  }
  return out;
}

void test_routed_commands() {
  auto built = build_calls(
      {{"start",
        {{"setMonsterVal", {2000, 1, 0x1abcd}},
         {"setMonsterVal", {2000, 4, 0x1abcd}},
         {"setMonsterVal", {2200, 1, 99}},
         {"setInfo", {2400}},
         {"showBust", {2600, 2800, 3, 4}},
         {"addHelpMessage", {3000, 3200}},
         {"hudFadeScreen", {40}},
         {"endMusic", {}},
         {"setCFlag", {3400, 0, 1}},
         {"getRand", {}}}}},
      {{2000, "ogre"},
       {2200, "prop"},
       {2400, "hello"},
       {2600, "face"},
       {2800, "hi"},
       {3000, "title"},
       {3200, "body"},
       {3400, "broon"}});
  sc::Host host;
  host.add_object("ogre", 0x20, {}, 0);
  host.add_object("prop", 0, {}, 0);
  host.add_object("broon", 0x20, {}, 0);
  auto *ogre = host.variables().find_first("ogre");
  auto *prop = host.variables().find_first("prop");
  auto *broon = host.variables().find_first("broon");
  check(ogre && prop && broon, "routed objects exist");
  if (!ogre || !prop || !broon)
    return;
  host.variables().memory.string(0x720000, "FatCyclops");
  host.variables().memory.word(broon->addr + 0x34, 0x720000);
  host.variables().memory.word(se::G_MUSIC_VOLUME, audio::music_volume_image);
  audio::VatEntry row;
  row.name = "mx_ba_broon_l.vag";
  row.length = 16;
  row.declared_rate = 48000;
  host.audio().set_global_vat(std::vector<std::uint8_t>(16, 0), {row}, 0);
  SharedRandom live(20973);
  Lcg expected(20973);
  const auto drawn = expected.draw();
  host.bind_seed(&live);
  const auto seed_before = live.state;
  plant(host, built);
  check(host.begin_level() == sc::Stop::Complete, "routed script completes");
  check(host.counts().faults == 0, "routed script does not fault");
  check(host.variables().memory.u16(ogre->addr + 0x3ac) == 0xabcd, "which 1 stores the low half");
  check(host.variables().memory.u32(ogre->addr + 0x3c0) == 0x472bcd00u, "which 4 stores from_int of the low half");
  check(host.variables().memory.u16(prop->addr + 0x3ac) == 0, "a non-creature stores nothing");
  check(host.counts().executed[static_cast<std::size_t>(index_of("setMonsterVal"))] == 3, "setMonsterVal executed");
  check(host.counts().refused[static_cast<std::size_t>(index_of("setMonsterVal"))] == 0, "setMonsterVal is not refused");
  check(host.variables().memory.u16(se::G_MESSAGE_FRAMES) == 180, "setInfo stores 180 frames");
  check(wide_at(host.variables().memory, se::G_MESSAGE_TEXT) == "hello", "setInfo copies the widened key");
  check(host.variables().memory.cstr(se::G_BUST_TEXTURE) == "face", "showBust copies the texture name");
  check(wide_at(host.variables().memory, se::G_BUST_TEXT) == "hi", "showBust copies the widened key");
  check(host.variables().memory.u8(se::G_BUST_COUNTER) == 5, "showBust stores counter 5");
  check(host.variables().memory.u32(se::G_BUST_A) == 3 && host.variables().memory.u32(se::G_BUST_B) == 4,
        "showBust stores both integers");
  check(host.variables().memory.u8(se::G_HELP_OPEN) == 1, "addHelpMessage opens the help box");
  check(host.variables().memory.u32(se::G_HELP_DELAY) == 90, "addHelpMessage stores the 90-frame delay");
  check(wide_at(host.variables().memory, host.variables().memory.u32(se::G_HELP_TITLE)) == "title",
        "help title is the widened key");
  check(host.variables().memory.u32(se::G_FADE) == 40, "hudFadeScreen adds the delta");
  check(host.callees().rects.size() == 1 && host.callees().rects[0].colour == (40u << 24),
        "a nonzero fade draws the 640 by 448 rect");
  check(host.variables().memory.u32(0x877158) == 1, "endMusic appends one stop marker");
  check(host.audio().queued_count(2) == 1, "setCFlag queues the boss track");
  check(host.variables().memory.u8(broon->addr + 0xa8) & 1u, "setCFlag sets bit 0");
  check(live.state == expected.state && live.state != seed_before, "only getRand draws the shared seed");
  check(static_cast<std::uint32_t>(host.last_value()) == drawn, "getRand is the script return");
  check(host.variables().memory.u32(0x404464) == live.state, "the seed word matches the one draw");

  sc::Host blind;
  blind.variables().memory.word(se::G_CAMERA, 0);
  auto faulted = build_calls({{"start", {{"showBust", {2000, 2200, 1, 2}}}}}, {{2000, "face"}, {2200, "hi"}});
  plant(blind, faulted);
  check(blind.begin_level() == sc::Stop::Fault, "a null camera faults showBust");
  check(blind.counts().faults == 1, "the null camera is a fault");
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
  check(host.begin_level() == sc::Stop::Complete, "refused commands still return");
  check(host.counts().faults == 0, "a refusal is not a fault");
  check(host.level().entrance.empty(), "a refusal does not set an entrance");
  check(host.variables().memory.word(0x41b6d0) == before, "a refusal does not request a world");
  check(host.spawns().empty() && host.effects().missing_definitions == 0, "a refusal spawns nothing");
  auto *gate = host.variables().find_first("gate");
  auto *link = gate && gate->heads[2].next ? gate->heads[2].next->partner : nullptr;
  check(gate && link && link->prev != &host.variables().world->off_grid,
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
  check(host.begin_level() == sc::Stop::Wait, "a queued name is the first wait");
  check(host.counts().started == 1 && host.counts().reached_wait == 1, "wait counted once");
  check(host.counts().reached_complete == 0, "a queued script has not completed");
  check(host.variables().quest_get("flag") == 0, "the deferred body has not run");
  host.on_ready_tick();
  check(host.waiting() && host.variables().quest_get("flag") == 0, "the first tick only promotes the queue");
  check(host.counts().reached_wait == 1, "promotion is not a second wait");
  host.on_ready_tick();
  check(!host.waiting() && host.counts().reached_complete == 1, "the deferred body completes the script");
  check(host.variables().quest_get("flag") == 1, "the deferred setv ran");
  check(host.counts().faults == 0, "deferred execution does not fault");
}

void test_missing_and_bad_image() {
  auto built = build_calls({{"other", {}}}, {});
  sc::Host host;
  plant(host, built);
  check(host.begin_level() == sc::Stop::Missing, "no preStart or start");
  check(host.counts().started == 0, "an unused public does not start the level");
  sc::Host broken;
  check(!broken.load({1, 2, 3, 4}), "a short image is refused");
  check(broken.counts().started == 0 && broken.counts().faults == 0, "a refused image changes no counts");
  check(!broken.adopt_objects(nullptr, 0), "empty objects are refused");
  check(broken.counts().objects_refused == 1, "object refusal is counted");
}
} // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "usage: script-commands-tests VECTORS\n";
    return 2;
  }
  test_table(argv[1]);
  test_variables_and_seed();
  test_entrance_grid_and_particle();
  test_routed_commands();
  test_refusals();
  test_wait_then_complete();
  test_missing_and_bad_image();
  if (failed) {
    std::cerr << failed << " failed\n";
    return 1;
  }
  std::cout << "script-commands ok\n";
  return 0;
}
