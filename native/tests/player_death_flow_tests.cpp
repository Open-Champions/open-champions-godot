#include "champions/player_death_flow.hpp"
#include "champions/combat.hpp"
#include <iostream>
#include <map>
#include <stdexcept>
#include <vector>

namespace R = champions::simulation::rta;
namespace D = R::player_death_flow;
namespace C = R::combat_runtime;
namespace {
unsigned checks = 0;
void check(bool condition, const char *message) {
  ++checks;
  if (!condition) throw std::runtime_error(message);
}
struct Fixture {
  std::map<D::Address, std::uint8_t> bytes;
  std::vector<std::string> calls;
  bool frozen = false, fewer_two = false, pending = false, match = true;
  int act = 0;
  R::Raw fade = 0;
  D::Memory memory() {
    return {[this](D::Address address, unsigned size) {
      std::uint64_t value = 0;
      for (unsigned index = 0; index < size; ++index)
        value |= std::uint64_t(bytes[address + index]) << (8 * index);
      return value;
    }, [this](D::Address address, unsigned size, std::uint64_t value) {
      for (unsigned index = 0; index < size; ++index)
        bytes[address + index] = std::uint8_t(value >> (8 * index));
    }};
  }
  D::Services services() {
    D::Services result;
    result.frozen = [this] { calls.push_back("frozen"); return frozen; };
    result.fewer_than_two_active = [this] { calls.push_back("peers"); return fewer_two; };
    result.close_menus = [this] { calls.push_back("close"); };
    result.clear_fade = [this] { calls.push_back("clear"); fade = 0; };
    result.step_fade = [this](R::Raw delta) {
      check(delta == 4, "death task fade argument");
      calls.push_back("fade"); return fade;
    };
    result.find_act = [this] { calls.push_back("act"); return act; };
    result.reload_checkpoint = [this] { calls.push_back("reload"); pending = true; };
    result.request_current_level = [this] { calls.push_back("restart"); };
    result.pending_level = [this] { calls.push_back("pending"); return pending; };
    result.qualified_names_match = [this] { calls.push_back("match"); return match; };
    result.arena_respawn = [this](unsigned slot) { calls.push_back("arena" + std::to_string(slot)); };
    return result;
  }
};
void checks_all_dead() {
  for (unsigned mask = 0; mask < 512; ++mask) {
    Fixture fixture;
    auto memory = fixture.memory();
    D::Flow flow(memory, fixture.services());
    flow.level_enter(2);
    memory.write(C::NUM_PLAYERS, 4, 2);
    const unsigned alive = (mask & 1) ? 1 : 0;
    const bool cutscene = mask & 2, loaded = mask & 4, pvp = mask & 8;
    const bool paused = mask & 16, net = mask & 32;
    fixture.frozen = mask & 64;
    fixture.fewer_two = mask & 128;
    const unsigned timer = (mask & 256) ? 1 : 240;
    memory.write(0x4f8420, 4, alive);
    memory.write(0x44fcd0, 1, cutscene);
    memory.write(0x4f85b0, 4, loaded);
    memory.write(0x4f7120, 4, pvp);
    memory.write(0x41b4f4, 4, paused);
    memory.write(0x402791, 1, net);
    memory.write(0x4f7194, 2, timer);
    memory.write(0x4f84a8, 2, timer);
    memory.write(C::PLAYERS + C::RECORD_SIZE + 0xac, 4, R::ONE);
    flow.check_all_dead();
    const bool wipe = !cutscene && loaded && !pvp && !alive && !fixture.frozen && (!net || !fixture.fewer_two);
    check(memory.read(0x4f7194, 2) == timer - unsigned(wipe), "wipe gate/timer");
    check(memory.read(0x4f84a8, 2) == timer - unsigned(!cutscene && !paused), "dead slot gate/timer");
    check(memory.read(0x4f84aa, 2) == 240, "living slot timer unchanged");
    check(flow.running() == (wipe && timer == 1), "schedule only on zero crossing");
    const bool arena = pvp && !cutscene && !paused && timer == 1;
    bool saw_arena = false;
    for (const auto &call : fixture.calls) saw_arena |= call == "arena0";
    check(saw_arena == arena, "only PvP B auto-respawns slot");
    if (flow.running()) {
      check(fixture.calls[fixture.calls.size() - 2] == "close" && fixture.calls.back() == "clear", "close before schedule setup");
      fixture.calls.clear(); flow.check_all_dead();
      check(fixture.calls == std::vector<std::string>({"frozen"}) ||
            fixture.calls == std::vector<std::string>({"frozen", "peers"}), "no duplicate schedule");
    }
  }
}
void task_phases() {
  for (int act : {-1, 0}) {
    Fixture fixture;
    fixture.act = act;
    auto memory = fixture.memory();
    memory.write(C::NUM_PLAYERS, 4, 2);
    memory.write(C::PLAYERS + 0xb0, 4, R::combat::itof(160));
    memory.write(C::PLAYERS + C::RECORD_SIZE + 0xb0, 4, R::combat::itof(80));
    for (auto offset : {0x5c, 0xe4, 0x43f0, 0xef}) memory.write(C::PLAYERS + offset, 4, 12345);
    D::Flow flow(memory, fixture.services());
    flow.schedule();
    flow.task_step(); check(flow.phase() == 0, "wait for full fade");
    fixture.fade = 128; flow.task_step(); check(flow.phase() == 1, "fade complete");
    flow.task_step();
    if (act == -1) {
      check(!flow.running(), "off-table restart removes task");
      check(memory.read(C::PLAYERS + 0xac, 4) == memory.read(C::PLAYERS + 0xb0, 4), "restart full HP");
      check(memory.read(C::PLAYERS + C::RECORD_SIZE + 0xac, 4) == memory.read(C::PLAYERS + C::RECORD_SIZE + 0xb0, 4), "restart every player");
    } else {
      check(flow.phase() == 2 && flow.running(), "checkpoint reload waits");
      check(memory.read(C::PLAYERS + 0xac, 4) == 0, "task must not invent checkpoint HP");
      flow.task_step(); check(flow.running(), "wait for pending load");
      fixture.pending = false; fixture.match = false; flow.task_step(); check(flow.running(), "wait for qualified name match");
      fixture.match = true; flow.task_step(); check(!flow.running(), "matched load removes task");
    }
    for (auto offset : {0x5c, 0xe4, 0x43f0, 0xef}) check(memory.read(C::PLAYERS + offset, 4) == 12345, "no invented death tax or item debit");
  }
  Fixture alternate;
  auto memory = alternate.memory();
  D::Flow flow(memory, alternate.services());
  memory.write(0x4f712c, 4, 1);
  flow.schedule(); flow.task_step();
  check(flow.phase() == 10 && alternate.calls == std::vector<std::string>{"clear"}, "alternate bypasses fade");
  flow.task_step(); check(flow.running(), "alternate waits for living player");
  memory.write(0x4f8420, 4, 1); flow.task_step(); check(!flow.running(), "alternate removes after external revive");
  memory.write(0x4f712c, 4, 0);
  flow.schedule(true); alternate.fade = 128; flow.task_step(); flow.task_step();
  check(flow.phase() == 2 && memory.read(0x402791, 1) == 1, "forced-network phase");
}
void refusals() {
  Fixture fixture;
  auto memory = fixture.memory();
  auto services = fixture.services();
  services.reload_checkpoint = {};
  D::Flow flow(memory, services);
  flow.schedule(); fixture.fade = 128; flow.task_step();
  bool refused = false;
  try { flow.task_step(); }
  catch (const C::Unsupported &error) { refused = std::string(error.what()).find("invalid-buffer behavior Unknown") != std::string::npos; }
  check(refused && flow.phase() == 1 && flow.running(), "unknown save buffer must refuse, not heal");
  for (unsigned timer : {0u, 0xffffu, 0x8000u}) {
    flow.level_enter(1);
    memory.write(C::NUM_PLAYERS, 4, 1); memory.write(0x4f85b0, 4, 1);
    memory.write(0x4f7194, 2, timer); memory.write(0x4f84a8, 2, timer);
    flow.check_all_dead();
    check(memory.read(0x4f7194, 2) == timer && memory.read(0x4f84a8, 2) == timer && !flow.running(), "signed/nonpositive countdown never underflows");
  }
  memory.write(C::NUM_PLAYERS, 4, 5);
  refused = false;
  try { flow.check_all_dead(); } catch (const C::Unsupported &) { refused = true; }
  check(refused, "refuse invalid player count");
}
// Real Player_processDamage/state-4 execution in the shared combat World,
// with authored service answers and clip identities, not a second death model.
void lethal_player_in_world() {
  for (unsigned model = 0; model < 12; ++model) {
    for (unsigned weapon : {2u, 5u}) {
      C::Address actor = 0;
      std::vector<std::uint64_t> started;
      auto services = C::flat_fixture_services();
      services.animation_start = [&](C::Address a, R::Raw clip, R::Raw flags, R::Raw blend, R::Raw out) {
        check(a == actor, "death animation on the original Player identity");
        started = {clip, flags, blend, out};
      };
      C::World world(services, 0, C::Composition::Full);
      C::Spawn spawn;
      spawn.hp = R::combat::itof(30);
      actor = world.spawn_player(0, spawn);
      spawn.position[0] = R::combat::itof(1000);
      const auto enemy = world.spawn_skelight(spawn);
      const auto record = C::PLAYERS;
      world.write(record + 0x54, 4, model);
      world.write(record + 0x4a86, 1, weapon);
      world.write(record + 0x5c, 4, 1234); // XP, mana, Gold, bags and equipment
      world.write(record + 0xc0, 4, R::ONE);
      world.write(record + 0xe4, 4, 678);
      world.write(record + 0x130, 4, 0x12345678);
      world.write(record + 0x43f0, 4, 0x87654321);
      world.write(C::FRAME, 4, 77);
      const auto definition = C::Address(world.read(actor + 0x24));
      const bool alternate = weapon == 2 && model != 2 && model != 3 && model != 6 && model != 7;
      const auto expected_clip = world.read(definition + (alternate ? 0x14 : 0x0c));
      world.take_damage(actor, enemy, 1, spawn.hp);
      check(world.read(record + 0xac) == spawn.hp, "takeDamage only queues the lethal hit");
      world.process_damage(actor);
      check(world.read(record + 0xac) == 0 && (world.read(actor + 12) & 8), "lethal damage clamps HP and marks the actual object dead");
      check(world.read(actor + 0x58c) == 4 && world.read(actor + 0x448) == 77, "death state and entry frame");
      check(started == std::vector<std::uint64_t>({expected_clip, 0x206, R::HALF, 0}), "audited model/weapon selector, flags and blend");
      check(world.read(record + 0x43d8) == actor && world.contains(actor), "death keeps the body, no invented respawn");
      check(world.read(0x4f8420) == 0, "death rebuilds alive list immediately");
      check(world.read(actor + 0xc8) == 0 && world.read(actor + 0x2dc) == 0, "pending damage cleared");
      check(world.read(record + 0x5c) == 1234 && world.read(record + 0xc0) == R::ONE &&
            world.read(record + 0xe4) == 678 && world.read(record + 0x130) == 0x12345678 &&
            world.read(record + 0x43f0) == 0x87654321, "no death debit of XP, mana, Gold, bags or equipment");
    }
  }
}
void world_task_order() {
  C::World *world = nullptr;
  D::Flow *flow = nullptr;
  std::vector<std::string> order;
  auto services = C::flat_fixture_services();
  services.death_task = [&] {
    check(world->read(C::FRAME) == 0, "priority-20 task before the frame increment");
    check(world->read(D::WIPE_COUNTDOWN, 2) == 1, "task before end-of-tick all-dead check");
    order.push_back("task");
    flow->task_step();
  };
  services.check_all_dead = [&] {
    check(world->read(C::FRAME) == 1, "all-dead at end of completed tick");
    order.push_back("check");
    flow->check_all_dead();
  };
  C::World actual(services, 0, C::Composition::Full);
  world = &actual;
  // No fixture player is needed to test task scheduling. Globals script a
  // party wipe, while the actual World owns the tick ordering and memory.
  Fixture answers;
  D::Memory memory{
      [&](D::Address a, unsigned n) { return actual.read(a, n); },
      [&](D::Address a, unsigned n, std::uint64_t v) { actual.write(a, n, v); }};
  D::Flow death(memory, answers.services());
  flow = &death;
  death.level_enter(1);
  actual.write(C::NUM_PLAYERS, 4, 1);
  actual.write(0x4f85b0, 4, 1);
  actual.write(D::WIPE_COUNTDOWN, 2, 1);
  actual.tick();
  check(order == std::vector<std::string>({"task", "check"}), "run-pass task precedes alive/wipe check");
  check(death.running() && death.phase() == 0 && answers.calls == std::vector<std::string>({"frozen", "close", "clear"}), "newly scheduled task must not fade in its scheduling tick");
}
}
int main() {
  try {
    checks_all_dead(); task_phases(); refusals(); lethal_player_in_world(); world_task_order();
    std::cout << "player-death-flow: " << checks << " checks passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n'; return 1;
  }
}
