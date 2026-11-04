#include "champions/campaign_entry.hpp"
#include "champions/campaign_inventory.hpp"
#include "champions/script_commands.hpp"
#include "champions/script_items.hpp"
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace ci = champions::simulation::rta::campaign_inventory;
namespace q = champions::simulation::rta::quests;
namespace sc = champions::simulation::rta::script_commands;
namespace si = champions::simulation::rta::script_items;

namespace {
int failed = 0;
void check(bool ok, const std::string &what) {
  if (ok)
    return;
  std::cerr << "FAIL " << what << '\n';
  ++failed;
}

constexpr std::uint64_t BIT39 = std::uint64_t(1) << 39;
constexpr std::uint32_t BASE = 0x72000000u;

struct Cell {
  std::string name;
  std::uint64_t flags = 0;
  int bag = 0;
  int equip = -1;
  int weapon = -1;
  bool second = false;
  int stamp = 0;
  int stack = 1;
  int equip_byte = 0;
};

struct World {
  sc::Host host;
  std::uint32_t cursor = BASE;
  std::uint32_t text(const std::string &name) {
    const auto at = cursor;
    cursor += 64;
    host.variables().memory.string(at, name);
    return at;
  }
  std::uint32_t define(const std::string &name, int bag, int equip_byte) {
    const auto at = cursor;
    cursor += 64;
    auto &memory = host.variables().memory;
    memory.w32(at, text(name));
    memory.w8(at + 0x1c, static_cast<std::uint8_t>(equip_byte));
    memory.w8(at + 0x1d, static_cast<std::uint8_t>(bag));
    return at;
  }
  void plant(int slot, const std::vector<Cell> &cells) {
    auto &memory = host.variables().memory;
    const auto rec = si::rec(slot);
    int counts[3] = {};
    for (const auto &cell : cells) {
      const auto index = counts[cell.bag]++;
      const auto addr = si::bag_entry(slot, cell.bag, index);
      const auto def = define(cell.name, cell.bag, cell.equip_byte);
      memory.w32(addr, def);
      memory.w16(addr + 4, static_cast<std::uint16_t>(cell.stack));
      memory.w64(addr + 8, cell.flags);
      memory.w16(addr + 0x22, static_cast<std::uint16_t>(cell.stamp));
      if (cell.equip >= 0)
        memory.w32(rec + 0x43f0 + static_cast<std::uint32_t>(cell.equip) * 4, addr);
      if (cell.weapon >= 0)
        memory.w32(rec + 0x4434 + static_cast<std::uint32_t>(cell.weapon) * 4, addr);
      if (cell.second)
        memory.w32(rec + 0x443c, addr);
    }
    for (int bag = 0; bag < 3; ++bag)
      memory.w32(si::bag_count_at(slot, bag), static_cast<std::uint32_t>(counts[bag]));
  }
};

void arm_progress(sc::Host &host) {
  auto &memory = host.variables().memory;
  memory.word(si::G_NUM_PLAYERS, 2u);
  const auto rec = si::rec(0);
  memory.w32(rec + 0xe4, 1234);
  memory.w32(rec + 0x5c, 50);
  memory.w8(rec + 0x59, 3);
  memory.w8(rec + 0x4bbf, 3);
  memory.w8(rec + 0x4bcc, 3);
  memory.w8(rec + 0x4c0d, 1);
  memory.string(rec + 0x41d0, "Aquest");
  memory.w32(rec + 0x43d0, 1);
  memory.w32(si::rec(1) + 0xe4, 9);
  memory.string(si::rec(1) + 0x41d0, "Other");
  memory.w32(si::rec(1) + 0x43d0, 1);
  memory.string(si::rec(2) + 0x41d0, "Canary");
  memory.w32(si::rec(2) + 0x43d0, 1);
  memory.string(0x86da80, "currentMission");
  memory.byte(0x86da80 + 15, 5);
  memory.string(0x86da80 + 16, "step");
  memory.byte(0x86da80 + 21, 9);
  memory.byte(0x86da80 + 22, 0);
  memory.w32(ci::ACT_MARK, 0);
  memory.w32(ci::ACT_WORD, 0);
}

std::vector<Cell> sample_cells() {
  return {
      {"questgem", BIT39, 0, 0, 0, false, 4, 1, 0},
      {"sword", 0, 0, 2, -1, true, 9, 1, 2},
      {"pickaxe", BIT39, 2, -1, -1, false, 1, 1, 0},
      {"bloodstonefay", BIT39, 2, -1, -1, false, 2, 1, 0},
      {"kept-plain", 0, 1, -1, -1, false, 0, 1, 0},
      {"not-bit-39", std::uint64_t(1) << 38, 1, -1, -1, false, 3, 1, 0},
  };
}

std::string name_at(sc::Host &host, int slot, int bag, int index) {
  const auto &memory = host.variables().memory;
  const auto def = memory.u32(si::bag_entry(slot, bag, index));
  return def ? memory.cstr(memory.u32(def)) : std::string();
}

void expect_untouched(sc::Host &host, const std::string &label) {
  const auto &memory = host.variables().memory;
  check(memory.s32(si::bag_count_at(0, 0)) == 2 && name_at(host, 0, 0, 0) == "questgem" &&
            name_at(host, 0, 0, 1) == "sword",
        label + " bags stay");
  check(memory.u32(si::rec(0) + 0x43f0) == si::bag_entry(0, 0, 0), label + " equipment stays");
  check(memory.s32(si::rec(0) + 0x43d0) == 1 && memory.cstr(si::rec(0) + 0x41d0) == "Aquest",
        label + " quest stays");
  check(memory.u8(0x86da80 + 15) == 5 && memory.u8(0x86da80 + 21) == 9, label + " variable values stay");
  check(memory.u32(ci::ACT_MARK) == 0, label + " mark stays 0");
  check(memory.s32(si::rec(0) + 0xe4) == 1234 && memory.s32(si::rec(0) + 0x5c) == 50 &&
            memory.u8(si::rec(0) + 0x59) == 3 && memory.u8(si::rec(0) + 0x4bbf) == 3 &&
            memory.u8(si::rec(0) + 0x4bcc) == 3 && memory.u8(si::rec(0) + 0x4c0d) == 1,
        label + " gold experience mission medal and level stay");
}

void test_travel_gates() {
  const auto acts = ci::documented_story_acts();
  auto run = [&](ci::Request request, const auto &body) {
    World world;
    world.plant(0, sample_cells());
    world.plant(1, {{"othergem", BIT39, 0, -1, -1, false, 0, 1, 0}});
    world.plant(2, {{"canary", BIT39, 0, -1, -1, false, 0, 1, 0}});
    arm_progress(world.host);
    if (request.act == -1)
      world.host.variables().memory.w32(ci::ACT_WORD, 0xffffffffu);
    const auto result = ci::apply_campaign_travel(world.host.variables(), acts, request);
    body(result, world.host);
  };

  run({"strife", 0, false, false, false}, [](const ci::Result &result, sc::Host &host) {
    check(result.changed == 0 && result.act == 0 && !result.cleared, "same act returns 0 and stores 0");
    expect_untouched(host, "same act");
  });
  run({"hub2", 0, false, false, false}, [](const ci::Result &result, sc::Host &host) {
    check(result.changed == 0 && result.act == 0 && !result.cleared &&
              host.variables().memory.s32(ci::ACT_WORD) == 0,
          "hub2 from act 0 stores 0");
    expect_untouched(host, "hub2");
  });
  run({"inmine2", 1, false, false, false}, [](const ci::Result &result, sc::Host &host) {
    check(result.changed == 0 && result.act == 1 && !result.cleared, "mission 2 chain stays in act 1");
    check(host.variables().memory.u32(ci::ACT_MARK) == 0, "same-act chain does not store the mark");
  });
  run({"innovat1", 0, false, false, false}, [](const ci::Result &result, sc::Host &host) {
    const auto &memory = host.variables().memory;
    check(result.changed == 1 && result.act == 1 && result.cleared && memory.s32(ci::ACT_WORD) == 1,
          "act change stores 1");
    check(memory.u32(ci::ACT_MARK) == 1, "act change stores 1 at 0x4f7118");
    check(memory.s32(si::bag_count_at(0, 0)) == 1 && name_at(host, 0, 0, 0) == "sword",
          "unkept flag-39 cell is removed and the ordinary item slides");
    check(memory.u32(si::rec(0) + 0x43f0) == 0, "removed cell's equipment slot is cleared");
    check(memory.u32(si::rec(0) + 0x43f0 + 8) == si::bag_entry(0, 0, 0),
          "later equipment pointer follows the slide");
    check(memory.u32(si::rec(0) + 0x4434) == 0 && memory.u8(si::rec(0) + 0x4a86) == 5,
          "active weapon word clears and the weapon class becomes 5");
    check(memory.u32(si::rec(0) + 0x443c) == si::bag_entry(0, 0, 0),
          "second weapon pointer follows the slide");
    check(memory.u16(si::bag_entry(0, 0, 0) + 0x22) == 8, "slid stamp drops by one");
    check(name_at(host, 0, 2, 0) == "pickaxe" && name_at(host, 0, 2, 1) == "bloodstonefay",
          "kept names stay");
    check(memory.s32(si::bag_count_at(0, 1)) == 2, "ordinary and bit-38 cells stay");
    check(memory.s32(si::bag_count_at(1, 0)) == 0, "the other joined player's unkept cell is removed");
    check(name_at(host, 2, 0, 0) == "canary" && memory.s32(si::rec(2) + 0x43d0) == 1 &&
              memory.cstr(si::rec(2) + 0x41d0) == "Canary",
          "a record at or above the player count is not walked");
    check(memory.s32(si::rec(0) + 0x43d0) == 0 && memory.cstr(si::rec(0) + 0x41d0) == "NoQuests" &&
              memory.s32(si::rec(1) + 0x43d0) == 0,
          "quest counts become 0 and the used title is the placeholder");
    check(memory.cstr(0x86da80) == "currentMission" && memory.u8(0x86da80 + 15) == 0 &&
              memory.cstr(0x86da80 + 16) == "step" && memory.u8(0x86da80 + 21) == 0,
          "quest variable names stay and their values become 0");
    check(memory.s32(si::rec(0) + 0xe4) == 1234 && memory.s32(si::rec(1) + 0xe4) == 9 &&
              memory.s32(si::rec(0) + 0x5c) == 50 && memory.u8(si::rec(0) + 0x59) == 3 &&
              memory.u8(si::rec(0) + 0x4bbf) == 3 && memory.u8(si::rec(0) + 0x4c0d) == 1,
          "gold experience level mission and medal stay");
  });
  run({"innovat1", 0, false, true, false}, [](const ci::Result &result, sc::Host &host) {
    check(result.changed == 1 && result.act == 1 && !result.cleared, "card latch stores the act and skips the clear");
    expect_untouched(host, "latch");
    check(host.variables().memory.s32(ci::ACT_WORD) == 1, "latch still stores act 1");
  });
  run({"innovat1", 0, false, false, true}, [](const ci::Result &result, sc::Host &host) {
    check(result.changed == 1 && result.act == 1 && !result.cleared, "death task stores the act and skips the clear");
    expect_untouched(host, "death");
  });
  {
    World world;
    world.plant(0, sample_cells());
    arm_progress(world.host);
    world.host.variables().memory.w32(ci::FRONTEND_WORD, 1u);
    const auto result = ci::apply_campaign_travel(world.host.variables(), acts,
                                                   {"innovat1", 0, true, false, false});
    check(result.changed == 0 && result.act == 0 && !result.cleared, "frontend skips the act call");
    expect_untouched(world.host, "frontend");
    check(world.host.variables().memory.u32(ci::FRONTEND_WORD) == 1 &&
              world.host.variables().memory.s32(ci::ACT_WORD) == 0,
          "frontend word is not stored and the act stays");
  }
  run({"blood1", -1, false, false, false}, [](const ci::Result &result, sc::Host &host) {
    check(result.changed == 1 && result.act == 0 && result.cleared &&
              host.variables().memory.s32(ci::ACT_WORD) == 0 &&
              host.variables().memory.u32(ci::ACT_MARK) == 1,
          "from act -1 stores 0 and the clear runs");
    check(name_at(host, 0, 0, 0) == "sword", "from act -1 removes the unkept cell");
  });
}

void test_share() {
  World world;
  world.host.variables().memory.word(si::G_NUM_PLAYERS, 3u);
  world.plant(0, {{"bag0", BIT39, 0, -1, -1, false, 0, 1, 0},
                  {"plain", 0, 2, -1, -1, false, 0, 1, 0},
                  {"questgem", BIT39, 2, -1, -1, false, 1, 4, 0}});
  world.plant(1, {{"keepbag0", BIT39, 0, -1, -1, false, 0, 1, 0},
                  {"keepbag1", BIT39, 1, -1, -1, false, 0, 1, 0},
                  {"plain", 0, 2, -1, -1, false, 0, 1, 0},
                  {"oldquest", BIT39, 2, 0, -1, false, 1, 1, 0}});
  world.plant(2, {{"canary", BIT39, 2, -1, -1, false, 0, 1, 0}});
  for (int slot = 0; slot < 4; ++slot) {
    world.host.variables().memory.w32(si::rec(slot) + 0xe4, static_cast<std::uint32_t>(40 + slot));
    world.host.variables().memory.w8(si::rec(slot) + 0x4bbf, 6);
  }
  const auto source_before = world.host.variables().memory.read(si::bag_entry(0, 2, 0), 0x48 * 2);
  ci::share_quest_items(world.host.variables(), 0);
  const auto &memory = world.host.variables().memory;
  check(memory.read(si::bag_entry(0, 2, 0), 0x48 * 2) == source_before, "source bag 2 is unchanged");
  check(name_at(world.host, 0, 0, 0) == "bag0", "bag 0 is not shared");
  check(name_at(world.host, 1, 0, 0) == "keepbag0" && name_at(world.host, 1, 1, 0) == "keepbag1",
        "bags 0 and 1 on the destination are not dropped");
  check(name_at(world.host, 1, 2, 0) == "plain" && name_at(world.host, 1, 2, 1) == "questgem",
        "destination bag 2 loses its flag-39 cell and gains the copy");
  check(memory.u16(si::bag_entry(1, 2, 1) + 4) == 4, "the copy keeps the stack count");
  check(memory.u32(si::rec(1) + 0x43f0) == 0, "the copy is not equipped");
  check(name_at(world.host, 2, 2, 0) == "questgem", "slot 2 below the player count receives the copy");
  for (int slot = 0; slot < 4; ++slot)
    check(memory.s32(si::rec(slot) + 0xe4) == 40 + slot && memory.u8(si::rec(slot) + 0x4bbf) == 6,
          "share leaves gold and the mission byte");

  World empty;
  empty.host.variables().memory.word(si::G_NUM_PLAYERS, 2u);
  empty.plant(1, {{"oldquest", BIT39, 2, -1, -1, false, 0, 1, 0},
                  {"plain", 0, 2, -1, -1, false, 1, 1, 0}});
  ci::share_quest_items(empty.host.variables(), 0);
  check(empty.host.variables().memory.s32(si::bag_count_at(1, 2)) == 1 &&
            name_at(empty.host, 1, 2, 0) == "plain",
        "an empty source still removes the other bag-2 flag-39 cells");

  World bit38;
  bit38.host.variables().memory.word(si::G_NUM_PLAYERS, 2u);
  bit38.plant(0, {{"questgem", std::uint64_t(1) << 38, 2, -1, -1, false, 0, 1, 0}});
  bit38.plant(1, {{"plain", 0, 2, -1, -1, false, 0, 1, 0}});
  ci::share_quest_items(bit38.host.variables(), 0);
  check(bit38.host.variables().memory.s32(si::bag_count_at(1, 2)) == 1 &&
            name_at(bit38.host, 1, 2, 0) == "plain",
        "bit 38 is not bit 39");

  World from1;
  from1.host.variables().memory.word(si::G_NUM_PLAYERS, 2u);
  from1.plant(1, {{"pickaxe", BIT39, 2, -1, -1, false, 0, 2, 0}});
  ci::share_quest_items(from1.host.variables(), 1);
  check(name_at(from1.host, 0, 2, 0) == "pickaxe" &&
            from1.host.variables().memory.u16(si::bag_entry(0, 2, 0) + 4) == 2,
        "slot 1 copies its bag 2 to the other slot");

  World stacked;
  stacked.host.variables().memory.word(si::G_NUM_PLAYERS, 2u);
  stacked.plant(0, {{"questgem", BIT39 | ci::FLAG_STACK, 2, -1, -1, false, 0, 1, 0}});
  stacked.plant(1, {{"oldquest", BIT39, 2, -1, -1, false, 0, 1, 0}});
  bool refused = false;
  try {
    ci::share_quest_items(stacked.host.variables(), 0);
  } catch (const std::runtime_error &) {
    refused = true;
  }
  check(refused && name_at(stacked.host, 1, 2, 0) == "oldquest",
        "flag bit 8 refuses before the other bag is edited");
}

void test_entry_carry() {
  q::ActTables tables;
  tables.names = ci::documented_story_acts();
  for (const auto &act : tables.names) {
    champions::simulation::rta::level_flow::Act flow;
    for (const auto &name : act)
      flow.push_back({name, 0});
    tables.flow.push_back(std::move(flow));
  }
  World source;
  source.host.start_single_player();
  source.host.campaign().set_act(0);
  source.host.campaign().set_acts(tables.names);
  source.plant(0, {{"questgem", BIT39, 0, 0, 0, false, 1, 1, 0},
                   {"sword", 0, 0, 2, -1, true, 4, 1, 2}});
  source.host.variables().quest_set("authored-progress", 7);
  source.host.variables().memory.string(si::rec(0) + 0x41d0, "authored-title");
  source.host.variables().memory.string(si::rec(0) + 0x41f0, "authored-body");
  source.host.variables().memory.w32(si::rec(0) + 0x43d0, 1);
  const auto image = source.host.save_campaign();
  const auto definitions = source.host.variables().memory.read(BASE, 0x4000);
  auto share_definitions = [&](sc::Host &host) { host.variables().memory.write(BASE, definitions); };

  sc::Host same;
  same.load_campaign(image);
  share_definitions(same);
  // The session copies bags before entry. Same-act entry must leave them.
  same.variables().memory.word(si::G_NUM_PLAYERS, 1u);
  // Re-plant through a temporary host is unnecessary: copy the source bytes.
  same.variables().memory.write(si::rec(0) + 0x130, source.host.variables().memory.read(si::rec(0) + 0x130, 3 * 0x1208));
  same.variables().memory.write(si::rec(0) + 0x43f0, source.host.variables().memory.read(si::rec(0) + 0x43f0, 0x50));
  const auto same_entry = sc::prepare_campaign_entry(same, &source.host, tables, "strife", false, false);
  check(!same_entry.changed && !same_entry.reset && same.campaign().record(0).quest_count == 1 &&
            same.variables().quest_get("authored-progress") == 7 &&
            same.variables().memory.s32(si::bag_count_at(0, 0)) == 2,
        "same-act entry leaves the copied bags and the quests");

  sc::Host next;
  next.load_campaign(image);
  share_definitions(next);
  const auto changed = sc::prepare_campaign_entry(next, &source.host, tables, "innovat1", false, false);
  check(changed.changed && changed.reset && changed.act == 1, "classified cells cross the act");
  check(next.variables().memory.s32(si::bag_count_at(0, 0)) == 1, "the unkept cell is removed on the destination");
  const auto def = next.variables().memory.u32(si::bag_entry(0, 0, 0));
  check(def && next.variables().memory.cstr(next.variables().memory.u32(def)) == "sword",
        "the ordinary item remains");
  check(next.variables().memory.u32(si::rec(0) + 0x43f0 + 8) == si::bag_entry(0, 0, 0),
        "destination equipment pointer follows the slide");
  check(next.campaign().record(0).quest_count == 0 && next.variables().quest_get("authored-progress") == 0,
        "the act change still clears the journal and the variable value");
  check(next.variables().memory.u32(ci::ACT_MARK) == 1, "the destination stores the act-change mark");

  source.host.variables().memory.w8(ci::CARD_LATCH, 1);
  sc::Host held;
  held.load_campaign(image);
  share_definitions(held);
  const auto latched = sc::prepare_campaign_entry(held, &source.host, tables, "innovat1", false, false);
  check(latched.changed && !latched.reset && held.campaign().record(0).quest_count == 1 &&
            held.variables().memory.s32(si::bag_count_at(0, 0)) == 2 &&
            held.variables().memory.u32(ci::ACT_MARK) == 0,
        "the card latch keeps the copied bags and the journal");
  source.host.variables().memory.w8(ci::CARD_LATCH, 0);

  source.host.variables().memory.w32(ci::FRONTEND_WORD, 1u);
  sc::Host front;
  front.load_campaign(image);
  share_definitions(front);
  const auto skipped = sc::prepare_campaign_entry(front, &source.host, tables, "innovat1", false, false);
  check(!skipped.changed && !skipped.reset && front.campaign().act() == 0 &&
            front.variables().memory.u32(ci::FRONTEND_WORD) == 1 &&
            front.variables().memory.s32(si::bag_count_at(0, 0)) == 2,
        "the frontend word skips the act call and leaves the bags");
}

} // namespace

int main() {
  test_travel_gates();
  test_share();
  test_entry_carry();
  if (failed) {
    std::cerr << failed << " failed\n";
    return 1;
  }
  std::cout << "PASS campaign inventory\n";
  return 0;
}
