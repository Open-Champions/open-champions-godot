#include "champions/combat_runtime.hpp"
#include "champions/combat_play.hpp"
#include "../src/combat_runtime_internal.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>
using namespace champions::simulation::rta::combat_runtime;
using namespace champions::simulation::rta;
static void check(bool yes, const char *message) {
  if (!yes) throw std::runtime_error(message);
}
static void test_property_callbacks() {
  // Exercise the installed original-call adapters, not the host helpers.
  // 338d459 ref_arena_monsters.py:107,588-589; ref_objects.py:199-233.
  auto services = flat_fixture_services();
  services.animation_start = [](Address, Raw, Raw, Raw, Raw) {};
  World::State state(std::move(services), 1, Composition::Full);
  const auto pair = state.spawn_mounted_horseman(0, Spawn{}, Spawn{});
  const auto key = state.alloc_fixture(32); // authored key buffer, no retail address
  auto text = [&](const std::string &value) {
    for (unsigned i = 0; i <= value.size(); ++i)
      state.w(key + i, 1, i == value.size() ? 0 : value[i]);
  };
  auto call = [&](const char *name, Address at, Raw occurrence) {
    return number(state.model.reference_call(name,
        {iv(pair[1]), iv(at), iv(occurrence)}));
  };
  check(call("obj_getPropInt", 0x52fa60, 0) == 1,
        "original MiniBoss key address resolves occurrence zero");
  check(call("obj_getPropInt", 0x52fa60, 1) == 0 &&
        call("obj_getPropStr", 0x52fa60, 0) == 0x547329 &&
        call("obj_getPropStr", 0x52fa60, 1) == 0,
        "single MiniBoss entry has no second occurrence");
  text("name");
  check(call("obj_getPropStr", key, 0) == 0x547335 &&
        call("obj_getPropStr", key, 1) == 0 &&
        call("obj_getPropInt", key, 0) == 0 &&
        call("obj_getPropInt", key, 1) == 0,
        "name callback preserves pointer, atoi and occurrence semantics");
  text("missing");
  for (Raw occurrence : {0u, 1u, 7u})
    check(call("obj_getPropInt", key, occurrence) == 0 &&
          call("obj_getPropStr", key, occurrence) == 0,
          "missing callback keys return zero regardless of occurrence");
  text("");
  for (const auto *name : {"obj_getPropInt", "obj_getPropStr"}) {
    bool refused = false;
    try { (void)call(name, key, 0); }
    catch (const Unsupported &) { refused = true; }
    check(refused, "unavailable callback key data refuses");
  }
}
static void test_mount_properties() {
  test_property_callbacks();
  // Expectations come from audited research 338d459, not the native table.
  const char *names[] = {"horsedisease", "horsewar", "horsepestilence", "horsedeath"};
  const Address lists[] = {0x500848, 0x500854, 0x500860, 0x50086c};
  const Address name_texts[] = {0x547330, 0x547348, 0x547358, 0x547370};
  for (Raw seed : {20973u, 7u}) {
    auto services = flat_fixture_services();
    services.animation_start = [](Address, Raw, Raw, Raw, Raw) {};
    services.is_class = [](Address, const std::string &) { return false; };
    services.loot_generate = [](const LootDrop &) {};
    World world(std::move(services), 1, Composition::Full);
    world.write(SHARED_SEED, 4, seed);
    SharedRandom expected(seed);
    for (unsigned v = 0; v < 4; ++v) {
      const auto pair = world.spawn_mounted_horseman(v, Spawn{}, Spawn{});
      const auto r = pair[0], m = pair[1];
      expected.draw(); expected.draw();
      check(world.read(SHARED_SEED) == expected.state,
            "mount properties introduce no extra shared random draw");
      check(world.read(m + 0x2c) == lists[v] &&
                world.read(lists[v]) == 0x547320 &&
                world.read(lists[v] + 4) == name_texts[v] &&
                world.read(lists[v] + 8) == 0,
            "mount retains the exact two-entry, null-terminated property list");
      check(world.property_int(m, "MiniBoss") == 1 &&
                (world.read(m + 0xa8, 8) & 1),
            "each mount consumes miniboss=1");
      check(world.property_string(m, "name") == names[v] &&
                world.find_by_name(names[v]) == m,
            "each mount name resolves to its own actor");
      check(!world.property_string(m, "ShowName") &&
                !world.property_string(m, "hide") &&
                world.property_int(m, "hide", 7) == 7 &&
                world.read(m + 0x368) == 0 &&
                !(world.read(m + 12) & 0x100000),
            "absent ShowName and hide do not gain synthetic values");
      check(world.read(r + 0x2c) == 0 &&
                !(world.read(r + 0xa8, 8) & 1) &&
                !world.property_string(r, "name") &&
                world.property_int(r, "miniboss") == 0,
            "the fixed mount properties do not leak onto the rider");
      check(world.read(m + 0x3c0) == from_int(2400) &&
                world.read(m + 0x3ae, 2) == 2400 &&
                world.read(m + 0xdc) == 0x3f999999,
            "Nightmare constructor overrides remain intact");
      world.write(lists[v] + 8, 4, 1); // authored malformed-terminator probe
      bool modified_refused = false;
      try { (void)world.property_string(m, "name"); }
      catch (const Unsupported &e) {
        modified_refused = std::string(e.what()).find("differs from audited") != std::string::npos;
      }
      world.write(lists[v] + 8, 4, 0);
      check(modified_refused, "modified fixed mount lists refuse with a diagnostic");
      // Exercise duplicate names, the original table's case-insensitive
      // lookup, and deletion without losing the second live registration.
      const auto duplicate = world.spawn_mounted_horseman(v, Spawn{}, Spawn{});
      expected.draw(); expected.draw();
      auto upper = std::string(names[v]);
      for (auto &c : upper) if (c >= 'a' && c <= 'z') c = char(c - 'a' + 'A');
      check(world.find_by_name(upper) == m, "name lookup preserves first duplicate");
      world.set_mounted_state(m, 0x14);
      world.destroy(m);
      check(!world.contains(m) && world.find_by_name(names[v]) == duplicate[1],
            "destroyed mount name is removed without hiding the live duplicate");
      world.set_mounted_state(duplicate[1], 0x14);
      world.destroy(duplicate[1]);
      check(world.find_by_name(names[v]) == 0, "last mount deletion clears name lookup");
      const auto saved = world.read(duplicate[0] + 0x2c);
      world.write(duplicate[0] + 0x2c, 4, 1); // authored unknown-list probe
      bool refused = false;
      try { (void)world.property_int(duplicate[0], "miniboss"); }
      catch (const Unsupported &e) {
        refused = std::string(e.what()).find("not documented") != std::string::npos;
      }
      world.write(duplicate[0] + 0x2c, 4, saved);
      check(refused, "unknown property lists refuse with a diagnostic");
    }
  }
  // The MiniBoss meter's original display-name body remains a refusal when
  // the host has not explicitly supplied a presentation service.
  auto services = flat_fixture_services();
  services.animation_start = [](Address, Raw, Raw, Raw, Raw) {};
  services.animation_advance = [](Address, Raw) { return Raw(0); };
  World unbound(std::move(services), 1, Composition::Full);
  unbound.spawn_player(0, Spawn{});
  const auto pair = unbound.spawn_mounted_horseman(0, Spawn{}, Spawn{});
  unbound.write(0x4f8424, 4, 1); // documented local alive count/list
  unbound.write(0x4f8430, 4, 0);
  unbound.write(pair[1] + 0xa8, 8,
      unbound.read(pair[1] + 0xa8, 8) | UINT64_C(0x8000000080000000));
  bool name_refused = false;
  try { unbound.run_actor(pair[1]); }
  catch (const Unsupported &e) {
    name_refused = std::string(e.what()).find("display-name service missing") != std::string::npos;
  }
  check(name_refused, "unmodelled original MiniBoss name body stays refused");
}
int main() {
  try {
    test_mount_properties();
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
    check(world.find_by_name("horsedisease") == 0,
          "fade deletion removes the mount's name registration");
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
    bool fixture_name_service = false;
    for (int i = 0; i < 10; ++i) {
      frame = fixture.advance(0);
      if (frame.failed)
        throw std::runtime_error("fourth mounted swing refused: " + frame.error);
      for (const auto &event : fixture.tick_events()) {
        if (event.name == "damage_apply" && event.args.size() >= 2 &&
            event.args[1] == fixture_pair[0] &&
            fw.read(fixture_pair[0] + 0x232, 2) == 0x1f)
          fourth_swing_event = true;
        fixture_name_service |= event.name == "presentation:Creature.v19" &&
            event.args.size() == 2 && event.args[0] == fixture_pair[1];
      }
    }
    check(fourth_swing_event,
          "state 0x1f rider hit event fires through the Full fixture");
    check(fixture_name_service && fw.read(0x41ba40, 1) == 0,
          "MiniBoss meter uses the Full fixture's explicit empty-name service");
    std::cout << "mounted fight smoke passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
