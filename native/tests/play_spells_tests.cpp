#include "champions/combat_play.hpp"
#include <iostream>
#include <stdexcept>

using namespace champions::simulation::rta;
namespace C = combat_runtime;

int main() {
  try {
    const auto terrain = session_control::Controller().setup().initial_terrain;
    combat_play::Session session(terrain, combat_play::Fixture::Full);
    session.select_disease_bolt();
    const auto rec = C::PLAYERS;
    const auto initial = Raw(session.world().read(rec + 0xc0));
    if (initial != from_int(100)) throw std::runtime_error("authored mana setup");
    session.advance(0); // first network submission is not a ready tick
    session.press_disease_bolt();
    session.release_disease_bolt();
    for (int tick = 0; tick < 3; ++tick) {
      const auto state = session.advance(0);
      if (state.failed || session.actors()[0].state == 5)
        throw std::runtime_error("released ability A admitted a later cast");
    }
    session.press_disease_bolt();
    bool admitted = false;
    bool spawned = false, hit = false, paid = false, regenerated = false;
    bool expiry_refused = false;
    const auto enemy = session.world().target(session.actors()[1].handle);
    const auto hp_before = Raw(session.world().read(enemy + 0x228));
    for (int tick = 0; tick < 50 && !hit; ++tick) {
      const auto state = session.advance(0);
      admitted |= session.actors()[0].state == 5;
      for (const auto &event : session.tick_events()) {
        if (event.name == "diseasebolt_spawn") {
          spawned = true;
          session.release_disease_bolt();
          if (Raw(session.world().read(rec + 0xc0)) != from_int(91))
            throw std::runtime_error("rank-1 cast cost differs from 9 mana");
        }
        hit |= event.name == "diseasebolt_hit";
      }
      const auto mana = Raw(session.world().read(rec + 0xc0));
      paid |= lt(mana, initial);
      regenerated |= spawned && lt(from_int(91), mana) && le(mana, initial);
      if (state.failed && !hit)
        throw std::runtime_error("cast failed before hit: " + state.error);
      if (state.failed && hit) {
        if (state.error != "Disease Bolt expiry needs its unverified destructor")
          throw std::runtime_error("unexpected post-hit refusal: " + state.error);
        expiry_refused = true;
      }
    }
    if (!admitted) throw std::runtime_error("held ability A did not enter cast state 5");
    if (!spawned) throw std::runtime_error("projectile did not spawn");
    if (!paid) throw std::runtime_error("cast did not debit mana");
    if (!regenerated) throw std::runtime_error("mana did not regenerate below the cap");
    if (!hit) throw std::runtime_error("projectile did not hit");
    if (!lt(Raw(session.world().read(enemy + 0x228)), hp_before))
      throw std::runtime_error("existing combat damage path did not reduce HP");
    std::cout << "play-spells: exact mana debit, regeneration, projectile spawn, fixture damage PASS; "
              << "unverified expiry refusal " << (expiry_refused ? "observed" : "not reached") << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "play-spells: FAIL: " << e.what() << '\n';
    return 1;
  }
}
