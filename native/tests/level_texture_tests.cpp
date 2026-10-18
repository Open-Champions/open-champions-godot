#include "champions/level_texture.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
using namespace champions::presentation::rta::level_texture;
namespace {
int checks = 0;
void check(bool ok, const char *why) { ++checks; if (!ok) throw std::runtime_error(why); }
template <typename F> void rejects(F f, const char *why) {
  bool thrown = false;
  try { f(); } catch (const Error &) { thrown = true; }
  check(thrown, why);
}
std::vector<std::uint8_t> read(const char *name) {
  std::ifstream in(name, std::ios::binary);
  check(bool(in), "synthetic fixture missing");
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
std::uint64_t fnv(const std::vector<std::uint8_t> &bytes) {
  std::uint64_t h = 14695981039346656037ull;
  for (auto b : bytes) h = (h ^ b) * 1099511628211ull;
  return h;
}
void write16(std::vector<std::uint8_t> &v, std::size_t at, std::uint16_t n) {
  v.at(at) = std::uint8_t(n); v.at(at + 1) = std::uint8_t(n >> 8);
}
void write32(std::vector<std::uint8_t> &v, std::size_t at, std::uint32_t n) {
  for (unsigned i = 0; i < 4; ++i) v.at(at + i) = std::uint8_t(n >> (i * 8));
}
} // namespace
int main() {
  try {
    const auto bytes = read(LEVEL_TEXTURE_FIXTURE);
    File file(bytes);
    const auto &dir = file.directory();
    check(dir.size() == 2 && dir[0].id == 5049 && dir[1].id == 5151, "directory IDs");
    check(dir[0].offset == 2048 && dir[0].size == 9032, "directory extents");
    auto table = cell_table(dir, 4949, 5151);
    check(table.size() == 203 && table[0].size == 0x40 && table[100].offset == 2048, "WORLD cell table");
    rejects([&] { cell_table(dir, 5100, 5151); }, "out-of-range WORLD cell");
    auto cell = file.load_cell(5049);
    check(cell.textures().size() == 2 && cell.palette_groups().size() == 1, "shared palette group");
    check(cell.index_at_header_offset(0x40) == 0 && cell.index_at_header_offset(0x80) == 1, "chunk header offsets");
    rejects([&] { cell.index_at_header_offset(0x41); }, "unaligned chunk header");
    const auto &a = cell.textures()[0], &b = cell.textures()[1];
    check(a.width == 40 && a.height == 20 && a.allocation_blocks == 9 && a.relocated_qwords == 164, "first header relocation");
    check(b.width == 140 && b.height == 18 && b.allocation_blocks == 39 && b.relocated_qwords == 222, "second header relocation");
    check(a.rectangles.size() == 2 && a.rectangles[0].block_offsets.size() == 4 && b.rectangles[0].block_offsets.size() == 9, "rectangle blocks");
    auto first = cell.decode(0), second = cell.decode(1);
    check(fnv(first.indices) == 0xe4da05fbde40ae85ull, "first indices match reference");
    check(fnv(first.covered) == 0xab74d88bf10e7ca5ull, "first coverage matches reference");
    check(fnv(first.rgba) == 0xb5aa79dfa7549ee5ull, "first RGBA matches reference");
    check(fnv(second.indices) == 0x10a13f5af26f78a9ull, "second indices match reference");
    check(fnv(second.covered) == 0xc708eee3c62ef29dull, "second coverage matches reference");
    check(fnv(second.rgba) == 0xf4ffe2f922c5b751ull, "second RGBA matches reference");
    check(first.covered[19 * 40 + 39] == 0 && first.indices[19 * 40 + 39] == 0, "uncovered texel index zero");
    check(first.rgba[4] == 3 && first.rgba[5] == 252, "CLUT bit exchange and RGBA");
    auto perm = block_permutation();
    auto sorted = perm; std::sort(sorted.begin(), sorted.end());
    for (unsigned i = 0; i < 256; ++i) check(sorted[i] == i, "permutation is bijective");
    const auto &group = cell.palette_groups().begin()->second;
    auto special = read(LEVEL_TEXTURE_SPECIAL);
    // The authored special stream uses successor rows 9->42 and 42->7.
    auto successor_group = group;
    successor_group.successors[9 * 8] = 42;
    successor_group.successors[42 * 8 + 1] = 7;
    auto block = decode_block(special, 0, successor_group);
    check(fnv(std::vector<std::uint8_t>(block.begin(), block.end())) == 0xfc700cd5068307a0ull, "all special symbols exact");
    auto uploaded = upload_block(block);
    for (unsigned i = 0; i < 256; ++i) check(uploaded[i] == block[perm[i]], "permuted upload byte");
    auto map = std::vector<std::uint8_t>{3, 7};
    check(relocate_offset(0x20010, map, 0x1000000) == 0x1000000ull + (7ull << 17) + 0x10, "128 KB map");
    rejects([&] { relocate_offset(0x40000, map, 0); }, "unmapped 128 KB piece");
    auto mutated = std::vector<std::uint8_t>(bytes.begin() + dir[0].offset, bytes.begin() + dir[0].offset + dir[0].size);
    write16(mutated, 0x48, 0x10d); rejects([&] { Cell c(mutated); }, "0x0001 indirection refusal");
    write16(mutated, 0x48, 0x10c);
    write32(mutated, 0x50, std::uint32_t(mutated.size())); rejects([&] { Cell c(mutated); }, "payload outside cell refusal");
    write32(mutated, 0x50, a.payload_offset);
    write32(mutated, 0x5c, 1); rejects([&] { Cell c(mutated); }, "runtime field refusal");
    rejects([&] { File bad(std::vector<std::uint8_t>(bytes.begin(), bytes.begin() + 4)); }, "truncated directory");
    champions::presentation::rta::world_geometry::Geometry world;
    champions::presentation::rta::world_geometry::Packet packet;
    champions::presentation::rta::world_geometry::Batch batch;
    batch.triangles.push_back({0,1,2});
    batch.triangle_slots.push_back({0,1,2});
    batch.uvs = std::vector<std::array<std::uint16_t,2>>{{0,0},{320,0},{0,320}};
    packet.batches.push_back(batch);
    world.packets.push_back(packet);
    champions::presentation::rta::world_geometry::Chunk chunk;
    chunk.index = 7; chunk.cell = 5049; chunk.texture_offset = 0x40; chunk.packet_index = 0;
    chunk.world_vertices = {{{0,0,0},{1,0,0},{0,1,0}}};
    world.chunks.push_back(chunk);
    auto surfaces = bind_world(world, file);
    check(surfaces.size() == 1 && surfaces[0].positions.size() == 3 && surfaces[0].image->rgba == first.rgba,
          "WORLD chunk binds selected cell/header to exact texture");
    check(surfaces[0].uv[1] == std::array<double,2>{0.5,0} && surfaces[0].uv[2] == std::array<double,2>{0,1},
          "WORLD strip UVs normalized from texels times 16");
    world.chunks.push_back(chunk);
    surfaces = bind_world(world, file);
    check(surfaces.size() == 2 && surfaces[0].image == surfaces[1].image, "same cell texture shares decoded image");
    // Explicit row provenance; inherited requires the actual preceding chunk.
    const std::array<std::int32_t, 4> rows{160, -1600, 32, 0};
    check(model_rows(ModelRows::DormantSetupZero, nullptr) == std::array<std::int32_t, 4>{0,0,0,0}, "dormant setup zero rows");
    check(model_0x46_position({16,16,16}, ModelRows::Inherited, &rows) == std::array<double,3>{11,-99,3}, "inherited row position");
    rejects([&] { model_rows(ModelRows::Inherited, nullptr); }, "inherited rows need caller");
    const std::array<std::int32_t, 4> wrap_rows{1,-1,0,0};
    check(model_0x46_position({0x7fffffff, -0x7fffffff - 1, 0}, ModelRows::Inherited,
                              &wrap_rows)[0] == -134217728.0, "signed 32-bit row wrap");
    std::cout << "level texture: " << checks << " checks passed\n";
  } catch (const std::exception &e) {
    std::cerr << "level texture failure: " << e.what() << '\n'; return 1;
  }
}
