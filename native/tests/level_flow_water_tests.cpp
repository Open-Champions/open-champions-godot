#include "champions/level_flow_water_adapter.hpp"
#include <iostream>
using namespace champions::simulation::rta;
int main() {
  // Authored one-leaf world, two overlapping water tiles, supplied relocated
  // memory. Delegation exercises the actual creature-water-2 implementation.
  creature_water::Memory memory;
  constexpr std::uint32_t world = 0x1000, tree = 0x2000, chunks = 0x3000,
                          payload = 0x5000, output = 0x9000;
  memory.poke(world + 4, 4, tree);
  memory.poke(world + 0x24, 4, chunks);
  memory.poke(tree, 2, 2);
  memory.poke(tree + 2, 2, 0);
  memory.poke(tree + 4, 2, 1);
  auto f = [](int v) { return from_int(std::uint32_t(v)); };
  for (unsigned i = 0; i < 2; ++i) {
    const auto chunk = chunks + 0x3c * i;
    memory.poke(chunk, 4, payload + 0x90 * i);
    memory.poke(chunk + 0x24, 4, 0x80);
    for (unsigned a = 0; a < 3; ++a) {
      memory.poke(chunk + 8 + 4 * a, 4, f(a < 2 ? 0 : 10 + int(i) * 10));
      memory.poke(chunk + 0x14 + 4 * a, 4, f(a < 2 ? 120 : 10 + int(i) * 10));
    }
    memory.poke(payload + 0x90 * i + 8, 4,
                0); // no simulation block, still counted
  }
  creature_water::Service service(memory);
  level_flow::WaterAdapter adapter{service, world};
  auto check = [](bool ok) {
    if (!ok)
      throw std::runtime_error("water adapter mismatch");
  };
  try {
    check(adapter.test(f(12), {f(60), f(60), f(10)}, output) == 1);
    check(memory.load(output, 4) == f(10));
    check(adapter.test(f(12), {f(60), f(60), f(22)}, output) == 0);
    check(memory.load(output, 4) == f(10));
    check(adapter.disturb(f(100), f(12), {f(60), f(60), f(10)}) == 2);
    memory.poke(tree + 2, 2, 1);
    memory.poke(tree + 4, 2, 0);
    check(adapter.test(f(12), {f(60), f(60), f(10)}, output) == 1);
    check(memory.load(output, 4) == f(20));
    std::cout << "PASS water adapter: actual shared Service, ordered hits, "
                 "miss output, disturbance count\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
