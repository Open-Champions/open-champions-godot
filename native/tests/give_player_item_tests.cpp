// Synthetic tests for the givePlayerItem script native (index 53, handler
// 0x145a88): research-items 5a683b4 docs/give-player-item.md, audits
// research-give-item.md and research-give-item-fix.md (PASS). Every
// definition row, name and record below is authored for this test; no game
// file is read and no retail value is used. The rows only carry the fields the
// rules read (category +0x1d, slot +0x1c, subtype +0x1e, count +0xc, weight
// +0x8, flags +0x28).
#include "champions/amx.hpp"
#include "champions/core_state.hpp"
#include "champions/items.hpp"
#include "champions/script_commands.hpp"
#include "champions/script_items.hpp"
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace a = champions::simulation::rta::amx;
namespace b = champions::simulation::rta::builtins;
namespace si = champions::simulation::rta::script_items;
namespace sc = champions::simulation::rta::script_commands;
namespace it = champions::simulation::rta::items;

namespace {
int failures = 0;
int checks = 0;
void check(bool ok, const std::string &what) {
  ++checks;
  if (!ok) {
    ++failures;
    std::cerr << "FAIL " << what << '\n';
  }
}

constexpr std::uint32_t F_ZERO = 0, F_ONE = 0x3f800000, F_64 = 0x42800000, F_100 = 0x42c80000;
constexpr std::uint64_t FLAG_STACK = 0x100, FLAG_39 = std::uint64_t(1) << 39, FLAG_41 = std::uint64_t(1) << 41;
constexpr std::uint32_t NAMES = 0x01000000;
constexpr std::uint32_t MEDAL_WORD = 0x4f713c, MEDAL_PLAYER = 0x4f7148;
constexpr std::uint32_t EQUIPMENT = 0x43f0, WEAPONS = 0x4434, WEAPON_CLASS = 0x4a86;

struct Row {
  const char *name;
  std::int8_t category, slot, subtype;
  std::int16_t count;
  std::uint32_t weight;
  std::uint64_t flags;
};
// Authored rows. 0..4 are base rows at A_DEFS, the last is one preset row.
const Row BLADE{"fixture blade", 0, 0, 0, 1, F_ONE, 0};
const Row CHARM{"fixture charm", 1, 4, 0, 1, F_ONE, 0};
const Row TOKEN{"fixture token", 2, 9, 0, 1, F_ZERO, FLAG_39};
const Row STACK{"fixture stack", 2, 9, 0, 5, F_ONE, FLAG_STACK};
const Row WARD{"fixture ward", 1, 4, 0, 1, F_ONE, FLAG_41};
const Row PRESET{"fixture charm", 1, 4, 0, 1, F_ONE, 0};
const Row *const BASE[] = {&BLADE, &CHARM, &TOKEN, &STACK, &WARD};

using Image = std::map<std::uint32_t, std::uint8_t>;
void put(Image &image, std::uint32_t at, std::uint64_t value, unsigned n) {
  for (unsigned k = 0; k < n; ++k)
    image[at + k] = std::uint8_t(value >> (8 * k));
}
std::uint32_t row_at(const Row *row) {
  if (row == &PRESET)
    return it::A_PRESETS;
  for (std::uint32_t k = 0; k < 5; ++k)
    if (BASE[k] == row)
      return it::A_DEFS + 0x40 * k;
  return 0;
}
Image authored_image() {
  Image image;
  std::uint32_t name_at = NAMES;
  auto write_row = [&](std::uint32_t at, const Row &row) {
    put(image, at, name_at, 4);
    for (const char *c = row.name; *c; ++c)
      image[name_at++] = std::uint8_t(*c);
    image[name_at++] = 0;
    name_at = (name_at + 15) & ~15u;
    put(image, at + 4, 1, 2);
    put(image, at + 8, row.weight, 4);
    put(image, at + 0xc, std::uint16_t(row.count), 2);
    image[at + 0x1c] = std::uint8_t(row.slot);
    image[at + 0x1d] = std::uint8_t(row.category);
    image[at + 0x1e] = std::uint8_t(row.subtype);
    put(image, at + 0x28, row.flags, 8);
  };
  for (const auto *row : BASE)
    write_row(row_at(row), *row);
  put(image, it::A_DEFS + 0x40 * 5, 0, 4); // terminator
  write_row(it::A_PRESETS, PRESET);
  put(image, it::A_PRESETS + 0x40, 0, 4);
  return image;
}
std::shared_ptr<it::Tables> tables_of(const Image &image) {
  return si::image_tables([&image](std::uint32_t at, std::size_t n) {
    std::vector<std::uint8_t> out(n);
    for (std::size_t k = 0; k < n; ++k) {
      const auto found = image.find(at + std::uint32_t(k));
      out[k] = found == image.end() ? 0 : found->second;
    }
    return out;
  });
}

void prepare_record(b::Memory &memory, std::int32_t slot, std::uint32_t limit) {
  const auto r = si::rec(slot);
  memory.word(r + si::P_LOAD, F_ZERO);
  memory.word(r + si::P_LOAD_LIMIT, limit);
  memory.byte(r + 0x58, 1);
  for (unsigned stat = 0; stat < 4; ++stat)
    memory.word(r + 0x90 + 4 * stat, 20);
  memory.word(r + si::P_SKILL_PAIRS, 0xffffffffu); // skill list terminator
}

// A binder world with the service, as the vectors build it.
struct World {
  Image image = authored_image();
  std::shared_ptr<b::Memory> memory;
  std::unique_ptr<b::Variables> variables;
  std::unique_ptr<si::Service> service;
  std::vector<std::pair<std::uint32_t, std::uint32_t>> parse_args;
  explicit World(std::int32_t players, std::uint32_t limit = F_100) {
    const auto *img = &image;
    memory = std::make_shared<b::Memory>([img](std::uint32_t at, std::size_t n) {
      std::vector<std::uint8_t> out(n);
      for (std::size_t k = 0; k < n; ++k) {
        const auto found = img->find(at + std::uint32_t(k));
        out[k] = found == img->end() ? 0 : found->second;
      }
      return out;
    });
    variables = std::make_unique<b::Variables>(memory);
    variables->local_player_slot = [] { return a::Cell(0); };
    service = std::make_unique<si::Service>(*variables);
    service->bind_tables(tables_of(image));
    // The script host's statement for a handler running outside a level
    // script; tests of the loaded case use the host itself.
    service->command = "givePlayerItem";
    service->level_script_loaded = [] { return false; };
    auto inner = variables->inventory_service;
    variables->inventory_service = [this, inner](const std::string &name,
                                                 const std::vector<b::Variables::InventoryValue> &args) {
      if (name == "item_parseDescriptor") {
        const auto *variant = std::get_if<std::uint32_t>(&args.at(2));
        const auto *presets = std::get_if<std::uint32_t>(&args.at(3));
        parse_args.emplace_back(variant ? *variant : 0xdeadu, presets ? *presets : 0xdeadu);
      }
      return inner(name, args);
    };
    memory->word(si::G_NUM_PLAYERS, std::uint32_t(players));
    for (std::int32_t slot = 0; slot < 4; ++slot)
      prepare_record(*memory, slot, limit);
  }
  // The handler through an AMX machine whose first cell points at the text.
  std::int64_t give(const std::string &text, a::Cell equip, a::Cell use_presets) {
    a::ImageOptions io;
    io.data.resize(4096);
    io.stack_cells = 128;
    a::Machine machine(a::build_image({0}, io));
    if (machine.load() != 0)
      throw std::runtime_error("machine");
    machine.params = 3000;
    machine.set_string(0, text, false);
    machine.set_cell(3004, 0);
    machine.set_cell(3008, std::uint64_t(std::uint32_t(equip)));
    machine.set_cell(3012, std::uint64_t(std::uint32_t(use_presets)));
    return variables->dispatch("givePlayerItem", machine);
  }
  std::int32_t count(std::int32_t slot, std::int32_t bag) const { return memory->s32(si::bag_count_at(slot, bag)); }
  std::uint32_t def(std::int32_t slot, std::int32_t bag, std::int32_t index) const {
    return memory->word(si::bag_entry(slot, bag, index));
  }
  std::size_t called(const std::string &name) const {
    return std::size_t(std::count_if(service->calls.begin(), service->calls.end(),
                                     [&](const si::Service::Call &c) { return c.name == name; }));
  }
};

void argument_contract() {
  for (a::Cell presets : {0, 1}) {
    World w(1);
    const auto result = w.give("fixture charm", 0, presets);
    const auto label = "usePresets " + std::to_string(presets);
    check(result == 0, label + ": result 0");
    check(w.parse_args.size() == 1 && w.parse_args[0].first == 0xffffffffu,
          label + ": the parser gets variant -1 (give-player-item.md:33-38)");
    check(!w.parse_args.empty() && w.parse_args[0].second == std::uint32_t(presets),
          label + ": the parser gets the third cell as usePresets");
    check(w.count(0, 1) == 1 && w.def(0, 1, 0) == row_at(presets ? &PRESET : &CHARM),
          label + ": the row comes from the table usePresets selects");
  }
}

void bags_by_category() {
  const Row *rows[] = {&BLADE, &CHARM, &TOKEN};
  for (const auto *row : rows) {
    World w(1);
    check(w.give(row->name, 0, 0) == 0, std::string(row->name) + ": result 0");
    for (std::int32_t bag = 0; bag < 3; ++bag)
      check(w.count(0, bag) == (bag == row->category ? 1 : 0),
            std::string(row->name) + ": only bag " + std::to_string(row->category) + " receives (give-player-item.md:54-56)");
    check(w.def(0, row->category, 0) == row_at(row), std::string(row->name) + ": index 0 holds the row");
    check(w.memory->u16(si::bag_entry(0, row->category, 0) + 0x22) == 0, std::string(row->name) + ": stamp +0x22 is 0");
  }
}

void recipients() {
  {
    World w(2);
    check(w.give("fixture charm", 0, 0) == 0, "two players: result 0");
    check(w.count(0, 1) == 1 && w.count(1, 1) == 1, "two players: slots 0 and 1 receive (give-player-item.md:45-48)");
    check(w.count(2, 1) == 0, "two players: slot 2 is not written");
  }
  {
    World w(2);
    w.memory->word(MEDAL_WORD, 1);
    w.memory->byte(MEDAL_PLAYER, 1);
    check(w.give("fixture charm", 0, 0) == 0, "medal mode: result 0");
    check(w.count(0, 1) == 0 && w.count(1, 1) == 1,
          "medal mode: only the medal player receives (give-player-item.md:50-52)");
  }
}

void stacking() {
  World w(1);
  w.give("fixture stack", 0, 0);
  w.give("fixture stack", 0, 0);
  check(w.count(0, 2) == 1, "bit 8: the bag count stays 1 (give-player-item.md:60-64)");
  check(w.memory->s16(si::bag_entry(0, 2, 0) + 4) == 2 * STACK.count, "bit 8: the definition count is added");
  check(w.def(0, 2, 1) == 0, "bit 8: the next cell stays empty");
  w.memory->byte(si::bag_entry(0, 2, 0) + 0x20, 0);
  check(w.give("fixture stack", 0, 0) == 0, "changed prefix: result 0");
  check(w.count(0, 2) == 2 && w.def(0, 2, 1) == row_at(&STACK), "a changed prefix byte appends a second cell");
  w.memory->byte(si::bag_entry(0, 2, 1) + 0x21, 0);
  w.memory->byte(si::bag_entry(0, 2, 0) + 0x20, 0xff);
  w.give("fixture stack", 0, 0);
  check(w.count(0, 2) == 2 && w.memory->s16(si::bag_entry(0, 2, 0) + 4) == 3 * STACK.count,
        "the first matching cell stacks when definition, flags, prefix and suffix match");
}

void full_bag() {
  {
    World w(1);
    w.memory->word(si::bag_count_at(0, 0), 63);
    check(w.give("fixture blade", 0, 0) == 0, "count 63: result 0");
    check(w.def(0, 0, 63) == row_at(&BLADE) && w.memory->u16(si::bag_entry(0, 0, 63) + 0x22) == 63 && w.count(0, 0) == 64,
          "count 63 appends at index 63 with stamp 63 (give-player-item.md:69-71)");
    check(w.memory->word(si::rec(0) + si::P_LOAD) == F_64, "count 63: load is 63 empty cells and the item");
  }
  {
    World w(1);
    w.memory->word(si::bag_count_at(0, 0), 64);
    check(w.give("fixture blade", 0, 0) == 0, "count 64: the handler result is still 0 (give-player-item.md:73-75)");
    bool empty = true;
    for (std::int32_t k = 0; k < 64; ++k)
      empty = empty && w.def(0, 0, k) == 0;
    check(empty && w.count(0, 0) == 64, "count 64: the item is lost; 64 cells stay empty and the count stays 64");
    check(w.service->counts.lost == 1 && w.service->counts.given == 0, "count 64: counted as lost");
    check(w.memory->word(si::rec(0) + si::P_LOAD) == F_64, "count 64: player_recalcLoad still runs; load 64.0 (:76-77)");
    check(w.called("pool_new") == 0, "count 64: nothing is dropped");
  }
}

void weight() {
  {
    World w(1, F_ZERO);
    check(w.give("fixture token", 0, 0) == 0, "zero weight at limit 0: result 0");
    check(w.count(0, 2) == 1, "a zero weight word is given at limit 0 (give-player-item.md:90-93)");
  }
  {
    World w(1, F_ZERO);
    bool refused = false;
    std::string why;
    try {
      w.give("fixture blade", 0, 0);
    } catch (const si::Refused &error) {
      refused = true;
      why = error.what();
    }
    check(refused && why.find("ItemProp_ctor2") != std::string::npos,
          "positive weight at limit 0: the ground prop refuses (ItemProp_ctor2 body Unknown, :183)");
    check(w.count(0, 0) == 0, "positive weight at limit 0: the bag is not touched (:86-87)");
  }
  {
    // The research probe's own stub: pool_new returned 0, so the constructor
    // was not entered and the handler returned 0 (give-player-item.md:86-88).
    World w(1, F_ZERO);
    std::vector<std::uint64_t> sizes;
    w.service->delegates["pool_new"] = [&sizes](const std::vector<std::uint64_t> &args) {
      sizes.push_back(args.at(0));
      return std::uint64_t(0);
    };
    check(w.give("fixture blade", 0, 0) == 0, "drop with pool_new 0: result 0 (:96)");
    check(sizes.size() == 1 && sizes[0] == 0x190, "drop: one pool_new(0x190)");
    check(w.called("ItemProp_ctor2") == 0 && w.count(0, 0) == 0, "drop with pool_new 0: no constructor, bag untouched");
  }
  {
    World w(1);
    w.memory->word(si::rec(0) + si::P_LOAD, 0x42c60000); // 99.0 + 1.0 is not below 100.0
    w.service->delegates["pool_new"] = [](const std::vector<std::uint64_t> &) { return std::uint64_t(0); };
    w.give("fixture blade", 0, 0);
    check(w.count(0, 0) == 0, "carried + weight equal to the limit does not fit (strict less-than, :85)");
  }
}

void flag_39() {
  World w(1);
  check(w.give("fixture token", 0, 0) == 0, "flag 39: result 0");
  check(w.count(0, 2) == 1 && (w.memory->u64(si::bag_entry(0, 2, 0) + 8) & FLAG_39) != 0,
        "flag 39: bag 2 with the row's own bit 39 (give-player-item.md:104-109)");
  check(w.called("inventory_removeItem") == 0 && w.service->counts.removed == 0,
        "flag 39: the keep list and its remover are not applied at give time (:109-111)");
}

void equipment() {
  {
    World w(1);
    const auto r = si::rec(0);
    w.memory->byte(r + WEAPON_CLASS, 0xa5);
    check(w.give("fixture blade", 1, 0) == 0, "weapon equip: result 0");
    const auto cell = si::bag_entry(0, 0, 0);
    check(w.memory->word(r + EQUIPMENT) == cell, "check 0: equipment +0x43f0 holds the new cell (give-player-item.md:129-131)");
    check(w.memory->u8(r + WEAPON_CLASS) == std::uint8_t(BLADE.subtype), "check 0: +0x4a86 becomes the subtype (sentinel 0xa5)");
    check(w.memory->word(r + WEAPONS) == cell, "check 0: the weapon set holds the cell");
    std::size_t runs = 0;
    for (const auto &c : w.service->calls)
      if (c.name == "script_runByName")
        runs += c.args.size() == 1 && c.args[0] == 0x538048;
    check(runs == 1 && w.called("script_runByName") == 1, "script_runByName is called once and returns without a level script (:132-134)");
    check(w.called("player_resetIdleAnim") >= 1, "the idle-anim reset follows the equip (:123-125)");
    check(w.service->counts.equipped == 1, "the equip is counted");
  }
  {
    World w(1);
    w.service->level_script_loaded = nullptr;
    bool refused = false;
    try {
      w.give("fixture blade", 1, 0);
    } catch (const si::Refused &) {
      refused = true;
    }
    check(refused && w.memory->word(si::rec(0) + EQUIPMENT) == 0,
          "no statement about g_scriptMachine: the weapon hook refuses and commits no equipment");
  }
  {
    World w(1);
    check(w.give("fixture ward", 1, 0) == 0, "flag 41 equip: result 0");
    const auto cell = si::bag_entry(0, 1, 0);
    check(w.count(0, 1) == 1 && w.def(0, 1, 0) == row_at(&WARD), "flag 41: the item is placed in the bag (:138)");
    bool held = false;
    for (std::uint32_t k = 0; k < 16; ++k)
      held = held || w.memory->word(si::rec(0) + EQUIPMENT + 4 * k) == cell;
    check(!held && w.service->counts.equipped == 1, "flag 41: equip is called and the cell is not left equipped (:138-139)");
  }
  {
    // Kept refusal: the equip after a lost give targets 0x48 bytes before bag 0.
    World w(1);
    w.memory->word(si::bag_count_at(0, 0), 64);
    bool refused = false;
    try {
      w.give("fixture blade", 1, 0);
    } catch (const si::Refused &) {
      refused = true;
    }
    check(refused && w.count(0, 0) == 64 && w.memory->word(si::rec(0) + si::P_LOAD) == F_64,
          "lost give with equip: the load recompute ran, then the equip before bag 0 refuses");
  }
}

// ---- through the script host ----------------------------------------------------------------
struct Call {
  std::string native;
  std::vector<a::Cell> args;
};
a::ByteVector script(const std::vector<Call> &calls) {
  std::vector<std::string> natives;
  std::vector<a::AsmLine> program;
  auto ins = [&](const std::string &name, std::vector<a::AsmValue> args = {}) {
    program.push_back(a::AsmInstruction{name, std::move(args), {}, {}});
  };
  program.push_back(a::Label{"start:"});
  ins("PROC");
  for (const auto &call : calls) {
    if (call.native[0] == '.') {
      ins(call.native.substr(1), std::vector<a::AsmValue>(call.args.begin(), call.args.end()));
      continue;
    }
    auto found = std::find(natives.begin(), natives.end(), call.native);
    if (found == natives.end())
      found = natives.insert(natives.end(), call.native);
    for (auto arg = call.args.rbegin(); arg != call.args.rend(); ++arg)
      ins("PUSH.C", {*arg});
    ins("PUSH.C", {a::Cell(4 * call.args.size())});
    ins("SYSREQ.C", {a::Cell(found - natives.begin())});
    ins("STACK", {a::Cell(4 * (call.args.size() + 1))});
  }
  ins("ZERO.pri");
  ins("RETN");
  const auto code = a::assemble(program);
  a::ImageOptions options;
  options.data.resize(4096);
  options.natives = natives;
  options.stack_cells = 256;
  options.publics.emplace_back("start", a::Cell(code.labels.at("start")));
  return a::build_image(code.cells, options);
}
std::size_t index_of(const char *name) {
  for (std::size_t i = 0; i < sc::COMMANDS; ++i)
    if (std::string(sc::row(i).name) == name)
      return i;
  throw std::runtime_error(name);
}
struct HostWorld {
  champions::simulation::rta::SharedRandom seed;
  sc::Host host;
  HostWorld(const std::string &text, a::Cell equip, std::int32_t bag_count) {
    const auto image = authored_image();
    auto &memory = host.variables().memory;
    for (const auto &byte : image)
      memory.byte(byte.first, byte.second);
    host.bind_seed(&seed);
    host.items().bind_tables(tables_of(image));
    memory.word(si::G_NUM_PLAYERS, 1);
    prepare_record(memory, 0, F_100);
    memory.word(si::bag_count_at(0, 0), std::uint32_t(bag_count));
    check(host.load(script({{"givePlayerItem", {2000, equip, 0}}, {".STOR.pri", {2400}}})), "synthetic script loads");
    host.machine()->set_string(2000, text, false);
    host.machine()->set_cell(2400, 0x55555555u);
    host.run_prestart();
  }
};

void host_paths() {
  for (std::int32_t bag_count : {0, 64}) {
    HostWorld w("fixture blade", 0, bag_count);
    const auto label = std::string(bag_count ? "host, full bag" : "host, success");
    check(w.host.run_start() == sc::Stop::Complete, label + ": the entry completes");
    check(w.host.machine()->cell(2400) == 0, label + ": PRI is 0 through the dispatcher (give-player-item.md:96-100)");
    check(w.host.counts().executed[index_of("givePlayerItem")] == 1 && w.host.counts().refused[index_of("givePlayerItem")] == 0,
          label + ": executed, not refused");
    check(w.host.variables().memory.s32(si::bag_count_at(0, 0)) == (bag_count ? 64 : 1), label + ": bag count");
    check(w.host.items().command.empty(), label + ": the command scope is closed after the handler");
  }
  {
    HostWorld w("fixture blade", 1, 0);
    check(w.host.run_start() == sc::Stop::Refused, "host weapon equip: a loaded level script stops the entry");
    check(w.host.counts().refused[index_of("givePlayerItem")] == 1, "host weapon equip: counted as refused");
    check(w.host.last_fault().find("level script loaded") != std::string::npos,
          "host weapon equip: the reason names the loaded level script (give-player-item.md:135-136)");
    const auto &memory = w.host.variables().memory;
    check(memory.s32(si::bag_count_at(0, 0)) == 1, "host weapon equip: the give before the refusal stays");
    check(memory.word(si::rec(0) + EQUIPMENT) == 0, "host weapon equip: no equipment pointer is committed");
    check(w.host.machine()->cell(2400) == 0x55555555u, "host weapon equip: nothing after the refusal runs");
  }
}
} // namespace

int main() {
  argument_contract();
  bags_by_category();
  recipients();
  stacking();
  full_bag();
  weight();
  flag_39();
  equipment();
  host_paths();
  std::cout << checks << " checks, " << failures << " failures\n";
  return failures == 0 ? 0 : 1;
}
