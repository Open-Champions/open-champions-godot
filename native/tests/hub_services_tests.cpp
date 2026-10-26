#include "champions/hub_services.hpp"
#include <iostream>

int main() {
  namespace hub = champions::simulation::rta::hub_services;
  int checks = 0;
  for (bool busy : {false, true}) {
    for (bool held : {false, true}) {
      hub::Savepoint point;
      const auto result = hub::use_savepoint(point, busy, held);
      const bool accepted = !busy && !held;
      const auto expected = busy ? hub::Use::CardBusy : held ? hub::Use::MenuHeld : hub::Use::Accepted;
      if (result != expected ||
          point.activated != std::uint32_t(accepted) ||
          point.effect_pending != std::uint32_t(accepted))
        return 1;
      ++checks;
      if (accepted) {
        if (hub::use_savepoint(point, true, false) != hub::Use::CardBusy ||
            point.activated != 1 || point.effect_pending != 1)
          return 1;
        ++checks;
      }
    }
  }
  std::cout << checks << " hub service checks passed\n";
}
