#pragma once
#include "champions/world_geometry.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <stdexcept>
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
};
// Base texture layer only. Triangle and UV order matches WORLD geometry.
// The caller owns presentation, blending, lights and scene publication.
std::vector<Surface> bind_world(const world_geometry::Geometry &world,
                                const File &textures);

enum class ModelRows { DormantSetupZero, Inherited };
std::array<std::int32_t, 4>
model_rows(ModelRows choice,
           const std::array<std::int32_t, 4> *preceding_chunk);
std::array<double, 3>
model_0x46_position(const std::array<std::int32_t, 3> &stored, ModelRows choice,
                    const std::array<std::int32_t, 4> *preceding_chunk);
} // namespace champions::presentation::rta::level_texture
