// Executed gold pile and HUD credit bodies, research-combat-ctors-4 6baf4cd,
// docs/spec/gold-run.md sections 4-6, tools/ref_gold.py.
#include "champions/combat_ctors.hpp"
#include "champions/core_math.hpp"

namespace champions::simulation::rta::combat_ctors {
namespace {
using A::Address;
using A::Value;
Value iv(std::uint64_t n) { return std::int64_t(n&0xffffffffu); }
struct B {
  A::Model &m;
  Raw u8(Address a) const { return Raw(m.mem.load(a,1)); }
  Raw u16(Address a) const { return Raw(m.mem.load(a,2)); }
  Raw u32(Address a) const { return Raw(m.mem.load(a,4)); }
  Vec3 vec(Address a) const { return {u32(a),u32(a+4),u32(a+8)}; }
  void w8(Address a,Raw x) { m.store(a,1,x); }
  void w32(Address a,Raw x) { m.store(a,4,x); }
  void wvec(Address a,const Vec3 &v) { for (unsigned k=0;k<3;++k) w32(a+4*k,v[k]); }
  Value call(const char *name,std::vector<Value> args) { return m.external_call(name,std::move(args)); }
  static Address floater(unsigned k) { return 0x86a900+0x24*k; }
  static Address record(Raw slot) { return 0x4d8af0+0x4c30*slot; }
  void credit(Address at,Raw amount) {
    auto sum=Raw(u32(at+0xe4)+amount);
    w32(at+0xe4,sum);
    if (s32(sum)>99999999) w32(at+0xe4,99999999);
  }
};
}

void gold_run(A::Model &m,Address gold) {
  B b{m};
  if (!std::int8_t(b.u8(gold+0x4c))) return;
  if (b.u32(gold+0x50)==Raw(b.u32(0x44fdcc)-18)) {
    auto entry=b.call("res_find",{iv(0x532478),iv(0x532838)});
    b.call("sound_playOnObject",{entry,iv(gold),iv(ONE)});
  }
  b.call("anim_advanceUnitSpeed",{iv(gold+0x58),iv(gold+0x10),iv(0),iv(0)});
  if (std::int8_t(b.u8(gold+0x4c))) b.w8(gold+0x4c,b.u8(gold+0x4c)-1);
  if (b.u16(gold+0x5e)&0x80) {
    b.w8(gold+0x4c,0);
    b.call("obj_stopRunning",{iv(gold)});
  }
}

void hud_add_gold_floater(A::Model &m,Raw slot,const Vec3 &position,Raw amount) {
  B b{m};
  unsigned k=0;
  while (k<8&&b.u32(B::floater(k)+0x1c)) ++k;
  if (k==8) { b.credit(B::record(slot),amount);return; }
  auto e=B::floater(k);
  b.w32(e+0x1c,amount);
  b.w8(e+0x20,slot);
  b.w32(e+0x18,b.u32(0x44fdcc));
  b.w8(e+0x21,0);
  Mat4 view{};
  for (unsigned j=0;j<16;++j) view[j]=b.u32(0x44f140+4*j);
  auto transformed=mat4_transform_point(view,position);
  const auto &p=transformed.point;
  auto w=transformed.w;
  if (lt(w,0x41a00000)||lt(mul(w,0x442f0000),fabs(p[0]))||
      lt(mul(w,0x43960000),fabs(p[1]))) b.w8(e+0x21,1);
  auto inv=div(ONE,w);
  auto sx=add(mul(mul(p[0],inv),HALF),0x43a00000);
  auto sy=add(mul(p[1],inv),0x43700000);
  b.wvec(e,{sx,sy,mul(p[2],inv)});
  b.w32(e+8,ZERO);
  auto tx=from_int(b.u32(0x4f6730+4*slot));
  auto ty=from_int(b.u32(0x4f6710+4*slot));
  auto dx=sub(tx,sx),dy=sub(ty,sy),dz=neg(ZERO);
  auto r=rsqrt(ONE,add(add(mul(dx,dx),mul(dy,dy)),mul(dz,dz)));
  dx=mul(dx,r);dy=mul(dy,r);
  auto seed=Raw(std::uint64_t(b.u32(0x404460))*0x19660d+0x3c6ef35f);
  b.w32(0x404460,seed);
  auto spread=mul(sub(mul(from_int(seed>>16),TWO_M16),HALF),0x41100000);
  b.w32(e+0xc,mul(dy,spread));
  b.w32(e+0x10,mul(neg(dx),spread));
  b.w32(e+0x14,ZERO);
}

void gold_on_touch(A::Model &m,Address gold,Address toucher) {
  B b{m};
  auto flags=b.u32(toucher+0xc);
  if ((flags&0x20000)||std::int8_t(b.u8(gold+0x4c))||!(flags&0x40)) return;
  auto amount=b.u32(gold+0x80);
  if (!amount) return;
  auto entry=b.call("res_find",{iv(0x532478),iv(0x532820)});
  b.call("sound_playOnObject",{entry,iv(gold),iv(ONE)});
  hud_add_gold_floater(m,b.u32(toucher+0x3c8),b.vec(gold+0x10),amount);
  b.w32(gold+0x80,0);
  b.call("obj_delete",{iv(gold)});
}

void hud_tick_gold(A::Model &m) {
  B b{m};auto frame=b.u32(0x44fdcc);
  for (unsigned k=0;k<8;++k) {
    auto e=B::floater(k);
    auto amount=b.u32(e+0x1c);
    if (!amount) continue;
    auto elapsed=Raw(frame-b.u32(e+0x18));
    if (s32(Raw(3*elapsed))<128) continue;
    auto slot=std::int32_t(std::int8_t(b.u8(e+0x20)));
    auto pending=Address(0x86ace8+4*slot);
    b.w32(e+0x1c,0);
    b.w32(pending,b.u32(pending)+amount);
  }
  for (unsigned slot=0;slot<4;++slot) {
    auto at=Address(0x86ace8+4*slot);
    auto p=s32(b.u32(at));
    if (p<=0) continue;
    auto step=Raw(s32(Raw(p+7))>>3);
    b.credit(B::record(slot),step);
    b.w32(at,Raw(p-step));
  }
}

void hud_flush_gold(A::Model &m) {
  B b{m};
  for (unsigned k=0;k<8;++k) {
    auto e=B::floater(k);
    auto amount=b.u32(e+0x1c);
    if (!amount) continue;
    auto slot=Raw(std::int32_t(std::int8_t(b.u8(e+0x20))));
    b.credit(B::record(slot),amount);
    b.w32(e+0x1c,0);
  }
  for (unsigned slot=0;slot<4;++slot) {
    auto at=Address(0x86ace8+4*slot);
    auto p=b.u32(at);
    if (s32(p)<=0) continue;
    b.credit(B::record(slot),p);
    b.w32(at,0);
  }
  for (unsigned slot=0;slot<4;++slot) {
    b.w32(0x4f6950+4*slot,0);
    b.w32(0x86acd8+4*slot,0);
  }
}
} // namespace champions::simulation::rta::combat_ctors
