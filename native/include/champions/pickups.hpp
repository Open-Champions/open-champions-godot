#pragma once
#include "champions/script_items.hpp"
#include "champions/combat_runtime.hpp"
#include "champions/arena_executable.hpp"
#include "champions/resource_manager.hpp"

// RTA US v2.00, research-loot-drop 77c2f70 and research-pickups 9911a6b.
// Runtime names/bytes are borrowed from the user's selected executable/disc.
namespace champions::simulation::rta::pickups {
void require_supported_chest(const std::vector<std::string> &props, bool trapped);
struct Grant {
  std::uint32_t descriptor;
  std::int32_t variant;
  std::optional<std::uint8_t> byte20;
  bool equip;
  unsigned copies;
  std::uint32_t announce;
};
std::vector<Grant> starting_grants(unsigned player_class);
void give_starting_items(script_items::Service &, unsigned slot, unsigned player_class,
                         const std::function<std::string(std::uint32_t)> &descriptor);
// The helper's success is deliberately independent of give_item's returned
// index: the original loses an accepted unique item when give returns -1.
std::vector<unsigned> take_item(script_items::Service &, builtins::Memory &,
    const items::Tables &, const std::array<std::uint8_t, 0x48> &, unsigned chosen);
// Exact signed mesh/LOD counts and signed-byte bounds, including the empty
// mask sentinel extrema. No render-geometry approximation.
std::array<Raw, 6> model_bounds(Bytes, std::uint64_t mask);

} // namespace champions::simulation::rta::pickups
