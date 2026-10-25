#include "champions/level_texture.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <tuple>
#include <unordered_set>

// Authored from the pinned level-texture and model-row-registers contracts.
// Only synthetic fixtures are part of this source tree.
namespace champions::presentation::rta::level_texture {
namespace {
void need(bool ok, const char *why) {
  if (!ok)
    throw Error(why);
}
struct Reader {
  const std::vector<std::uint8_t> &d;
  void span(std::uint64_t p, std::uint64_t n) const {
    need(p <= d.size() && n <= d.size() - p,
         "level texture range exceeds input");
  }
  std::uint8_t u8(std::uint64_t p) const {
    span(p, 1);
    return d[std::size_t(p)];
  }
  std::uint16_t u16(std::uint64_t p) const {
    return std::uint16_t(u8(p) | (unsigned(u8(p + 1)) << 8));
  }
  std::uint32_t u32(std::uint64_t p) const {
    return u16(p) | (std::uint32_t(u16(p + 2)) << 16);
  }
  std::int32_t i32(std::uint64_t p) const {
    const auto x = u32(p);
    return x <= 0x7fffffff ? std::int32_t(x)
                           : -1 - std::int32_t(0xffffffffu - x);
  }
  std::int8_t i8(std::uint64_t p) const {
    const auto x = u8(p);
    return x < 128 ? std::int8_t(x) : std::int8_t(int(x) - 256);
  }
};
constexpr std::uint8_t block32[4][8] = {{0, 1, 4, 5, 16, 17, 20, 21},
                                        {2, 3, 6, 7, 18, 19, 22, 23},
                                        {8, 9, 12, 13, 24, 25, 28, 29},
                                        {10, 11, 14, 15, 26, 27, 30, 31}};
std::uint32_t gs_block(std::uint32_t x, std::uint32_t y, std::uint32_t dbw) {
  return ((y / 4) * dbw + x / 8) * 32 + block32[y % 4][x % 8];
}
PaletteGroup read_group(const Reader &r, std::uint32_t p) {
  r.span(p, 0xc04);
  PaletteGroup g;
  for (unsigned i = 0; i < 256; ++i) {
    const unsigned clut = (i & ~24u) | ((i & 8u) << 1) | ((i & 16u) >> 1);
    for (unsigned c = 0; c < 4; ++c)
      g.rgba[i][c] = r.u8(p + 4 * clut + c);
  }
  for (unsigned i = 0; i < 2048; ++i)
    g.successors[i] = r.u8(p + 0x400 + i);
  const auto n = r.u32(p + 0xc00);
  // The disc contains 270-entry tables; entries are allowed to be unused.
  need(n <= 270, "level texture Huffman table exceeds documented disc bound");
  const std::uint64_t start = std::uint64_t(p) + 0xc04;
  r.span(start, std::uint64_t(n) * 2 + 18 * 4 + 17 * 4);
  g.symbols.reserve(n);
  for (unsigned i = 0; i < n; ++i) {
    auto s = r.u16(start + i * 2);
    need(s < 269, "level texture Huffman symbol exceeds alphabet");
    g.symbols.push_back(s);
  }
  const auto s_off = start + n * 2, t_off = s_off + 18 * 4;
  for (unsigned i = 0; i < 18; ++i)
    g.first_index_minus_code[i] = r.i32(s_off + i * 4);
  for (unsigned i = 0; i < 17; ++i)
    g.last_code[i] = r.i32(t_off + i * 4);
  std::int32_t last = -1;
  for (unsigned k = 1; k <= 16; ++k) {
    auto t = g.last_code[k];
    if (t < 0)
      continue;
    need(t < (1 << k) && t >= last,
         "level texture Huffman last code is not canonical");
    const std::int64_t index = std::int64_t(t) + g.first_index_minus_code[k];
    need(index >= 0 && index < n,
         "level texture Huffman code points outside symbols");
    last = t;
  }
  for (unsigned prefix = 0; prefix < 256; ++prefix) {
    for (unsigned k = 1; k <= 8; ++k) {
      const unsigned code = prefix >> (8 - k);
      if (g.last_code[k] < 0 || code > unsigned(g.last_code[k]))
        continue;
      const std::int64_t index =
          std::int64_t(code) + g.first_index_minus_code[k];
      need(index >= 0 && index < n,
           "level texture fast Huffman code points outside symbols");
      g.fast[prefix] = {g.symbols[std::size_t(index)], std::uint8_t(k)};
      break;
    }
  }
  return g;
}
struct BitReader {
  const Reader &r;
  std::uint32_t start;
  std::uint32_t bits = 0;
  std::uint16_t peek16() const {
    const std::uint64_t p =
        std::uint64_t(start) + (std::uint64_t(bits >> 4) * 2);
    r.span(p, 4);
    const std::uint32_t w = (std::uint32_t(r.u16(p)) << 16) | r.u16(p + 2);
    return std::uint16_t((w << (bits & 15)) >> 16);
  }
};
std::uint16_t symbol(BitReader &bits, const PaletteGroup &g) {
  const auto word = bits.peek16();
  const auto entry = g.fast[word >> 8];
  if (entry.length) {
    bits.bits += entry.length;
    return entry.symbol;
  }
  for (unsigned k = 9; k <= 16; ++k) {
    const unsigned code = unsigned(word) >> (16 - k);
    if (g.last_code[k] < 0 || code > unsigned(g.last_code[k]))
      continue;
    const std::int64_t i = std::int64_t(code) + g.first_index_minus_code[k];
    need(i >= 0 && i < std::int64_t(g.symbols.size()),
         "level texture Huffman index exceeds symbols");
    bits.bits += k;
    return g.symbols[std::size_t(i)];
  }
  throw Error("level texture has unmatched Huffman code");
}
} // namespace

std::vector<Entry> parse_directory(const std::vector<std::uint8_t> &bytes) {
  const Reader r{bytes};
  r.span(0, 4);
  std::vector<Entry> out;
  for (std::uint64_t p = 4;; p += 12) {
    need(p + 12 <= std::min<std::size_t>(bytes.size(), 0x5000),
         "level texture directory has no bounded terminator");
    const auto id = r.i32(p);
    if (id == -1 || id == -2 || (p == 4 && id == 0))
      return out;
    Entry e{id, r.u32(p + 4), r.u32(p + 8)};
    r.span(e.offset, e.size);
    out.push_back(e);
  }
}
std::vector<Entry> cell_table(const std::vector<Entry> &directory,
                              std::int32_t low, std::int32_t high) {
  need(high >= low && std::int64_t(high) - low < 40000,
       "invalid level texture cell range");
  std::vector<Entry> result(std::size_t(std::int64_t(high) - low + 1),
                            Entry{0, 0, 0x40});
  for (const auto &e : directory) {
    need(e.id >= low && e.id <= high, "level texture cell outside WORLD range");
    result[std::size_t(std::int64_t(e.id) - low)] = e;
  }
  return result;
}
std::uint64_t relocate_offset(std::uint32_t offset,
                              const std::vector<std::uint8_t> &map,
                              std::uint64_t base) {
  const auto piece = offset >> 17;
  need(piece < map.size() && map[piece] != 0xff,
       "level texture offset outside mapped pieces");
  return base + (std::uint64_t(map[piece]) << 17) + (offset & 0x1ffffu);
}
std::array<std::uint8_t, 256> block_permutation() {
  std::array<std::uint8_t, 256> out{};
  unsigned k = 0;
  for (unsigned r = 0; r < 8; ++r)
    for (unsigned c = 0; c < 8; ++c) {
      const unsigned base =
          ((r >> 1) & 3) * 64 + ((c >> 1) & 3) * 16 + (r & 1) * 8 + (c & 1) * 4;
      for (unsigned j = 0; j < 4; ++j) {
        const unsigned p = base + j, p2 = p >> 2;
        const unsigned e8 = ((p2 & 2) >> 1) | ((p & 1) << 1);
        const unsigned x = ((p2 & 1) | ((p2 & 0xc) >> 1) | ((p & 2) << 2)) ^
                           ((((p >> 6) & 1) ^ (e8 >> 1)) << 2);
        const unsigned y = (((p >> 6) & 3) << 2) | e8;
        out[k++] = std::uint8_t(x + 16 * y);
      }
    }
  return out;
}
std::array<std::uint8_t, 256>
decode_block(const std::vector<std::uint8_t> &cell, std::uint32_t offset,
             const PaletteGroup &group) {
  const Reader r{cell};
  BitReader bits{r, offset};
  std::array<std::uint8_t, 256> out{};
  std::uint8_t previous = 0;
  constexpr int deltas[5] = {-1, -16, -17, -15, -2};
  for (int i = 0; i < 256; ++i) {
    const auto s = symbol(bits, group);
    std::uint8_t value = 0;
    if (s < 256)
      value = std::uint8_t(s);
    else if (s < 0x105) {
      const auto source = i + deltas[s - 0x100];
      need(source >= 0, "level texture back-reference precedes block");
      value = out[std::size_t(source)];
    } else
      value = group.successors[unsigned(previous) * 8 + s - 0x105];
    out[std::size_t(i)] = previous = value;
  }
  return out;
}
std::array<std::uint8_t, 256>
upload_block(const std::array<std::uint8_t, 256> &texels) {
  std::array<std::uint8_t, 256> out{};
  const auto permutation = block_permutation();
  for (unsigned i = 0; i < 256; ++i)
    out[i] = texels[permutation[i]];
  return out;
}

Cell::Cell(std::vector<std::uint8_t> bytes) : bytes_(std::move(bytes)) {
  const Reader r{bytes_};
  r.span(0, 0x40);
  const auto count = r.u32(0);
  need(count <= 16000, "level texture cell count exceeds game limit");
  r.span(0x40, std::uint64_t(count) * 0x40);
  textures_.reserve(count);
  for (unsigned i = 0; i < count; ++i) {
    const std::uint64_t p = 0x40 + std::uint64_t(i) * 0x40;
    Header h{r.u16(p),
             r.u16(p + 2),
             r.u16(p + 4),
             r.u16(p + 6),
             r.u16(p + 8),
             r.u32(p + 0xc),
             r.u32(p + 0x10),
             0,
             0,
             {}};
    need(h.flags == 0x10c,
         "level texture form is undocumented (including 0x0001)");
    need(h.width && h.height && h.width <= 1024 && h.height <= 1024,
         "level texture dimensions are unsupported");
    need(h.file_qwords == 0 && r.u16(p + 0xa) == 0 && r.u32(p + 0x14) == 0 &&
             r.u16(p + 0x18) == 0 && r.u16(p + 0x1a) == 0 &&
             r.u32(p + 0x1c) == 0,
         "level texture file has nonzero relocation or runtime fields");
    for (unsigned j = 0x20; j < 0x40; ++j)
      need(r.u8(p + j) == 0, "level texture reserved header bytes are nonzero");
    const unsigned dbw = (h.width + 127) >> 7;
    const auto f = h.tex0;
    need((f & 63) == 2 * dbw && ((f >> 6) & 63) == 0x13 &&
             ((f >> 12) & 15) == 10 && ((f >> 16) & 15) == 10 &&
             ((f >> 20) & 1) == 1 && (f >> 21) == 0,
         "level texture TEX0 is not documented PSMT8/TBW layout");
    need(h.payload_offset % 4 == 0,
         "level texture payload offset is unaligned");
    r.span(h.payload_offset, 4);
    h.palette_offset = r.u32(h.payload_offset);
    r.span(h.palette_offset, 0xc04);
    if (groups_.find(h.palette_offset) == groups_.end())
      groups_.emplace(h.palette_offset, read_group(r, h.palette_offset));
    std::uint64_t q = std::uint64_t(h.payload_offset) + 4;
    std::uint32_t top = 0;
    bool wrote = false;
    unsigned qwords = 0x48;
    for (;;) {
      r.span(q, 4);
      const int x0 = r.i8(q), y0 = r.i8(q + 1), x1 = r.i8(q + 2),
                y1 = r.i8(q + 3);
      q += 4;
      if (x0 == -1)
        break;
      need(x0 >= 0 && y0 >= 0 && x1 >= x0 && y1 >= y0,
           "level texture rectangle is invalid");
      need(unsigned((x1 + 1) * 8) <= dbw * 64 &&
               (x1 + 1) * 16 <= ((h.width + 15) & ~15) &&
               (y1 + 1) * 16 <= ((h.height + 15) & ~15),
           "level texture rectangle exceeds surface");
      const unsigned blocks = unsigned((x1 - x0 + 1) * (y1 - y0 + 1));
      r.span(q, std::uint64_t(blocks) * 4);
      Rectangle rect{std::uint8_t(x0),
                     std::uint8_t(y0),
                     std::uint8_t(x1),
                     std::uint8_t(y1),
                     {}};
      rect.block_offsets.reserve(blocks);
      for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) {
          const auto off = r.u32(q);
          q += 4;
          need((off & 1) == 0, "level texture block stream offset is odd");
          r.span(off, 2);
          rect.block_offsets.push_back(off);
          top = std::max(top, gs_block(unsigned(x), unsigned(y), dbw));
          wrote = true;
        }
      h.rectangles.push_back(std::move(rect));
      qwords += 6 + blocks * 16;
    }
    h.relocated_qwords = std::uint16_t(qwords);
    need(h.allocation_blocks == (wrote ? 4 + top + 1 : 4),
         "level texture GS allocation block count disagrees with records");
    textures_.push_back(std::move(h));
  }
}
std::size_t Cell::index_at_header_offset(std::uint32_t offset) const {
  need(offset >= 0x40 && offset % 0x40 == 0,
       "WORLD chunk texture offset is not a header");
  const std::size_t i = offset / 0x40 - 1;
  need(i < textures_.size(), "WORLD chunk texture header exceeds cell");
  return i;
}
Image Cell::decode(std::size_t index) const {
  need(index < textures_.size(), "level texture index exceeds cell");
  const auto &h = textures_[index];
  const auto &group = groups_.at(h.palette_offset);
  Image image;
  image.width = h.width;
  image.height = h.height;
  const std::size_t pixels = std::size_t(h.width) * h.height;
  image.indices.assign(pixels, 0);
  image.covered.assign(pixels, 0);
  image.rgba.resize(pixels * 4);
  for (const auto &rect : h.rectangles) {
    unsigned n = 0;
    for (unsigned by = rect.y0; by <= rect.y1; ++by)
      for (unsigned bx = rect.x0; bx <= rect.x1; ++bx) {
        const auto block = decode_block(bytes_, rect.block_offsets[n++], group);
        for (unsigned y = 0; y < 16; ++y)
          for (unsigned x = 0; x < 16; ++x) {
            const unsigned px = bx * 16 + x, py = by * 16 + y;
            if (px >= h.width || py >= h.height)
              continue;
            const auto at = std::size_t(py) * h.width + px;
            image.indices[at] = block[y * 16 + x];
            image.covered[at] = 1;
          }
      }
  }
  for (std::size_t i = 0; i < pixels; ++i) {
    const auto &color = group.rgba[image.indices[i]];
    std::copy(color.begin(), color.end(), image.rgba.begin() + i * 4);
  }
  return image;
}
File::File(std::vector<std::uint8_t> bytes)
    : bytes_(std::move(bytes)), directory_(parse_directory(bytes_)) {}
Cell File::load_cell(std::int32_t id) const {
  const auto it = std::find_if(directory_.begin(), directory_.end(),
                               [id](const Entry &e) { return e.id == id; });
  need(it != directory_.end(), "WORLD texture cell has no directory record");
  return Cell(std::vector<std::uint8_t>(
      bytes_.begin() + it->offset, bytes_.begin() + it->offset + it->size));
}
namespace {
std::vector<Surface>
bind_filtered(const world_geometry::Geometry &world, const File &textures,
              const std::vector<std::uint8_t> &world_bytes,
              const std::unordered_set<std::int32_t> *only) {
  std::map<std::int32_t, Cell> cells;
  std::map<std::pair<std::int32_t, std::size_t>, std::shared_ptr<const Image>>
      images;
  std::vector<Surface> out;
  for (const auto &chunk : world.chunks) {
    if (!chunk.cell || !chunk.packet_index)
      continue;
    if (only != nullptr && only->find(std::int32_t(chunk.cell)) == only->end())
      continue;
    auto ci = cells.find(chunk.cell);
    if (ci == cells.end())
      ci = cells.emplace(chunk.cell, textures.load_cell(chunk.cell)).first;
    const auto ti = ci->second.index_at_header_offset(chunk.texture_offset);
    const auto key = std::make_pair(std::int32_t(chunk.cell), ti);
    auto image = images.find(key);
    if (image == images.end())
      image =
          images.emplace(key, std::make_shared<Image>(ci->second.decode(ti)))
              .first;
    const auto &packet = world.packets.at(*chunk.packet_index);
    const auto materials = packet_materials(world_bytes, packet, chunk.flags);
    for (std::size_t b = 0; b < packet.batches.size(); ++b) {
      const auto &batch = packet.batches[b];
      if (batch.triangles.empty())
        continue;
      need(batch.uvs.has_value(),
           "WORLD geometry batch has no documented UV data");
      Surface s{chunk.index, b, ti, chunk.cell, {}, {}, image->second, {}};
      s.material = materials.at(b);
      for (std::size_t t = 0; t < batch.triangles.size(); ++t)
        for (unsigned c = 0; c < 3; ++c) {
          const auto vertex = batch.triangles[t][c],
                     slot = batch.triangle_slots[t][c];
          need(vertex < chunk.world_vertices.at(b).size() &&
                   slot < batch.uvs->size(),
               "WORLD triangle exceeds positions or UVs");
          s.positions.push_back(chunk.world_vertices[b][vertex]);
          if (!batch.normals.empty()) {
            need(vertex < batch.normals.size(),
                 "WORLD triangle exceeds stored normals");
            s.stored_normals.push_back(batch.normals[vertex]);
          } else {
            need(!s.material.environment,
                 "environment batch is missing stored normals");
          }
          const auto &uv = batch.uvs->at(slot);
          s.uv.push_back({double(uv[0]) / (16.0 * s.image->width),
                          double(uv[1]) / (16.0 * s.image->height)});
        }
      if (chunk.flags & 1u)
        s.quarter = static_cast<std::uint8_t>(
            static_cast<std::uint16_t>(chunk.rotation[0]) & 7u);
      out.push_back(std::move(s));
    }
  }
  std::map<const Image *, std::int64_t> texture_ids;
  std::int64_t next_texture = 1;
  std::vector<std::tuple<int, bool, std::int64_t, std::uint32_t, std::size_t>>
      items;
  items.reserve(out.size());
  for (std::size_t i = 0; i < out.size(); ++i) {
    const auto inserted =
        texture_ids.emplace(out[i].image.get(), next_texture);
    if (inserted.second)
      ++next_texture;
    // World chunks are queued with packet flag 0 and a texture.
    items.emplace_back(int(out[i].material.sort_layer), true,
                       inserted.first->second, 0u, i);
  }
  const auto order = sorted_draw_order(items);
  std::vector<Surface> sorted;
  sorted.reserve(out.size());
  for (const auto &layer : order)
    for (const std::size_t index : layer) {
      out[index].draw_order = static_cast<int>(sorted.size());
      sorted.push_back(std::move(out[index]));
    }
  return sorted;
}
} // namespace
std::vector<Surface> bind_world(const world_geometry::Geometry &world,
                                const File &textures,
                                const std::vector<std::uint8_t> &world_bytes) {
  return bind_filtered(world, textures, world_bytes, nullptr);
}
double texture_cell_size(const world_geometry::Geometry &world) {
  const auto bits = world.header_words[0x68 / 4];
  float value = 0.f;
  std::memcpy(&value, &bits, sizeof value);
  if (value == 0.f)
    return 960.0;
  need(std::isfinite(value) && value > 0.f, "invalid WORLD texture cell size");
  return double(value);
}
int cell_id_of(double x, double y, double cell_size) {
  need(std::isfinite(x) && std::isfinite(y) && std::isfinite(cell_size) &&
           cell_size > 0.0,
       "invalid texture cell query");
  const float size = float(cell_size);
  const int column = static_cast<int>(std::floor(float(x) / size)) + 50;
  const int row = static_cast<int>(std::floor(float(y) / size)) + 50;
  return row * 100 + column;
}
std::array<int, 9> cells_around(int cell_id) {
  const int row = cell_id / 100;
  const int column = cell_id % 100;
  std::array<int, 9> out{};
  std::size_t n = 0;
  for (const int dr : {-1, 0, 1})
    for (const int dc : {-1, 0, 1})
      out[n++] = (row + dr) * 100 + column + dc;
  return out;
}
std::vector<DrawSlot> draw_slots(const world_geometry::Geometry &world,
                                 const std::vector<std::uint8_t> &world_bytes,
                                 const std::vector<std::int32_t> *cells) {
  std::unordered_set<std::int32_t> only;
  if (cells != nullptr)
    only.insert(cells->begin(), cells->end());
  std::vector<DrawSlot> slots;
  std::map<std::pair<std::int32_t, std::uint32_t>, std::int64_t> texture_ids;
  std::int64_t next_texture = 1;
  std::vector<std::tuple<int, bool, std::int64_t, std::uint32_t, std::size_t>>
      items;
  for (const auto &chunk : world.chunks) {
    if (!chunk.cell || !chunk.packet_index)
      continue;
    if (cells != nullptr && only.find(std::int32_t(chunk.cell)) == only.end())
      continue;
    const auto &packet = world.packets.at(*chunk.packet_index);
    const auto materials = packet_materials(world_bytes, packet, chunk.flags);
    const auto key = std::make_pair(std::int32_t(chunk.cell),
                                    std::uint32_t(chunk.texture_offset));
    for (std::size_t b = 0; b < packet.batches.size(); ++b) {
      if (packet.batches[b].triangles.empty())
        continue;
      const auto inserted = texture_ids.emplace(key, next_texture);
      if (inserted.second)
        ++next_texture;
      items.emplace_back(int(materials.at(b).sort_layer), true,
                         inserted.first->second, 0u, slots.size());
      slots.push_back({chunk.index, b, 0});
    }
  }
  const auto order = sorted_draw_order(items);
  int next = 0;
  for (const auto &layer : order)
    for (const std::size_t index : layer)
      slots[index].draw_order = next++;
  return slots;
}
std::vector<Surface> bind_cells(const world_geometry::Geometry &world,
                                const File &textures,
                                const std::vector<std::uint8_t> &world_bytes,
                                const std::vector<std::int32_t> &cells) {
  const std::unordered_set<std::int32_t> only(cells.begin(), cells.end());
  return bind_filtered(world, textures, world_bytes, &only);
}
std::array<std::int32_t, 4>
model_rows(ModelRows choice,
           const std::array<std::int32_t, 4> *preceding_chunk) {
  if (choice == ModelRows::DormantSetupZero)
    return {0, 0, 0, 0};
  need(choice == ModelRows::Inherited, "unknown model 0x46 row provenance");
  need(preceding_chunk != nullptr,
       "inherited model 0x46 rows require preceding chunk");
  return *preceding_chunk;
}
std::array<double, 3>
model_0x46_position(const std::array<std::int32_t, 3> &stored, ModelRows choice,
                    const std::array<std::int32_t, 4> *preceding_chunk) {
  const auto rows = model_rows(choice, preceding_chunk);
  std::array<double, 3> out{};
  for (unsigned c = 0; c < 3; ++c) {
    const std::uint32_t sum = std::uint32_t(stored[c]) + std::uint32_t(rows[c]);
    const std::int32_t signed_sum = sum <= 0x7fffffff
                                        ? std::int32_t(sum)
                                        : -1 - std::int32_t(0xffffffffu - sum);
    out[c] = double(signed_sum) / 16.0;
  }
  return out;
}
} // namespace champions::presentation::rta::level_texture
