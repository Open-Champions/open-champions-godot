// Authored from research-combat-ctors-3 9ff6ae0, gut-chunk.md and
// ref_phprop.py. All resources and rigid-body setup remain explicit callbacks.
#include "champions/combat_ctors.hpp"
#include "champions/core_math.hpp"
#include <algorithm>
#include <cstdint>
#include <string>

namespace champions::simulation::rta::combat_ctors {
namespace {
// Index of the highest set bit of a non-zero mask (63 - count of leading zeros),
// written out because MSVC has no __builtin_clzll and C++17 has no std::countl_zero.
int highest_bit(std::uint64_t mask) {
  int bit = 63;
  while (!((mask >> bit) & 1u)) --bit;
  return bit;
}
using A::Value;
constexpr Address G_GRID=0x5db4f0,G_OFF_GRID=0x5f76f0,S_HIDE=0x5321a8,
                  G_PROP_OPTIONS=0x44fe08;
Value iv(std::uint64_t v) {return std::int64_t(v&0xffffffffu);}
std::int64_t number(const Value &v) {
  if (auto p=std::get_if<std::int64_t>(&v)) return *p;
  if (auto p=std::get_if<A::CallbackResult>(&v))
    if (p->kind==A::CallbackResult::Kind::integer)
      return p->negative?-std::int64_t(p->magnitude):std::int64_t(p->magnitude);
  throw A::Error("prop integer callback required");
}
Mat34 matrix(const Value &v) {
  auto p=std::get_if<A::CallbackResult>(&v);
  if (!p||p->elements.size()!=12) throw A::Error("prop joint matrix callback required");
  Mat34 out{};
  for (unsigned k=0;k<12;++k) out[k]=Raw(p->elements[k].magnitude);
  return out;
}
struct B {
  A::Model &m;
  Raw u8(Address a) const {return Raw(m.mem.load(a,1));}
  Raw u16(Address a) const {return Raw(m.mem.load(a,2));}
  Raw u32(Address a) const {return Raw(m.mem.load(a,4));}
  std::uint64_t u64(Address a) const {return m.mem.load(a,8);}
  std::int32_t s8m(Address a) const {auto x=u8(a);return x&128?int(x)-256:int(x);}
  Vec3 vec(Address a) const {return {u32(a),u32(a+4),u32(a+8)};}
  void w8(Address a,Raw v) {m.store(a,1,v);}
  void w16(Address a,Raw v) {m.store(a,2,v);}
  void w32(Address a,Raw v) {m.store(a,4,v);}
  void w64(Address a,std::uint64_t v) {m.store(a,8,v);}
  void wvec(Address a,const Vec3 &v) {w64(a,std::uint64_t(v[0])|(std::uint64_t(v[1])<<32));w32(a+8,v[2]);}
  Value call(const char *n,std::vector<Value> args) {return m.external_call(n,std::move(args));}
  Raw fraction(Address obj) {return mul(from_int(m.own_draw(obj)),TWO_M16);}
  void copy_name(Address dst,Address src) {
    for (Address k=0;k<256;++k) {auto v=u8(src+k);w8(dst+k,v);if (!v) return;}
    throw A::Error("prop name over 256 bytes");
  }
  void gameobject(Address obj,const Vec3 &pos) {
    if (auto cb=m.callbacks.find("World_gameobject_prepare");cb!=m.callbacks.end())
      cb->second(m,{iv(obj)});
    for (auto off:{0u,4u,8u,12u,0x34u}) w32(obj+off,0);
    w32(obj+0x48,0x5122a8);w32(obj+0x1c,0x41c00000);w32(obj+0x20,0x42900000);
    w8(obj+0x28,0x19);w8(obj+0x29,0x49);wvec(obj+0x10,pos);w32(obj+0x24,0);
    auto col=(s32(to_int(pos[0]))+0x3c00)>>8;
    auto row=(s32(to_int(pos[1]))+0x3c00)>>8;
    auto cell=col>=0&&col<0x78&&row>=0&&row<0x78
        ? Raw(G_GRID+4*(row*0x78+col)):G_OFF_GRID;
    call("handle_link",{iv(cell),iv(obj+8),iv(obj)});
    if (number(call("obj_getPropInt",{iv(obj),iv(S_HIDE),iv(0)})))
      throw A::Unsupported("hidden prop's grid move is not executed");
    w32(obj+0x2c,0);w32(obj+0x44,m.draw_shared());
  }
  std::array<Raw,6> bounds(Address obj) {
    auto v=call("model_getBounds",{iv(u32(obj+0x4c)),Value(std::int64_t(u64(obj+0xe8)))});
    if (auto p=std::get_if<A::Bounds>(&v)) return p->values;
    if (auto p=std::get_if<A::CallbackResult>(&v))
      if (p->elements.size()==6) {
        std::array<Raw,6> out{};
        for (unsigned k=0;k<6;++k) out[k]=Raw(p->elements[k].magnitude);
        return out;
      }
    throw A::Error("model_getBounds six-word result required");
  }
  void launch(Address obj,Raw bits) {
    w32(obj+0xc,u32(obj+0xc)|0x1809);
    call("obj_updateGridCell",{iv(obj)});
    w32(obj+0x1c,0x40800000);
    if (bits&0x100) {w16(obj+0x140,u16(obj+0x140)|2);w32(obj+0xc,u32(obj+0xc)|0x4000);}
    if (bits&0x200) w16(obj+0x140,u16(obj+0x140)|4);
    for (auto pair:{std::pair<Raw,Raw>{1,0x42f80000},{8,0x43600000},{0x10,0x43a20000}})
      if (bits&pair.first) {
        w32(obj+0xfc,mul(sub(fraction(obj),HALF),pair.second));
        w32(obj+0x100,mul(sub(fraction(obj),HALF),pair.second));
        if (pair.first==0x10) w32(obj+0x30,mul(fraction(obj),0x43f00000));
      }
    if (bits&0x20) w32(obj+0x30,add(mul(fraction(obj),0x41f00000),0x43020000));
    if (bits&0x40) w32(obj+0x30,add(mul(fraction(obj),0x42700000),0x43820000));
    if (bits&2) w16(obj+0x10c,2000);
    call("obj_runEveryFrame",{iv(obj)});
    w16(obj+0x10e,bits&0x8000?0x7fff:180);
    if (bits&0x2000) w16(obj+0x140,u16(obj+0x140)|8);
    else if (bits&0x4000) w16(obj+0x140,u16(obj+0x140)|0x10);
    if (bits&0x10000) w16(obj+0x140,u16(obj+0x140)|0x20);
    if (bits&0x80) {
      auto old=u16(obj+0x140);
      w16(obj+0x10e,0);w16(obj+0x140,old|1);
      if (!(old&2)) call("obj_stopRunning",{iv(obj)});
    }
    if (u32(G_PROP_OPTIONS)&0x100) {
      if (lt(0x43c80000,u32(obj+0x30))) w32(obj+0x30,0x43c80000);
      w32(obj+0x30,mul(u32(obj+0x30),0x3e800000));
      w32(obj+0xfc,mul(u32(obj+0xfc),HALF));
      w32(obj+0x100,mul(u32(obj+0x100),HALF));
    }
  }
  void named(Address obj,const Vec3 &pos,Address lump,Address model,Address tex,
             std::uint64_t parts,Raw flags) {
    gameobject(obj,pos);
    w32(obj+0x48,0x511d58);
    copy_name(obj+0x54,lump);copy_name(obj+0x64,model);copy_name(obj+0xa4,tex);
    w32(obj+0x4c,Raw(number(call("res_find",{iv(obj+0x54),iv(obj+0x64)}))));
    auto texture=Raw(number(call("res_find",{iv(obj+0x54),iv(obj+0xa4)})));
    w64(obj+0xe8,parts);w32(obj+0x50,texture);
    auto facing=m.own_draw(obj);
    w16(obj+0x10c,0);w16(obj+0x38,facing);
    w32(obj+0x30,ZERO);w32(obj+0x100,ZERO);w32(obj+0xfc,ZERO);
    auto bb=bounds(obj);
    Raw centre[3]{};
    for (unsigned k=0;k<3;++k)
      centre[k]=from_int(Raw(s32(bb[k]+bb[k+3])>>1));
    w32(obj+0xf8,centre[2]);w32(obj+0xf0,centre[0]);w32(obj+0xf4,centre[1]);
    w32(obj+0x108,sub(centre[2],from_int(bb[2])));
    if (flags&4) {
      for (unsigned k=0;k<3;++k) w32(obj+0x10+4*k,add(u32(obj+0x10+4*k),centre[k]));
      call("obj_updateGridCell",{iv(obj)});
    }
    launch(obj,flags);
  }
  void record(Address obj,Address item,Raw scale) {
    w32(obj+0x48,0x511ba8);
    for (unsigned k=0;k<4;++k) w16(obj+0x16c+8*k,0xffff);
    w8(obj+0x169,0xff);w8(obj+0x168,0xff);
    for (unsigned k=0;k<4;++k) {w16(obj+0x158+2*k,0);w16(obj+0x160+2*k,0);}
    w32(obj+0x240,scale);w32(obj+0xc,u32(obj+0xc)|0x4000);
    if (item) {
      for (unsigned k=0;k<0x48;k+=4) w32(obj+0x148+k,u32(item+k));
      w8(obj+0x238,1);w32(obj+0x34,0x403ab8);
      w32(obj+0xc,(u32(obj+0xc)|0x100000)&~0x4800u);
    }
  }
  void physics(Address obj,Raw flags) {
    if (s8m(obj+0x238)&&(u64(obj+0x150)&0x20000)) flags|=0x1000;
    auto bb=bounds(obj);
    auto scale=u32(obj+0x240);
    auto extent=[&](unsigned k){return sub(mul(from_int(bb[k+3]),scale),mul(from_int(bb[k]),scale));};
    call("rigidbody_init",{iv(obj+0x190),iv(ONE),iv(ONE),iv(extent(0)),iv(extent(1)),iv(extent(2))});
    wvec(obj+0x1a0,vec(obj+0x10));
    w32(obj+0x1bc,mul(u32(obj+0xfc),0x3c888888));
    w32(obj+0x1c0,mul(u32(obj+0x100),0x3c888888));
    w32(obj+0x1c4,mul(u32(obj+0x30),0x3c888888));
    auto spin=flags&0x410?0x41200000:TWO;
    for (auto off:{0x1c8u,0x1ccu,0x1d0u}) {
      auto f=fraction(obj);w32(obj+off,mul(sub(add(f,f),ONE),spin));
    }
    w32(obj+0x22c,M32);
    auto mask=u64(obj+0xe8);
    if (mask) w32(obj+0x22c,Raw(highest_bit(mask)));
    call("quat_fromMat3",{iv(obj+0x1ac),iv(obj+0x110)});
    call("quat_normalize",{iv(obj+0x1ac)});
    call("rigidbody_updateDerived",{iv(obj+0x190)});
    w32(obj+0x23c,flags&0x800?0xbdcccccc:0xbecccccc);
    if (flags&0x1000) for (auto off:{0x19cu,0x194u,0x198u}) w32(obj+off,0);
  }
  void sound(Address obj,Raw flags) {
    if (!s8m(obj+0x238)||!(flags&0x19)) return;
    auto n=m.own_draw(obj)%3+1;
    auto name=call("libc_sprintf",{iv(0x532688),iv(n)});
    auto res=call("res_find",{iv(0x532478),name});
    call("sound_playOnObject",{res,iv(obj),iv(ONE)});
  }
};
} // namespace

void phprop_ctor(A::Model &m,Address obj,const Vec3 &pos,Address lump,Address model,
                 Address texture,std::uint64_t parts,Raw launch,Address item,Raw scale) {
  B b{m};b.named(obj,pos,lump,model,texture,parts,launch);
  b.record(obj,item,scale);b.w8(obj+0x23a,0);b.w32(obj+0x234,0);
  b.physics(obj,launch);b.sound(obj,launch);
}
void phprop_ctor2(A::Model &m,Address obj,const Vec3 &pos,Raw facing,Address model,
                  Address anim,Address lump,Address name,Address texture,
                  std::uint64_t parts,Raw launch,Address item,Raw scale) {
  B b{m};b.named(obj,pos,lump,name,texture,parts,launch|4);
  b.record(obj,item,scale);b.w32(obj+0x22c,M32);b.w8(obj+0x23a,0);b.w32(obj+0x234,0);
  if (auto mask=b.u64(obj+0xe8)) b.w32(obj+0x22c,Raw(highest_bit(mask)));
  auto joint=Raw(number(b.call("model_getMeshJoint",{iv(model),iv(b.u32(obj+0x22c))})));
  auto mat=matrix(b.call("anim_getJointMatrix",{iv(anim),iv(joint)}));
  auto rot=mat34_set_pos_rot_z_scale(b.u32(obj+0x240),{ZERO,ZERO,ZERO},s16(s16(facing)+0x4000));
  mat=mat34_mul(mat,rot);
  auto here=b.vec(obj+0x10);Vec3 offset{},moved{};
  for (unsigned k=0;k<3;++k) offset[k]=sub(here[k],pos[k]);
  offset=mat34_transform_point(mat,offset);
  for (unsigned k=0;k<3;++k) moved[k]=add(pos[k],offset[k]);
  b.wvec(obj+0x10,moved);b.physics(obj,launch);b.sound(obj,launch);
  b.w32(obj+0xc,b.u32(obj+0xc)|0x2000);
}
void gutchunk_ctor(A::Model &m,Address obj,const Vec3 &pos,Raw kind,Raw bits) {
  B b{m};constexpr Address base=0x4f86a0;
  phprop_ctor(m,obj,pos,b.u32(base),b.u32(base+0x50),
              b.u32(base+(bits&2?0x60:0x58)),std::uint64_t(std::int64_t(s32(kind))),
              0xb41,0,ONE);
  b.w32(obj+0x248,bits);b.w32(obj+0x48,0x50f3b8);
  if (s32(b.u32(0x44fdc0))<2) b.w32(obj+0x248,bits|1);
  b.wvec(obj+0x250,b.vec(obj+0x10));b.w32(obj+0x25c,2);b.w8(obj+0x260,0);
}
void gold_ctor2(A::Model &m,Address obj,const Vec3 &pos,Raw amount) {
  B b{m};b.gameobject(obj,pos);b.w32(obj+0x48,0x511a88);
  b.w8(obj+0x5d,1);b.w8(obj+0x4c,0x29);b.w32(obj+0x80,amount);
  b.w32(obj+0xc,b.u32(obj+0xc)|0x220c);
  b.call("anim_clear",{iv(obj+0x58)});
  b.w32(obj+0x50,b.u32(0x44fdcc));
  auto clip=b.call("resref_get",{iv(0x6e01b0)});
  b.call("anim_push",{iv(obj+0x58),clip,iv(0x204)});
  b.call("obj_runEveryFrame",{iv(obj)});
}
} // namespace champions::simulation::rta::combat_ctors
