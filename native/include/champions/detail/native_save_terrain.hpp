#pragma once
// The OCNSAVE terrain encoding (docs/NATIVE_SAVE.md "Supplied initial terrain
// encoding", seven limits then the complete LevelData), shared with the fixture
// continuation (play_fixture_save.hpp). The existing formats are unchanged.
#include "champions/world_queries.hpp"
#include <cstdint>
#include <vector>

namespace champions::simulation::rta::native_save::terrain_codec {
// Appends limits then terrain to `out`.
void encode_terrain(std::vector<std::uint8_t> &out, const world_queries::Snapshot &terrain);
// Reads them from `bytes` at `at` (advanced), validated as native_save loads do.
world_queries::Snapshot decode_terrain(const std::vector<std::uint8_t> &bytes, std::size_t &at);
} // namespace champions::simulation::rta::native_save::terrain_codec
