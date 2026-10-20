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
// Water chunk (flag 0x80) data, 0x90 bytes (docs/formats/world.md "Water
// chunks"; research t3/research-levels docs/level-flow.md section 12).
// Neighbours are chunk indices in the order -y, +y, -x, +x, -1 for none. The
// mask is two 64-byte halves (rows 0-15, 16-31); the u16 at byte 2*col of a
// half is column col (along y) and bit 15 - (row mod 16) is row (along x).
// runtime_grid is the +0x08 word (0 in files; the run-time ripple block).
struct WaterTile {
  std::uint32_t chunk = 0;
  std::array<std::int16_t, 4> neighbours{};
  std::uint32_t runtime_grid = 0;
  std::array<std::uint8_t, 128> mask{};
  bool second_set = false; // chunk flag 0x1000: second drawing-constant set
  bool cell(unsigned row, unsigned column) const;
};
// Moving-piece record, 0x1c bytes (docs/formats/world.md "Moving pieces").
struct MovingPiece {
  std::int32_t first_chunk = 0;
  std::int16_t count = 0;
  std::uint16_t runtime_flags = 0;
  std::array<std::int32_t, 3> rest_translation{};
  std::int16_t rest_cosine = 0, rest_sine = 0, element = 0, unknown_1a = 0;
};
struct Geometry {
  std::array<std::uint32_t, 33> header_words{};
  std::vector<Packet> packets{};
  std::vector<Chunk> chunks{};
  std::size_t omitted_models = 0, omitted_emitters = 0, omitted_lights = 0;
  std::vector<WaterTile> water{};
  std::vector<MovingPiece> pieces{};
};
// Which documented non-static parts decode. The default accepts water tiles
// and moving pieces. STATIC_ONLY is the original rta-world-geometry-v1 static
// presentation policy: either refuses atomically with Unsupported.
struct Policy {
  bool water = true;
  bool moving_pieces = true;
};
inline constexpr Policy STATIC_ONLY{false, false};
// Owned mesh projection of an RTA WORLD. Water tiles keep their payload but
// produce no packet (their surface is generated at run time). Moving-piece
// chunks (flag 0x20) decode as geometry at their file (rest) transform; the
// piece table is kept for level flow. Models/lights/emitters retain chunk
// metadata but are not rendered. Shared packets retain identity by
// (data_offset,qwc). No textures, lighting, culling or simulation behavior is
// implemented. Use neutral two-sided shading; cell==0 is the documented
// not-drawn state. Input remains unchanged.
Geometry decode(const std::vector<std::uint8_t> &bytes, Limits limits = {},
                Policy policy = {});
// The documented 2x2 xy matrix {xx, xy, yx, yy} of a chunk: quarter-turn code
// for flag 1, else cosine/sine x 32767, mirrored for flag 0x40. Presentation
// binary64 only.
std::array<double, 4> chunk_matrix(std::uint32_t flags,
                                   const std::array<std::int16_t, 3> &rotation);
} // namespace champions::presentation::rta::world_geometry
