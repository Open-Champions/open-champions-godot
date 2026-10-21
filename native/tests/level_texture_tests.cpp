#include "champions/level_texture.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
using namespace champions::presentation::rta::level_texture;
namespace {
int checks = 0;
void check(bool ok, const char *why) {
  ++checks;
  if (!ok)
    throw std::runtime_error(why);
}
template <typename F> void rejects(F f, const char *why) {
  bool thrown = false;
  try {
    f();
  } catch (const Error &) {
    thrown = true;
  }
  check(thrown, why);
}
std::vector<std::uint8_t> read(const char *name) {
  std::ifstream in(name, std::ios::binary);
  check(bool(in), "synthetic fixture missing");
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
std::uint64_t fnv(const std::vector<std::uint8_t> &bytes) {
  std::uint64_t h = 14695981039346656037ull;
  for (auto b : bytes)
    h = (h ^ b) * 1099511628211ull;
  return h;
}
unsigned gs_addr32(unsigned x, unsigned y) {
  static constexpr unsigned blocks[4][8] = {{0, 1, 4, 5, 16, 17, 20, 21},
                                            {2, 3, 6, 7, 18, 19, 22, 23},
                                            {8, 9, 12, 13, 24, 25, 28, 29},
                                            {10, 11, 14, 15, 26, 27, 30, 31}};
  static constexpr unsigned words[8] = {0, 1, 4, 5, 8, 9, 12, 13};
  return (blocks[(y >> 3) & 3][(x >> 3) & 7] * 64 + ((y & 7) >> 1) * 16 +
          (y & 1) * 2 + words[x & 7]) *
         4;
}
unsigned gs_addr8(unsigned x, unsigned y) {
  static constexpr unsigned words[8] = {0, 1, 4, 5, 8, 9, 12, 13};
  const unsigned yy = y & 15, row = yy & 3;
  const unsigned xi = (x & 7) ^ (((((yy + 2) >> 2) & 1) != 0) ? 4u : 0u);
  const unsigned word = (yy >> 2) * 16 + (row & 1) * 2 + words[xi];
  return word * 4 + (row >> 1) + (((x >> 3) & 1) << 1);
}
void write16(std::vector<std::uint8_t> &v, std::size_t at, std::uint16_t n) {
  v.at(at) = std::uint8_t(n);
  v.at(at + 1) = std::uint8_t(n >> 8);
}
void write32(std::vector<std::uint8_t> &v, std::size_t at, std::uint32_t n) {
  for (unsigned i = 0; i < 4; ++i)
    v.at(at + i) = std::uint8_t(n >> (i * 8));
}
void write64(std::vector<std::uint8_t> &v, std::size_t at, std::uint64_t n) {
  write32(v, at, std::uint32_t(n));
  write32(v, at + 4, std::uint32_t(n >> 32));
}
} // namespace
int main() {
  try {
    const auto bytes = read(LEVEL_TEXTURE_FIXTURE);
    File file(bytes);
    const auto &dir = file.directory();
    check(dir.size() == 2 && dir[0].id == 5049 && dir[1].id == 5151,
          "directory IDs");
    check(dir[0].offset == 2048 && dir[0].size == 9032, "directory extents");
    auto table = cell_table(dir, 4949, 5151);
    check(table.size() == 203 && table[0].size == 0x40 &&
              table[100].offset == 2048,
          "WORLD cell table");
    rejects([&] { cell_table(dir, 5100, 5151); }, "out-of-range WORLD cell");
    auto cell = file.load_cell(5049);
    check(cell.textures().size() == 2 && cell.palette_groups().size() == 1,
          "shared palette group");
    check(cell.index_at_header_offset(0x40) == 0 &&
              cell.index_at_header_offset(0x80) == 1,
          "chunk header offsets");
    rejects([&] { cell.index_at_header_offset(0x41); },
            "unaligned chunk header");
    const auto &a = cell.textures()[0], &b = cell.textures()[1];
    check(a.width == 40 && a.height == 20 && a.allocation_blocks == 9 &&
              a.relocated_qwords == 164,
          "first header relocation");
    check(b.width == 140 && b.height == 18 && b.allocation_blocks == 39 &&
              b.relocated_qwords == 222,
          "second header relocation");
    check(a.rectangles.size() == 2 &&
              a.rectangles[0].block_offsets.size() == 4 &&
              b.rectangles[0].block_offsets.size() == 9,
          "rectangle blocks");
    auto first = cell.decode(0), second = cell.decode(1);
    check(fnv(first.indices) == 0xe4da05fbde40ae85ull,
          "first indices match reference");
    check(fnv(first.covered) == 0xab74d88bf10e7ca5ull,
          "first coverage matches reference");
    check(fnv(first.rgba) == 0xb5aa79dfa7549ee5ull,
          "first RGBA matches reference");
    check(fnv(second.indices) == 0x10a13f5af26f78a9ull,
          "second indices match reference");
    check(fnv(second.covered) == 0xc708eee3c62ef29dull,
          "second coverage matches reference");
    check(fnv(second.rgba) == 0xf4ffe2f922c5b751ull,
          "second RGBA matches reference");
    check(first.covered[19 * 40 + 39] == 0 && first.indices[19 * 40 + 39] == 0,
          "uncovered texel index zero");
    check(first.rgba[4] == 3 && first.rgba[5] == 252,
          "CLUT bit exchange and RGBA");
    const auto &group = cell.palette_groups().begin()->second;
    auto two_groups =
        std::vector<std::uint8_t>(bytes.begin() + dir[0].offset,
                                  bytes.begin() + dir[0].offset + dir[0].size);
    const std::size_t group_bytes =
        0xc04 + group.symbols.size() * 2 + 18 * 4 + 17 * 4;
    const auto new_group = std::uint32_t(two_groups.size());
    const std::vector<std::uint8_t> group_copy(
        two_groups.begin() + a.palette_offset,
        two_groups.begin() + a.palette_offset + group_bytes);
    two_groups.insert(two_groups.end(), group_copy.begin(), group_copy.end());
    write32(two_groups, b.payload_offset, new_group);
    two_groups[new_group + 17 * 4] = 99; // CLUT location for index 9.
    Cell split(std::move(two_groups));
    check(split.palette_groups().size() == 2,
          "distinct per-cell palette groups are loaded independently");
    check(split.decode(0).rgba == first.rgba &&
              split.decode(1).rgba[4 * 140 * 16] == 99,
          "texture selects its own palette group");
    auto perm = block_permutation();
    auto sorted = perm;
    std::sort(sorted.begin(), sorted.end());
    for (unsigned i = 0; i < 256; ++i)
      check(sorted[i] == i, "permutation is bijective");
    for (unsigned k = 0; k < 256; ++k) {
      const unsigned r = k >> 5, c = (k >> 2) & 7, lane = k & 3;
      const unsigned texel = perm[k];
      check(gs_addr32(c, r) + lane == gs_addr8(texel & 15, texel >> 4),
            "permutation matches independent GS PSMCT32-to-PSMT8 address");
    }
    for (unsigned prefix = 0; prefix < 256; ++prefix) {
      std::uint16_t expected_symbol = 0;
      unsigned expected_length = 0;
      for (unsigned k = 1; k <= 8; ++k) {
        const unsigned code = prefix >> (8 - k);
        if (group.last_code[k] < 0 || code > unsigned(group.last_code[k]))
          continue;
        expected_symbol = group.symbols[std::size_t(
            std::int64_t(code) + group.first_index_minus_code[k])];
        expected_length = k;
        break;
      }
      check(group.fast[prefix].symbol == expected_symbol &&
                group.fast[prefix].length == expected_length,
            "256 Huffman fast prefixes match reference rule");
    }
    auto special = read(LEVEL_TEXTURE_SPECIAL);
    // All eight successor symbols select distinct entries from the previous
    // decoded texel's row, in addition to the five back-reference symbols.
    auto successor_group = group;
    std::uint8_t prior = 9;
    const std::array<std::uint8_t, 8> chain{42, 7, 20, 33, 46, 59, 72, 85};
    for (unsigned j = 0; j < chain.size(); ++j) {
      successor_group.successors[unsigned(prior) * 8 + j] = chain[j];
      prior = chain[j];
    }
    auto block = decode_block(special, 0, successor_group);
    check(fnv(std::vector<std::uint8_t>(block.begin(), block.end())) ==
              0xee3cce0cc9c5dd57ull,
          "all special symbols exact");
    rejects(
        [&] { decode_block(std::vector<std::uint8_t>(4), 0, successor_group); },
        "back-reference before texel zero");
    rejects(
        [&] {
          decode_block(
              std::vector<std::uint8_t>(special.begin(), special.begin() + 2),
              0, successor_group);
        },
        "truncated Huffman stream");
    auto uploaded = upload_block(block);
    for (unsigned i = 0; i < 256; ++i)
      check(uploaded[i] == block[perm[i]], "permuted upload byte");
    auto map = std::vector<std::uint8_t>{3, 7};
    check(relocate_offset(0x20010, map, 0x1000000) ==
              0x1000000ull + (7ull << 17) + 0x10,
          "128 KB map");
    rejects([&] { relocate_offset(0x40000, map, 0); }, "unmapped 128 KB piece");
    auto mutated =
        std::vector<std::uint8_t>(bytes.begin() + dir[0].offset,
                                  bytes.begin() + dir[0].offset + dir[0].size);
    write16(mutated, 0x48, 0x10d);
    rejects([&] { Cell c(mutated); }, "0x0001 indirection refusal");
    write16(mutated, 0x48, 0x10c);
    write32(mutated, 0x50, std::uint32_t(mutated.size()));
    rejects([&] { Cell c(mutated); }, "payload outside cell refusal");
    write32(mutated, 0x50, a.payload_offset);
    write32(mutated, 0x5c, 1);
    rejects([&] { Cell c(mutated); }, "runtime field refusal");
    rejects(
        [&] {
          File bad(std::vector<std::uint8_t>(bytes.begin(), bytes.begin() + 4));
        },
        "truncated directory");
    champions::presentation::rta::world_geometry::Geometry world;
    champions::presentation::rta::world_geometry::Packet packet;
    champions::presentation::rta::world_geometry::Batch batch;
    batch.triangles.push_back({0, 1, 2});
    batch.triangle_slots.push_back({0, 1, 2});
    batch.uvs =
        std::vector<std::array<std::uint16_t, 2>>{{0, 0}, {320, 0}, {0, 320}};
    packet.batches.push_back(batch);
    world.packets.push_back(packet);
    champions::presentation::rta::world_geometry::Chunk chunk;
    chunk.index = 7;
    chunk.cell = 5049;
    chunk.texture_offset = 0x40;
    chunk.packet_index = 0;
    chunk.world_vertices = {{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}}};
    world.chunks.push_back(chunk);
    auto surfaces = bind_world(world, file);
    check(surfaces.size() == 1 && surfaces[0].positions.size() == 3 &&
              surfaces[0].image->rgba == first.rgba,
          "WORLD chunk binds selected cell/header to exact texture");
    check(surfaces[0].uv[1] == std::array<double, 2>{0.5, 0} &&
              surfaces[0].uv[2] == std::array<double, 2>{0, 1},
          "WORLD strip UVs normalized from texels times 16");
    world.chunks.push_back(chunk);
    surfaces = bind_world(world, file);
    check(surfaces.size() == 2 && surfaces[0].image == surfaces[1].image,
          "same cell texture shares decoded image");
    auto header_state = chunk_header_material(0x10100);
    check(header_state.alpha1 == 0x48 && header_state.test1 == 0x587f4 &&
              header_state.light_constant == -230 && !header_state.dynamic_light,
          "header additive and one-frame light priority match reference");
    header_state = chunk_header_material(0x410);
    check(header_state.alpha1 == 9 && header_state.test1 == 0x582f5 &&
              header_state.environment && header_state.sort_layer == 0,
          "environment and cutout header state match reference");
    header_state = chunk_header_material(0x2200);
    check(header_state.alpha1 == 1 && header_state.light_constant == 230 &&
              header_state.sort_layer == 4,
          "translucent layer and light priority match reference");
    // Two authored DIRECT blocks override a chunk header before successive
    // batches. Each holds one packed A+D GIF tag with ALPHA_1 and TEST_1.
    std::vector<std::uint8_t> packet_bytes(128);
    auto state_block = [&](std::size_t at, std::uint64_t alpha,
                           std::uint64_t test) {
      write32(packet_bytes, at, 0x50000003);
      write64(packet_bytes, at + 4, (1ull << 60) | 2);
      write64(packet_bytes, at + 12, 0xe);
      write64(packet_bytes, at + 20, alpha);
      write64(packet_bytes, at + 28, 0x42);
      write64(packet_bytes, at + 36, test);
      write64(packet_bytes, at + 44, 0x47);
    };
    state_block(0, 1, 0x582f5);
    write32(packet_bytes, 52, 0x69010000);
    state_block(64, 2, 0x587f4);
    write32(packet_bytes, 116, 0x69010000);
    world.packets[0].data_offset = 0;
    world.packets[0].qwc = 8;
    auto &first_batch = world.packets[0].batches[0];
    first_batch.offset = 52;
    first_batch.gif_tags = {{{0, 0x30ae4000, 0, 0}}};
    auto second_batch = first_batch;
    second_batch.offset = 116;
    second_batch.gif_tags[0][1] = 0x308e4000;
    world.packets[0].batches.push_back(second_batch);
    world.chunks.resize(1);
    world.chunks[0].flags = 0x10;
    world.chunks[0].world_vertices.push_back(world.chunks[0].world_vertices[0]);
    auto states = packet_materials(packet_bytes, world.packets[0], 0x10);
    check(states.size() == 2 && states[0].alpha1 == 1 && states[0].cutout() &&
              states[0].blend_enabled && states[1].alpha1 == 2 &&
              !states[1].cutout() && !states[1].blend_enabled,
          "DIRECT last write and GIF ABE apply per batch");
    surfaces = bind_world(world, file, packet_bytes);
    check(surfaces.size() == 2 && surfaces[0].material.alpha1 == 1 &&
              surfaces[1].material.alpha1 == 2 &&
              surfaces[0].uv[1] == std::array<double, 2>{0.5, 0},
          "WORLD binding carries exact batch state and UV");
    auto resident = bind_cells(world, file, packet_bytes, {5049});
    check(resident.size() == surfaces.size() &&
              resident[0].positions == surfaces[0].positions &&
              resident[0].uv == surfaces[0].uv &&
              resident[0].image->rgba == surfaces[0].image->rgba &&
              resident[1].material.alpha1 == surfaces[1].material.alpha1,
          "one requested cell matches the full bind");
    check(bind_cells(world, file, packet_bytes, {1}).empty(),
          "an unrequested cell binds nothing");
    check(cell_id_of(0, 0, 720) == 5050, "origin is texture cell 5050");
    check(cell_id_of(-0.1, 0, 720) == 5049, "cell id uses floor division");
    check(cell_id_of(719.9, -720, 720) == 4950 &&
              cell_id_of(720, 1439.9, 720) == 5151,
          "cell id rows follow y and columns follow x");
    check(cells_around(5050) ==
              std::array<int, 9>{4949, 4950, 4951, 5049, 5050, 5051, 5149, 5150,
                                 5151},
          "texture window is the 3 by 3 around the focus cell");
    {
      const auto all = draw_slots(world, packet_bytes, nullptr);
      bool same = all.size() == surfaces.size();
      for (std::size_t i = 0; same && i < all.size(); ++i) {
        const auto &s = surfaces[std::size_t(all[i].draw_order)];
        same = s.chunk == all[i].chunk && s.batch == all[i].batch;
      }
      check(same, "geometry-only draw slots equal the bind_world draw order");
      const std::vector<std::int32_t> one{5049}, none{1};
      const auto window = draw_slots(world, packet_bytes, &one);
      same = window.size() == resident.size();
      for (std::size_t i = 0; same && i < window.size(); ++i) {
        const auto &s = resident[std::size_t(window[i].draw_order)];
        same = s.chunk == window[i].chunk && s.batch == window[i].batch;
      }
      check(same, "window draw slots equal the bind_cells draw order");
      check(draw_slots(world, packet_bytes, &none).empty(),
            "a window with no drawn cell has no draw slots");
    }
    {
      // Interleaved textures, a layer-4 chunk (flag 0x200) and a chunk in an
      // absent cell: slots must reproduce the full bind's reordering.
      auto mixed = world;
      auto second = world.chunks[0];
      second.index = 8;
      second.texture_offset = 0x80;
      auto third = world.chunks[0];
      third.index = 9;
      third.flags |= 0x200;
      auto fourth = world.chunks[0];
      fourth.index = 10;
      mixed.chunks = {world.chunks[0], second, third, fourth};
      const auto bound = bind_world(mixed, file, packet_bytes);
      const auto slots = draw_slots(mixed, packet_bytes, nullptr);
      bool same = slots.size() == bound.size() && bound.size() == 8;
      for (std::size_t i = 0; same && i < slots.size(); ++i) {
        const auto &s = bound[std::size_t(slots[i].draw_order)];
        same = s.chunk == slots[i].chunk && s.batch == slots[i].batch;
      }
      check(same && bound.front().chunk != 7 &&
                bound.back().material.sort_layer == 4,
            "draw slots follow texture groups and layers like bind_world");
      mixed.chunks.push_back(world.chunks[0]);
      mixed.chunks.back().index = 11;
      mixed.chunks.back().cell = 5050;
      const std::vector<std::int32_t> present{5049};
      const auto subset = draw_slots(mixed, packet_bytes, &present);
      const auto subset_bound = bind_cells(mixed, file, packet_bytes, present);
      same = subset.size() == subset_bound.size() && subset.size() == 8;
      for (std::size_t i = 0; same && i < subset.size(); ++i) {
        const auto &s = subset_bound[std::size_t(subset[i].draw_order)];
        same = s.chunk == subset[i].chunk && s.batch == subset[i].batch;
      }
      check(same, "window slots skip chunks of cells outside the window");
    }
    champions::presentation::rta::world_geometry::Geometry sized;
    check(texture_cell_size(sized) == 960.0, "a zero cell-size word means 960");
    const float seven_twenty = 720.f;
    std::uint32_t size_bits = 0;
    std::memcpy(&size_bits, &seven_twenty, sizeof size_bits);
    sized.header_words[0x68 / 4] = size_bits;
    check(texture_cell_size(sized) == 720.0, "cell size is the header float");
    sized.header_words[0x68 / 4] = 0xbf800000u;
    rejects([&] { texture_cell_size(sized); }, "negative cell size");
    // Explicit row provenance; inherited requires the actual preceding chunk.
    const std::array<std::int32_t, 4> rows{160, -1600, 32, 0};
    check(model_rows(ModelRows::DormantSetupZero, nullptr) ==
              std::array<std::int32_t, 4>{0, 0, 0, 0},
          "dormant setup zero rows");
    check(model_0x46_position({16, 16, 16}, ModelRows::Inherited, &rows) ==
              std::array<double, 3>{11, -99, 3},
          "inherited row position");
    rejects([&] { model_rows(ModelRows::Inherited, nullptr); },
            "inherited rows need caller");
    const std::array<std::int32_t, 4> wrap_rows{1, -1, 0, 0};
    check(model_0x46_position({0x7fffffff, -0x7fffffff - 1, 0},
                              ModelRows::Inherited,
                              &wrap_rows)[0] == -134217728.0,
          "signed 32-bit row wrap");
    std::cout << "level texture: " << checks << " checks passed\n";
  } catch (const std::exception &e) {
    std::cerr << "level texture failure: " << e.what() << '\n';
    return 1;
  }
}
