// Synthetic vectors for mission2_objects. Rows, variables, seeds and names are
// authored here; the expected values are the executed results cited in
// native/include/champions/mission2_objects.hpp (research d17b46f).
#include "champions/mission2_objects.hpp"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {
namespace m2 = champions::simulation::rta::mission2_objects;
int checks = 0;

void require(bool ok, const char *what) {
  ++checks;
  if (!ok) {
    std::cerr << "fail: " << what << "\n";
    std::exit(1);
  }
}

template <class F> void refuses(F &&f, const char *what) {
  try {
    f();
  } catch (const m2::Refused &) {
    require(true, what);
    return;
  }
  require(false, what);
}

m2::MissionRow row_of(std::initializer_list<int> head) {
  m2::MissionRow row{};
  std::size_t i = 0;
  for (int v : head)
    row[i++] = static_cast<std::uint8_t>(v);
  return row;
}

std::uint32_t bits_of(float value) {
  std::uint32_t bits;
  std::memcpy(&bits, &value, sizeof bits);
  return bits;
}

void test_hub2_cold_open() {
  // compose:134-139, compose.py:699-704.
  for (int first : {0, 3}) {
    for (int second = 0; second <= 3; ++second) {
      auto open = m2::hub2_open(row_of({3, first, second}));
      const int node = (second == 0 || second == 2) ? 1 : second == 1 ? 4 : 5;
      const int visible = node == 1 ? 1 : 2;
      require(open.node == node, "cold node follows mission 2's byte");
      require(open.visible == visible, "cold visible choices");
    }
  }
  refuses([] { m2::hub2_open(row_of({0, 0, 0})); }, "cold open needs mission 0 at 3");
  refuses([] { m2::hub2_open(row_of({3, 1, 0})); }, "mission 1 byte 1 not executed");
  refuses([] { m2::hub2_open(row_of({3, 2, 0})); }, "mission 1 byte 2 not executed");
  refuses([] { m2::hub2_open(row_of({3, 0, 4})); }, "mission 2 byte above 3");
  refuses([] { m2::hub2_open(row_of({3, 0, 0, 1})); }, "mission 3 nonzero");
  refuses([] { m2::hub2_open(row_of({3, 0, 0}), m2::Node1Variable::Unknown); },
          "reopen after node 2 is not read back");
}

void test_hub2_choices() {
  // compose.py:680-691: (first, second, choice) -> entry, landing, availability.
  struct Case {
    int first, second, choice;
    std::optional<std::uint32_t> script;
    int node;
    int available; // -1 none
  };
  const Case cases[] = {
      {0, 0, 0, 0x29bcu, 2, -1}, {0, 2, 0, 0x29bcu, 2, -1}, {3, 0, 0, 0x29bcu, 3, -1},
      {3, 2, 0, 0x29bcu, 3, -1}, {0, 1, 0, 0x41b4u, 4, -1}, {3, 1, 0, 0x41b4u, 4, -1},
      {0, 1, 1, std::nullopt, 4, -1}, {3, 1, 1, std::nullopt, 4, -1},
      {0, 3, 0, 0x2dd8u, 5, 3}, {3, 3, 0, 0x2dd8u, 5, 3},
      {0, 3, 1, 0x42c8u, 5, 3}, {3, 3, 1, 0x42c8u, 5, 3},
  };
  for (const auto &c : cases) {
    const auto row = row_of({3, c.first, c.second});
    auto got = m2::hub2_choose(m2::hub2_open(row), c.choice);
    require(got.script == c.script, "choice script entry");
    require(got.node && *got.node == c.node, "choice landing node");
    if (c.available < 0) {
      require(got.available.empty(), "no availability write");
      require(got.after.row == row, "mission bytes unchanged");
    } else {
      require(got.available == std::vector<int>{c.available}, "availability write");
      require(got.after.row[3] == 1, "mission 3 becomes available");
      require(got.after.row[2] == 3, "mission 2 byte stays 3");
    }
    if (c.script && *c.script == 0x41b4u)
      require(got.natives == std::vector<std::string>{"changeSwoop", "activateStore"},
              "node 4 first choice natives");
    require(got.after.visible == 0, "landing choices need a reopen");
    refuses([&] { m2::hub2_choose(got.after, 0); }, "no choice on the landed node");
  }
  refuses([] { m2::hub2_choose(m2::hub2_open(row_of({3, 0, 0})), 1); }, "node 1 has one choice");
}

void test_hub2_continuation() {
  // compose:151-155, compose.py:775-800.
  for (int choice : {0, 1}) {
    auto first = m2::hub2_choose(m2::hub2_open(row_of({3, 3, 0})), 0);
    require(first.after.variable == m2::Node1Variable::Stored, "node 3 landing stores the variable");
    auto reopen = m2::hub2_open(first.after.row, first.after.variable);
    require(reopen.node == 3 && reopen.visible == 2, "reopen selects node 3 with two choices");
    auto got = m2::hub2_choose(reopen, choice);
    require(got.script == (choice == 0 ? 0x2b6cu : 0x4224u), "node 3 script entry");
    require(got.available == std::vector<int>{2}, "node 3 grants mission 2");
    require(got.after.row == row_of({3, 3, 1}), "mission 2 byte becomes 1");
    require(!got.node, "node 3 landing not recorded");
  }
  auto node2 = m2::hub2_choose(m2::hub2_open(row_of({3, 0, 0})), 0);
  require(node2.after.variable == m2::Node1Variable::Unknown, "node 2 landing variable unknown");
  refuses([&] { m2::hub2_open(node2.after.row, node2.after.variable); }, "reopen after node 2");
  refuses([] { m2::hub2_open(row_of({3, 0, 0}), m2::Node1Variable::Stored); },
          "stored reopen only on (3, 3, 0)");
}

void test_trigger_key() {
  // compose:159-165. Sample set of compose.py:713-726.
  auto base = row_of({3});
  require(m2::hub2_trigger_key(base) == 0, "base row closed");
  struct Sample {
    int index, value, expect;
  };
  const Sample samples[] = {{1, 1, 1},  {1, 3, 0},  {2, 1, 1},  {2, 2, 0},
                            {2, 3, 0},  {11, 1, 1}, {11, 3, 0}, {12, 1, 1},
                            {12, 2, 0}, {12, 3, 1}, {2, 0, 0}};
  for (const auto &s : samples) {
    auto row = base;
    row[static_cast<std::size_t>(s.index)] = static_cast<std::uint8_t>(s.value);
    require(m2::hub2_trigger_key(row) == s.expect, "trigger key sample");
  }
  m2::MissionRow all{};
  all.fill(3);
  require(m2::hub2_trigger_key(all) == 1, "mission 12 available opens complete rows");
}

void test_gate_and_action() {
  // compose:167-190, tools/test_mission2_compose.py:39-53.
  const auto planted = row_of({3, 0, 1});
  m2::GateVariables clear{};
  auto second = clear;
  second[1] = 1;
  require(m2::hub2_action_gate(planted, clear) == 0, "planted row gate false");
  require(m2::hub2_action_gate(planted, second) == 1, "mission 2 variable opens the gate");
  auto done = planted;
  done[2] = 3;
  require(m2::hub2_action_gate(done, second) == 0, "complete mission keeps the gate closed");
  auto last = row_of({3});
  last[12] = 1;
  auto twelfth = clear;
  twelfth[11] = 1;
  require(m2::hub2_action_gate(last, twelfth) == 1, "mission 12 uses its own variable");
  require(m2::hub2_action_gate(planted, twelfth) == 0, "another mission's variable does not open");

  auto off = m2::hub2_action(planted, clear, "ord-a");
  require(off.gate == 0 && off.returned == 0, "gate-false action returns 0");
  require(off.scripts == std::vector<std::uint32_t>{0x2210u}, "gate-false scripts");
  require(off.natives == std::vector<std::string>{"setEntrance", "activateMissionMenu"},
          "setEntrance then activateMissionMenu");
  require(off.entrance == m2::EVIL_ENTRANCE, "entrance evilstart");
  require(off.mission_menu_activated && !off.menu_open_refusal.empty(),
          "menu open stays undecided under the seeded stub");

  auto on = m2::hub2_action(planted, second, "ord-a");
  require(on.gate == 1 && on.returned == 0, "gate-true action returns 0");
  require(on.scripts == std::vector<std::uint32_t>{0x2210u, 0x22d0u}, "gate-true enters 0x22d0");
  require(on.natives == std::vector<std::string>{"addHelpMessage", "addDeferedCall"},
          "help message then deferred call");
  require(on.entrance == "ord-a" && !on.mission_menu_activated, "no entrance or menu calls");

  auto closed = m2::hub2_action(done, second, "ord-a");
  require(closed.gate == 0 && closed.entrance == m2::EVIL_ENTRANCE, "complete row takes entrance");
}

void test_particles() {
  // compose:194-210, compose.py:497-535.
  m2::ParticlePool cold;
  refuses([&] { m2::particle_reset_all(cold); }, "menu-image reset faults");
  for (std::uint32_t frozen : {0u, 1u}) {
    auto pool = m2::particle_init();
    pool.trails_frozen = frozen;
    pool.group_handle = 9;
    pool.trail_handle = 6;
    pool.group_generation = 4;
    pool.trail_generation = 4;
    auto got = m2::particle_reset_all(pool);
    require(got.returned == 1, "reset returns 1");
    require(pool.slots[0] == 1, "slot 0 stays occupied");
    bool free = true;
    for (int i = 1; i < m2::PARTICLE_SLOTS; ++i)
      free = free && pool.slots[static_cast<std::size_t>(i)] == -1;
    require(free, "slots 1..31 free");
    require(pool.group_handle == 0 && pool.trail_handle == 0, "handles cleared");
    require(pool.group_generation == 1 && pool.trail_generation == 1, "generations 1");
    std::vector<std::uint32_t> calls;
    if (frozen)
      calls.push_back(m2::PARTICLE_EMPTY_FN);
    calls.push_back(m2::TRAIL_RESET_ALL);
    calls.push_back(m2::GROUP_RESET_ALL);
    require(got.calls == calls, "reset call order");
  }
  auto pool = m2::particle_init();
  m2::ParticleDefinition bare;
  refuses([&] { m2::particle_claim(pool, bare); }, "claim needs setDefaults");
  m2::ParticleDefinition held;
  m2::particle_set_defaults(held);
  require(held.defaults_set, "setDefaults");
  require(m2::particle_claim(pool, held) == 1, "claim returns 1");
  require(held.slot == 1 && pool.type == &held, "first claim takes slot 1");
  m2::ParticleDefinition next;
  m2::particle_set_defaults(next);
  refuses([&] { m2::particle_claim(pool, next); }, "second claim not executed");
  auto got = m2::particle_reset_all(pool);
  require(held.slot == -1 && held.word8 == 0, "reset releases the definition");
  require(pool.slots[1] == -1, "slot word 1 free");
  require(got.calls == std::vector<std::uint32_t>{m2::TRAIL_RESET_ALL, m2::GROUP_RESET_ALL},
          "claimed reset calls");
  m2::ParticlePool uninit;
  refuses([&] { m2::particle_claim(uninit, next); }, "claim before init");
}

void test_archive() {
  // round4:14-45.
  const std::string gob = "ord-a.gob";
  const std::string base = "ord-a";
  require(m2::gob_header_request(gob).name == gob, "header name");
  require(m2::gob_header_request(gob).size == 0x800 && m2::gob_header_request(gob).offset == 0,
          "header 0x800 at 0");
  require(m2::tex_request(base).name == "ord-a.tex" && m2::tex_request(base).size == 0x5000 &&
              m2::tex_request(base).offset == 0,
          "tex 0x5000 at 0");
  require(m2::tex_request("").name == ".tex", "unset tex name requests .tex");
  // Authored middle reads; the contract does not generate these.
  std::vector<m2::ReadRecord> reads = {
      {m2::gob_header_request(gob), 1},
      {{gob, 0x1000u, 0x800u}, 1},
      {m2::tex_request(base), 1},
      {{gob, 0x2000u, 0x1800u}, 1},
  };
  auto ok = m2::accept_archive_open(gob, base, reads, 0, 3);
  require(ok.accepted && ok.lump_count == 3 && ok.refusal.empty(), "archive open accepted");
  require(!m2::accept_archive_open(gob, "", reads, 0, 3).accepted, "unset tex name refused");
  auto dot = reads;
  dot[2] = {m2::tex_request(""), 0};
  require(!m2::accept_archive_open(gob, base, dot, 0, 3).accepted, ".tex miss refused");
  auto late = reads;
  std::swap(late[0], late[1]);
  require(!m2::accept_archive_open(gob, base, late, 0, 3).accepted, "header must be first");
  auto notex = reads;
  notex.erase(notex.begin() + 2);
  require(!m2::accept_archive_open(gob, base, notex, 0, 3).accepted, "tex read required");
  for (int hits : {0, 2}) {
    auto miss = reads;
    miss[3].hits = hits;
    require(!m2::accept_archive_open(gob, base, miss, 0, 3).accepted, "every read hits one file");
  }
  require(!m2::accept_archive_open(gob, base, reads, 1, 3).accepted, "count from explicit 0");
  require(!m2::accept_archive_open(gob, base, reads, 0, 2).accepted, "three lumps registered");
  require(!m2::accept_archive_open(gob, base, {}, 0, 3).accepted, "no reads refused");
  refuses([] { m2::refuse_load_level(true); }, "loader texcache_reset stop");
  refuses([] { m2::refuse_load_level(false); }, "loader DMA stop");
}

void test_pool_and_constructors() {
  // round4:71-92, compose:237-243.
  m2::ObjectPool null_head;
  null_head.seed = 7;
  m2::pool_init(null_head);
  require(null_head.free_head == 0, "pool_init leaves the head");
  refuses([&] { m2::game_object_ctor(null_head); }, "null link-head store");
  m2::ObjectPool early;
  refuses([&] { m2::handle_init_pool(early); }, "handle_initPool after pool_init");
  for (std::uint32_t seed : {20973u, 7u}) {
    m2::ObjectPool pool;
    pool.seed = seed;
    m2::pool_init(pool);
    m2::handle_init_pool(pool);
    require(pool.free_head == 0x847268u, "free-list head");
    auto built = m2::game_object_ctor(pool);
    const std::uint32_t stepped = seed * 0x19660du + 0x3c6ef35fu;
    require(built.returned == 0, "GameObject_ctor returns 0");
    require(pool.seed && *pool.seed == stepped, "one shared-seed step");
    require(built.word44 == ((stepped >> 16) & 0xffffu), "+0x44 is the step >> 16");
    refuses([&] { m2::game_object_ctor(pool); }, "second object not executed");
    struct Kind {
      m2::ObjectKind kind;
      std::uint32_t size, flags;
    };
    for (const Kind &k : {Kind{m2::ObjectKind::PushTrigger, 0x4c, 0x0010000b},
                          Kind{m2::ObjectKind::Trigger, 0x70, 0x3},
                          Kind{m2::ObjectKind::Npc, 0x480, 0x01100028}}) {
      m2::ObjectPool fresh;
      fresh.seed = seed;
      m2::pool_init(fresh);
      m2::handle_init_pool(fresh);
      auto got = m2::construct(fresh, k.kind);
      require(got.size == k.size && got.flags == k.flags, "constructor flags");
      require(!fresh.seed, "typed constructor seed use unmeasured");
      refuses([&] { m2::construct(fresh, k.kind); }, "fresh pool only");
    }
    m2::ObjectPool portal;
    m2::pool_init(portal);
    m2::handle_init_pool(portal);
    refuses([&] { m2::construct(portal, m2::ObjectKind::JumpGate); }, "portal lump refused");
    m2::ObjectPool unlinked;
    m2::pool_init(unlinked);
    refuses([&] { m2::construct(unlinked, m2::ObjectKind::Npc); }, "constructors need handles");
  }
}

void test_interact() {
  // compose:228-235, compose.py:578-598.
  for (int slot : {0, 1}) {
    m2::InteractPlayer player;
    player.slot = slot;
    player.hit_point_bits = bits_of(0.0f);
    auto got = m2::player_interact(player, {});
    require(got.returned == 0x4d8af0u + static_cast<std::uint32_t>(slot) * 0x4c30u,
            "zero hit points return the record");
    require(!got.box_query && got.prompt_calls == 0 && got.get_prop_calls == 0 &&
                got.strlen_calls == 0 && !got.use_entered,
            "zero hit points make no calls");
  }
  m2::InteractPlayer marked;
  marked.word540 = 1;
  refuses([&] { m2::player_interact(marked, {}); }, "nonzero +0x540 not executed");

  // round4:102-125, r4.py:177-229.
  auto player = m2::init_new_character_null_r11();
  require(player.hit_point_bits == 0x4e6e6b27u && player.maximum == 0 && !player.linked_object,
          "init with null r11");
  const std::vector<m2::InRange> push = {{m2::ObjectKind::PushTrigger, true, true}};
  refuses([&] { m2::player_interact(player, push); }, "in-range needs the linked object");
  player.linked_object = true;
  auto got = m2::player_interact(player, push);
  require(got.box_query && got.prompt_calls == 1, "box query and one prompt");
  require(got.prompt_returned && *got.prompt_returned == 1, "prompt returns 1");
  require(got.get_prop_calls == 2 && got.strlen_calls == 2, "getPropStr and strlen twice");
  require(!got.use_entered && !got.use_refusal.empty(), "use not entered");
  require(got.returned == 0x4d8ac0u, "interact return");
  refuses([&] { m2::player_interact(player, {{m2::ObjectKind::Trigger, true, true}}); },
          "trigger prompt unknown");
  refuses([&] { m2::player_interact(player, {{m2::ObjectKind::Npc, true, true}}); },
          "npc prompt unknown");
  refuses([&] { m2::player_interact(player, {}); }, "nothing in range unknown");
  refuses([&] { m2::player_interact(player, {{m2::ObjectKind::PushTrigger, true, false}}); },
          "use bits not planted unknown");
  m2::InteractPlayer alive;
  alive.hit_point_bits = bits_of(1.0f);
  alive.linked_object = true;
  refuses([&] { m2::player_interact(alive, push); }, "positive hit points outside the init state");
  refuses([] { m2::refuse_use(); }, "use refused");
  refuses([] { m2::refuse_arrival(); }, "arrival refused");
}
} // namespace

int main() {
  test_hub2_cold_open();
  test_hub2_choices();
  test_hub2_continuation();
  test_trigger_key();
  test_gate_and_action();
  test_particles();
  test_archive();
  test_pool_and_constructors();
  test_interact();
  std::cout << "mission2-objects: " << checks << " checks passed\n";
  return 0;
}
