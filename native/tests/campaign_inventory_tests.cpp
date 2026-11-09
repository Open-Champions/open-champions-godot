#include "champions/campaign_entry.hpp"
#include "champions/campaign_inventory.hpp"
#include "champions/frontend.hpp"
#include "champions/script_commands.hpp"
#include "champions/script_items.hpp"
#include <cstdint>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace ci = champions::simulation::rta::campaign_inventory;
namespace fe = champions::frontend;
namespace lf = champions::simulation::rta::level_flow;
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

void arm_progress(sc::Host &host, unsigned players = 2) {
  auto &memory = host.variables().memory;
  memory.word(si::G_NUM_PLAYERS, players);
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
            name_at(host, 0, 0, 1) == "sword" && memory.s32(si::bag_count_at(0, 1)) == 2 &&
            memory.s32(si::bag_count_at(0, 2)) == 2,
        label + " bags stay");
  check(memory.u32(si::rec(0) + 0x43f0) == si::bag_entry(0, 0, 0), label + " equipment stays");
  check(memory.s32(si::rec(0) + 0x43d0) == 1 && memory.cstr(si::rec(0) + 0x41d0) == "Aquest",
        label + " quest stays");
  check(memory.u8(0x86da80 + 15) == 5 && memory.u8(0x86da80 + 21) == 9, label + " variable values stay");
  check(memory.u32(ci::ACT_MARK) == 0, label + " mark stays 0");
  // +0x4bbf is row 0, not a mission row, and does not travel in the campaign
  // image (quests_bridge.cpp MISSION_AT); +0x4bcc is mission row 1.
  check(memory.s32(si::rec(0) + 0xe4) == 1234 && memory.s32(si::rec(0) + 0x5c) == 50 &&
            memory.u8(si::rec(0) + 0x59) == 3 && memory.u8(si::rec(0) + 0x4bcc) == 3 &&
            memory.u8(si::rec(0) + 0x4c0d) == 1,
        label + " gold experience mission medal and level stay");
}


// The act table is the executable's in the client (quests::read_act_tables).
// This fixture is the documented one: research-story-route-r3 019a155
// docs/story-route.md:242-255 (the table) and :271-279 (between two story
// levels). Mission-1 names and both hubs are act 0 (hub is a prefix of hub2),
// the innovat1/inmine1/inmine2/innovat2 chain is act 1, each later mission row
// is one act, and an evil name shares its row's act.
q::ActTables story_tables() {
  q::ActTables tables;
  tables.names = {
      {"hub", "hub2", "blood1", "strife", "drundar", "drunarm", "drunare"},
      {"innovat1", "inmine1", "inmine2", "innovat2"},
      {"islc1"}, {"wat1"}, {"tormenta"}, {"swamp1", "swamp2"}, {"valor"},
      {"fire1"}, {"storm1"}, {"fay2", "fay1"}, {"nit1"}, {"bossgood", "bossevil"},
  };
  for (const auto &act : tables.names) {
    lf::Act flow;
    for (const auto &name : act)
      flow.push_back({name, 0});
    tables.flow.push_back(std::move(flow));
  }
  return tables;
}

// session_controller.cpp mission2_story: the names the session marks mission 2.
bool mission2_story(const std::string &level) {
  return level == "innovat1" || level == "inmine1" || level == "inmine2" || level == "innovat2";
}

// The front end's real latch after a card error: a slot is chosen (latch 1),
// the save fails and keeps it, and circle returns to play with it still 1
// (frontend.cpp SaveGame, save_result; inventory-reset.md:87-90).
int latch_after_card_error() {
  auto tap = [](fe::Flow &flow, std::uint32_t bits) {
    flow.frame(static_cast<std::uint16_t>(bits));
    flow.frame(0);
  };
  fe::Flow flow;
  tap(flow, fe::CROSS);  // boot card
  tap(flow, fe::CROSS);  // logo
  for (int i = 0; i < 5; ++i)
    tap(flow, fe::CROSS);
  tap(flow, fe::CROSS);
  tap(flow, fe::CROSS);
  for (int i = 0; i < 20; ++i)
    tap(flow, fe::RIGHT);
  tap(flow, fe::CROSS);
  flow.set_name("S");
  tap(flow, fe::DOWN);
  tap(flow, fe::CROSS);
  flow.request_save();
  tap(flow, fe::CROSS);
  check(flow.save_latch() == 1, "choosing a slot sets the front end's latch");
  flow.save_result(false, "disk full");
  tap(flow, fe::CIRCLE);
  check(flow.screen() == fe::Screen::Playing && flow.save_latch() == 1,
        "a card error leaves the latch at 1 after returning to play");
  return flow.save_latch();
}

// The front end in play, as latch_after_card_error reaches it.
void flow_tap(fe::Flow &flow, std::uint32_t bits) {
  flow.frame(static_cast<std::uint16_t>(bits));
  flow.frame(0);
}
fe::Flow flow_in_play() {
  fe::Flow flow;
  flow_tap(flow, fe::CROSS);
  flow_tap(flow, fe::CROSS);
  for (int i = 0; i < 5; ++i)
    flow_tap(flow, fe::CROSS);
  flow_tap(flow, fe::CROSS);
  flow_tap(flow, fe::CROSS);
  for (int i = 0; i < 20; ++i)
    flow_tap(flow, fe::RIGHT);
  flow_tap(flow, fe::CROSS);
  flow.set_name("S");
  flow_tap(flow, fe::DOWN);
  flow_tap(flow, fe::CROSS);
  return flow;
}

// The client refuses the save for its own reasons (pickups.cpp: inventory
// reload is not implemented). That is not a card error, so the front end has
// no reading of the original's latch, and circle returns to play with none.
int latch_after_client_refusal(fe::Flow &flow) {
  flow.request_save();
  flow_tap(flow, fe::CROSS);
  flow.save_refused("Saving refused: inventory reload is not implemented.");
  flow_tap(flow, fe::CIRCLE);
  check(flow.screen() == fe::Screen::Playing && flow.save_latch() == fe::LATCH_UNKNOWN,
        "a client save refusal leaves no latch reading after returning to play");
  return flow.save_latch();
}

// Opening the load screen from the pause menu stores 0 (memcard_requestLoad
// 0x217760); backing out stores nothing more.
int latch_after_opening_load(fe::Flow &flow) {
  flow_tap(flow, fe::START);
  flow_tap(flow, fe::DOWN);
  flow_tap(flow, fe::DOWN);
  flow_tap(flow, fe::CROSS);
  check(flow.screen() == fe::Screen::LoadGame && flow.save_latch() == 0, "the load screen opens with latch 0");
  flow_tap(flow, fe::CIRCLE);
  flow_tap(flow, fe::CIRCLE);
  check(flow.screen() == fe::Screen::Playing && flow.save_latch() == 0, "back in play with latch 0");
  return flow.save_latch();
}

// One local player at a story act with the sample cells, journal and progress.
struct Source {
  World world;
  explicit Source(int act) {
    auto &host = world.host;
    host.start_single_player();
    world.plant(0, sample_cells());
    arm_progress(host, 1);
    host.campaign().set_act(act);
    host.campaign().set_acts(story_tables().names);
  }
  sc::Host &host() { return world.host; }
};

// The session's entry for one level host: a destination built from the
// carried campaign image, the bag copy under the guard, then the client's
// prepare_campaign_entry (session_controller.cpp enable_level_script).
struct Entry {
  sc::Host next;
  sc::CampaignEntry result;
  std::string refusal;
};
void enter(Entry &entry, Source &source, const std::string &level, const sc::BagCarryGuard &guard, int latch,
           bool death_task = false, bool definitions = true,
           const std::function<void(sc::Host &)> &before = {}) {
  entry.next.load_campaign(source.host().save_campaign());
  if (definitions)
    entry.next.variables().memory.write(BASE, source.host().variables().memory.read(BASE, 0x4000));
  const sc::ActCarry carry{guard.holds(), latch};
  if (carry.bags_carried)
    sc::carry_live_bags(entry.next, source.host());
  if (before)
    before(entry.next);
  try {
    entry.result = sc::prepare_campaign_entry(entry.next, &source.host(), story_tables(), level,
                                              mission2_story(level), false, carry, death_task);
  } catch (const std::runtime_error &error) {
    entry.refusal = error.what();
  }
}

const sc::BagCarryGuard HELD{true, false, 3, 3};

void test_live_travel() {
  check(HELD.holds(), "the guard holds for a held record whose inventory was written for it");
  {
    Source source(0);
    Entry entry;
    enter(entry, source, "strife", HELD, 0);
    check(entry.refusal.empty() && !entry.result.changed && !entry.result.reset && entry.result.act == 0,
          "same act: no act change");
    expect_untouched(entry.next, "same act");
    check(entry.next.campaign().record(0).quest_count == 1 && !entry.result.items_crossed_act,
          "same act keeps the journal and starts no act-transfer chain");
  }
  {
    Source source(0);
    Entry entry;
    enter(entry, source, "hub2", HELD, 0);
    check(entry.refusal.empty() && !entry.result.changed && entry.result.act == 0 &&
              entry.next.variables().memory.s32(ci::ACT_WORD) == 0,
          "hub2 from act 0 stays act 0 (hub is a prefix of hub2)");
    expect_untouched(entry.next, "hub2");
  }
  {
    Source source(1);
    Entry entry;
    enter(entry, source, "inmine2", HELD, 0);
    check(entry.refusal.empty() && !entry.result.changed && !entry.result.reset && entry.result.act == 1 &&
              entry.result.removed == 0 && entry.result.mark == 0,
          "the mission-2 chain stays in act 1 and does not clear");
    expect_untouched(entry.next, "mission 2 chain");
  }
  {
    Source source(0);
    Entry entry;
    enter(entry, source, "innovat1", HELD, 0);
    const auto &memory = entry.next.variables().memory;
    check(entry.refusal.empty(), "act change with classified cells is accepted: " + entry.refusal);
    check(entry.result.changed && entry.result.reset && entry.result.act == 1 && memory.s32(ci::ACT_WORD) == 1,
          "act change stores act 1");
    check(entry.result.carried_cells == 6 && entry.result.removed == 1 && entry.result.mark == 1 &&
              memory.u32(ci::ACT_MARK) == 1,
          "the entry reports six carried cells, one removed, and 1 at 0x4f7118");
    check(memory.s32(si::bag_count_at(0, 0)) == 1 && name_at(entry.next, 0, 0, 0) == "sword",
          "unkept flag-39 cell is removed and the ordinary item slides");
    check(memory.u32(si::rec(0) + 0x43f0) == 0, "removed cell's equipment slot is cleared");
    check(memory.u32(si::rec(0) + 0x43f0 + 8) == si::bag_entry(0, 0, 0),
          "later equipment pointer follows the slide");
    check(memory.u32(si::rec(0) + 0x4434) == 0 && memory.u8(si::rec(0) + 0x4a86) == 5,
          "active weapon word clears and the weapon class becomes 5");
    check(memory.u32(si::rec(0) + 0x443c) == si::bag_entry(0, 0, 0), "second weapon pointer follows the slide");
    check(memory.u16(si::bag_entry(0, 0, 0) + 0x22) == 8, "slid stamp drops by one");
    check(name_at(entry.next, 0, 2, 0) == "pickaxe" && name_at(entry.next, 0, 2, 1) == "bloodstonefay",
          "kept names stay");
    check(memory.s32(si::bag_count_at(0, 1)) == 2, "ordinary and bit-38 cells stay");
    check(memory.s32(si::rec(0) + 0x43d0) == 0 && memory.cstr(si::rec(0) + 0x41d0) == "NoQuests",
          "the quest count becomes 0 and the used title is the placeholder");
    check(memory.s32(si::rec(0) + 0xe4) == 1234 && memory.s32(si::rec(0) + 0x5c) == 50 &&
              memory.u8(si::rec(0) + 0x59) == 3 && memory.u8(si::rec(0) + 0x4bcc) == 3 &&
              memory.u8(si::rec(0) + 0x4c0d) == 1,
          "gold experience level mission and medal stay");
    check(entry.result.items_crossed_act && entry.next.items_crossed_act(),
          "an act transfer that carried cells starts the act-transfer chain");
    sc::Host later;
    later.load_campaign(entry.next.save_campaign());
    const auto chained = sc::prepare_campaign_entry(later, &entry.next, story_tables(), "inmine1", true, false,
                                                    {true, 0});
    check(!chained.changed && chained.items_crossed_act, "a later carried same-act entry stays on the chain");
    sc::Host fresh;
    const auto unchained = sc::prepare_campaign_entry(fresh, nullptr, story_tables(), "inmine1", true, false,
                                                      {true, 0});
    check(!unchained.items_crossed_act, "an entry without a previous host leaves the chain");
  }
  {
    Source source(0);
    const auto latch = latch_after_card_error();
    Entry entry;
    enter(entry, source, "innovat1", HELD, latch);
    check(entry.refusal.empty() && entry.result.changed && !entry.result.reset && entry.result.act == 1 &&
              entry.next.variables().memory.s32(ci::ACT_WORD) == 1 && entry.result.removed == 0 &&
              entry.result.mark == 0,
          "the front end's latch left at 1 stores the act and skips the clear");
    check(entry.next.variables().memory.u8(ci::CARD_LATCH) == 1, "the latch is mirrored at 0x4f8f28");
    expect_untouched(entry.next, "latch");
    check(entry.next.campaign().record(0).quest_count == 1, "the latch keeps the journal");
  }
  {
    Source source(0);
    Entry entry;
    enter(entry, source, "innovat1", HELD, 0, true);
    check(entry.refusal.empty() && entry.result.changed && !entry.result.reset && entry.result.act == 1 &&
              entry.result.removed == 0,
          "a running death task stores the act and skips the clear");
    expect_untouched(entry.next, "death task");
  }
  {
    Source source(0);
    source.host().variables().memory.w32(ci::FRONTEND_WORD, 1u);
    Entry entry;
    enter(entry, source, "innovat1", HELD, 0);
    const auto &memory = entry.next.variables().memory;
    check(entry.refusal.empty() && !entry.result.changed && !entry.result.reset && entry.result.act == 0,
          "the frontend word skips the act call");
    check(memory.u32(ci::FRONTEND_WORD) == 1 && memory.s32(ci::ACT_WORD) == 0,
          "the frontend word is not stored and the act stays");
    expect_untouched(entry.next, "frontend");
  }
  {
    Source source(0);
    Entry entry;
    enter(entry, source, "innovat1", HELD, -1);
    check(entry.refusal.find("card latch 0x4f8f28 has no reading") != std::string::npos,
          "an act transfer without a latch reading refuses");
    expect_untouched(entry.next, "no latch reading");
    check(entry.next.campaign().act() == 0, "no latch reading leaves the destination act");
  }
}

// PCR2-1: the session's latch comes from the real front end. After a client
// save refusal the next act change refuses before any edit; once the load
// screen has stored 0, the act change clears as with no save at all.
void test_latch_from_front_end() {
  auto flow = flow_in_play();
  {
    Source source(0);
    Entry entry;
    enter(entry, source, "innovat1", HELD, latch_after_client_refusal(flow));
    check(entry.refusal.find("card latch 0x4f8f28 has no reading") != std::string::npos,
          "after a client save refusal the act transfer refuses: " + entry.refusal);
    expect_untouched(entry.next, "client save refusal");
    check(entry.next.campaign().act() == 0 && entry.next.variables().memory.u32(ci::ACT_MARK) == 0 &&
              entry.next.campaign().record(0).quest_count == 1,
          "the refused transfer leaves the act, 0x4f7118 and the journal");
  }
  {
    Source source(0);
    Entry entry;
    enter(entry, source, "strife", HELD, flow.save_latch());
    check(entry.refusal.empty() && !entry.result.changed, "a same-act entry does not read the latch");
  }
  {
    Source source(0);
    Entry entry;
    enter(entry, source, "innovat1", HELD, latch_after_opening_load(flow));
    check(entry.refusal.empty() && entry.result.changed && entry.result.reset && entry.result.removed == 1 &&
              entry.result.mark == 1 && entry.next.variables().memory.u8(ci::CARD_LATCH) == 0,
          "after the load screen stores 0 the act change clears: " + entry.refusal);
  }
}

// Host::change_level is the client's act change. From act -1 the first level
// clears (inventory-reset.md:83-85); slots below g_numPlayers are walked.
void test_change_level_from_unbound() {
  World world;
  world.plant(0, sample_cells());
  world.plant(1, {{"othergem", BIT39, 0, -1, -1, false, 0, 1, 0}});
  world.plant(2, {{"canary", BIT39, 0, -1, -1, false, 0, 1, 0}});
  arm_progress(world.host);
  auto &host = world.host;
  const auto tables = story_tables();
  host.campaign().set_acts(tables.names);
  host.campaign().set_act(-1);
  host.level().acts = tables.flow;
  host.level().act = -1;
  host.variables().memory.w32(ci::ACT_WORD, 0xffffffffu);
  host.level().pending.request("blood1");
  const auto change = host.change_level({});
  const auto &memory = host.variables().memory;
  check(change.act_changed && change.reset_act && host.campaign().act() == 0 && memory.s32(ci::ACT_WORD) == 0 &&
            memory.u32(ci::ACT_MARK) == 1 && host.act_change_removed() == 2,
        "from act -1 the first level stores act 0, clears, and removes two cells");
  check(name_at(host, 0, 0, 0) == "sword", "from act -1 removes slot 0's unkept cell");
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

  // Without readable names the clear refuses before its first edit.
  World bare;
  bare.plant(0, sample_cells());
  arm_progress(bare.host, 1);
  bare.host.variables().memory.write(BASE, std::vector<std::uint8_t>(0x4000, 0));
  bare.host.campaign().set_acts(tables.names);
  bare.host.campaign().set_act(-1);
  bare.host.level().acts = tables.flow;
  bare.host.level().act = -1;
  bare.host.level().pending.request("blood1");
  bool refused = false;
  try {
    bare.host.change_level({});
  } catch (const std::runtime_error &error) {
    refused = std::string(error.what()).find("refused before any edit") != std::string::npos;
  }
  check(refused && bare.host.variables().memory.s32(si::bag_count_at(0, 0)) == 2 &&
            bare.host.variables().memory.u32(ci::ACT_MARK) == 0,
        "Host::change_level refuses an unreadable flag-39 name and removes nothing");
}

// session_controller.cpp: creating a character (starting items pending) and
// loading or clearing a companion (revision bump) do not write the bags, so
// the previous host's cells are not the new character's.
void test_bag_carry_guard() {
  const sc::BagCarryGuard created{true, true, 3, 4};
  const sc::BagCarryGuard companion{true, false, 3, 4};
  const sc::BagCarryGuard cleared{false, false, 4, 4};
  check(!created.holds() && !companion.holds() && !cleared.holds(),
        "creation, companion load and a cleared record each fail the guard");
  for (const auto &[guard, label] : {std::pair{created, "character creation"},
                                     std::pair{companion, "companion load"},
                                     std::pair{cleared, "no player record"}}) {
    Source source(0);
    Entry entry;
    enter(entry, source, "innovat1", guard, 0);
    check(entry.refusal == "Act transfer with inventory is refused: live bag carry and flag-39 migration are unported.",
          std::string(label) + ": the act transfer refuses as wave 37 did");
    check(entry.next.variables().memory.s32(si::bag_count_at(0, 0)) == 0 &&
              entry.next.variables().memory.s32(si::bag_count_at(0, 2)) == 0 &&
              entry.next.variables().memory.u32(si::rec(0) + 0x43f0 + 8) == 0,
          std::string(label) + ": the destination is not given the old cells");
    check(entry.next.campaign().act() == 0 && entry.next.variables().memory.u32(ci::ACT_MARK) == 0,
          std::string(label) + ": the refusal leaves the destination's act and mark");
    bool preflight = false;
    try {
      sc::require_campaign_act_transfer(source.host(), story_tables(), "innovat1", false, {guard.holds(), 0});
    } catch (const std::runtime_error &) {
      preflight = true;
    }
    check(preflight, std::string(label) + ": the session's preflight refuses too");
    Entry same;
    enter(same, source, "strife", guard, 0);
    check(same.refusal.empty() && same.next.variables().memory.s32(si::bag_count_at(0, 0)) == 0,
          std::string(label) + ": a same-act entry carries no old cells either");
  }
  // An empty inventory still crosses without the guard, as at wave 37.
  Source empty(0);
  for (int bag = 0; bag < si::BAGS; ++bag)
    empty.host().variables().memory.w32(si::bag_count_at(0, bag), 0);
  Entry entry;
  enter(entry, empty, "innovat1", created, 0);
  check(entry.refusal.empty() && entry.result.changed && entry.result.reset && !entry.result.items_crossed_act,
        "with no live cells the act transfer runs without the guard and starts no chain");
}

// The keep decision reads the definition's name in the destination memory.
void test_unreadable_names() {
  {
    Source source(0);
    Entry entry;
    enter(entry, source, "innovat1", HELD, 0, false, false);
    check(entry.refusal.find("has no name pointer in this memory") != std::string::npos,
          "without definitions in the destination the act transfer refuses: " + entry.refusal);
    const auto &memory = entry.next.variables().memory;
    check(memory.s32(si::bag_count_at(0, 0)) == 2 && memory.s32(si::bag_count_at(0, 2)) == 2 &&
              memory.u32(ci::ACT_MARK) == 0 && entry.next.campaign().act() == 0 &&
              entry.next.campaign().record(0).quest_count == 1,
          "the refusal removes nothing and leaves the act and journal");
  }
  {
    Source source(0);
    Entry entry;
    enter(entry, source, "innovat1", HELD, 0, false, true, [](sc::Host &next) {
      auto &memory = next.variables().memory;
      const auto definition = memory.u32(si::bag_entry(0, 0, 0));
      memory.w8(memory.u32(definition), 0);  // questgem's name reads as ""
    });
    check(entry.refusal.find("name cannot be read in this memory") != std::string::npos,
          "an empty flag-39 name refuses instead of being removed: " + entry.refusal);
    check(entry.next.variables().memory.s32(si::bag_count_at(0, 0)) == 2, "the unreadable cell stays");
  }
  {
    // A cell without flag 39 is never removed, so its name is not needed.
    Source source(0);
    for (const auto bag : {0, 2})
      for (int index = 0; index < 2; ++index) {
        const auto cell = si::bag_entry(0, bag, index);
        source.host().variables().memory.w64(cell + 8, 0);
      }
    Entry entry;
    enter(entry, source, "innovat1", HELD, 0, false, false);
    check(entry.refusal.empty() && entry.result.reset && entry.result.removed == 0,
          "cells without flag 39 cross without readable names");
  }
}

void test_listed_refusals() {
  {
    Source source(0);
    source.host().variables().memory.word(si::G_NUM_PLAYERS, 2u);
    Entry entry;
    enter(entry, source, "innovat1", HELD, 0);
    check(entry.refusal == "Act transfer supports one local player.", "a second local player refuses");
    expect_untouched(entry.next, "two players");
  }
  {
    Source source(0);
    auto record = source.host().campaign().record(0);
    record.bags[1].push_back({"authored-item", 0});
    source.host().campaign().set_record(0, record);
    Entry entry;
    enter(entry, source, "innovat1", HELD, 0);
    check(entry.refusal == "Act transfer with campaign-model bags is refused: they have no equipment pointers to slide.",
          "a campaign-model bag refuses: " + entry.refusal);
  }
  {
    Source source(0);
    source.host().variables().memory.w32(si::bag_entry(0, 1, 0), 0);
    Entry entry;
    enter(entry, source, "innovat1", HELD, 0);
    check(entry.refusal.find("has no definition, so it is not a classified item") != std::string::npos,
          "a live cell with no definition refuses");
  }
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

} // namespace

int main() {
  test_live_travel();
  test_latch_from_front_end();
  test_change_level_from_unbound();
  test_bag_carry_guard();
  test_unreadable_names();
  test_listed_refusals();
  test_share();
  if (failed) {
    std::cerr << failed << " failed\n";
    return 1;
  }
  std::cout << "PASS campaign inventory\n";
  return 0;
}
