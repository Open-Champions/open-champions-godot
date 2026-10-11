#pragma once
#include "champions/core_math.hpp"
#include "champions/world_queries.hpp"
#include <algorithm>
#include <utility>

namespace champions::simulation::rta::world_queries::detail {
struct Access {
  static LevelData &data(Level &level) { return level.data_; }
  static const Limits &limits(const Level &level) { return level.limits_; }
};
struct Work {
  std::size_t remaining;
  explicit Work(const Level &level)
      : remaining(Access::limits(level).query_work) {}
  void visit() {
    if (!remaining)
      throw ResourceLimit("terrain query work budget exceeded");
    --remaining;
  }
};
inline constexpr Raw QUARTER = 0x3e800000u, INV160 = 0x3bccccccu;
inline constexpr Raw INV32767 = 0x38000100u, EPS = 0x3a83126eu;
inline constexpr Raw MARGIN = 0x3c23d70au, STEP = 0x41400000u;
inline constexpr Raw DT = 0x3c888888u, GRAVITY = 0x410fffffu;
inline Raw integer(std::int64_t value) {
  return from_int(std::uint32_t(value));
}
inline std::int64_t floor_int(Raw value) { return to_int(floorf(value)); }
inline std::int64_t ceil_int(Raw value) { return to_int(ceilf(value)); }
inline std::int64_t sample_height(std::int16_t value) {
  const auto v = std::int64_t(value);
  return v >= 0 ? v / 16 : -((-v + 15) / 16);
}
inline std::size_t index(std::int64_t value, std::size_t size) {
  if (value < 0) {
    if (value < -std::int64_t(size))
      throw Error("terrain index out of range");
    return std::size_t(std::int64_t(size) + value);
  }
  if (std::uint64_t(value) >= size)
    throw Error("terrain index out of range");
  return std::size_t(value);
}
inline std::pair<std::int64_t, std::int64_t>
span(std::int64_t lo, std::int64_t hi, std::int64_t first, std::int64_t size) {
  return {std::max(lo, first), std::min(hi, first + size - 1)};
}
inline std::pair<Raw, Raw> rotation(const Element &e) {
  return {mul(integer(e.cos), INV32767), mul(integer(e.sin), INV32767)};
}
inline std::pair<Raw, Raw> to_samples(const Element &e, Raw c, Raw s, Raw px,
                                      Raw py) {
  const auto ux = sub(px, integer(e.x)), uy = sub(py, integer(e.y));
  const auto lx = add(mul(ux, c), mul(uy, s));
  const auto ly = (e.flags & 1) ? sub(mul(ux, s), mul(uy, c))
                                : add(mul(neg(ux), s), mul(uy, c));
  return {mul(lx, QUARTER), mul(ly, QUARTER)};
}
inline std::pair<Raw, Raw> direction(const Element &e, Raw c, Raw s, Raw dx,
                                     Raw dy) {
  return {add(mul(dx, c), mul(dy, s)), (e.flags & 1)
                                           ? sub(mul(dx, s), mul(dy, c))
                                           : add(mul(neg(dx), s), mul(dy, c))};
}
std::vector<std::size_t> collect(Level &, Raw, Raw, Raw, Raw, std::int64_t,
                                 Work &);
Raw height(Level &, Raw, const Vec3 &, bool, Work &);
SweepResult sweep(Level &, Raw, const Vec3 &, const Vec3 &, std::uint32_t, bool,
                  Work &);
SlideResult slide(Level &, Raw, const Vec3 &, const Vec3 &, std::uint32_t,
                  Work &);
GroundResult ground(Level &, Raw, const Vec3 &, Raw, Raw, Work &);
} // namespace champions::simulation::rta::world_queries::detail
