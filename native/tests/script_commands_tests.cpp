#include "champions/amx.hpp"
#include "champions/core_arithmetic.hpp"
#include "champions/core_state.hpp"
#include "champions/native_audio.hpp"
#include "champions/script_commands.hpp"
#include "champions/script_effects.hpp"
#include "champions/script_items.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace sc = champions::simulation::rta::script_commands;
namespace se = champions::simulation::rta::script_effects;
namespace si = champions::simulation::rta::script_items;
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

// A synthetic objects.ob with several records (class, name). Authored strings.
std::vector<std::uint8_t> objects_blob(const std::vector<std::pair<std::string, std::string>> &records,
                                       const std::vector<std::string> &extra_props = {}) {
  std::vector<std::uint8_t> strtab{0}, body, out;
  auto intern = [&](const std::string &text) {
    const auto at = static_cast<std::uint32_t>(strtab.size());
    strtab.insert(strtab.end(), text.begin(), text.end());
    strtab.push_back(0);
    return at;
  };
  auto put = [](std::vector<std::uint8_t> &v, std::uint32_t value, int n) {
    for (int k = 0; k < n; ++k)
      v.push_back(static_cast<std::uint8_t>(value >> (8 * k)));
  };
  for (std::size_t i = 0; i < records.size(); ++i) {
    const auto &r = records[i];
    const bool extra = i < extra_props.size() && !extra_props[i].empty();
    put(body, intern(r.first), 4);
    put(body, extra ? 32 : 28, 2);
    put(body, 0, 2);
    for (int k = 0; k < 3; ++k)
      put(body, 0, 4);
    put(body, intern("name=" + r.second), 4);
    if (extra)
      put(body, intern(extra_props[i]), 4);
    put(body, 0, 4);
  }
  put(out, static_cast<std::uint32_t>(records.size()), 2);
  put(out, 0, 2);
  put(out, static_cast<std::uint32_t>(8 + body.size()), 4);
  out.insert(out.end(), body.begin(), body.end());
  out.insert(out.end(), strtab.begin(), strtab.end());
  return out;
}

std::vector<std::uint8_t> objects_blob(const std::vector<std::pair<std::string, std::string>> &records,
                                       const std::string &extra_prop) {
  return objects_blob(records, std::vector<std::string>(records.size(), extra_prop));
}

// An authored executable for the object factory: a placeholder string at every
// name test's address, "Gnaw" at the Gnaw row's (level_factory_table.inc), the
// change-key format Creature_init needs, an ASCII class table. Not the game's.
champions::simulation::rta::level_factory::Executable authored_executable() {
  namespace LF = champions::simulation::rta::level_factory;
  auto strings = std::make_shared<std::map<std::uint32_t, std::string>>();
  int k = 0;
  for (const auto &row : LF::tests())
    (*strings)[row.name] = "Cls" + std::to_string(k++);
  (*strings)[0x532b88] = "Gnaw";
  (*strings)[0x52fac8] = "dead";
  (*strings)[0x52fad0] = "change%d";
  LF::Executable exe;
  exe.string = [strings](std::uint32_t a) {
    auto it = strings->find(a);
    return it == strings->end() ? std::string() : it->second;
  };
  exe.byte = [](std::uint32_t a) -> std::uint8_t {
    const auto c = std::int64_t(a) - std::int64_t(LF::CTYPE);
    return std::uint8_t((c >= 'A' && c <= 'Z') ? 1 : 0);
  };
  return exe;
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

// Host() and Host(authority) must run the same script commands to identical
// results, and the shared combat memory type must not change script-host byte
// semantics: unbounded zero-backed store, 32-bit address wrap, HostFault errors.
struct HostTrace {
  std::string text;
  void add(const std::string &key, std::uint64_t value) { text += key + '=' + std::to_string(value) + ';'; }
};
HostTrace run_equivalence(const std::shared_ptr<champions::simulation::rta::WorldAuthority> &authority) {
  HostTrace trace;
  auto built = build_calls({{"start", {{"setv", {2000, 7}}, {"getv", {2000}}, {"getRand", {}}}}},
                           {{2000, "quest"}}, true);
  // A refused command (no session seed) must take the same stop/fault path.
  {
    const auto rand = static_cast<std::size_t>(index_of("getRand"));
    auto refused_build = build_calls({{"start", {{"getRand", {}}}}}, {});
    auto refused = authority ? std::make_unique<sc::Host>(authority) : std::make_unique<sc::Host>();
    plant(*refused, refused_build);
    trace.add("refused-run", std::uint64_t(refused->run("start")));
    trace.add("refused", refused->counts().refused[rand]);
    trace.add("refused-exec", refused->counts().executed[rand]);
    trace.add("refused-faults", refused->counts().faults);
  }
  auto host_owner = authority ? std::make_unique<sc::Host>(authority) : std::make_unique<sc::Host>();
  sc::Host &host = *host_owner;
  SharedRandom live(20973);
  host.bind_seed(&live);
  plant(host, built);
  trace.add("run", std::uint64_t(host.run("start")));
  trace.add("quest", std::uint64_t(host.variables().quest_get("quest")));
  trace.add("seed", live.state);
  trace.add("seedword", host.variables().memory.word(0x404464));
  trace.add("faults", host.counts().faults);
  auto &memory = host.variables().memory;
  // More than 1 MiB of distinct non-zero bytes: the original store was unbounded.
  const std::uint32_t base = 0x10000000u;
  std::vector<std::uint8_t> chunk(1 << 19);
  for (unsigned part = 0; part < 3; ++part) {
    for (std::size_t i = 0; i < chunk.size(); ++i) chunk[i] = std::uint8_t(1 + (i + part) % 250);
    memory.write(base + part * std::uint32_t(chunk.size()), chunk);
  }
  std::uint64_t sum = 0;
  for (std::uint32_t at = base; at < base + 3 * chunk.size(); at += 997) sum += memory.u8(at) * (at & 255);
  trace.add("sum", sum);
  // Zero writes are erased, never counted, and read back as zero.
  memory.write(base + 100, std::vector<std::uint8_t>(5000, 0));
  trace.add("zeroed", memory.u64(base + 100) | memory.u64(base + 4000));
  trace.add("after", memory.u8(base + 5100));
  memory.write(0x20000000u, std::vector<std::uint8_t>(2 << 20, 0));
  trace.add("zero-big", memory.u32(0x20100000u));
  // A span crossing 0xffffffff wraps to address zero, as the paged store did.
  memory.write(0xfffffffcu, {1, 2, 3, 4, 5, 6, 7, 8});
  trace.add("wrap-tail", memory.u32(0xfffffffcu));
  trace.add("wrap-head", memory.u32(0));
  trace.add("wrap-read", memory.u64(0xfffffffcu));
  trace.add("cstr", memory.cstr(0xfffffffeu, 16).size());
  trace.add("writes", memory.writes.size());
  return trace;
}

void test_authority_equivalence() {
  const auto plain = run_equivalence(nullptr);
  auto authority = std::make_shared<champions::simulation::rta::WorldAuthority>();
  const auto shared = run_equivalence(authority);
  check(plain.text == shared.text, "Host() and Host(authority) agree: " + plain.text + " vs " + shared.text);
  check(plain.text.find("run=") != std::string::npos, "equivalence trace recorded");
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

// research t3/research-script-session 6841b8a, docs/script-session.md section 1:
// an offline session's seed is a clock reading, so the session's generator
// answers getRand although an adopted record was never constructed.
void test_offline_seed_policy() {
  const auto rand = static_cast<std::size_t>(index_of("getRand"));
  auto built = build_calls({{"start", {{"getRand", {}}}}}, {});
  sc::Host host;
  SharedRandom live(20973);
  host.bind_seed(&live, sc::SeedPolicy::OfflineSession);
  plant(host, built);
  const auto blob = gate_objects();
  host.adopt_objects(blob.data(), blob.size());
  host.run("start");
  SharedRandom once(20973);
  once.draw();
  check(host.counts().executed[rand] == 1 && host.counts().refused[rand] == 0,
        "getRand runs on the session generator after an unconstructed record");
  check(live.state == once.state, "the session generator made the one draw");
  check(host.counts().load_draws_unmodelled == 1, "the record's draws are counted as unmodelled");
}

// docs/script-session.md section 2: g_numPlayers 1, g_difficultyIdx 1.
void test_single_player() {
  sc::Host none;
  check(none.campaign().num_players() == 0, "no player count before start_single_player");
  sc::Host host;
  host.start_single_player();
  check(host.variables().memory.word(0x4d8ae0) == 1 && host.variables().memory.word(0x4f719c) == 1,
        "one player, difficulty 1");
  check(host.campaign().num_players() == 1 && host.campaign().difficulty() == 1,
        "the campaign mirrors them");
  auto built = build_calls({{"start", {{"addQuest", {2000, 2100}}}}}, {{2000, "title"}, {2100, "text"}});
  plant(host, built);
  check(host.run("start") == sc::Stop::Complete, "addQuest completes");
  check(host.campaign().record(0).quest_count == 1, "addQuest writes the one player's quest list");
}

// A newly created player has no established current HP/mana in the research.
// The script host must not turn its zero-initialized storage into a fake read.
void test_unknown_player_currents() {
  auto built = build_calls({{"start", {{"checkPlayerAlive", {0}}, {"healPlayer", {}},
                                      {"zeroPlayerMana", {}}, {"givePlayerExp", {100}}}}}, {});
  sc::Host host;
  host.set_player_currents_known(false);
  const auto rec = host.variables().player_record(0);
  auto &memory = host.variables().memory;
  memory.word(rec + 0xac, 0x41200000u); // authored sentinel 10.0
  memory.word(rec + 0xc0, 0x41400000u); // authored sentinel 12.0
  host.start_single_player();
  plant(host, built);
  check(host.run_start() == sc::Stop::Complete, "discarded unknown-current calls finish");
  check(host.counts().unknown_player_current_refusals == 4,
        "every HP/mana-dependent call is counted as refused");
  for (const char *name : {"checkPlayerAlive", "healPlayer", "zeroPlayerMana", "givePlayerExp"})
    check(host.counts().refused[std::size_t(index_of(name))] == 1 &&
              host.counts().executed[std::size_t(index_of(name))] == 0,
          std::string("unknown current refusal: ") + name);
  check(memory.word(rec + 0xac) == 0x41200000u && memory.word(rec + 0xc0) == 0x41400000u,
        "unknown current commands did not write HP or mana");
  const auto saved = host.save_campaign();
  sc::Host carried;
  carried.set_player_currents_known(false);
  auto &next_memory = carried.variables().memory;
  const auto next_rec = carried.variables().player_record(0);
  next_memory.word(next_rec + 0xac, 0x41200000u);
  next_memory.word(next_rec + 0xc0, 0x41400000u);
  carried.start_single_player();
  carried.load_campaign(saved);
  check(next_memory.word(next_rec + 0xac) == 0x41200000u &&
            next_memory.word(next_rec + 0xc0) == 0x41400000u,
        "campaign restore does not write unknown current fields");
}

// checkPlayerAlive reads HP only when which == 0 (script.md:688).
void test_check_alive_which_unknown_currents() {
  for (std::uint32_t which : {0u, 1u}) {
    auto built = build_calls({{"start", {{"checkPlayerAlive", {static_cast<int>(which)}}}}}, {});
    sc::Host host;
    host.set_player_currents_known(false);
    host.start_single_player();
    plant(host, built);
    host.run_start();
    check(host.counts().unknown_player_current_refusals == (which == 0 ? 1u : 0u),
          which == 0 ? "checkPlayerAlive(0) refused for unknown currents"
                     : "checkPlayerAlive(1) reads no HP and is not refused");
  }
}

// playGotItemSound reads +0xac only with two or more players (research-menus
// docs/script.md row 61), so unknown currents refuse it only then.
void test_got_item_sound_unknown_currents() {
  auto built = build_calls({{"start", {{"playGotItemSound", {}}}}}, {});
  for (std::uint32_t players : {1u, 2u}) {
    sc::Host host;
    host.set_player_currents_known(false);
    host.start_single_player();
    host.variables().memory.word(0x4d8ae0, players);
    plant(host, built);
    host.run_start();
    check(host.counts().unknown_player_current_refusals == (players >= 2 ? 1u : 0u),
          players >= 2 ? "two players: playGotItemSound refused for unknown currents"
                       : "one player: playGotItemSound is not blocked by unknown currents");
  }
}

// docs/script-session.md sections 3-5: the factory decides what is registered;
// A built creature without selected level statistics is readable by
// hideMonster, but its hit points remain unknown.
void test_classified_objects() {
  const auto hide = static_cast<std::size_t>(index_of("hideMonster"));
  const auto hp = static_cast<std::size_t>(index_of("getHitPoints"));
  auto built = build_calls({{"start",
                             {{"hideMonster", {2000}},
                              {"hideMonster", {2100}},
                              {"hideMonster", {2200}},
                              {"getHitPoints", {2000}}}}},
                           {{2000, "gnawling"}, {2100, "ghost"}, {2200, "lamp"}});
  sc::Host host;
  SharedRandom live(20973);
  host.bind_seed(&live);
  host.set_executable(authored_executable());
  plant(host, built);
  host.run_prestart();
  const auto blob = objects_blob({{"gnaw", "gnawling"}, {"Nope", "ghost"}, {"Cls0", "lamp"}});
  check(host.adopt_objects(blob.data(), blob.size()), "classified records adopt");
  const auto &counts = host.counts();
  check(counts.objects_built == 1 && counts.objects_unbuilt == 1 && counts.objects_not_created == 1 &&
            counts.objects_registered == 2,
        "one built, one stub, one record the factory builds nothing for");
  check(counts.unbuilt_constructors.size() == 1 && counts.unbuilt_constructors.count(0x158bcc) == 1,
        "the stub is counted by its constructor");
  auto *gnaw = host.variables().find_first("gnawling");
  check(gnaw && host.is_built(gnaw->addr), "the built creature is found by name");
  check(host.variables().find_first("ghost") == nullptr, "a record the factory skips is not registered");
  SharedRandom once(20973);
  once.draw();
  check(live.state == once.state, "GameObject_ctor drew once from the session seed");
  host.run_start();
  // Cls0 is the factory row whose class the research decided as a non-creature
  // (creature_class_table.inc): hideMonster runs on its stub too.
  check(counts.executed[hide] == 3 && counts.refused[hide] == 0,
        "hideMonster runs on the built creature, on no object and on the decided stub");
  check(counts.refused[hp] == 1 && counts.executed[hp] == 0, "a built creature's hit points refuse");
  const auto flags = host.variables().memory.word(gnaw->addr + 0xc);
  check((flags & 0x20) && (flags & 0x8000), "creature flag and the hidden flag");
  check(host.variables().memory.u8(gnaw->addr + 0x231) & 8, "the inert hidden bit");
  check(counts.stub_refusals == 1, "only the hit-point read of the built creature is counted");
}

// research t3/research-items-gaps ecf9742 docs/spec/hide-monster.md 3 and 5:
// hideMonster / showMonster need only whether the class is a Creature. A stub
// whose factory row the probe decided runs; an undecided row stays refused.
void test_hide_show_on_stubs() {
  namespace LF = champions::simulation::rta::level_factory;
  const auto hide = static_cast<std::size_t>(index_of("hideMonster"));
  const auto show = static_cast<std::size_t>(index_of("showMonster"));
  std::string creature, plain, unknown;
  int k = 0;
  for (const auto &row : LF::tests()) {
    const auto name = "Cls" + std::to_string(k++);
    if (row.name == 0x532870)
      creature = name; // chain reaches Creature_ctor
    if (row.name == 0x5323f0)
      plain = name; // chain reaches GameObject_ctor first
    if (row.name == 0x532f78)
      unknown = name; // the probe did not decide
  }
  check(!creature.empty() && !plain.empty() && !unknown.empty(), "authored rows found");
  auto built = build_calls({{"start",
                             {{"hideMonster", {2000}},
                              {"hideMonster", {2100}},
                              {"hideMonster", {2200}},
                              {"showMonster", {2000}},
                              {"showMonster", {2100}}}}},
                           {{2000, "crea"}, {2100, "plain"}, {2200, "mystery"}});
  sc::Host host;
  SharedRandom live(20973);
  host.bind_seed(&live);
  host.set_executable(authored_executable());
  plant(host, built);
  host.run_prestart();
  const auto blob = objects_blob({{creature, "crea"}, {plain, "plain"}, {unknown, "mystery"}});
  check(host.adopt_objects(blob.data(), blob.size()), "stub records adopt");
  auto *c = host.variables().find_first("crea");
  auto *p = host.variables().find_first("plain");
  auto *u = host.variables().find_first("mystery");
  check(c && p && u && host.is_stub(c->addr) && host.is_stub(p->addr) && host.is_stub(u->addr),
        "all three are unconstructed stubs");
  auto &memory = host.variables().memory;
  memory.byte(c->addr + 0x231, 0x01); // a bit Creature_ctor would have seeded
  memory.byte(p->addr + 0x231, 0x01);
  host.run_start();
  const auto &counts = host.counts();
  check(counts.executed[hide] == 2 && counts.refused[hide] == 1,
        "hideMonster runs on the two decided stubs and refuses the undecided one");
  check(counts.executed[show] == 2 && counts.refused[show] == 0, "showMonster runs on both decided stubs");
  check(counts.stub_refusals == 1, "the undecided stub is the only refusal");
  // After the script: both are shown again (the refusal stopped nothing: the
  // answers are discarded), so check the intermediate state with a second run.
  check(!(memory.word(c->addr + 0xc) & 0x8000u) && !(memory.word(p->addr + 0xc) & 0x8000u),
        "showMonster clears the not-run flag");
  check(memory.u8(c->addr + 0x231) == 0x01, "show keeps the other bits of a creature's byte");
  check(memory.u8(p->addr + 0x231) == 0x01, "a non-creature's byte is never touched");
  check(!off_grid(host, c) && !off_grid(host, p), "shown objects are on the grid again");
  // Hide alone: the flag, the off-grid list and, for the creature only, bit 3.
  auto hide_only = build_calls({{"start", {{"hideMonster", {2000}}, {"hideMonster", {2100}}}}},
                               {{2000, "crea"}, {2100, "plain"}});
  sc::Host second;
  SharedRandom live2(20973);
  second.bind_seed(&live2);
  second.set_executable(authored_executable());
  plant(second, hide_only);
  second.run_prestart();
  const auto blob2 = objects_blob({{creature, "crea"}, {plain, "plain"}});
  second.adopt_objects(blob2.data(), blob2.size());
  auto *c2 = second.variables().find_first("crea");
  auto *p2 = second.variables().find_first("plain");
  second.variables().memory.byte(c2->addr + 0x231, 0x01);
  second.variables().memory.byte(p2->addr + 0x231, 0x01);
  second.run_start();
  auto &m2 = second.variables().memory;
  check((m2.word(c2->addr + 0xc) & 0x8000u) && (m2.word(p2->addr + 0xc) & 0x8000u), "both stop running");
  check(!(m2.word(c2->addr + 0xc) & 0x20u), "the stub's flag word never gains the creature bit");
  check(m2.u8(c2->addr + 0x231) == 0x09, "a creature's byte gains bit 3 and keeps the rest");
  check(m2.u8(p2->addr + 0x231) == 0x01, "a non-creature's byte stays");
  check(off_grid(second, c2) && off_grid(second, p2), "both are on the off-grid list");
  check(second.counts().stub_refusals == 0, "no refusal");

  auto gated = build_calls({{"start", {{"hideMonster", {2000}}, {"showMonster", {2100}}}}},
                           {{2000, "hidden"}, {2100, "waiting"}});
  sc::Host third;
  SharedRandom live3(20973);
  third.bind_seed(&live3);
  third.set_executable(authored_executable());
  plant(third, gated);
  const auto blob3 = objects_blob({{creature, "hidden"}, {creature, "waiting"}},
                                  std::vector<std::string>{"hide=1", "wait=1"});
  check(third.adopt_objects(blob3.data(), blob3.size()), "property-bearing stubs adopt");
  third.run_start();
  check(third.counts().refused[hide] == 1 && third.counts().refused[show] == 1 &&
            third.counts().stub_refusals == 2,
        "hide and wait load properties keep unconstructed creature stubs refused");
  check(third.counts().build_refusals.at("creature stub with hide/wait load property") == 2,
        "both load-property refusals are counted");
  unsigned labelled = 0;
  for (const auto &reason : third.counts().stub_refusal_reasons)
    if (reason.first.find("hide/wait load property") != std::string::npos)
      labelled += static_cast<unsigned>(reason.second);
  check(labelled == 2, "command refusal reasons identify the load property");
}

// research t3/research-items-gaps ecf9742 docs/spec/hide-monster.md 4.1 and
// docs/script-items.md 5.1: ScriptGenerator records are built (one shared draw
// each); generateMonster spawns through the factory row and a ported
// constructor, and refuses a class whose constructor is not ported.
void test_generate_monster() {
  namespace LF = champions::simulation::rta::level_factory;
  const auto gen = static_cast<std::size_t>(index_of("generateMonster"));
  std::string generator, plain;
  int k = 0;
  for (const auto &row : LF::tests()) {
    const auto name = "Cls" + std::to_string(k++);
    if (row.call_count == 1 && row.calls[0] == sc::SCRIPT_GENERATOR_CTOR)
      generator = name;
    if (row.name == 0x5323f0)
      plain = name;
  }
  check(!generator.empty() && !plain.empty(), "authored generator row found");
  auto built = build_calls({{"start", {{"generateMonster", {2000, 2100}}, {"generateMonster", {2000, 2200}}}}},
                           {{2000, "maker"}, {2100, "gnaw"}, {2200, plain}});
  sc::Host host;
  SharedRandom live(20973);
  host.bind_seed(&live);
  host.set_executable(authored_executable());
  plant(host, built);
  host.run_prestart();
  const auto blob = objects_blob({{generator, "maker"}});
  check(host.adopt_objects(blob.data(), blob.size()), "the generator record adopts");
  check(host.counts().objects_built == 1 && host.counts().objects_unbuilt == 0, "the generator is built");
  SharedRandom once(20973);
  once.draw();
  check(live.state == once.state, "its GameObject_ctor drew once");
  auto *maker = host.variables().find_first("maker");
  check(maker && maker->type_name == "15ScriptGenerator" && !host.is_stub(maker->addr),
        "the generator is found by name as a ScriptGenerator");
  const auto &memory = host.variables().memory;
  check((memory.word(maker->addr + 0xc) & 9u) == 9u && memory.word(maker->addr + 0x48) == 0x514e60,
        "ScriptGenerator_ctor's flags and table");
  host.run_start();
  const auto &counts = host.counts();
  if (counts.executed[gen] != 1 || counts.refused[gen] != 1)
    std::cerr << "generateMonster executed " << counts.executed[gen] << " refused " << counts.refused[gen]
              << " fault: " << host.last_fault() << '\n';
  check(counts.executed[gen] == 1 && counts.refused[gen] == 1, "the ported class spawns, the plain one refuses");
  // The spawn carries the generator's name, so the name finds two objects.
  const auto found = host.variables().find("maker", 0x200);
  check(found.size() == 2, "the spawned monster carries the generator's name");
  const champions::simulation::rta::objects::GameObject *spawned = nullptr;
  for (const auto *obj : found)
    if (obj != maker)
      spawned = obj;
  check(spawned && spawned->type_name == "4Gnaw", "the spawn is the ported Gnaw");
  check((memory.u64(spawned->addr + 0xa8) & 0x1000000u) != 0, "ScriptGenerator_spawn sets +0xa8 bit 24");
  check(memory.word(spawned->addr + 0x34) == 0x5d32c8, "+0x34 is the interned class name");
  check(counts.objects_built == 2, "the spawn is counted as built");
}

// docs/spec/items-gaps.md 2.3: every machine applies record 0; only modes 2 and
// 3 copy +0xf4 into 0x4f7110.
void test_receive_launch() {
  namespace nl = champions::simulation::rta::net_launch;
  sc::Host host;
  host.start_single_player();
  nl::Record rec;
  rec.difficulty = 0xfe;
  rec.kill_limit = 30;
  rec.mode = 1;
  rec.cm1 = 4;
  host.receive_launch(rec);
  auto &memory = host.variables().memory;
  check(memory.word(0x4f719c) == 0xfffffffeu && memory.u8(0x4f8408) == 30, "difficulty is signed; kill limit");
  check(memory.word(0x4f711c) == 0 && memory.word(0x4f7120) == 1 && memory.word(0x4f7124) == 0, "mode 1: B only");
  check(memory.s32(0x4f7110) == -1, "mode 1 leaves the cm1 word at the image's -1");
  rec.mode = 3;
  host.receive_launch(rec);
  check(memory.word(0x4f711c) == 1 && memory.word(0x4f7124) == 1, "mode 3: A and mode 3");
  check(memory.s32(0x4f7110) == 4, "mode 3 copies the host's +0xf4 into 0x4f7110");
  rec.cm1 = 0xffffffffu; // game_set4f7110(-1) rescans this machine's profile: none set -> 9
  host.receive_launch(rec);
  check(memory.s32(0x4f7110) == 9, "-1 rescans the profile");
}

// docs/spec/item-drops.md section 6 leaves the stack record unknown after ten
// failed generations. The command refuses before any draw even with inputs.
void test_give_random_gate() {
  namespace LF = champions::simulation::rta::level_factory;
  const auto random = static_cast<std::size_t>(index_of("givePlayerRandom"));
  auto built = build_calls({{"start", {{"givePlayerRandom", {}}}}}, {});
  sc::Host host;
  SharedRandom live(20973);
  host.bind_seed(&live);
  host.items().bind_tables(std::make_shared<champions::simulation::rta::items::Tables>());
  plant(host, built);
  host.variables().memory.word(0x4d8ae0, 1);
  check(host.run("start") == sc::Stop::Complete && host.counts().refused[random] == 1 &&
            host.counts().callee_refusals == 1,
        "givePlayerRandom stays refused without bound inputs");
  check(live.state == 20973u, "a gated refusal draws nothing");
  // Supplying the executable and monster level does not lift the refusal.
  sc::Host second;
  SharedRandom live2(20973);
  second.bind_seed(&live2);
  second.items().bind_tables(std::make_shared<champions::simulation::rta::items::Tables>());
  second.set_executable(authored_executable());
  plant(second, built);
  second.bind_monster_level(0x40a00000);
  bool level_name_refused = false;
  try {
    second.bind_level_name("somewhere");
  } catch (const si::Refused &) {
    level_name_refused = true;
  }
  check(level_name_refused && second.variables().memory.cstr(0x41b721).empty(),
        "level-name global refuses without a traced capacity");
  second.variables().memory.word(0x4d8ae0, 1);
  second.run("start");
  check(second.counts().callee_refusals == 1 && live2.state == 20973u,
        "random giver still refuses with an executable and monster level");
}

void test_hit_points_need_faithful_seed() {
  const auto hp = static_cast<std::size_t>(index_of("getHitPoints"));
  auto script = build_calls({{"start", {{"getHitPoints", {2000}}}}},
                            {{2000, "gnawling"}});
  constexpr std::uint32_t level_name_at = 0x7f0000;
  auto stat_image = authored_executable();
  const auto old_string = stat_image.string;
  const auto old_byte = stat_image.byte;
  stat_image.string = [old_string](std::uint32_t at) {
    return at == level_name_at ? std::string("testlevel") : old_string(at);
  };
  stat_image.byte = [old_byte](std::uint32_t at) {
    if (at >= 0x4f7210 && at < 0x4f7214)
      return static_cast<std::uint8_t>(level_name_at >> (8 * (at - 0x4f7210)));
    if (at == 0x4f7214 || (at >= 0x4f7240 && at < 0x4f7244))
      return std::uint8_t{0};
    return old_byte(at);
  };
  const std::uint8_t empty_balance[] = {0, 0};
  for (int scenario = 0; scenario < 5; ++scenario) {
    sc::Host host;
    SharedRandom seed(20973);
    host.bind_seed(&seed);
    host.start_single_player();
    host.set_executable(stat_image);
    host.set_level_stats("testlevel", empty_balance, sizeof(empty_balance));
    if (scenario == 1 || scenario >= 3)
      host.advance_first_level_items("testlevel", true);
    else if (scenario == 2)
      host.advance_first_level_items("testlevel");
    plant(host, script);
    host.run_prestart();
    // Scenario 3: an unbuilt record precedes the creature (its draw is not
    // modelled). Scenario 4: it follows, so the creature's own seed was exact.
    const auto records = scenario == 3
        ? objects_blob({{"Cls0", "ghost"}, {"gnaw", "gnawling"}})
        : scenario == 4 ? objects_blob({{"gnaw", "gnawling"}, {"Cls0", "ghost"}})
                        : objects_blob({{"gnaw", "gnawling"}});
    check(host.adopt_objects(records.data(), records.size()), "seed gate records adopt");
    host.run_start();
    const bool faithful = scenario == 1 || scenario == 4;
    check(host.counts().executed[hp] == (faithful ? 1u : 0u) &&
          host.counts().refused[hp] == (faithful ? 0u : 1u),
          "getHitPoints requires observed gate and no earlier unbuilt draw");
    if (scenario >= 3)
      check(host.counts().load_draws_unmodelled == 1,
            "earlier unbuilt record makes seed inexact");
  }
}


// research t3/research-horsemen-mount 397c35c, docs/spec/horsemen-mount.md sections 1.1 and 2.2: a
// A rider record builds its observed named MiniBoss mount and two reciprocal handles.
void test_mounted_records() {
  namespace LF = champions::simulation::rta::level_factory;
  auto row_class = [](std::uint32_t name) {
    int k = 0;
    for (const auto &row : LF::tests()) {
      if (row.name == name)
        return "Cls" + std::to_string(k);
      ++k;
    }
    return std::string();
  };
  const std::array<std::string, 4> rider_classes = {
      row_class(0x532cc8), row_class(0x532cd8), row_class(0x532ce8), row_class(0x532cf8)};
  const auto nightmare_class = row_class(0x532c98);
  const std::array<std::string, 4> foot_classes = {
      row_class(0x532dc8), row_class(0x532dd8), row_class(0x532de8), row_class(0x532df8)};
  for (const auto &cls : rider_classes) check(!cls.empty(), "a mounted factory row exists");
  for (const auto &cls : foot_classes) check(!cls.empty(), "an on-foot factory row exists");
  check(!nightmare_class.empty(), "the Nightmare factory row exists");
  auto built = build_calls({{"start", {}}}, {});
  sc::Host host;
  SharedRandom live(20973);
  host.bind_seed(&live, sc::SeedPolicy::OfflineSession);
  host.set_executable(authored_executable());
  plant(host, built);
  host.run_prestart();
  const auto blob = objects_blob({{rider_classes[0], "rider0"}, {rider_classes[1], "rider1"},
                                  {rider_classes[2], "rider2"}, {rider_classes[3], "rider3"},
                                  {foot_classes[0], "foot0"}, {foot_classes[1], "foot1"},
                                  {foot_classes[2], "foot2"}, {foot_classes[3], "foot3"},
                                  {nightmare_class, "horse"}});
  check(host.adopt_objects(blob.data(), blob.size()), "mounted records adopt");
  const auto &counts = host.counts();
  // Each mounted row draws twice; each on-foot row and the placed mount once.
  SharedRandom expect(20973);
  for (int i = 0; i < 13; ++i) expect.draw();
  check(live.state == expect.state, "four pairs, four on-foot rows and Nightmare shared draws");
  const std::array<std::string, 4> mount_names = {
      "horsedisease", "horsewar", "horsepestilence", "horsedeath"};
  check(counts.build_refusals.empty(), "all four documented mount lists build");
  const auto &mem = host.variables().memory;
  for (int i = 0; i < 4; ++i) {
    auto *rider = host.variables().find_first("rider" + std::to_string(i));
    auto *mount = host.variables().find_first(mount_names[i]);
    check(rider && mount && host.is_built(rider->addr) && host.is_built(mount->addr),
          "rider and named mount build");
    if (rider && mount) {
      const auto rider_node = mem.word(rider->addr + 0x3c8);
      const auto mount_node = mem.word(mount->addr + 0x3c8);
      check((mem.word(mount->addr + 0xa8) & 1) && !(mem.word(mount->addr + 0x0c) & 0x100000) &&
                mem.u8(rider->addr + 0x470) == i &&
                mem.u16(rider->addr + 0x232) == 0x1b && mem.u16(mount->addr + 0x232) == 0,
            "variant, MiniBoss flag and linked initial states");
      check(rider_node && mount_node && mem.word(rider_node + 12) == mount->addr &&
                mem.word(mount_node + 12) == rider->addr,
            "reciprocal rider-mount handles");
    }
  }
  for (int i = 0; i < 4; ++i) {
    auto *foot = host.variables().find_first("foot" + std::to_string(i));
    check(foot && host.is_built(foot->addr), "on-foot Horsemen record builds");
    check(mem.u8(foot->addr + 0x470) == i && mem.u16(foot->addr + 0x232) == 0 &&
              mem.word(foot->addr + 0x3c8) == 0,
          "on-foot variant, initial state and absent mount link");
  }
  check(counts.objects_unbuilt == 0 && counts.load_draws_unmodelled == 0,
        "all documented Horsemen rows have modelled draws");
  auto *horse = host.variables().find_first("horse");
  check(horse && host.is_built(horse->addr) && counts.objects_built == 9, "all documented rows build");
  check(mem.word(horse->addr + 0x48) == 0x5155e8 && mem.u8(horse->addr + 0xb4) == 10 &&
            mem.word(horse->addr + 0xe0) == 0xc1a00000 && mem.u8(horse->addr + 0x28) == 0x32,
        "the built Nightmare carries its table, kind, seat offset and byte 0x28");
  check(counts.objects_registered == 13, "four pairs, four on-foot rows and Nightmare are registered");

  // Leave exactly one rider-sized block, with no larger fallback block.
  // The rider's pool_new succeeds and its mount's pool_new returns zero.
  sc::Host exhausted;
  SharedRandom one(20973);
  exhausted.bind_seed(&one, sc::SeedPolicy::OfflineSession);
  exhausted.set_executable(authored_executable());
  plant(exhausted, built);
  exhausted.run_prestart();
  auto &pool = exhausted.variables().world->pool;
  for (int i = 0; i < 511; ++i) check(pool.allocate(0x478) != 0, "reserve rider pool block");
  for (int i = 0; i < 32; ++i) check(pool.allocate(3000) != 0, "reserve large fallback pool block");
  const auto one_blob = objects_blob({{rider_classes[0], "unmounted"}});
  check(exhausted.adopt_objects(one_blob.data(), one_blob.size()), "mount allocation refusal stays in level load");
  const auto &no_mount = exhausted.counts();
  check(no_mount.objects_pool_refused == 1 && no_mount.objects_unbuilt == 1 &&
            no_mount.objects_registered == 1 && no_mount.load_draws_unmodelled == 0 &&
            no_mount.build_refusals.count("rider on foot (mount pool exhausted)") == 1,
        "exhausted mount is a labelled stub with its one shared draw accounted");

  sc::Host placed_refusal;
  SharedRandom placed_seed(20973);
  placed_refusal.bind_seed(&placed_seed, sc::SeedPolicy::OfflineSession);
  placed_refusal.set_executable(authored_executable());
  plant(placed_refusal, built);
  placed_refusal.run_prestart();
  const auto dead_blob = objects_blob({{nightmare_class, "dead_horse"}}, "dead=1");
  check(placed_refusal.adopt_objects(dead_blob.data(), dead_blob.size()),
        "placed Nightmare refusal stays in level load");
  const auto &dead = placed_refusal.counts();
  check(dead.objects_unbuilt == 1 && dead.objects_registered == 1 &&
            dead.load_draws_unmodelled == 0 && dead.build_refusals.count("Creature_init dead") == 1,
        "placed Nightmare refusal preserves its completed GameObject draw");
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

// Unknown live charm handles refuse before the binder runs. The executable's
// initial fade word is known, so a step before an explicit reset executes.
void test_callee_refusals() {
  auto built = build_calls(
      {{"start",
        {{"charmMonster", {2000}},
         {"followPlayer", {2000}},
         {"hudFadeScreen", {40}}}}},
      {{2000, "imp"}});
  sc::Host host;
  host.add_object("imp", 0x20, {}, 0);
  auto &memory = host.variables().memory;
  auto *imp = host.variables().find_first("imp");
  check(imp != nullptr, "refusal object exists");
  if (!imp)
    return;
  memory.w32(imp->addr + 0x234, 0x44);
  plant(host, built);
  host.run_prestart();
  check(host.run_start() == sc::Stop::Complete, "discarded refusals do not stop the entry");
  for (const char *name : {"charmMonster", "followPlayer"})
    check(host.counts().refused[at(name)] == 1 && host.counts().executed[at(name)] == 0,
          std::string("callee refusal: ") + name);
  check(host.counts().callee_refusals == 2 && host.counts().stub_refusals == 0, "callee refusals counted");
  // 0x44 is not a node of the host's handle pool: handle_clear cannot run on
  // it, so the charm is still refused (text_host::test_charm_live_target runs
  // a real link).
  check(host.counts().executed[at("hudFadeScreen")] == 1 && host.counts().refused[at("hudFadeScreen")] == 0,
        "fade step executes from the initial word");
  check(memory.u16(imp->addr + 0x312) == 0 && memory.u32(imp->addr + 0x234) == 0x44,
        "a target word outside the handle pool is left alone");
  check((memory.u32(imp->addr + 0xa8) & (1u << 23)) == 0, "followPlayer stores nothing when refused");
  check(host.fade_level() == 40 && memory.u32(se::G_FADE) == 40 && host.callees().calls.size() == 1 &&
            host.callees().calls[0].name == "ui_drawRect", "initial fade zero steps to 40");
  sc::Host next;
  next.set_fade_level(host.fade_level());
  check(next.fade_level() == 40, "a replacement host carries the presentation fade word");
}

// With an explicit caller volume, startMusic and the boss bit of setCFlag run.
void test_music_with_volume() {
  auto built = build_calls({{"start", {{"startMusic", {2200}}, {"setCFlag", {2400, 0, 1}}, {"setCFlag", {2400, 26, 1}}}}},
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
  check(host.counts().executed[at("setCFlag")] == 2 && host.counts().inferred[at("setCFlag")] == 1,
        "only setCFlag's bit-0 boss route is counted as inferred");
  check(memory.u8(broon->addr + 0xa8) & 1u, "setCFlag sets bit 0");
  check(host.audio().queued_count(2) == 1, "setCFlag queues the boss track");
}

// The five text setters sit behind text_lookup. Without a registered string
// table they are refused and nothing is written to the HUD words.
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

void test_object_level_exit() {
  auto built = build_calls({{"exit", {{"newWorld", {2000}}}}}, {{2000, "aa"}});
  sc::Host host;
  plant(host, built);
  host.start_single_player();
  const auto blob = gate_objects();
  check(host.adopt_objects(blob.data(), blob.size()), "exit record adopted");
  check(host.run_object_entry(1, "exit") == sc::Stop::Refused,
        "an absent record cannot supply exit context");
  check(host.run_object_entry(0, "exit") == sc::Stop::Complete,
        "an object exit runs its authored script public");
  check(host.variables().memory.cstr(0x41b6d0) == "aa" &&
            host.variables().memory.read(0x41b720) == 255,
        "newWorld writes the documented next-world request");
  check(host.variables().memory.word(SELECTED_WORD) == 0 &&
            host.variables().memory.word(0x4d8ae4) == 0,
        "object script context is released after the call");
}

void test_refusals() {
  // Item and executed-callee routes have their own tests; these still refuse.
  const char *names[] = {"givePlayerRandom", "setInfo", "dropItem",
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
  // givePlayerRandom stays refused (playerHasItem is routed by the items port).
  const auto has = static_cast<std::size_t>(index_of("givePlayerRandom"));
  const auto setv = static_cast<std::size_t>(index_of("setv"));
  // Discarded: the next call's SYSREQ.C overwrites PRI. The entry goes on.
  auto discarded = build_calls({{"start", {{"givePlayerRandom", {2000}}, {"setv", {2100, 1}}}}},
                               {{2000, "key"}, {2100, "flag"}});
  sc::Host quiet;
  plant(quiet, discarded);
  quiet.run_prestart();
  check(quiet.run_start() == sc::Stop::Complete, "a discarded refusal does not stop the entry");
  check(quiet.counts().refusal_stops == 0 && quiet.counts().executed[setv] == 1,
        "the entry runs on after a discarded refusal");
  // Used: STOR.pri stores the refused answer. The entry stops there.
  auto stored = build_calls({{"start", {{"givePlayerRandom", {2000}}, {".STOR.pri", {2400}},
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
  auto returned = build_calls({{"start", {{"givePlayerRandom", {2000}}}}}, {{2000, "key"}}, true);
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
#include "text_host_tests.hpp"
} // namespace

int main(int argc, char **argv) {
  if (argc != 3) {
    std::cerr << "usage: script-commands-tests VECTORS HOST_VECTORS\n";
    return 2;
  }
  test_table(argv[1]);
  test_host_vectors(argv[2]);
  test_variables_and_seed();
  test_authority_equivalence();
  test_seed_refused_without_session_seed();
  test_offline_seed_policy();
  test_single_player();
  test_unknown_player_currents();
  test_got_item_sound_unknown_currents();
  test_check_alive_which_unknown_currents();
  test_classified_objects();
  test_hit_points_need_faithful_seed();
  test_hide_show_on_stubs();
  test_generate_monster();
  test_receive_launch();
  test_give_random_gate();
  test_mounted_records();
  test_entrance_grid_and_particle();
  test_routed_commands();
  test_callee_refusals();
  test_music_with_volume();
  test_text_setters_refused();
  text_host::test_text_commands();
  text_host::test_text_refusals();
  text_host::test_charm_live_target();
  test_selected_stub();
  test_object_level_exit();
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
