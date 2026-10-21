#pragma once
#include "champions/world_geometry.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace champions::presentation::rta::level_texture {
struct Error : std::runtime_error {
  using std::runtime_error::runtime_error;
};
struct Entry {
  std::int32_t id;
  std::uint32_t offset, size;
};
struct Rectangle {
  std::uint8_t x0, y0, x1, y1;
  std::vector<std::uint32_t> block_offsets;
};
struct Header {
  std::uint16_t width, height, allocation_blocks, file_qwords, flags;
  std::uint32_t tex0, payload_offset, palette_offset;
  std::uint16_t relocated_qwords;
  std::vector<Rectangle> rectangles;
};
struct PaletteGroup {
  struct FastSymbol {
    std::uint16_t symbol = 0;
    std::uint8_t length = 0;
  };
  std::array<std::array<std::uint8_t, 4>, 256> rgba{};
  std::array<std::uint8_t, 2048> successors{};
  std::vector<std::uint16_t> symbols;
  std::array<std::int32_t, 18> first_index_minus_code{};
  std::array<std::int32_t, 17> last_code{};
  std::array<FastSymbol, 256> fast{};
};
struct Image {
  std::uint16_t width = 0, height = 0;
  std::vector<std::uint8_t> indices, covered, rgba;
};
class Cell {
public:
  explicit Cell(std::vector<std::uint8_t> bytes);
  const std::vector<Header> &textures() const { return textures_; }
  const std::map<std::uint32_t, PaletteGroup> &palette_groups() const {
    return groups_;
  }
  Image decode(std::size_t index) const;
  // Header offsets in a WORLD chunk refer to the 0x40-byte header table.
  std::size_t index_at_header_offset(std::uint32_t offset) const;

private:
  std::vector<std::uint8_t> bytes_;
  std::vector<Header> textures_;
  std::map<std::uint32_t, PaletteGroup> groups_;
};
class File {
public:
  explicit File(std::vector<std::uint8_t> bytes);
  const std::vector<Entry> &directory() const { return directory_; }
  Cell load_cell(std::int32_t id) const;

private:
  std::vector<std::uint8_t> bytes_;
  std::vector<Entry> directory_;
};
std::vector<Entry> parse_directory(const std::vector<std::uint8_t> &bytes);
std::vector<Entry> cell_table(const std::vector<Entry> &directory,
                              std::int32_t low, std::int32_t high);
std::uint64_t relocate_offset(std::uint32_t offset,
                              const std::vector<std::uint8_t> &map,
                              std::uint64_t base);
std::array<std::uint8_t, 256> block_permutation();
std::array<std::uint8_t, 256>
decode_block(const std::vector<std::uint8_t> &cell, std::uint32_t offset,
             const PaletteGroup &group);
std::array<std::uint8_t, 256>
upload_block(const std::array<std::uint8_t, 256> &texels);

struct Surface {
  std::size_t chunk, batch, texture;
  std::int32_t cell;
  std::vector<world_geometry::Vec3> positions;
  std::vector<std::array<double, 2>> uv;
  std::shared_ptr<const Image> image;
  struct Material {
    std::uint8_t alpha1 = 2;
    bool alpha2_set = false;
    std::uint8_t alpha2 = 0;
    std::uint32_t test1 = 0x587f4;
    bool blend_enabled = true;
    std::int16_t light_constant = 0;
    bool dynamic_light = true;
    std::uint8_t sort_layer = 0;
    bool environment = false;
    // Chunk flags after the 0x100 flash bit clears itself.
    std::uint32_t flags_after = 0;
    bool cutout() const { return test1 == 0x582f5; }
  } material;
  // Stored vertex normals (127 = 1). Empty when the batch has none.
  std::vector<std::array<std::int8_t, 3>> stored_normals;
  // Low 3 bits of chunk +0x36 when flag 0x1 is set.
  std::optional<std::uint8_t> quarter;
  // Console send index: layer 0, then 1, … then 7, texture groups inside a layer.
  int draw_order = 0;
};
// The source WORLD bytes supply packet DIRECT state. Triangle and UV order
// matches WORLD geometry; environment and point-light inputs remain caller-owned.
std::vector<Surface> bind_world(const world_geometry::Geometry &world,
                                const File &textures,
                                const std::vector<std::uint8_t> &world_bytes = {});
// Texture cells and streaming (research docs/level-flow.md 13.1 and
// docs/formats/world.md; per machine, drawing only).
// World header +0x68. A zero word means 960 world units.
double texture_cell_size(const world_geometry::Geometry &world);
// column = floorf(x / size) + 50, row = floorf(y / size) + 50 in float,
// id = row * 100 + column (0x17d830).
int cell_id_of(double x, double y, double cell_size);
// The 3 x 3 cells kept around the camera focus. The original's slot order is
// not documented; these are rows -1..+1, each with columns -1..+1.
std::array<int, 9> cells_around(int cell_id);
// bind_world restricted to the given texture cells. Draw order is recomputed
// on that subset. An empty cell list binds nothing.
std::vector<Surface> bind_cells(const world_geometry::Geometry &world,
                                const File &textures,
                                const std::vector<std::uint8_t> &world_bytes,
                                const std::vector<std::int32_t> &cells);
// The draw order bind_cells (or bind_world, when cells is null) assigns, from
// geometry alone: no cell is read or decoded. A texture is identified by its
// cell and header offset, as bind_world identifies one decoded image.
struct DrawSlot {
  std::size_t chunk, batch;
  int draw_order;
};
std::vector<DrawSlot> draw_slots(const world_geometry::Geometry &world,
                                 const std::vector<std::uint8_t> &world_bytes,
                                 const std::vector<std::int32_t> *cells);
Surface::Material chunk_header_material(std::uint32_t flags);
std::vector<Surface::Material> packet_materials(
    const std::vector<std::uint8_t> &world_bytes,
    const world_geometry::Packet &packet, std::uint32_t chunk_flags);

enum class ModelRows { DormantSetupZero, Inherited };
std::array<std::int32_t, 4>
model_rows(ModelRows choice,
           const std::array<std::int32_t, 4> *preceding_chunk);
std::array<double, 3>
model_0x46_position(const std::array<std::int32_t, 3> &stored, ModelRows choice,
                    const std::array<std::int32_t, 4> *preceding_chunk);

// Geometry-chunk GS state, light path, environment rows and automap tiles.
// Flags 0x80, 0x800, 0x4000 and 0x8000 are other drawers and are refused.
// dome.vif is unreferenced on the retail disc, so model form 0x46 stays the
// explicit ModelRows choice above.
struct ChunkGs {
  std::uint8_t alpha1 = 2;
  bool alpha2_set = false;
  std::uint8_t alpha2 = 0;
  std::uint32_t test = 0x587f4;
  int layer = 0;
  bool env = false;
};
enum class ChunkLightKind { Flash, Full, Flat, Dynamic };
struct ChunkLight {
  ChunkLightKind kind = ChunkLightKind::Dynamic;
  std::optional<int> constant;
  std::uint32_t flags_after = 0;
};
ChunkGs chunk_gs(std::uint32_t flags);
ChunkLight chunk_light(std::uint32_t flags);
// One channel of the constant term that replaces the dynamic call.
// Non-negative sums clamp at 255. A negative sum wraps as (2 * sum) mod 256,
// which is 52, 180 and 116 at bases 128, 64 and 32 for the −230 flash.
int world_constant_channel(int base, int added);
std::array<double, 6> axis_xy(const std::array<double, 9> &camera);
std::array<double, 6> quarter_turn(const std::array<double, 6> &axis, int code);
// quarter is disengaged when flag 0x1 is clear. Scroll uses level option bit 2.
std::pair<std::array<double, 4>, std::array<double, 4>>
env_rows(const std::array<double, 6> &axis, std::uint32_t frame,
         std::uint32_t level_options, const std::optional<int> &quarter);
// (u, v) in texels. normal is the stored vertex normal; (sx, sy) is the
// pixel offset from the buffer centre.
std::array<double, 2> env_uv(const std::array<double, 3> &normal,
                             const std::array<double, 4> &row4,
                             const std::array<double, 4> &row5, double sx,
                             double sy);
struct AutomapFields {
  int col = 0, orient = 0, row = 0, explored = 0, alpha = 0;
};
struct AutomapCorner {
  int u = 0, v = 0, alpha = 0;
};
AutomapFields automap_fields(std::uint16_t cell);
std::array<int, 4> automap_uv_rect(int col, int row);
std::array<AutomapCorner, 4> automap_quad(std::uint16_t cell_c_r,
                                          std::uint16_t cell_c_r1,
                                          std::uint16_t cell_c1_r,
                                          std::uint16_t cell_c1_r1);
// File grid at the header +0x80 offset: s16 origin x, y, width, height,
// then width × height u16 cells, row-major. The HUD window that selects a
// rectangle of this grid is not part of the file.
struct AutomapGrid {
  std::int16_t origin_x = 0, origin_y = 0, width = 0, height = 0;
  std::vector<std::uint16_t> cells;
};
AutomapGrid read_automap_grid(const std::vector<std::uint8_t> &world);
// No-parent packets. texture_present false, or flags bit 0, inserts at the
// front. Otherwise the packet follows the previous one of the same texture.
// Returns layer 0..7 in send order.
template <typename T>
std::array<std::vector<T>, 8> sorted_draw_order(
    const std::vector<std::tuple<int, bool, std::int64_t, std::uint32_t, T>>
        &items) {
  std::array<std::vector<T>, 8> layers;
  std::map<std::pair<int, std::int64_t>, T> latest;
  for (const auto &item : items) {
    const int layer = std::get<0>(item);
    const bool textured = std::get<1>(item);
    const auto texture = std::get<2>(item);
    const auto flags = std::get<3>(item);
    const T &payload = std::get<4>(item);
    if (layer < 0 || layer > 7)
      throw Error("draw layer is outside 0..7");
    auto &row = layers[static_cast<std::size_t>(layer)];
    if (!textured || (flags & 1u)) {
      row.insert(row.begin(), payload);
    } else {
      const auto prev = latest.find({layer, texture});
      if (prev == latest.end()) {
        row.insert(row.begin(), payload);
      } else {
        const auto at = std::find(row.begin(), row.end(), prev->second);
        if (at == row.end())
          throw Error("draw order lost a texture group");
        row.insert(at + 1, payload);
      }
    }
    if (textured)
      latest[{layer, texture}] = payload;
  }
  return layers;
}
} // namespace champions::presentation::rta::level_texture
