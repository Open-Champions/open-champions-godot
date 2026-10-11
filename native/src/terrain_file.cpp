// Authored terrain projection from world_info.py at
// 2f937fca823488bc97b9680fde9d957c3fa9907a, SHA256
// 2fa037872b8b5619a65ced370b5744a63219e64d4d32c1ed33795dd8ecc249a5,
// and ref_world.py at 6831458ee7ee12bb166d1dce4eb8e4340095bc48,
// SHA256 30366cb77918630241cd036b20df6cd86c4869be453c4160bd20c99b786ea4b8.
// https://github.com/Open-Champions/champions-return-to-arms-re
// No original assets or executable instructions are included.
#include "champions/terrain_file.hpp"
#include <algorithm>
#include <utility>

namespace champions::simulation::rta::terrain_file {
namespace {
using Offset = std::uint64_t;
constexpr Offset HEADER = 0x84;
void require(bool condition, const char *message) {
  if (!condition)
    throw Error(message);
}
void budget(Offset amount, std::size_t limit, const char *message) {
  if (amount > limit)
    throw ResourceLimit(message);
}
void consume(std::size_t amount, std::size_t &left, const char *message) {
  budget(amount, left, message);
  left -= amount;
}
class Reader {
  const std::vector<std::uint8_t> &data_;

public:
  explicit Reader(const std::vector<std::uint8_t> &data) : data_(data) {}
  void span(Offset offset, Offset count) const {
    require(offset <= data_.size() && count <= data_.size() - offset,
            "terrain file range exceeds input");
  }
  std::uint16_t u16(Offset offset) const {
    span(offset, 2);
    const auto i = std::size_t(offset);
    return std::uint16_t(std::uint32_t(data_[i]) |
                         (std::uint32_t(data_[i + 1]) << 8));
  }
  std::uint32_t u32(Offset offset) const {
    span(offset, 4);
    const auto i = std::size_t(offset);
    return std::uint32_t(data_[i]) | (std::uint32_t(data_[i + 1]) << 8) |
           (std::uint32_t(data_[i + 2]) << 16) |
           (std::uint32_t(data_[i + 3]) << 24);
  }
  std::int32_t i16(Offset offset) const { return s16(u16(offset)); }
  std::int32_t i32(Offset offset) const { return s32(u32(offset)); }
};
bool finite(std::uint32_t value) {
  return (value & 0x7f800000u) != 0x7f800000u;
}
void limits_valid(const ParseLimits &limits) {
  const ParseLimits hard;
  budget(limits.max_input_bytes, hard.max_input_bytes,
         "input limit exceeds hard cap");
  budget(limits.max_chunks, hard.max_chunks, "chunk limit exceeds hard cap");
  budget(limits.max_pieces, hard.max_pieces, "piece limit exceeds hard cap");
  budget(limits.max_list_steps, hard.max_list_steps,
         "list work limit exceeds hard cap");
  budget(limits.max_fixup_samples, hard.max_fixup_samples,
         "piece work limit exceeds hard cap");
  // Validate the supplied Level limits before any file-driven allocation.
  (void)world_queries::Level({}, limits.level_limits);
}
world_queries::LevelData parse(const std::vector<std::uint8_t> &bytes,
                               const ParseLimits &limits) {
  limits_valid(limits);
  budget(bytes.size(), limits.max_input_bytes, "terrain input budget exceeded");
  Reader r(bytes);
  r.span(0, HEADER);
  const auto chunks = r.u32(0), tree = r.u32(4);
  const auto terrain_offset = r.u32(0x18), elements = r.u32(0x1c);
  const auto element_offset = r.u32(0x20), chunk_offset = r.u32(0x24);
  const auto path_offset = r.u32(0x38), bit_offset = r.u32(0x4c);
  const auto pieces = r.u32(0x50), piece_offset = r.u32(0x54);
  const auto tail_offset = r.u32(0x78);
  world_queries::LevelData out;
  out.tg_x0 = r.i32(8);
  out.tg_y0 = r.i32(0xc);
  out.tg_w = r.u32(0x10);
  out.tg_h = r.u32(0x14);
  out.pg_x0 = r.i32(0x28);
  out.pg_y0 = r.i32(0x2c);
  out.pg_w = r.u32(0x30);
  out.pg_h = r.u32(0x34);
  require(chunks > 0 && chunks <= 65535, "invalid terrain chunk count");
  budget(chunks, limits.max_chunks, "terrain chunk budget exceeded");
  require(chunk_offset >= HEADER, "chunk table overlaps file header");
  r.span(chunk_offset, Offset(chunks) * 0x3c);
  if (tree) {
    require(tree >= HEADER && tree < bytes.size() - 4,
            "invalid terrain tree offset");
    require(r.u16(tree) == 0x4567, "invalid terrain tree marker");
  }
  require(out.tg_w <= 4096 && out.tg_h <= 4096,
          "invalid terrain grid dimensions");
  require(elements <= 1000000, "invalid terrain element count");
  budget(elements, limits.level_limits.elements,
         "terrain element budget exceeded");
  if (elements)
    r.span(element_offset, Offset(elements) * 0x24);
  const auto grid_count = Offset(out.tg_w) * Offset(out.tg_h);
  budget(grid_count, limits.level_limits.grid_cells,
         "terrain grid budget exceeded");
  if (terrain_offset) {
    r.span(terrain_offset, grid_count * 4);
    require(tail_offset >= HEADER && tail_offset < bytes.size(),
            "invalid terrain list block offset");
  } else {
    require(grid_count == 0, "nonempty terrain grid has no table");
  }
  require(out.pg_w <= 8192 && out.pg_h <= 8192, "invalid path grid dimensions");
  const auto path_count = Offset(out.pg_w) * Offset(out.pg_h);
  budget(path_count, limits.level_limits.path_cells,
         "path grid budget exceeded");
  if (path_offset)
    r.span(path_offset, path_count * 4);
  else
    require(path_count == 0, "nonempty path grid has no table");
  if (bit_offset) {
    const auto w = r.u32(0x44), h = r.u32(0x48);
    require(w <= 65536 && h <= 65536, "invalid bit grid dimensions");
    r.span(bit_offset, (Offset(w) * h + 7) / 8);
  }
  require(pieces <= 10000, "invalid moving piece count");
  budget(pieces, limits.max_pieces, "moving piece budget exceeded");
  auto references_left = limits.level_limits.references;
  consume(pieces, references_left, "terrain reference budget exceeded");
  if (pieces)
    r.span(piece_offset, Offset(pieces) * 0x1c);
  const auto cell_min = std::int64_t(r.i32(0x58));
  const auto cell_max = std::int64_t(r.i32(0x5c));
  require(cell_min <= cell_max && cell_max - cell_min < 20000,
          "invalid texture cell range");
  r.span(r.u32(0x64), Offset(cell_max - cell_min + 1) * 8);
  const auto cell_size = r.u32(0x68);
  require(finite(cell_size) &&
              (!(cell_size & 0x80000000u) || (cell_size & 0x7fffffffu) == 0),
          "invalid texture cell size");
  require(r.u32(0x6c) < bytes.size() && r.u32(0x80) < bytes.size(),
          "invalid automap offset");
  for (Offset i = 0; i < chunks; ++i) {
    const auto offset = Offset(chunk_offset) + i * 0x3c;
    require(r.u32(offset) < bytes.size(), "invalid chunk data offset");
    for (Offset j = 0; j < 6; ++j)
      require(finite(r.u32(offset + 8 + j * 4)), "nonfinite chunk bounds");
  }

  // Source offsets define sharing; equal contents at separate offsets are not
  // merged. Keep the registry in the reference's sorted-offset order.
  std::vector<std::uint32_t> offsets;
  offsets.reserve(elements);
  for (Offset i = 0; i < elements; ++i) {
    const auto hf = r.u32(Offset(element_offset) + i * 0x24 + 0xc);
    require(hf >= HEADER, "heightfield overlaps file header");
    r.span(hf, 0x14);
    offsets.push_back(hf);
  }
  std::sort(offsets.begin(), offsets.end());
  offsets.erase(std::unique(offsets.begin(), offsets.end()), offsets.end());
  budget(offsets.size(), limits.level_limits.heightfields,
         "heightfield budget exceeded");
  out.heightfields.reserve(offsets.size());
  auto samples_left = limits.level_limits.samples;
  for (const auto offset32 : offsets) {
    const Offset offset = offset32;
    world_queries::HeightField hf;
    hf.offset = offset32;
    hf.x0 = r.i32(offset);
    hf.y0 = r.i32(offset + 4);
    hf.w = r.u32(offset + 8);
    hf.h = r.u32(offset + 0xc);
    hf.lo = r.i16(offset + 0x10);
    hf.hi = r.i16(offset + 0x12);
    require(hf.w > 0 && hf.w <= 4096 && hf.h > 0 && hf.h <= 4096,
            "invalid heightfield dimensions");
    const auto count = Offset(hf.w) * Offset(hf.h);
    budget(count, samples_left, "heightfield sample budget exceeded");
    samples_left -= std::size_t(count);
    r.span(offset + 0x14, count * 2);
    hf.samples.reserve(std::size_t(count));
    for (Offset i = 0; i < count; ++i)
      hf.samples.push_back(std::int16_t(r.i16(offset + 0x14 + i * 2)));
    out.heightfields.push_back(std::move(hf));
  }
  out.elements.reserve(elements);
  for (Offset i = 0; i < elements; ++i) {
    const auto o = Offset(element_offset) + i * 0x24;
    const auto hf = r.u32(o + 0xc);
    const auto index =
        std::lower_bound(offsets.begin(), offsets.end(), hf) - offsets.begin();
    out.elements.push_back(world_queries::make_element(
        std::int64_t(i), r.i16(o), r.i16(o + 2), r.i16(o + 4), r.i16(o + 6),
        std::int64_t(index), r.u32(o + 0x10) & 0xffffu, r.i32(o + 0x14),
        r.i32(o + 0x18), r.i32(o + 0x1c), r.i16(o + 0x20), r.i16(o + 0x22)));
  }
  out.grid.reserve(std::size_t(grid_count));
  auto steps_left = limits.max_list_steps;
  for (Offset i = 0; i < grid_count; ++i) {
    auto cursor = Offset(tail_offset) + r.u32(Offset(terrain_offset) + i * 4);
    std::vector<std::int64_t> cell;
    for (;;) {
      consume(1, steps_left, "terrain list work budget exceeded");
      const auto entry = r.i16(cursor);
      if (entry == -1)
        break;
      require(entry >= 0 && std::uint32_t(entry) < elements,
              "terrain list element is out of range");
      consume(1, references_left, "terrain reference budget exceeded");
      cell.push_back(entry);
      cursor += 2;
    }
    out.grid.push_back(std::move(cell));
  }
  out.path_cells.reserve(std::size_t(path_count));
  for (Offset i = 0; i < path_count; ++i)
    out.path_cells.push_back(r.u32(Offset(path_offset) + i * 4));
  out.pieces.reserve(pieces);
  for (Offset i = 0; i < pieces; ++i) {
    const auto o = Offset(piece_offset) + i * 0x1c;
    const auto first = std::int64_t(r.i32(o));
    const auto count = std::int64_t(r.i16(o + 4));
    const auto element = r.i16(o + 0x18);
    require(first >= 0 && count >= 0 && first + count <= chunks &&
                element >= 0 && std::uint32_t(element) < elements,
            "moving piece refers outside tables or has negative count");
    out.pieces.push_back(element);
  }
  auto fixups_left = limits.max_fixup_samples;
  for (const auto element : out.pieces) {
    auto &hf =
        out.heightfields[std::size_t(out.elements[std::size_t(element)].hf)];
    consume(hf.samples.size(), fixups_left,
            "piece sample work budget exceeded");
    for (auto &value : hf.samples) {
      const auto bits = std::uint16_t(value);
      if (bits & 1)
        value = std::int16_t((bits & 15) + 0x7d00);
    }
    hf.hi = 2000;
  }
  // The constructor checks every output reference and aggregate storage bound.
  // No partially parsed or unvalidated LevelData escapes on any failure.
  (void)world_queries::Level(out, limits.level_limits);
  return out;
}
} // namespace
world_queries::LevelData decode(const std::vector<std::uint8_t> &bytes,
                                ParseLimits limits) {
  try {
    return parse(bytes, limits);
  } catch (const world_queries::ResourceLimit &e) {
    throw ResourceLimit(e.what());
  } catch (const world_queries::Error &e) {
    throw Error(e.what());
  }
}
} // namespace champions::simulation::rta::terrain_file
