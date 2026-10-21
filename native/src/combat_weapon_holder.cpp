// Authored from research-combat-ctors-3 9ff6ae0, weapon-holder.md and
// ref_weapon_holder.py. Weapon2 remains a separate, refused table.
#include "champions/combat_ctors.hpp"
#include "champions/core_math.hpp"

namespace champions::simulation::rta::combat_ctors {
namespace {
using A::Value;
Value iv(std::uint64_t n) { return std::int64_t(n&0xffffffffu); }
std::int64_t number(const Value &v) {
  if (auto p=std::get_if<std::int64_t>(&v)) return *p;
  if (auto p=std::get_if<A::CallbackResult>(&v))
    if (p->kind==A::CallbackResult::Kind::integer)
      return p->negative ? -std::int64_t(p->magnitude) : std::int64_t(p->magnitude);
  throw A::Error("weapon holder integer callback required");
}
struct B {
  A::Model &m;
  Raw u8(Address a) const {return Raw(m.mem.load(a,1));}
  Raw u16(Address a) const {return Raw(m.mem.load(a,2));}
  Raw u32(Address a) const {return Raw(m.mem.load(a,4));}
  std::uint64_t u64(Address a) const {return m.mem.load(a,8);}
  Vec3 vec(Address a) const {return {u32(a),u32(a+4),u32(a+8)};}
  void w8(Address a,Raw v) {m.store(a,1,v);}
  void w16(Address a,Raw v) {m.store(a,2,v);}
  Value call(const char *name,std::vector<Value> a) {return m.external_call(name,std::move(a));}
};
Raw arg(const std::vector<Value> &a,unsigned k) {return k<a.size()?Raw(number(a[k])):0;}
bool first_hand(Raw which) {return s32(which)>=0&&s32(which)<2;}
constexpr Raw IDS[7]={0,0x15,0x16,0x17,0x18,0x26,0x1f};
constexpr Address SKILLS[7]={0,0x41b1a4,0x41b1a8,0x41b1d8,0x41b1a0,0x41b1d4,0x41b1a4};
} // namespace

void build_element_damage(A::Model &m,Address desc,Raw kind) {
  B b{m}; kind&=255;
  for (int k=3;k>=0;--k) b.w16(desc+0x24+8*k,0xffff);
  if (kind<1||kind>6) return;
  auto skill=b.u32(SKILLS[kind]);
  auto level=to_int(b.u32(0x4f714c));
  auto base=Raw(number(m.table_call("Skill",skill,0x60,11,{iv(0),iv(Raw(level)),iv(0)})));
  auto d=m.draw_shared();
  auto scaled=mul(from_int(base),0x3e4ccccc);
  auto value=to_int(mul(scaled,mul(from_int(d),TWO_M16)));
  b.w16(desc+0x24,IDS[kind]);
  b.w16(desc+0x2a,0);
  b.w16(desc+0x26,base);
  b.w16(desc+0x28,Raw(base+value));
}
Raw damage_desc_get_element(A::Model &m,Address desc) {
  B b{m};Raw out=0;
  for (unsigned k=0;k<4;++k) {
    auto id=b.u16(desc+0x24+8*k);
    if (id==0x15) out=1;
    else if (id==0x16) out=2;
    else if (id==0x17) out=3;
    else if (id==0x1f) out=6;
    else if (id==0x18) out=4;
  }
  return out;
}
void drop_held_part(A::Model &m,Raw part,Address c,Address definition,Raw flags) {
  B b{m};
  if (!part||!c||(b.u32(c+0xc)&2)) return;
  if (!definition) definition=b.u32(c+0x24);
  auto rec=Address(definition+8*part);
  auto record=b.u32(rec+4);
  auto bone=b.u8(record+0x13);
  auto signed_bone=bone&0x80 ? std::int32_t(bone)-256 : std::int32_t(bone);
  auto model=Raw(number(m.vcall(c,16)));
  auto position=b.vec(c+0x10);
  auto v=b.call("model_getAttachPoint",{iv(ONE),iv(model),position,
              iv(Raw(s16(b.u16(c+0x38)-0x4000))),iv(c+0x114),iv(Raw(signed_bone))});
  if (auto p=std::get_if<Vec3>(&v)) position=*p;
  else if (auto p=std::get_if<A::Sample>(&v)) position=p->point;
  if (!flags) flags=0x4001;
  if (b.u64(c+0xa8)&UINT64_C(0x101000000)) flags|=0x100;
  auto block=Raw(number(b.call("pool_new",{iv(0x248)})));
  if (block) b.call("PhProp_ctor",{iv(b.u32(c+0xdc)),iv(block),position,
                     iv(b.u32(definition)),iv(b.u32(rec)),iv(b.u32(rec+8)),
                     iv(1),iv(flags),iv(0)});
}

Raw holder_slot(A::Model &m,Address h,unsigned slot,const std::vector<Value> &args) {
  B b{m};
  auto table=b.u32(h);
  bool shield=table==VT_WEAPONSHIELD;
  if (!shield&&table!=0x50cf40)
    throw A::Unsupported("Weapon2 or unknown holder table is not executed in round 3");
  auto which=arg(args,0);
  auto hand=s32(which);
  auto bits=[&]{return b.u8(h+4);};
  auto put=[&](Raw x){b.w8(h+4,x);};
  auto drop=[&](Raw part){drop_held_part(m,part,arg(args,1),arg(args,2),arg(args,3));};
  switch (slot) {
  case 1: {
    auto part=arg(args,1)&255;
    if (hand==2&&shield) {
      b.w8(h+0x98,part);put(part?bits()|2:bits()&~2u);
    } else if (hand==1) {
      b.w8(h+8,part);put(part?bits()|1:bits()&~1u);
    }
    break;
  }
  case 2:case 3:case 4:case 5:case 6:case 7:
    if (first_hand(which)) {
      constexpr Raw kind[6]={1,6,2,3,4,5};
      build_element_damage(m,h+0x50,kind[slot-2]);
    }
    break;
  case 8:
    if (first_hand(which)) build_element_damage(m,h+0x50,arg(args,1));
    break;
  case 9:case 10:
    if (first_hand(which)) {
      auto base=h+(slot==9?0x60:0x68);
      b.w16(base+6,arg(args,4)&255);
      for (unsigned k=0;k<3;++k) b.w16(base+2*k,arg(args,k+1)&255);
    }
    break;
  case 11:
    if (first_hand(which)&&(bits()&1)) {drop(b.u8(h+8));put(bits()&~1u);}
    if (shield&&(hand==0||hand==2)&&(bits()&2)) {drop(b.u8(h+0x98));put(bits()&~2u);}
    break;
  case 12:
    if (first_hand(which)) put(bits()|1);
    if (shield&&(hand==0||hand==2)) put(bits()|2);
    break;
  case 13:
    if (first_hand(which)) put(shield?bits()|1:bits()&~1u);
    if (shield&&(hand==0||hand==2)) put(bits()&~2u);
    break;
  case 14:
    return hand==1 ? bits()&1 : shield&&hand==2 ? (bits()>>1)&1 : 0;
  case 15:
    return hand==1 ? b.u8(h+8) : shield&&hand==2 ? b.u8(h+0x98) : 0;
  case 16:
    return hand==1 ? damage_desc_get_element(m,h+0x50) : 0;
  case 17:
    return hand==1 ? h+0x50 : 0;
  default:
    throw A::Unsupported("holder slot outside executed 1-17");
  }
  return 0;
}
} // namespace champions::simulation::rta::combat_ctors
