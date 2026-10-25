#pragma once
// Exact snapshot of one per-machine EffectWorld (fx_simulation.hpp): the particle
// pools and their resident definitions, trails, beams, the glow queue, decals and
// their growers, level light, light effects and linked nodes, the cosmetic and
// libc generators, fx_spawn's made flags and every counter. An authored native
// encoding (little endian, fixed widths, binary64/raw-float values as their bit
// patterns, no pointers): a resident definition slot is its definition address.
// The host's near_camera_focus callback is not state and is not saved.
//
// load_world decodes into a scratch world, requires the canonical re-encoding to
// match the input byte for byte, and only then overwrites the target: a refused
// load never changes the target.
#include "champions/fx_simulation.hpp"
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace champions::rta::fx::sim {
using SaveBytes = std::vector<std::uint8_t>;
inline constexpr std::uint32_t WORLD_SAVE_VERSION = 1;
struct WorldSaveError : std::runtime_error {
  using std::runtime_error::runtime_error;
};
SaveBytes save_world(const EffectWorld &world);
void load_world(EffectWorld &target, const SaveBytes &bytes);
} // namespace champions::rta::fx::sim
