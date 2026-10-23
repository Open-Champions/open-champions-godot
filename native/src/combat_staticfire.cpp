// Executed RTA fire constructor chain: research-combat-5 f33d1a3,
// docs/spec/staticfire-ctor.md, tools/ref_staticfire.py.
#include "champions/combat_ctors.hpp"
#include <limits>

namespace champions::simulation::rta::combat_ctors {
namespace {
using V=A::Value;
V iv(std::uint64_t x) { return std::int64_t(x&0xffffffffu); }
Raw number(const V &v) {
  if (auto p=std::get_if<std::int64_t>(&v)) return Raw(*p);
  if (auto p=std::get_if<A::CallbackResult>(&v))
    if (p->kind==A::CallbackResult::Kind::integer)
      return Raw(p->negative?-std::int64_t(p->magnitude):std::int64_t(p->magnitude));
  throw A::Error("StaticFire integer callback required");
}
struct B {
  A::Model &m;
  Raw u(Address a,unsigned n=4) const {return Raw(m.mem.load(a,n));}
  int sh(Address a) const {return s16(u(a,2));}
  int sb(Address a) const {auto x=u(a,1);return x&128?int(x)-256:int(x);}
  Vec3 vec(Address a) const {return {u(a),u(a+4),u(a+8)};}
  void w(Address a,unsigned n,Raw v) {m.store(a,n,v);}
  void w(Address a,Raw v) {w(a,4,v);}
  void wvec(Address a,const Vec3 &v) {
    for (unsigned k=0;k<3;++k) w(a+4*k,v[k]);
  }
  V call(const char *name,std::vector<V> args) {return m.external_call(name,std::move(args));}
};
}
void staticfire_push(A::Model &m, Address fire, Raw facing) {
  B b{m};
  auto c=cos16(facing),s=sin16(facing);
  auto inv=rsqrt(ONE,add(mul(c,c),mul(s,s)));
  b.w(fire+0x74,2,Raw(to_int(mul(mul(s,inv),0x406eeeee))));
  b.w(fire+0x72,2,Raw(to_int(mul(mul(c,inv),0x406eeeee))));
  auto draw=m.own_draw(fire)&3;
  int index=b.sh(fire+0x7a),stop=b.sh(fire+0x78),size=b.sh(fire+0x76);
  if (index==stop) return;
  if (size<=0 || size>250 || index<0 || index>=size || stop<0 || stop>=size)
    throw A::Error("StaticFire particle ring bounds");
  for (int count=0;count<size && index!=stop;++count) {
    auto entry=b.u(fire+0x4a4)+Address(10*index);
    if ((index&3)==int(draw)) {
      b.w(entry+6,2,b.u(entry+6,2)+b.u(fire+0x72,2));
      b.w(entry+8,2,b.u(entry+8,2)+b.u(fire+0x74,2));
    }
    ++index;if (index==size) index=0;
  }
}
void staticfire_init_common(A::Model &m, Address fire) {
  B b{m};
  b.w(fire+0x1c,0x41800000);b.w(fire+0x38,2,0);b.w(fire+0x28,1,60);
  b.w(fire+0x78,2,0);b.w(fire+0x7a,2,0);
  auto draw=m.own_draw(fire);
  b.w(fire+0x68,0x46c35000);b.w(fire+0xc,b.u(fire+0xc)|0x40001);
  b.w(fire+0x7c,0xffffffff);b.w(fire+0x54,ONE);b.w(fire+0x64,ONE);
  b.w(fire+0x6c,1,0);b.w(fire+0x4c,from_int(draw%300));
  staticfire_push(m,fire,Raw(b.sh(fire+0x38)));
  b.call("obj_runEveryFrame",{iv(fire)});
  b.call("obj_addToAlwaysDraw",{iv(fire)});
  b.call("path_blockNodes",{iv(b.u(0x44fdc8)),b.vec(fire+0x10),iv(b.u(fire+0x1c))});
  if (b.sb(fire+0x6f)) {
    b.w(fire+0x76,2,250);
    b.w(fire+0x4a4,number(b.call("mem_allocTagged",{iv(0x9c4),iv(0x54e768),iv(fire+0xbc)})));
    b.w(fire+0x50,0x44160000);
  } else {
    b.w(fire+0x50,0x43f00000);b.w(fire+0x76,2,100);
    b.w(fire+0x4a4,fire+0xbc);
    if (b.sb(fire+0x70)) b.w(fire+0x50,0x43700000);
  }
}
void staticfire_ctor(A::Model &m, Address fire, const Vec3 &position, Raw life,
                     Address owner, Raw flag, Raw size, Raw scale) {
  B b{m};
  b.call("GameObject_ctor",{iv(fire),position,iv(0),iv(0)});
  b.w(fire+0x48,0x50da38);
  b.wvec(fire+0xb0,{add(position[0],0),position[1],add(position[2],0x42100000)});
  auto light=fire+0x80;
  b.w(light+9,1,0);b.w(light+0x24,0x42f00000);
  b.w(light+0x28,0x3891a2b3);b.w(light+0x2c,ONE);
  b.w(light+0x12,2,255);b.w(light+0x14,2,128);
  b.w(light+0x1c,0x42f00000);b.w(light+0x20,0x3891a2b3);
  b.w(light+0xa,2,255);b.w(light+0xc,2,128);
  for (auto k:{0xeu,0x10u,0x16u,0x18u}) b.w(light+k,2,0);
  b.w(light+8,1,0);b.call("light_link",{iv(light)});
  b.w(fire+0x6f,1,0);b.w(fire+0x70,1,0);
  staticfire_init_common(m,fire);
  b.w(fire+0x7c,life);b.w(fire+0x68,mul(b.u(fire+0x68),scale));
  b.w(fire+0x50,size);b.w(fire+0x4a8,owner);b.w(fire+0x6e,1,1);
  if (flag) b.call("light_unlink",{iv(light)});
  const auto here=b.vec(fire+0x10);
  Vec3 low{},high{};
  for (unsigned k=0;k<3;++k) {
    low[k]=sub(here[k],0x41f00000);high[k]=add(here[k],0x41f00000);
  }
  auto found=b.call("world_collectObjectsInBox",{low,high,iv(128)});
  auto list=std::get_if<A::Objects>(&found);
  if (!list) throw A::Error("StaticFire box result required");
  unsigned count=0;
  for (auto other:list->values) {
    if (other==fire) continue;
    if (!flag) {
      // The host adapter resolves the executed slot-0 getter for this object.
      const auto actual=number(b.call("CT_type_id",{iv(other)}));
      const auto wanted=number(b.call("StaticFire_typeinfo",{}));
      if (number(b.call("CT_type_info_equals",{iv(actual),iv(wanted)})))
        b.call("light_unlink",{iv(other+0x80)});
    }
    ++count;
  }
  if (count>=4) {
    auto n=s32(b.u(fire+0x7c));
    b.w(fire+0x7c,Raw((std::int64_t(n)+(n<0))>>1));
  }
}
} // namespace champions::simulation::rta::combat_ctors
