// Authored from the published ref_world.py contract, commit
// 6831458ee7ee12bb166d1dce4eb8e4340095bc48, SHA256
// 30366cb77918630241cd036b20df6cd86c4869be453c4160bd20c99b786ea4b8.
// Terrain only: no nearby-object contact or player input policy is supplied.
#include "world_queries_internal.hpp"

namespace champions::simulation::rta::world_queries {
namespace detail {
SweepResult sweep(Level &level, Raw radius, const Vec3 &p, const Vec3 &delta,
                  std::uint32_t mask, bool want_normal, Work &work) {
  auto &d = Access::data(level);
  const auto ex = add(p[0], delta[0]), ey = add(p[1], delta[1]);
  ++d.stamp;
  const auto found = collect(
      level, sub(fmin(p[0], ex), radius), sub(fmin(p[1], ey), radius),
      add(fmax(p[0], ex), radius), add(fmax(p[1], ey), radius), 256, work);
  const auto sx = mul(delta[0], QUARTER), r4 = mul(radius, QUARTER),
             r4sq = mul(r4, r4);
  const auto sy = mul(delta[1], QUARTER),
             length = sqrt(add(mul(sx, sx), mul(sy, sy)));
  const auto inv = div(ONE, length), dx = mul(sx, inv), dy = mul(sy, inv),
             dz = mul(delta[2], inv);
  const auto reach = ceil_int(r4),
             base = floor_int(fmin(p[2], add(p[2], delta[2]))) + 12;
  auto best = length;
  Raw hit_x = ZERO, hit_y = ZERO;
  const auto margin = add(r4, MARGIN);
  for (auto idx : found) {
    const auto &e = d.elements[idx];
    const auto &hf = d.heightfields[index(e.hf, d.heightfields.size())];
    if (!(e.flags & 4) && hf.hi + e.z < base)
      continue;
    const auto [c, s] = rotation(e);
    const auto [lx, ly] = to_samples(e, c, s, p[0], p[1]);
    const auto [dlx, dly] = direction(e, c, s, dx, dy);
    const auto i0 = floor_int(lx), j0 = floor_int(ly);
    const auto i1 = floor_int(add(lx, mul(dlx, length))),
               j1 = floor_int(add(ly, mul(dly, length)));
    const auto cols = span(std::min(i0, i1) - reach,
                           std::max(i0, i1) + reach + 1, hf.x0, hf.w);
    const auto rows = span(std::min(j0, j1) - reach,
                           std::max(j0, j1) + reach + 1, hf.y0, hf.h);
    if (cols.first > cols.second || rows.first > rows.second)
      continue;
    for (auto j = rows.first; j <= rows.second; ++j) {
      std::optional<Raw> b;
      for (auto i = cols.first; i <= cols.second; ++i) {
        work.visit();
        const auto sample =
            hf.samples[std::size_t((j - hf.y0) * hf.w + i - hf.x0)];
        const auto h = (std::uint32_t(std::int32_t(sample)) & mask)
                           ? INT64_C(32000)
                           : sample_height(sample) + e.z;
        if (h < base)
          continue;
        const auto a = sub(integer(i), lx);
        if (!b)
          b = sub(integer(j), ly);
        const auto perp = sub(mul(a, dly), mul(*b, dlx));
        if (lt(margin, fabs(perp)))
          continue;
        const auto along = add(mul(a, dlx), mul(*b, dly));
        if (lt(along, ZERO))
          continue;
        if (lt(integer(h), add(add(p[2], mul(along, dz)), STEP)))
          continue;
        const auto q = sub(r4sq, mul(perp, perp));
        const auto time = lt(ZERO, q) ? sub(along, sqrt(q)) : sub(along, ZERO);
        if (!lt(time, best))
          continue;
        best = time;
        // Python shifts its unbounded signed coordinates before i2f wraps32.
        const auto wx = integer(i * 4), wy = integer(j * 4);
        if (e.flags & 1) {
          hit_y = add(sub(mul(wx, s), mul(wy, c)), integer(e.y));
          hit_x = add(add(mul(wx, c), mul(wy, s)), integer(e.x));
        } else {
          hit_y = add(add(mul(wx, s), mul(wy, c)), integer(e.y));
          hit_x = add(sub(mul(wx, c), mul(wy, s)), integer(e.x));
        }
      }
    }
  }
  if (le(length, best))
    return {ONE, {}};
  std::optional<Vec3> normal;
  if (want_normal) {
    const auto nx = sub(add(p[0], mul(dx, best)), hit_x),
               ny = sub(add(p[1], mul(dy, best)), hit_y);
    const auto k = rsqrt(ONE, add(mul(nx, nx), mul(ny, ny)));
    normal = Vec3{mul(nx, k), mul(ny, k), ZERO};
  }
  return {mul(best, inv), normal};
}
SlideResult slide(Level &level, Raw radius, const Vec3 &position,
                  const Vec3 &delta, std::uint32_t mask, Work &work) {
  auto d = delta, p = position;
  Raw remain = ONE;
  int hit = 0;
  std::optional<Vec3> normal;
  for (unsigned iteration = 0; iteration < 11; ++iteration) {
    const Vec3 step{mul(d[0], remain), mul(d[1], remain), mul(d[2], remain)};
    const auto result = sweep(level, radius, p, step, mask, true, work);
    if (result.normal)
      normal = result.normal;
    remain = sub(remain, result.time);
    for (unsigned i = 0; i < 3; ++i)
      p[i] = add(p[i], mul(step[i], result.time));
    if (!lt(EPS, remain))
      break;
    hit = 1;
    if (!normal)
      throw Error("slide: more than 0.001 remains but no sweep was blocked");
    const auto k =
        sub(EPS, add(add(mul((*normal)[0], d[0]), mul((*normal)[1], d[1])),
                     mul((*normal)[2], d[2])));
    if (lt(ZERO, k))
      for (unsigned i = 0; i < 3; ++i)
        d[i] = add(d[i], mul((*normal)[i], k));
  }
  return {hit, p, normal};
}
GroundResult ground(Level &level, Raw radius, const Vec3 &pos, Raw vz,
                    Raw stand, Work &work) {
  auto floor = height(level, radius, pos, true, work);
  if (lt(floor, stand))
    floor = stand;
  vz = sub(vz, GRAVITY);
  auto z = add(pos[2], mul(vz, DT));
  int landed = 0;
  if (le(sub(z, MARGIN), floor)) {
    z = floor;
    if (lt(vz, ZERO)) {
      vz = ZERO;
      landed = 1;
    }
  }
  return {{pos[0], pos[1], z}, vz, landed};
}
} // namespace detail
SweepResult terrain_sweep_circle(Level &l, Raw radius, const Vec3 &pos,
                                 const Vec3 &delta, std::uint32_t mask,
                                 bool want_normal) {
  detail::Work work(l);
  return detail::sweep(l, radius, pos, delta, mask, want_normal, work);
}
SlideResult world_slide_move(Level &l, Raw radius, const Vec3 &pos,
                             const Vec3 &delta, std::uint32_t mask) {
  detail::Work work(l);
  return detail::slide(l, radius, pos, delta, mask, work);
}
std::uint32_t mover_mask(std::uint32_t flags) noexcept {
  auto mask = (flags & 0x20) ? 3u : 1u;
  if (flags & 0x40)
    mask = (mask | 4u) & 5u;
  return mask;
}
GroundResult ground_step(Level &l, Raw radius, const Vec3 &pos, Raw vz,
                         Raw stand) {
  detail::Work work(l);
  return detail::ground(l, radius, pos, vz, stand, work);
}
MoveResult object_move_isolated(Level &l, Raw radius, const Vec3 &pos,
                                const Vec3 &velocity, Raw vz,
                                std::uint32_t flags) {
  using namespace detail;
  Work work(l);
  const Vec3 d{mul(velocity[0], DT), mul(velocity[1], DT),
               mul(velocity[2], DT)};
  auto p = pos;
  int bits = 0;
  if (lt(EPS, add(add(mul(d[0], d[0]), mul(d[1], d[1])), mul(d[2], d[2])))) {
    const auto result = slide(l, radius, p, d, mover_mask(flags), work);
    p = result.position;
    bits = result.hit ? 1 : 0;
  }
  const auto result = ground(l, radius, p, vz, NO_STAND, work);
  if (result.landed)
    bits |= 4;
  return {result.position, result.vz, bits};
}
} // namespace champions::simulation::rta::world_queries
