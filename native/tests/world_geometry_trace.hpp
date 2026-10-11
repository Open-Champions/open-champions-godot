#pragma once
// Complete owned presentation observation. Shared by the public strict replay
// and optional private validators; contains no input loading or expected data.
#include "champions/world_geometry.hpp"
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>
namespace champions::test::world_geometry_trace {
namespace G = champions::presentation::rta::world_geometry;
template <class T> std::string n(T value) { return std::to_string(value); }
inline std::string sequence(const std::vector<std::string> &v) {
  std::string s = "[";
  for (const auto &x : v) {
    if (s.size() > 1)
      s += ',';
    s += x;
  }
  return s + ']';
}
template <class V> std::string ints(const V &v) {
  std::vector<std::string> o;
  for (auto x : v)
    o.push_back(n(x));
  return sequence(o);
}

template <class V> std::string rows(const V &value) {
  std::vector<std::string> result;
  for (const auto &row : value)
    result.push_back(ints(row));
  return sequence(result);
}
inline std::uint64_t double_bits(double value) {
  std::uint64_t out;
  static_assert(sizeof(out) == sizeof(value));
  static_assert(std::numeric_limits<double>::is_iec559 &&
                std::numeric_limits<double>::digits == 53);
  std::memcpy(&out, &value, sizeof(out));
  return out;
}
inline std::string limits(const G::Limits &l) {
  return sequence({n(l.max_input_bytes), n(l.max_chunks), n(l.max_packets),
                   n(l.max_batches), n(l.max_vertices), n(l.max_strip_slots),
                   n(l.max_transformed_vertices), n(l.max_triangles),
                   n(l.max_commands), n(l.max_unpack_rows),
                   n(l.max_render_triangles)});
}
inline std::string geometry(const G::Geometry &g) {
  std::vector<std::string> packets, chunks;
  for (const auto &p : g.packets) {
    std::vector<std::string> batches;
    for (const auto &b : p.batches) {
      std::vector<std::string> strip;
      for (const auto &slot : b.strip)
        strip.push_back(sequence({n(slot.vertex), n(slot.nodraw)}));
      batches.push_back(sequence(
          {n(b.offset), rows(b.positions), rows(b.normals),
           rows(b.vertex_locations), rows(b.extra_copies), rows(b.gif_tags),
           sequence(strip), b.uvs ? rows(*b.uvs) : "_", n(b.nreg), n(b.prim),
           n(b.program), rows(b.triangles), rows(b.triangle_slots)}));
    }
    packets.push_back(
        sequence({n(p.data_offset), n(p.qwc), sequence(batches)}));
  }
  for (const auto &c : g.chunks) {
    std::vector<std::string> batches;
    for (const auto &batch : c.world_vertices) {
      std::vector<std::string> points;
      for (const auto &p : batch)
        points.push_back(sequence({n(double_bits(p[0])), n(double_bits(p[1])),
                                   n(double_bits(p[2]))}));
      batches.push_back(sequence(points));
    }
    chunks.push_back(sequence(
        {n(c.index), n(c.data_offset), n(c.qwc), ints(c.bounds_bits),
         n(c.texture_offset), n(c.flags), ints(c.translation), n(c.cell),
         ints(c.rotation), c.packet_index ? n(*c.packet_index) : "_",
         sequence(batches)}));
  }
  return sequence({ints(g.header_words), sequence(packets), sequence(chunks),
                   n(g.omitted_models), n(g.omitted_emitters),
                   n(g.omitted_lights)});
}

} // namespace champions::test::world_geometry_trace
