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
Raw Model::clip(Address c, std::uint32_t off) const { raw_creature(c); return u32(Address(u32(c+0x24))+off); }
Raw Model::class_model(Address c) const { raw_creature(c); return u32(Address(u32(c+0x24))+0xc); }
std::int32_t half_toward_zero(Raw value) { return s16(value)/2; }
std::int32_t third_toward_zero(Raw value) { return s16(value)/3; }
Value Model::prop_int(Address c, Raw name) {
  Depth guard(depth_,limits_.depth); raw_creature(c);
  return call("obj_getPropInt",{i(c),i(name),i(0)});
}
Value Model::weapon_descriptor(Address c) {
  Depth guard(depth_,limits_.depth); raw_creature(c);
  auto holder=u32(c+0x50);
  return holder ? holder_call(holder,17,{i(1)}) : Value(std::int64_t(0));
}
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
  if (!external_slot16 && !external.count("Creature.v16") && u32(Address(table)+132) == 0x3c2738)
    return u32(Address(u32(adjusted+0x24))+0xc);
  // Every other known arena/base body is outside this subset. Do not substitute
  // an external handler for a body the reference would execute internally.
  const auto function = u32(Address(table)+132);
  if (!external_slot16 && !external.count("Creature.v16")) for (auto known : {0x27236cu,0x272f4cu,0x2a8b84u,0x2a91b8u,
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
std::int32_t scaled_time(std::int64_t count) {
  return to_int(mul(mul(from_int(Raw(std::uint64_t(count))),0x42700000),0x3c888888));
}
ArenaScript::ArenaScript(const std::map<std::string,std::int64_t> &quest, std::size_t limit)
    : history_limit_(limit) {
  for (auto key : {"sawknights","sawcaz","killedAv","endsound","firstmissionever",
                   "knight1","knight2","knight3","knight4"}) q[key]=0;
  for (auto &entry:quest) q[entry.first]=entry.second;
  g={{0,0},{4,0}};
}
void ArenaScript::emit(const std::string &name, std::vector<ScriptValue> args) {
  if (log.size()>=history_limit_) throw creature_hp::ResourceLimit("arena script history limit");
  log.push_back({name,std::move(args)});
}
void ArenaScript::increment(unsigned key) {
  auto &count=g.at(key);
  if (count==INT64_MAX) throw creature_hp::ResourceLimit("arena script counter exceeds signed64 host domain");
  ++count;
}
void ArenaScript::defer(const std::string &name) {
  if (back.size()<64) back.push_back(name);
}
void ArenaScript::frame(std::int64_t value) {
  Depth guard(frame_depth_,creature_hp::MAX_DEPTH);
  paused=value!=0;
  // Python's iterator retains the original list if __init__ replaces front.
  const auto pending=front;
  for (const auto &name:pending) {
    if (name=="__init__") {
      ArenaScript reset({},history_limit_);
      q=std::move(reset.q);g=std::move(reset.g);
      front.clear();back.clear();log.clear();paused=false;
    }
    else if (name=="frame") frame();
    else if (name=="start") start();
    else if (name=="incoming") incoming();
    else if (name=="nosave") nosave();
    else if (name=="setFlag") setFlag();
    else if (name=="knightDefeatedPause") knightDefeatedPause();
    else if (name=="knightDefeatedDelay") knightDefeatedDelay();
    else if (name=="knightDefeated") knightDefeated();
    else if (name=="avatar_death") avatar_death();
    else if (name=="bossDefeatedDelay") bossDefeatedDelay();
    else if (name=="endmission") endmission();
    else if (name=="levelChange") levelChange();
    else throw Unsupported("unknown or argument-requiring deferred arena entry: "+name);
  }
  front=std::move(back);back.clear();
}
void ArenaScript::start() {
  for (auto name:{"Rallos","avatar","knight"}) emit("hideMonster",{std::string(name)});
  emit("removeQuest");
  if (q.at("killedAv")) emit("autoPropSetState",{std::string("safety"),std::int64_t(1)});
  emit("setMonsterVal",{std::string("avatar"),std::int64_t(4),std::int64_t(2000)});
  emit("setMonsterVal",{std::string("knight"),std::int64_t(4),std::int64_t(2000)});
}
void ArenaScript::incoming() {
  if (!q.at("sawknights")) {
    emit("cutScene",{std::string("knights")});emit("postAction",{std::string("nosave")});q["sawknights"]=1;
  } else setFlag();
}
void ArenaScript::nosave() { emit("setSaveInhibit",{std::int64_t(1)});setFlag(); }
void ArenaScript::setFlag() { emit("setCFlag",{std::string("avatar"),std::int64_t(0),std::int64_t(1)}); }
void ArenaScript::knight_death(std::int64_t index) { q["knight"+std::to_string(index)]=1;knightDefeatedPause(); }
void ArenaScript::knightDefeatedPause() {
  for (unsigned k=1;k<=4;++k) if (!q.at("knight"+std::to_string(k))) return;
  g[4]=0;defer("knightDefeatedDelay");
}
void ArenaScript::knightDefeatedDelay() {
  if (!paused) increment(4);
  if (scaled_time(g.at(4))>300) knightDefeated();else defer("knightDefeatedDelay");
}
void ArenaScript::knightDefeated() {
  if (!q.at("sawcaz")) { emit("cutScene",{std::string("cazic")});q["sawcaz"]=1; }
}
void ArenaScript::avatar_death() { g[0]=0;defer("bossDefeatedDelay"); }
void ArenaScript::bossDefeatedDelay() {
  if (!paused) increment(0);
  if (scaled_time(g.at(0))>250 && !q.at("endsound")) {
    emit("setInfo");emit("soundSequence");q["endsound"]=1;q["firstmissionever"]=1;defer("bossDefeatedDelay");
  }
  if (scaled_time(g.at(0))>1500) endmission();else defer("bossDefeatedDelay");
}
void ArenaScript::endmission() {
  if (!q.at("killedAv")) {
    emit("postAction",{std::string("levelChange")});emit("cutScene",{std::string("bossdefeated")});
    emit("reapEnemies");emit("removeQuest");q["killedAv"]=1;
  }
}
void ArenaScript::levelChange() { emit("grantMission",{std::int64_t(1)});emit("newWorld",{std::string("hub2")}); }
// Tagar's complete skill/spell family, specification section 7.
namespace {
constexpr Raw DT = 0x3c888888, TAGAR_SPEED = 0x434b0da1, TAGAR_TIME = 0x3ba1605b;
constexpr Raw TAGAR_TABLE = 0x51ba60, SPELL_TABLE = 0x515ba0, MARKER_TABLE = 0x51ba90;
constexpr Raw FRAME = 0x44fdcc;
constexpr Raw CLOUD[] = {0x8e7598,0x8e75e8,0x8e7638,0x8e7688};
std::int32_t signed_byte(Raw n) { return (n & 255) < 128 ? std::int32_t(n & 255) : std::int32_t(n & 255)-256; }
// Signed slot indices intentionally address outside the nominal eight slots,
// just as the reference does. Borrowed Memory supplies the checked host bound.
Address indexed(Address base, std::int32_t index) {
  auto result = std::int64_t(base)+4*std::int64_t(index);
  if (result < 0) throw creature_hp::ResourceLimit("negative arena slot address");
  return Address(result);
}
}
Raw Model::shared_draw() {
  auto seed = Raw(std::uint64_t(u32(0x404464))*0x19660d+0x3c6ef35f);
  write(0x404464,4,seed); ++draws; return seed>>16;
}
Raw Model::spell_rand(Address s) {
  Depth guard(depth_,limits_.depth); raw_creature(s);
  auto seed = Raw(std::uint64_t(u32(s+0x10))*0x19660d+0x3c6ef35f);
  write(s+0x10,4,seed); return seed>>16;
}
Raw Model::unit(Raw draw) {
  auto m = mul(from_int(draw),TWO_M16); return sub(add(m,m),ONE);
}
Vec3 Model::spell_ball(Address s) {
  Depth guard(depth_,limits_.depth); raw_creature(s);
  // Every rejected attempt performs visible writes and is bounded by history.
  for (;;) {
    auto x=unit(spell_rand(s)), y=unit(spell_rand(s)), z=unit(spell_rand(s));
    if (!lt(ONE,add(add(mul(x,x),mul(y,y)),mul(z,z)))) return {x,y,z};
  }
}
Value Model::tagar_virtual(const std::string &cls, Address obj, unsigned slot, std::vector<Value> args) {
  raw_creature(obj);
  Raw offset = cls=="Creature"?0x48:cls=="Skill"?0x60:0x14;
  auto table=u32(obj+offset), function=u32(Address(table)+8*slot+4);
  auto adjusted=Raw(obj+std::uint64_t(std::int64_t(s16(mem.load(Address(table)+8*slot,2)))));
  auto name=cls+".v"+std::to_string(slot);
  if (!external.count(name)) {
    if (cls=="Creature") {
      if (slot==34 && function==0x3c2708) {
        slot34_set_home_range(adjusted,Raw(integer(args.at(0))));return std::int64_t(0);
      }
      if (slot==37 && function==0x3c2730) return std::int64_t(signed_byte(Raw(mem.load(Address(adjusted)+0x310,1))));
      // Tagar requires only slot20. Matching inherited bodies run internally in
      // Python; never silently replace one with an engine callback.
      if (slot==20 && function==0x119328)
        throw Unsupported("base enemy predicate requires explicit Creature.v20");
      for (auto known:{0x27236cu,0x272f4cu,0x2a8b84u,0x2a91b8u,0x2c2440u,
                      0x2c382cu,0x2c34e4u,0x2c3c54u,0x3c2738u,0x10e658u,0x10beb8u,0x3c2708u})
        if (function==known) throw Unsupported("Tagar creature virtual redirects to incompatible arena signature");
    } else if (cls=="Skill") {
      if (function==0x2e2368 || function==0x2e18a4 || function==0x30c708)
        throw Unsupported("Tagar statistic virtual redirects to incompatible skill signature");
    } else if (function==0x2e177c || function==0x30cbc8) {
      // The requested slot1 takes one argument, while the run signature takes
      // two optional arguments. Preserve the modeled dispatch for Tagar run.
      if (function==0x30cbc8) { tagar_spell_run(adjusted); return std::int64_t(0); }
      summon_spell_run(adjusted); return std::int64_t(0);
    }
  }
  args.insert(args.begin(),i(adjusted)); return call(name,std::move(args));
}
void Model::spell_destroy(Address s) { tagar_virtual("Spell",s,1,{i(3)}); }
void Model::spell_base_ctor(Address s, Raw level, Raw owner, Raw skill, Raw table) {
  Depth guard(depth_,limits_.depth); raw_creature(s);
  write(s,1,level); write(s+0x14,4,SPELL_TABLE); write(s+0xc,4,owner);
  write(s+4,4,0); write(s+8,4,skill); write(s+0x10,4,shared_draw()); write(s+0x14,4,table);
}
Raw Model::tagar_cast(Address skill, Raw level, Raw caster, const Vec3 &pos, Raw) {
  Depth guard(depth_,limits_.depth); raw_creature(skill);
  level=Raw(integer(tagar_virtual("Skill",skill,14,{i(level),i(0x14)})));
  auto resource=call("res_find",{i(0x552428),i(0x552430)});
  call("sound_playAtPos",{i(std::uint64_t(integer(resource))),pos,i(ONE)});
  auto spell=Raw(integer(call("pool_new",{i(0x94)})));
  if (spell) tagar_spell_ctor(spell,level,caster,Raw(skill));
  return spell;
}
void Model::tagar_spell_ctor(Address s, Raw level, Raw owner, Raw skill) {
  Depth guard(depth_,limits_.depth); raw_creature(s);
  spell_base_ctor(s,level,owner,skill,TAGAR_TABLE);
  for (unsigned k=0;k<8;++k) write(s+0x18+4*k,4,0);
  write(s+0x80,2,0xffff); write(s+0x82,2,0); write(s+0x7c,4,0x4000101f); write(s+0x78,4,0);
  auto position=vec(Address(u32(s+0xc))+0x10);
  // Preserve the reference's twelve-byte vector copy as 8+4 stores.
  write(s+0x88,8,std::uint64_t(position[0])|(std::uint64_t(position[1])<<32));
  write(s+0x90,4,position[2]);
  write(s+0x86,2,0); write(s+0x84,2,mem.load(Address(u32(s+0xc))+0x38,2));
  auto heading=s16(mem.load(s+0x84,2));
  write(s+0x88,4,add(u32(s+0x88),mul(cos16(heading),0x41c00000)));
  auto y=add(u32(s+0x8c),mul(sin16(heading),0x41c00000));
  write(s+0x90,4,add(u32(s+0x90),0x42400000)); write(s+0x8c,4,y);
}
std::int64_t Model::tagar_is_valid(Address s, Raw obj, Raw marker_test) {
  Depth guard(depth_,limits_.depth); raw_creature(s);
  if (!obj || (u32(Address(obj)+0xc)&9)) return 0;
  if (!integer(tagar_virtual("Creature",u32(s+0xc),20,{i(obj)}))) return 0;
  if (marker_test && integer(call("creature_findSpell",{i(obj),i(0x57),i(u32(s+0xc))}))) return 0;
  return 1;
}
Raw Model::tagar_pick_target(Address s, Raw later) {
  Depth guard(depth_,limits_.depth); raw_creature(s);
  auto swarm=vec(s+0x88);auto heading=s16(mem.load(s+0x84,2));
  auto found=later ? call("world_collectInSphere",{swarm,i(heading),i(0x42f00000)}) :
    call("world_collectObjectsInArc",{swarm,i(heading),i(u32(s+0xc)),i(0x42f00000),i(0x42b40000),i(ZERO)});
  auto objects=std::get_if<Objects>(&found);
  if (!objects) throw Error("arena collection requires object list");
  if (objects->values.size()>limits_.history_entries) throw creature_hp::ResourceLimit("arena collection limit");
  Raw best=0,best_d=0x49742400;
  for (auto obj:objects->values) {
    if (!tagar_is_valid(s,obj,1)) continue;
    auto count=s16(mem.load(s+0x82,2));bool duplicate=false;
    for (int k=0;k<count;++k) {
      auto node=u32(s+0x18+4*k);
      if (node && u32(Address(node)+0xc)==obj) { duplicate=true;break; }
    }
    if (duplicate) continue;
    if (!later) { call("handle_clear",{std::string("stack")}); return obj; }
    auto dx=sub(u32(s+0x88),u32(Address(obj)+0x10));
    auto dy=sub(u32(s+0x8c),u32(Address(obj)+0x14));
    auto dz=sub(u32(s+0x90),u32(Address(obj)+0x18));
    auto d=add(add(mul(dx,dx),mul(dy,dy)),mul(dz,dz));
    if (lt(d,best_d)) { best=obj;best_d=d; }
  }
  call("handle_clear",{std::string("stack")}); return best;
}
std::int64_t Model::tagar_tick_victims(Address s) {
  Depth guard(depth_,limits_.depth); raw_creature(s);
  unsigned active=0;
  for (int k=0;k<s16(mem.load(s+0x82,2));++k) {
    auto node=u32(s+0x18+4*k), victim=node?u32(Address(node)+0xc):0;
    auto slot=s+0x38+4*k;
    if (!victim || lt(u32(slot),ZERO)) continue;
    if (le(u32(slot),u32(s+0x78)) || !tagar_is_valid(s,victim,0)) {
      auto marker=Raw(integer(call("creature_findSpell",{i(victim),i(0x57),i(u32(s+0xc))})));
      if (marker) spell_destroy(marker);
      write(slot,4,MINUS_ONE);continue;
    }
    write(Address(victim)+0x98,2,100);write(Address(victim)+0x9e,2,4);
    write(Address(victim)+0x9a,2,0);write(Address(victim)+0x9c,2,0);++active;
    auto tick=s+0x58+4*k;
    if (le(u32(tick),u32(s+0x78))) {
      auto owner=u32(s+0xc),skill=u32(s+8);auto level=signed_byte(Raw(mem.load(s,1)));
      Raw low,high;
      if (owner && (u32(Address(owner)+0xc)&0x40)) {
        auto range=tagar_virtual("Skill",skill,8,{i(1),i(level),i(0),i(0x3e4ccccc)});
        auto pair=std::get_if<DamageRange>(&range);
        if (!pair) throw Error("Skill.v8 requires low/high damage range");
        low=pair->low;high=pair->high;
      } else {
        auto value=from_int(Raw(integer(tagar_virtual("Skill",skill,12,{i(1),i(level),i(owner),i(0)}))));
        auto spread=mul(value,0x3e4ccccc);high=Raw(to_int(add(value,spread)));low=Raw(to_int(sub(value,spread)));
      }
      call("damage_apply",{i(victim),i(u32(s+0xc)),i(100000),i(0),i(0),i(0x4000000),i(0),i(0),i(from_int(low)),i(from_int(high))});
      write(tick,4,add(u32(tick),ONE));
    }
    for (unsigned j=0;j<5;++j) {
      auto frame=u32(FRAME),sign=j&1?MINUS_ONE:ONE;
      auto ball=spell_ball(s);Vec3 velocity{mul(ball[0],0x40800000),mul(ball[1],0x40800000),mul(ball[2],0x40800000)};
      auto swirl=Raw((frame+j*0x2aaa)<<9);
      auto px=mul(mul(sign,u32(Address(victim)+0x1c)),cos16(swirl));
      auto py=mul(mul(sign,u32(Address(victim)+0x1c)),sin16(swirl));
      auto bob=sin16(Raw((frame-j*0x2aaa)<<10));
      auto pz=mul(mul(sign,bob),mul(u32(Address(victim)+0x20),0x3e800000));
      px=add(px,u32(Address(victim)+0x10));py=add(py,u32(Address(victim)+0x14));
      pz=add(add(pz,u32(Address(victim)+0x18)),mul(u32(Address(victim)+0x20),HALF));
      call("particle_spawn",{i(CLOUD[0]),Vec3{px,py,pz},velocity,i(ZERO)});
    }
  }
  return active>0;
}
void Model::tagar_spell_run(Address s, const Vec3 &, Raw) {
  Depth guard(depth_,limits_.depth); raw_creature(s);
  auto active=tagar_tick_victims(s);auto clock=add(u32(s+0x78),DT);write(s+0x78,4,clock);
  if (le(u32(s+0x7c),clock)) { if (!active && s) spell_destroy(s);return; }
  auto limit=std::min(8,s32(Raw(integer(tagar_virtual("Skill",u32(s+8),12,{i(0),i(signed_byte(Raw(mem.load(s,1)))),i(u32(s+0xc)),i(10)})))));
  if (s16(mem.load(s+0x82,2))>=limit) return;
  Raw target=0;auto current=s16(mem.load(s+0x80,2));
  if (current>=0) {
    auto node=u32(indexed(s+0x18,current));if (node) target=u32(Address(node)+0xc);
    if (!node || !tagar_is_valid(s,target,0)) write(s+0x80,2,0xffff);
  }
  if (s16(mem.load(s+0x80,2))<0) {
    target=tagar_pick_target(s,s16(mem.load(s+0x82,2))>0);
    if (target) {
      auto count=Raw(mem.load(s+0x82,2));write(s+0x80,2,count);
      call("handle_link",{i(indexed(s+0x18,s16(count))),i(target),i(target)});
      auto dy=sub(u32(Address(target)+0x14),u32(s+0x8c)),dx=sub(u32(Address(target)+0x10),u32(s+0x88));
      auto dist=sqrt(add(mul(dx,dx),mul(dy,dy)));
      write(indexed(s+0x38,s16(mem.load(s+0x80,2))),4,add(u32(s+0x78),mul(dist,TAGAR_TIME)));
    }
  }
  Raw speed=TAGAR_SPEED;
  if (target) {
    auto dx=sub(u32(Address(target)+0x10),u32(s+0x88)),dy=sub(u32(Address(target)+0x14),u32(s+0x8c));
    if (lt(ONE,add(mul(dx,dx),mul(dy,dy)))) {
      auto error=atan2_16(dy,dx)-s16(mem.load(s+0x84,2));
      if (error>=0x4000) error=0x7fff-error;else if (error< -0x3fff) error=-0x8000-error;
      write(s+0x84,2,mem.load(s+0x84,2)+error/5);
    }
    dx=sub(u32(Address(target)+0x10),u32(s+0x88));dy=sub(u32(Address(target)+0x14),u32(s+0x8c));
    auto slot=indexed(s+0x38,s16(mem.load(s+0x80,2)));auto dist=sqrt(add(mul(dx,dx),mul(dy,dy)));
    auto eta=add(u32(s+0x78),mul(dist,TAGAR_TIME));if (lt(eta,u32(slot))) write(slot,4,eta);
    clock=u32(s+0x78);auto left=sub(u32(slot),clock);speed=div(dist,left);
    if (lt(DT,left)) {
      auto sz=u32(s+0x90),gap=sub(add(u32(Address(target)+0x18),0x42400000),sz);
      write(s+0x90,4,add(sz,div(gap,mul(left,0x42700000))));
    }
    constexpr Raw thirtieth=0x3d088888;
    if (le(u32(slot),add(clock,thirtieth))) {
      write(s+0x82,2,mem.load(s+0x82,2)+1);
      auto seconds=Raw(integer(tagar_virtual("Skill",u32(s+8),12,{i(2),i(signed_byte(Raw(mem.load(s,1)))),i(u32(s+0xc)),i(10)})));
      current=s16(mem.load(s+0x80,2));
      write(indexed(s+0x38,current),4,add(from_int(seconds),thirtieth));write(indexed(s+0x58,current),4,u32(s+0x78));
      auto marker=Raw(integer(call("pool_new",{i(0x20)})));
      if (marker) {
        auto owner=u32(s+0xc);write(marker,1,1);write(Address(marker)+0x14,4,SPELL_TABLE);write(Address(marker)+4,4,0);
        write(Address(marker)+0xc,4,target);write(Address(marker)+8,4,0);write(Address(marker)+0x10,4,shared_draw());
        write(Address(marker)+0x14,4,MARKER_TABLE);write(Address(marker)+0x18,4,owner);write(Address(marker)+0x1c,4,0x57);
      }
      call("creature_addSpell",{i(target),i(marker)});speed=TAGAR_SPEED;
      auto resource=Raw(integer(call("res_find",{i(0x552428),i(0x552460)})));
      call("sound_playAtPos",{i(resource),vec(Address(target)+0x10),i(ONE)});
      write(s+0x80,2,0xffff);write(s+0x7c,4,add(u32(s+0x7c),0x3f0041aa));
    }
  }
  if (spell_rand(s)<0x4906) {
    auto turn=sub(mul(from_int(spell_rand(s)),TWO_M16),HALF);write(s+0x86,2,Raw(to_int(mul(turn,0x45f81000))));
  }
  auto angle=s16(Raw(mem.load(s+0x84,2)+mem.load(s+0x86,2)));
  write(s+0x88,4,add(u32(s+0x88),mul(mul(cos16(angle),speed),DT)));
  auto sine=sin16(angle),grow=mul(u32(s+0x78),0x40226fa7);
  write(s+0x8c,4,add(u32(s+0x8c),mul(mul(sine,speed),DT)));if (lt(ONE,grow)) grow=ONE;
  for (unsigned k=0;k<50;++k) {
    auto ball=spell_ball(s);auto factor=mul(mul(grow,0x436d90b9),DT);
    Vec3 velocity{mul(ball[0],factor),mul(ball[1],factor),mul(ball[2],factor)};
    auto b=spell_ball(s);auto inverse=rsqrt(ONE,add(add(mul(b[0],b[0]),mul(b[1],b[1])),mul(b[2],b[2])));
    for (auto &v:b) v=mul(v,inverse);
    auto reach=mul(mul(grow,0x4151d268),mul(from_int(spell_rand(s)),TWO_M16));
    Vec3 point{add(mul(b[0],reach),u32(s+0x88)),add(mul(b[1],reach),u32(s+0x8c)),add(mul(b[2],reach),u32(s+0x90))};
    auto kind=spell_rand(s);call("particle_spawn",{i(CLOUD[kind>=0x7d01?0:kind>=0x3a99?1:2]),point,velocity,i(ZERO)});
  }
  constexpr Raw ring=0x4151d268,lean=0x3f0f56fb;
  auto side=neg(sin16(s16(mem.load(s+0x84,2)))),spin=Raw(u32(FRAME)<<10);
  auto px=mul(mul(side,lean),mul(cos16(spin),ring));
  auto ahead=mul(cos16(s16(mem.load(s+0x84,2))),lean);
  auto py=mul(ahead,mul(cos16(Raw(u32(FRAME)<<10)),ring));
  auto pz=mul(mul(sin16(Raw(u32(FRAME)<<10)),ring),lean);
  call("particle_spawn",{i(CLOUD[3]),Vec3{add(px,u32(s+0x88)),add(py,u32(s+0x8c)),add(pz,u32(s+0x90))},Vec3{0,0,0},i(ZERO)});
}
// Summon Skeleton, specification sections 4.2 and 6. All engine effects are call-outs.
Raw Model::cosmetic_draw() {
  Depth guard(depth_,limits_.depth);
  auto seed=Raw(std::uint64_t(u32(0x404460))*0x19660d+0x3c6ef35f);
  write(0x404460,4,seed); return seed>>16;
}
Vec3 Model::cosmetic_ball() {
  Depth guard(depth_,limits_.depth);
  for (;;) {
    auto x=unit(cosmetic_draw()),y=unit(cosmetic_draw()),z=unit(cosmetic_draw());
    if (!lt(ONE,add(add(mul(x,x),mul(y,y)),mul(z,z)))) return {x,y,z};
  }
}
std::string Model::cstr(Address a, std::size_t limit) const {
  raw_creature(a);
  if (limit>limits_.memory_bytes) throw creature_hp::ResourceLimit("arena string limit");
  std::string out;
  for (std::size_t k=0;k<limit;++k) { auto b=mem.load(a+k,1); if (!b) break; out+=char(b); }
  return out;
}
Value Model::holder_call(Address h,unsigned slot,std::vector<Value> args) {
  Depth guard(depth_,limits_.depth);
  raw_creature(h); auto table=u32(h);
  auto adjusted=Raw(h+std::uint64_t(std::int64_t(s16(mem.load(Address(table)+8*Address(slot),2)))));
  args.insert(args.begin(),i(adjusted)); return call("Holder.v"+std::to_string(slot),std::move(args));
}
void Model::init_holder(Address c,Address h,const Raw *final_table) {
  Depth guard(depth_,limits_.depth);
  raw_creature(c);raw_creature(h);
  write(h,4,0x512148);write(h+4,1,0);write(h,4,0x50cf40);write(h+8,1,0);
  auto block=h+0x50;
  for (unsigned k=0;k<4;++k) write(block+0x24+8*k,2,0xffff);
  write(block+0x21,1,255);write(block+0x20,1,255);
  for (unsigned k=0;k<4;++k) {write(block+0x10+2*k,2,0);write(block+0x18+2*k,2,0);}
  if (final_table) {write(h+0x98,1,0);write(h,4,*final_table);}
}
void Model::skelight_ctor(Address c,const Vec3 &pos,Raw facing,Raw props,Raw level) {
  Depth guard(depth_,limits_.depth);raw_creature(c);
  creature_ctor(c,0,pos,Raw(std::int64_t(s16(facing))),0x502728,props,0x41200000);
  auto holder=c+0x3c8;
  write(c+0x48,4,0x5174b0);init_holder(c,holder);
  write(c+0x28,1,0x14);write(c+0xb0,4,0x3fc00000);write(c+0x1c,4,0x41400000);
  write(c+0xb8,4,0);write(c+0x232,2,0);write(c+0xec,4,u32(FRAME));
  write(c+0xa8,8,mem.load(c+0xa8,8)|0x10000);write(c+0x68,4,ONE);write(c+0x64,4,ONE);
  write(c+0x50,4,holder);holder_call(holder,1,{i(1),i(4)});write(c+0x460,4,level);
  auto saved=u32(0x4f714c);write(0x4f714c,4,from_int(s32(level)));
  auto lower=[](std::string s) {for (auto &ch:s) if (ch>='A' && ch<='Z') ch=char(ch+('a'-'A'));return s;};
  auto same=lower(cstr(0x44fe18))==lower(cstr(0x54c080));
  call("Creature_setStats",{i(c),i(1),i(0x3faa3d70),i(0x3f999999),i(0x3f999999),i(ONE),i(same?ZERO:ONE)});
  write(0x4f714c,4,saved);write(c+0xe8,4,0);
  write(c+0x220,8,std::uint64_t(integer(call("model_buildHideMask",{i(slot16_model(c)),i(0)}))));
  call("anim_clear",{i(c+0x114)});call("anim_push",{i(c+0x114),i(clip(c,0x3c)),i(0xc)});
  skelight_set_state(c,0x17);call("obj_runEveryFrame",{i(c)});
  set_def_entry_params(c,3,{1,0,0,0},{0x3e999999,0,0,0});
  set_def_entry_params(c,4,{1,1,0,0},{0x3e999999,ONE,0,0});
  call("Creature_init",{i(c)});write(c+0xb4,1,0x10);
  write(c+0xa8,8,mem.load(c+0xa8,8)|UINT64_C(0x8000000000000000));
}
std::int64_t Model::summon_can_use(Address skill,Address c) {
  Depth guard(depth_,limits_.depth);raw_creature(skill);raw_creature(c);
  Raw name;
  if (u32(c+0xc)&0x40) {auto slot=u32(c+0x3c8);if(slot>=4)return 0;name=u32(0x503778+4*Address(slot));}
  else name=u32(0x503788);
  return integer(call("world_findObjectByName",{i(name)}))?0:1;
}
Vec3 Model::summon_sample(Address c,std::int32_t count) {
  Depth guard(depth_,limits_.depth);raw_creature(c);
  auto model=model_slot16(c),draw=cosmetic_draw();
  if (!count) throw Error("summon sample requires nonzero count");
  auto magnitude=count<0?std::uint64_t(-std::int64_t(count)):std::uint64_t(count);
  auto result=call("model_getSamplePoint",{i(model),i(c+0x114),vec(c+0x10),
    i(std::uint64_t(std::int64_t(s16(mem.load(c+0x38,2))))),i(draw%magnitude),i(0),i(0),i(ONE)});
  if(auto p=std::get_if<Vec3>(&result))return *p;
  throw Error("summon sample requires vector result");
}
Raw Model::summon_cast(Address skill,Raw level,Address c,const Vec3 &pos,Raw facing) {
  Depth guard(depth_,limits_.depth);raw_creature(skill);raw_creature(c);
  auto face=std::int64_t(s16(facing));
  write(c+0x9a,2,0xff92);write(c+0x9c,2,0xff80);write(c+0x9e,2,0x1e);write(c+0x98,2,0xff80);
  auto count=s32(integer(call("model_getSamplePointCount",{i(model_slot16(c))})));
  if (count) {
    for (unsigned k=0;k<232;++k) {
      auto point=summon_sample(c,count);
      if (k<32) {auto b=cosmetic_ball();for(unsigned j=0;j<3;++j)point[j]=add(point[j],mul(b[j],0x3e72af9a));}
      auto velocity=cosmetic_ball();for(auto &v:velocity)v=mul(v,k<32?0x3d8b4634:0x41c000e5);
      call("particle_spawn",{i(k<32?0x8e5c80:0x8e5cd0),point,velocity,i(ZERO)});
    }
  }
  Vec3 spot{add(pos[0],mul(cos16(face),0x42900000)),add(pos[1],mul(sin16(face),0x42900000)),pos[2]};
  spot[2]=Raw(integer(call("terrain_height",{i(u32(0x44fdc8)),spot,i(0x41400000)})));
  // Reference compares abs(the f32 difference) as a double. Exact raw ordering
  // gives the same strict test, including infinities and unordered NaNs.
  auto difference=sub(spot[2],u32(c+0x18))&0x7fffffff;
  if (c && difference>0x41c00000 && difference<=0x7f800000) {
    auto result=call("world_findFreeSpot",{vec(c+0x10),i(0x48),i(0xc),i(0)});
    auto p=std::get_if<FreeSpot>(&result);if(!p)throw Error("summon free spot requires point and found byte");
    if (signed_byte(p->found))spot=p->point;
  }
  auto d1=mul(from_int(shared_draw()),TWO_M16);
  auto strength=Raw(integer(tagar_virtual("Skill",skill,12,{i(0),i(level),i(c),i(10)})));
  Raw props=0;
  if (!(u32(c+0xc)&0x40))props=0x503770;
  else if(u32(c+0x3c8)<4)props=Raw(0x503750+8*Address(u32(c+0x3c8)));
  auto skel=Raw(integer(call("pool_new",{i(0x468)})));
  if(skel) {
    skelight_ctor(skel,spot,Raw(face),props,strength);write(Address(skel)+0xc,4,u32(Address(skel)+0xc)|0x4000);
    auto chance=Raw(integer(tagar_virtual("Skill",skill,12,{i(1),i(level),i(c),i(10)})));
    if(lt(d1,mul(from_int(s32(chance)),0x3c23d70a))) {
      auto pick=shared_draw()%3,holder=u32(Address(skel)+0x50);
      if(holder)holder_call(holder,2+2*pick,{i(0)});
    }
    if((u32(c+0xc)&0x40) && s32(level)>0) {
      write(Address(skel)+0x5a,1,mem.load(c+0x3c8,1));
      write(Address(skel)+0xa8,8,mem.load(Address(skel)+0xa8,8)|0x400000);
      auto team=tagar_virtual("Creature",c,37,{});
      call("Creature_setCharm",{i(skel),i(UINT32_MAX),team});
      write(Address(skel)+0x3ac,2,400);tagar_virtual("Creature",skel,34,{i(500)});
    }
  }
  auto sound=call("res_find",{i(0x54fdd0),i(0x54fdd8)});
  call("sound_playAtPos",{sound,pos,i(ONE)});
  if(!skel)return 0;
  auto spell=Raw(integer(call("pool_new",{i(0x1c)})));
  if(spell)summon_spell_ctor(spell,level,Raw(c),Raw(skill),skel);
  return spell;
}
void Model::summon_spell_ctor(Address s,Raw level,Raw owner,Raw skill,Raw skel) {
  Depth guard(depth_,limits_.depth);raw_creature(s);
  spell_base_ctor(s,level,owner,skill,0x519918);write(s+0x18,4,0);
  if(skel)call("handle_link",{i(s+0x18),i(skel),i(skel)});
}
void Model::summon_spell_run(Address s,const Vec3 &,Raw) {
  Depth guard(depth_,limits_.depth);raw_creature(s);
  if(!u32(s+0x18) && s)spell_destroy(s);
}
// Zek state-entry and travelling-wave lifecycle, arena specification section 5.
Raw Model::own_draw(Address c) {
  auto seed=Raw(std::uint64_t(u32(c+0x44))*0x19660d+0x3c6ef35f);
  write(c+0x44,4,seed);return seed>>16;
}
void Model::write_vec(Address a,const Vec3 &v) {
  write(a,8,std::uint64_t(v[0])|(std::uint64_t(v[1])<<32));write(a+8,4,v[2]);
}
void Model::clear_path(Address c) {
  write(c+0x244,4,UINT32_MAX);write(c+0xf8,4,0xbf800000);write(c+0xfc,4,0xbf800000);
  for(auto off:{0x240,0x23c,0x248})write(c+off,4,UINT32_MAX);
  write(c+0x2bc,4,0);write(c+0x2c4,4,0);write_vec(c+0x2c8,vec(c+0x10));
  write(c+0x2d4,4,u32(c+0x300));write_vec(c+0x2ec,{});
}
void Model::pick_wander_dest(Address c) {
  auto cf=mem.load(c+0xa8,8);if(cf&0x8000000)return;
  auto home=vec(c+0x3b4);
  auto dx=sub(u32(c+0x10),home[0]),dy=sub(u32(c+0x14),home[1]),dz=sub(u32(c+0x18),home[2]);
  auto d2=add(add(mul(dx,dx),mul(dy,dy)),mul(dz,dz));
  if((cf&0xc00000) && lt(0x46610000,d2))return;
  std::int32_t reach=cf&0xc00000?120:300;
  if(lt(u32(c+0x3c0),from_int(reach)))reach=to_int(u32(c+0x3c0));
  auto point=home;
  if(!le(from_int(s32(std::uint64_t(std::int64_t(reach)*reach))),d2) &&
     !((cf&0x400000) && lt(0x46610000,d2))) {
    for(unsigned k=0;k<10;++k) {
      auto x=add(u32(c+0x3b4),from_int(s32(Raw(own_draw(c)&(Raw(reach)*2))-Raw(reach))));
      auto y=add(u32(c+0x3b8),from_int(s32(Raw(own_draw(c)&(Raw(reach)*2))-Raw(reach))));
      point={x,y,u32(c+0x3bc)};
      auto ground=Raw(integer(call("terrain_heightBelow",{i(u32(0x44fdc8)),point,i(u32(c+0x1c))})));
      auto z=u32(c+0x18);
      if(lt(sub(z,0x41400000),ground) && lt(ground,add(z,0x41400000)))break;
    }
  }
  write_vec(c+0x104,point);
}
void Model::zek_death_virtual(Address c) {
  auto table=u32(c+0x48),fn=u32(Address(table)+28*8+4);
  auto adjusted=Raw(c+std::uint64_t(std::int64_t(s16(mem.load(Address(table)+28*8,2)))));
  if(!external.count("Creature.v28")) {
    if(fn==0x10beb8){slot28_on_death(adjusted,0);return;}
    if(fn==0x2c382c){zek_set_state(adjusted,0);return;}
    if(fn==0x2a91b8){skelight_set_state(adjusted,0);return;}
    if(fn==0x3c2708){slot34_set_home_range(adjusted,0);return;}
    if(fn==0x10e658){slot25_can_see_target(adjusted,0);return;}
    // Other internal redirects have incompatible signatures or unported bodies.
    for(auto known:{0x27236cu,0x272f4cu,0x2a8b84u,0x2c2440u,0x2c34e4u,0x2c3c54u,
                    0x3c2738u,0x10e658u})
      if(fn==known)throw Unsupported("Zek death redirect requires an explicit external selection");
  }
  call("Creature.v28",{i(adjusted),i(0)});
}
void Model::zek_set_state(Address c,Raw requested) {
  Depth guard(depth_,limits_.depth);raw_creature(c);
  auto filtered=filter_state(c,requested);if(filtered.refused)return;
  auto state=filtered.state;auto anim=c+0x114;
  auto start=[&](Raw off,Raw flags,Raw blend=0x3dcccccc){call("anim_start",{i(anim),i(clip(c,off)),i(flags),i(blend)});};
  auto swing=[&](Raw off,Raw flags){call("anim_startBlendInOut",{i(anim),i(clip(c,off)),i(flags),i(0x3dcccccc),i(0x3dcccccc)});};
  switch(state) {
  case 0:start(0x7c,3);break;
  case 0x13:start(0x7c,3,0x3e999999);break;
  case 1:case 3:case 0x10:
    if(state==0x10)write(c+0x318,2,60);
    clear_path(c);if(state==1)pick_wander_dest(c);
    start(0x94,0x803);call("anim_syncToSimFrame",{i(anim)});break;
  case 5:start(0x7c,3);call("anim_syncToSimFrame",{i(anim)});break;
  case 6:write(c+0x47c,4,state);swing(0x1c+8*(own_draw(c)&1),0x808);break;
  case 7:case 8:case 9:case 10:case 11:case 12:case 13:case 14: {
    constexpr Raw offsets[]={0x54,0x8c,0x3c,0x4c,0x34,0x44,0x84,0x2c};
    write(c+0x47c,4,state);swing(offsets[state-7],state==9||state==10?0x4808:0x808);break;
  }
  case 0x12:swing(0x5c+8*(own_draw(c)&3),0x10);break;
  case 0x14: {
    write(c+0xa8,8,mem.load(c+0xa8,8)|UINT64_C(0x20000000000));
    call("Creature_dropLoot",{i(c),vec(c+0x10),i(std::int64_t(s16(mem.load(c+0x38,2)))),i(0),
      i(0x42100000),i(0x40000000),i(0x3fcccccc),i(0x3f199999),i(0x3fb33333),i(0x3e800000)});
    if(cstr(0x44fe18)==cstr(0x54e5f8))start(0x9c,0x806,0x3e4ccccc);
    else {auto h=u32(c+0x50);if(h)holder_call(h,11,{i(1),i(c),i(0),i(0)});start(0xa4,0x806,0x3e4ccccc);}
    zek_death_virtual(c);break;
  }
  default:state=0;break;
  }
  write(c+0x232,2,std::int64_t(state));
}
void Model::zek_ctor(Address c,Raw type,const Vec3 &pos,Raw facing,Raw props) {
  Depth guard(depth_,limits_.depth);raw_creature(c);
  creature_ctor(c,type,pos,Raw(std::int64_t(s16(facing))),0x503688,props,0x41200000);
  auto h=c+0x3c8;write(c+0x48,4,0x5180c8);init_holder(c,h);auto frame=u32(FRAME);
  write(c+0x28,1,0x32);write(c+0xb4,1,11);write(c+0xb0,4,0x3f999999);write(c+0x1c,4,0x41f00000);
  write(c+0xec,4,frame);write(c+0xf4,4,Raw(frame+300));
  write(c+0xa8,8,mem.load(c+0xa8,8)|UINT64_C(0x2000040000));
  for(auto off:{0x68,0x5c,0x60,0x6c,0x64})write(c+off,4,HALF);
  for(auto off:{0x460,0xb8,0x464,0x478})write(c+off,4,0);
  write(c+0x232,2,0);write(c+0x50,4,h);holder_call(h,1,{i(1),i(0x15)});
  call("anim_clear",{i(c+0x114)});zek_set_state(c,0);call("obj_runEvery60",{i(c)});
  for(auto entry:{3u,4u,5u,10u,17u,6u,8u,7u,9u,16u,19u}) {
    std::array<Raw,4> masks={1,0,0,0},times={0x3f4ccccc,0,0,0};
    if(entry==17){masks={1,2,0,0};times={0x3e999999,0x3f333333,0,0};}
    if(entry==16)times[0]=0x3fe66666;
    if(entry==19){masks[0]=0x80;times[0]=0x3e4ccccc;}
    set_def_entry_params(c,entry,masks,times);
  }
  call("Creature_init",{i(c)});
}
std::int32_t Model::zek_slot35(Address c) {
  Depth guard(depth_,limits_.depth);raw_creature(c);return to_int(mul(u32(0x4f714c),0x3e999999));
}
bool Model::zek_specials(Address c,const StateSetter &st) {
  Depth guard(depth_,limits_.depth);raw_creature(c);
  auto clock=s32(u32(c+0x300));
  if(s32(u32(c+0x478))<s32(Raw(clock)-180) && integer(call("creature_findEnemyInCone",{
    i(c),i(std::int64_t(s16(mem.load(c+0x38,2)))),i(5000),i(0x42f00000),i(0x44480000)}))) {
    if(!st)throw MissingHandler("missing Zek state setter");
    st(c,14);return true;
  }
  if(s32(u32(c+0x478))<s32(u32(c+0x300)) && integer(call("creature_findEnemyInCone",{
    i(c),i(std::int64_t(s16(mem.load(c+0x38,2)))),i(0x7fff),i(0),i(0x43820000)}))) {
    if(!st)throw MissingHandler("missing Zek state setter");
    st(c,13);return true;
  }
  return false;
}
void Model::zek_wave(Address c) {
  Depth guard(depth_,limits_.depth);raw_creature(c);if(!u32(c+0x464))return;
  auto wave=c+0x46c;auto old=vec(wave);
  auto cs=cos16(s16(mem.load(c+0x468,2))),sn=sin16(s16(mem.load(c+0x468,2)));
  write(wave+4,4,add(u32(wave+4),mul(sn,0x40a00000)));
  write(wave,4,add(u32(wave),mul(cs,0x40a00000)));
  auto z=add(Raw(integer(call("terrain_height",{i(u32(0x44fdc8)),vec(wave),i(0x41400000)}))),ONE);
  write(wave+8,4,z);bool over=false;
  auto diff=sub(z,old[2])&0x7fffffff;
  if(diff>0x41400000 && diff<=0x7f800000){write_vec(wave,old);over=true;}
  auto node=u32(c+0x460);
  if(node) {
    auto enemy=vec(Address(u32(Address(node)+12))+0x10);
    auto bearing=atan2_16(sub(u32(wave+4),enemy[1]),sub(u32(wave),enemy[0]));
    auto heading=Raw(mem.load(c+0x468,2));auto off=s16(Raw(bearing)-heading);
    auto step=to_int(mul(from_int(off),0x3ba3d70a));
    auto damp=to_int(mul(sqrt(0x3ca3d70a),from_int(step)));
    write(c+0x468,2,Raw(heading-(Raw(step)-Raw(damp))));
    auto dx=sub(u32(wave),enemy[0]),dy=sub(u32(wave+4),enemy[1]);
    if(lt(sqrt(add(mul(dx,dx),mul(dy,dy))),0x42100000))over=true;
  }
  auto dx=sub(u32(c+0x10),u32(wave)),dy=sub(u32(c+0x14),u32(wave+4));
  if(lt(0x491c4000,add(mul(dx,dx),mul(dy,dy))))over=true;
  if(over) {
    write(c+0x464,4,0);call("fx_ringBurst",{vec(wave),i(0x40800000)});
    auto sound=call("res_find",{i(0x54e558),i(0x54e560)});
    call("sound_playOnObject",{sound,i(c),i(ONE)});
    call("combat_applyAreaDamage",{i(c),vec(wave),i(std::int64_t(half_toward_zero(Raw(mem.load(c+0xd0,2))))),
      i(std::int64_t(half_toward_zero(Raw(mem.load(c+0xd2,2))))),i(0x80840),i(0),i(0x42200000),i(0x43340000)});
    auto turn=s16(own_draw(c));call("decal_add",{vec(wave),i(std::int64_t(turn)),i(9),i(0),i(0x42400000)});
    auto block=Raw(integer(call("pool_new",{i(0xbc)})));
    if(block)call("LightEffect_ctor",{i(block),vec(wave),i(0),i(0),i(0x43000000),i(0x42c80000),i(0x429c0000),
      i(0xc2000000),i(0),i(0),i(0),i(0),i(0x43280000),i(0x42700000),i(ONE),i(0x3eb33333)});
    call("handle_clear",{i(c+0x460)});write(c+0x460,4,0);
  }
  call("fx_spawn",{i(7),vec(wave)});
}
// Horsemen setup, mounted state entry and casts, arena specification 3.2/3.4/3.5.
Value Model::horsemen_virtual(Address c,unsigned slot,std::vector<Value> args) {
  Depth guard(depth_,limits_.depth);raw_creature(c);
  auto table=u32(c+0x48),fn=u32(Address(table)+8*slot+4);
  auto adjusted=Raw(c+std::uint64_t(std::int64_t(s16(mem.load(Address(table)+8*slot,2)))));
  auto name="Creature.v"+std::to_string(slot);
  if(!external.count(name)) {
    if(args.empty()) {
      if(fn==0x2c3c54 || (slot==35 && fn==0x118c50))return std::int64_t(zek_slot35(adjusted));
      if(fn==0x3c2738)return std::int64_t(slot16_model(adjusted));
    } else if(args.size()==1) {
      auto arg=Raw(integer(args[0]));
      if(fn==0x272f4c){horsemen_set_state(adjusted,arg);return std::int64_t(0);}
      if(fn==0x2a91b8){skelight_set_state(adjusted,arg);return std::int64_t(0);}
      if(fn==0x2c382c){zek_set_state(adjusted,arg);return std::int64_t(0);}
      if(fn==0x10beb8){slot28_on_death(adjusted,arg);return std::int64_t(0);}
      if(fn==0x10e658)return slot25_can_see_target(adjusted,arg);
      if(fn==0x3c2708){slot34_set_home_range(adjusted,arg);return std::int64_t(0);}
      if(slot==27 && fn==0x3c2688){write(Address(adjusted)+0x232,2,arg);return std::int64_t(0);}
    }
    for(auto known:{0x27236cu,0x272f4cu,0x2a8b84u,0x2a91b8u,0x2c2440u,0x2c382cu,
                    0x2c34e4u,0x2c3c54u,0x3c2738u,0x10e658u,0x10beb8u,0x3c2708u})
      if(fn==known)throw Unsupported("Horsemen virtual body requires a supported signature or explicit external selection");
  }
  args.insert(args.begin(),i(adjusted));return call(name,std::move(args));
}
Value Model::horsemen_skill_cast(Address skill,Raw level,Address c,const Vec3 &pos,Raw facing) {
  raw_creature(skill);auto table=u32(skill+0x60),fn=u32(Address(table)+20);
  auto adjusted=Raw(skill+std::uint64_t(std::int64_t(s16(mem.load(Address(table)+16,2)))));
  if(!external.count("Skill.v2")) {
    if(fn==0x2e18a4)return std::int64_t(summon_cast(adjusted,level,Raw(c),pos,facing));
    if(fn==0x30c708)return std::int64_t(tagar_cast(adjusted,level,Raw(c),pos,facing));
    if(fn==0x2e2368)throw Unsupported("Horsemen cast redirects to incompatible can-use signature");
  }
  return call("Skill.v2",{i(adjusted),i(level),i(c),pos,i(facing)});
}
void Model::horsemen_cast(Address c) {
  Depth guard(depth_,limits_.depth);raw_creature(c);
  auto variant=signed_byte(Raw(mem.load(c+0x470,1)));
  if(variant<0 || variant>3)return;
  auto level=Raw(integer(horsemen_virtual(c,35,{})));
  Vec3 pos;Address global;
  if(variant==0) {
    auto lift=mul(u32(c+0x20),HALF);
    pos={add(u32(c+0x10),ZERO),u32(c+0x14),add(u32(c+0x18),lift)};global=0x41b180;
  } else if(variant==1) {pos=vec(Address(u32(Address(u32(c+0x234))+12))+0x10);global=0x41b18c;}
  else {pos=vec(c+0x10);global=variant==2?0x41b2cc:0x41b19c;}
  auto spell=horsemen_skill_cast(u32(global),level,c,pos,Raw(std::int64_t(s16(mem.load(c+0x38,2)))));
  if(variant==2)call("creature_addSpell",{i(c),spell});
}
void Model::horsemen_set_state(Address c,Raw requested) {
  Depth guard(depth_,limits_.depth);raw_creature(c);
  auto filtered=filter_state(c,requested);if(filtered.refused)return;
  auto state=filtered.state;auto mount=u32(c+0x3c8);
  write(c+0xa8,8,mem.load(c+0xa8,8)&UINT64_C(0xfffffffffffffdff));
  if(mount && state<0x19)return;
  auto anim=c+0x114;
  auto start=[&](Raw off,Raw flags,Raw blend=0x3dcccccc){call("anim_start",{i(anim),i(clip(c,off)),i(flags),i(blend)});};
  auto swing=[&](Raw off,Raw flags){call("anim_startBlendInOut",{i(anim),i(clip(c,off)),i(flags),i(0x3dcccccc),i(0x3dcccccc)});};
  switch(state) {
  case 0: {
    write(c+0xc,4,u32(c+0xc)&0xfffffff6);
    auto clock=u32(c+0x300),draw=own_draw(c);write(c+0xf4,4,Raw(clock+draw%120));start(0x54,3);
    if(u32(c+0x3c8)){call("handle_clear",{i(c+0x3c8)});write(c+0x3c8,4,0);}break;
  }
  case 1:case 3:case 0x10:
    clear_path(c);if(state==1)pick_wander_dest(c);start(0x6c,0x803);call("anim_syncToSimFrame",{i(anim)});break;
  case 5:start(0x3c,3);break;
  case 0x1c:start(0xd4,3);break;case 0x1d:start(0xcc,3);break;
  case 0x1e:start(0xbc,3);break;case 0x1f:start(0xc4,3);break;
  case 6:swing(0x1c,0x808);break;case 7:swing(0x24,0x808);break;case 8:swing(0x74,0x808);break;
  case 10:swing(0x2c,0x808);break;case 11:swing(0x34,0x808);break;
  case 0x17:swing(0x44,0x808);break;case 0x18:swing(0x64,0x808);break;
  case 0x20:swing(0x8c,8);break;case 0x21:swing(0x94,8);break;case 0x22:swing(0x9c,8);break;
  case 0x11:write(c+0xa8,8,mem.load(c+0xa8,8)|0x200);start(0x3c,3);break;
  case 0x12:
    if((s16(mem.load(c+0x232,2))==6 && (mem.load(c+0x11a,2)&8)) || mount)state=s16(mem.load(c+0x232,2));
    else swing(0x4c,0x10);break;
  case 0x13:if(mount)state=s16(mem.load(c+0x232,2));else start(0x54,3,0x3e999999);break;
  case 0x14: {
    write(c+0xa8,8,mem.load(c+0xa8,8)|UINT64_C(0x20000000000));
    if(signed_byte(Raw(mem.load(c+0x470,1)))==3) {
      auto pet=Raw(integer(call("world_findObjectByName",{i(0x547420)})));
      if(pet && (u32(Address(pet)+0xc)&0x20))horsemen_virtual(pet,27,{i(0x14)});
    }
    call("Creature_dropLoot",{i(c),vec(c+0x10),i(std::int64_t(s16(mem.load(c+0x38,2)))),i(0),
      i(0x42100000),i(0x40000000),i(0x3fe66666),i(0x3f199999),i(0x40000000),i(0x3e800000)});
    auto h=u32(c+0x50);if(h){holder_call(h,11,{i(1),i(c),i(0),i(0)});h=u32(c+0x50);}
    if(h)holder_call(h,11,{i(2),i(c),i(0),i(0)});start(0x7c,6);horsemen_virtual(c,28,{i(0)});break;
  }
  case 0x19:write(c+0xc,4,u32(c+0xc)|0xb);start(0xa4,3);break;
  case 0x1a:write(c+0xc,4,u32(c+0xc)&0xfffffffd);start(0xa4,3);break;
  case 0x1b:write(c+0xc,4,u32(c+0xc)|9);start(0xa4,3);break;
  case 0x23:
    start(0xb4,0x80c);
    if(u32(c+0x3c8)) {
      call("handle_clear",{i(c+0x3c8)});write(c+0x3c8,4,0);
      auto z=add(u32(c+0x18),0x42400000);write(c+0x3c,4,z);write(c+0x18,4,z);
    }break;
  default:horsemen_virtual(c,27,{i(3)});break;
  }
  write(c+0x232,2,std::int64_t(state));
}
void Model::horsemen_ctor(Address c,Raw type,const Vec3 &pos,Raw facing,Raw props,Raw variant,Raw mounted) {
  Depth guard(depth_,limits_.depth);raw_creature(c);
  creature_ctor(c,type,pos,Raw(std::int64_t(s16(facing))),0x5006e8,props,0x41200000);
  auto h=c+0x3d0;constexpr Raw table=0x50cda0;
  write(c+0x48,4,0x515730);write(c+0x3c8,4,0);init_holder(c,h,&table);
  write(c+0x28,1,0x30);write(c+0x1c,4,0x41a00000);write(c+0xb8,4,0);write(c+0xb0,4,0x3f999999);
  write(c+0x232,2,0);write(c+0xb4,1,9);write(c+0xec,4,u32(FRAME));write(c+0x50,4,h);
  holder_call(h,1,{i(1),i((2*variant+0x1b)&255)});h=u32(c+0x50);
  if(h)holder_call(h,1,{i(2),i((2*variant+0x23)&255)});
  write(c+0xa8,8,mem.load(c+0xa8,8)|UINT64_C(0x2020020000));write(c+0xdc,4,0x3f999999);
  write(c+0x472,2,0);write(c+0x5c,4,0x3e4ccccc);write(c+0x68,4,ONE);write(c+0x64,4,ONE);write(c+0x6c,4,0x3e999999);
  auto draw=own_draw(c);write(c+0x470,1,variant);auto bits=Raw(1u<<(variant&31));write(c+0xe8,4,bits);
  write(c+0x474,4,Raw(u32(c+0x300)+draw%120+480));
  write(c+0x220,8,std::uint64_t(integer(call("model_buildHideMask",{i(class_model(c)),i(bits)}))));
  if(signed_byte(Raw(mem.load(c+0x470,1)))==3)call("lump_load",{i(0x547310)});
  call("anim_clear",{i(c+0x114)});call("anim_push",{i(c+0x114),i(clip(c,0x54)),i(1)});call("obj_runEvery60",{i(c)});
  struct Entry {Raw entry;std::array<Raw,4> masks,times;};
  const Entry entries[]={
    {3,{1,1,0,0},{0x3e4ccccc,0x3f199999,0,0}},
    {4,{1,1,1,0},{0x3e4ccccc,0x3f199999,ONE,0}},
    {5,{2,0,0,0},{0x3e999999,0,0,0}},
    {6,{1,1,1,0},{0x3e999999,0x3f4ccccc,0x3fcccccc,0}},
    {14,{1,0,0,0},{HALF,0,0,0}},{8,{1,0,0,0},{0x40000000,0,0,0}},
    {17,{1,0,0,0},{0x3e4ccccc,0,0,0}},{18,{1,0,0,0},{0x3e4ccccc,0,0,0}},
    {19,{1,0,0,0},{0x3e4ccccc,0,0,0}},{24,{1,0,0,0},{0x3e4ccccc,0,0,0}},
    {15,{0x80,0,0,0},{0x3dcccccc,0,0,0}}};
  for(const auto &e:entries)set_def_entry_params(c,e.entry,e.masks,e.times);
  if(mounted) {
    auto block=Raw(integer(call("pool_new",{i(0x3d8)})));
    if(block) {
      call("Nightmare_ctor",{i(block),i(0x547380),vec(c+0x10),i(std::int64_t(s16(mem.load(c+0x38,2)))),
        i(0x500848+12*std::int64_t(signed_byte(Raw(mem.load(c+0x470,1)))))});
      call("handle_link",{i(c+0x3c8),i(block),i(block)});call("handle_link",{i(Address(block)+0x3c8),i(c),i(c)});
      horsemen_set_state(c,0x1b);
    }
  }
  call("Creature_init",{i(c)});
}
} // namespace champions::simulation::rta::arena_monsters
