// Authored static presentation projection from world_info.py at
// 2f937fca823488bc97b9680fde9d957c3fa9907a, SHA256
// 2fa037872b8b5619a65ced370b5744a63219e64d4d32c1ed33795dd8ecc249a5.
// Strip/transform contract: docs/formats/world.md; program scope and two-sided
// presentation: docs/spec/render-pipeline.md at abf07ea. Water tiles and
// moving pieces: docs/formats/world.md and docs/level-flow.md sections 11-12 on
// research t3/research-levels 36036da. No original assets or executable
// instructions are included. Binary64 here is never gameplay state.
#include "champions/world_geometry.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <utility>

namespace champions::presentation::rta::world_geometry {
namespace {
using Offset = std::uint64_t;
constexpr std::uint32_t NOT_MESH = 0x80 | 0x800 | 0x4000 | 0x8000;
void require(bool b, const char *message) {
  if (!b)
    throw Error(message);
}
void supported(bool b, const char *message) {
  if (!b)
    throw Unsupported(message);
}
void budget(std::size_t n, std::size_t max, const char *message) {
  if (n > max)
    throw ResourceLimit(message);
}
void charge(std::size_t n, std::size_t &left) {
  budget(n, left, "WORLD geometry aggregate budget exceeded");
  left -= n;
}
std::int32_t signed32(std::uint32_t n) {
  return n <= 0x7fffffffu ? std::int32_t(n)
                          : -1 - std::int32_t(0xffffffffu - n);
}
std::int16_t signed16(std::uint16_t n) {
  return std::int16_t(n < 0x8000 ? std::int32_t(n) : std::int32_t(n) - 0x10000);
}
class Reader {
  const std::vector<std::uint8_t> &data_;

public:
  explicit Reader(const std::vector<std::uint8_t> &d) : data_(d) {}
  void span(Offset p, Offset n) const {
    require(p <= data_.size() && n <= data_.size() - p,
            "WORLD geometry range exceeds input");
  }
  std::uint8_t u8(Offset p) const {
    span(p, 1);
    return data_[std::size_t(p)];
  }
  std::uint16_t u16(Offset p) const {
    return std::uint16_t(std::uint32_t(u8(p)) |
                         (std::uint32_t(u8(p + 1)) << 8));
  }
  std::uint32_t u32(Offset p) const {
    return std::uint32_t(u16(p)) | (std::uint32_t(u16(p + 2)) << 16);
  }
  std::int16_t i16(Offset p) const { return signed16(u16(p)); }
  std::int32_t i32(Offset p) const { return signed32(u32(p)); }
};
bool finite32(std::uint32_t v) { return (v & 0x7f800000u) != 0x7f800000u; }
void validate_limits(const Limits &l) {
  const Limits h;
#define CAP(name)                                                              \
  budget(l.name, h.name, "WORLD geometry limit exceeds hard cap")
  CAP(max_input_bytes);
  CAP(max_chunks);
  CAP(max_packets);
  CAP(max_batches);
  CAP(max_vertices);
  CAP(max_strip_slots);
  CAP(max_transformed_vertices);
  CAP(max_triangles);
  CAP(max_commands);
  CAP(max_unpack_rows);
  CAP(max_render_triangles);
#undef CAP
}
void header(const Reader &r, Geometry &g, std::size_t size, const Limits &l,
            const Policy &policy) {
  r.span(0, 0x84);
  for (std::size_t i = 0; i < 33; ++i)
    g.header_words[i] = r.u32(i * 4);
  auto u = [&](std::size_t o) { return g.header_words[o / 4]; };
  const auto n = u(0), tree = u(4), co = u(0x24);
  require(n && n <= 65535 && co >= 0x84, "invalid WORLD chunk table");
  budget(n, l.max_chunks, "WORLD chunk budget exceeded");
  r.span(co, Offset(n) * 0x3c);
  if (tree) {
    require(tree >= 0x84, "invalid WORLD tree offset");
    r.span(tree, 5);
    require(r.u16(tree) == 0x4567, "invalid WORLD tree marker");
  }
  require(u(0x10) <= 4096 && u(0x14) <= 4096,
          "invalid WORLD terrain grid dimensions");
  require(u(0x1c) <= 1000000, "invalid WORLD element count");
  if (u(0x1c))
    r.span(u(0x20), Offset(u(0x1c)) * 0x24);
  if (u(0x18)) {
    r.span(u(0x18), Offset(u(0x10)) * u(0x14) * 4);
    require(u(0x78) >= 0x84, "invalid WORLD tail offset");
    r.span(u(0x78), 1);
  }
  require(u(0x30) <= 8192 && u(0x34) <= 8192, "invalid WORLD path dimensions");
  if (u(0x38))
    r.span(u(0x38), Offset(u(0x30)) * u(0x34) * 4);
  if (u(0x4c)) {
    require(u(0x44) <= 65536 && u(0x48) <= 65536,
            "invalid WORLD bit grid dimensions");
    r.span(u(0x4c), (Offset(u(0x44)) * u(0x48) + 7) / 8);
  }
  require(u(0x50) <= 10000, "invalid WORLD piece count");
  if (u(0x50))
    r.span(u(0x54), Offset(u(0x50)) * 0x1c);
  supported(policy.moving_pieces || u(0x50) == 0,
            "moving WORLD pieces are unsupported");
  const auto lo = std::int64_t(signed32(u(0x58))),
             hi = std::int64_t(signed32(u(0x5c)));
  require(lo <= hi && hi - lo < 20000, "invalid WORLD texture cell range");
  r.span(u(0x64), Offset(hi - lo + 1) * 8);
  const auto cs = u(0x68);
  require(finite32(cs) && (!(cs & 0x80000000u) || (cs & 0x7fffffffu) == 0),
          "invalid WORLD texture cell size");
  require(u(0x6c) < size && u(0x80) < size, "invalid WORLD automap offsets");
  // Validate the reader's terrain-record extents even though terrain output is
  // a separate projection. Repeated heightfield offsets remain legal aliases.
  for (std::size_t i = 0; i < u(0x1c); ++i) {
    const Offset p = Offset(u(0x20)) + i * 0x24, hf = r.u32(p + 0xc);
    require(hf >= 0x84, "invalid WORLD heightfield offset");
    r.span(hf, 0x14);
    const auto w = r.u32(hf + 8), h = r.u32(hf + 12);
    require(w && h && w <= 4096 && h <= 4096,
            "invalid WORLD heightfield dimensions");
    r.span(hf + 0x14, Offset(w) * h * 2);
  }
}
void finish(Batch &b, Limits &left) {
  require(!b.positions.empty() && b.normals.size() == b.positions.size() &&
              b.vertex_locations.size() == b.positions.size() + 2 &&
              !b.extra_copies.empty() && !b.gif_tags.empty(),
          "incomplete WORLD geometry batch");
  const auto n = b.gif_tags.front()[0] & 0x7fffu;
  b.nreg = std::uint8_t((b.gif_tags.front()[1] >> 28) & 15);
  b.prim = std::uint16_t((b.gif_tags.front()[1] >> 15) & 0x7ff);
  require(b.nreg == 3 || b.nreg == 4, "unsupported WORLD GIF register count");
  supported((b.prim & 7) == 4,
            "unsupported WORLD primitive (expected triangle strip)");
  require((b.program == 0x46 && b.nreg == 3) ||
              (b.program == 0x48 && b.nreg == 4),
          "WORLD program and GIF stride disagree");
  require(b.extra_copies.front()[0] == b.extra_copies.size() - 1,
          "WORLD extra-copy count mismatch");
  charge(n, left.max_strip_slots);
  b.strip.resize(n);
  std::vector<bool> filled(n, false);
  auto copy = [&](std::uint16_t src, std::uint16_t dst) {
    if (src == dst)
      return; // Published reader semantics, including ADC bits.
    const auto s = (std::uint32_t(src) & 0x7fffu) / b.nreg,
               t = (std::uint32_t(dst) & 0x7fffu) / b.nreg;
    require(s < n && t < n && filled[s] && !filled[t],
            "invalid WORLD strip copy order or destination");
    b.strip[t] = {b.strip[s].vertex, bool(dst >> 15)};
    filled[t] = true;
  };
  for (std::size_t i = 0; i < b.positions.size(); ++i) {
    const auto &row = b.vertex_locations[i + 2];
    copy(row[1], row[2]);
    const auto s = (std::uint32_t(row[0]) & 0x7fffu) / b.nreg;
    require(s < n && !filled[s], "invalid WORLD strip placement");
    b.strip[s] = {std::uint32_t(i), bool(row[0] >> 15)};
    filled[s] = true;
  }
  for (std::size_t i = 1; i < b.extra_copies.size(); ++i) {
    const auto &row = b.extra_copies[i];
    copy(row[0], row[1]);
    copy(row[2], row[3]);
  }
  require(std::all_of(filled.begin(), filled.end(), [](bool x) { return x; }),
          "WORLD strip has empty slots");
  for (std::uint32_t i = 2; i < n; ++i) {
    const auto a = b.strip[i - 2].vertex, c = b.strip[i - 1].vertex,
               d = b.strip[i].vertex;
    if (!b.strip[i].nodraw && a != c && c != d && a != d) {
      charge(1, left.max_triangles);
      b.triangles.push_back({a, c, d});
      b.triangle_slots.push_back({i - 2, i - 1, i});
    }
  }
}
Packet packet(const Reader &r, std::uint32_t offset, std::uint32_t qwc,
              Limits &left) {
  require(qwc != 0, "empty WORLD packet");
  const Offset end = Offset(offset) + Offset(qwc) * 16;
  r.span(offset, Offset(qwc) * 16);
  Packet out;
  out.data_offset = offset;
  out.qwc = qwc;
  std::optional<Batch> cur;
  Offset p = offset;
  while (p < end) {
    charge(1, left.max_commands);
    require(end - p >= 4, "truncated WORLD VIF code");
    const auto here = p;
    const auto w = r.u32(p);
    p += 4;
    supported(!(w & 0x80000000u), "WORLD VIF IRQ is unsupported");
    const auto cmd = (w >> 24) & 0x7f, num = (w >> 16) & 0xff, imm = w & 0xffff;
    if (cmd == 0) {
      require(num == 0 && imm == 0, "invalid WORLD NOP");
      continue;
    }
    if (cmd == 1) {
      supported(num == 0 && (imm == 0x101 || imm == 0x103 || imm == 0x104),
                "unsupported WORLD VIF cycle");
      continue;
    }
    if (cmd == 4) {
      require(num == 0, "invalid WORLD ITOP");
      continue;
    }
    if (cmd == 5) {
      supported(num == 0 && imm <= 1, "unsupported WORLD VIF mode");
      continue;
    }
    if (cmd == 0x11) {
      require(num == 0 && imm == 0, "invalid WORLD FLUSH");
      continue;
    }
    if (cmd == 0x50) {
      require(num == 0, "invalid WORLD DIRECT");
      const Offset size = Offset(imm ? imm : 65536) * 16;
      require(size <= end - p, "WORLD DIRECT exceeds packet");
      p += size;
      continue;
    }
    if (cmd == 0x14) {
      supported(imm == 0x46 || imm == 0x48, "unsupported WORLD microprogram");
      require(num == 0 && cur.has_value(), "WORLD MSCAL without active batch");
      cur->program = std::uint16_t(imm);
      finish(*cur, left);
      out.batches.push_back(std::move(*cur));
      cur.reset();
      continue;
    }
    require((cmd & 0x60) == 0x60 && !(cmd & 0x10),
            "unsupported WORLD VIF command");
    const auto vn = (cmd >> 2) & 3, vl = cmd & 3, components = vn + 1,
               count = num ? num : 256;
    require(vl < 3, "unsupported WORLD VIF packed format");
    supported(!(imm & 0x3c00), "unsupported WORLD VIF address bits");
    const bool us = (imm & 0x4000) != 0;
    const auto unit = 4u >> vl;
    const Offset size = Offset(count) * components * unit,
                 aligned = (size + 3) & ~Offset(3);
    require(aligned <= end - p, "WORLD UNPACK exceeds packet");
    charge(count, left.max_unpack_rows);
    if (components == 3 && vl == 1 && !us) {
      require(!cur, "WORLD positions before previous MSCAL");
      charge(1, left.max_batches);
      charge(count, left.max_vertices);
      cur.emplace();
      cur->offset = std::uint32_t(here);
      cur->positions.resize(count);
      for (std::size_t i = 0; i < count; ++i)
        for (std::size_t j = 0; j < 3; ++j)
          cur->positions[i][j] = r.i16(p + (i * 3 + j) * 2);
    } else if (components == 3 && vl == 2 && !us) {
      require(cur && cur->normals.empty(), "duplicate or stray WORLD normals");
      cur->normals.resize(count);
      for (std::size_t i = 0; i < count; ++i)
        for (std::size_t j = 0; j < 3; ++j) {
          const auto v = r.u8(p + i * 3 + j);
          cur->normals[i][j] = std::int8_t(v < 128 ? int(v) : int(v) - 256);
        }
    } else if (components == 3 && vl == 1 && us) {
      require(cur && cur->vertex_locations.empty(),
              "duplicate or stray WORLD strip table");
      cur->vertex_locations.resize(count);
      for (std::size_t i = 0; i < count; ++i)
        for (std::size_t j = 0; j < 3; ++j)
          cur->vertex_locations[i][j] = r.u16(p + (i * 3 + j) * 2);
    } else if (components == 4 && vl == 1 && us) {
      require(cur && cur->extra_copies.empty(),
              "duplicate or stray WORLD extra-copy table");
      cur->extra_copies.resize(count);
      for (std::size_t i = 0; i < count; ++i)
        for (std::size_t j = 0; j < 4; ++j)
          cur->extra_copies[i][j] = r.u16(p + (i * 4 + j) * 2);
    } else if (components == 4 && vl == 0 && us) {
      require(cur && cur->gif_tags.empty(),
              "duplicate or stray WORLD GIF tags");
      cur->gif_tags.resize(count);
      for (std::size_t i = 0; i < count; ++i)
        for (std::size_t j = 0; j < 4; ++j)
          cur->gif_tags[i][j] = r.u32(p + (i * 4 + j) * 4);
    } else if (components == 2 && vl == 1 && us) {
      require(!cur && !out.batches.empty() && !out.batches.back().uvs &&
                  count == out.batches.back().strip.size(),
              "WORLD UVs do not match completed batch");
      auto &uv = out.batches.back().uvs;
      uv.emplace(count);
      for (std::size_t i = 0; i < count; ++i)
        for (std::size_t j = 0; j < 2; ++j)
          (*uv)[i][j] = r.u16(p + (i * 2 + j) * 2);
    } else
      throw Unsupported("unsupported WORLD UNPACK shape");
    p += aligned;
  }
  require(!cur, "WORLD packet ends before MSCAL");
  require(!out.batches.empty(), "WORLD packet has no geometry batches");
  return out;
}
} // namespace
std::array<double, 4> chunk_matrix(std::uint32_t flags,
                                   const std::array<std::int16_t, 3> &rotation) {
  if (flags & 1) {
    const auto code = std::uint16_t(rotation[0]) & 7;
    const double x = (code & 1) ? -1 : 1, y = (code & 2) ? -1 : 1;
    return (code & 4) ? std::array<double, 4>{0, y, x, 0}
                      : std::array<double, 4>{x, 0, 0, y};
  }
  const double co = rotation[0] / 32767.0, si = rotation[1] / 32767.0;
  return (flags & 0x40) ? std::array<double, 4>{co, si, si, -co}
                        : std::array<double, 4>{co, -si, si, co};
}
bool WaterTile::cell(unsigned row, unsigned column) const {
  if (row > 31 || column > 31)
    throw Error("WORLD water mask cell outside 32 x 32");
  const auto at = (row / 16) * 64 + column * 2;
  const auto half = std::uint32_t(mask[at]) | (std::uint32_t(mask[at + 1]) << 8);
  return (half >> (15 - row % 16)) & 1;
}
Geometry decode(const std::vector<std::uint8_t> &bytes, Limits limits,
                Policy policy) {
  validate_limits(limits);
  budget(bytes.size(), limits.max_input_bytes, "WORLD input budget exceeded");
  Reader r(bytes);
  Geometry out;
  header(r, out, bytes.size(), limits, policy);
  Limits left = limits;
  const auto n = out.header_words[0], co = out.header_words[0x24 / 4];
  out.chunks.reserve(n);
  for (std::uint32_t i = 0; i < n; ++i) {
    const Offset p = Offset(co) + Offset(i) * 0x3c;
    Chunk c;
    c.index = i;
    c.data_offset = r.u32(p);
    c.qwc = r.u32(p + 4);
    for (std::size_t j = 0; j < 6; ++j) {
      c.bounds_bits[j] = r.u32(p + 8 + j * 4);
      require(finite32(c.bounds_bits[j]), "nonfinite WORLD chunk bounds");
    }
    c.texture_offset = r.u32(p + 0x20);
    c.flags = r.u32(p + 0x24);
    for (std::size_t j = 0; j < 3; ++j) {
      c.translation[j] = r.i32(p + 0x28 + j * 4);
      c.rotation[j] = r.i16(p + 0x36 + j * 2);
    }
    c.cell = r.i16(p + 0x34);
    r.span(c.data_offset, 1);
    supported(policy.water || !(c.flags & 0x80), "WORLD water is unsupported");
    supported(policy.moving_pieces || !(c.flags & 0x20),
              "moving WORLD chunk is unsupported");
    const auto kind = c.flags & NOT_MESH;
    require(!kind || !(kind & (kind - 1)), "conflicting WORLD chunk kinds");
    if (kind == 0x80) {
      r.span(c.data_offset, 0x90);
      WaterTile w;
      w.chunk = i;
      for (std::size_t j = 0; j < 4; ++j) {
        w.neighbours[j] = r.i16(Offset(c.data_offset) + j * 2);
        require(w.neighbours[j] >= -1 && std::uint32_t(w.neighbours[j] + 1) <= n,
                "WORLD water neighbour outside chunk table");
      }
      w.runtime_grid = r.u32(Offset(c.data_offset) + 8);
      for (std::size_t j = 0; j < 128; ++j)
        w.mask[j] = r.u8(Offset(c.data_offset) + 0x10 + j);
      w.second_set = (c.flags & 0x1000) != 0;
      out.water.push_back(w);
    } else if (kind == 0x800) {
      r.span(c.data_offset, 0xb0);
      ++out.omitted_models;
    } else if (kind == 0x4000) {
      r.span(c.data_offset, 0x48);
      ++out.omitted_emitters;
    } else if (kind == 0x8000) {
      r.span(c.data_offset, 0x30);
      ++out.omitted_lights;
    }
    out.chunks.push_back(std::move(c));
  }
  const auto piece_count = out.header_words[0x50 / 4],
             piece_table = out.header_words[0x54 / 4];
  out.pieces.reserve(piece_count);
  for (std::uint32_t i = 0; i < piece_count; ++i) {
    const Offset p = Offset(piece_table) + Offset(i) * 0x1c;
    MovingPiece m;
    m.first_chunk = r.i32(p);
    m.count = r.i16(p + 4);
    m.runtime_flags = r.u16(p + 6);
    for (std::size_t j = 0; j < 3; ++j)
      m.rest_translation[j] = r.i32(p + 8 + j * 4);
    m.rest_cosine = r.i16(p + 0x14);
    m.rest_sine = r.i16(p + 0x16);
    m.element = r.i16(p + 0x18);
    m.unknown_1a = r.i16(p + 0x1a);
    // A count <= 0 names no chunk (world_movePiece then writes nothing).
    if (m.count > 0)
      require(m.first_chunk >= 0 &&
                  std::uint64_t(m.first_chunk) + std::uint64_t(m.count) <= n,
              "WORLD moving piece chunks outside chunk table");
    out.pieces.push_back(m);
  }
  std::map<std::pair<std::uint32_t, std::uint32_t>, std::size_t> cache;
  for (auto &c : out.chunks) {
    if (c.flags & NOT_MESH)
      continue;
    const auto key = std::make_pair(c.data_offset, c.qwc);
    auto found = cache.find(key);
    if (found == cache.end()) {
      charge(1, left.max_packets);
      const auto index = out.packets.size();
      out.packets.push_back(packet(r, c.data_offset, c.qwc, left));
      found = cache.emplace(key, index).first;
    }
    c.packet_index = found->second;
    const auto &p = out.packets[*c.packet_index];
    const auto m = chunk_matrix(c.flags, c.rotation);
    c.world_vertices.reserve(p.batches.size());
    for (const auto &b : p.batches) {
      charge(b.positions.size(), left.max_transformed_vertices);
      charge(b.triangles.size(), left.max_render_triangles);
      std::vector<Vec3> v;
      v.reserve(b.positions.size());
      for (const auto &raw : b.positions) {
        const double x = (std::int64_t(raw[0]) + c.translation[0]) * 0.25,
                     y = (std::int64_t(raw[1]) + c.translation[1]) * 0.25,
                     z = (std::int64_t(raw[2]) + c.translation[2]) * 0.25;
        const Vec3 point{m[0] * x + m[1] * y, m[2] * x + m[3] * y, z};
        require(std::all_of(point.begin(), point.end(),
                            [](double a) { return std::isfinite(a); }),
                "nonfinite transformed WORLD vertex");
        v.push_back(point);
      }
      c.world_vertices.push_back(std::move(v));
    }
  }
  return out;
}
} // namespace champions::presentation::rta::world_geometry
