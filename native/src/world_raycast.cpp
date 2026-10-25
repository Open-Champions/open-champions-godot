// terrain_raycast 0x18a5c0, authored from the executed ref_world contract
// (research-arena-match 5de93b5, docs/spec/arena-match-probe.md round 10).
#include "world_queries_internal.hpp"

namespace champions::simulation::rta::world_queries {
namespace {
constexpr Raw NEG_ONE = 0xbf800000u, EPSILON = 0x3c23d70au;
constexpr Raw NEG_EPSILON = 0xbc23d70au, TWO_FORTY = 0x43700000u;
constexpr Raw LARGE = 0x51ba43b7u, HUGE_ = 0x501502f9u;
constexpr Raw LOW = 0xcb189680u;
Raw minf(Raw a, Raw b) { return lt(a, b) ? a : b; }
Raw maxf(Raw a, Raw b) { return lt(a, b) ? b : a; }
bool clip(Raw edge, Raw component, Raw delta, bool lower, Raw &t,
          Raw &x, Raw &y, Raw &z, Raw dx, Raw dy, Raw dz) {
  if (lower ? !lt(component, edge) : !lt(edge, component)) return true;
  if (lower ? le(delta, EPSILON) : le(NEG_EPSILON, delta)) return false;
  const auto k = div(sub(edge, component), delta);
  t = add(t, k);
  x = add(x, mul(dx, k));
  y = add(y, mul(dy, k));
  z = add(z, mul(dz, k));
  return true;
}
} // namespace

Raw terrain_raycast(Level &level, Raw max_t, const Vec3 &origin,
                    const Vec3 &direction, std::uint32_t flags) {
  using namespace detail;
  Work work(level);
  auto &data = Access::data(level);
  auto ox = origin[0], oy = origin[1], oz = origin[2];
  auto dx = mul(direction[0], QUARTER), dy = mul(direction[1], QUARTER);
  auto dz = direction[2];
  const auto length = sqrt(add(mul(dx, dx), mul(dy, dy)));
  if (lt(length, EPSILON)) {
    if (!lt(dz, ZERO)) return NEG_ONE;
    return sub(oz, terrain_height_at_point(level, origin));
  }
  const auto limit = mul(max_t, length), inv = div(ONE, length);
  ++data.stamp;
  Raw marched = ZERO;
  const auto flag_bits = Raw(flags & 0xffffu) << 16;
  dx = mul(dx, inv); dy = mul(dy, inv); dz = mul(dz, inv);
  if (!lt(ZERO, limit)) return NEG_ONE;
  for (unsigned segment = 0; segment < 1001; ++segment) {
    work.visit();
    const auto ex = add(ox, mul(dx, TWO_FORTY));
    const auto ey = add(oy, mul(dy, TWO_FORTY));
    const auto ez = add(oz, mul(dz, TWO_FORTY));
    const auto found = collect(level, minf(ox, ex), minf(oy, ey),
                               maxf(ox, ex), maxf(oy, ey), 256, work);
    Raw best = LARGE;
    const auto low_z = lt(dz, ZERO) ? LOW : oz;
    for (auto index : found) {
      work.visit();
      const auto &e = data.elements[index];
      const auto &hf = data.heightfields[detail::index(e.hf, data.heightfields.size())];
      if (!(flag_bits && (e.flags & 4)) && lt(integer(hf.hi + e.z), low_z))
        continue;
      const auto [c, s] = rotation(e);
      auto [x, y] = to_samples(e, c, s, ox, oy);
      auto z = sub(oz, integer(e.z));
      const auto [vx, vy] = detail::direction(e, c, s, dx, dy);
      Raw t = ZERO;
      if (!clip(integer(hf.x0), x, vx, true, t, x, y, z, vx, vy, dz) ||
          !clip(integer(hf.x0 + hf.w), x, vx, false, t, x, y, z, vx, vy, dz) ||
          !clip(integer(hf.y0), y, vy, true, t, x, y, z, vx, vy, dz) ||
          !clip(integer(hf.y0 + hf.h), y, vy, false, t, x, y, z, vx, vy, dz))
        continue;
      auto col = std::int64_t(s32(Raw(std::int64_t(to_int(x)) - hf.x0)));
      auto row = std::int64_t(s32(Raw(std::int64_t(to_int(y)) - hf.y0)));
      unsigned outside = 0;
      for (unsigned step = 1; step <= 1001; ++step) {
        work.visit();
        if (col < 0 || row < 0 || col >= hf.w || row >= hf.h) {
          if (outside >= 4) break;
          ++outside;
        } else {
          const auto word = Raw(std::uint16_t(hf.samples[std::size_t(row * hf.w + col)])) << 16;
          const auto height = s32(Raw(s32(word) >> 20) + (word & flag_bits));
          if (lt(z, integer(height))) {
            if (lt(t, best)) best = t;
            break;
          }
        }
        t = add(t, ONE);
        x = add(x, vx); y = add(y, vy); z = add(z, dz);
        col = std::int64_t(s32(Raw(std::int64_t(to_int(x)) - hf.x0)));
        row = std::int64_t(s32(Raw(std::int64_t(to_int(y)) - hf.y0)));
      }
    }
    if (lt(best, HUGE_)) {
      const auto total = add(best, mul(marched, QUARTER));
      return lt(total, limit) ? mul(total, inv) : NEG_ONE;
    }
    marched = add(marched, TWO_FORTY);
    ox = ex; oy = ey; oz = ez;
    if (!lt(mul(marched, QUARTER), limit)) return NEG_ONE;
  }
  return NEG_ONE;
}
} // namespace champions::simulation::rta::world_queries
