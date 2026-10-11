// Authored port of ref_world.py, commit
// 6831458ee7ee12bb166d1dce4eb8e4340095bc48, SHA256
// 30366cb77918630241cd036b20df6cd86c4869be453c4160bd20c99b786ea4b8.
// https://github.com/Open-Champions/champions-return-to-arms-re
// Supplied post-fixup terrain; no original data or executable instructions.
#include "world_queries_internal.hpp"
#include <limits>

namespace champions::simulation::rta::world_queries {
namespace {
void coordinate(std::int64_t v) {
  if (v < INT32_MIN || v > INT32_MAX)
    throw Error("terrain coordinate exceeds signed32 domain");
}
std::size_t area(std::int64_t w, std::int64_t h, std::size_t limit) {
  if (w < 0 || h < 0 || w > INT32_MAX || h > INT32_MAX)
    throw Error("invalid terrain dimensions");
  const auto n = std::uint64_t(w) * std::uint64_t(h);
  if (n > limit)
    throw ResourceLimit("terrain dimensions exceed budget");
  return std::size_t(n);
}
void count(std::size_t n, std::size_t &left) {
  if (n > left)
    throw ResourceLimit("terrain aggregate storage budget exceeded");
  left -= n;
}
void validate(const LevelData &d, const Limits &l) {
  const Limits hard;
  if (l.heightfields > hard.heightfields || l.elements > hard.elements ||
      l.grid_cells > hard.grid_cells || l.samples > hard.samples ||
      l.references > hard.references || l.path_cells > hard.path_cells ||
      l.query_work > hard.query_work)
    throw ResourceLimit("terrain limits exceed hard caps");
  if (d.heightfields.size() > l.heightfields || d.elements.size() > l.elements)
    throw ResourceLimit("terrain record count exceeds budget");
  coordinate(d.tg_x0);
  coordinate(d.tg_y0);
  coordinate(d.pg_x0);
  coordinate(d.pg_y0);
  if (d.grid.size() != area(d.tg_w, d.tg_h, l.grid_cells) ||
      d.path_cells.size() != area(d.pg_w, d.pg_h, l.path_cells))
    throw Error("terrain grid extent disagrees with dimensions");
  auto samples = l.samples, refs = l.references;
  for (const auto &hf : d.heightfields) {
    coordinate(hf.x0);
    coordinate(hf.y0);
    coordinate(hf.lo);
    coordinate(hf.hi);
    const auto n = area(hf.w, hf.h, l.samples);
    if (hf.samples.size() != n)
      throw Error("heightfield sample extent disagrees with dimensions");
    count(n, samples);
  }
  for (const auto &e : d.elements) {
    for (auto v :
         {e.min_x, e.min_y, e.max_x, e.max_y, e.x, e.y, e.z, e.cos, e.sin})
      coordinate(v);
    if (e.flags < 0 || std::uint64_t(e.flags) > UINT32_MAX)
      throw Error("terrain flags exceed unsigned32 domain");
    (void)detail::index(e.hf, d.heightfields.size());
  }
  for (const auto &cell : d.grid) {
    count(cell.size(), refs);
    for (auto i : cell)
      (void)detail::index(i, d.elements.size());
  }
  count(d.pieces.size(), refs);
  for (auto i : d.pieces)
    (void)detail::index(i, d.elements.size());
}
} // namespace
Level::Level(const LevelData &data, Limits limits) : limits_(limits) {
  validate(data, limits);
  data_ = data;
}
Element make_element(std::int64_t label, std::int64_t min_x, std::int64_t min_y,
                     std::int64_t max_x, std::int64_t max_y, std::int64_t hf,
                     std::int64_t flags, std::int64_t x, std::int64_t y,
                     std::int64_t z, std::int64_t cosine, std::int64_t sine) {
  Element e;
  e.index = label;
  e.min_x = min_x;
  e.min_y = min_y;
  e.max_x = max_x;
  e.max_y = max_y;
  e.hf = hf;
  e.flags = flags;
  e.x = x;
  e.y = y;
  e.z = z;
  e.cos = cosine;
  e.sin = sine;
  e.f_min_x = detail::integer(min_x);
  e.f_min_y = detail::integer(min_y);
  e.f_max_x = detail::integer(max_x);
  e.f_max_y = detail::integer(max_y);
  return e;
}
void Level::set_piece_collision(std::int64_t piece, bool on) {
  if (piece < 0)
    return;
  auto &e = data_.elements[detail::index(
      data_.pieces[detail::index(piece, data_.pieces.size())],
      data_.elements.size())];
  e.flags = on ? (e.flags & 0xfffd) : (e.flags | 2);
}
void Level::set_piece_height(std::int64_t piece, Raw value) {
  if (piece < 0)
    return;
  data_
      .elements[detail::index(
          data_.pieces[detail::index(piece, data_.pieces.size())],
          data_.elements.size())]
      .z = to_int(value);
}
std::int64_t Level::get_piece_height(std::int64_t piece) const {
  if (piece < 0)
    return 0;
  return data_
      .elements[detail::index(
          data_.pieces[detail::index(piece, data_.pieces.size())],
          data_.elements.size())]
      .z;
}
namespace detail {
std::vector<std::size_t> collect(Level &level, Raw min_x, Raw min_y, Raw max_x,
                                 Raw max_y, std::int64_t limit, Work &work) {
  auto &d = Access::data(level);
  auto col0 =
      std::int64_t(s32(std::uint32_t(floor_int(mul(min_x, INV160)) - d.tg_x0)));
  auto row0 =
      std::int64_t(s32(std::uint32_t(floor_int(mul(min_y, INV160)) - d.tg_y0)));
  auto col1 =
      std::int64_t(s32(std::uint32_t(floor_int(mul(max_x, INV160)) - d.tg_x0)));
  auto row1 =
      std::int64_t(s32(std::uint32_t(floor_int(mul(max_y, INV160)) - d.tg_y0)));
  row0 = std::max(INT64_C(0), row0);
  row1 = std::min(d.tg_h - 1, row1);
  col0 = std::max(INT64_C(0), col0);
  col1 = std::min(d.tg_w - 1, col1);
  std::vector<std::size_t> out;
  if (col0 > col1 || row0 > row1)
    return out;
  for (auto row = row0; row <= row1; ++row)
    for (auto col = col0; col <= col1; ++col) {
      work.visit();
      for (auto ref : d.grid[std::size_t(row * d.tg_w + col)]) {
        work.visit();
        const auto i = index(ref, d.elements.size());
        auto &e = d.elements[i];
        if (e.stamp == d.stamp || (e.flags & 2))
          continue;
        if (lt(max_x, e.f_min_x) || lt(max_y, e.f_min_y) ||
            lt(e.f_max_x, min_x) || lt(e.f_max_y, min_y))
          continue;
        e.stamp = d.stamp;
        out.push_back(i);
        // Python limit<=1 never reaches its negative/zero stopping size.
        if (limit > 1 && out.size() == std::uint64_t(limit - 1))
          return out;
      }
    }
  return out;
}
Raw height(Level &level, Raw radius, const Vec3 &p, bool below, Work &work) {
  auto &d = Access::data(level);
  ++d.stamp;
  const auto found = collect(level, sub(p[0], radius), sub(p[1], radius),
                             add(p[0], radius), add(p[1], radius), 64, work);
  const auto r4 = mul(radius, QUARTER), r4sq = mul(r4, r4);
  const auto reach = ceil_int(r4);
  std::int64_t best = -1000000;
  for (auto idx : found) {
    const auto &e = d.elements[idx];
    const auto &hf = d.heightfields[index(e.hf, d.heightfields.size())];
    const auto top = hf.hi + e.z;
    if (!(best < top))
      continue;
    if (below && lt(add(p[2], STEP), integer(hf.lo + e.z)))
      continue;
    const auto [c, s] = rotation(e);
    const auto [lx, ly] = to_samples(e, c, s, p[0], p[1]);
    const auto i0 = floor_int(lx), j0 = floor_int(ly);
    const auto cols = span(i0 - reach, i0 + reach + 1, hf.x0, hf.w),
               rows = span(j0 - reach, j0 + reach + 1, hf.y0, hf.h);
    if (cols.first > cols.second || rows.first > rows.second)
      continue;
    bool done = false;
    for (auto j = rows.first; j <= rows.second && !done; ++j)
      for (auto i = cols.first; i <= cols.second; ++i) {
        work.visit();
        const auto h =
            sample_height(
                hf.samples[std::size_t((j - hf.y0) * hf.w + i - hf.x0)]) +
            e.z;
        if (!(best < h) || (below && lt(add(p[2], STEP), integer(h))))
          continue;
        const auto a = sub(integer(i), lx), b = sub(integer(j), ly);
        if (!lt(add(mul(a, a), mul(b, b)), r4sq))
          continue;
        best = h;
        if (!(best < top)) {
          done = true;
          break;
        }
      }
  }
  return integer(best);
}
} // namespace detail
std::vector<std::size_t> collect_elements(Level &l, Raw ax, Raw ay, Raw bx,
                                          Raw by, std::int64_t limit) {
  detail::Work w(l);
  return detail::collect(l, ax, ay, bx, by, limit, w);
}
Raw terrain_height(Level &l, Raw radius, const Vec3 &p) {
  detail::Work w(l);
  return detail::height(l, radius, p, false, w);
}
Raw terrain_height_below(Level &l, Raw radius, const Vec3 &p) {
  detail::Work w(l);
  return detail::height(l, radius, p, true, w);
}
Raw terrain_height_at_point(Level &l, const Vec3 &p) {
  using namespace detail;
  Work work(l);
  auto &d = Access::data(l);
  const auto col =
      std::int64_t(s32(std::uint32_t(floor_int(mul(p[0], INV160)) - d.tg_x0)));
  const auto row =
      std::int64_t(s32(std::uint32_t(floor_int(mul(p[1], INV160)) - d.tg_y0)));
  if (col < 0 || row < 0 || col >= d.tg_w || row >= d.tg_h)
    return ZERO;
  work.visit();
  std::int64_t best = -1000000;
  for (auto ref : d.grid[std::size_t(row * d.tg_w + col)]) {
    work.visit();
    const auto &e = d.elements[index(ref, d.elements.size())];
    if ((e.flags & 2) || lt(p[0], e.f_min_x) || lt(p[1], e.f_min_y) ||
        lt(e.f_max_x, p[0]) || lt(e.f_max_y, p[1]))
      continue;
    const auto [c, s] = rotation(e);
    const auto [lx, ly] = to_samples(e, c, s, p[0], p[1]);
    const auto &hf = d.heightfields[index(e.hf, d.heightfields.size())];
    const auto i = floor_int(lx) - hf.x0, j = floor_int(ly) - hf.y0;
    if (i < 0 || j < 0 || i >= hf.w || j >= hf.h)
      continue;
    work.visit();
    const auto h = sample_height(hf.samples[std::size_t(j * hf.w + i)]) + e.z;
    if (best < h)
      best = h;
  }
  return best < -100000 ? ZERO : integer(best);
}
Raw terrain_test_flag1(Level &l, Raw radius, const Vec3 &p) {
  using namespace detail;
  Work work(l);
  auto &d = Access::data(l);
  ++d.stamp;
  const auto found = collect(l, sub(p[0], radius), sub(p[1], radius),
                             add(p[0], radius), add(p[1], radius), 64, work);
  const auto r4 = mul(radius, QUARTER), r4sq = mul(r4, r4);
  const auto reach = ceil_int(r4);
  for (auto idx : found) {
    const auto &e = d.elements[idx];
    const auto &hf = d.heightfields[index(e.hf, d.heightfields.size())];
    if (!(-1000000 < hf.hi + e.z))
      continue;
    const auto [c, s] = rotation(e);
    const auto [lx, ly] = to_samples(e, c, s, p[0], p[1]);
    const auto i0 = floor_int(lx), j0 = floor_int(ly);
    const auto cols = span(i0 - reach, i0 + reach + 1, hf.x0, hf.w),
               rows = span(j0 - reach, j0 + reach + 1, hf.y0, hf.h);
    if (cols.first > cols.second || rows.first > rows.second)
      continue;
    for (auto j = rows.first; j <= rows.second; ++j)
      for (auto i = cols.first; i <= cols.second; ++i) {
        work.visit();
        if (!(std::uint16_t(
                  hf.samples[std::size_t((j - hf.y0) * hf.w + i - hf.x0)]) &
              1))
          continue;
        const auto a = sub(integer(i), lx), b = sub(integer(j), ly);
        if (lt(add(mul(a, a), mul(b, b)), r4sq))
          return ONE;
      }
  }
  return ZERO;
}
} // namespace champions::simulation::rta::world_queries
