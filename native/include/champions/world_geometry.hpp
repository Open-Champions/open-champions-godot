#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

namespace champions::presentation::rta::world_geometry {
struct Error : std::runtime_error {
  using std::runtime_error::runtime_error;
};
struct ResourceLimit : Error {
  using Error::Error;
};
struct Unsupported : Error {
  using Error::Error;
};
struct Limits {
  // Defaults are hard caps; callers may lower any cap, including to zero.
  std::size_t max_input_bytes = 64 * 1024 * 1024;
  std::size_t max_chunks = 65535;
  std::size_t max_packets = 65535;
  std::size_t max_batches = 262144;
  std::size_t max_vertices = 2000000;
  std::size_t max_strip_slots = 4000000;
  std::size_t max_transformed_vertices = 4000000;
  std::size_t max_triangles = 4000000;
  std::size_t max_commands = 4000000;
  std::size_t max_unpack_rows = 4000000;
  std::size_t max_render_triangles = 4000000;
};
using Vec3 = std::array<double, 3>;
struct StripSlot {
  std::uint32_t vertex = 0;
  bool nodraw = false;
};
struct Batch {
  std::uint32_t offset = 0;
  std::vector<std::array<std::int16_t, 3>> positions{};
  std::vector<std::array<std::int8_t, 3>> normals{};
  std::vector<std::array<std::uint16_t, 3>> vertex_locations{};
  std::vector<std::array<std::uint16_t, 4>> extra_copies{};
  std::vector<std::array<std::uint32_t, 4>> gif_tags{};
  std::vector<StripSlot> strip{};
  std::optional<std::vector<std::array<std::uint16_t, 2>>> uvs{};
  std::uint8_t nreg = 0;
  std::uint16_t prim = 0;
  std::uint16_t program = 0;
  // Same order as world_info.triangles; no winding correction. triangle_slots
  // indexes strip/UV corners, triangles indexes the source position/normal.
  std::vector<std::array<std::uint32_t, 3>> triangles{};
  std::vector<std::array<std::uint32_t, 3>> triangle_slots{};
};
struct Packet {
  std::uint32_t data_offset = 0, qwc = 0;
  std::vector<Batch> batches{};
};
struct Chunk {
  std::uint32_t index = 0, data_offset = 0, qwc = 0;
  std::array<std::uint32_t, 6> bounds_bits{};
  std::uint32_t texture_offset = 0, flags = 0;
  std::array<std::int32_t, 3> translation{};
  std::int16_t cell = 0;
  std::array<std::int16_t, 3> rotation{};
  std::optional<std::size_t> packet_index{};
  // Native right-handed Z-up, in world units; per packet batch, source order.
  // Presentation-only binary64. Never feed these values into simulation.
  std::vector<std::vector<Vec3>> world_vertices{};
};
struct Geometry {
  std::array<std::uint32_t, 33> header_words{};
  std::vector<Packet> packets{};
  std::vector<Chunk> chunks{};
  std::size_t omitted_models = 0, omitted_emitters = 0, omitted_lights = 0;
};
// Owned static mesh projection of an RTA WORLD. Water and moving pieces refuse
// atomically. Models/lights/emitters retain chunk metadata but are not
// rendered. Shared packets retain identity by (data_offset,qwc). No textures,
// lighting, culling or simulation behavior is implemented. Use neutral
// two-sided shading; cell==0 is the documented not-drawn state. Input remains
// unchanged.
Geometry decode(const std::vector<std::uint8_t> &bytes, Limits limits = {});
} // namespace champions::presentation::rta::world_geometry
