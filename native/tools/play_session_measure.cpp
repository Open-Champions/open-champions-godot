// Authored like-for-like retention benchmark; no checkpoint/replay after ticks.
#include "champions/native_save.hpp"
#include <chrono>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
namespace R = champions::simulation::rta;
namespace C = R::session_control;
int main(int argc, char **argv) {
  try {
    if (argc != 2 || (std::string(argv[1]) != "--legacy" &&
                      std::string(argv[1]) != "--play"))
      throw std::runtime_error("expected --legacy or --play");
    const bool play = std::string(argv[1]) == "--play";
    C::Config config;
    config.movement = C::MovementPolicy::DryCreature;
    config.max_ready_ticks = 512;
    config.play_window = play ? 128 : 0;
    R::native_save::Scene scene(config);
    const auto begin = std::chrono::steady_clock::now();
    for (unsigned i = 0; i <= 512; ++i) {
      auto o = scene.advance(((i / 32) % 2 ? C::NEG_X : C::POS_X) |
                             (i % 2 ? 0 : C::DAMAGE));
      if (o.failed || (o.stopped && (play || i != 512)))
        throw std::runtime_error("measurement workload stopped early");
      if (i != 128 && i != 256 && i != 512)
        continue;
      const auto s = play ? scene.continuation_snapshot() : scene.snapshot();
      const auto &h = s.session.hp->adapter.model;
      const auto &w = s.session.hp->adapter.world;
      const auto &c = *s.session.creature_movement;
      const auto records =
          s.session.events.size() + s.session.draws.size() +
          s.session.network.log.size() + s.session.network.outbox.size() +
          h.stores.size() + h.writes.size() + h.calls.size() + h.events.size() +
          w.events.size() + w.seed.draws.size() + w.cosmetic.draws.size() +
          c.writes.size() + c.calls.size() + c.events.size() +
          c.callback_results.size();
      std::cout << (play ? "play" : "diagnostic") << " ready=" << o.ready_ticks
                << " records=" << records
                << " memory_bytes=" << h.backing.size()
                << " world_links=" << s.session.world->links.size()
                << " iterator_serial=" << s.session.world->iterator_serial
                << " elapsed_us="
                << std::chrono::duration_cast<std::chrono::microseconds>(
                       std::chrono::steady_clock::now() - begin)
                       .count()
                << '\n';
    }
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
