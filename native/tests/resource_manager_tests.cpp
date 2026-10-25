#include "champions/resource_manager.hpp"

#include <cstdint>
#include <stdexcept>
#include <vector>

namespace {
void put(std::vector<std::uint8_t> &b, std::size_t at, std::uint32_t value) {
  for (unsigned i = 0; i < 4; ++i) b.at(at + i) = std::uint8_t(value >> (i * 8));
}
void require(bool value) {
  if (!value) throw std::runtime_error("resource manager contract failed");
}
std::vector<std::uint8_t> fixture(std::uint8_t byte) {
  std::vector<std::uint8_t> b(36, 0);
  put(b, 0, 1); put(b, 4, 16); put(b, 8, 32); put(b, 12, 1);
  b[16] = 'a'; b[17] = 0; b[32] = byte;
  return b;
}
} // namespace

int main() {
  champions::resources::Manager manager;
  const auto first = fixture(7);
  manager.load_lmp("actor.lmp", {first.data(), first.size()});
  const auto handle = manager.find("ACTOR.LMP", "A");
  require(handle.has_value() && manager.loaded_lumps() == 1);
  require(!manager.find("actor.lmp", "missing"));
  require(manager.bytes(*handle)->size == 1 && manager.bytes(*handle)->data[0] == 7);
  const auto second = fixture(9);
  manager.load_lmp("actor.lmp", {second.data(), second.size()});
  require(!manager.bytes(*handle));
  const auto replacement = manager.find("actor.lmp", "a");
  require(replacement && manager.bytes(*replacement)->data[0] == 9);
  manager.unload("actor.lmp");
  require(!manager.bytes(*replacement) && manager.loaded_lumps() == 0);
  return 0;
}
