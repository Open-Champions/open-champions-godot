// Executed RTA cold-arrow timer and exploding-arrow light: research-combat-5
// f33d1a3, docs/spec/arrow-hit.md, tools/ref_arrow_hit.py.
#include "champions/combat_ctors.hpp"

namespace champions::simulation::rta::combat_ctors {
namespace {
A::Value iv(std::uint64_t x) { return std::int64_t(x & 0xffffffffu); }
Raw get(A::Model &m, Address a) { return Raw(m.mem.load(a, 4)); }
void w(A::Model &m, Address a, unsigned n, Raw v) { m.store(a, n, v); }
void word(A::Model &m, Address a, Raw v) { w(m, a, 4, v); }
}
void creature_set_effect_timer(A::Model &m, Address creature, Raw which,
                               Address source, Raw seconds) {
  if (which < 5) word(m, creature + 0x340 + 4 * which, seconds);
  if (lt(0, seconds)) {
    m.external_call("handle_clear", {iv(creature + 0x360)});
    if (source)
      m.external_call("handle_link", {iv(creature + 0x360), iv(source), iv(source)});
  }
}
void light_effect_ctor(A::Model &m, Address block, const Vec3 &position,
                       Address follow, Raw tag, const std::array<Raw, 12> &f) {
  m.external_call("GameObject_ctor", {iv(block), position, iv(0), iv(0)});
  constexpr Raw vt=0x50e198, sixty=0x42700000;
  const Address light=block+0x4c;
  word(m, block+0x48, vt);
  w(m, light+9, 1, 0);
  for (unsigned k=0;k<3;++k) word(m, block+0x7c+4*k, position[k]);
  const Raw inverse_square=div(ONE, mul(f[8], f[8]));
  word(m, light+0x28, inverse_square);
  word(m, light+0x2c, f[10]);
  for (unsigned base:{0xau,0x12u})
    for (unsigned k=0;k<4;++k)
      w(m, light+base+2*k, 2, Raw(to_int(f[k])));
  word(m, light+0x20, inverse_square);
  word(m, light+0x1c, f[8]);
  word(m, light+0x24, f[8]);
  w(m, light+8, 1, 0);
  m.external_call("light_link", {iv(light)});
  word(m, block+0x88, 0);
  word(m, block+0xc, get(m,block+0xc)|3);
  if (follow)
    m.external_call("handle_link", {iv(block+0x88),iv(follow),iv(follow)});
  const auto frames=to_int(mul(f[11],sixty));
  const auto n=frames>0?frames:1;
  for (unsigned k=0;k<4;++k) word(m,block+0x8c+4*k,f[k]);
  word(m,block+0xac,f[8]);
  word(m,block+0xb4,get(m,0x44fdcc)+Raw(n));
  const Raw inverse=div(ONE,from_int(n));
  word(m,block+0xb0,mul(sub(f[9],f[8]),inverse));
  for (unsigned k=0;k<4;++k)
    word(m,block+0x9c+4*k,mul(sub(f[4+k],f[k]),inverse));
  m.external_call("obj_runEveryFrame", {iv(block)});
  word(m,block+0xb8,tag);
}
} // namespace champions::simulation::rta::combat_ctors
