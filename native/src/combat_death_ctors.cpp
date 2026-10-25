// Authored from research-combat-ctors-3 9ff6ae0, combat-death.md section 3
// and ref_combat_death.py's executed Skeleton_shatter body.
#include "champions/combat_ctors.hpp"
#include <array>

namespace champions::simulation::rta::combat_ctors {
namespace {
using A::Value;
constexpr Address MASKS=0x8e44c0;
constexpr std::uint64_t GROUP1[]={UINT64_C(0xfe000000000000),UINT64_C(0xc0)};
constexpr std::uint64_t GROUP2[]={UINT64_C(0x80000),UINT64_C(0x100000),UINT64_C(0x200000),UINT64_C(0x1000001000000000)};
constexpr std::uint64_t GROUP3[]={UINT64_C(0x10000),UINT64_C(0x20000),UINT64_C(0x40000),UINT64_C(0x2000002000000000)};
constexpr std::uint64_t GROUP4[]={UINT64_C(0x2),UINT64_C(0x4000),UINT64_C(0x8000)};
constexpr std::uint64_t GROUP5[]={UINT64_C(0x1),UINT64_C(0x1000),UINT64_C(0x2000)};
constexpr std::uint64_t GROUP8[]={UINT64_C(0xc0),UINT64_C(0x338),UINT64_C(0xc04),UINT64_C(0x10000),UINT64_C(0x20000),UINT64_C(0x80000),UINT64_C(0x100000),UINT64_C(0x1),UINT64_C(0x2),UINT64_C(0x1000),UINT64_C(0x4000),UINT64_C(0x40000),UINT64_C(0x200000),UINT64_C(0x8000),UINT64_C(0x2000),UINT64_C(0xfe000000000000)};
template <std::size_t N> void write_group(A::Model &m,const std::uint64_t (&data)[N]) {
  for (std::size_t k=0;k<N;++k) m.store(MASKS+8*k,8,data[k]);
  m.store(MASKS+8*N,8,0);
}
Value iv(std::uint64_t x) {return std::int64_t(x&0xffffffffu);}
std::int64_t number(const Value &v) {
  if (auto p=std::get_if<std::int64_t>(&v)) return *p;
  if (auto p=std::get_if<A::CallbackResult>(&v))
    if (p->kind==A::CallbackResult::Kind::integer)
      return p->negative ? -std::int64_t(p->magnitude) : std::int64_t(p->magnitude);
  throw A::Error("Skeleton_shatter pool result required");
}
}
void skeleton_shatter(A::Model &m,Address c,Raw group) {
  auto u32=[&](Address a){return Raw(m.mem.load(a,4));};
  auto s16m=[&](Address a){return s16(Raw(m.mem.load(a,2)));};
  m.store(MASKS,8,0);
  switch (s32(group)) {
  case 1:write_group(m,GROUP1);break;
  case 2:write_group(m,GROUP2);break;
  case 3:write_group(m,GROUP3);break;
  case 4:write_group(m,GROUP4);break;
  case 5:write_group(m,GROUP5);break;
  case 8:write_group(m,GROUP8);break;
  default:break;
  }
  auto show=m.mem.load(c+0x220,8);
  for (Address k=0;;++k) {
    auto mask=m.mem.load(MASKS+8*k,8);
    if (!mask) break;
    auto part=show&mask;
    if (part) {
      auto block=Raw(number(m.external_call("pool_new",{iv(0x248)})));
      if (block) {
        auto def=u32(c+0x24);
        Vec3 pos{u32(c+0x10),u32(c+0x14),u32(c+0x18)};
        m.external_call("PhProp_ctor2",{iv(ONE),iv(block),pos,iv(Raw(s16m(c+0x38))),
          iv(u32(def+0xc)),iv(c+0x114),iv(u32(def)),iv(u32(def+8)),iv(u32(def+0x10)),
          Value(std::int64_t(part)),iv(0x107),iv(0)});
      }
      show&=~mask;
    }
  }
  m.store(c+0x220,8,show);
}
} // namespace champions::simulation::rta::combat_ctors
