// Executed PhProp_run and GutChunk_run, research-combat-ctors-4 6baf4cd,
// docs/spec/phprop-run.md, tools/ref_phprop_run.py. Raw words retain EE
// single-precision operation order. World terrain and pools are call-outs.
#include "champions/combat_ctors.hpp"
#include "champions/core_math.hpp"
#include <array>
#include <string>

namespace champions::simulation::rta::combat_ctors {
namespace {
using A::Value;
Value iv(std::uint64_t x) { return std::int64_t(x&0xffffffffu); }
Raw number(const Value &v) {
  if (auto p=std::get_if<std::int64_t>(&v)) return Raw(*p);
  if (auto p=std::get_if<A::CallbackResult>(&v))
    if (p->kind==A::CallbackResult::Kind::integer)
      return Raw(p->negative?-std::int64_t(p->magnitude):std::int64_t(p->magnitude));
  throw A::Error("PhProp_run integer callback required");
}
struct B {
  A::Model &m;
  Raw u8(Address a) const {return Raw(m.mem.load(a,1));}
  Raw u16(Address a) const {return Raw(m.mem.load(a,2));}
  Raw u32(Address a) const {return Raw(m.mem.load(a,4));}
  std::uint64_t u64(Address a) const {return m.mem.load(a,8);}
  int s8m(Address a) const {auto x=u8(a);return x&0x80?int(x)-256:int(x);}
  int s16m(Address a) const {return s16(u16(a));}
  Vec3 vec(Address a) const {return {u32(a),u32(a+4),u32(a+8)};}
  Mat3 mat3(Address a) const {
    Mat3 r{};for (unsigned i=0;i<9;++i) r[i]=u32(a+4*i);return r;
  }
  void w8(Address a,Raw x) {m.store(a,1,x);}
  void w16(Address a,Raw x) {m.store(a,2,x);}
  void w32(Address a,Raw x) {m.store(a,4,x);}
  void w64(Address a,std::uint64_t x) {m.store(a,8,x);}
  void wvec(Address a,const Vec3 &v) {for (unsigned i=0;i<3;++i) w32(a+4*i,v[i]);}
  void wmat(Address a,const Mat3 &r) {for (unsigned i=0;i<9;++i) w32(a+4*i,r[i]);}
  Value call(const char *name,std::vector<Value> args) {return m.external_call(name,std::move(args));}
  Raw draw(Address obj) {return m.own_draw(obj);}
  static Vec3 cross(const Vec3 &a,const Vec3 &b) {
    return {sub(mul(a[1],b[2]),mul(a[2],b[1])),
            sub(mul(a[2],b[0]),mul(a[0],b[2])),
            sub(mul(a[0],b[1]),mul(a[1],b[0]))};
  }
  static Raw dot(const Vec3 &a,const Vec3 &b) {
    return add(add(mul(a[0],b[0]),mul(a[1],b[1])),mul(a[2],b[2]));
  }
  Vec3 point_velocity(Address body,const Vec3 &point) const {
    Vec3 r{},out{};
    for (unsigned i=0;i<3;++i) r[i]=sub(point[i],u32(body+0x10+4*i));
    out=cross(vec(body+0x8c),r);
    for (unsigned i=0;i<3;++i)
      out[i]=add(out[i],mul(u32(body+0x2c+4*i),u32(body)));
    return out;
  }
  void add_force(Address body,const Vec3 &point,const Vec3 &force,
                 Vec3 &sum,Vec3 &torque) const {
    Vec3 r{};
    for (unsigned i=0;i<3;++i) {
      sum[i]=add(sum[i],force[i]);
      r[i]=sub(point[i],u32(body+0x10+4*i));
    }
    auto t=cross(r,force);
    for (unsigned i=0;i<3;++i) torque[i]=add(torque[i],t[i]);
  }
  void impulse(Raw e,Address body,const Vec3 &point,const Vec3 &normal) {
    auto v=point_velocity(body,point);
    auto vn=dot(v,normal);
    auto numerator=sub(mul(vn,e),vn);
    if (le(numerator,ZERO)) return;
    Vec3 r{};
    for (unsigned i=0;i<3;++i) r[i]=sub(point[i],u32(body+0x10+4*i));
    auto c=cross(r,normal);
    auto iw=mat3(body+0x68);
    auto u=mat3_mul_vec_transposed(iw,c);
    auto w=cross(u,r);
    auto j=div(numerator,add(u32(body),dot(w,normal)));
    Vec3 J{};
    for (unsigned i=0;i<3;++i) J[i]=mul(normal[i],j);
    auto t=cross(r,J);
    for (unsigned i=0;i<3;++i) {
      w32(body+0x2c+4*i,add(u32(body+0x2c+4*i),J[i]));
      w32(body+0x38+4*i,add(u32(body+0x38+4*i),t[i]));
    }
    wvec(body+0x8c,mat3_mul_vec_transposed(iw,vec(body+0x38)));
  }
  void derive(Address body,const Quat &q) {
    auto r=mat3_transpose(mat3_from_quat(q));
    wmat(body+0x44,r);
    Mat3 scaled{};
    for (unsigned row=0;row<3;++row)
      for (unsigned col=0;col<3;++col)
        scaled[3*row+col]=mul(r[3*row+col],u32(body+4+4*col));
    auto iw=mat3_mul(scaled,mat3_transpose(r));
    wmat(body+0x68,iw);
    wvec(body+0x8c,mat3_mul_vec_transposed(iw,vec(body+0x38)));
  }
  void integrate(Address body,const Vec3 &force,const Vec3 &torque) {
    Quat q{},omega{};
    for (unsigned i=0;i<4;++i) q[i]=u32(body+0x1c+4*i);
    for (unsigned i=0;i<3;++i) omega[i]=u32(body+0x8c+4*i);
    omega[3]=ZERO;
    auto dq=quat_mul(omega,q);
    for (unsigned i=0;i<4;++i) q[i]=add(q[i],mul(dq[i],HALF));
    for (unsigned i=0;i<4;++i) w32(body+0x1c+4*i,q[i]);
    for (unsigned i=0;i<3;++i) {
      w32(body+0x2c+4*i,add(u32(body+0x2c+4*i),force[i]));
      w32(body+0x38+4*i,add(u32(body+0x38+4*i),torque[i]));
    }
    q=quat_normalize(q);
    for (unsigned i=0;i<4;++i) w32(body+0x1c+4*i,q[i]);
    derive(body,q);
  }
  Address sound(Address obj) const {
    if (!s8m(obj+0x238)) {
      auto bits=u16(obj+0x140);
      return bits&8?0x5326c0:bits&0x10?0x5326d8:0;
    }
    auto definition=u32(obj+0x148);
    auto flags=u64(obj+0x150);
    if (!s8m(definition+0x1d))
      return flags&0x800000?0x5326c0:flags&0x40000?0x5326d8:0;
    if (s8m(definition+0x1c)==3)
      return flags&0x840000?0x5326f0:0;
    for (auto entry:{std::pair<std::uint64_t,Address>{0x800000,0x5326c0},
                    {0x200000,0x532708},{0x40000,0x532720},
                    {0x100000,0x532738},{0x80000,0x532750},
                    {0x1000000,0x532768},{0x4000000,0x532780},
                    {0x2000000,0x532780},{0x10000000,0x532780}})
      if (flags&entry.first) return entry.second;
    return 0;
  }
  void copy_text(Address to,Address from) {
    for (unsigned i=0;i<256;++i) {
      auto c=u8(from+i);w8(to+i,c);
      if (!c) return;
    }
    throw A::Error("resting prop resource name exceeds 255 bytes");
  }
  void resting_ctor(Address newobj,Address obj,bool item,bool named) {
    auto pos=vec(obj+0x10);
    Address lump=obj+0x54,model=obj+0x64,texture=obj+0xa4;
    if (item) {
      auto a=number(call("item_getResourceIndex",{iv(obj+0x148)}));
      auto b=number(call("item_getResourceIndex",{iv(obj+0x148)}));
      lump=0x5326b0;model=u32(0x3e9ce0+16*a);texture=u32(0x3e9ce4+16*b);
    }
    if (auto cb=m.callbacks.find("World_gameobject_prepare");cb!=m.callbacks.end())
      cb->second(m,{iv(newobj)});
    for (auto off:{0u,4u,8u,12u,0x34u}) w32(newobj+off,0);
    w32(newobj+0x48,0x5122a8);
    wvec(newobj+0x10,pos);
    w32(newobj+0x1c,0x41c00000);w32(newobj+0x20,0x42900000);
    w8(newobj+0x28,0x19);w8(newobj+0x29,0x49);
    auto col=(s32(to_int(pos[0]))+0x3c00)>>8;
    auto row=(s32(to_int(pos[1]))+0x3c00)>>8;
    auto cell=col>=0&&col<0x78&&row>=0&&row<0x78
        ? Raw(0x5db4f0+4*(row*0x78+col)):Raw(0x5f76f0);
    call("handle_link",{iv(cell),iv(newobj+8),iv(newobj)});
    if (number(call("obj_getPropInt",{iv(newobj),iv(0x5321a8),iv(0)})))
      throw A::Unsupported("hidden resting prop grid move is unexecuted");
    w32(newobj+0x2c,0);w32(newobj+0x44,m.draw_shared());
    w32(newobj+0x48,0x511d58);
    w32(newobj+0xc,0x1809|(named?0:0x4000));
    call("obj_updateGridCell",{iv(newobj)});
    auto bits=item?Raw(0):u16(obj+0x140);
    w16(newobj+0x140,bits|1);
    w32(newobj+0x1c,0x40800000);
    for (unsigned i=0;i<12;++i) w32(newobj+0x110+4*i,u32(obj+0x110+4*i));
    if (named) {
      copy_text(newobj+0x54,lump);
      copy_text(newobj+0x64,model);
      copy_text(newobj+0xa4,texture);
      w32(newobj+0x4c,number(call("res_find",{iv(newobj+0x54),iv(newobj+0x64)})));
      w32(newobj+0x50,number(call("res_find",{iv(newobj+0x54),iv(newobj+0xa4)})));
    } else {
      w32(newobj+0x4c,u32(obj+0x4c));w32(newobj+0x50,u32(obj+0x50));
    }
    w64(newobj+0xe8,item?UINT64_MAX:u64(obj+0xe8));
    if (bits&2) {
      call("obj_runEveryFrame",{iv(newobj)});
      w32(newobj+0xc,u32(newobj+0xc)|0x4000);
    }
    if (bits&4) w32(newobj+0xc,u32(newobj+0xc)|0x1000);
    if (item) {
      w32(newobj+0x48,0x511c38);
      for (unsigned k=0;k<4;++k) w16(newobj+0x16c+8*k,0xffff);
      w8(newobj+0x169,0xff);w8(newobj+0x168,0xff);
      for (unsigned k=0;k<4;++k) {
        w16(newobj+0x158+2*k,0);w16(newobj+0x160+2*k,0);
      }
      for (unsigned k=0;k<0x48;k+=8) w64(newobj+0x148+k,u64(obj+0x148+k));
      auto flags=(u32(newobj+0xc)|0x102000)&~0x1800u;
      if (u64(newobj+0x150)&UINT64_C(0x8000000000)) flags&=~0x3000u;
      w32(newobj+0xc,flags);
      call("obj_updateGridCell",{iv(newobj)});
    }
  }
  void rest(Address obj) {
    auto r=mat3_transpose(mat3(obj+0x1d4));
    auto scaled=mat3_scale(u32(obj+0x240),r);
    Mat34 matrix{};
    for (unsigned k=0;k<9;++k) matrix[k]=scaled[k];
    auto position=vec(obj+0x1a0);
    for (unsigned k=0;k<3;++k) matrix[9+k]=position[k];
    matrix=mat34_translate_local(neg(u32(obj+0xf0)),neg(u32(obj+0xf4)),
                                neg(u32(obj+0xf8)),matrix);
    for (unsigned k=0;k<12;++k) w32(obj+0x110+4*k,matrix[k]);
    bool item=s8m(obj+0x238)!=0;
    auto block=number(call("pool_new",{iv(item?0x190:0x148)}));
    if (block) {
      bool named=item||s8m(obj+0x54)!=0;
      resting_ctor(block,obj,item,named);
    }
    call("obj_delete",{iv(obj)});
  }
};
Raw kind(Raw bits,Raw a,Raw b,Raw c) {return bits&4?a:bits&2?b:c;}
void decal(B &b,Address obj,const Vec3 &spot,Raw type,Raw flag,bool second) {
  auto angle=Raw(std::int32_t(s16(b.draw(obj))));
  if (second) b.draw(obj);
  b.call("decal_add",{spot,iv(angle),iv(type),iv(flag),iv(0x41400000)});
}
} // namespace

void phprop_run(A::Model &m,Address obj) {
  B b{m};
  if (!b.s8m(obj+0x239)&&!(b.u32(0x44fdcc)&7)) {
    if (number(b.call("water_test",{iv(b.u32(0x44fdc8)),b.vec(obj+0x10),
                                    iv(0),iv(0x41400000)}))) {
      b.w8(obj+0x239,1);b.draw(obj);
      auto sound=b.call("libc_sprintf",{iv(0x532458),iv(1)});
      auto entry=b.call("res_find",{iv(0x532478),sound});
      b.call("sound_playOnObject",{entry,iv(obj),iv(0x3e4ccccc)});
      b.call("water_disturb",{iv(b.u32(0x44fdc8)),b.vec(obj+0x10),
                              iv(0x442f0000),iv(0x41c00000)});
    }
  }
  auto body=obj+0x190;
  auto grav=b.u32(0x44fe08)&0x100?Raw(0x3d5d2f18):Raw(0x3e3851e9);
  b.w32(obj+0x1c4,sub(b.u32(obj+0x1c4),div(grav,b.u32(body))));
  auto level=add(b.u32(obj+0x228),0x40000000);
  b.w32(obj+0x230,0);
  Vec3 force{},torque{};bool contact=false;
  auto mask=b.u64(obj+0xe8);
  for (unsigned mesh=0;mask;++mesh,mask>>=1) if (mask&1) {
    auto model=b.u32(obj+0x4c);
    auto k=b.u8(model+0x25);
    if (!k) throw A::Unsupported("model_getHullPoints returned null");
    auto points=Address(model+16*k+24*mesh);
    for (unsigned j=0;j<8;++j) {
      Vec3 p{};
      for (unsigned i=0;i<3;++i)
        p[i]=sub(from_int(b.s8m(points+3*j+i)),b.u32(obj+0xf0+4*i));
      auto height=add(add(add(mul(p[0],b.u32(obj+0x1ec)),
                              mul(p[1],b.u32(obj+0x1f0))),
                          mul(p[2],b.u32(obj+0x1f4))),b.u32(obj+0x1a8));
      if (le(level,height)) continue;
      auto local=mat3_mul_vec_transposed(b.mat3(obj+0x1d4),p);
      Vec3 point{};
      for (unsigned i=0;i<3;++i) point[i]=add(local[i],b.u32(obj+0x1a0+4*i));
      auto countdown=b.u32(obj+0x234);
      if (countdown) {b.w32(obj+0x234,countdown-1);continue;}
      if (!lt(point[2],level)) continue;
      contact=true;
      auto vz=b.point_velocity(body,point)[2];
      if (lt(vz,0xbf800000)&&!b.s8m(obj+0x239)) {
        auto sound=b.sound(obj);
        if (sound) {
          auto entry=b.call("res_find",{iv(0x532478),iv(sound)});
          b.call("sound_playOnObject",{entry,iv(obj),iv(ONE)});
        }
      }
      if (lt(vz,ZERO)) {
        b.impulse(b.u32(obj+0x23c),body,point,{ZERO,ZERO,ONE});
        b.w32(obj+0x230,1);
      }
      auto depth=sub(level,point[2]);
      if (lt(ONE,depth)) depth=ONE;
      auto push=mul(depth,0x3bc49ba5);
      if (lt(ZERO,push))
        b.add_force(body,point,{ZERO,ZERO,push},force,torque);
    }
  }
  if (b.u16(obj+0x140)&0x20) for (auto pair:{std::pair<unsigned,unsigned>{0x1c8,0x194},
                                              {0x1cc,0x198},{0x1d0,0x19c}}) {
    auto inertia=b.u32(obj+pair.second);
    if (lt(0x3d4ccccc,mul(b.u32(obj+pair.first),inertia)))
      b.w32(obj+pair.first,div(0x3d4ccccc,inertia));
    if (lt(mul(b.u32(obj+pair.first),inertia),0xbd4ccccc))
      b.w32(obj+pair.first,div(0xbd4ccccc,inertia));
  }
  b.integrate(body,force,torque);
  if (contact) for (auto off:{0x1bcu,0x1c0u})
    b.w32(obj+off,mul(b.u32(obj+off),0x3f733333));
  for (auto off:{0x1d0u,0x1c8u,0x1ccu})
    b.w32(obj+off,mul(b.u32(obj+off),0x3f733333));
  b.wvec(obj+0x10,b.vec(obj+0x1a0));
  auto invmass=b.u32(body);
  b.w32(obj+0x1a8,add(b.u32(obj+0x1a8),mul(b.u32(obj+0x1c4),invmass)));
  Vec3 delta{mul(b.u32(obj+0x1bc),invmass),mul(b.u32(obj+0x1c0),invmass),ZERO};
  auto ground=number(b.call("terrain_heightBelow",{iv(b.u32(0x44fdc8)),b.vec(obj+0x10),
                                                    iv(b.u32(obj+0x1c))}));
  b.w32(obj+0x228,ground);
  if (le(sub(b.u32(obj+0x18),0x3c23d70a),ground)) b.w32(obj+0x18,ground);
  if (lt(0x3a83126e,add(mul(delta[0],delta[0]),mul(delta[1],delta[1])))) {
    auto sweep=b.call("terrain_sweepCircle",{iv(b.u32(0x44fdc8)),b.vec(obj+0x10),
                                              delta,iv(1),iv(b.u32(obj+0x1c))});
    auto p=std::get_if<A::CallbackResult>(&sweep);
    if (!p||p->elements.size()!=3) throw A::Error("terrain sweep needs fraction and normal");
    auto fraction=Raw(p->elements[0].magnitude),nx=Raw(p->elements[1].magnitude),
         ny=Raw(p->elements[2].magnitude);
    for (unsigned i:{1u,2u,0u})
      b.w32(obj+0x10+4*i,add(b.u32(obj+0x10+4*i),mul(delta[i],fraction)));
    b.call("obj_updateGridCell",{iv(obj)});
    if (lt(fraction,ONE)) {
      auto px=b.u32(obj+0x1bc),py=b.u32(obj+0x1c0);
      auto a=sub(0x3a83126e,add(mul(nx,px),mul(ny,py)));
      if (lt(ZERO,a)) {
        b.w32(obj+0x1c0,add(py,mul(mul(ny,a),0x3fc00000)));
        b.w32(obj+0x1bc,add(px,mul(mul(nx,a),0x3fc00000)));
      }
    }
    if (b.s8m(obj+0x23a)) {
      auto v=b.point_velocity(body,b.vec(obj+0x10));
      b.call("fx_impactPuffNearCamera",{b.vec(obj+0x1a0),v,iv(0),
            iv(mul(from_int(b.u8(obj+0x28)),0x3f4ccccc)),iv(ONE)});
    }
  }
  b.w32(obj+0x1a0,b.u32(obj+0x10));
  b.w32(obj+0x1a4,b.u32(obj+0x14));
  b.call("obj_updateGridCell",{iv(obj)});
  constexpr unsigned speeds[]{0x1c8,0x1cc,0x1d0,0x1bc,0x1c0,0x1c4};
  auto all_below=[&](Raw limit) {
    for (auto off:speeds) if (!lt(fabs(b.u32(obj+off)),limit)) return false;
    return true;
  };
  if (b.s8m(obj+0x23a)==1&&
      lt(fabs(sub(b.u32(obj+0x18),ground)),0x41400000)&&all_below(0x3f4ccccc)) {
    b.w8(obj+0x23a,2);
    auto block=number(b.call("pool_new",{iv(0x4ac)}));
    if (block) {
      auto draw=b.draw(obj);
      b.call("StaticFire_ctor",{iv(block),b.vec(obj+0x10),
            iv((draw%40)*60+480),iv(0),iv(0),iv(0x43700000),iv(HALF)});
    }
  }
  auto life=b.s16m(obj+0x10e);
  bool rest=life<-220;
  if (!rest) {
    b.w16(obj+0x10e,Raw(life-1));
    rest=life<=0&&all_below(0x3e4ccccc);
  }
  if (rest) {b.rest(obj);return;}
  if (s32(b.u32(0x4d8ae0))>0) {
    auto slot=s32(b.u32(0x4f8440));
    if (slot>=0) {
      auto player=b.u32(0x4dcec8+slot*0x4c30);
      if (player&&lt(add(b.u32(obj+0x18),0x42f00000),b.u32(player+0x18)))
        b.call("obj_delete",{iv(obj)});
    }
  }
}

void gutchunk_run(A::Model &m,Address obj) {
  B b{m};
  phprop_run(m,obj);
  if (lt(ONE,b.u32(obj+0x1c4))) {
    auto d=b.draw(obj);
    if (!(d%3)&&b.u32(0x44fdc0)) {
      auto def=kind(b.u32(obj+0x248),0x870458,0x870408,0x8703b8);
      b.call("particle_spawn",{iv(def),b.vec(obj+0x10),Vec3{},iv(ZERO)});
    }
  }
  auto pos=b.vec(obj+0x10);
  auto h=number(b.call("terrain_height",{iv(b.u32(0x44fdc8)),pos,iv(0x41000000)}));
  Vec3 spot{pos[0],pos[1],h};
  auto last=b.u32(obj+0x24c),bits=b.u32(obj+0x248),vz=b.u32(obj+0x1c4);
  if ((bits&9)==1&&lt(h,pos[2])&&lt(0xc61c4000,h)&&
      ((lt(0xbf800000,last)&&lt(vz,0xbf800000))||
       (lt(0xc0000000,last)&&lt(vz,0xc0000000)))) {
    auto angle=Raw(std::int32_t(s16(b.draw(obj))));
    auto type=kind(bits,4,7,1)+(b.draw(obj)&1);
    b.call("decal_add",{spot,iv(angle),iv(type),iv(0),iv(0x41400000)});
    last=b.u32(obj+0x24c);
  }
  if (lt(last,ZERO)&&lt(ZERO,vz)) {
    if (lt(last,0xbf800000)) {
      auto entry=b.call("res_find",{iv(0x53e3c0),iv(0x53e4a0)});
      b.call("sound_playOnObject",{entry,iv(obj),iv(HALF)});
    }
    bits=b.u32(obj+0x248);
    if (bits&1) {
      auto table=b.u32(obj+0x48);
      auto self=Address(obj+b.s16m(table+8));
      auto fn=b.u32(table+0xc);
      b.call(fn==0x3c4d48?"GutChunk_dtor":"PhProp_dtor",{iv(self),iv(3)});
      return;
    }
    auto count=b.u32(obj+0x25c);
    if (count) {
      if (!(bits&8)&&lt(h,pos[2])&&lt(0xc61c4000,h)) {
        decal(b,obj,spot,kind(bits,4,7,1),0,true);
        bits=b.u32(obj+0x248);count=b.u32(obj+0x25c);
      }
      b.w32(obj+0x25c,count-1);
    }
  }
  if (!(bits&1)&&!b.s8m(obj+0x260)&&lt(b.u32(obj+0x1bc),0x3dcccccc)&&
      lt(b.u32(obj+0x1c0),0x3dcccccc)) {
    b.w8(obj+0x260,1);
    if (!(bits&8)&&le(h,pos[2])&&lt(0xc61c4000,h))
      decal(b,obj,spot,kind(bits,3,6,0),1,false);
  }
  b.w32(obj+0x24c,b.u32(obj+0x1c4));
}
} // namespace champions::simulation::rta::combat_ctors
