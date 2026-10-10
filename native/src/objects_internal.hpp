#pragma once
#include "champions/objects.hpp"
#include <algorithm>
#include <limits>
namespace champions::simulation::rta::objects::detail {
inline void bounded(std::size_t n, std::size_t cap = MAX_BYTES) {
  if (n > cap)
    throw ResourceLimit("object runtime capacity exceeded");
}
inline std::int64_t step(std::int64_t n, int delta) {
  if ((delta > 0 && n == INT64_MAX) || (delta < 0 && n == INT64_MIN))
    throw ResourceLimit("object counter overflow");
  return n + delta;
}
inline std::int32_t asr(std::uint32_t n, unsigned shift) {
  return s32((n >> shift) |
             ((n & 0x80000000U) ? (~std::uint32_t(0) << (32 - shift)) : 0));
}
inline std::string lower(std::string s) {
  bounded(s.size());
  for (auto &c : s)
    if (c >= 'A' && c <= 'Z')
      c = char(c + 32);
  return s;
}
inline std::size_t mod(std::int64_t n, unsigned d) {
  auto s = s32(std::uint32_t(n));
  if (s < 0)
    throw ObjectError("negative frame indexes before run lists");
  return std::uint32_t(s) % d;
}
inline void check_props(const Props &p) {
  if (!p)
    return;
  bounded(p->size(), MAX_RETAINED);
  std::size_t n = 0;
  for (const auto &s : *p) {
    bounded(s.size(), MAX_BYTES - n);
    n += s.size();
  }
}
inline void budget(std::size_t &n) {
  if (++n > MAX_WORK)
    throw ResourceLimit("object traversal budget exceeded");
}
} // namespace champions::simulation::rta::objects::detail
