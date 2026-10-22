// Authored from research-combat-ctors-3 9ff6ae0, docs/spec/soul-run.md
// and tools/ref_soul_run.py. Matrix arithmetic uses the existing core model.
#include "champions/combat_ctors.hpp"
#include "champions/core_math.hpp"
#include <algorithm>
#include <cstdlib>

namespace champions::simulation::rta::combat_ctors {
namespace {
using A::Value;
constexpr Address G_WORLD=0x44fdc8,G_FRAME=0x44fdcc,G_NUM_PLAYERS=0x4d8ae0,
                  G_PLAYERS=0x4d8af0,REC_SIZE=0x4c30,REC_OBJ=0x43d8;
constexpr Raw F_15=0x41700000,F_300=0x43960000,F_0_2=0x3e4ccccc,
              F_0_4=0x3ecccccc,F_400=0x43c80000,F_M60=0xc2700000,
              F_M80=0xc2a00000,F_STEP=0x3b888888,F_36=0x42100000,
              F_6=0x40c00000,F_10=0x41200000,F_50=0x42480000;
Value iv(std::uint64_t n) { return std::int64_t(n&0xffffffffu); }
std::int64_t number(const Value &v) {
  if (auto p=std::get_if<std::int64_t>(&v)) return *p;
  if (auto p=std::get_if<A::CallbackResult>(&v))
    if (p->kind==A::CallbackResult::Kind::integer)
      return p->negative ? -std::int64_t(p->magnitude) : std::int64_t(p->magnitude);
  throw A::Error("Soul callback integer required");
}
Vec3 point_of(const Value &v) {
  if (auto p=std::get_if<A::Sample>(&v)) return p->point;
  if (auto p=std::get_if<Vec3>(&v)) return *p;
  if (auto p=std::get_if<A::CallbackResult>(&v)) {
    const auto *q=p;
    if (q->elements.size()==2) q=&q->elements[0];
    if (q->elements.size()==3) {
      Vec3 out{};
      for (unsigned k=0;k<3;++k) out[k]=Raw(q->elements[k].magnitude);
      return out;
    }
  }
  throw A::Error("Soul sample point callback required");
}
Mat34 matrix_of(const Value &v) {
  auto p=std::get_if<A::CallbackResult>(&v);
  if (!p||p->elements.size()!=12) throw A::Error("Soul attach matrix callback required");
  Mat34 m{};
  for (unsigned k=0;k<12;++k) m[k]=Raw(p->elements[k].magnitude);
  return m;
}
struct Body {
  A::Model &m;
  Raw u8(Address a) const {return Raw(m.mem.load(a,1));}
  Raw u16(Address a) const {return Raw(m.mem.load(a,2));}
  Raw u32(Address a) const {return Raw(m.mem.load(a,4));}
  std::int32_t s8m(Address a) const {auto x=u8(a);return x&0x80?int(x)-256:int(x);}
  std::int32_t s16m(Address a) const {return s16(u16(a));}
  std::int32_t s32m(Address a) const {return s32(u32(a));}
  Vec3 vec(Address a) const {return {u32(a),u32(a+4),u32(a+8)};}
  void w8(Address a,Raw x) {m.store(a,1,x);}
  void w16(Address a,Raw x) {m.store(a,2,x);}
  void w32(Address a,Raw x) {m.store(a,4,x);}
  Value call(const char *name,std::vector<Value> args) {return m.external_call(name,std::move(args));}
  bool expired(Address c) const {return s32m(c+0xf4)<s32m(c+0x300);}
  bool due(Address c) const {return ((u32(G_FRAME)^Raw(s8m(c+0x230)))&15)==0;}
  void st(Address c,Raw x) {m.vcall(c,27,{iv(x)});}
  Vec3 ball(Address c) {
    for (;;) {
      Vec3 v{};
      for (auto &x:v) {auto f=mul(from_int(m.own_draw(c)),TWO_M16);x=sub(add(f,f),ONE);}
      auto n=add(add(mul(v[0],v[0]),mul(v[1],v[1])),mul(v[2],v[2]));
      if (!lt(ONE,n)) return v;
    }
  }
  void spawn(const Vec3 &point,const Vec3 &v) {
    call("particle_spawn",{iv(P_SOUL),point,Vec3{mul(v[0],F_15),mul(v[1],F_15),mul(ZERO,F_15)},iv(ZERO)});
  }
  Vec3 sample(Address c,const Vec3 &pos,Raw facing,Raw index) {
    auto model=Raw(number(m.vcall(c,16)));
    return point_of(call("model_getSamplePoint",{iv(model),iv(c+0x114),pos,iv(facing),iv(index),
                                                 iv(1),iv(0),iv(u32(c+0xdc))}));
  }
  std::int32_t sample_count(Address c) {
    return s32(Raw(number(call("model_getSamplePointCount",{m.vcall(c,16)}))));
  }
  bool demon_near(Address c) {
    if (m.callbacks.count("Soul_isDemonNear"))
      return number(call("Soul_isDemonNear",{iv(c)}))!=0;
    auto centre=vec(c+0x10);Vec3 low{},high{};
    for (unsigned k=0;k<3;++k) {low[k]=sub(centre[k],F_300);high[k]=add(centre[k],F_300);}
    auto v=call("world_collectObjectsInBox",{low,high,iv(0x80)});
    auto found=std::get_if<A::Objects>(&v);
    if (!found) throw A::Error("Soul demon search object list required");
    for (auto obj:found->values) {
      // soul-run.md "Soul_isDemonNear" and ref_soul_run.py soul_is_demon_near
      // take typeid(obj) of every box object; no object is skipped. type_id
      // resolves the getter (a documented type-info getter, a class body, or
      // the Creature.v0 / CT_type_id call-outs). When none applies the
      // search refuses rather than continuing past the object.
      auto fn=u32(u32(obj+0x48)+4);
      if (!m.callbacks.count("CT_type_id")&&!m.callbacks.count("Creature.v0")&&
          !find_type_info(fn)&&!m.class_bodies.count(fn))
        throw A::Error("Soul_isDemonNear: typeid of a box object has no documented getter or call-out");
      auto demon=type_info(m,TI_DEMON);
      if (type_info_equals(m,type_id(m,obj),demon)&&!(u32(obj+0xc)&8)) return true;
    }
    return false;
  }
  static std::int32_t remainder(Raw a,std::int32_t n) {return std::int32_t(a%Raw(std::abs(n)));}
  void dissolve(Address c) {
    if (u32(c+0xc)&2) return;
    auto n=sample_count(c);
    if (n) for (int j=0;j<3;++j) {
      auto d=m.own_draw(c);
      auto point=sample(c,vec(c+0x10),Raw(s16m(c+0x38)),Raw(remainder(d,n)));
      auto v=ball(c);
      if (!s8m(c+0x231)) spawn(point,v);
    }
    if (!expired(c)&&!(u16(c+0x11a)&0x80)) return;
    for (int i=0;i<n;++i) {
      auto point=sample(c,vec(c+0x10),Raw(s16m(c+0x38)),Raw(i));
      for (int j=0;j<2;++j) {
        auto v=ball(c);
        if (!s8m(c+0x231)) spawn(point,v);
      }
    }
    auto flags=u32(c+0xc);w32(c+0xc,flags|2);
    if (s16m(c+0x232)==0x14) call("obj_delete",{iv(c)});
    else {w32(c+0xc,flags|0xb);call("obj_stopRunning",{iv(c)});}
  }
  void thrown(Address c) {
    auto n=sample_count(c);
    auto transform=mat34_set_pos_rot_z_scale(u32(c+0xdc),vec(c+0x10),s16(u16(c+0x38)+0x5f40));
    transform=mat34_rotate_axis(transform,0,0x4fa0);
    if (n) for (int j=0;j<3;++j) {
      auto d=m.own_draw(c);
      auto point=sample(c,{ZERO,ZERO,ZERO},0xffffc000,Raw(remainder(d,n)));
      spawn(mat34_transform_point(transform,point),ball(c));
    }
    Vec3 velocity{};
    auto handle=u32(c+0x3cc);
    if (!handle) {
      auto f=s16m(c+0x38);
      velocity={mul(cos16(f),F_400),mul(sin16(f),F_400),F_M80};
    } else {
      auto t=u32(handle+0xc);auto p=vec(t+0x10);
      p[0]=add(p[0],mul(u32(t+0x308),F_0_2));
      p[1]=add(p[1],mul(u32(t+0x30c),F_0_2));
      w32(c+0xb0,F_0_4);
      m.vcall(c,26,{iv(0),p});
      t=u32(u32(c+0x3cc)+0xc);
      Vec3 d{sub(u32(t+0x10),u32(c+0x10)),sub(u32(t+0x14),u32(c+0x14)),
             sub(u32(t+0x18),u32(c+0x18))};
      auto n2=add(add(mul(d[0],d[0]),mul(d[1],d[1])),mul(d[2],d[2]));
      auto nz=mul(d[2],rsqrt(ONE,n2));
      auto f=s16m(c+0x38);
      velocity={mul(cos16(f),F_400),mul(sin16(f),F_400),mul(nz,F_400)};
      if (lt(F_M60,velocity[2])) velocity[2]=F_M60;
    }
    bool hit=false;
    for (int step=0;step<4;++step) {
      Vec3 delta{};
      for (unsigned k=0;k<3;++k) delta[k]=mul(velocity[k],F_STEP);
      auto p=vec(c+0x10);
      w32(c+0x14,add(p[1],delta[1]));
      w32(c+0x18,add(p[2],delta[2]));
      w32(c+0x10,add(p[0],delta[0]));
      for (int j=0;j<s32m(G_NUM_PLAYERS);++j) {
        auto obj=u32(G_PLAYERS+Raw(j)*REC_SIZE+REC_OBJ);
        if (!obj||(u32(obj+0xc)&8)) continue;
        auto q=vec(obj+0x10);
        q[2]=add(q[2],mul(u32(obj+0x20),HALF));
        auto f=s16m(c+0x38);
        Vec3 a{add(u32(c+0x10),mul(cos16(f),F_36)),
               add(u32(c+0x14),mul(sin16(f),F_36)),u32(c+0x18)};
        auto dx=sub(a[0],q[0]),dy=sub(a[1],q[1]),dz=sub(a[2],q[2]);
        auto dist=sqrt(add(add(mul(dx,dx),mul(dy,dy)),mul(dz,dz)));
        if (le(dist,add(u32(c+0x1c),u32(obj+0x1c)))) {
          call("damage_apply",{iv(obj),iv(c),iv(100000),iv(0),iv(0),iv(0x4000140),
                               iv(0),iv(0),iv(F_10),iv(F_50)});
          hit=true;break;
        }
      }
    }
    if (!hit&&!expired(c)) {
      auto h=Raw(number(call("terrain_height",{iv(u32(G_WORLD)),vec(c+0x10),iv(F_6)})));
      if (!lt(u32(c+0x18),h)) return;
    }
    call("obj_delete",{iv(c)});
    for (int i=0;i<n;++i) {
      auto p=sample(c,{ZERO,ZERO,ZERO},0xffffc000,Raw(i));
      p=mat34_transform_point(transform,p);
      for (int j=0;j<2;++j) spawn(p,ball(c));
    }
  }
  void burst_owned(Address c) {
    auto n=sample_count(c);
    for (int i=0;i<n;++i) {
      auto p=sample(c,{ZERO,ZERO,ZERO},0xffffc000,Raw(i));
      auto owner=u32(c+0x3c8);
      if (owner) {
        auto a=matrix_of(call("creature_getAttachMatrix0",{iv(u32(owner+0xc))}));
        auto b=matrix_of(call("model_getAttachMatrix",{iv(u32(u32(c+0x24)+0x1c)),iv(c+0x114),iv(0),iv(ONE)}));
        p=mat34_transform_point(mat34_mul(mat34_invert(b),a),p);
      }
      for (int j=0;j<2;++j) spawn(p,ball(c));
    }
    w32(c+0xc,u32(c+0xc)|2);
    call("obj_delete",{iv(c)});
  }
};
} // namespace

void creature_set_charm(A::Model &m,Address c,Raw frames,Raw team) {
  Body b{m};
  if (b.s16m(c+0x312)==-1) return;
  auto flags=b.u32(c+0xc);
  if (!(flags&0x20)||(flags&0x40)) return;
  b.w8(c+0x310,team);
  b.w16(c+0x312,frames);
  b.call("handle_clear",{iv(c+0x234)});
  b.w32(c+0x234,0);
}

bool soul_is_demon_near(A::Model &m,Address c) {Body b{m};return b.demon_near(c);}
void soul_run(A::Model &m,Address c) {
  Raw ev=0;
  if (!soul_run_prologue(m,c,&ev)) return;
  Body b{m};auto state=b.s16m(c+0x232);
  if (state<0||state>=0x23) return;
  if (state==0) {
    if (b.due(c)&&b.demon_near(c)) {b.st(c,0x18);return;}
    if (b.expired(c)||m.is_away_from_home(c)) b.st(c,1);
  } else if (state==1) {
    if (b.due(c)&&b.demon_near(c)) {b.st(c,0x18);return;}
    if (m.move_to(c,b.vec(c+0x104))||b.expired(c)) b.st(c,0);
  } else if (state==3) {
    if (!b.u32(c+0x234)) b.st(c,0);
  } else if (state==5) {
    if (m.is_not_rooted(c)) b.st(c,0);
  } else if (state==0x12||state==0x19) {
    if (!(b.u16(c+0x11a)&0x10)) b.st(c,0x18);
  } else if (state==0x14||state==0x22) b.dissolve(c);
  else if (state==0x17) {if (b.expired(c)) b.st(c,1);}
  else if (state==0x18) {if (b.due(c)&&!b.demon_near(c)) b.st(c,0);}
  else if (state==0x1b) b.thrown(c);
  else if (state==0x1d&&(ev&1)) b.burst_owned(c);
}
} // namespace champions::simulation::rta::combat_ctors
