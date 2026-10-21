// Authored from research-combat-ctors-3 9ff6ae0, docs/spec/skeleton-run.md
// and tools/ref_skeleton_run.py. The callback boundary remains explicit.
#include "champions/combat_ctors.hpp"
#include "champions/core_math.hpp"
#include <algorithm>
#include <cstdlib>

namespace champions::simulation::rta::combat_ctors {
namespace {
using A::Value;
constexpr Address G_WORLD = 0x44fdc8, G_FRAME = 0x44fdcc,
                  G_MONSTER_LEVEL = 0x4f714c, G_MISSILE_WORDS = 0x3e9ce0,
                  S_SWISH_FORMAT = 0x54bed8, S_FX_LUMP = 0x54bef0;
constexpr Raw F_13 = 0x41500000, F_18 = 0x41900000, F_20 = 0x41a00000,
              F_24 = 0x41c00000, F_36 = 0x42100000, F_48 = 0x42400000,
              F_300 = 0x43960000, F_400 = 0x43c80000, F_3 = 0x40400000,
              F_1_75 = 0x3fe00000;
Value iv(std::uint64_t n) { return std::int64_t(n & 0xffffffffu); }
Value sv(std::int32_t n) { return iv(Raw(n)); }
std::int64_t integer(const Value &v) {
  if (auto p = std::get_if<std::int64_t>(&v)) return *p;
  if (auto p = std::get_if<A::CallbackResult>(&v))
    if (p->kind == A::CallbackResult::Kind::integer)
      return p->negative ? -std::int64_t(p->magnitude) : std::int64_t(p->magnitude);
  throw A::Error("combat ctors 3: integer callback required");
}
struct Body {
  A::Model &m;
  Raw u8(Address a) const { return Raw(m.mem.load(a, 1)); }
  Raw u16(Address a) const { return Raw(m.mem.load(a, 2)); }
  Raw u32(Address a) const { return Raw(m.mem.load(a, 4)); }
  std::int32_t s8m(Address a) const { auto n=u8(a); return n&0x80 ? int(n)-256 : int(n); }
  std::int32_t s16m(Address a) const { return s16(u16(a)); }
  std::int32_t s32m(Address a) const { return s32(u32(a)); }
  std::uint64_t u64(Address a) const { return m.mem.load(a, 8); }
  Vec3 vec(Address a) const { return {u32(a),u32(a+4),u32(a+8)}; }
  void w32(Address a, Raw n) { m.store(a, 4, n); }
  void w64(Address a, std::uint64_t n) { m.store(a, 8, n); }
  Value call(const char *name, std::vector<Value> args) { return m.external_call(name,std::move(args)); }
  bool expired(Address c) const { return s32m(c+0xf4)<s32m(c+0x300); }
  bool flags(Address c, Raw mask) const { return (u16(c+0x11a)&mask)!=0; }
  bool bow(Address c) const { return s32m(c+0x3d0)==4; }
  bool see(Address c, Raw arc, Raw ray, Raw far) {
    return m.can_see_target_in(c, Raw(s16m(c+0x38)),arc,ray,ZERO,far,ZERO)!=0;
  }
  void st(Address c,Raw n) { m.vcall(c,27,{iv(n)}); }
  Vec3 ahead(Address c,Raw distance) const {
    auto f=s16m(c+0x38);
    return {add(u32(c+0x10),mul(cos16(f),distance)),
            add(u32(c+0x14),mul(sin16(f),distance)),u32(c+0x18)};
  }
  Vec3 ball(Address c) {
    for (;;) {
      Vec3 v{};
      for (auto &x:v) {
        auto f=mul(from_int(m.own_draw(c)),TWO_M16);
        x=sub(add(f,f),ONE);
      }
      auto n=add(add(mul(v[0],v[0]),mul(v[1],v[1])),mul(v[2],v[2]));
      if (!lt(ONE,n)) return v;
    }
  }
  void count_down(Address c) {
    if (!flags(c,8)) w32(c+0xf4,u32(c+0xf4)-1);
    if (s32m(c+0xf4)<=0) st(c,3);
  }
  void turn_or_chase(Address c,bool seen) {
    if (seen) m.turn_to_target(c); else m.move_to_target(c);
  }
  std::string text_of(const Value &v) {
    if (auto p=std::get_if<std::string>(&v)) return *p;
    if (auto p=std::get_if<A::CallbackResult>(&v)) {
      if (p->kind==A::CallbackResult::Kind::text || p->kind==A::CallbackResult::Kind::bytes)
        return p->data;
    }
    throw A::Error("bow resource name callback required");
  }
  Vec3 release(Address c,Vec3 lead) {
    auto holder=u32(c+0x50);
    if (holder) m.holder_call(holder,1,{iv(2),iv(0)});
    Vec3 origin{u32(c+0x10),u32(c+0x14),
                add(add(u32(c+0x18),mul(u32(c+0x20),HALF)),F_18)};
    Vec3 d{sub(lead[0],origin[0]),sub(lead[1],origin[1]),sub(lead[2],origin[2])};
    auto angle=atan2_16(d[1],d[0]);
    if (std::abs(angle-s16m(c+0x38))>1000) {
      lead=ahead(c,F_300);
      angle=atan2_16(d[1],d[0]);
    }
    auto n=add(add(mul(d[0],d[0]),mul(d[1],d[1])),mul(d[2],d[2]));
    auto inv=rsqrt(ONE,n);
    for (auto &x:d) x=mul(mul(x,inv),F_400);
    auto scatter=ball(c);
    Vec3 velocity{};
    for (unsigned k=0;k<3;++k) velocity[k]=add(d[k],mul(scatter[k],F_48));
    auto roll=m.own_draw(c);
    auto sound=text_of(call("libc_sprintf",{iv(S_SWISH_FORMAT),iv((roll&3)+1)}));
    auto entry=call("res_find",{iv(S_FX_LUMP),sound});
    call("sound_playOnObject",{entry,iv(c),iv(ONE)});
    holder=u32(c+0x50);
    auto element=holder ? s32(Raw(integer(m.holder_call(holder,16,{iv(1)})))) : 0;
    const char *ctor=nullptr;
    Raw size=0, skill=0;
    if (element==2) {ctor="ColdArrowProjectile_ctor";size=0xb0;skill=0x41b220;}
    else if (element==1) {ctor="FireArrowProjectile_ctor";size=0xb0;skill=0x41b21c;}
    else if (element==6) {ctor="ExplodeArrowProjectile_ctor";size=0x74;skill=0x41b22c;}
    if (ctor) {
      auto block=Raw(integer(call("pool_new",{iv(size)})));
      if (block) call(ctor,{iv(block),origin,sv(angle),iv(c),sv(to_int(u32(G_MONSTER_LEVEL))),iv(u32(skill))});
    } else {
      auto block=Raw(integer(call("pool_new",{iv(0x1c8)})));
      if (block) call("MissileWeapon_ctor",{iv(block),iv(0),iv(u32(G_MISSILE_WORDS+8)),
                 iv(u32(G_MISSILE_WORDS+0xc)),origin,velocity,sv(angle-0x4000),iv(c),
                 iv(0),iv(0),sv(s16m(c+0xd0)),sv(s16m(c+0xd2)/2),iv(0),iv(0),
                 sv(s16m(c+0xd4)),iv(4),iv(0)});
    }
    return lead;
  }
};
} // namespace

Raw count_living_nonplayer_in_box(A::Model &m,const Vec3 &centre,Raw half_size) {
  Body b{m}; Vec3 low{},high{};
  for (unsigned k=0;k<3;++k) {low[k]=sub(centre[k],half_size);high[k]=add(centre[k],half_size);}
  auto found=b.call("world_collectObjectsInBox",{low,high,iv(0x80)});
  auto list=std::get_if<A::Objects>(&found);
  if (!list) throw A::Error("living-box object list required");
  Raw count=0;
  for (auto obj:list->values) if ((b.u32(obj+0xc)&0x68)==0x20) ++count;
  return count;
}
Raw creature_obj_in_arc_range(A::Model &m,Address c,Address object,Raw arc,Raw range) {
  Body b{m};
  if (!object) return 0;
  Vec3 d{sub(b.u32(object+0x10),b.u32(c+0x10)),
         sub(b.u32(object+0x14),b.u32(c+0x14)),
         sub(add(b.u32(object+0x18),mul(b.u32(object+0x20),HALF)),
             add(b.u32(c+0x18),mul(b.u32(c+0x20),HALF)))};
  auto bearing=s16(0x4000-atan2_16(d[0],d[1]));
  auto diff=s16(Raw(bearing-b.s16m(c+0x38)));
  auto absdiff=diff<0 ? -diff : diff;
  if (absdiff>=s16(arc)) return 0;
  auto dx=sub(b.u32(c+0x10),b.u32(object+0x10));
  auto dy=sub(b.u32(c+0x14),b.u32(object+0x14));
  auto radius=sub(add(mul(dx,dx),mul(dy,dy)),b.u32(object+0x1c));
  if (!lt(radius,mul(range,range))) return 0;
  Vec3 origin=b.vec(c+0x10);
  origin[2]=add(origin[2],b.u32(c+0x20));
  auto t=Raw(integer(b.call("terrain_raycast",{iv(b.u32(G_WORLD)),origin,d,iv(3),iv(0x3f8ccccc)})));
  return lt(t,ZERO)||lt(ONE,t) ? 1 : 0;
}
Raw ally_in_line_of_fire(A::Model &m,Address c,Address target,Raw previous,Raw reach,Raw distance) {
  Body b{m};
  if (!target || ((b.u32(G_FRAME)^Raw(b.s8m(c+0x230)))&7)) return previous;
  auto n=count_living_nonplayer_in_box(m,b.vec(target+0x10),mul(b.u32(target+0x1c),F_1_75));
  if (!creature_obj_in_arc_range(m,c,target,0x4000,reach)) return 0;
  auto bearing=atan2_16(sub(b.u32(target+0x10),b.u32(c+0x10)),
                        sub(b.u32(target+0x14),b.u32(c+0x14)));
  auto heading=s16(0x4000-bearing);
  Vec3 origin{b.u32(c+0x10),b.u32(c+0x14),add(b.u32(c+0x18),F_13)};
  Vec3 dir{mul(cos16(heading),distance),mul(sin16(heading),distance),ZERO};
  auto result=b.call("world_raycast",{iv(b.u32(G_WORLD)),origin,dir,iv(c),iv(1),iv(3),iv(0),iv(ONE)});
  auto hit=std::get_if<A::RayHit>(&result);
  if (!hit) throw A::Error("line-of-fire ray result required");
  return n<(previous?3u:4u)||!lt(ZERO,hit->fraction)||!lt(hit->fraction,ONE)||hit->object==target ? 0:1;
}
Raw lead_target_point(A::Model &m,Address c,Vec3 *out,Raw speed) {
  Body b{m}; auto node=b.u32(c+0x234);
  if (!node || !b.u32(node+0xc)) return 0;
  auto t=b.u32(node+0xc);
  auto dx=sub(b.u32(t+0x10),b.u32(c+0x10));
  auto dy=sub(b.u32(t+0x14),b.u32(c+0x14));
  auto time=div(sqrt(add(mul(dx,dx),mul(dy,dy))),speed);
  if (out) *out={add(b.u32(t+0x10),mul(b.u32(t+0x308),time)),
                 add(b.u32(t+0x14),mul(b.u32(t+0x30c),time)),
                 add(b.u32(t+0x18),mul(b.u32(t+0x20),HALF))};
  return 1;
}
Raw skeleton_has_shield(A::Model &m,Address c) {
  Body b{m};auto holder=b.u32(c+0x50);
  auto part=holder ? Raw(integer(m.holder_call(holder,15,{iv(2)}))) : 0;
  return part==0x60||part==0x62 ? 1:0;
}
void skeleton_on_nearby_event(A::Model &m,Address c,Raw code,const Vec3 &,Address other) {
  Body b{m};
  if (s32(code)!=2 || !skeleton_has_shield(m,c)) return;
  auto state=b.s16m(c+0x232);
  if ((state!=3 && state!=5)||!other) return;
  auto bearing=atan2_16(sub(b.u32(other+0x14),b.u32(c+0x14)),
                         sub(b.u32(other+0x10),b.u32(c+0x10)));
  if (Raw(bearing-b.s16m(c+0x38)+9999)<19999) b.st(c,0x11);
}
void skeleton_run(A::Model &m,Address c) {
  Raw ev=0;
  if (!skeleton_run_prologue(m,c,&ev)) return;
  Body b{m}; auto state=b.s16m(c+0x232);
  if (state<0 || state>=0x18) return;
  if (state==0||state==1) {
    if (integer(m.vcall(c,25,{iv(0)}))) {b.st(c,3);return;}
    if (b.bow(c)&&integer(m.vcall(c,25,{iv(1)}))) {b.st(c,3);return;}
    if (b.expired(c)) {b.st(c,state==0?1:0);return;}
    if (state==0) {if (m.is_away_from_home(c)) b.st(c,1);}
    else m.move_to_dest(c);
  } else if (state==3) {
    auto node=b.u32(c+0x234);
    if (!node) {b.st(c,0);return;}
    if (b.bow(c)) {
      if (b.see(c,2000,1,F_400)) {b.st(c,8);return;}
      b.turn_or_chase(c,b.see(c,0x7fff,1,F_400));return;
    }
    if (b.see(c,6000,0,F_24)) {b.st(c,6);return;}
    auto target=b.u32(node+0xc);
    if (ally_in_line_of_fire(m,c,target,0,F_300,mul(b.u32(c+0x1c),F_3))) {
      auto d=m.own_draw(c);
      b.w32(c+0xf4,b.u32(c+0x300)+d%240+240);b.st(c,5);return;
    }
    b.turn_or_chase(c,b.see(c,0x7fff,0,F_20));
  } else if (state==5) {
    bool melee=true;
    if (b.bow(c)) {if (b.see(c,2000,0,F_400)) {b.st(c,8);return;} melee=!b.bow(c);}
    if (melee&&b.see(c,6000,0,F_20)) {b.st(c,6);return;}
    if (!m.is_not_rooted(c)) return;
    if (b.expired(c)) {b.st(c,3);return;}
    auto node=b.u32(c+0x234);
    if (!node) return;
    if (!ally_in_line_of_fire(m,c,b.u32(node+0xc),1,F_300,mul(b.u32(c+0x1c),F_3))) b.st(c,3);
  } else if (state==6) {
    if (b.u32(c+0x234)) {
      if (ev&1) {
        auto desc=m.weapon_descriptor(c);
        b.call("Creature_hitTargetAtReach",{iv(c),sv(b.s16m(c+0xd0)),sv(b.s16m(c+0xd2)),
               sv(b.s16m(c+0xd4)),iv(8),desc,iv(F_36)});
      }
      if (b.flags(c,8)) m.turn_to_target(c);
      else b.w64(c+0xa8,b.u64(c+0xa8)|0x200);
    }
    b.count_down(c);
  } else if (state==8) {
    if (!b.u32(c+0x234)) {b.st(c,0);b.count_down(c);return;}
    auto lead=b.ahead(c,F_300);
    lead_target_point(m,c,&lead,F_400);
    if (ev&1) lead=b.release(c,lead);
    auto holder=b.u32(c+0x50);
    if ((ev&2)&&holder) m.holder_call(holder,1,{iv(2),iv(0x56)});
    if (b.flags(c,8)) m.vcall(c,26,{iv(0),lead});
    b.count_down(c);
  } else if (state==0x10) {
    if (m.is_attack_ready(c)) b.st(c,0); else m.flee_from_target(c,0);
  } else if (state==0x11) {
    if (b.expired(c)) b.st(c,3);
  } else if (state==0x12) {
    if (!b.flags(c,0x10)) b.st(c,3);
  } else if (state==0x13) {
    if (m.is_not_stunned(c)) b.st(c,0);
  } else if (state==0x14) {
    b.call("Creature_deadUpdate",{iv(c),iv(ev),Vec3{ZERO,ZERO,ZERO},iv(1),iv(F_20)});
  } else if (state==0x17) {
    b.call("fx_spawn",{iv(7),b.vec(c+0x10)});
    if (!b.flags(c,0x10)) b.st(c,0);
  }
}

void install_round3(A::Model &m) {
  m.class_bodies[SKELETON_RUN]=[](A::Model &x,Address self,const std::vector<Value> &) -> Value {
    skeleton_run(x,self);return std::monostate{};
  };
  m.class_bodies[0x2a7688]=[](A::Model &x,Address self,const std::vector<Value> &args) -> Value {
    // The World notification carries its search radius as an extra service
    // argument. The executed Skeleton body reads only the first three.
    if (args.size()<3) throw A::Error("Skeleton_onNearbyEvent arguments");
    auto pos=std::get_if<Vec3>(&args[1]);
    if (!pos) throw A::Error("Skeleton_onNearbyEvent position");
    skeleton_on_nearby_event(x,self,Raw(integer(args[0])),*pos,Raw(integer(args[2])));
    return std::monostate{};
  };
  m.class_bodies[SOUL_RUN]=[](A::Model &x,Address self,const std::vector<Value> &) -> Value {
    soul_run(x,self);return std::monostate{};
  };
}
} // namespace champions::simulation::rta::combat_ctors
