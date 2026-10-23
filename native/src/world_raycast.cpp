// Authored from the documented contract of the terrain ray query (research
// repository docs/spec/world-queries.md section 9.1 and tools/ref_world.py
// terrain_raycast, commit 328b072). Supplied post-fixup terrain only; no
// original data or executable instructions.
#include "world_queries_internal.hpp"

namespace champions::simulation::rta::world_queries {
namespace {
constexpr Raw SEGMENT = 0x43700000u;       // 240.0, the ray segment length
constexpr Raw MINUS_ONE = 0xbf800000u;     // -1.0, the no-hit result
constexpr Raw MINUS_EPS = 0xbc23d70au;     // -0.01
constexpr Raw MINUS_1E7 = 0xcb189680u;     // -1.0e7
constexpr Raw BEST_START = 0x51ba43b7u;    // 1.0e11
constexpr Raw BEST_LIMIT = 0x501502f9u;    // 1.0e10
constexpr int MAX_SEGMENTS = 1001;
constexpr int MAX_STEPS = 1001;
std::int64_t wrap32(std::int64_t value) {
  return std::int64_t(s32(std::uint32_t(value)));
}
} // namespace

Raw terrain_raycast(Level &level, Raw max_t, const Vec3 &origin,
                    const Vec3 &dir, Raw flags) {
  using namespace detail;
  Work work(level);
  auto &d = Access::data(level);
  auto ox = origin[0], oy = origin[1], oz = origin[2];
  auto dx = mul(dir[0], QUARTER), dy = mul(dir[1], QUARTER);
  auto dz = dir[2];
  const auto length = sqrt(add(mul(dx, dx), mul(dy, dy)));
  if (lt(length, MARGIN)) {
    // Vertical: a distance down to the cell's floor, not a parameter.
    if (!lt(dz, ZERO))
      return MINUS_ONE;
    return sub(oz, terrain_height_at_point(level, origin));
  }
  const auto limit = mul(max_t, length);
  const auto inv = div(ONE, length);
  ++d.stamp; // once for the whole ray
  Raw marched = ZERO;
  const std::uint32_t flag_bits = (flags & 0xffffu) << 16;
  dx = mul(dx, inv);
  dy = mul(dy, inv);
  dz = mul(dz, inv);
  if (!lt(ZERO, limit))
    return MINUS_ONE;
  for (int segment = 0; segment < MAX_SEGMENTS; ++segment) {
    const auto ex = add(ox, mul(dx, SEGMENT));
    const auto ey = add(oy, mul(dy, SEGMENT));
    const auto ez = add(oz, mul(dz, SEGMENT));
    const auto found = collect(level, fmin(ox, ex), fmin(oy, ey), fmax(ox, ex),
                               fmax(oy, ey), 256, work);
    auto best = BEST_START;
    const auto low_z = lt(dz, ZERO) ? MINUS_1E7 : oz;
    for (auto idx : found) {
      const auto &e = d.elements[idx];
      const auto &hf = d.heightfields[index(e.hf, d.heightfields.size())];
      if (!(flag_bits && (e.flags & 4)))
        if (lt(integer(hf.hi + e.z), low_z))
          continue;
      const auto [c, s] = rotation(e);
      const auto start = to_samples(e, c, s, ox, oy);
      auto lx = start.first, ly = start.second;
      auto z = sub(oz, integer(e.z));
      const auto dirs = direction(e, c, s, dx, dy);
      const auto dlx = dirs.first, dly = dirs.second;
      auto t = ZERO;
      // Clip the start of the march to the field, one side after the other.
      auto edge = integer(hf.x0);
      const auto advance = [&](Raw k, bool first) {
        t = first ? k : add(t, k);
        lx = add(lx, mul(dlx, k));
        ly = add(ly, mul(dly, k));
        z = add(z, mul(dz, k));
      };
      if (lt(lx, edge)) {
        if (le(dlx, MARGIN))
          continue;
        advance(div(sub(edge, lx), dlx), true);
      }
      edge = integer(hf.x0 + hf.w);
      if (lt(edge, lx)) {
        if (le(MINUS_EPS, dlx))
          continue;
        advance(div(sub(edge, lx), dlx), false);
      }
      edge = integer(hf.y0);
      if (lt(ly, edge)) {
        if (le(dly, MARGIN))
          continue;
        advance(div(sub(edge, ly), dly), false);
      }
      edge = integer(hf.y0 + hf.h);
      if (lt(edge, ly)) {
        if (le(MINUS_EPS, dly))
          continue;
        advance(div(sub(edge, ly), dly), false);
      }
      auto col = wrap32(std::int64_t(to_int(lx)) - hf.x0);
      auto row = wrap32(std::int64_t(to_int(ly)) - hf.y0);
      int outside = 0;
      for (int step = 1; step <= MAX_STEPS; ++step) {
        work.visit();
        if (col < 0 || row < 0 || !(col < hf.w) || !(row < hf.h)) {
          if (!(outside < 4))
            break;
          ++outside;
        } else {
          const auto sample =
              hf.samples[std::size_t(row * hf.w + col)];
          const std::uint32_t word = std::uint32_t(std::uint16_t(sample)) << 16;
          const auto height = from_int(
              std::uint32_t(sample_height(sample)) + (word & flag_bits));
          if (lt(z, height)) {
            if (lt(t, best))
              best = t;
            break;
          }
        }
        t = add(t, ONE);
        lx = add(lx, dlx);
        ly = add(ly, dly);
        col = wrap32(std::int64_t(to_int(lx)) - hf.x0);
        row = wrap32(std::int64_t(to_int(ly)) - hf.y0);
        z = add(z, dz);
      }
    }
    if (lt(best, BEST_LIMIT)) {
      const auto total = add(best, mul(marched, QUARTER));
      return lt(total, limit) ? mul(total, inv) : MINUS_ONE;
    }
    marched = add(marched, SEGMENT);
    ox = ex;
    oy = ey;
    oz = ez;
    if (!lt(mul(marched, QUARTER), limit))
      return MINUS_ONE;
  }
  return MINUS_ONE;
}
} // namespace champions::simulation::rta::world_queries
