#pragma once
// Optional: include after integrating codex/creature-water-2. Does not copy or
// reconstruct water masks, trees, grids, or Memory. Service lifetime is
// caller-owned.
#include "champions/creature_water.hpp"
namespace champions::simulation::rta::level_flow {
struct WaterAdapter {
  creature_water::Service &service;
  creature_water::Address world;
  std::uint32_t test(Raw radius, const creature_water::Vec3 &position,
                     creature_water::Address height_out = 0) {
    return service.water_test(radius, world, position, height_out);
  }
  std::uint32_t disturb(Raw strength, Raw radius,
                        const creature_water::Vec3 &position) {
    return service.water_disturb(strength, radius, world, position);
  }
};
} // namespace champions::simulation::rta::level_flow
