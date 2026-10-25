#include "champions/world_geometry.hpp"
#include <array>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace W = champions::presentation::rta::world_geometry;
namespace {
std::size_t checks = 0;
void check(bool value, const char *message) {
  ++checks;
  if (!value)
    throw std::runtime_error(message);
}
std::uint32_t word(const std::vector<std::uint8_t> &v, std::size_t offset) {
  return std::uint32_t(v.at(offset)) | (std::uint32_t(v.at(offset + 1)) << 8) |
         (std::uint32_t(v.at(offset + 2)) << 16) |
         (std::uint32_t(v.at(offset + 3)) << 24);
}
void store(std::vector<std::uint8_t> &v, std::size_t offset,
           std::uint32_t value) {
  for (std::size_t i = 0; i < 4; ++i)
    v.at(offset + i) = static_cast<std::uint8_t>(value >> (i * 8));
}
template <class E>
void refused(const std::vector<std::uint8_t> &bytes, W::Limits limits,
             const char *message, W::Policy policy = {}) {
  const auto before = bytes;
  bool expected = false;
  try {
    (void)W::decode(bytes, limits, policy);
  } catch (const E &) {
    expected = true;
  }
  check(expected, message);
  check(bytes == before, "refused input must remain unchanged");
}
} // namespace
int main(int argc, char **argv) {
  try {
    check(argc == 2, "provide the authored public WORLD fixture");
    std::ifstream stream(argv[1], std::ios::binary);
    check(bool(stream), "authored fixture must open");
    const std::vector<std::uint8_t> bytes(
        (std::istreambuf_iterator<char>(stream)),
        std::istreambuf_iterator<char>());
    const auto before = bytes;
    const auto geometry = W::decode(bytes);
    check(bytes == before, "successful decode must leave input unchanged");
    check(geometry.chunks.size() == 1 && geometry.packets.size() == 1,
          "authored first case has one owned chunk and packet");
    check(geometry.packets.front().batches.size() == 1,
          "authored first packet has one batch");
    const auto &batch = geometry.packets.front().batches.front();
    check(batch.positions.size() == 3 && batch.normals.size() == 3,
          "source positions and normals remain distinct raw records");
    check(batch.strip.size() == 3 && batch.triangles.size() == 1,
          "ordered strips produce the authored nondegenerate triangle");
    check(geometry.chunks.front().world_vertices.front().size() == 3,
          "chunk owns every transformed source vertex");
    auto copied = geometry;
    copied.packets.front().batches.front().positions.front()[0] = 123;
    check(geometry.packets.front().batches.front().positions.front()[0] != 123,
          "copied geometry cannot alias decoded packet storage");
    const std::vector<std::uint8_t> empty;
    refused<W::Error>(empty, {}, "empty input must refuse");
    auto malformed = bytes;
    store(malformed, 0x24, 0xfffffffcU);
    refused<W::Error>(malformed, {}, "overflowing chunk table must refuse");
    malformed = bytes;
    const auto chunk = word(bytes, 0x24);
    store(malformed, chunk + 0x24, word(bytes, chunk + 0x24) | 0x80U);
    refused<W::Unsupported>(malformed, {}, "static policy refuses water",
                            W::STATIC_ONLY);
    {
      // Authored water tile: the chunk's own data is read as the 0x90-byte
      // payload. Neighbours -1, a mask with row 0 column 0 and row 17
      // column 3 set, no packet.
      auto water = malformed;
      water.resize(water.size() + 0x90, 0);
      const auto data = std::uint32_t(water.size() - 0x90);
      store(water, chunk, data);
      store(water, chunk + 0x24, 0x80U | 0x1000U);
      store(water, data, 0xffffffffU);
      store(water, data + 4, 0xffffffffU);
      water.at(data + 0x10 + 1) = 0x80;            // half 0, column 0, row 0
      water.at(data + 0x10 + 64 + 6 + 1) = 0x40;   // half 1, column 3, row 17
      const auto g = W::decode(water);
      check(g.water.size() == 1 && g.water[0].chunk == 0 && g.packets.empty(),
            "a water tile decodes without a packet");
      check(g.water[0].neighbours ==
                std::array<std::int16_t, 4>{-1, -1, -1, -1} &&
                g.water[0].second_set && g.water[0].runtime_grid == 0,
            "water payload fields keep their documented order");
      check(g.water[0].cell(0, 0) && g.water[0].cell(17, 3) &&
                !g.water[0].cell(1, 0) && !g.water[0].cell(16, 3),
            "water mask follows the documented bit order");
      store(water, data, 0x00010001U); // neighbour 1 of a one-chunk table
      refused<W::Error>(water, {}, "water neighbour outside the chunk table");
    }
    malformed = bytes;
    store(malformed, 0x50, 1);
    refused<W::Unsupported>(malformed, {}, "static policy refuses pieces",
                            W::STATIC_ONLY);
    {
      auto moving = bytes;
      const auto table = std::uint32_t(moving.size());
      moving.resize(moving.size() + 0x1c, 0);
      store(moving, 0x50, 1);
      store(moving, 0x54, table);
      store(moving, table + 4, 1);          // first chunk 0, count 1
      store(moving, table + 8, 400);        // rest translation x
      store(moving, table + 0x14, 32767);   // rest cosine, sine 0
      store(moving, table + 0x18, 0xfffd0000U); // element 0, +0x1a = -3
      store(moving, chunk + 0x24, word(bytes, chunk + 0x24) | 0x20U);
      const auto g = W::decode(moving);
      check(g.pieces.size() == 1 && g.pieces[0].first_chunk == 0 &&
                g.pieces[0].count == 1 && g.pieces[0].rest_translation[0] == 400 &&
                g.pieces[0].rest_cosine == 32767 && g.pieces[0].unknown_1a == -3,
            "moving piece record keeps every documented field");
      check(g.chunks[0].packet_index.has_value(),
            "a moving piece chunk decodes as geometry at its rest pose");
      refused<W::Unsupported>(moving, {}, "static policy refuses piece chunk",
                              W::STATIC_ONLY);
      store(moving, table, 1); // first chunk 1 of a one-chunk table
      refused<W::Error>(moving, {}, "piece chunks outside the chunk table");
    }
    malformed = bytes;
    store(malformed, chunk + 8, 0x7f800000U);
    refused<W::Error>(malformed, {}, "nonfinite bounds must refuse");
    using Cap = std::size_t W::Limits::*;
    for (const Cap cap :
         {&W::Limits::max_input_bytes, &W::Limits::max_chunks,
          &W::Limits::max_packets, &W::Limits::max_batches,
          &W::Limits::max_vertices, &W::Limits::max_strip_slots,
          &W::Limits::max_transformed_vertices, &W::Limits::max_triangles,
          &W::Limits::max_commands, &W::Limits::max_unpack_rows,
          &W::Limits::max_render_triangles}) {
      W::Limits limits;
      limits.*cap = 0;
      refused<W::ResourceLimit>(bytes, limits,
                                "lowered zero budget must apply");
      limits = W::Limits{};
      ++(limits.*cap);
      refused<W::Error>(bytes, limits, "callers cannot widen a hard cap");
    }
    std::cout << "WORLD geometry API checks: " << checks << "; failures: 0\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "WORLD geometry API failure: " << error.what() << '\n';
    return 1;
  }
}
