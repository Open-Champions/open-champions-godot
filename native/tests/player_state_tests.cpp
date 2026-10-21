#include "champions/player_state.hpp"
#include "champions/combat.hpp"
#include <iostream>
#include <stdexcept>

namespace p = champions::simulation::rta::player_state;
namespace c = champions::simulation::rta::combat;
void check(bool value, const char *message) {
  if (!value) throw std::runtime_error(message);
}
int main() {
  try {
    p::Tables t;
    for (int n = 0; n < 80; ++n) t.experience[std::size_t(n)] = n * n * 1000;
    t.experience[80] = 99999999;
    for (auto &f : t.formulas)
      f = {c::itof(13), c::itof(2), c::itof(6), c::itof(14), c::itof(3), c::itof(65)};
    const auto made = p::create("Mira", 1, 4, {{46, 37, 24, 28}}, t);
    check(made.level == 1 && made.acknowledged == 1 && made.experience == 0 && made.gold == 500,
          "new character's documented starting fields");
    check(made.max_mana == c::itof(54) && made.max_hp == c::itof(33),
          "new character's maximums use class formula and base statistics");
    check(!made.current_known, "undocumented current values remain unset");
    auto imported = made;
    imported.experience = 4000;
    imported.gold = 8765;
    imported.hp = c::itof(20); imported.mana = c::itof(15);
    imported = p::import(imported, t);
    check(imported.imported && imported.current_known && imported.gold == 8765 && imported.mana == c::itof(15),
          "import retains record fields");
    const auto saved = p::save(imported, t);
    const auto restored = p::load(saved, t);
    check(restored.name == imported.name && restored.experience == imported.experience &&
          restored.max_mana == imported.max_mana && restored.imported,
          "native companion round trip");
    bool refused = false;
    try { auto bad = saved; bad[0] ^= 1; (void)p::load(bad, t); }
    catch (const p::Refusal &) { refused = true; }
    check(refused, "unknown native companion version refused");
    refused = false;
    try { (void)p::create("", 1, 4, {{46, 37, 24, 28}}, t); }
    catch (const p::Refusal &) { refused = true; }
    check(refused, "empty name refused");
    std::cout << "PASS player-state 7 checks\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "FAIL player-state: " << error.what() << '\n';
    return 1;
  }
}
