// Player damage slot, player death and knock-back on the live combat World.
//
// Contract: research-spells-r3 1e764d4 (audits research-spells-r5.md and
// research-spells-r5-fix.md, both PASS), docs/spec/player-spells-numeric-r3.md
// lines 121-200, executed by tools/difftest_spell_numeric_r3.py:440-631.
//
// Every case calls the public World entry the campaign fight uses:
// World::take_damage (Fight::diagnostic_lethal_hit, damage_apply's player
// victim), World::process_damage (Player_run's Player_processDamage call-out,
// Creature_processDamage) and World::apply_knockback (Creature_applyKnockback
// 0x115eb8 in the campaign callback table). No second model is built here.
//
// Fixtures are synthetic. Planted words are the probe's planted words
// (difftest_spell_numeric_r3.py:26-60, :449-462): pending +0xc8 and speed
// +0x2e8 at 1.0, flag word +0x2dc 0x11, object and record hit points 100,
// state 2, object flags 0x60 (player) or 0x20 (creature), the two probe seeds.
// The hit direction +0x2e0, the stagger threshold, kill credit, spatial
// queries, the holy-shield split and the real cast body are not asserted:
// the note leaves them Unknown (player-spells-numeric-r3.md:140, :178-179,
// :194-195, :229-231).
#include "champions/combat_runtime.hpp"
#include "champions/combat.hpp"
#include <cstdint>
#include <cstdio>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace R = champions::simulation::rta;
namespace C = R::combat_runtime;
namespace {
unsigned checks = 0;
void check(bool condition, const std::string &message) {
  ++checks;
  if (!condition) throw std::runtime_error(message);
}
// difftest_spell_numeric_r3.py:26, :36-60
constexpr std::uint32_t SEEDS[] = {20973, 7};
constexpr R::Raw SENTINEL = 0x3f800000, PLAIN = 0x42200000, HP_START = 0x42c80000,
                 THIRTY = 0x41f00000, TWENTY_EIGHT = 0x41e00000;
constexpr R::Raw KNOCK_BITS[] = {0x40000, 0x80000, 0x100000, 0x200000};
// player-spells-numeric-r3.md:173-175
constexpr R::Raw PLAYER_SPEED[] = {0x42c80000, 0x43160000, 0x43480000, 0x437a0000};
// player-spells-numeric-r3.md:184-192
constexpr R::Raw CREATURE_HIT_SPEED[] = {0x42c80000, 0x43160000, 0x43610000, 0x43bb8000};
constexpr R::Raw CREATURE_IDLE_SPEED[] = {0x42480000, 0x42960000, 0x42c80000, 0x431c4000};
constexpr R::Raw CREATURE_DEAD_SPEED[] = {0x42c80000, 0x43160000, 0x43480000, 0x439c4000};
constexpr C::Address G_RECALL = 0x41b51d, G_MENU_LOCKOUT = 0x41b4fc,
                     G_EXIT_COUNTDOWN = 0x4f694a, G_NETWORK = 0x402791,
                     G_PVP_B = 0x4f7120, ALIVE_COUNT = 0x4f8420;
const C::Address RECORD = C::PLAYERS; // slot 0 at 0x4d8af0

std::string hex(std::uint32_t v) {
  char out[16];
  std::snprintf(out, sizeof out, "0x%x", v);
  return out;
}
// The engine generator (research tools/ref_combat.py:637-651).
std::uint32_t seed_step(std::uint32_t state) {
  return state * 0x19660dU + 0x3c6ef35fU;
}

struct Rig {
  C::World world;
  C::Address object = 0;
  explicit Rig(C::Services services, bool player = true)
      : world(std::move(services), 0, C::Composition::Full) {
    C::Spawn spawn;
    spawn.hp = HP_START;
    if (player) {
      object = world.spawn_player(0, spawn);
    } else {
      world.spawn_player(0, spawn);
      spawn.position[0] = R::combat::itof(1000);
      object = world.spawn_skelight(spawn);
    }
  }
  std::uint32_t u(C::Address a, unsigned n = 4) const {
    return std::uint32_t(world.read(a, n));
  }
  void plant(std::uint32_t seed, R::Raw flags, R::Raw pending, R::Raw bits,
             R::Raw hp, R::Raw record) {
    world.write(C::SHARED_SEED, 4, seed);
    world.write(object + 0x0c, 4, flags);
    world.write(object + 0x58c, 4, 2);
    world.write(object + 0xc8, 4, pending);
    world.write(object + 0x2dc, 4, bits);
    world.write(object + 0x2e8, 4, SENTINEL);
    world.write(object + 0x228, 4, hp);
    world.write(RECORD + 0xac, 4, record);
    world.write(RECORD + 0xb0, 4, HP_START);
    world.clear_history();
  }
  // Recorded stores since the plant (the probe's emu.writes).
  bool stored(C::Address a, std::uint64_t value) const {
    for (const auto &w : world.memory().log())
      if (w.address == a && w.value == value) return true;
    return false;
  }
  unsigned writes_to(C::Address a) const {
    unsigned n = 0;
    for (const auto &w : world.memory().log())
      if (w.address == a) ++n;
    return n;
  }
  bool wrote(C::Address a) const { return writes_to(a) != 0; }
};

// The probe stubs every presentation callee (player-spells-numeric-r3.md:
// 145-147). Clip starts are counted, not interpreted.
unsigned clip_starts = 0;
C::Services services() {
  auto s = C::flat_fixture_services();
  s.animation_start = [](C::Address, R::Raw, R::Raw, R::Raw, R::Raw) { ++clip_starts; };
  return s;
}

// Player_takeDamage 0x1b5330, player-spells-numeric-r3.md:121-140;
// difftest_spell_numeric_r3.py:467-505.
void player_queue(std::uint32_t seed) {
  Rig rig(services());
  const auto p = rig.object;
  rig.plant(seed, 0x60, SENTINEL, 0x11, HP_START, HP_START);
  rig.world.take_damage(p, 0, 0x80000, PLAIN);
  const auto added = R::add(SENTINEL, PLAIN);
  check(rig.u(p + 0xc8) == added && rig.stored(p + 0xc8, added),
        "take: pending +0xc8 stores fadd(pending, amount)");
  check(rig.u(p + 0x2dc) == (0x11 | 0x80000) && rig.stored(p + 0x2dc, 0x11 | 0x80000),
        "take: flag word +0x2dc stores the OR");
  check(rig.u(p + 0x2e8) == SENTINEL && rig.u(p + 0x228) == HP_START &&
            rig.u(RECORD + 0xac) == HP_START && rig.u(p + 0x58c) == 2 &&
            rig.u(C::SHARED_SEED) == seed,
        "take: speed, object and record hit points, state and seed untouched");
  check(!rig.wrote(p + 0x228) && !rig.wrote(RECORD + 0xac),
        "take: no hit-point store in the damage slot");
  rig.world.clear_history();
  rig.world.take_damage(p, 0, 0, 0);
  check(rig.u(p + 0xc8) == added && rig.stored(p + 0xc8, added),
        "take: amount 0 stores the same sum again");

  struct Drop {
    const char *label;
    std::function<void(Rig &)> setup;
    bool gate;
  };
  const Drop drops[] = {
      {"recall", [](Rig &r) { r.world.write(G_RECALL, 1, 1); }, false},
      {"menu lockout", [](Rig &r) { r.world.write(G_MENU_LOCKOUT, 4, 60); }, false},
      {"exit countdown", [](Rig &r) { r.world.write(G_EXIT_COUNTDOWN, 2, 1); }, false},
      {"jump-gate query", [](Rig &) {}, true},
  };
  for (const auto &drop : drops) {
    auto s = services();
    if (drop.gate) s.jumpgate_travel_state = [] { return R::Raw(1); };
    Rig r(s);
    const auto body = r.object;
    r.plant(seed, 0x60, SENTINEL, 0x11, HP_START, HP_START);
    drop.setup(r);
    r.world.clear_history();
    r.world.take_damage(body, 0, 0x80000, PLAIN);
    check(r.world.memory().log().empty() && r.u(body + 0xc8) == SENTINEL &&
              r.u(body + 0x2dc) == 0x11,
          std::string("take: ") + drop.label + " drops the hit with an empty write log");
  }
}

// Player_processDamage 0x1b2cc0, player-spells-numeric-r3.md:143-163;
// difftest_spell_numeric_r3.py:507-566.
void player_resolve(std::uint32_t seed) {
  {
    Rig rig(services());
    const auto p = rig.object;
    rig.plant(seed, 0x60, PLAIN, 0x11, HP_START, HP_START);
    rig.world.process_damage(p);
    check(rig.u(RECORD + 0xac) == R::sub(HP_START, PLAIN) && rig.u(p + 0x228) == HP_START,
          "process: record stores fsub(100, 40); object hit points stay 100");
    check(rig.stored(p + 0xc8, 0) && rig.stored(p + 0x2dc, 0) && rig.u(p + 0x58c) == 2 &&
              rig.u(p + 0x0c) == 0x160 && rig.u(p + 0x2e8) == SENTINEL &&
              !rig.wrote(p + 0x2e8),
          "process: survived hit clears pending and flags, 0x160, state 2, no speed");
    check(rig.u(C::SHARED_SEED) == seed_step(seed) && !rig.wrote(ALIVE_COUNT),
          "process: survived hit draws one seed step and does not rebuild the alive list");
  }
  {
    Rig rig(services());
    const auto p = rig.object;
    rig.plant(seed, 0x60, HP_START - 1, 0x11, HP_START, HP_START);
    rig.world.process_damage(p);
    const auto left = R::sub(HP_START, HP_START - 1);
    check(rig.u(RECORD + 0xac) == left && left != 0 && rig.u(p + 0x58c) == 2,
          "process: one pattern below the record leaves it positive, state 2");
  }
  {
    Rig rig(services());
    const auto p = rig.object;
    rig.plant(seed, 0x60, HP_START, 0x11, HP_START, HP_START);
    rig.world.process_damage(p);
    check(rig.u(RECORD + 0xac) == 0 && rig.stored(RECORD + 0xac, 0) &&
              rig.u(p + 0x58c) == 4 && rig.u(p + 0x0c) == 0x169,
          "process: killing hit stores record 0, state 4, flags 0x169");
    check(rig.stored(p + 0xc8, 0) && rig.stored(p + 0x2dc, 0),
          "process: killing hit clears pending and the flag word");
    check(rig.writes_to(ALIVE_COUNT) == 1 && rig.u(ALIVE_COUNT) == 0,
          "process: killing hit rebuilds the alive list once");
    check(rig.u(C::SHARED_SEED) == seed && rig.u(p + 0x2e8) == SENTINEL &&
              !rig.wrote(p + 0x2e8),
          "process: killing hit draws no seed and writes no knock-back speed");
  }
  {
    Rig rig(services());
    const auto p = rig.object;
    rig.plant(seed, 0x60, 0, 0x11, 0, 0);
    rig.world.process_damage(p);
    check(rig.u(p + 0x58c) == 2 && rig.u(RECORD + 0xac) == 0 &&
              !rig.wrote(RECORD + 0xac) && rig.u(p + 0x0c) == 0x60 &&
              rig.stored(p + 0x2dc, 0) && rig.u(C::SHARED_SEED) == seed &&
              !rig.wrote(ALIVE_COUNT),
          "process: pending 0 does not kill a record already at 0");
  }
  // Knock-back speed, player-spells-numeric-r3.md:166-179;
  // difftest_spell_numeric_r3.py:567-587.
  for (unsigned k = 0; k < 4; ++k) {
    struct Row { R::Raw pending; std::uint32_t state; };
    for (const Row row : {Row{PLAIN, 3}, Row{HP_START, 4}, Row{0, 3}}) {
      Rig rig(services());
      const auto p = rig.object;
      rig.plant(seed, 0x60, row.pending, KNOCK_BITS[k], HP_START, HP_START);
      rig.world.process_damage(p);
      check(rig.u(p + 0x2e8) == PLAYER_SPEED[k] && rig.stored(p + 0x2e8, PLAYER_SPEED[k]) &&
                rig.u(p + 0x58c) == row.state && rig.u(C::SHARED_SEED) == seed_step(seed),
            "process: knock bit " + std::to_string(k) + " speed, state and one seed step");
      if (row.pending == 0)
        check(rig.u(RECORD + 0xac) == HP_START, "process: knock tick without pending keeps the record");
      else if (row.pending == PLAIN)
        check(rig.u(RECORD + 0xac) == R::sub(HP_START, PLAIN), "process: knock hit still debits");
      else
        check(rig.u(RECORD + 0xac) == 0 && rig.u(p + 0x0c) == 0x169,
              "process: knock-back killing hit ends at 0x169");
    }
  }
  {
    Rig rig(services());
    const auto p = rig.object;
    rig.plant(seed, 0x60, PLAIN, 0x40000 | 0x80000, HP_START, HP_START);
    rig.world.process_damage(p);
    check(rig.u(p + 0x2e8) == PLAYER_SPEED[0], "process: the first set knock bit selects the speed");
  }
}

// Creature_processDamage 0x110d30, player-spells-numeric-r3.md:181-195;
// difftest_spell_numeric_r3.py:589-612. The note's slot 27 is a zero stand-in
// (:194); the live and no-pending rows run the Skelight's own slot 27 and match
// the note's words unchanged.
void creature_knock(std::uint32_t seed) {
  for (unsigned k = 0; k < 4; ++k) {
    {
      Rig rig(services(), false);
      const auto c = rig.object;
      rig.plant(seed, 0x20, PLAIN, KNOCK_BITS[k], HP_START, HP_START);
      rig.world.process_damage(c);
      check(rig.u(c + 0x2e8) == CREATURE_HIT_SPEED[k] &&
                rig.stored(c + 0x2e8, CREATURE_HIT_SPEED[k]) && rig.u(c + 0x228) == HP_START &&
                rig.u(c + 0x0c) == 0x120 && rig.stored(c + 0xc8, 0) &&
                rig.u(C::SHARED_SEED) == seed,
            "creature: live hit table " + std::to_string(k));
    }
    {
      Rig rig(services(), false);
      const auto c = rig.object;
      rig.plant(seed, 0x20, 0, KNOCK_BITS[k], HP_START, HP_START);
      rig.world.process_damage(c);
      check(rig.u(c + 0x2e8) == CREATURE_IDLE_SPEED[k] &&
                rig.stored(c + 0x2e8, CREATURE_IDLE_SPEED[k]) && rig.u(c + 0x0c) == 0x20 &&
                rig.stored(c + 0x2dc, 0) && rig.u(C::SHARED_SEED) == seed,
            "creature: no-pending table " + std::to_string(k));
    }
    // Already-dead row. The note's 0x2129 is 0x20 | 0x100 | 0x9 | 0x2000 with a
    // zero stand-in at slot 27 (:191-194). The live Skelight's own slot 27
    // (Skelight_setState 0x2a91b8, state 0x14) also ORs 2 into +0xc
    // (native/src/arena_monsters.cpp skelight_set_state case 0x14). The store
    // order pins that bit to the slot-27 call between the 0x9 death store and
    // the 0x2000 store, so Creature_processDamage's own words are the note's.
    {
      Rig rig(services(), false);
      const auto c = rig.object;
      rig.plant(seed, 0x20, PLAIN, KNOCK_BITS[k], 0, HP_START);
      rig.world.process_damage(c);
      std::vector<std::uint64_t> flag_stores;
      for (const auto &w : rig.world.memory().log())
        if (w.address == c + 0x0c && w.size == 4) flag_stores.push_back(w.value);
      std::string seen;
      for (auto v : flag_stores) seen += " " + hex(std::uint32_t(v));
      check(rig.u(c + 0x2e8) == CREATURE_DEAD_SPEED[k] &&
                rig.stored(c + 0x2e8, CREATURE_DEAD_SPEED[k]) && rig.u(c + 0x228) == 0 &&
                rig.u(C::SHARED_SEED) == seed &&
                flag_stores == std::vector<std::uint64_t>({0x120, 0x129, 0x12b, 0x212b}) &&
                (rig.u(c + 0x0c) & ~R::Raw(2)) == 0x2129,
            "creature: zero-hit-point table " + std::to_string(k) + " speed " +
                hex(rig.u(c + 0x2e8)) + " flag stores" + seen + " hp " +
                hex(rig.u(c + 0x228)) + " seed " + hex(rig.u(C::SHARED_SEED)));
    }
  }
}

// Creature_applyKnockback 0x115eb8, player-spells-numeric-r3.md:197-201;
// difftest_spell_numeric_r3.py:614-631.
void apply_knock(std::uint32_t seed) {
  struct Case { std::uint32_t network; bool player; R::Raw pvp_b; bool moves; R::Raw speed; };
  const Case cases[] = {{1, true, 0, false, 0},
                        {0, true, 0, true, TWENTY_EIGHT},
                        {0, true, 1, false, 0},
                        {1, false, 0, true, TWENTY_EIGHT}};
  for (const auto &kase : cases) {
    Rig rig(services(), kase.player);
    const auto c = rig.object;
    rig.plant(seed, kase.player ? 0x60 : 0x20, 0, 0, HP_START, HP_START);
    rig.world.write(G_NETWORK, 1, kase.network);
    rig.world.write(G_PVP_B, 4, kase.pvp_b);
    rig.world.write(c + 0x2e8, 4, THIRTY);
    rig.world.write(c + 0x2e0, 2, 0);
    const auto x = rig.u(c + 0x10);
    rig.world.clear_history();
    rig.world.apply_knockback(c, R::ONE);
    const bool moved = rig.u(c + 0x10) != x;
    check(moved == kase.moves && rig.u(c + 0x2e8) == kase.speed &&
              rig.stored(c + 0x2e8, kase.speed),
          std::string("knockback: ") + (kase.player ? "player" : "creature") +
              (kase.network ? " net" : " offline") + (kase.pvp_b ? " PvP B" : ""));
  }
}
} // namespace

int main() {
  try {
    for (auto seed : SEEDS) {
      player_queue(seed);
      player_resolve(seed);
      creature_knock(seed);
      apply_knock(seed);
    }
  } catch (const std::exception &e) {
    std::cerr << "player-damage: FAIL after " << checks << " checks: " << e.what() << "\n";
    return 1;
  }
  std::cout << "player-damage: " << checks << " checks passed\n";
  return 0;
}
