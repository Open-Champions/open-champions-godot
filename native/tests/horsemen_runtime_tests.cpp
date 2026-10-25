#include "champions/combat_runtime.hpp"
#include "champions/combat_play.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>
using namespace champions::simulation::rta::combat_runtime;
using namespace champions::simulation::rta;
static void check(bool yes, const char *message) {
  if (!yes) throw std::runtime_error(message);
}
int main() {
  try {
    auto services = flat_fixture_services();
    auto attacking_rider = std::make_shared<Address>(0);
    auto charging_mount = std::make_shared<Address>(0);
    services.animation_start = [](Address, Raw, Raw, Raw, Raw) {};
    services.animation_advance = [attacking_rider](Address actor, Raw) {
      return actor == *attacking_rider ? Raw(1) : Raw(0);
    };
    services.in_knockdown_anim = [](Address) { return false; };
    services.animation_root = [charging_mount](Address actor) {
      return std::array<Raw, 2>{actor == *charging_mount ? from_int(1) : Raw(0),
                                Raw(0)};
    };
    services.is_class = [](Address, const std::string &) { return false; };
    services.loot_generate = [](const LootDrop &) {};
    World world(std::move(services), 1, Composition::Full);
    Spawn player;
    player.position = {from_int(-20), from_int(80), from_int(0)};
    player.team = 0;
    auto p = world.spawn_player(0, player);
    Spawn rider, mount;
    rider.position = mount.position = {from_int(0), from_int(80), from_int(0)};
    rider.facing = mount.facing = 0xc000;
    rider.team = mount.team = 1;
    auto pair = world.spawn_mounted_horseman(0, rider, mount);
    auto r = pair[0], m = pair[1];
    *attacking_rider = r;
    *charging_mount = m;
    check(world.contains(r) && world.contains(m), "both pool actors registered");
    check(world.target(r + 0x3c8) == m && world.target(m + 0x3c8) == r,
          "rider and mount link both directions");
    check(world.read(r + 0x232, 2) == 0x1b, "rider starts mounted");
    auto holder = r + 0x3d0;
    check(world.read(r + 0x50) == holder &&
              world.read(holder) == 0x50cda0 &&
              world.read(holder + 8, 1) == 0x1b &&
              world.read(holder + 0x98, 1) == 0x23,
          "rider has the equipped WeaponShield holder");
    world.relink(m + TARGET, p);
    world.run_every_frame(r);
    world.run_every_frame(m);
    world.set_mounted_state(m, 7);
    world.write(m + 0x11a, 2, 8); // authored clip is playing
    const auto charge_x = world.read(m + 0x10);
    world.tick();
    check(world.read(m + 0x10) != charge_x &&
              world.read(r + 0x10) == world.read(m + 0x10),
          "charge root motion moves the mount and rider copies its position");
    if (world.read(m + 0x232, 2) != 7)
      throw std::runtime_error("mount charge state after run: " +
                               std::to_string(world.read(m + 0x232, 2)));
    bool trampled = false;
    for (const auto &event : world.events())
      trampled |= event.name == "damage_apply" && event.args.size() >= 2 &&
                   event.args[0] == p && event.args[1] == m;
    check(trampled, "charge contact routes damage from the mount");
    auto player_hp = world.read(p + 0x228);
    world.set_mounted_state(m, 8);
    world.write(r + 0x11a, 2, 8); // authored rider swing clip is playing
    check(world.read(r + 0x232, 2) >= 0x20 &&
              world.read(r + 0x232, 2) <= 0x22,
          "mount swing pushes a documented rider attack");
    world.tick();
    check(world.read(p + 0x228) < player_hp, "mounted rider swing damages player");
    bool swung = false;
    for (const auto &event : world.events())
      swung |= event.name == "damage_apply" && event.args.size() >= 2 &&
               event.args[0] == p && event.args[1] == r;
    if (!swung) {
      std::string detail = "mounted swing routes damage from the rider; state=" +
          std::to_string(world.read(r + 0x232, 2));
      for (const auto &event : world.events())
        if (event.name == "damage_apply") {
          detail += " hit=";
          for (auto arg : event.args) detail += std::to_string(arg) + ",";
        }
      throw std::runtime_error(detail);
    }
    auto rider_hp = world.read(r + 0x228);
    auto mount_hp = world.read(m + 0x228);
    world.take_damage(r, p, 0, from_int(1));
    check(world.read(r + 0x228) != rider_hp && world.read(m + 0x228) == mount_hp,
          "rider damage stays on rider");
    world.take_damage(r, p, 0, from_int(1000));
    world.process_damage(r);
    check(world.read(r + 0x228) > 0x80000000u &&
              world.read(r + 0x232, 2) >= 0x19 &&
              world.target(r + 0x3c8) == m,
          "lethal rider hit keeps the documented mounted-state refusal");
    check(!world.experience().empty(),
          "base lethal path still records its experience award call");
    rider_hp = world.read(r + 0x228);
    world.take_damage(m, p, 0, from_int(1000));
    check(world.read(r + 0x228) == rider_hp,
          "mount damage does not redirect to the rider");
    world.process_damage(m);
    check(world.read(m + 0x232, 2) == 0x14, "mount enters death");
    check(world.read(r + 0x232, 2) == 0x23, "mount death dismounts rider");
    check(world.target(m + 0x3c8) == 0 && world.target(r + 0x3c8) == 0,
          "death and dismount clear both handles");
    // Keep the synthetic rider alive through the mount's deletion flush;
    // the earlier linked-rider lethal probe intentionally left its HP below zero.
    world.write(r + 0x228, 4, from_int(1));
    for (int i = 0; i < 100; ++i) world.run_actor(m);
    check(world.read(m + 0x3cc, 2) >= 100 &&
              (world.read(m + 12) & 0x4002) == 0x4002,
          "dead mount fades for 100 runs and queues deletion");
    world.tick();
    check(!world.contains(m), "run-list flush deletes faded mount");
    world.take_damage(r, p, 0, from_int(2));
    world.process_damage(r);
    check(world.read(r + 0x232, 2) == 0x14,
          "the dismounted rider can enter ordinary on-foot death");
    auto terrain = session_control::Controller().setup().initial_terrain;
    combat_play::Session fixture(terrain, combat_play::Fixture::Full);
    Spawn fixture_rider = rider, fixture_mount = mount;
    fixture_rider.position = fixture_mount.position =
        {from_int(0), from_int(-20), from_int(0)};
    fixture_rider.facing = fixture_mount.facing = 0x8000;
    auto fixture_pair = fixture.spawn_mounted_horseman(0, fixture_rider, fixture_mount);
    check(fixture.world().contains(fixture_pair[0]) &&
              fixture.world().contains(fixture_pair[1]),
          "Full combat fixture registers the mounted pair");
    auto seen = fixture.actors();
    bool rider_seen = false, mount_seen = false;
    for (const auto &actor : seen) {
      rider_seen |= actor.slot == 7 && actor.present;
      mount_seen |= actor.slot == 8 && actor.present;
    }
    check(rider_seen && mount_seen,
          "Full combat fixture exposes both mounted actors");
    check(fixture.world().read(fixture_pair[0] + 0x50) == fixture_pair[0] + 0x3d0,
          "Full combat fixture rider has a weapon holder");
    auto &fw = fixture.world();
    fw.run_every_frame(fixture_pair[0]);
    fw.run_every_frame(fixture_pair[1]);
    fw.set_mounted_state(fixture_pair[1], 7);
    fixture.advance(0);
    auto frame = fixture.advance(0);
    check(!frame.failed && frame.ready_ticks == 1,
          "mounted pair completes a Full combat ready tick");
    bool rider_ran = false, mount_ran = false;
    for (const auto &event : fixture.tick_events())
      if (event.name == "run" && !event.args.empty()) {
        rider_ran |= event.args[0] == fixture_pair[0];
        mount_ran |= event.args[0] == fixture_pair[1];
      }
    check(rider_ran && mount_ran,
          "both mounted actors pass through the ordinary run-list scheduler");
    Address fixture_player = 0;
    for (const auto &actor : fixture.actors())
      if (actor.slot == 0) fixture_player = fw.target(actor.handle);
    check(fixture_player != 0, "Full fixture player exists");
    fw.relink(fixture_pair[1] + TARGET, fixture_player);
    fw.set_mounted_state(fixture_pair[1], 0x18);
    check(fw.read(fixture_pair[0] + 0x232, 2) == 0x1f,
          "mount state 0x18 pushes the fourth rider swing");
    bool fourth_swing_event = false;
    for (int i = 0; i < 10; ++i) {
      frame = fixture.advance(0);
      check(!frame.failed, "fourth mounted swing completes a ready tick");
      for (const auto &event : fixture.tick_events())
        if (event.name == "damage_apply" && event.args.size() >= 2 &&
            event.args[1] == fixture_pair[0] &&
            fw.read(fixture_pair[0] + 0x232, 2) == 0x1f)
          fourth_swing_event = true;
    }
    check(fourth_swing_event,
          "state 0x1f rider hit event fires through the Full fixture");
    std::cout << "mounted fight smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
