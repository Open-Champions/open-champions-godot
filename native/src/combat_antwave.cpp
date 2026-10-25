// AntWave class, research-combat-ctors-4 6baf4cd, docs/spec/antwave.md and
// tools/ref_antwave.py. Base Creature_run and moveTo use the existing model.
#include "champions/combat_ctors.hpp"

namespace champions::simulation::rta::combat_ctors {
namespace {
A::Value iv(std::uint64_t v) { return std::int64_t(v&0xffffffffu); }
struct B {
  A::Model &m;
  Raw u32(Address a) const {return Raw(m.mem.load(a,4));}
  Vec3 vec(Address a) const {return {u32(a),u32(a+4),u32(a+8)};}
  void w32(Address a,Raw v) {m.store(a,4,v);}
  void run(Address c) {
    if (m.callbacks.count("AntWave_Creature_run"))
      m.external_call("AntWave_Creature_run",{iv(c),iv(0)});
    else m.creature_run(c,0);
  }
  void move(Address c,const Vec3 &to) {
    if (m.callbacks.count("AntWave_Creature_moveTo"))
      m.external_call("AntWave_Creature_moveTo",{iv(c),to});
    else m.move_to(c,to);
  }
  Address target(Address c) const {
    auto node=u32(c+0x3d0);
    return node?u32(node+0xc):0;
  }
};
}
void antwave_ctor(A::Model &m,Address c,Address target,const Vec3 &position,
                  Raw facing,Raw lifetime) {
  B b{m};
  m.creature_ctor(c,0,position,Raw(s16(facing)),0,0,0x461c4000);
  b.w32(c+0xb0,0x3f199999);
  b.w32(c+0xb8,0x42c80000);
  b.w32(c+0x1c,0x40a00000);
  auto radius=add(b.u32(target+0x1c),0x41200000);
  auto clock=b.u32(c+0x300);
  b.w32(c+0x48,0x50d298);
  b.w32(c+0x3c8,clock);
  b.w32(c+0x3cc,clock+lifetime);
  b.w32(c+0x1c,radius);
  b.w32(c+0x3d0,0);
  b.w32(c+0xc,b.u32(c+0xc)|1);
  m.external_call("handle_link",{iv(c+0x3d0),iv(target),iv(target)});
  m.external_call("obj_runEveryFrame",{iv(c)});
}
void antwave_dtor(A::Model &m,Address c,Raw flags) {
  B b{m};b.w32(c+0x48,0x50d298);
  if (b.u32(c+0x3d0)) m.external_call("handle_clear",{iv(c+0x3d0)});
  m.external_call("handle_clear",{iv(c+0x3d0)});
  creature_dtor(m,c,0);
  if (flags&1) m.external_call("pool_free",{iv(c)});
}
void antwave_run(A::Model &m,Address c) {
  B b{m};b.run(c);
  auto target=b.target(c);
  if (!target) {m.vcall(c,1,{iv(3)});return;}
  b.move(c,b.vec(target+0x10));
  if (s32(b.u32(c+0x300))%60==0) {
    target=b.target(c);
    Vec3 d{};
    for (unsigned k=0;k<3;++k)
      d[k]=sub(b.u32(target+0x10+4*k),b.u32(c+0x10+4*k));
    auto distance=add(add(mul(d[0],d[0]),mul(d[1],d[1])),mul(d[2],d[2]));
    auto radius=b.u32(c+0x1c);
    if (lt(distance,mul(radius,radius)))
      m.vcall(target,2,{iv(c),iv(0x100),iv(ONE)});
  }
  if (s32(b.u32(c+0x3cc))<s32(b.u32(c+0x300)))
    m.vcall(c,1,{iv(3)});
}
void antwave_draw(A::Model &m,Address c) {
  B b{m};
  if (s32(b.u32(c+0x300))%3) return;
  Vec3 ball{};
  do {
    for (auto &v:ball) {
      auto f=mul(from_int(m.own_draw(c)),TWO_M16);
      v=sub(add(f,f),ONE);
    }
  } while (lt(ONE,add(add(mul(ball[0],ball[0]),mul(ball[1],ball[1])),
                         mul(ball[2],ball[2]))));
  auto pos=b.vec(c+0x10);
  for (unsigned k=0;k<3;++k) pos[k]=add(mul(ball[k],0x41200000),pos[k]);
  m.external_call("particle_spawn",{iv(0x4febe0),pos,Vec3{},iv(ZERO)});
}
} // namespace champions::simulation::rta::combat_ctors
