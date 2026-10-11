#pragma once
#include "champions/world_queries.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace champions::simulation::rta::terrain_file {
struct Error : std::runtime_error {
  using std::runtime_error::runtime_error;
};
struct ResourceLimit : Error {
  using Error::Error;
};
struct ParseLimits {
  // Defaults are hard caps. Callers may lower any budget, including to zero.
  std::size_t max_input_bytes = 64 * 1024 * 1024;
  std::size_t max_chunks = 65535;
  std::size_t max_pieces = 10000;
  // Aggregate list reads, including the terminating signed -1 entries.
  std::size_t max_list_steps = 8388608;
  // Aggregate sample visits across all pieces, including shared/repeated
  // fields.
  std::size_t max_fixup_samples = 16777216;
  world_queries::Limits level_limits{};
};
// Explicit RTA unrelocated .world terrain projection, not a complete level
// loader or revision detector. Input is unchanged; output owns all post-fixup
// values. Preserves heightfield sharing by offset-sorted registry index, file
// list order, and piece-order fixups. Geometry/tree/texture/automap payloads
// remain outside the projection. Native safety policy rejects negative piece
// counts and missing offsets for nonempty terrain/path grids. All returned
// topology passes Level.
world_queries::LevelData decode(const std::vector<std::uint8_t> &bytes,
                                ParseLimits limits = {});
} // namespace champions::simulation::rta::terrain_file
