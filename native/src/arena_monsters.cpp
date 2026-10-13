// Authored from ref_arena_monsters.py at 356add27d50e50972b2a671b31e8f4ec93e6778c
// SHA256 84911d47d9471fc5ac3c82b927b2f1d272922554342185af299e1e5744e16f16.
#include "champions/arena_monsters.hpp"
#include <utility>
#include "champions/core_math.hpp"
namespace champions::simulation::rta::arena_monsters {
namespace {
struct Depth {
  std::size_t &n;
  Depth(std::size_t &v, std::size_t max) : n(v) {
    if (n >= max) throw creature_hp::ResourceLimit("arena operation depth limit");
    ++n;
  }
  ~Depth() { --n; }
};
std::int64_t integer(const Value &v) {
  if (auto p = std::get_if<std::int64_t>(&v)) return *p;
  throw Error("arena callback requires integer result");
}
Value i(std::uint64_t v) { return std::int64_t(v & UINT32_MAX); }
void raw_creature(Address c) {
  if (c > UINT32_MAX) throw creature_hp::ResourceLimit("arena creature outside raw32 domain");
}
}
Model::Model(Memory &m, creature_hp::Limits limits) : limits_(limits), mem(m) {}
Raw Model::u32(Address a) const { return Raw(mem.load(a,4)); }
Vec3 Model::vec(Address a) const { return {u32(a),u32(a+4),u32(a+8)}; }
Raw Model::clip(Address c, std::uint32_t off) const { return u32(Address(u32(c+0x24))+off); }
void Model::write(Address a, std::size_t size, std::uint64_t value) {
  if (a > UINT32_MAX || size > 8 || size > UINT64_C(0x100000000)-a)
    throw creature_hp::ResourceLimit("arena write outside raw32 domain");
  if (writes.size() >= limits_.history_entries || events.size() >= limits_.history_entries)
    throw creature_hp::ResourceLimit("arena history limit");
  if (size < 8) value &= (UINT64_C(1) << (size*8))-1;
  Write w{std::uint32_t(a),size,value}; writes.push_back(w); events.emplace_back(w);
  mem.store(a,size,value);
}
Value Model::call(const std::string &name, std::vector<Value> args) {
  if (callbacks.size() > limits_.callback_entries || calls.size() >= limits_.history_entries ||
      events.size() >= limits_.history_entries)
    throw creature_hp::ResourceLimit("arena callback/history limit");
  for (auto &v : args) if (auto p = std::get_if<std::int64_t>(&v)) *p = std::int64_t(std::uint64_t(*p)&UINT32_MAX);
  Call c{name,std::move(args)}; calls.push_back(c); events.emplace_back(c);
  auto it = callbacks.find(name);
  if (it == callbacks.end()) throw MissingHandler("missing arena handler: " + name);
  auto fn = it->second;
  return fn(*this,c.args);
}
Raw Model::model_slot16(Address c) {
  auto table = u32(c+0x48);
  auto adjusted = (c + std::uint64_t(std::int64_t(s16(mem.load(Address(table)+128,2))))) & UINT32_MAX;
  if (!external_slot16 && u32(Address(table)+132) == 0x3c2738)
    return u32(Address(u32(adjusted+0x24))+0xc);
  // Every other known arena/base body is outside this subset. Do not substitute
  // an external handler for a body the reference would execute internally.
  const auto function = u32(Address(table)+132);
  if (!external_slot16) for (auto known : {0x27236cu,0x272f4cu,0x2a8b84u,0x2a91b8u,
      0x2c2440u,0x2c382cu,0x2c34e4u,0x2c3c54u,0x10e658u,0x10beb8u,0x3c2708u})
    if (function == known) throw Unsupported("slot16 redirected to another modeled arena body");
  return Raw(std::uint64_t(integer(call("Creature.v16",{i(adjusted)}))) & UINT32_MAX);
}
FilterResult Model::filter_state(Address c, std::uint32_t requested) {
  Depth guard(depth_,limits_.depth);
  if (c > UINT32_MAX) throw creature_hp::ResourceLimit("arena creature outside raw32 domain");
  auto state = s32(requested);
  if (u32(0x4f7120) && !u32(0x4f711c) && u32(0x503890) && !u32(0x503894)) return {true,state};
  if (mem.load(c+0x231,1) && state == 1) return {true,state};
  if (state == 0x14) for (auto off : {0x31c,0x318,0x314,0x31a}) write(c+off,2,0);
  if (s16(mem.load(c+0x314,2)) && state != 0 && state != 5 && !(6 <= state && state <= 15))
    state = u32(c+0x234) ? 5 : 0;
  else if (s16(mem.load(c+0x31c,2)) && state != 0x13) state = 0x13;
  else {
    if (!u32(c+0x234)) write(c+0x318,2,0);
    if (s16(mem.load(c+0x318,2)) && state != 0x10) state = 0x10;
  }
  if (!state) { call("handle_clear",{i(c+0x234)}); write(c+0x234,4,0); }
  return {false,state};
}
void Model::skelight_shatter(Address, std::uint32_t) {
  Depth guard(depth_,limits_.depth); write(G_SKELIGHT_STATIC,8,0);
}
void Model::skelight_set_state(Address c, std::uint32_t requested) {
  Depth guard(depth_,limits_.depth);
  auto filtered = filter_state(c,requested);
  if (filtered.refused) return;
  auto state = filtered.state;
  const auto anim = c+0x114;
  auto idle = [&] { call("anim_start",{i(anim),i(clip(c,0x34)),i(3),i(0x3dcccccc)}); };
  switch (state) {
  case 0: idle(); write(c+0xf4,4,std::uint64_t(u32(c+0x300))+60); break;
  case 1: throw Unsupported("Skelight wander state 1 not ported");
  case 3: case 0x10:
    call("anim_start",{i(anim),i(clip(c,0x44)),i(0x803),i(0x3dcccccc)});
    call("anim_syncToSimFrame",{i(anim)}); break;
  case 5: case 0x13: idle(); break;
  case 6: {
    auto seed = std::uint32_t(std::uint64_t(u32(c+0x44))*0x19660d+0x3c6ef35f);
    write(c+0x44,4,seed);
    call("anim_startBlendInOut",{i(anim),i(clip(c,0x1c+8*((seed>>16)&1))),i(0x808),i(0x3dcccccc),i(0x3dcccccc)});
    write(c+0xf4,4,1); break;
  }
  case 0x12: state = s16(mem.load(c+0x232,2)); break;
  case 0x14: {
    write(c+0xc,4,u32(c+0xc)|2);
    write(c+0xa8,8,mem.load(c+0xa8,8)|UINT64_C(0x20000000000));
    skelight_shatter(c,8);
    auto count = s32(std::uint64_t(integer(call("model_getSamplePointCount",{i(model_slot16(c))}))));
    for (std::int64_t index = 0; index < count; index += 3) {
      auto result = call("model_getSamplePoint",{i(model_slot16(c)),i(anim),vec(c+0x10),
          i(std::uint64_t(std::int64_t(s16(mem.load(c+0x38,2))))),i(index),i(1),i(0),i(ONE)});
      auto sample = std::get_if<Sample>(&result);
      if (!sample) throw Error("arena sample callback requires two vectors");
      call("fx_spawn",{i(2),sample->point});
    }
    call("obj_delete",{i(c)}); break;
  }
  case 0x17: break;
  default: state = 0; idle(); break;
  }
  write(c+0x232,2,std::uint64_t(std::int64_t(state)));
}

Value Model::virtual_call(Address c, unsigned slot, std::vector<Value> args) {
  auto table = u32(c+0x48);
  auto adjusted = (c + std::uint64_t(std::int64_t(s16(mem.load(Address(table)+8*slot,2))))) & UINT32_MAX;
  auto function = u32(Address(table)+8*slot+4);
  if (!(slot == 26 && external_slot26)) {
    for (auto known : {0x27236cu,0x272f4cu,0x2a8b84u,0x2a91b8u,0x2c2440u,
                      0x2c382cu,0x2c34e4u,0x2c3c54u,0x3c2738u,0x10e658u,0x10beb8u,0x3c2708u})
      if (function == known) throw Unsupported("slot26 redirect to arena body with incompatible signature");
    // ref_creature resolves base bodies only for their matching slot.
    if (slot == 26 && function == 0x10bb08)
      throw Unsupported("base slot26 turning body requires external_slot26");
  }
  args.insert(args.begin(), i(adjusted));
  return call("Creature.v"+std::to_string(slot),std::move(args));
}
std::int64_t Model::is_not_rooted(Address c) {
  Depth guard(depth_,limits_.depth); raw_creature(c); return mem.load(c+0x314,2)==0;
}
std::int64_t Model::is_not_stunned(Address c) {
  Depth guard(depth_,limits_.depth); raw_creature(c); return mem.load(c+0x31c,2)==0;
}
std::int64_t Model::is_attack_ready(Address c) {
  Depth guard(depth_,limits_.depth); raw_creature(c);
  if (!u32(c+0x234)) write(c+0x318,2,0);
  return mem.load(c+0x318,2)==0;
}
Raw Model::slot16_model(Address c) {
  Depth guard(depth_,limits_.depth); raw_creature(c); return u32(Address(u32(c+0x24))+12);
}
void Model::slot34_set_home_range(Address c, Raw value) {
  Depth guard(depth_,limits_.depth); raw_creature(c); write(c+0x3c0,4,from_int(value&0xffff));
}
std::int64_t Model::turn_to_target(Address c) {
  Depth guard(depth_,limits_.depth); raw_creature(c); auto node=u32(c+0x234);
  if (node) virtual_call(c,26,{i(0),vec(Address(u32(Address(node)+12))+0x10)});
  return 0;
}
std::int64_t Model::can_see_target_in(Address c, Raw facing, Raw arc, Raw mode, Raw near, Raw far, Raw lead) {
  Depth guard(depth_,limits_.depth); raw_creature(c); auto node=u32(c+0x234);
  if (!node) return 0;
  return integer(call("creature_canSeeObj",{i(c),i(u32(Address(node)+12)),
      i(std::uint64_t(std::int64_t(s16(facing)))),i(std::uint64_t(std::int64_t(s16(arc)))),i(mode),i(near),i(far),i(lead)}));
}
std::int64_t Model::count_allies_near_target(Address c, Raw radius) {
  Depth guard(depth_,limits_.depth); raw_creature(c); auto node=u32(c+0x234);
  if (!node) return 0;
  return integer(call("creature_countAlliesNear",{i(c),vec(Address(u32(Address(node)+12))+0x10),i(radius)}));
}
std::int64_t Model::is_blocked_by_target(Address c) {
  Depth guard(depth_,limits_.depth); raw_creature(c); auto node=u32(c+0x234);
  if (!node) return 0;
  return integer(call("creature_isBlockedBy",{i(c),i(u32(Address(node)+12))}));
}
std::int64_t Model::is_target_charging(Address c) {
  Depth guard(depth_,limits_.depth); raw_creature(c); auto node=u32(c+0x234);
  if (!node) return 0;
  auto t=Address(u32(Address(node)+12));
  auto bearing=atan2_16(sub(u32(t+0x14),u32(c+0x14)),sub(u32(t+0x10),u32(c+0x10)));
  t=u32(Address(u32(c+0x234))+12);
  if (Raw(std::int64_t(bearing)-s16(mem.load(t+0x38,2))+0x3fff)>=0x7fff) return 0;
  auto vy=u32(t+0x30c),vx=u32(t+0x308);
  return lt(0x42c80000,sqrt(add(mul(vx,vx),mul(vy,vy))));
}
std::int64_t Model::slot25_can_see_target(Address c, Raw force) {
  Depth guard(depth_,limits_.depth); raw_creature(c);
  Raw mode=force || mem.load(c+0x3b0,2)==0x7fff ? 1 : 0;
  auto near=from_int(Raw(mem.load(c+0x3ac,2))),node=u32(c+0x234);
  if (node && integer(call("creature_canSeeObj",{i(c),i(u32(Address(node)+12)),i(0),i(0x7fff),i(mode),i(ZERO),i(near),i(ZERO)}))) return 1;
  auto far=from_int(Raw(mem.load(c+0x3ae,2))); node=u32(c+0x234);
  if (!node) return 0;
  return integer(call("creature_canSeeObj",{i(c),i(u32(Address(node)+12)),
      i(std::uint64_t(std::int64_t(s16(mem.load(c+0x38,2))))),
      i(std::uint64_t(std::int64_t(s16(mem.load(c+0x3b0,2))))),i(mode),i(ZERO),i(far),i(ZERO)})) != 0;
}
void Model::slot28_on_death(Address c, Raw quiet) {
  Depth guard(depth_,limits_.depth); raw_creature(c);
  write(c+0xa8,8,mem.load(c+0xa8,8)&UINT64_C(0xfffffffff73fffff));
  auto script=integer(call("obj_getPropStr",{i(c),i(0x52fbf8),i(0)}));
  if (script) {
    write(0x4039f8,4,c);call("script_runByName",{i(std::uint64_t(script))});write(0x4039f8,4,0);
  }
  if (!quiet) {
    auto item=integer(call("obj_getPropStr",{i(c),i(0x52fc08),i(0)}));
    if (item) call("loot_spawnNamedItem",{vec(c+0x10),i(std::uint64_t(item))});
  }
  auto flags=u32(c+0xc);
  if (flags&2) {
    write(c+0xc,4,flags|0x4000);call("anim_jumpTopToEnd",{i(c+0x114)});call("obj_delete",{i(c)});
  }
  if (u32(0x4f7120)) {
    auto node=u32(c+0x2e4);call("arena_addKill",{i(node?u32(Address(node)+12):0),i(0)});
  }
  call("creature_stopBossMusic",{i(c)});
}
void Model::set_def_entry_params(Address c, Raw entry, const std::array<Raw,4> &masks, const std::array<Raw,4> &times) {
  Depth guard(depth_,limits_.depth); raw_creature(c);
  for (unsigned k=0;k<4;++k) {
    auto record=Address(u32(Address(u32(c+0x24))+8*Address(entry)+4));
    write(record+0x18+8*k,1,masks[k]);write(record+0x14+8*k,4,times[k]);
  }
}
void Model::creature_ctor(Address c, Raw type, const Vec3 &pos, Raw facing, Raw def, Raw props, Raw hp) {
  Depth guard(depth_,limits_.depth); raw_creature(c);
  auto prop=[&](Raw name){return integer(call("obj_getPropInt",{i(c),i(name),i(0)}));};
  auto str=[&](Raw name){return integer(call("obj_getPropStr",{i(c),i(name),i(0)}));};
  call("GameObject_ctor",{i(c),pos,i(def),i(props)});
  write(c+0x50,4,0);write(c+0x48,4,0x50cc58);write(c+0x4c,4,0x50d010);write(c+0x119,1,8);
  for (auto off:{0x2f4,0x2ec,0x2f0,0x234,0x238,0x2e4,0x360,0x364}) write(c+off,4,0);
  write(c+0x3b4,8,std::uint64_t(pos[0])|(std::uint64_t(pos[1])<<32));
  write(c+0x3bc,4,pos[2]);
  write(c+0x38,2,facing);write(c+0x22c,4,hp);write(c+0x220,8,UINT64_MAX);
  write(c+0xc,4,u32(c+0xc)|0x20);write(c+0xc8,4,0);write(c+0x228,4,hp);
  write(c+0x23c,4,UINT32_MAX);write(c+0x240,4,UINT32_MAX);
  for (auto off:{0x2bc,0x2c0,0x2dc,0xf4,0x2c4,0x300}) write(c+off,4,0);
  for (auto off:{0x110,0x31c,0x312,0x316}) write(c+off,2,0);
  write(c+0x231,1,0);write(c+0x2fc,1,0);write(c+0x310,1,255);write(c+0x5a,1,255);write(c+0xa0,1,0);
  write(c+0x3c,4,u32(c+0x18));write(c+0xdc,4,ONE);write(c+0x58,2,0xffff);write(c+0x3c4,2,600);
  for (auto off:{0xb8,0x304,0xe0,0x2d8}) write(c+off,4,0);
  for (auto off:{0x314,0x31a,0x318}) write(c+off,2,0);
  auto serial=mem.load(0x4f85c4,1);write(0x4f85c4,1,serial+1);write(c+0x230,1,serial);
  if (prop(0x52f9f0)) write(c+0x231,1,mem.load(c+0x231,1)|1);
  if (prop(0x52f9f8)) {
    write(c+0x231,1,mem.load(c+0x231,1)|8);call("obj_moveOffGrid",{i(c)});write(c+0xc,4,u32(c+0xc)|0x8000);
  }
  if (str(0x52fa00)) {write(c+0x368,4,std::uint64_t(str(0x52fa00)));write(c+0xc,4,u32(c+0xc)|0x100000);}
  if (prop(0x52fa10)) write(c+0xc8,4,0x447a0000);
  write(c+0x3c0,4,0x447a0000);
  if (prop(0x52fa18)) write(c+0x3c0,4,from_int(Raw(prop(0x52fa18))));
  for (auto row : {std::array<Raw,3>{0x3ac,0x52fa28,300},{0x3ae,0x52fa38,500},{0x3b0,0x52fa48,0x4000}}) {
    write(c+row[0],2,row[2]);if (prop(row[1])) write(c+row[0],2,std::uint64_t(prop(row[1])));
  }
  if (prop(0x52fa58)) write(c+0xdc,4,mul(from_int(Raw(prop(0x52fa58))),0x3c23d70a));
  if (prop(0x52fa60)) write(c+0xa8,8,mem.load(c+0xa8,8)|1);
  if (prop(0x52fa70)) write(c+0xa8,8,mem.load(c+0xa8,8)|UINT64_C(0x1000000000000));
  if (str(0x52fa80)||str(0x52fa88)||str(0x52fa90)) write(c+0xa8,8,mem.load(c+0xa8,8)|0x40000000);
  if (type) {
    write(c+0x34,4,type); auto result=call("balance_lookup",{i(type)});
    auto b=std::get_if<Balance>(&result);if (!b) throw Error("balance callback requires found and five factors");
    std::vector<Value> args{i(c),i(1)};for (auto f:b->factors) args.push_back(i(b->found?f:ONE));
    call("Creature_setStats",std::move(args));
  }
  write(c+0x2fa,2,mem.load(c+0x38,2));
}
} // namespace champions::simulation::rta::arena_monsters
