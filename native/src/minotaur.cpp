#include "champions/minotaur.hpp"
#include <limits>

namespace champions::simulation::rta::minotaur {
const std::array<EventParams, 8> EVENTS{{
    {5,{1,0,0,0},{0x3e4ccccc,0,0,0}},
    {6,{1,1,0,0},{0x3e4ccccc,0x3f199999,0,0}},
    {7,{1,0,0,0},{0x3ecccccc,0,0,0}},
    {8,{1,4,0,0},{0x3e4ccccc,0x3eb851eb,0,0}},
    {9,{1,0,0,0},{0x3e4ccccc,0,0,0}},
    {13,{1,0,0,0},{0x3e4ccccc,0,0,0}},
    {3,{1,0,0,0},{0x3f199999,0,0,0}},
    {4,{1,0,0,0},{0x3f199999,0,0,0}},
}};
namespace {
using A::Address;
using A::Value;
Value iv(std::uint64_t n) { return std::int64_t(n & UINT32_MAX); }
Raw number(const Value &v) {
  if (auto p = std::get_if<std::int64_t>(&v)) return Raw(*p);
  if (auto p = std::get_if<A::CallbackResult>(&v))
    if (p->kind == A::CallbackResult::Kind::integer)
      return Raw(p->negative ? 0 - p->magnitude : p->magnitude);
  throw A::Error("Minotaur: integer service result required");
}
struct Body {
  A::Model &m;
  Address c;
  Raw u(Raw off) const { return Raw(m.mem.load(c + off, 4)); }
  Raw h(Raw off) const { return Raw(m.mem.load(c + off, 2)); }
  std::uint64_t flags() const { return m.mem.load(c + 0xa8, 8); }
  void w(Raw off, unsigned n, std::uint64_t value) { m.store(c + off, n, value); }
  Value call(const char *name, std::vector<Value> args) {
    return m.external_call(name, std::move(args));
  }
  Raw query(const char *name) { return number(call(name, {iv(c)})); }
  Raw clip(unsigned index) const { return m.clip(c, 12 + 8 * index); }
  Raw facing(int offset = 0) const { return Raw(s16(h(0x38) + Raw(offset))); }
  A::Vec3 position() const { return {u(0x10),u(0x14),u(0x18)}; }
  bool middle() const {
    return lt(u(0x228), mul(u(0x22c), 0x3f28f5c2)) &&
           lt(mul(u(0x22c), 0x3ea8f5c2), u(0x228));
  }
  void change(unsigned index, Raw bits, Raw blend = 0x3dcccccc) {
    call("anim_start", {iv(c+0x114),iv(clip(index)),iv(bits),iv(blend)});
  }
  void blend(unsigned index, Raw bits) {
    call("anim_startBlendInOut", {iv(c+0x114),iv(clip(index)),iv(bits),
                                 iv(0x3dcccccc),iv(0x3dcccccc)});
  }
  void sync() { call("anim_syncToSimFrame", {iv(c+0x114)}); }
  bool hand(Raw which) {
    const auto holder = u(0x50);
    return holder && number(m.holder_call(holder,14,{iv(which)}));
  }
  void drop_hands() {
    for (Raw which : {1u,2u})
      if (auto holder = u(0x50))
        m.holder_call(holder,11,{iv(which),iv(c),iv(0),iv(0)});
  }
  void state(Raw next) { m.vcall(c,27,{iv(next)}); }
  bool see(Raw near, Raw far, int offset = 0, Raw arc = 8000) {
    return number(call("Creature_canSeeTargetIn", {iv(c),iv(facing(offset)),iv(arc),
                         iv(0),iv(near),iv(far),iv(0)})) != 0;
  }
  void turn() { call("Creature_turnToTarget",{iv(c)}); }
  void settle() { call("Creature_settle",{iv(c)}); }
  void move_zero() { call("Creature_moveBy",{iv(c),A::Vec3{}}); }
  void hit(Raw bits, bool descriptor) {
    Value desc = std::int64_t(0);
    if (descriptor) desc = m.weapon_descriptor(c);
    call("Creature_hitTargetInArc", {iv(c),iv(facing()),iv(8000),
         iv(Raw(s16(h(0xd0)))),iv(Raw(s16(h(0xd2)))),iv(Raw(s16(h(0xd4)))),
         iv(bits),desc,iv(0),iv(0x42100000)});
  }
};
} // namespace

std::uint64_t hide_mask(const std::uint8_t *data, std::size_t size, Raw selector) {
  auto get = [&](std::size_t off, unsigned n) {
    if (!data || off > size || n > size - off)
      throw A::Unsupported("model_buildHideMask: truncated metadata");
    std::uint64_t value = 0;
    for (unsigned k=0;k<n;++k) value |= std::uint64_t(data[off+k]) << (8*k);
    return value;
  };
  auto mask = ~get(8,8);
  if (get(0x10,2) & 2) {
    const auto base = 16 * get(0x21,1);
    for (unsigned bit=0;bit<32;++bit)
      if (selector & (1u<<bit)) {
        mask |= get(std::size_t(base + 16*bit),8);
        mask &= ~get(std::size_t(base + 16*bit + 8),8);
      }
  }
  return mask;
}

void finish_constructor(A::Model &m, Address c, const std::string &type_name) {
  Body b{m,c};
  b.w(0x48,4,VTABLE);
  const Raw table = 0x50cda0;
  m.init_holder(c,c+0x3c8,&table);
  b.w(0x50,4,c+0x3c8);
  b.w(0xb0,4,0x3f999999);
  b.w(0x1c,4,0x41c80000);
  b.w(0xb8,4,0);
  b.w(0xb4,1,11);
  const auto frame = Raw(m.mem.load(0x44fdcc,4));
  b.w(0xec,4,frame);
  b.w(0xf4,4,frame+300u);
  b.w(0xa8,8,b.flags() | UINT64_C(0x200040000));
  b.w(0x232,2,0);
  b.w(0x46c,4,0);
  std::string lower = type_name;
  for (auto &ch : lower) if (ch>='A' && ch<='Z') ch = char(ch+32);
  const bool boss = lower == "minotaurboss";
  m.holder_call(c+0x3c8,1,{iv(1),iv(21)});
  m.holder_call(c+0x3c8,1,{iv(2),iv(boss?23:25)});
  const Raw variant = boss ? 4u : 1u << (m.own_draw(c)&1u);
  b.w(0xe8,4,variant);
  const auto mask = b.call("model_buildHideMask",{iv(m.class_model(c)),iv(variant)});
  if (auto p=std::get_if<std::int64_t>(&mask)) b.w(0x220,8,std::uint64_t(*p));
  else throw A::Error("Minotaur: u64 hide mask required");
  b.call("anim_clear",{iv(c+0x114)});
  set_state(m,c,0);
  b.call("obj_runEvery60",{iv(c)});
  for (const auto &e : EVENTS)
    b.call("creature_setDefEntryParams",{iv(c),iv(e.entry),iv(e.masks[0]),
        iv(e.masks[1]),iv(e.masks[2]),iv(e.masks[3]),iv(e.times[0]),
        iv(e.times[1]),iv(e.times[2]),iv(e.times[3])});
  b.call("Creature_init",{iv(c)});
}
void constructor(A::Model &m, Address c, Raw type_name, const A::Vec3 &pos,
                 Raw angle, Raw props) {
  m.creature_ctor(c,type_name,pos,Raw(s16(angle)),DEFINITION,props,0x41200000);
  finish_constructor(m,c,m.cstr(type_name));
}
void set_state(A::Model &m, Address c, Raw requested) {
  Body b{m,c};
  auto filtered = m.filter_state(c,requested);
  if (filtered.refused) return;
  auto state = Raw(filtered.state);
  b.w(0xa8,8,b.flags() & ~UINT64_C(0x200));
  switch (state) {
  case 0: b.change(14,3); break;
  case 1: m.clear_path(c); m.pick_wander_dest(c); b.change(19,0x803); b.sync(); break;
  case 3: m.clear_path(c); b.change(b.middle()?19:16,0x803); b.sync(); break;
  case 5: b.change(15,3); b.sync(); break;
  case 6: case 7: case 8: b.blend(state-2,0x808); break;
  case 9: case 10: b.blend(state-2,0x4008); break;
  case 11: b.blend(3,0x4808); break;
  case 12:
    b.change(16,0x80b);
    if (!b.u(0x46c)) { b.blend(18,8); b.w(0x46c,4,1); }
    break;
  case 16:
    b.w(0x318,2,60); m.clear_path(c); b.change(16,0x803); b.sync(); break;
  case 18:
    if (lt(b.u(0x228),mul(b.u(0x22c),0x3ea8f5c2))) b.drop_hands();
    b.blend(13,0x810); break;
  case 19: b.change(14,3,0x3e999999); break;
  case 20:
    b.call("Creature_dropLoot",{iv(c),b.position(),iv(b.facing()),iv(0),
        iv(0x42100000),iv(0x40000000),iv(0x3fcccccc),iv(0x3f199999),
        iv(0x3fb33333),iv(0x3e800000)});
    b.drop_hands();
    b.change(10+(m.own_draw(c)&1),0x806,0x3e4ccccc);
    m.vcall(c,28,{iv(0)});
    break;
  case 23:
    b.w(0xa8,8,b.flags()|0x200);
    b.change(9,3);
    b.w(0xf4,4,b.u(0x300)+m.own_draw(c)%120+60); break;
  default: state=0; break;
  }
  b.w(0x232,2,state);
}
void run(A::Model &m, Address c) {
  Body b{m,c};
  b.call("Creature_processDamage",{iv(c),m.vcall(c,16)});
  if (number(b.call("Creature_run",{iv(c),m.vcall(c,16)}))) return;
  if (!b.query("Creature_updateActive")) return;
  if (auto holder=b.u(0x50)) m.holder_call(holder,21,{iv(DEFINITION),iv(c),iv(HALF)});
  const auto events=number(b.call("Creature_stepAnim",{iv(c),iv(ONE)}));
  b.call("Creature_applyKnockback",{iv(c),iv(0x3f333333)});
  m.vcall(c,23,{iv(0)});
  const auto state=s16(b.h(0x232));
  auto deadline_passed=[&] { return s32(b.u(0x468))<s32(b.u(0x300)); };
  switch (state) {
  case 0:
    if (number(m.vcall(c,25,{iv(0)})) || b.query("Creature_isAwayFromHome")) b.state(1);
    break;
  case 1:
    if (number(m.vcall(c,25,{iv(0)}))) {
      b.turn();
      if (b.see(0,from_int(b.h(0x3ae)))) b.state(3);
    } else b.call("Creature_moveToDest",{iv(c)});
    break;
  case 3:
    if (!b.u(0x234)) { b.state(0); break; }
    if (b.see(0,0x41400000)) { b.state(5); break; }
    if (!b.query("Creature_isTargetCharging")) {
      if (b.see(0,0x42100000)) { b.state(5); break; }
      if (b.see(0,0x42100000,0,32767)) { b.turn(); break; }
    }
    if (!b.hand(1) && deadline_passed() && b.see(0x42700000,0x43960000)) b.state(12);
    else b.call("Creature_moveToTarget",{iv(c)});
    break;
  case 5:
    if (b.middle() && b.see(0,0x42100000)) {
      if (b.query("Creature_isTargetAttackingMe") && m.own_draw(c)%3 && b.hand(2)) b.state(23);
      else b.state(6);
    } else if (!b.hand(1) && b.see(0,0x42100000)) b.state(6);
    else if (b.hand(1) && b.see(0,0x42100000)) b.state(7+(m.own_draw(c)&1));
    else if (b.hand(1) && b.see(0,0x42100000,0x4000)) b.state(9);
    else if (b.hand(1) && b.see(0,0x42100000,-0x4000)) b.state(10);
    else if (b.hand(1) && b.see(0,0x42100000,-0x7fff)) b.state(11);
    else if (!b.hand(1) && deadline_passed() && b.see(0x43960000,0x42700000)) b.state(12);
    else if (b.see(0,0x42100000,0,32767)) b.turn();
    else if (b.query("Creature_isNotRooted")) b.state(3);
    break;
  case 6: case 7: case 8:
    b.settle(); b.turn();
    if (events&1) b.hit(state==6?0x440001:0x440008,state!=6);
    if (!(b.h(0x11a)&8)) b.state(5);
    break;
  case 9: case 10: case 11:
    b.move_zero();
    if (events&1) b.hit(8,true);
    if (!(b.h(0x11a)&8)) b.state(5);
    break;
  case 12:
    if (number(b.call("anim_findTrack",{iv(c+0x114),iv(b.clip(16))})) &&
        !number(b.call("anim_findTrack",{iv(c+0x114),iv(b.clip(12))})) && b.see(0,0x42100000)) {
      b.change(15,0x803); b.blend(12,0x818);
    }
    if (b.u(0x234)) { b.turn(); b.settle(); if (events&1) b.hit(0x100041,false); }
    if ((b.h(0x11a)&8) && ((b.h(0x11a)&16) || b.see(0,0x43960000))) break;
    b.w(0x468,4,b.u(0x300)+m.own_draw(c)%180+300); b.state(3); break;
  case 16:
    if (b.query("Creature_isAttackReady")) b.state(0);
    else b.call("Creature_fleeFromTarget",{iv(c),iv(0)});
    break;
  case 18: if (!(b.h(0x11a)&16)) b.state(3); break;
  case 19: if (b.query("Creature_isNotStunned")) b.state(3); break;
  case 20:
    b.move_zero();
    b.call("Creature_deadUpdate",{iv(c),iv(events),A::Vec3{},iv(0),iv(0x41a00000)}); break;
  case 23: if (s32(b.u(0xf4))<s32(b.u(0x300))) b.state(3); break;
  default: b.state(0); break;
  }
}
void draw(A::Model &m, Address c) {
  Body b{m,c};
  auto result=b.call("Creature_draw",{iv(c),m.vcall(c,16),iv(b.clip(1)),iv(0)});
  auto model=m.vcall(c,16);
  if (auto holder=b.u(0x50)) m.holder_call(holder,18,{iv(DEFINITION),model,result});
}
} // namespace champions::simulation::rta::minotaur
