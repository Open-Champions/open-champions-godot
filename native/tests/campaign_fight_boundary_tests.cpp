// MINOP-1: missing live services must refuse before gameplay effects commit.
// The placements and holder bytes here are authored synthetic fixtures.
#include "champions/campaign_fight.hpp"
#include "champions/combat_ctors.hpp"
#include "champions/combat_runtime.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace R = champions::simulation::rta;
namespace C = R::combat_runtime;
namespace F = R::campaign_fight;
namespace A = R::arena_monsters;

namespace {
void check(bool value, const char *label) {
  if (!value) throw std::runtime_error(label);
}
template <class Fn> void refuses(Fn fn, const char *label) {
  bool refused = false;
  try { fn(); } catch (const C::Unsupported &) { refused = true; }
  check(refused, label);
}
void boundaries() {
  auto services = C::flat_fixture_services();
  services.raycast = [](const R::Vec3 &, const R::Vec3 &, R::Raw, R::Raw) {
    return R::MINUS_ONE;
  };
  C::World world(services, 0, C::Composition::Full);
  F::Counts counts;
  std::vector<std::string> labels;
  auto &model = world.creature_services();
  F::bind_minotaur_boundaries(model, world, counts,
      [&](const std::string &label) { labels.push_back(label); });
  auto call = [&](const char *name, std::vector<A::Value> args) {
    return model.callbacks.at(name)(model, args);
  };
  refuses([&] { call("Creature_init", {std::int64_t(0x100000)}); },
          "unavailable Creature_init refuses instead of completing construction");
  check(labels.size() == 1 && labels[0].find("Creature_init") != std::string::npos,
        "constructor refusal is recorded");

  C::Spawn creature;
  creature.team = 1;
  const auto attacker = world.spawn_skelight(creature);
  creature.team = 2;
  creature.position[0] = R::from_int(20);
  const auto victim = world.spawn_skelight(creature);
  world.relink(attacker + C::TARGET, victim);
  check(world.can_see(attacker, victim, 0, 8000, 0, 0, R::from_int(80)),
        "synthetic hit passes the sight gate");
  const auto hp = world.read(victim + 0x228);
  refuses([&] { call("Creature_hitTargetInArc", {
      std::int64_t(attacker), 0, 8000, 1, 2, 3, 8, 0, 0,
      std::int64_t(R::from_int(80))}); },
      "sight-approved hit refuses when damage service is unbound");
  check(counts.minotaur_hit_requests == 1 && world.read(victim + 0x228) == hp,
        "hit request counted without claiming delivered damage");

  const auto holder = attacker + 0x3c8;
  world.write(holder, 4, R::combat_ctors::VT_WEAPONSHIELD);
  world.write(holder + 4, 1, 1);
  world.write(holder + 8, 1, 2);
  refuses([&] { call("Holder.v11", {std::int64_t(holder), 1,
      std::int64_t(attacker), 0, 0}); },
      "held-part publication refuses when drop service is unbound");
  check(counts.held_part_requests == 1 && world.read(holder + 4, 1) == 1,
        "held bit stays set until publication succeeds");
  check(labels.size() == 3, "all three refusals were recorded");
}
} // namespace

int main() {
  try { boundaries(); std::cout << "campaign-fight-boundaries: 3 refusals pass\n"; }
  catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
