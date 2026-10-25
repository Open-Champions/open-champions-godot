// Authored translation of the pinned MIT research contracts; see MONSTER_SYSTEMS.md.
#include "champions/monsters_1.hpp"
#include "champions/core_math.hpp"
#include <utility>
#include <algorithm>
namespace champions::simulation::rta::monsters_1 {
namespace {
constexpr std::size_t history_limit = 65536;
void creature_span(Address c) {
  if (c > UINT32_MAX - 0x3c7u) throw Unsupported("creature raw32 span");
}
Raw pointer_result(std::uint64_t value) {
  if (value > UINT32_MAX) throw Unsupported("callback pointer outside raw32");
  return static_cast<Raw>(value);
}
struct SkelightScope {
  bool &value;
  bool saved;
  explicit SkelightScope(bool &v) : value(v), saved(v) { value = true; }
  ~SkelightScope() { value = saved; }
};
void skelight_span(Address c) {
  if (c > UINT32_MAX - 0x463u) throw Unsupported("Skelight raw32 span");
}
struct Depth {
  std::size_t &value;
  explicit Depth(std::size_t &v) : value(v) {
    if (v == 64) throw Unsupported("monster recursion limit");
    ++value;
  }
  ~Depth() { --value; }
};
std::int32_t signed8(std::uint64_t v) {
  return (v & 128) ? static_cast<std::int32_t>(v & 255) - 256 : static_cast<std::int32_t>(v & 255);
}
}
// Hand-authored ABI projection of all 143 pinned EXTERNALS rows. Addresses,
// float-register counts and notes do not participate in shape_result.
namespace {
struct Signature { const char *kinds; char result; };
const std::map<std::string, Signature> signatures = {
  {"obj_getPropStr", {"ppi", 'i'}},
  {"obj_getPropInt", {"ppi", 'i'}},
  {"obj_updateGridCell", {"p", 'n'}},
  {"obj_runEveryFrame", {"p", 'n'}},
  {"obj_delete", {"p", 'n'}},
  {"script_runByName", {"p", 'n'}},
  {"world_notifyObjectsNear", {"ivp", 'n'}},
  {"pool_new", {"i", 'i'}},
  {"pathFollower_init", {"-pv", 'n'}},
  {"paths_findByName", {"p", 'i'}},
  {"pointList_snapToNearest", {"-", 'i'}},
  {"pointList_getPoint", {"pi", 'i'}},
  {"players_anyWithinRadius", {"v", 'i'}},
  {"net_objOrGhost", {"p", 'i'}},
  {"player_speedScale", {"i", 'f'}},
  {"player_getSkillRank", {"ii", 'i'}},
  {"player_giveExp", {"ii", 'n'}},
  {"SkillLifeTap_getStat0Fraction", {"pi", 'f'}},
  {"anim_advance", {"pppp", 'i'}},
  {"anim_releaseCaches", {"p", 'n'}},
  {"anim_snapTopToEnd", {"p", 'n'}},
  {"model_getBounds", {"piwwwwww", 'n'}},
  {"model_getAttachPoint", {"pvipioO", 'n'}},
  {"SpellDrowsy_typeinfo", {"", 'i'}},
  {"creature_findSpellByType", {"pp", 'i'}},
  {"SpellDrowsy_getStat3Fraction", {"p", 'f'}},
  {"creature_bossRank", {"p", 'i'}},
  {"damage_apply", {"ppiiiiei", 'i'}},
  {"fx_spawn", {"iv", 'n'}},
  {"fx_burnModel", {"ppvip", 'n'}},
  {"fx_shockModel", {"ppvipi", 'n'}},
  {"fx_disintegrateModel", {"pppvii", 'n'}},
  {"fx_shatterToLooseIce", {"ppvip", 'n'}},
  {"particle_spawn", {"pvv", 'n'}},
  {"decal_add", {"viii", 'n'}},
  {"GutChunk_ctor", {"pvii", 'n'}},
  {"PhProp_initPhysics", {"pi", 'n'}},
  {"OniBall_ctor", {"pvipi", 'n'}},
  {"Critter_ctor", {"ppvip", 'n'}},
  {"libc_sprintf", {"-pi", 's'}},
  {"res_find", {"pS", 'i'}},
  {"sound_playOnObject", {"pp", 'i'}},
  {"terrain_raycast", {"pvvi", 'f'}},
  {"world_raycast", {"pvvpWii", 'f'}},
  {"terrain_heightBelow", {"pv", 'f'}},
  {"world_collectObjectsInBox", {"vvni", 'n'}},
  {"world_slideMove", {"pviO", 'i'}},
  {"world_findFreeSpot", {"oviibi", 'n'}},
  {"water_test", {"pvi", 'i'}},
  {"water_disturb", {"pv", 'i'}},
  {"path_requestToPoint", {"pvvppi", 'n'}},
  {"path_requestType1", {"pvvppi", 'n'}},
  {"path_requestType2", {"pvvppi", 'n'}},
  {"path_requestType3", {"pvvppi", 'n'}},
  {"handle_clear", {"p", 'n'}},
  {"handle_link", {"ppp", 'n'}},
  {"handle_removeNode", {"p", 'i'}},
  {"iter_begin", {"-pi", 'i'}},
  {"iter_next", {"-", 'i'}},
  {"iter_end", {"-i", 'n'}},
  {"Creature.v2", {"ppi", 'n'}},
  {"Creature.v6", {"pp", 'n'}},
  {"Creature.v7", {"pi", 'n'}},
  {"Creature.v10", {"pv", 'n'}},
  {"Creature.v11", {"pl", 'n'}},
  {"Creature.v17", {"pv", 'n'}},
  {"Creature.v19", {"pp", 'n'}},
  {"Creature.v20", {"pp", 'i'}},
  {"Creature.v23", {"pi", 'n'}},
  {"Creature.v26", {"piv", 'i'}},
  {"Creature.v27", {"pi", 'n'}},
  {"Creature.v30", {"pii", 'n'}},
  {"Creature.v31", {"pi", 'n'}},
  {"Creature.v35", {"p", 'i'}},
  {"Creature.v36", {"p", 'i'}},
  {"Creature.v37", {"p", 'i'}},
  {"Skill.v11", {"piii", 'i'}},
  {"Spell.v2", {"pvi", 'n'}},
  {"Creature_applyKnockback", {"p", 'n'}},
  {"Creature_deadUpdate", {"pivi", 'n'}},
  {"Creature_dropLoot", {"pvii", 'n'}},
  {"Creature_hitTargetInArc", {"piiiiiii", 'i'}},
  {"Creature_hitAllEnemiesInArc", {"piiiiiii", 'i'}},
  {"creature_canSeeObj", {"ppiii", 'i'}},
  {"creature_findEnemyInCone", {"pii", 'i'}},
  {"creature_countAlliesNear", {"pv", 'i'}},
  {"creature_isBlockedBy", {"pp", 'i'}},
  {"creature_addSpell", {"pp", 'n'}},
  {"creature_stopBossMusic", {"p", 'n'}},
  {"Creature_setCharm", {"pii", 'n'}},
  {"Creature_setStats", {"pi", 'n'}},
  {"Creature_init", {"p", 'n'}},
  {"balance_lookup", {"pwwwww", 'i'}},
  {"GameObject_ctor", {"pvpp", 'n'}},
  {"obj_moveOffGrid", {"p", 'n'}},
  {"obj_runEvery60", {"p", 'n'}},
  {"world_findObjectByName", {"p", 'i'}},
  {"loot_spawnNamedItem", {"vp", 'n'}},
  {"arena_addKill", {"pi", 'n'}},
  {"lump_load", {"p", 'n'}},
  {"Nightmare_ctor", {"ppvip", 'n'}},
  {"GroundPoundEffect_ctor", {"pvipip", 'n'}},
  {"LightEffect_ctor", {"pvii", 'n'}},
  {"particleDef_ctor", {"piiiiiii", 'n'}},
  {"anim_start", {"ppi", 'n'}},
  {"anim_startBlendInOut", {"ppi", 'n'}},
  {"anim_push", {"ppi", 'n'}},
  {"anim_clear", {"p", 'n'}},
  {"anim_syncToSimFrame", {"p", 'n'}},
  {"anim_jumpTopToEnd", {"p", 'n'}},
  {"model_buildHideMask", {"pi", 'q'}},
  {"model_getSamplePointCount", {"p", 'i'}},
  {"model_getSamplePoint", {"ppviioOi", 'n'}},
  {"fx_ringBurst", {"v", 'n'}},
  {"combat_applyAreaDamage", {"pviiii", 'n'}},
  {"sound_playAtPos", {"pv", 'n'}},
  {"terrain_height", {"pv", 'f'}},
  {"Creature.v16", {"p", 'i'}},
  {"Creature.v25", {"pi", 'i'}},
  {"Creature.v28", {"pi", 'n'}},
  {"Creature.v34", {"pi", 'n'}},
  {"Holder.v1", {"pii", 'n'}},
  {"Holder.v2", {"pi", 'n'}},
  {"Holder.v4", {"pi", 'n'}},
  {"Holder.v6", {"pi", 'n'}},
  {"Holder.v11", {"pipii", 'n'}},
  {"Holder.v15", {"pi", 'i'}},
  {"Holder.v17", {"pi", 'i'}},
  {"Holder.v21", {"ppp", 'n'}},
  {"Holder.v22", {"ppp", 'n'}},
  {"Skill.v1", {"pp", 'i'}},
  {"Skill.v2", {"pipvi", 'i'}},
  {"Skill.v12", {"piipi", 'i'}},
  {"Skill.v14", {"pii", 'i'}},
  {"Spell.v1", {"pi", 'n'}},
  {"creature_findSpell", {"pip", 'i'}},
  {"world_collectInSphere", {"viL", 'n'}},
  {"world_collectObjectsInArc", {"viLp", 'n'}},
  {"Skill.v8", {"piiiww", 'n'}},
  {"anim_randomizeTopTime", {"p", 'n'}},
  {"Creature_hitTargetInArcEdge", {"piiiiiii", 'i'}},
  {"Creature_hitTargetAtReach", {"piiiiE", 'i'}},
  {"GutChunk_ctorFromProp", {"pviipiii", 'n'}},
};
bool truth(const Argument &a) {
  if (const auto *v=std::get_if<Raw>(&a)) return *v!=0;
  if (const auto *v=std::get_if<std::string>(&a)) return !v->empty();
  if (!std::holds_alternative<Structured>(a)) return true;
  const auto &v=std::get<Structured>(a);
  switch(v.kind) {
  case creature_movement::ValueKind::None: return false;
  case creature_movement::ValueKind::Integer: return v.scalar!=0;
  case creature_movement::ValueKind::List:
  case creature_movement::ValueKind::Tuple: return !v.items.empty();
  default: return true; // vector and ('buf', bytes) tuples are always nonempty
  }
}
}
Result shape_result(const std::string &name,const std::vector<Argument> &args,
                    HeadResult ret,HeadResult fret,
                    std::optional<std::vector<Structured>> outs) {
  const auto found=signatures.find(name);
  if (found==signatures.end()) throw Unsupported("unknown callback ABI: "+name);
  const auto &ext=found->second;
  std::vector<Structured> values;
  std::size_t position=0,given=0;
  for (const char kind:std::string(ext.kinds)) {
    if (std::string("oOwWbnL").find(kind)!=std::string::npos) {
      Structured value;
      if (outs && given<outs->size()) value=(*outs)[given++];
      else if (kind=='o' || kind=='O') value=Structured::vector({});
      else if (kind=='n' || kind=='L') value=Structured::list({});
      else value=Structured::integer(0);
      // Consume an optional output even when its pointer is null.
      if (kind!='O' && kind!='W') values.push_back(std::move(value));
      else {
        if (position>=args.size()) throw Unsupported("missing optional output argument");
        if (truth(args[position])) values.push_back(std::move(value));
      }
    }
    if (std::string("ipvelsSOWE").find(kind)!=std::string::npos) ++position;
  }
  if (ext.result=='s') {
    if (outs && given<outs->size()) return BufferResult{(*outs)[given]};
    return BufferResult{Structured::buffer({})};
  }
  const auto head=ext.result=='f'?fret:ret;
  if (ext.result!='n') {
    if (values.empty()) return std::visit([](const auto &v) -> Result { return v; },head);
    Structured value;
    if (const auto *integer=std::get_if<std::uint64_t>(&head)) {
      if (*integer>INT64_MAX) return CompoundResult{head,std::move(values)};
      value=Structured::integer(static_cast<std::int64_t>(*integer));
    } else if (const auto *structured=std::get_if<Structured>(&head)) value=*structured;
    else return CompoundResult{head,std::move(values)};
    values.insert(values.begin(),std::move(value));
  }
  if (values.empty()) return Structured{};
  if (values.size()==1) return values.front();
  return Structured::tuple(std::move(values));
}
Model::Model(Memory &m, CallbackPolicy p) : mem(m), policy(p) {}
Raw Model::anim_flags(Address c, Raw mask) const {
  if (c>UINT32_MAX-0x11bu) throw Unsupported("animation flag raw32 span");
  return static_cast<Raw>(mem.load(c+0x11a,2)) & mask;
}
Raw Model::u32(Address a) const { return static_cast<Raw>(mem.load(a, 4)); }
Vec3 Model::vec(Address a) const { return {u32(a), u32(a+4), u32(a+8)}; }
void Model::write(Address a, std::size_t size, std::uint64_t v) {
  if (writes.size() >= history_limit || events.size() >= history_limit)
    throw Unsupported("monster history limit");
  if (a > UINT32_MAX) throw Unsupported("non-raw32 write address");
  if (size < 8) v &= (UINT64_C(1) << (size*8)) - 1;
  Write w{static_cast<Raw>(a), size, v};
  writes.push_back(w); events.push_back({false, w, {}});
  mem.store(a, size, v);
}
void Model::wvec(Address a, Vec3 v) {
  write(a, 8, v[0] | (static_cast<std::uint64_t>(v[1]) << 32));
  write(a+8, 4, v[2]);
}
void Model::record_call(const Call &c) {
  if (calls.size() >= history_limit || events.size() >= history_limit)
    throw Unsupported("monster history limit");
  calls.push_back(c); events.push_back({true, {}, c});
}
std::uint64_t Model::call(const std::string &name, std::vector<Argument> args) {
  const auto result=call_value(name,std::move(args));
  if (const auto *v=std::get_if<std::uint64_t>(&result)) return *v;
  if (std::holds_alternative<BufferResult>(result)) throw Unsupported("buffer callback used by integer-only body");
  if (std::holds_alternative<CompoundResult>(result)) throw Unsupported("compound callback used by integer-only body");
  const auto &v=std::get<Structured>(result);
  if (v.kind==creature_movement::ValueKind::None) return 0;
  if (v.kind==creature_movement::ValueKind::Integer) return static_cast<std::uint64_t>(v.scalar);
  throw Unsupported("noninteger callback used by integer-only body");
}
Result Model::call_value(const std::string &name,std::vector<Argument> args) {
  for (auto &arg:args) if (auto *v=std::get_if<Structured>(&arg))
    if (v->kind==creature_movement::ValueKind::Integer) arg=static_cast<Raw>(v->scalar);
  Call c{name,std::move(args)};
  record_call(c);
  auto complete=result_callbacks.find(name);
  if (complete!=result_callbacks.end()) { auto fn=complete->second;return fn(*this,c.args); }
  auto structured=structured_callbacks.find(name);
  if (structured!=structured_callbacks.end()) { auto fn=structured->second;return fn(*this,c.args); }
  auto it=callbacks.find(name);
  if (it!=callbacks.end()) { auto fn=it->second;return fn(*this,c.args); }
  if (balance_lookup && name=="balance_lookup") {
    auto fn=balance_lookup;auto factors=fn(*this,std::get<Raw>(c.args.at(0)));
    std::vector<Structured> values;for (auto f:factors) values.push_back(Structured::integer(f));
    return Structured::tuple(std::move(values));
  }
  if (policy==CallbackPolicy::RequireHandlers)
    throw creature_hp::MissingHandler("missing monster callback: "+name);
  return shape_result(name,c.args);
}
std::uint32_t Model::random(Address c) {
  const auto seed = u32(c+0x44)*0x19660du + 0x3c6ef35fu;
  write(c+0x44,4,seed); return seed >> 16;
}
std::pair<bool, std::int32_t> Model::filter(Address c, std::uint32_t requested) {
  auto state = s32(requested);
  if (u32(0x4f7120) && !u32(0x4f711c) && u32(0x503890) && !u32(0x503894)) return {true,state};
  if (signed8(mem.load(c+0x231,1)) && state == 1) return {true,state};
  if (state == 20) for (auto off : {0x31c,0x318,0x314,0x31a}) write(c+off,2,0);
  if (mem.load(c+0x314,2) && state != 0 && state != 5 && !(state >= 6 && state <= 15))
    state = u32(c+0x234) ? 5 : 0;
  else if (mem.load(c+0x31c,2) && state != 19) state = 19;
  else {
    if (!u32(c+0x234)) write(c+0x318,2,0);
    if (mem.load(c+0x318,2) && state != 16) state = 16;
  }
  if (state == 0) { call("handle_clear", {static_cast<Raw>(c+0x234)}); write(c+0x234,4,0); }
  return {false,state};
}
void Model::clear_path(Address c) {
  for (auto off : {0x244,0xf8,0xfc,0x240,0x23c,0x248})
    write(c+off,4,(off == 0xf8 || off == 0xfc) ? MINUS_ONE : UINT32_MAX);
  write(c+0x2bc,4,0); write(c+0x2c4,4,0);
  wvec(c+0x2c8,vec(c+0x10)); write(c+0x2d4,4,u32(c+0x300)); wvec(c+0x2ec,{0,0,0});
}
void Model::wander(Address c) {
  const auto cf = mem.load(c+0xa8,8);
  if (cf & 0x8000000) return;
  auto home = vec(c+0x3b4);
  const auto distance = [&] {
    auto p = vec(c+0x10);
    auto x = sub(p[0],home[0]), y = sub(p[1],home[1]), z = sub(p[2],home[2]);
    return add(add(mul(x,x),mul(y,y)),mul(z,z));
  };
  if ((cf & 0xc00000) && lt(0x46610000,distance())) return;
  std::int32_t reach = (cf & 0xc00000) ? 120 : 300;
  if (lt(u32(c+0x3c0),from_int(static_cast<Raw>(reach)))) reach = to_int(u32(c+0x3c0));
  auto d2 = distance();
  const auto square = static_cast<Raw>(reach)*static_cast<Raw>(reach);
  if (le(from_int(square),d2) || ((mem.load(c+0xa8,8)&0x400000) && lt(0x46610000,d2))) {
    wvec(c+0x104,home); return;
  }
  Vec3 point = home;
  for (unsigned i=0;i<10;++i) {
    auto x = add(u32(c+0x3b4),from_int((random(c)&(static_cast<Raw>(reach)*2))-static_cast<Raw>(reach)));
    auto y = add(u32(c+0x3b8),from_int((random(c)&(static_cast<Raw>(reach)*2))-static_cast<Raw>(reach)));
    point = {x,y,u32(c+0x3bc)};
    auto ground = static_cast<Raw>(call("terrain_heightBelow",{u32(0x44fdc8),point,u32(c+0x1c)}));
    auto z = u32(c+0x18);
    if (lt(sub(z,0x41400000),ground) && lt(ground,add(z,0x41400000))) break;
  }
  wvec(c+0x104,point);
}
void Model::start(Address c, std::uint32_t offset, std::uint32_t flags, Raw blend) {
  call("anim_start",{static_cast<Raw>(c+0x114),u32(static_cast<Address>(u32(c+0x24))+offset),flags,blend});
}
void Model::swing(Address c, std::uint32_t offset, std::uint32_t flags) {
  call("anim_startBlendInOut",{static_cast<Raw>(c+0x114),u32(static_cast<Address>(u32(c+0x24))+offset),flags,0x3dccccccu,0x3dccccccu});
}
void Model::drop_loot(Address c, Raw item, Raw a, Raw b, Raw d) {
  call("Creature_dropLoot",{static_cast<Raw>(c),vec(c+0x10),static_cast<Raw>(s16(static_cast<Raw>(mem.load(c+0x38,2)))),item,0x42100000u,TWO,a,b,d,0x3e800000u});
}
void Model::on_death(Address c, std::uint32_t quiet) {
  write(c+0xa8,8,mem.load(c+0xa8,8)&UINT64_C(0xfffffffff73fffff));
  auto script = pointer_result(call("obj_getPropStr",{static_cast<Raw>(c),0x52fbf8u,0u}));
  if (script) { write(0x4039f8,4,c); call("script_runByName",{script}); write(0x4039f8,4,0); }
  if (!quiet) {
    auto item = pointer_result(call("obj_getPropStr",{static_cast<Raw>(c),0x52fc08u,0u}));
    if (item) call("loot_spawnNamedItem",{vec(c+0x10),item});
  }
  auto flags = u32(c+0xc);
  if (flags&2) {
    write(c+0xc,4,flags|0x4000); call("anim_jumpTopToEnd",{static_cast<Raw>(c+0x114)}); call("obj_delete",{static_cast<Raw>(c)});
  }
  if (u32(0x4f7120)) {
    auto node = u32(c+0x2e4);
    call("arena_addKill",{node ? u32(static_cast<Address>(node)+0xc) : 0u,0u});
  }
  call("creature_stopBossMusic",{static_cast<Raw>(c)});
}
std::optional<std::uint64_t> Model::virtual_call(Address c, unsigned slot, std::uint32_t arg) {
  if (skelight_domain_) return dispatch(c,slot,{arg});
  const auto table = u32(c+0x48);
  const auto fn = u32(static_cast<Address>(table)+8*slot+4);
  const auto self = static_cast<Raw>(c + s16(static_cast<Raw>(mem.load(static_cast<Address>(table)+8*slot,2))));
  const auto name = "Creature.v"+std::to_string(slot);
  if (!external.count(name)) {
    if (slot == 27 && fn == 0x2ab704) { smallspider_set_state(self,arg); return std::nullopt; }
    if (slot == 27 && fn == 0x267a0c) { gnaw_set_state(self,arg); return std::nullopt; }
    if (slot == 27 && fn == 0x297f54) { mosquito_set_state(self,arg); return std::nullopt; }
    if (slot == 27 && fn == 0x2a55ec) { scorpion_set_state(self,arg); return std::nullopt; }
    if (slot == 27 && fn == 0x244aa4) { badger_set_state(self,arg); return std::nullopt; }
    if (slot == 27 && fn == 0x3c2688) { write(static_cast<Address>(self)+0x232,2,arg); return std::nullopt; }
    if (slot == 28 && fn == 0x10beb8) { on_death(self,arg); return std::nullopt; }
    // Both derived Python dispatch tables recognize body pointers even in an
    // unexpected slot. Do not turn such unported bodies into guessed callbacks.
    constexpr Raw modelled[] = {
      0x2ab5d8,0x2ab704,0x2ab8e0,0x267654,0x267a0c,0x297b30,
      0x297f54,0x2a5068,0x2a55ec,0x244380,0x244aa4,0x244a9c,
      0x27236c,0x272f4c,0x2a8b84,0x2a91b8,0x2c2440,0x2c382c,
      0x2c34e4,0x2c3c54,0x3c2738,0x10e658,0x10beb8,0x3c2708};
    for (auto body : modelled)
      if (fn == body) throw Unsupported("unported or mismatched modelled virtual body");
  }
  return call(name,{self,arg});
}
void Model::spider_state_body(Address c, std::uint32_t requested) {
  auto [refused,state] = filter(c,requested);
  if (refused) return;
  if (state == 0) {
    auto d = random(c); write(c+0xf4,4,u32(c+0x300)+d%300+120); start(c,0x3c,3,0x3e999999);
  } else if (state == 1) { clear_path(c); wander(c); start(c,0x4c,3,0x3e999999); }
  else if (state == 5 || state == 16 || state == 18 || state == 19) state = s16(static_cast<Raw>(mem.load(c+0x232,2)));
  else if (state == 20) {
    write(c+0xc,4,u32(c+0xc)|9);
    auto angle = random(c); auto size = random(c)%10+10;
    call("decal_add",{vec(c+0x10),static_cast<Raw>(s16(angle)),6u,1u,from_int(size)});
    start(c,0x1c+8*(random(c)&3),6,0x3e999999);
    if (u32(c+0xc)&2) call("anim_jumpTopToEnd",{static_cast<Raw>(c+0x114)});
    virtual_call(c,28,0);
  }
  write(c+0x232,2,static_cast<Raw>(state));
}
void Model::smallspider_set_state(Address c, std::uint32_t state) {
  Depth guard(depth_); creature_span(c); spider_state_body(c,state);
}
void Model::gnaw_set_state(Address c, std::uint32_t requested) {
  Depth guard(depth_); creature_span(c);
  auto [refused,state] = filter(c,requested); if (refused) return;
  const auto anim = static_cast<Raw>(c+0x114);
  if (state == 0) start(c,0x1c,3,0x3dcccccc);
  else if (state == 1) { clear_path(c); wander(c); start(c,0x3c,0x803,0x3dcccccc); call("anim_syncToSimFrame",{anim}); }
  else if (state == 3) { clear_path(c); start(c,0x3c,0x803,0x3dcccccc); }
  else if (state == 5) { start(c,0x1c+8*(random(c)%3),3,0x3dcccccc); call("anim_syncToSimFrame",{anim}); }
  else if (state == 6) swing(c,0x1c+8*(random(c)%3),0x808);
  else if (state == 16) { write(c+0x318,2,60); clear_path(c); start(c,0x34,0x803,0x3dcccccc); call("anim_syncToSimFrame",{anim}); }
  else if (state == 18) state = s16(static_cast<Raw>(mem.load(c+0x232,2)));
  else if (state == 19) start(c,0x1c,3,0x3e999999);
  else if (state == 20) {
    for (unsigned i=0;i<6;++i) {
      auto spot=vec(c+0x10); spot[2]=add(spot[2],0x41c00000);
      if (i<2) {
        auto block=pointer_result(call("pool_new",{0x268u}));
        if (block) { auto variant=random(c)%3; call("GutChunk_ctor",{block,spot,variant,2u,ONE}); call("PhProp_initPhysics",{block,0x12u}); }
      }
      call("fx_spawn",{9u,spot});
    }
    start(c,0x1c,0x803,0x3dcccccc); virtual_call(c,28,0);
  } else state=0;
  write(c+0x232,2,static_cast<Raw>(state));
}
void Model::mosquito_set_state(Address c, std::uint32_t requested) {
  Depth guard(depth_); creature_span(c);
  auto [refused,state]=filter(c,requested); if (refused) return;
  if (state == 0 || state == 5) {
    start(c,0x84,3,0x3dcccccc);
    // The clock is read before the RNG write, as in the reference expression.
    auto clock=u32(c+0x300); auto d=random(c);
    write(c+0xf4,4,clock+d%120+(state==5 ? 60 : 0));
  } else if (state == 1) { clear_path(c); wander(c); start(c,0x64,0x803,HALF); }
  else if (state == 3 || state == 16) {
    if (state==16) write(c+0x318,2,60);
    clear_path(c); start(c,0x64,0x803,0x3dcccccc);
  } else if (state == 6) swing(c,0x1c+8*(random(c)&1),0x808);
  else if (state == 18) { swing(c,0x6c+8*(random(c)&1),0x810); mosquito_lose_part(c,random(c)%3+1); }
  else if (state == 19) start(c,0x84,3,0x3dcccccc);
  else if (state == 20) {
    drop_loot(c,0,0x3fcccccc,0x3f199999,0x3fb33333);
    start(c,0x2c+8*(random(c)&3),0x806,0x3e4ccccc);
    mosquito_lose_part(c,(random(c)&3)+4); virtual_call(c,28,0);
  } else if (state == 23) swing(c,0x4c+8*(random(c)%3),0x810);
  else state=0;
  write(c+0x232,2,static_cast<Raw>(state));
  auto flags=mem.load(c+0xa8,8);
  write(c+0xa8,8,(s16(static_cast<Raw>(state))==0 || s16(static_cast<Raw>(state))==20) ? flags&~UINT64_C(0x100000) : flags|UINT64_C(0x100000));
}
void Model::scorpion_set_state(Address c, std::uint32_t requested) {
  Depth guard(depth_); creature_span(c);
  auto [refused,state]=filter(c,requested); if (refused) return;
  if (state == 0) { auto clock=u32(c+0x300); auto d=random(c); write(c+0xf4,4,clock+d%60+120); start(c,0x84,3,0x3e999999); }
  else if (state == 1) { clear_path(c); wander(c); start(c,0x94,0x803,0x3e999999); }
  else if (state == 3 || state == 16) { clear_path(c); start(c,0x9c,0x803,0x3e999999); }
  else if (state == 5) start(c,0x8c,3,0x3dcccccc);
  else if (state == 6) {
    start(c,0x8c,3,0x3dcccccc);
    if (random(c)%6==0) { state=7; swing(c,0x44,8); }
    else swing(c,0x2c+8*(random(c)%3),8);
    write(c+0xf4,4,random(c)%20);
  } else if (state == 18) {
    auto current=s16(static_cast<Raw>(mem.load(c+0x232,2)));
    if (current==19 || (current==6 && anim_flags(c,8))) state=current;
    else swing(c,0x7c,0x10);
  } else if (state == 19) start(c,0x84,3,0x3e999999);
  else if (state == 20) {
    if (random(c)%3==0) scorpion_sever_part(c,random(c)%6);
    drop_loot(c,0x54b648,0x3fcccccc,0x3f199999,0x3fb33333);
    start(c,0x5c+8*(random(c)&1),0x806,0x3e999999); virtual_call(c,28,0);
  } else if (state == 23 || state == 24) { start(c,0x8c,3,0x3dcccccc); swing(c,state==23 ? 0x6c : 0x74,0x808); }
  write(c+0x232,2,static_cast<Raw>(state));
}
void Model::badger_set_state(Address c, std::uint32_t requested) {
  Depth guard(depth_);
  // Badger state transitions also write the extension word at +0x3c8.
  if (c>UINT32_MAX-0x3cfu) throw Unsupported("badger raw32 span");
  auto [refused,state]=filter(c,requested); if (refused) return;
  const auto anim=static_cast<Raw>(c+0x114);
  if (state == 0) { start(c,0x7c,3,0x3dcccccc); write(c+0xf4,4,u32(c+0x300)+180); }
  else if (state == 1) {
    clear_path(c); wander(c); start(c,mem.load(c+0x312,2)==0 ? 0x8c : 0x4c,0x803,0x3dcccccc); call("anim_syncToSimFrame",{anim});
  } else if (state == 3) {
    clear_path(c); auto current=s16(static_cast<Raw>(mem.load(c+0x232,2)));
    if (current!=3 && !(current>=6 && current<=10)) start(c,0x4c,0x803,0x3e4ccccc);
    call("anim_syncToSimFrame",{anim});
  } else if (state == 5 || state == 19) start(c,0x7c,3,0x3dcccccc);
  else if (state>=6 && state<=10) {
    constexpr Raw offsets[]={0x24,0x54,0x2c,0x3c,0x34};
    swing(c,offsets[state-6],(state==8 || state==9) ? 0x4808 : 0x808); write(c+0x3c8,4,0);
  } else if (state == 16) { clear_path(c); start(c,0x44,0x803,0x3dcccccc); call("anim_syncToSimFrame",{anim}); }
  else if (state == 18) {
    if (s16(static_cast<Raw>(mem.load(c+0x232,2)))==6 && anim_flags(c,8)) state=6;
    else swing(c,0x6c+8*(random(c)&1),0x10);
  } else if (state == 20) {
    auto flags=u32(c+0xc); if (!(flags&2)) write(c+0xc,4,flags&~1u);
    drop_loot(c,0,0x40333333,0x40400000,0x40400000);
    start(c,0x5c+8*(random(c)&1),6,0x3dcccccc); virtual_call(c,28,0);
  }
  write(c+0x232,2,static_cast<Raw>(state));
}

void Model::smallspider_on_touch(Address c, std::uint32_t other) {
  Depth guard(depth_); creature_span(c);
  if (other && s16(static_cast<Raw>(mem.load(c+0x232,2))) != 20 && (u32(static_cast<Address>(other)+0xc)&0x40)) virtual_call(c,27,20);
  if (u32(static_cast<Address>(other)+0xc)&0x40) write(c+0xa8,8,mem.load(c+0xa8,8)|UINT64_C(0x800000000000000));
}
void Model::shed(Address c) {
  auto block = pointer_result(call("pool_new",{0x268u}));
  if (!block) return;
  auto definition = u32(c+0x24);
  call("GutChunk_ctorFromProp",{block,vec(c+0x10),static_cast<Raw>(s16(static_cast<Raw>(mem.load(c+0x38,2)))),u32(static_cast<Address>(definition)+0xc),static_cast<Raw>(c+0x114),u32(definition),u32(static_cast<Address>(definition)+8),u32(static_cast<Address>(definition)+0x10),ONE});
  write(static_cast<Address>(block)+0x1c4,4,add(u32(static_cast<Address>(block)+0x1c4),TWO));
}
void Model::mosquito_lose_part(Address c, std::uint32_t part) {
  Depth guard(depth_); creature_span(c);
  auto model = [&] { return u32(static_cast<Address>(u32(c+0x24))+0xc); };
  call("model_buildHideMask",{model(),u32(c+0xe8)});
  constexpr Raw bits[] = {0,0x10,0x10,0x40,2,1,4,8};
  if (part >= 1 && part <= 7) write(c+0xe8,4,u32(c+0xe8)|bits[part]);
  auto mask = call("model_buildHideMask",{model(),u32(c+0xe8)}); write(c+0x220,8,mask);
  shed(c);
}
void Model::scorpion_sever_part(Address c, std::uint32_t part) {
  Depth guard(depth_); creature_span(c);
  call("model_buildHideMask",{u32(static_cast<Address>(u32(c+0x24))+0xc),u32(c+0xe8)});
  auto bits = u32(c+0xe8)|(1u<<(part&31));
  auto model = u32(static_cast<Address>(u32(c+0x24))+0xc);
  write(c+0xe8,4,bits);
  auto mask = call("model_buildHideMask",{model,bits}); write(c+0x220,8,mask);
  shed(c);
}
std::uint32_t Model::str_prefix_differs(Address text, Address prefix) {
  Depth guard(depth_);
  if (text > UINT32_MAX || prefix > UINT32_MAX) throw Unsupported("prefix raw32 address");
  for (unsigned i=0;i<4096;++i) {
    auto want = signed8(mem.load(prefix+i,1));
    if (!want) return 0;
    auto have = signed8(mem.load(text+i,1));
    if (mem.load(static_cast<Raw>(0x5628e1+want),1)&1) want += 32;
    if (mem.load(static_cast<Raw>(0x5628e1+have),1)&1) have += 32;
    if (have != want) return 1;
  }
  throw Unsupported("prefix scan limit");
}
// Base constructor writes are part of each derived constructor's contract.
// GameObject, property lookup, balancing and statistics remain host services.
void Model::creature_ctor(Address c, Raw name, Vec3 pos, Raw facing, Raw def, Raw props, Raw hp) {
  call("GameObject_ctor",{static_cast<Raw>(c),pos,def,props});
  write(c+0x50,4,0); write(c+0x48,4,0x50cc58); write(c+0x4c,4,0x50d010); write(c+0x119,1,8);
  for (auto off:{0x2f4,0x2ec,0x2f0,0x234,0x238,0x2e4,0x360,0x364}) write(c+off,4,0);
  wvec(c+0x3b4,pos); write(c+0x38,2,facing); write(c+0x22c,4,hp); write(c+0x220,8,UINT64_MAX);
  write(c+0xc,4,u32(c+0xc)|0x20); write(c+0xc8,4,0); write(c+0x228,4,hp);
  write(c+0x23c,4,UINT32_MAX); write(c+0x240,4,UINT32_MAX);
  for (auto off:{0x2bc,0x2c0,0x2dc,0xf4,0x2c4,0x300}) write(c+off,4,0);
  for (auto off:{0x110,0x31c,0x312,0x316}) write(c+off,2,0);
  write(c+0x231,1,0); write(c+0x2fc,1,0); write(c+0x310,1,255); write(c+0x5a,1,255); write(c+0xa0,1,0);
  write(c+0x3c,4,u32(c+0x18)); write(c+0xdc,4,ONE); write(c+0x58,2,65535); write(c+0x3c4,2,600);
  for (auto off:{0xb8,0x304,0xe0,0x2d8}) write(c+off,4,0);
  for (auto off:{0x314,0x31a,0x318}) write(c+off,2,0);
  auto serial=mem.load(0x4f85c4,1); write(0x4f85c4,1,serial+1); write(c+0x230,1,serial);
  auto prop=[&](Raw key){return call("obj_getPropInt",{static_cast<Raw>(c),key,0u});};
  auto str=[&](Raw key){return call("obj_getPropStr",{static_cast<Raw>(c),key,0u});};
  if (prop(0x52f9f0)) write(c+0x231,1,mem.load(c+0x231,1)|1);
  if (prop(0x52f9f8)) { write(c+0x231,1,mem.load(c+0x231,1)|8); call("obj_moveOffGrid",{static_cast<Raw>(c)}); write(c+0xc,4,u32(c+0xc)|0x8000); }
  if (str(0x52fa00)) { write(c+0x368,4,pointer_result(str(0x52fa00))); write(c+0xc,4,u32(c+0xc)|0x100000); }
  if (prop(0x52fa10)) write(c+0xc8,4,0x447a0000);
  write(c+0x3c0,4,0x447a0000);
  if (prop(0x52fa18)) write(c+0x3c0,4,from_int(static_cast<Raw>(prop(0x52fa18))));
  constexpr Raw keys[]={0x52fa28,0x52fa38,0x52fa48}, offsets[]={0x3ac,0x3ae,0x3b0}, defaults[]={300,500,0x4000};
  for (unsigned i=0;i<3;++i) { write(c+offsets[i],2,defaults[i]); if (prop(keys[i])) write(c+offsets[i],2,prop(keys[i])); }
  if (prop(0x52fa58)) write(c+0xdc,4,mul(from_int(static_cast<Raw>(prop(0x52fa58))),0x3c23d70a));
  if (prop(0x52fa60)) write(c+0xa8,8,mem.load(c+0xa8,8)|1);
  if (prop(0x52fa70)) write(c+0xa8,8,mem.load(c+0xa8,8)|UINT64_C(0x1000000000000));
  if (str(0x52fa80)||str(0x52fa88)||str(0x52fa90)) write(c+0xa8,8,mem.load(c+0xa8,8)|0x40000000);
  if (name) {
    write(c+0x34,4,name); record_call({"balance_lookup",{name}});
    Balance factors{};
    if (balance_lookup) { auto fn=balance_lookup; factors=fn(*this,name); }
    else if (policy==CallbackPolicy::RequireHandlers) throw creature_hp::MissingHandler("missing monster callback: balance_lookup");
    if (!factors[0]) factors={0,ONE,ONE,ONE,ONE,ONE};
    call("Creature_setStats",{static_cast<Raw>(c),1u,factors[1],factors[2],factors[3],factors[4],factors[5]});
  }
  write(c+0x2fa,2,mem.load(c+0x38,2));
}
void Model::event_params(Address c, Raw entry, std::array<Raw,4> masks, std::array<Raw,4> times) {
  for (unsigned k=0;k<4;++k) {
    auto record=u32(static_cast<Address>(u32(c+0x24))+8*entry+4);
    write(static_cast<Address>(record)+0x18+8*k,1,masks[k]);
    write(static_cast<Address>(record)+0x14+8*k,4,times[k]);
  }
}
void Model::smallspider_ctor(Address c, Raw name, Vec3 pos, Raw facing, Raw props) {
  Depth guard(depth_); creature_span(c);
  creature_ctor(c,name,pos,static_cast<Raw>(s16(facing)),0x502818,props,ONE);
  write(c+0xb8,4,0x42480000); write(c+0x1c,4,0x41200000); write(c+0x48,4,0x50fd40); write(c+0x38,2,facing);
  write(c+0xb0,4,0x3f333333); write(c+0x232,2,0); write(c+0xb4,1,0x12);
  write(c+0xa8,8,mem.load(c+0xa8,8)|UINT64_C(0x8004001000)); write(c+0xec,4,u32(0x44fdcc));
  write(c+0x1c,4,0x40a00000); write(c+0xdc,4,0x3e4ccccc); write(c+0x64,4,ONE);
  call("anim_push",{static_cast<Raw>(c+0x114),u32(static_cast<Address>(u32(c+0x24))+0x3c),1u});
  call("anim_randomizeTopTime",{static_cast<Raw>(c+0x114)}); write(c+0x232,2,0);
  call("obj_runEvery60",{static_cast<Raw>(c)}); call("Creature_init",{static_cast<Raw>(c)});
}
void Model::gnaw_ctor(Address c, Raw name, Vec3 pos, Raw facing, Raw props) {
  Depth guard(depth_); creature_span(c);
  creature_ctor(c,name,pos,static_cast<Raw>(s16(facing)),0x5000a0,props,0x41200000);
  auto frame=u32(0x44fdcc); write(c+0xb4,1,1); write(c+0xb0,4,0x3f999999); write(c+0xec,4,frame);
  write(c+0xa8,8,mem.load(c+0xa8,8)|UINT64_C(0x200001000)); write(c+0x48,4,0x515038);
  write(c+0x1c,4,0x40c00000); write(c+0xf4,4,frame+300); write(c+0xb8,4,0); write(c+0x232,2,0);
  auto unit=f2d(mul(from_int(random(c)),0x37800000));
  write(c+0xdc,4,d2f(dadd(dmul(unit,mem.load(0x545ea0,8)),mem.load(0x545ea8,8))));
  call("anim_clear",{static_cast<Raw>(c+0x114)}); gnaw_set_state(c,0); call("obj_runEveryFrame",{static_cast<Raw>(c)});
  for (Raw e:{3u,4u,5u}) event_params(c,e,{1,0,0,0},{0x3e4ccccc,0,0,0});
  call("Creature_init",{static_cast<Raw>(c)});
}
void Model::mosquito_ctor(Address c, Vec3 pos, Raw facing, Raw props, Raw name) {
  Depth guard(depth_); creature_span(c);
  creature_ctor(c,name,pos,static_cast<Raw>(s16(facing)),0x5017a0,props,0x41200000);
  auto frame=u32(0x44fdcc); write(c+0x28,1,0x1e); write(c+0xb4,1,0x12); write(c+0x48,4,0x516a08);
  write(c+0xb0,4,0x3f999999); write(c+0x1c,4,0x41200000); write(c+0xec,4,frame); write(c+0xf4,4,frame+300);
  write(c+0xa8,8,mem.load(c+0xa8,8)|UINT64_C(0x8000001000)); write(c+0x5c,4,0xbe4ccccc); write(c+0xb8,4,0);
  write(c+0x232,2,0); write(c+0x64,4,ONE); write(c+0x68,4,ONE);
  if (str_prefix_differs(name,0x549af0)) write(c+0xdc,4,0x3f333333);
  call("anim_clear",{static_cast<Raw>(c+0x114)}); mosquito_set_state(c,0); call("obj_runEvery60",{static_cast<Raw>(c)});
  for (Raw e:{3u,4u}) event_params(c,e,{1,0,0,0},{0x3e4ccccc,0,0,0});
  for (Raw e:{5u,6u,7u,8u}) event_params(c,e,{0x80,0,0,0},{e==5?ONE:e==8?HALF:0x3fc00000u,0,0,0});
  call("Creature_init",{static_cast<Raw>(c)});
}
void Model::scorpion_ctor(Address c, Raw name, Vec3 pos, Raw facing, Raw props) {
  Depth guard(depth_); if (c>UINT32_MAX-0x3c8u) throw Unsupported("scorpion raw32 span");
  creature_ctor(c,name,pos,static_cast<Raw>(s16(facing)),0x502238,props,0x41200000);
  write(c+0x1c,4,0x41f00000); write(c+0x48,4,0x50fe88); write(c+0xb0,4,0x3f999999); write(c+0x28,1,0x46);
  write(c+0xb8,4,0); write(c+0x232,2,0); write(c+0xb4,1,0x12); write(c+0xec,4,u32(0x44fdcc)); write(c+0x64,4,0x3ecccccc);
  write(c+0xa8,8,mem.load(c+0xa8,8)|UINT64_C(0x8000001000)); write(c+0xe8,4,0);
  auto step=mul(from_int(random(c)%6),0x3d4ccccc); auto scale=d2f(dadd(f2d(step),UINT64_C(0x3fe8000000000000)));
  write(c+0xdc,4,scale); write(c+0x1c,4,mul(u32(c+0x1c),scale)); write(c+0x3c8,1,random(c)&1);
  auto mask=call("model_buildHideMask",{u32(static_cast<Address>(u32(c+0x24))+0xc),u32(c+0xe8)}); write(c+0x220,8,mask);
  call("anim_clear",{static_cast<Raw>(c+0x114)}); call("anim_push",{static_cast<Raw>(c+0x114),u32(static_cast<Address>(u32(c+0x24))+0x84),1u});
  call("obj_runEvery60",{static_cast<Raw>(c)});
  for (Raw e:{5u,6u,7u,8u}) event_params(c,e,{1,0,0,0},{0x3e999999,0,0,0});
  for (Raw e:{11u,12u}) event_params(c,e,{0x80,0,0,0},{HALF,0,0,0});
  call("Creature_init",{static_cast<Raw>(c)});
}
void Model::badger_ctor(Address c, Raw name, Vec3 pos, Raw facing, Raw props, Raw variant) {
  Depth guard(depth_); if (c>UINT32_MAX-0x3ccu) throw Unsupported("badger constructor raw32 span");
  creature_ctor(c,name,pos,static_cast<Raw>(s16(facing)),0x4fecf8+0xa0*variant,props,0x41200000);
  write(c+0x48,4,0x510f30); write(c+0x1c,4,0x420c0000); write(c+0x28,1,0x28); write(c+0xb8,4,0); write(c+0xb0,4,ONE);
  write(c+0x232,2,0); write(c+0xb4,1,3); write(c+0xec,4,u32(0x44fdcc)); write(c+0xa8,8,mem.load(c+0xa8,8)|UINT64_C(0x200000000)); write(c+0x3cc,1,variant);
  if (signed8(variant)==1) { write(c+0x60,4,ONE); write(c+0x5c,4,0xbe999999); write(c+0xdc,4,0x3f4ccccc); write(c+0xe0,4,0xc1200000); write(c+0x1c,4,0x41c80000); }
  else { write(c+0x28,1,0x2d); write(c+0xe0,4,0xc1700000); }
  write(c+0xe8,4,0); auto mask=call("model_buildHideMask",{u32(static_cast<Address>(u32(c+0x24))+0xc),0u}); write(c+0x220,8,mask);
  call("anim_clear",{static_cast<Raw>(c+0x114)}); call("anim_push",{static_cast<Raw>(c+0x114),u32(static_cast<Address>(u32(c+0x24))+0x7c),1u});
  call("obj_runEvery60",{static_cast<Raw>(c)});
  for (Raw e:{4u,5u,6u,7u}) event_params(c,e,{1,2,0,0},{0x3e999999,0x3f4ccccc,0,0});
  event_params(c,10,{1,2,0,0},{HALF,0x3f666666,0,0});
  for (Raw e:{11u,12u}) event_params(c,e,{0x84,0,0,0},{ONE,0,0,0});
  call("Creature_init",{static_cast<Raw>(c)});
}
// Attack-resolution family. Inherited movement shares memory and forwards each
// observation before host callbacks, retaining exact prefixes on exceptions.
std::uint64_t Model::see(Address c, Raw arc, Raw far, Raw near,
                         std::optional<Raw> facing) {
  creature_span(c);
  const auto face = facing.value_or(static_cast<Raw>(mem.load(c+0x38,2)));
  const auto node=u32(c+0x234);
  if (!node) return 0;
  return call("creature_canSeeObj",{static_cast<Raw>(c),u32(static_cast<Address>(node)+12),
    static_cast<Raw>(s16(face)),static_cast<Raw>(s16(arc)),0u,near,far,0u});
}
void Model::dead_update(Address c, Raw ev, Raw flag, Raw size, Vec3 push) {
  creature_span(c);
  call("Creature_deadUpdate",{static_cast<Raw>(c),ev,push,flag,size});
}
Raw Model::badger_pick(Address c) {
  creature_span(c);
  const auto pick=random(c)%5;
  return pick==0 ? 8 : pick==1 ? 9 : 6;
}
void Model::turn_to_target(Address c) {
  if (skelight_domain_) {
    auto node=u32(c+0x234);
    if (node) dispatch(c,26,{0u,vec(static_cast<Address>(u32(static_cast<Address>(node)+12))+0x10)});
    return;
  }
  const auto node=u32(c+0x234);
  if (!node) return;
  auto table=u32(c+0x48);
  auto self=static_cast<Raw>(c+s16(static_cast<Raw>(mem.load(static_cast<Address>(table)+8*26,2))));
  const auto point=vec(static_cast<Address>(u32(static_cast<Address>(node)+12))+0x10);
  if (u32(static_cast<Address>(table)+8*26+4)==0x10bb08 && !external.count("Creature.v26")) {
    // The same bridge executes the independently ported turning body.
    creature_movement::Model inherited(mem,policy);
    try { inherited.turn_toward(self,0,point); }
    catch (...) {
      for (const auto &w:inherited.writes) {
        if (writes.size()>=history_limit || events.size()>=history_limit) throw Unsupported("monster history limit");
        writes.push_back(w);events.push_back({false,w,{}});
      }
      throw;
    }
    for (const auto &w:inherited.writes) {
      if (writes.size()>=history_limit || events.size()>=history_limit) throw Unsupported("monster history limit");
      writes.push_back(w);events.push_back({false,w,{}});
    }
  } else {
    // Known model bodies in a wrong slot refuse, just as the existing API does.
    const auto fn=u32(static_cast<Address>(table)+8*26+4);
    for (auto body:{0x2ab5d8u,0x2ab704u,0x2ab8e0u,0x267654u,0x267a0cu,0x297b30u,
                   0x297f54u,0x2a5068u,0x2a55ecu,0x244380u,0x244aa4u,0x244a9cu,
                   0x27236cu,0x272f4cu,0x2a8b84u,0x2a91b8u,0x2c2440u,0x2c382cu,0x2c34e4u,0x2c3c54u,
                   0x3c2738u,0x10e658u,0x10beb8u,0x3c2708u})
      if (fn==body && !external.count("Creature.v26")) throw Unsupported("mismatched modelled turn body");
    call("Creature.v26",{self,0u,point});
  }
}
Structured Model::movement_virtual(Address obj,unsigned slot,std::vector<Structured> args) {
  namespace V=creature_movement;
  if (skelight_domain_) {
    std::vector<Argument> values;
    for (const auto &a:args) {
      if (a.kind==V::ValueKind::Integer) values.emplace_back(static_cast<Raw>(a.scalar));
      else if (a.kind==V::ValueKind::Vector) values.emplace_back(a.vec);
      else values.emplace_back(a);
    }
    auto r=dispatch(obj,slot,std::move(values));
    return r ? Structured::integer(static_cast<std::int64_t>(*r)) : Structured{};
  }
  const auto table=u32(obj+0x48),fn=u32(static_cast<Address>(table)+8*slot+4);
  const auto self=static_cast<Raw>(obj+s16(static_cast<Raw>(mem.load(static_cast<Address>(table)+8*slot,2))));
  if (fn==0x2ab8e0 && slot==6 && !external.count("Creature.v6")) { smallspider_on_touch(self,static_cast<Raw>(args.at(0).scalar));return {}; }
  if (!external.count("Creature.v"+std::to_string(slot)) &&
      ((slot==6 && fn==0x11a418) || (slot==10 && fn==0x11a3f0) ||
       (slot==11 && (fn==0x11a448 || fn==0x244a9c)))) {
    if (slot==6 && (u32(static_cast<Address>(static_cast<Raw>(args.at(0).scalar))+12)&0x40))
      write(static_cast<Address>(self)+0xa8,8,mem.load(static_cast<Address>(self)+0xa8,8)|UINT64_C(0x800000000000000));
    return {};
  }
  for (auto body:{0x2ab5d8u,0x2ab704u,0x2ab8e0u,0x267654u,0x267a0cu,0x297b30u,
                 0x297f54u,0x2a5068u,0x2a55ecu,0x244380u,0x244aa4u,0x244a9cu,
                 0x27236cu,0x272f4cu,0x2a8b84u,0x2a91b8u,0x2c2440u,0x2c382cu,0x2c34e4u,0x2c3c54u,
                 0x3c2738u,0x10e658u,0x10beb8u,0x3c2708u})
    if (fn==body && !external.count("Creature.v"+std::to_string(slot))) throw Unsupported("unported or mismatched movement virtual body");
  std::vector<Argument> values{self};
  for (const auto &a:args) {
    if (a.kind==V::ValueKind::Integer) values.emplace_back(static_cast<Raw>(a.scalar));
    else if (a.kind==V::ValueKind::Vector) values.emplace_back(a.vec);
    else values.emplace_back(a);
  }
  call("Creature.v"+std::to_string(slot),std::move(values));return {};
}
void Model::movement(Address c,bool settle,Vec3 velocity) {
  namespace V=creature_movement;
  V::Model inherited(mem,policy);
  std::size_t cursor=0;
  auto flush=[&] {
    while (cursor<inherited.events.size()) {
      const auto &e=inherited.events[cursor++];
      if (e.kind==V::EventKind::Write) {
        if (writes.size()>=history_limit || events.size()>=history_limit) throw Unsupported("monster history limit");
        writes.push_back(e.write);events.push_back({false,e.write,{}});
      }
      // Calls are recorded by the forwarding handler, after the preceding writes.
    }
  };
  inherited.virtual_dispatch=[&](auto obj,auto slot,auto args) {
    flush();return movement_virtual(obj,slot,std::move(args));
  };
  for (const auto &name:{"water_disturb","water_test","libc_sprintf","res_find","sound_playOnObject",
                        "world_slideMove","obj_updateGridCell","world_collectObjectsInBox","terrain_heightBelow"}) {
    inherited.callbacks[name]=[&,name](auto &,const auto &args) {
      flush();std::vector<Argument> values;
      for (const auto &a:args) {
        if (a.kind==V::ValueKind::Integer) values.emplace_back(static_cast<Raw>(a.scalar));
        else if (a.kind==V::ValueKind::Vector) values.emplace_back(a.vec);
        else values.emplace_back(a);
      }
      auto it=structured_callbacks.find(name);
      if (it!=structured_callbacks.end()) {
        record_call({name,values});auto cb=it->second;return cb(*this,values);
      }
      // Structured services cannot be faked by an integer return.
      if (std::string(name)=="world_collectObjectsInBox" || std::string(name)=="libc_sprintf" ||
          (std::string(name)=="world_slideMove" && std::get<Raw>(values.at(3)))) {
        record_call({name,values});
        if (policy==CallbackPolicy::RequireHandlers) throw creature_hp::MissingHandler("missing structured monster callback: "+std::string(name));
        if (std::string(name)=="world_collectObjectsInBox") return V::Value::list({});
        if (std::string(name)=="libc_sprintf") return V::Value::buffer({});
        return V::Value::tuple({V::Value::integer(0),V::Value::vector({})});
      }
      return V::Value::integer(static_cast<std::int64_t>(call(name,std::move(values))));
    };
  }
  try { if (settle) inherited.settle(c);else inherited.move_by(c,velocity); }
  catch (...) { flush();throw; }
  flush();
}
void Model::scorpion_attack(Address c,Raw ev) {
  Depth guard(depth_);creature_span(c);
  bool count_down=true;
  if (u32(c+0x234)) {
    if (ev&1) {
      Raw flags=8;Argument descriptor=0u;
      if (s16(static_cast<Raw>(mem.load(c+0x232,2)))==7) {
        const auto level=u32(0x4f719c);
        if (level==2 || level==3) {
          const auto divisor=level==2 ? 4 : 8;
          for (auto off:{0xd0,0xd2}) {
            const auto value=s16(static_cast<Raw>(mem.load(c+off,2)));
            write(c+off,2,static_cast<Raw>(value/divisor));
          }
        }
        flags=0x10004;
        descriptor=Descriptor{{0x17,static_cast<Raw>(mem.load(c+0xd0,2)),static_cast<Raw>(mem.load(c+0xd2,2)),0}};
      }
      call("Creature_hitTargetAtReach",{static_cast<Raw>(c),static_cast<Raw>(s16(static_cast<Raw>(mem.load(c+0xd0,2)))),
        static_cast<Raw>(s16(static_cast<Raw>(mem.load(c+0xd2,2)))),static_cast<Raw>(s16(static_cast<Raw>(mem.load(c+0xd4,2)))),flags,descriptor,0x42100000u});
    }
    if (anim_flags(c,8)) { turn_to_target(c);count_down=!anim_flags(c,8); }
  } else count_down=!anim_flags(c,8);
  if (count_down) write(c+0xf4,4,u32(c+0xf4)-1);
  if (s32(u32(c+0xf4))>0) return;
  const auto draw=random(c);virtual_call(c,27,draw&1 ? 23 : draw&2 ? 24 : 3);
}
void Model::badger_attack(Address c,Raw ev) {
  Depth guard(depth_);
  if (c>UINT32_MAX-0x3cfu) throw Unsupported("badger raw32 span");
  movement(c,true);
  auto state=[&]{return s16(static_cast<Raw>(mem.load(c+0x232,2)));};
  if (ev&1) {
    Raw flags=8;const auto st=state();
    if (signed8(mem.load(c+0x3cc,1))!=1 && (st==6 || st==7 || st==10))
      flags=random(c)%3==0 ? 0x80008 : 0x40008;
    call("Creature_hitAllEnemiesInArc",{static_cast<Raw>(c),static_cast<Raw>(s16(static_cast<Raw>(mem.load(c+0x38,2)))),7000u,
      static_cast<Raw>(s16(static_cast<Raw>(mem.load(c+0xd0,2)))),static_cast<Raw>(s16(static_cast<Raw>(mem.load(c+0xd2,2)))),
      static_cast<Raw>(s16(static_cast<Raw>(mem.load(c+0xd4,2)))),flags,0u,0u,add(u32(c+0x1c),0x41c80000)});
    write(c+0x3c8,4,1);
  }
  if (u32(c+0x234)) {
    auto st=state();
    if (st==6 && !u32(c+0x3c8) && anim_flags(c,8)) { if (see(c,7000,0x41200000)) turn_to_target(c);st=state(); }
    if (st==10) { if (anim_flags(c,8) && see(c,7000,0x41200000)) turn_to_target(c);st=state(); }
    if (st==7 && anim_flags(c,8) && see(c,7000,0x41200000)) turn_to_target(c);
  }
  if ((ev&2) && state()!=6 && see(c,7000,0x42700000)) virtual_call(c,27,6);
  if (!anim_flags(c,8)) virtual_call(c,27,3);
}
// Badger pursuit family, including the inherited raw-memory navigation bodies.
// Path search, ray casts and free-spot selection remain explicit host services.
Raw Model::turn_to_point(Address c,Vec3 point) {
  if (full_domain_) return static_cast<Raw>(dispatch(c,26,{0u,point}).value_or(0));
  const auto table=u32(c+0x48);
  const auto fn=u32(static_cast<Address>(table)+8*26+4);
  const auto self=static_cast<Raw>(c+s16(static_cast<Raw>(mem.load(static_cast<Address>(table)+8*26,2))));
  if (fn==0x10bb08 && !external.count("Creature.v26")) {
    creature_movement::Model inherited(mem,policy);
    auto flush=[&] {
      for (const auto &w:inherited.writes) {
        if (writes.size()>=history_limit || events.size()>=history_limit) throw Unsupported("monster history limit");
        writes.push_back(w);events.push_back({false,w,{}});
      }
    };
    std::int16_t result;
    try { result=inherited.turn_toward(self,0,point); }
    catch (...) { flush();throw; }
    flush();return static_cast<Raw>(result);
  }
  // Retain the same known-body refusal policy as turn_to_target.
  for (auto body:{0x2ab5d8u,0x2ab704u,0x2ab8e0u,0x267654u,0x267a0cu,0x297b30u,
                 0x297f54u,0x2a5068u,0x2a55ecu,0x244380u,0x244aa4u,0x244a9cu,
                 0x27236cu,0x272f4cu,0x2a8b84u,0x2a91b8u,0x2c2440u,0x2c382cu,0x2c34e4u,0x2c3c54u,
                 0x3c2738u,0x10e658u,0x10beb8u,0x3c2708u})
    if (fn==body && !external.count("Creature.v26")) throw Unsupported("mismatched modelled turn body");
  return static_cast<Raw>(call("Creature.v26",{self,0u,point}));
}
Vec3 Model::waypoint(Address c,std::int64_t ahead) const {
  const auto index=static_cast<std::int64_t>(s32(u32(c+0x2bc)))+ahead;
  if (index>=16) return {MINUS_ONE,MINUS_ONE,MINUS_ONE};
  // Python's inherited accessor permits negative indexes into the supplied
  // memory image. Preserve that arithmetic within the raw32 memory domain.
  const auto address=static_cast<std::int64_t>(c)+0x23c+8*index;
  if (address<0 || address>UINT32_MAX-7) throw Unsupported("waypoint raw32 span");
  return {from_int(u32(static_cast<Address>(address))),from_int(u32(static_cast<Address>(address)+4)),MINUS_ONE};
}
Vec3 Model::advance_waypoint(Address c,Vec3 point,Vec3 dest) {
  const auto index=static_cast<std::int64_t>(s32(u32(c+0x2bc)));
  point[0]=point[1]=MINUS_ONE;
  if (index>=16) return point;
  const auto address=static_cast<std::int64_t>(c)+0x23c+8*index;
  if (address<0 || address>UINT32_MAX-15) throw Unsupported("waypoint advance raw32 span");
  if (s32(u32(static_cast<Address>(address)))!=-1) {
    write(c+0x2bc,4,static_cast<Raw>(index+1));
    if (index+1!=16) {
      const auto x=u32(static_cast<Address>(address)+8);
      if (s32(x)!=-1) return {from_int(x),from_int(u32(static_cast<Address>(address)+12)),point[2]};
    }
  }
  call("path_requestToPoint",{u32(0x44fdc8),vec(c+0x10),dest,static_cast<Raw>(c),static_cast<Raw>(c+0x23c),16u,u32(c+0x1c)});
  write(c+0x2bc,4,16);return point;
}
Raw Model::probe_rays(Address c,Vec3 direction) {
  const auto world=u32(0x44fdc8);Raw t=0;
  for (std::int32_t side:{0,0x4000,-0x4000}) {
    Raw x=side ? u32(c+0x10) : add(u32(c+0x10),0),y=u32(c+0x14);
    if (side) {
      const auto face=s16(static_cast<Raw>(mem.load(c+0x38,2)));
      const auto ox=mul(cos16(static_cast<Raw>(face+side)),u32(c+0x1c));
      const auto oy=mul(sin16(static_cast<Raw>(s16(static_cast<Raw>(mem.load(c+0x38,2)))+side)),u32(c+0x1c));
      x=add(u32(c+0x10),ox);y=add(u32(c+0x14),oy);
    }
    const Vec3 origin{x,y,add(u32(c+0x18),0x41300000)};
    t=static_cast<Raw>(call("world_raycast",{world,origin,direction,static_cast<Raw>(c),0u,3u,1u,0x3f8cccccu}));
    if (!lt(t,0) && !le(ONE,t)) return t;
  }
  return lt(t,0) ? ONE : t;
}
Vec3 Model::avoidance_target(Address c,Vec3 dest) {
  const auto pos=vec(c+0x10);
  const Vec3 d{sub(dest[0],pos[0]),sub(dest[1],pos[1]),sub(dest[2],pos[2])};
  const auto t=probe_rays(c,d);
  const Vec3 straight{add(pos[0],mul(d[0],0x44960000)),add(pos[1],mul(d[1],0x44960000)),add(pos[2],mul(d[2],0x44960000))};
  if (le(ONE,t)) return straight;
  const auto two_r=add(u32(c+0x1c),u32(c+0x1c));
  const auto near=mul(t,add(add(mul(d[0],d[0]),mul(d[1],d[1])),mul(d[2],d[2])));
  if (!lt(near,mul(two_r,two_r))) return straight;
  const auto world=u32(0x44fdc8);
  const Vec3 origin{add(u32(c+0x10),0),u32(c+0x14),add(u32(c+0x18),0x41400000)};
  auto side=[&](std::int32_t delta) {
    const auto x=mul(cos16(static_cast<Raw>(s16(static_cast<Raw>(mem.load(c+0x38,2)))+delta)),two_r);
    const auto y=mul(sin16(static_cast<Raw>(s16(static_cast<Raw>(mem.load(c+0x38,2)))+delta)),two_r);
    return Vec3{x,y,0};
  };
  const auto left=side(0x4000);
  const auto tl=static_cast<Raw>(call("world_raycast",{world,origin,left,static_cast<Raw>(c),0u,3u,1u,0x3f8cccccu}));
  std::int32_t turn;
  if (!le(0,tl) || !lt(tl,ONE)) turn=0x2000;
  else {
    const auto right=side(-0x4000);
    const auto tr=static_cast<Raw>(call("world_raycast",{world,origin,right,static_cast<Raw>(c),0u,3u,1u,ONE}));
    if (!le(0,tr) || !lt(tr,ONE)) turn=-0x2000;
    else if (lt(mul(tl,tl),near) && lt(near,mul(tr,tr))) turn=0;
    else turn=lt(tr,tl) ? 0x2000 : -0x2000;
  }
  const auto cs=cos16(static_cast<Raw>(s16(static_cast<Raw>(mem.load(c+0x38,2)))+turn));
  const auto sn=sin16(static_cast<Raw>(s16(static_cast<Raw>(mem.load(c+0x38,2)))+turn));
  return {add(u32(c+0x10),mul(cs,0x44960000)),add(u32(c+0x14),mul(sn,0x44960000)),u32(c+0x18)};
}
Vec3 Model::steer_without_path(Address c,Vec3 dest,Vec3 point) {
  const auto frame=u32(0x44fdcc),serial=static_cast<Raw>(signed8(mem.load(c+0x230,1)));
  if (!((frame^serial)&63)) {
    const Vec3 ray{sub(dest[0],u32(c+0x10)),sub(dest[1],u32(c+0x14)),sub(dest[2],u32(c+0x18))};
    if (lt(probe_rays(c,ray),ONE)) advance_waypoint(c,point,dest);
  }
  const auto off=atan2_16(sub(u32(c+0x2f0),u32(c+0x14)),sub(u32(c+0x2ec),u32(c+0x10)))-s16(static_cast<Raw>(mem.load(c+0x38,2)));
  if (off>-1000 && off<1000) {
    if (!((u32(0x44fdcc)^static_cast<Raw>(signed8(mem.load(c+0x230,1))))&7)) wvec(c+0x2ec,avoidance_target(c,dest));
    else wvec(c+0x2ec,dest);
  } else {
    const auto sx=sub(u32(c+0x10),u32(c+0x2ec)),sy=sub(u32(c+0x14),u32(c+0x2f0));
    const auto ex=sub(u32(c+0x10),dest[0]),ey=sub(u32(c+0x14),dest[1]);
    if (lt(add(mul(sx,sx),mul(sy,sy)),add(mul(ex,ex),mul(ey,ey)))) wvec(c+0x2ec,dest);
  }
  return vec(c+0x2ec);
}
Vec3 Model::steer_along_path(Address c,Vec3 dest,Vec3 point) {
  const Vec3 ray{sub(dest[0],u32(c+0x10)),sub(dest[1],u32(c+0x14)),sub(dest[2],u32(c+0x18))};
  if (le(ONE,probe_rays(c,ray))) { clear_path(c);wvec(c+0x2ec,dest);return vec(c+0x2ec); }
  for (unsigned i=0;i<16;++i) {
    const auto next=waypoint(c,1);
    const std::array<Raw,2> to_next{sub(next[0],point[0]),sub(next[1],point[1])};
    const std::array<Raw,2> to_me{sub(u32(c+0x10),point[0]),sub(u32(c+0x14),point[1])};
    if (!eq(point[0],MINUS_ONE)) {
      const auto wx=sub(point[0],u32(c+0x10)),wy=sub(point[1],u32(c+0x14));
      if (!lt(add(mul(wx,wx),mul(wy,wy)),0x44100000) && !(s32(u32(c+0x2c0))<s32(u32(0x44fdcc)))) {
        if (eq(next[0],MINUS_ONE)) break;
        if (!lt(0,add(mul(to_next[0],to_me[0]),mul(to_next[1],to_me[1])))) break;
      }
    }
    write(c+0x2c0,4,u32(0x44fdcc)+30);
    point=advance_waypoint(c,point,dest);
    if (eq(point[0],MINUS_ONE)) break;
  }
  return eq(point[0],MINUS_ONE) ? dest : point;
}
void Model::turn_and_step(Address c,Vec3 aim) {
  const auto off=s16(turn_to_point(c,aim));
  auto speed=u32(c+0xb8);
  if (s16(static_cast<Raw>(mem.load(c+0x31a,2)))>0) {
    const auto info=static_cast<Raw>(call("SpellDrowsy_typeinfo",{}));
    const auto spell=call("creature_findSpellByType",{static_cast<Raw>(c),info});
    if (spell) speed=mul(speed,static_cast<Raw>(call("SpellDrowsy_getStat3Fraction",{static_cast<Raw>(spell)})));
  }
  if (off<=-0x3e81 || off>=0x3e81) {
    write(c+0xbc,4,mul(u32(c+0xbc),HALF));write(c+0xc0,4,mul(u32(c+0xc0),HALF));
    speed=mul(u32(c+0xb8),HALF);
  }
  const auto slow=mem.load(c+0x2fc,1);
  if (slow>=1 && slow<=60) speed=mul(speed,mul(from_int(static_cast<Raw>(60-signed8(mem.load(c+0x2fc,1)))),0x3c888888));
  const auto vx=mul(cos16(static_cast<Raw>(s16(static_cast<Raw>(mem.load(c+0x38,2))))),speed);
  const auto vy=mul(sin16(static_cast<Raw>(s16(static_cast<Raw>(mem.load(c+0x38,2))))),speed);
  movement(c,false,{vx,vy,0});
}
Raw Model::move_to(Address c,Vec3 dest) {
  if (eq(u32(c+0x2ec),0) && eq(u32(c+0x2f0),0)) wvec(c+0x2ec,dest);
  const auto dx=sub(dest[0],u32(c+0x10)),dy=sub(dest[1],u32(c+0x14)),dz=sub(dest[2],u32(c+0x18));
  const auto r2=mul(u32(c+0x1c),u32(c+0x1c));
  const auto d2=add(add(mul(dx,dx),mul(dy,dy)),mul(dz,dz));
  if (lt(d2,r2)) return 1;
  const auto frame=u32(0x44fdcc),serial=static_cast<Raw>(signed8(mem.load(c+0x230,1)));
  if (!((frame^serial)&7) && lt(d2,add(r2,r2))) {
    const auto off=atan2_16(dy,dx)-s16(static_cast<Raw>(mem.load(c+0x38,2)));
    if (off<=-0x4001 || off>=0x4001) return 1;
  }
  Vec3 aim=dest;
  if (s32(u32(c+0x300))<s32(u32(c+0x2d8))) aim=vec(c+0x2ec);
  else {
    const auto point=waypoint(c);
    aim=eq(point[0],MINUS_ONE) ? steer_without_path(c,dest,point) : steer_along_path(c,dest,point);
  }
  turn_and_step(c,aim);
  const auto px=sub(u32(c+0x10),u32(c+0x2c8)),py=sub(u32(c+0x14),u32(c+0x2cc));
  if (signed8(mem.load(0x44fcd0,1)) || lt(0x41800000,add(mul(px,px),mul(py,py)))) {
    wvec(c+0x2c8,vec(c+0x10));write(c+0x2d4,4,u32(c+0x300));return 0;
  }
  const auto until=s32(u32(c+0x2d8)),clock=s32(u32(c+0x300));
  if (until<clock && lt(from_int(u32(c+0x2d4)),sub(from_int(static_cast<Raw>(clock)),0x41f00000))) {
    write(c+0x2d8,4,static_cast<Raw>(clock)+30);
    auto angle=s16(static_cast<Raw>(mem.load(c+0x38,2))-0x8000);
    angle=s16(static_cast<Raw>(angle)+(random(c)&1 ? 0x2000 : -0x2000));
    write(c+0x2ec,4,add(u32(c+0x10),mul(cos16(static_cast<Raw>(angle)),0x44960000)));
    write(c+0x2f0,4,add(u32(c+0x14),mul(sin16(static_cast<Raw>(angle)),0x44960000)));return 0;
  }
  if (until==clock && s32(u32(c+0x2d4))<static_cast<std::int64_t>(until)-300) {
    const std::vector<Argument> args{vec(c+0x10),0x48u,static_cast<Raw>(to_int(u32(c+0x1c))),0u};
    record_call({"world_findFreeSpot",args});
    Structured result;
    auto it=structured_callbacks.find("world_findFreeSpot");
    if (it!=structured_callbacks.end()) { auto cb=it->second;result=cb(*this,args); }
    else if (policy==CallbackPolicy::RequireHandlers) throw creature_hp::MissingHandler("missing structured monster callback: world_findFreeSpot");
    else result=Structured::tuple({Structured::vector({}),Structured::integer(0)});
    if (result.kind!=creature_movement::ValueKind::Tuple || result.items.size()!=2 ||
        result.items[0].kind!=creature_movement::ValueKind::Vector || result.items[1].kind!=creature_movement::ValueKind::Integer)
      throw Unsupported("free-spot result shape");
    if (signed8(static_cast<std::uint64_t>(result.items[1].scalar))) {
      wvec(c+0x10,result.items[0].vec);call("obj_updateGridCell",{static_cast<Raw>(c)});
    }
  }
  return 0;
}
void Model::move_to_target(Address c) {
  const auto node=u32(c+0x234);
  if (!node) { virtual_call(c,27,0);return; }
  const auto target=vec(static_cast<Address>(u32(static_cast<Address>(node)+12))+0x10);
  const auto bearing=atan2_16(sub(u32(c+0x14),target[1]),sub(u32(c+0x10),target[0]));
  auto radius=[&] {
    const auto link=u32(c+0x234);
    const auto obj=link ? u32(static_cast<Address>(link)+12) : 0;
    return add(u32(static_cast<Address>(obj)+0x1c),0x40a00000);
  };
  const auto ox=mul(cos16(static_cast<Raw>(bearing)),radius());
  const auto oy=mul(sin16(static_cast<Raw>(bearing)),radius());
  move_to(c,{add(target[0],ox),add(target[1],oy),target[2]});
}
std::optional<std::uint64_t> Model::badger_chase(Address c) {
  Depth guard(depth_);if (c>UINT32_MAX-0x3cfu) throw Unsupported("badger raw32 span");
  auto state=[&](Raw s) { return virtual_call(c,27,s); };
  if (!u32(c+0x234)) { return state(0); }
  auto face=[&](std::int32_t delta) {return static_cast<Raw>(s16(static_cast<Raw>(mem.load(c+0x38,2))+static_cast<Raw>(delta)));};
  auto cone=[&](std::int32_t delta,Raw arc) {return call("creature_findEnemyInCone",{static_cast<Raw>(c),face(delta),arc,0u,0x41c80000u});};
  if (see(c,7000,0x42700000)) { return state(badger_pick(c)); }
  if (see(c,7000,0x42c80000,0x41c80000)) { return state(10); }
  if (see(c,3000,0x42700000,0,face(8000))) { return state(8); }
  if (see(c,3000,0x42700000,0,face(-8000))) { return state(9); }
  if (cone(0,7000)) { return state(badger_pick(c)); }
  if (cone(8000,3000)) { return state(8); }
  if (cone(-8000,3000)) { return state(9); }
  if (see(c,0x7fff,0x41a00000,0,0)) turn_to_target(c);
  else move_to_target(c);
  return {};
}
// Inherited lifecycle of the ordinary classes. All constants are from the pin.
std::int32_t Model::i16(Address a) const { return s16(static_cast<Raw>(mem.load(a,2))); }
std::int32_t Model::i8(Address a) const { return signed8(mem.load(a,1)); }
std::uint64_t Model::flags(Address c) const { return mem.load(c+0xa8,8); }
Raw Model::handle(Address a) const { auto node=u32(a);return node?u32(static_cast<Address>(node)+12):0; }
Raw Model::player(Raw slot) const { return u32(static_cast<Raw>(0x4d8af0u+static_cast<Raw>(s32(slot))*0x4c30u+0x43d8u)); }
Structured Model::service(const std::string &name,std::vector<Argument> args) {
  auto r=call_value(name,std::move(args));
  if (auto v=std::get_if<Structured>(&r)) return *v;
  if (auto v=std::get_if<std::uint64_t>(&r)) {
    if (*v>INT64_MAX) throw Unsupported("structured integer outside signed64");
    return Structured::integer(static_cast<std::int64_t>(*v));
  }
  if (auto v=std::get_if<BufferResult>(&r)) return v->payload;
  throw Unsupported("unexpected structured compound result");
}
Raw Model::shared_draw() { auto seed=u32(0x404464)*0x19660du+0x3c6ef35fu;write(0x404464,4,seed);++draws;return seed>>16; }
Structured Model::table_service(const std::string &cls,Address obj,Raw off,unsigned slot,std::vector<Argument> args,bool skill_dispatch) {
  auto table=u32(obj+off);auto self=static_cast<Raw>(obj+i16(static_cast<Address>(table)+8*slot));
  auto fn=u32(static_cast<Address>(table)+8*slot+4);
  if (cls=="Spell" && !external.count("Spell.v"+std::to_string(slot)) && (fn==0x2e177c || fn==0x30cbc8)) {
    spell_run(self,fn);return {};
  }
  // Creature burning uses table_call: Skill entries remain callbacks even
  // when their address names an arena body. Tagar uses skill_call instead.
  if (skill_dispatch && cls=="Skill" && !external.count("Skill.v"+std::to_string(slot)) &&
      (fn==0x2e2368 || fn==0x2e18a4 || fn==0x30c708)) {
    if (!arena_cast_domain_) throw Unsupported("arena skill body outside ordinary run domain");
    auto word=[&](unsigned k) {return std::get<Raw>(args.at(k));};
    Raw result;
    if (fn==0x2e2368) result=summon_can_use(self,word(0));
    else if (fn==0x2e18a4) result=summon_cast(self,word(0),word(1),std::get<Vec3>(args.at(2)),word(3));
    else result=tagar_cast(self,word(0),word(1),std::get<Vec3>(args.at(2)),word(3));
    return Structured::integer(result);
  }
  args.insert(args.begin(),self);return service(cls+".v"+std::to_string(slot),std::move(args));
}
std::optional<std::uint64_t> Model::dispatch(Address c,unsigned slot,std::vector<Argument> args) {
  Depth guard(depth_);if (full_domain_) dispatched_result_.reset();auto table=u32(c+0x48);auto fn=u32(static_cast<Address>(table)+8*slot+4);
  auto self=static_cast<Raw>(c+i16(static_cast<Address>(table)+8*slot));
  auto name="Creature.v"+std::to_string(slot);
  auto arg=[&](unsigned k=0) {return std::get<Raw>(args.at(k));};
  if (!external.count(name)) {
    // Derived lookup is by body, as in both Python dispatch layers.
    switch(fn) {
    case 0x2ab5d8:return smallspider_run(self);
    case 0x267654:return gnaw_run(self);
    case 0x297b30:return mosquito_run(self);
    case 0x2a5068:return scorpion_run(self);
    case 0x244380:return badger_run(self);
    case 0x2ab704:smallspider_set_state(self,arg());return {};
    case 0x267a0c:gnaw_set_state(self,arg());return {};
    case 0x297f54:mosquito_set_state(self,arg());return {};
    case 0x2a55ec:scorpion_set_state(self,arg());return {};
    case 0x244aa4:badger_set_state(self,arg());return {};
    case 0x2ab8e0:smallspider_on_touch(self,arg());return {};
    case 0x244a9c:return {};
    case 0x3c2738:return u32(static_cast<Address>(u32(self+0x24))+12);
    case 0x3c2708:write(self+0x3c0,4,from_int(arg()&0xffff));return {};
    case 0x10beb8:on_death(self,arg());return {};
    case 0x10e658: {
      auto mode=arg() || mem.load(self+0x3b0,2)==0x7fff?1u:0u;
      auto node=u32(self+0x234);
      auto seen=node?call("creature_canSeeObj",{self,u32(static_cast<Address>(node)+12),0u,0x7fffu,mode,0u,from_int(static_cast<Raw>(mem.load(self+0x3ac,2))),0u}):0;
      if (seen) return 1;
      auto far=from_int(static_cast<Raw>(mem.load(self+0x3ae,2)));node=u32(self+0x234);
      if (!node) return 0;
      return call("creature_canSeeObj",{self,u32(static_cast<Address>(node)+12),static_cast<Raw>(i16(self+0x38)),static_cast<Raw>(i16(self+0x3b0)),mode,0u,far,0u})?1:0;
    }
    case 0x2a8b84:if (skelight_domain_) return skelight_run(self);throw Unsupported("Skelight body outside selected domain");
    case 0x2a91b8:if (skelight_domain_) {skelight_set_state(self,arg());return {};}throw Unsupported("Skelight body outside selected domain");
    case 0x27236c:if (horsemen_domain_) return horsemen_run(self);throw Unsupported("Horsemen body outside selected domain");
    case 0x272f4c:if (horsemen_domain_) {horsemen_set_state(self,arg());return {};}throw Unsupported("Horsemen body outside selected domain");
    case 0x2c2440:if (zek_domain_) return zek_run(self);throw Unsupported("Zek body outside selected domain");
    case 0x2c382c:if (zek_domain_) {zek_set_state(self,arg());return {};}throw Unsupported("Zek body outside selected domain");
    case 0x2c34e4:if (zek_domain_) {zek_take_damage(self,arg(0),arg(1),arg(2));return {};}throw Unsupported("Zek body outside selected domain");
    case 0x2c3c54:if (zek_domain_) return static_cast<std::uint64_t>(static_cast<std::int64_t>(zek_slot35(self)));throw Unsupported("Zek body outside selected domain");
    }
    if (full_domain_ && slot==7 && fn==0x11a260) {
      write(self+0x2bc,4,0);
      if (arg()) write(self+0x2c4,4,0);
      else {auto failures=u32(self+0x2c4);write(self+0x240,4,UINT32_MAX);write(self+0x23c,4,UINT32_MAX);write(self+0x2c4,4,failures+1);}
      return {};
    }
    if (full_domain_ && slot==30 && fn==0x118a58) {
      if (call("creature_bossRank",{self}) || i16(self+0x232)==20) return {};
      write(self+0x31c,2,arg());dispatch(self,27,{19u});
      if (arg(1)) for (auto pair:std::array<std::pair<unsigned,Raw>,7>{{{0x90,0x3a},{0x96,0x48},{0x92,0x9b},{0x94,0x9a},{0x98,0x3a},{0x9a,0x9b},{0x9c,0x9a}}}) write(self+pair.first,2,pair.second);
      return {};
    }
    if (full_domain_ && slot==31 && fn==0x118af8) {
      if (u32(0x4f711c) || call("creature_bossRank",{self})) return {};
      write(self+0x316,2,arg());call("handle_clear",{self+0x234u});write(self+0x234,4,0);return {};
    }
    if (zek_domain_ && slot==2 && fn==0x116638) {take_damage(self,arg(0),arg(1),arg(2));return {};}
    if (!full_domain_ && skelight_domain_ && ((slot==2 && fn==0x116638) ||
        (slot==7 && fn==0x11a260) || (slot==30 && fn==0x118a58) ||
        (slot==31 && fn==0x118af8)))
      throw Unsupported("base virtual body outside selected Skelight lifecycle family");
    if (slot==17 && fn==0x11a868) {
      auto node=pointer_result(call("iter_begin",{self+0x364,0u}));
      for (unsigned n=0;node;++n) {
        if (n>=4096) throw Unsupported("spell iteration budget");
        table_service("Spell",u32(static_cast<Address>(node)+12),0x14,2,{std::get<Vec3>(args.at(0)),static_cast<Raw>(i16(self+0x38))});
        node=pointer_result(call("iter_next",{}));
      }
      call("iter_end",{2u});return {};
    }
    if (slot==20 && fn==0x119328) return is_enemy(self,arg());
    if (slot==23 && fn==0x10ded0) {pick_target(self,arg());return {};}
    if (slot==37 && fn==0x3c2730) return static_cast<std::uint64_t>(static_cast<std::int64_t>(i8(self+0x310)));
    if (slot==27 && fn==0x3c2688) {write(self+0x232,2,arg());return {};}
    if (slot==26 && fn==0x10bb08) {
      creature_movement::Model inherited(mem,policy);
      auto flush=[&] {for (const auto &w:inherited.writes) {
        if (writes.size()>=history_limit || events.size()>=history_limit) throw Unsupported("monster history limit");
        writes.push_back(w);events.push_back({false,w,{}});
      }};
      std::int16_t result;
      try {result=inherited.turn_toward(self,arg(),std::get<Vec3>(args.at(1)));}
      catch (...) {flush();throw;}
      flush();return static_cast<std::uint64_t>(static_cast<std::int64_t>(result));
    }
    if ((slot==10 && fn==0x11a3f0) || (slot==11 && fn==0x11a448)) return {};
    if (slot==6 && fn==0x11a418) {if (u32(static_cast<Address>(arg())+12)&0x40) write(self+0xa8,8,flags(self)|UINT64_C(0x800000000000000));return {};}
    if (slot==35 && fn==0x118c50) return static_cast<std::uint64_t>(static_cast<std::int64_t>(to_int(mul(u32(0x4f714c),0x3e999999))));
    if (slot==36 && fn==0x118c78) {auto rank=s32(static_cast<Raw>(call("creature_bossRank",{self})));return rank==1?4:rank==2?10:0;}
  }
  args.insert(args.begin(),self);
  if (full_domain_) {
    auto value=call_value(name,std::move(args));dispatched_result_=value;
    if (auto integer=std::get_if<std::uint64_t>(&value)) return *integer;
    if (auto structured=std::get_if<Structured>(&value)) {
      if (structured->kind==creature_movement::ValueKind::Integer) return static_cast<std::uint64_t>(structured->scalar);
      if (structured->kind==creature_movement::ValueKind::None) return 0;
    }
    // The raw return carrier is only used to forward a run's final state call.
    // Its complete value is retained separately; integer consumers stay bounded.
    if (slot==27) return 1;
    throw Unsupported("noninteger callback used by integer-only virtual consumer");
  }
  return call(name,std::move(args));
}
bool Model::away(Address c) const {
  auto cf=flags(c);auto dx=sub(u32(c+0x10),u32(c+0x3b4)),dy=sub(u32(c+0x14),u32(c+0x3b8));
  if (cf&0x8000000) return lt(0x45100000,add(mul(dx,dx),mul(dy,dy)));
  auto dz=sub(u32(c+0x18),u32(c+0x3bc)),d2=add(add(mul(dx,dx),mul(dy,dy)),mul(dz,dz));
  if (cf&0xc00000) return lt(0x46610000,d2);
  auto limit=mul(u32(c+0x3c0),0x3f2a7ef9);return lt(mul(limit,limit),d2);
}
bool Model::near_view(Address a,Raw radius) const {
  auto px=u32(a),py=u32(a+4);
  auto inside=[&](Address p) {auto x=u32(p),y=u32(p+4);return le(px,add(x,radius)) && le(sub(x,radius),px) && le(py,add(y,radius)) && le(sub(y,radius),py);};
  if (i8(0x4039c1)) return inside(0x44f048);
  auto count=s32(u32(0x4d8ae0));std::int32_t slot=0;
  for (;slot<count;++slot) {auto p=player(static_cast<Raw>(slot));if (p && inside(static_cast<Address>(p)+0x10)) break;}
  return slot!=s32(u32(0x4d8ae0));
}
bool Model::update_active(Address c) {
  auto cf=flags(c),was=cf>>63;
  write(c+0xa8,8,was?cf|UINT64_C(0x4000000000000000):cf&UINT64_C(0xbfffffffffffffff));
  auto radius=std::max({mem.load(c+0x3c4,2),mem.load(c+0x3ac,2),mem.load(c+0x3ae,2)});
  Raw r=u32(c+12)&0x80000?0xabe0:static_cast<Raw>(radius);
  auto r2=from_int(r*r);auto pos=vec(c+0x10);
  auto near=[&](Address a) {auto d=vec(a);for (unsigned k=0;k<3;++k) d[k]=sub(pos[k],d[k]);return lt(add(add(mul(d[0],d[0]),mul(d[1],d[1])),mul(d[2],d[2])),r2);};
  bool now=false;
  if (i8(0x4039c1)) now=near(0x44f048);
  else for (int i=0;i<std::max(i8(0x403a08),0);++i) if (near(0x4039c8+12*i)) {now=true;break;}
  if (now!=was) {
    auto node=u32(c+4);if (node) call("handle_removeNode",{node});
    if (now) call("handle_link",{0x5db3a8u,static_cast<Raw>(c+4),static_cast<Raw>(c)});
    else {
      auto cursor=static_cast<std::int64_t>(s32(u32(0x4039fc)))+1;write(0x4039fc,4,static_cast<Raw>(cursor));
      if (cursor>=60) write(0x4039fc,4,0);
      call("handle_link",{static_cast<Raw>(0x5db3f0+4*s32(u32(0x4039fc))),static_cast<Raw>(c+4),static_cast<Raw>(c)});
    }
  }
  if ((u32(c+12)&0xc00000) && was!=now) {
    auto script=pointer_result(call("obj_getPropStr",{static_cast<Raw>(c),now?0x52fc60u:0x52fc48u,0u}));
    if (script) call("script_runByName",{script});
  }
  if (now) write(c+0xa8,8,flags(c)|UINT64_C(0x8000000000000000));
  else {call("anim_releaseCaches",{static_cast<Raw>(c+0x114)});write(c+0xa8,8,flags(c)&UINT64_C(0x7fffffffffffffff));}
  return now;
}
Raw Model::step_anim(Address c,Raw speed) {
  if (i16(c+0x232)==3) speed=mul(speed,static_cast<Raw>(call("player_speedScale",{UINT32_MAX})));
  if (i16(c+0x31a)>0) {
    auto info=static_cast<Raw>(call("SpellDrowsy_typeinfo",{}));auto spell=call("creature_findSpellByType",{static_cast<Raw>(c),info});
    if (spell) speed=mul(speed,static_cast<Raw>(call("SpellDrowsy_getStat3Fraction",{static_cast<Raw>(spell)})));
  }
  Raw ev=0;auto cf=flags(c);
  if (cf>>63) {
    auto sound=cf&0x2000000?0u:static_cast<Raw>(c+0x10);
    if (mem.load(c+0x2fc,1)>=1 && mem.load(c+0x2fc,1)<=60) speed=mul(mul(speed,from_int(60-i8(c+0x2fc))),0x3c888888);
    ev=static_cast<Raw>(call("anim_advance",{static_cast<Raw>(c+0x114),sound,static_cast<Raw>(c+0xbc),static_cast<Raw>(c+0xc4),speed}));
  }
  auto scale=u32(c+0xdc),rx=mul(u32(c+0xbc),scale),ry=mul(u32(c+0xc0),scale);
  write(c+0xbc,4,rx);write(c+0xc0,4,ry);
  auto cs=cos16(i16(c+0x38)-0x4000),sn=sin16(i16(c+0x38)-0x4000);
  rx=neg(u32(c+0xbc));ry=u32(c+0xc0);
  write(c+0xbc,4,add(mul(rx,cs),mul(ry,sn)));write(c+0xc0,4,sub(mul(rx,sn),mul(ry,cs)));
  if (mem.load(c+0x2fc,1)>=1 && mem.load(c+0x2fc,1)<=60) {
    auto k=mul(from_int(60-i8(c+0x2fc)),0x3c888888);auto yaw=i16(c+0xc4);
    write(c+0xc0,4,mul(u32(c+0xc0),k));write(c+0xbc,4,mul(u32(c+0xbc),k));write(c+0xc4,2,static_cast<Raw>(yaw*to_int(k)));
  }
  return ev;
}
void Model::follow_path(Address c,Raw name) {
  call("pathFollower_init",{name,vec(c+0x10)});auto points=pointer_result(call("paths_findByName",{name}));
  auto index=i16(c+0x58);if (index==-1) index=s32(static_cast<Raw>(call("pointList_snapToNearest",{})));
  auto point=pointer_result(call("pointList_getPoint",{points,static_cast<Raw>(index)}));if (!point) return;
  auto dx=sub(u32(c+0x10),u32(point)),dy=sub(u32(c+0x14),u32(static_cast<Address>(point)+4));
  if (lt(sqrt(add(mul(dx,dx),mul(dy,dy))),0x42400000)) {
    auto next=pointer_result(call("pointList_getPoint",{points,static_cast<Raw>(index+1)}));
    wvec(c+0x104,vec(next));wvec(c+0x3b4,vec(c+0x104));write(c+0x54,4,u32(c+0x300)+600);write(c+0x58,2,mem.load(static_cast<Address>(next)+0x3c,2));return;
  }
  wvec(c+0x104,vec(point));wvec(c+0x3b4,vec(c+0x104));auto mine=i16(c+0x58);auto number=mem.load(static_cast<Address>(point)+0x3c,1);
  if (mine!=static_cast<int>(number)) {write(c+0x54,4,u32(c+0x300)+600);write(c+0x58,2,mem.load(static_cast<Address>(point)+0x3c,2));mine=i16(c+0x58);}
  if (number==static_cast<unsigned>(mine) && s32(u32(c+0x54))<s32(u32(c+0x300)) && !near_view(c+0x10,0x447a0000)) {
    wvec(c+0x10,vec(c+0x3b4));call("obj_updateGridCell",{static_cast<Raw>(c)});
  }
}
void Model::follow_player(Address c) {
  auto slot=i8(c+0x5a);Raw leader=0;
  if (slot>=0 && slot<s32(u32(0x4d8ae0))) leader=player(slot);
  if (!leader || (u32(static_cast<Address>(leader)+12)&8)) {
    auto pos=vec(c+0x10);int best=-1,team=i8(c+0x310);Raw reach=0x43fa0000;
    for (int s=0;s<std::max(s32(u32(0x4d8ae0)),0);++s) {
      auto obj=player(s);if (!obj || (u32(static_cast<Address>(obj)+12)&8)) continue;
      auto p=vec(static_cast<Address>(obj)+0x10);for (unsigned k=0;k<3;++k) p[k]=sub(pos[k],p[k]);
      auto dist=sqrt(add(add(mul(p[0],p[0]),mul(p[1],p[1])),mul(p[2],p[2])));
      if (lt(reach,dist) || (team!=-1 && i8(0x4d8af0+static_cast<Address>(s)*0x4c30+0x4c24)!=team)) continue;
      reach=dist;best=s;
    }
    write(c+0x5a,1,static_cast<Raw>(best));
  } else {
    wvec(c+0x3b4,vec(static_cast<Address>(leader)+0x10));leader=player(static_cast<Raw>(i8(c+0x5a)));
    auto ox=mul(cos16(i16(static_cast<Address>(leader)+0x38)),0x41f00000);leader=player(static_cast<Raw>(i8(c+0x5a)));
    auto oy=mul(sin16(i16(static_cast<Address>(leader)+0x38)),0x41f00000);
    auto hx=(flags(c)&0x400000)?add(u32(c+0x3b4),ox):sub(u32(c+0x3b4),ox);
    auto hy=(flags(c)&0x400000)?add(u32(c+0x3b8),oy):sub(u32(c+0x3b8),oy);
    write(c+0x3b4,4,hx);write(c+0x3b8,4,hy);if (away(c)) wvec(c+0x104,vec(c+0x3b4));
  }
  if (flags(c)>>63) return;
  auto home=vec(c+0x3b4);Vec3 origin{home[0],home[1],add(home[2],0x41300000)};
  auto bearing=atan2_16(sub(u32(c+0x14),home[1]),sub(u32(c+0x10),home[0]));
  for (int i=0;i<32;++i) {
    auto offset=s16((i>>1)<<11);if (i&1) offset=s16(-offset);auto angle=s16(bearing+offset);
    auto length=u32(0x40278c);Vec3 direction{mul(cos16(angle),length),mul(sin16(angle),length),mul(0,length)};
    auto t=static_cast<Raw>(call("terrain_raycast",{u32(0x44fdc8),origin,direction,3u,0x3f8cccccu}));if (lt(t,0)) t=ONE;if (!le(ONE,t)) continue;
    Vec3 spot;for (unsigned k=0;k<3;++k) spot[k]=add(u32(c+0x3b4+4*k),mul(direction[k],t));
    if (call("players_anyWithinRadius",{spot,0x43e10000u})) continue;
    wvec(c+0x10,spot);call("obj_updateGridCell",{static_cast<Raw>(c)});call("obj_runEveryFrame",{static_cast<Raw>(c)});
    write(c+0xa8,8,flags(c)|UINT64_C(0x8000000000000000));wvec(c+0x104,vec(c+0x3b4));clear_path(c);return;
  }
}
void Model::chill(Address c) {
  auto left=sub(u32(c+0x344),0x3d888888);write(c+0x344,4,left);
  if (le(left,0)) {write(c+0x344,4,0);write(c+0x2fc,1,0);}
  auto of=u32(c+12);
  if (of&8) write(c+0x2fc,1,0);
  else {left=u32(c+0x344);write(c+0x2fc,1,le(left,0x42700000)?static_cast<Raw>(to_int(left)):0x3b);}
  auto limit=sub(0x42700000,mul(u32(c+0x60),0x42700000));
  if (lt(limit,from_int(i8(c+0x2fc)))) write(c+0x2fc,1,static_cast<Raw>(to_int(limit)));
  if (of&0x40) {if (i8(c+0x2fc)>=31) write(c+0x2fc,1,30);write(c+0x344,4,sub(u32(c+0x344),0x3d088888));}
  else if (call("creature_bossRank",{static_cast<Raw>(c)}) && i8(c+0x2fc)>=16) write(c+0x2fc,1,15);
  else if (i8(c+0x2fc)>=31) write(c+0x2fc,1,30);
}
void Model::boss_meter(Address c,std::uint64_t cf) {
  bool shown=false;
  if ((cf>>63) && !(u32(c+12)&8) && s32(u32(0x4f8424))>0) {
    auto pos=vec(c+0x10);
    for (int i=0;;++i) {
      auto p=player(u32(0x4f8430+4*i));
      if (p) {auto d=vec(static_cast<Address>(p)+0x10);for (unsigned k=0;k<3;++k) d[k]=sub(d[k],pos[k]);
        if (lt(add(add(mul(d[0],d[0]),mul(d[1],d[1])),mul(d[2],d[2])),mul(u32(c+0x3c0),u32(c+0x3c0)))) {
          shown=true;
          if (!(cf&0x80000000)) {write(c+0xa8,8,cf|0x80000000);write(0x41b6a8,4,0);write(0x41b6a4,4,0);write(0x41b6a0,4,0);}
          else {dispatch(c,19,{0x41ba40u});write(0x41b6a4,4,static_cast<Raw>(to_int(u32(c+0x22c))));write(0x41b6a0,4,static_cast<Raw>(to_int(u32(c+0x228))));}
          break;
        }
      }
      if (i+1>=s32(u32(0x4f8424))) break;
    }
  }
  if (!shown) {cf=flags(c);if (cf&0x80000000) {write(0x41b6a0,4,0);write(0x41b6a4,4,0);write(c+0xa8,8,cf&UINT64_C(0xffffffff7fffffff));}}
}
void Model::scavenger(Address c) {
  if (i16(c+0x232)!=20 || !(mem.load(c+0x11a,2)&0x80) || (flags(c)&UINT64_C(0xc000000000000000))!=UINT64_C(0x8000000000000000)) return;
  auto ally=handle(c+0x238);if ((ally && (flags(ally)&UINT64_C(0x40000000000))) || (u32(c+12)&2)) return;
  auto seed=u32(0x404464)*0x19660du+0x3c6ef35fu;write(0x404464,4,seed);++draws;
  auto turn=s32(seed)>>16,angle=i16(c+0x38)+turn;
  auto x=add(u32(c+0x10),mul(cos16(angle),0x41f00000)),y=add(u32(c+0x14),mul(sin16(angle),0x41f00000));
  Vec3 spot{x,y,u32(c+0x18)};auto facing=atan2_16(sub(u32(c+0x14),y),sub(u32(c+0x10),x));
  auto kind=s32(u32(0x402768));Raw critter=0;
  if (kind==1 || kind==2) {critter=pointer_result(call("pool_new",{0x448u}));if (critter) call("Critter_ctor",{critter,kind==1?0x530020u:0x530040u,spot,static_cast<Raw>(facing),kind==1?0x402770u:0x402780u});}
  if (!critter) return;
  angle=i16(static_cast<Address>(critter)+0x38);int best=0;Raw best_t=0;
  for (int i=0;i<32;++i) {
    Vec3 direction{mul(cos16(angle),0x43700000),mul(sin16(angle),0x43700000),mul(0x3e4ccccc,0x43700000)};
    auto origin=vec(static_cast<Address>(critter)+0x10);origin[2]=add(origin[2],0x41c00000);
    auto t=static_cast<Raw>(call("terrain_raycast",{u32(0x44fdc8),origin,direction,0u,ONE}));if (lt(t,0) || lt(ONE,t)) t=ONE;
    if (lt(best_t,t)) {best_t=t;best=angle;}if (eq(best_t,ONE)) break;angle=s16(angle+0x800);
  }
  write(static_cast<Address>(critter)+0x38,2,static_cast<Raw>(best));call("handle_clear",{static_cast<Raw>(c+0x238)});write(c+0x238,4,0);call("handle_link",{static_cast<Raw>(c+0x238),critter,critter});
}
void Model::creature_run(Address c,Raw model) {
  if (!(u32(c+12)&0x40)) dispatch(c,17,{Vec3{add(u32(c+0x10),0),u32(c+0x14),add(u32(c+0x18),mul(u32(c+0x20),HALF))}});
  for (auto off:{0x98,0x90}) for (unsigned a=off;a<static_cast<unsigned>(off+8);a+=2) {
    auto v=i16(c+a);if (v>0) {write(c+a,2,static_cast<Raw>(v-2));if (v-2<0) write(c+a,2,0);}
    else if (v<0) {write(c+a,2,static_cast<Raw>(v+2));if (v+2>0) write(c+a,2,0);}
  }
  if (flags(c)&0x8000000) {auto name=pointer_result(call("obj_getPropStr",{static_cast<Raw>(c),0x530048u,0u}));if (name) follow_path(c,name);}
  auto frame=u32(0x44fdcc);
  auto effect=[&](Raw kind) {call("fx_spawn",{kind,Vec3{u32(c+0x10),u32(c+0x14),add(u32(c+0x18),u32(c+0x20))}});};
  if (i16(c+0x316)) {write(c+0x316,2,mem.load(c+0x316,2)-1);if (!(frame&3)) effect(0x12);}
  auto root=mem.load(c+0x314,2);if (((root+1)&0xffff)>=2) write(c+0x314,2,root-1);
  for (auto off:{0x318,0x31a}) if (i16(c+off)) write(c+off,2,mem.load(c+off,2)-1);
  if (i16(c+0x31c)) {write(c+0x31c,2,mem.load(c+0x31c,2)-1);if (!(frame&3)) effect(0x10);}
  auto charm=i16(c+0x312);
  if (!charm) {if ((flags(c)&UINT64_C(0x2000000000000)) && i16(c+0x232)!=20) dispatch(c,27,{20u});}
  else {
    if (charm!=-1) {--charm;write(c+0x312,2,static_cast<Raw>(charm));}
    if (!charm) {write(c+0x310,1,255);write(c+0xa8,8,flags(c)&UINT64_C(0xffffffffff3fffff));}
    if (flags(c)&0xc00000) follow_player(c);
  }
  if (i8(c+0x2fc)!=60) write(c+0x300,4,u32(c+0x300)+1);
  if (u32(0x44fe08)&0x100) write(c+0x2fc,1,(u32(c+12)&0x40)?10:15);
  auto glow=u32(c+0x354);
  if (lt(0,glow)) {auto level=to_int(mul(fmin(ONE,glow),0x43000000));write(c+0x96,2,0x20);write(c+0x354,4,sub(glow,0x3c888888));for (auto off:{0x90,0x9c,0x98,0x94}) write(c+off,2,static_cast<Raw>(level));}
  if (lt(0,u32(c+0x340))) {
    auto of=u32(c+12);
    if (call("net_objOrGhost",{static_cast<Raw>(c)})==c && !(of&0x4000000)) {
      call("fx_burnModel",{model,static_cast<Raw>(c+0x114),vec(c+0x10),static_cast<Raw>(i16(c+0x38)),static_cast<Raw>(c+0x320),u32(c+0x340),u32(c+0xdc),u32(c+0xe0),from_int(static_cast<Raw>(mem.load(c+0x28,1)))});of=u32(c+12);
    }
    write(c+0x340,4,sub(u32(c+0x340),0x3c888888));
    if (!(of&0x20000) && !(u32(0x44fdcc)&31)) {
      Raw low=1,high=1;
      if (of&0x40) {
        auto number=table_service("Skill",u32(0x41b1a4),0x60,11,{0u,static_cast<Raw>(to_int(u32(0x4f714c))),0u});
        if (number.kind!=creature_movement::ValueKind::Integer) throw Unsupported("burn skill result");
        auto base=mul(from_int(static_cast<Raw>(number.scalar)),0x3e199999);auto roll=shared_draw();low=static_cast<Raw>(to_int(base));high=static_cast<Raw>(to_int(add(base,mul(mul(base,0x3e4ccccc),mul(from_int(roll),0x37800000)))));
      }
      call("damage_apply",{static_cast<Raw>(c),static_cast<Raw>(c),100000u,0u,0u,0x1180c040u,TaggedTuple{"elem",{Structured::integer(low&0xffff),Structured::integer(high&0xffff)}},0u,0u,0u});
    }
  }
  if (lt(0,u32(c+0x348))) {
    if (call("net_objOrGhost",{static_cast<Raw>(c)})==c && !(u32(c+12)&0x4000000)) call("fx_shockModel",{model,static_cast<Raw>(c+0x114),vec(c+0x10),static_cast<Raw>(i16(c+0x38)),static_cast<Raw>(c+0x320),0x5e808080u,u32(c+0x348),u32(c+0xdc),u32(c+0xe0)});
    auto left=sub(u32(c+0x348),0x3c888888);write(c+0x348,4,left);if (le(left,0)) for (auto off:{0x33c,0x334,0x32c}) write(c+off,4,0);
  }
  if (lt(0,u32(c+0x344))) chill(c);
  auto count=s32(u32(c+0x388));
  if (count) {
    if (count<=0 || (u32(c+12)&8)) {write(c+0x388,4,0);write(c+0x2fc,1,0);}
    else {auto elapsed=s32(u32(0x44fdcc)-u32(c+0xf0));if (elapsed<31) {write(c+0x2fc,1,static_cast<Raw>(elapsed)*2);if (signed8(static_cast<Raw>(elapsed)*2)>=61) write(c+0x2fc,1,60);}else write(c+0x2fc,1,count<31?mem.load(c+0x388,1):60);}
    auto limit=sub(0x42700000,mul(u32(c+0x6c),0x42700000));if (lt(limit,from_int(i8(c+0x2fc)))) write(c+0x2fc,1,static_cast<Raw>(to_int(limit)));
  }
  for (auto t: {std::array<unsigned,3>{0x34c,0x358,0x35a},std::array<unsigned,3>{0x350,0x35c,0x35e}}) {
    auto left=u32(c+t[0]);if (lt(0,left)) {write(c+t[0],4,sub(left,0x3c888888));if (!(u32(c+12)&0x20000) && !(u32(0x44fdcc)&31)) call("damage_apply",{static_cast<Raw>(c),static_cast<Raw>(c),100000u,0u,0u,0x1080c040u,0u,0u,mul(from_int(i16(c+t[1])),0x3dcccccc),mul(from_int(i16(c+t[2])),0x3dcccccc)});}
  }
  auto cf=flags(c);if ((cf&1) && !i8(c+0x231)) boss_meter(c,cf);scavenger(c);
  auto z=u32(c+0x3c),v=add(u32(c+0x40),mul(sub(u32(c+0x18),z),0x40400000));v=sub(v,mul(sqrt(0x3db851eb),v));write(c+0x40,4,v);write(c+0x3c,4,add(z,mul(v,0x3c888888)));
  auto state=i16(c+0x232);
  if ((state==0 || state==5) && i16(c+0x312)) {cf=flags(c);if (cf&UINT64_C(0x800000000000000)) {dispatch(c,27,{1u});cf=flags(c);}write(c+0xa8,8,cf&UINT64_C(0xf7ffffffffffffff));}
  if ((flags(c)&UINT64_C(0x8000000001000000))==0x1000000 && i16(c+0x232)==20) {write(c+12,4,u32(c+12)|0x4002);call("obj_delete",{static_cast<Raw>(c)});}
}
void Model::knockback(Address c,Raw f,std::array<Raw,4> speed) {
  for (unsigned i=0;i<4;++i) if (f&(0x40000u<<i)) {write(c+0x2e8,4,speed[i]);return;}
}
void Model::hit_reaction(Address c) {
  if (!((u32(c+0xa8)^1)&1)) {if (s32(u32(0x4d8ae0))>=2 || !(random(c)&1)) return;}
  if (!(u32(c+0x2dc)&0x4380000)) if (!((flags(c)&0x400) && random(c)%3!=0)) if (random(c)%3!=0) return;
  dispatch(c,27,{18u});
}
Raw Model::gib(Address c,Vec3 pos,std::optional<Raw> variant,Raw kind) {
  auto chunk=pointer_result(call("pool_new",{0x268u}));if (chunk) {if (!variant) variant=random(c)%3;call("GutChunk_ctor",{chunk,pos,*variant,kind,ONE});}return chunk;
}
void Model::hit_effect(Address c,Raw model) {
  std::array<Raw,6> bounds{};
  if (model) {
    auto b=service("model_getBounds",{model,UINT32_MAX});
    if (b.kind!=creature_movement::ValueKind::Tuple || b.items.size()!=6) throw Unsupported("model bounds shape");
    for (unsigned k=0;k<6;++k) {if (b.items[k].kind!=creature_movement::ValueKind::Integer) throw Unsupported("model bound lane");bounds[k]=static_cast<Raw>(b.items[k].scalar);}
  }
  Raw x,y,z0=u32(c+0x18);
  if (le(0x41c80000,u32(c+0x1c))) {
    auto angle=i16(c+0x2e0)+0x7fff;
    x=add(u32(c+0x10),mul(cos16(angle),mul(u32(c+0x1c),0x3f400000)));
    y=add(u32(c+0x14),mul(sin16(angle),mul(u32(c+0x1c),0x3f400000)));
  } else {x=u32(c+0x10);y=u32(c+0x14);}
  auto z=add(z0,0x42100000);auto top=s32(bounds[5]),bottom=s32(bounds[2]);auto height=s32(static_cast<Raw>(top)-static_cast<Raw>(bottom));
  if (top<36 && height>0) z=add(add(z0,from_int(height>>1)),0x41400000);
  Vec3 pos{x,y,z};auto cf=flags(c);
  if (lt(0,u32(c+0x344)) || (cf&0x4000)) call("fx_spawn",{10u,pos});
  else if (cf&0x8000) call("fx_spawn",{11u,pos});
  else if (cf&0x10000) call("fx_spawn",{2u,pos});
  else if (cf&0x20000) {
    auto text=service("libc_sprintf",{0x52ffe0u,Structured{}});
    auto entry=static_cast<Raw>(call("res_find",{0x52fc18u,text}));call("sound_playOnObject",{entry,static_cast<Raw>(c),ONE});
    auto attach=[&](Raw index) {
      auto a=service("model_getAttachPoint",{model,vec(c+0x10),static_cast<Raw>(i16(c+0x38)),static_cast<Raw>(c+0x114),index,0u,ONE});
      if (a.kind!=creature_movement::ValueKind::Vector) throw Unsupported("attach point shape");return a.vec;
    };
    auto a=attach(0),b=attach(1);Vec3 mid;for (unsigned k=0;k<3;++k) mid[k]=mul(add(a[k],b[k]),HALF);
    Vec3 vel{mul(cos16(i16(c+0x38)),0x41200000),mul(sin16(i16(c+0x38)),0x41200000),0x41200000};
    for (unsigned n=0;n<50;++n) {
      Vec3 r{};unsigned attempt=0;
      do {if (++attempt>4096) throw Unsupported("particle rejection budget");for (auto &lane:r) {auto u=mul(from_int(random(c)),0x37800000);lane=sub(add(u,u),ONE);}}
      while (lt(ONE,add(add(mul(r[0],r[0]),mul(r[1],r[1])),mul(r[2],r[2]))));
      for (unsigned k=0;k<3;++k) vel[k]=add(vel[k],mul(r[k],0x40a00000));call("particle_spawn",{0x5bf608u,mid,vel,0u});
    }
  } else if (cf&0x3000) {
    Raw kind=cf&0x2000?5:3;call("fx_spawn",{cf&0x2000?3u:9u,pos});gib(c,pos,2,kind);
    if (random(c)%3==0) gib(c,pos,2,kind);if (le(u32(c+0x228),0) && random(c)%3==0) gib(c,pos,2,kind);
  } else if ((s32(u32(0x44fdc0))>0 || i8(0x402791)) && (cf&0x40000)) {
    call("fx_spawn",{0u,pos});if (random(c)%3==0) gib(c,pos,2,1);
  }
}
Raw Model::remove_body(Address c) {
  auto cf=flags(c);auto f=u32(c+0x2dc);write(c+12,4,u32(c+12)|0x4002);write(c+0xa8,8,cf|UINT64_C(0x20000000000));return f;
}
Raw Model::shatter(Address c,Raw model,Raw f) {
  if ((u32(c+0xa8)^1)&1) {
    auto cf=flags(c);
    if (!(cf&0x20000000) && lt(0,u32(c+0x344)) && model) {
      if (!(random(c)&7)) {call("fx_shatterToLooseIce",{model,static_cast<Raw>(c+0x114),vec(c+0x10),static_cast<Raw>(i16(c+0x38)),static_cast<Raw>(c+0x320),u32(c+0x1c),u32(c+0xdc),u32(c+0xe0)});return remove_body(c);}
      f=u32(c+0x2dc);
    }
  }
  auto cf=flags(c);if ((cf&0x20000001) || !(f&0x800) || !(f&0x3c0000)) return f;
  if (!(cf&0x3000) && (!(cf&0x40000) || s32(u32(0x44fdc0))<=0 || i8(0x402791))) return f;
  auto pos=vec(c+0x10);
  if (s32(u32(0x44fdc0))>0) {
    auto seed=u32(0x404468)*0x19660du+0x3c6ef35fu;auto angle=s32(seed)>>16;seed=seed*0x19660du+0x3c6ef35fu;write(0x404468,4,seed);
    auto kind=((seed>>16)&1)+(cf&0x2000?4:cf&0x1000?7:1);call("decal_add",{pos,static_cast<Raw>(angle),kind,0u,0x42300000u});
    for (unsigned i=0;i<3;++i) {
      seed=u32(0x404468);Vec3 r{};unsigned attempt=0;
      do {if (++attempt>4096) throw Unsupported("gib rejection budget");for (auto &lane:r) {seed=seed*0x19660du+0x3c6ef35fu;auto u=mul(from_int(seed>>16),0x37800000);lane=sub(add(u,u),ONE);}}
      while (lt(ONE,add(add(mul(r[0],r[0]),mul(r[1],r[1])),mul(r[2],r[2]))));
      cf=flags(c);write(0x404468,4,seed);Vec3 base{add(pos[0],0),pos[1],add(pos[2],0x42100000)},spot;
      for (unsigned k=0;k<3;++k) spot[k]=add(mul(r[k],0x41200000),base[k]);call("fx_spawn",{cf&0x1000?9u:cf&0x2000?3u:1u,spot});
    }
  }
  for (auto row:{std::array<Raw,6>{2,0,0x42a00000,0x41a00000,16000,0xe0c0},std::array<Raw,6>{4,1,0x42480000,0x41200000,40000,0xb1e0}}) {
    for (unsigned i=0;i<row[0];++i) {
      cf=flags(c);auto chunk=gib(c,pos,std::nullopt,cf&0x1000?2:cf&0x2000?5:row[1]);if (!chunk) continue;
      auto push=add(mul(mul(from_int(random(c)),0x37800000),row[2]),row[3]);auto angle=s16(row[5]+random(c)%row[4]);
      auto heading=i16(c+0x2e0)+angle;write(static_cast<Address>(chunk)+0xfc,4,add(mul(u32(static_cast<Address>(chunk)+0xfc),0x3e999999),mul(cos16(heading),push)));
      heading=i16(c+0x2e0)+angle;write(static_cast<Address>(chunk)+0x100,4,add(mul(u32(static_cast<Address>(chunk)+0x100),0x3e999999),mul(sin16(heading),push)));call("PhProp_initPhysics",{chunk,0x12u});
    }
  }
  return remove_body(c);
}
void Model::award_kill(Address c) {
  auto players=s32(u32(0x4d8ae0));std::int64_t levels=0;for (int s=0;s<std::max(players,0);++s) levels+=i8(0x4d8af0+static_cast<Address>(s)*0x4c30+0x59);
  auto value=powf(div(u32(c+0xd8),from_int(static_cast<Raw>(levels))),0x3fb33333);std::int64_t exp=to_int(value);int killer=-1;
  auto node=u32(c+0x2e4);
  if (node) {auto who=u32(static_cast<Address>(node)+12);if (player(0)==who) killer=0;else for (int s=1;s<s32(u32(0x4d8ae0));++s) if (player(s)==who) killer=s;}
  if (killer!=-1) {
    auto rank=static_cast<Raw>(call("player_getSkillRank",{static_cast<Raw>(killer),0x3bu}));auto chance=mul(static_cast<Raw>(call("SkillLifeTap_getStat0Fraction",{u32(0x41b25c),rank})),0x47800000);
    if (lt(from_int(random(c)),chance)) {auto ball=pointer_result(call("pool_new",{0xb8u}));if (ball) call("OniBall_ctor",{ball,vec(c+0x10),static_cast<Raw>(i16(c+0x38)),static_cast<Raw>(c),0u});}
  }
  exp=to_int(mul(from_int(static_cast<Raw>(exp)),0x3f400000));
  auto cap=[&](Address rec) -> std::int64_t {auto level=i8(rec+0x59);auto a=static_cast<Raw>(0x41b558+4*level);return level<80?static_cast<std::int64_t>(s32(u32(a+4)))-s32(u32(a)):s32(u32(a));};
  auto mana=[&](Address rec) {auto m=add(u32(rec+0xc0),mul(u32(c+0x22c),0x3d088888));write(rec+0xc0,4,m);if (lt(u32(rec+0xc4),m)) write(rec+0xc0,4,u32(rec+0xc4));};
  if (s32(u32(0x4d8ae0))<2) {
    for (int s=0;s<s32(u32(0x4d8ae0));++s) {Address rec=0x4d8af0+static_cast<Address>(s)*0x4c30;if (lt(0,u32(rec+0xac))) {exp=std::min(exp,cap(rec));call("player_giveExp",{static_cast<Raw>(s),static_cast<Raw>(exp)});if (!u32(rec+0x50)) mana(rec);}}return;
  }
  auto base=from_int(static_cast<Raw>(exp));
  for (int s=0;s<s32(u32(0x4d8ae0));++s) {
    Address rec=0x4d8af0+static_cast<Address>(s)*0x4c30;auto share=killer==-1?HALF:killer==s?0x3f199999u:0x3eccccccu;
    auto amount=to_int(mul(base,share));if (le(u32(rec+0xac),0)) amount=to_int(mul(from_int(static_cast<Raw>(amount)),HALF));
    call("player_giveExp",{static_cast<Raw>(s),static_cast<Raw>(std::min(static_cast<std::int64_t>(amount),cap(rec)))});if (!u32(rec+0x50) && killer==s) mana(rec);
  }
}
void Model::die(Address c,Raw model) {
  write(c+12,4,u32(c+12)|9);for (auto off:{0x340,0x344,0x348,0x34c,0x350}) if (lt(ONE,u32(c+off))) write(c+off,4,ONE);
  auto f=u32(c+0x2dc);
  if ((f&0x20) || lt(ONE,u32(c+0x340))) write(c+12,4,u32(c+12)|0x2000000);
  else if (((f&0x10) || (f&0x2000000)) && model) {
    call("fx_disintegrateModel",{static_cast<Raw>(c),model,static_cast<Raw>(c+0x114),vec(c+0x10),static_cast<Raw>(i16(c+0x38)),f&0x10?0u:1u,u32(c+0xdc),u32(c+0xe0)});f=u32(c+0x2dc);
  } else f=shatter(c,model,f);
  knockback(c,f,{0x42c80000,0x43160000,0x43480000,0x439c4000});dispatch(c,27,{20u});write(c+12,4,u32(c+12)|0x2000);
  if (call("obj_getPropInt",{static_cast<Raw>(c),0x52fa10u,0u})) {call("anim_snapTopToEnd",{static_cast<Raw>(c+0x114)});return;}
  if (flags(c)&UINT64_C(0x100000000)) return;award_kill(c);
}
void Model::process_damage(Address c,Raw model) {
  // The four cos16/sin16 pairs (hit_effect and the gibs) already pass the
  // registers combat round 5 executed: hit +0x2e0 plus 0x7fff, facing +0x38,
  // hit plus s16(bias + draw mod span). fx_trig::process_damage_* name them.
  if (!lt(0,u32(c+0xc8))) {
    if (i16(c+0x232)!=20) {auto f=u32(c+0x2dc);knockback(c,f,{0x42480000,0x42960000,0x42c80000,0x431c4000});if (f==0x2000 && i16(c+0x232)!=18) hit_reaction(c);}
    write(c+0x2dc,4,0);return;
  }
  if (s32(u32(0x4d8ae0))==1) call("world_notifyObjectsNear",{0u,vec(c+0x10),static_cast<Raw>(c),0x44160000u});write(c+0xc8,4,0);
  if (i16(c+0x232)==20) return;write(c+12,4,u32(c+12)|0x100);
  if (u32(c+0x2dc)&13) hit_effect(c,model);
  if (!le(u32(c+0x228),0)) {
    if (i16(c+0x232)!=20) {auto f=u32(c+0x2dc);knockback(c,f,{0x42c80000,0x43160000,0x43610000,0x43bb8000});if (!(f&0x400000) && i16(c+0x232)!=18) hit_reaction(c);}
    write(c+0x2dc,4,0);return;
  }
  die(c,model);write(c+0x2dc,4,0);
}
Raw Model::is_enemy(Address c,Raw other) {
  if (!other || other==c) return 0;auto f=u32(static_cast<Address>(other)+12);
  if (!(f&0x60) || (f&0x20008) || (flags(other)&0x4000800)) return 0;
  auto a=dispatch(c,37),b=dispatch(other,37);
  if (s32(static_cast<Raw>(a.value_or(0)))==s32(static_cast<Raw>(b.value_or(0)))) return 0;
  return flags(other)&UINT64_C(0x100000000000)?4:1;
}
void Model::pick_target(Address c,Raw force) {
  if (i8(c+0x231)) return;
  if (i16(c+0x316) || (flags(c)&0x4000000)) {call("handle_clear",{static_cast<Raw>(c+0x234)});write(c+0x234,4,0);return;}
  auto node=u32(c+0x234);
  if (i16(c+0x232)!=20 && node && (u32(static_cast<Address>(u32(static_cast<Address>(node)+12))+12)&8)) {dispatch(c,27,{0u});return;}
  Raw best=0;if (node && dispatch(c,20,{u32(static_cast<Address>(node)+12)}).value_or(0)) best=u32(static_cast<Address>(u32(c+0x234))+12);
  if (!((u32(0x44fdcc)^static_cast<Raw>(i8(c+0x230)))&31) || force) {
    auto reach=u32(c+0x3c0);auto p=vec(c+0x10);Vec3 low,high;for (unsigned k=0;k<3;++k) {low[k]=sub(p[k],reach);high[k]=add(p[k],reach);}
    auto saved=u32(0x4039b8);write(0x4039b8,4,8);auto found=service("world_collectObjectsInBox",{low,high,0xc0u});write(0x4039b8,4,saved);
    if (found.kind!=creature_movement::ValueKind::List) throw Unsupported("target box list shape");
    best=0;std::int64_t best_score=0;auto best_d2=mul(u32(c+0x3c0),u32(c+0x3c0));
    for (const auto &entry:found.items) {
      if (entry.kind!=creature_movement::ValueKind::Integer) throw Unsupported("target list lane");Raw other=static_cast<Raw>(entry.scalar);
      std::int64_t score=s32(static_cast<Raw>(dispatch(c,20,{other}).value_or(0)));if (!score) continue;
      auto dy=sub(u32(c+0x14),u32(static_cast<Address>(other)+0x14)),dx=sub(u32(c+0x10),u32(static_cast<Address>(other)+0x10));auto d2=add(mul(dx,dx),mul(dy,dy));
      if (handle(c+0x2e4)==other && u32(c+0x2e4)) ++score;node=u32(c+0x234);auto current=node?u32(static_cast<Address>(node)+12):0;if (node && current==other) ++score;
      bool take=best_score<score || (score==best_score && lt(d2,best_d2));if (!take) take=force && node && current!=other;if (best && force && node && current==other) take=false;if (!take) continue;
      auto hx=u32(c+0x3b4),hy=u32(c+0x3b8);auto oy=sub(hy,u32(static_cast<Address>(other)+0x14)),ox=sub(hx,u32(static_cast<Address>(other)+0x10));auto from_home=add(mul(ox,ox),mul(oy,oy));
      if (!(s32(u32(0x44fdcc)-480)<s32(u32(c+0x304)))) if (lt(mul(u32(c+0x3c0),u32(c+0x3c0)),from_home)) {
        auto my=sub(u32(c+0x14),hy),mx=sub(u32(c+0x10),hx);if (lt(add(mul(mx,mx),mul(my,my)),from_home)) {auto near=add(add(u32(static_cast<Address>(other)+0x1c),u32(c+0x1c)),0x42700000);if (lt(mul(near,near),d2)) continue;}
      }
      best=other;best_d2=d2;best_score=score;
    }
  }
  node=u32(c+0x234);if (!best || (node && u32(static_cast<Address>(node)+12)!=best)) {call("handle_clear",{static_cast<Raw>(c+0x234)});write(c+0x234,4,0);}
  if (best && !u32(c+0x234)) call("handle_link",{static_cast<Raw>(c+0x234),best,best});
}
Vec3 Model::random_home(Address c,Raw supplied) {
  auto reach=s32(supplied);if (lt(u32(c+0x3c0),from_int(static_cast<Raw>(reach)))) reach=to_int(u32(c+0x3c0));
  auto home=vec(c+0x3b4),d=vec(c+0x10);for (unsigned k=0;k<3;++k) d[k]=sub(d[k],home[k]);auto d2=add(add(mul(d[0],d[0]),mul(d[1],d[1])),mul(d[2],d[2]));
  if (le(from_int(static_cast<Raw>(reach)*static_cast<Raw>(reach)),d2) || ((flags(c)&0x400000) && lt(0x46610000,d2))) return home;
  auto point=home;
  for (unsigned i=0;i<10;++i) {
    auto x=add(u32(c+0x3b4),from_int((random(c)&(static_cast<Raw>(reach)*2))-static_cast<Raw>(reach)));
    auto y=add(u32(c+0x3b8),from_int((random(c)&(static_cast<Raw>(reach)*2))-static_cast<Raw>(reach)));point={x,y,u32(c+0x3bc)};
    auto ground=static_cast<Raw>(call("terrain_heightBelow",{u32(0x44fdc8),point,u32(c+0x1c)}));auto z=u32(c+0x18);if (lt(sub(z,0x41400000),ground) && lt(ground,add(z,0x41400000))) break;
  }return point;
}
void Model::move_to_dest(Address c) {
  if (move_to(c,vec(c+0x104))) dispatch(c,27,{0u});
  if ((flags(c)&0x8000000) || s32(u32(c+0x2c4))<4) return;clear_path(c);wvec(c+0x104,random_home(c,300));
}
Vec3 Model::avoidance_offset(Address c,Raw ignore,Raw length) {
  auto world=u32(0x44fdc8);
  auto cast=[&](Vec3 origin,Vec3 direction) {
    auto v=service("world_raycast",{world,origin,direction,static_cast<Raw>(c),1u,3u,1u,ONE});
    if (v.kind!=creature_movement::ValueKind::Tuple || v.items.size()!=2 || v.items[0].kind!=creature_movement::ValueKind::Integer || v.items[1].kind!=creature_movement::ValueKind::Integer) throw Unsupported("avoidance ray shape");
    Raw t=static_cast<Raw>(v.items[0].scalar),hit=static_cast<Raw>(v.items[1].scalar);return std::pair<Raw,Raw>{lt(t,0)?ONE:t,hit};
  };
  auto beside=[&](int turn,Raw reach) {return std::array<Raw,2>{mul(cos16(i16(c+0x38)+turn),reach),mul(sin16(i16(c+0x38)+turn),reach)};};
  Vec3 forward{mul(cos16(i16(c+0x38)),length),mul(sin16(i16(c+0x38)),length),u32(c+0x1c)};
  Vec3 centre{u32(c+0x10),u32(c+0x14),add(u32(c+0x18),0x41500000)};auto [tc,h0]=cast(centre,forward);
  auto side_origin=[&](int turn) {auto o=beside(turn,u32(c+0x1c));return Vec3{add(u32(c+0x10),o[0]),add(u32(c+0x14),o[1]),add(u32(c+0x18),0x41500000)};};
  auto [tl,h1]=cast(side_origin(0x4000),forward);auto [tr,h2]=cast(side_origin(-0x4000),forward);
  if (!(lt(tc,ONE) || lt(tl,ONE) || lt(tr,ONE))) {write(c+0x2f8,1,0);return {};}
  auto side=i8(c+0x2f8);
  if (!side) {
    auto nearest=fmin(fmin(tc,tl),tr);centre={u32(c+0x10),u32(c+0x14),add(u32(c+0x18),0x41500000)};
    auto o=beside(0x4000,add(u32(c+0x1c),0x40a00000));auto [tsl,h3]=cast(centre,{o[0],o[1],u32(c+0x1c)});
    o=beside(-0x4000,add(u32(c+0x1c),0x40a00000));auto [tsr,h4]=cast(centre,{o[0],o[1],u32(c+0x1c)});
    if (ignore && ((h0==ignore && eq(tc,nearest)) || (h1==ignore && eq(tl,nearest)) || (h2==ignore && eq(tr,nearest)))) return {};
    auto blocked=lt(tc,ONE) || lt(tr,ONE) || lt(tl,ONE);
    if (h3==ignore && lt(tsl,nearest)) side=2;else if (h4==ignore && lt(tsr,nearest)) side=1;
    else if (blocked && lt(tsr,ONE)) side=2;else if (blocked && lt(tsl,ONE)) side=1;else side=lt(tr,ONE)?2:1;
    write(c+0x2f8,1,side);
  }
  if (side!=1 && side!=2) return {};auto o=beside(side==1?-12000:12000,u32(c+0x1c));return {add(o[0],o[0]),add(o[1],o[1]),0};
}
void Model::step(Address c,std::int32_t off) {
  auto speed=u32(c+0xb8);
  if (i16(c+0x31a)>0) {auto info=static_cast<Raw>(call("SpellDrowsy_typeinfo",{}));auto spell=call("creature_findSpellByType",{static_cast<Raw>(c),info});if (spell) speed=mul(speed,static_cast<Raw>(call("SpellDrowsy_getStat3Fraction",{static_cast<Raw>(spell)})));}
  if (off<=-0x3e81 || off>=0x3e81) {write(c+0xbc,4,mul(u32(c+0xbc),HALF));write(c+0xc0,4,mul(u32(c+0xc0),HALF));speed=mul(u32(c+0xb8),HALF);}
  if (mem.load(c+0x2fc,1)>=1 && mem.load(c+0x2fc,1)<=60) speed=mul(speed,mul(from_int(60-i8(c+0x2fc)),0x3c888888));
  auto vx=mul(cos16(i16(c+0x38)),speed),vy=mul(sin16(i16(c+0x38)),speed);movement(c,false,{vx,vy,0});
}
void Model::flee(Address c,Raw kind) {
  auto target=handle(c+0x234);if (!u32(c+0x234) || !target) return;
  auto fx=u32(c+0xf8),fy=u32(c+0xfc);bool pick=eq(fx,MINUS_ONE) && eq(fy,MINUS_ONE);
  if (!pick) {auto dx=sub(fx,u32(c+0x10)),dy=sub(fy,u32(c+0x14));pick=lt(sqrt(add(mul(dx,dx),mul(dy,dy))),u32(c+0x1c));}
  if (pick) wvec(c+0xf8,random_home(c,300));
  else {
    auto hx=sub(u32(c+0x10),u32(c+0x3b4)),hy=sub(u32(c+0x14),u32(c+0x3b8)),hz=sub(u32(c+0x18),u32(c+0x3bc));
    auto limit=sub(mul(u32(c+0x3c0),u32(c+0x3c0)),0x43c80000);if (lt(limit,add(add(mul(hx,hx),mul(hy,hy)),mul(hz,hz)))) wvec(c+0xf8,vec(c+0x3b4));
  }
  if (!((u32(0x44fdcc)^static_cast<Raw>(i8(c+0x230)))&7)) wvec(c+0x2ec,avoidance_offset(c,0,add(u32(c+0x1c),0x41200000)));
  int off;
  if (!eq(u32(c+0x2ec),0) && !eq(u32(c+0x2f0),0)) {
    Vec3 aim{add(u32(c+0x2ec),u32(c+0x10)),add(u32(c+0x2f0),u32(c+0x14)),add(u32(c+0x2f4),u32(c+0x18))};off=s16(static_cast<Raw>(dispatch(c,26,{0u,aim}).value_or(0)));
  } else {
    auto point=waypoint(c);
    for (unsigned i=0;i<16;++i) {
      auto next=waypoint(c,1);auto nx=sub(next[0],point[0]),ny=sub(next[1],point[1]),mx=sub(u32(c+0x10),point[0]),my=sub(u32(c+0x14),point[1]);
      if (!eq(point[0],MINUS_ONE)) {auto wx=sub(point[0],u32(c+0x10)),wy=sub(point[1],u32(c+0x14));if (!lt(sqrt(add(mul(wx,wx),mul(wy,wy))),u32(c+0x1c))) {if (eq(next[0],MINUS_ONE) || !lt(0,add(mul(nx,mx),mul(ny,my)))) break;}}
      auto index=static_cast<std::int64_t>(s32(u32(c+0x2bc)));point[0]=point[1]=MINUS_ONE;
      if (index<16) {
        auto address=static_cast<std::int64_t>(c)+0x23c+8*index;if (address<0 || address>UINT32_MAX-15) throw Unsupported("flee waypoint span");
        bool request=true;
        if (s32(u32(static_cast<Address>(address)))!=-1) {write(c+0x2bc,4,static_cast<Raw>(index+1));if (index+1!=16) {auto x=u32(static_cast<Address>(address)+8);if (s32(x)!=-1) {point={from_int(x),from_int(u32(static_cast<Address>(address)+12)),point[2]};request=false;}}}
        if (request) {call(signed8(kind)?"path_requestType3":"path_requestType2",{u32(0x44fdc8),vec(c+0x10),vec(static_cast<Address>(target)+0x10),static_cast<Raw>(c),static_cast<Raw>(c+0x23c),16u,0x44160000u});write(c+0x2bc,4,16);}
      }
      if (eq(point[0],MINUS_ONE)) break;
    }
    if (eq(point[0],MINUS_ONE)) point=vec(c+0xf8);off=s16(static_cast<Raw>(dispatch(c,26,{0u,point}).value_or(0)));wvec(c+0xf8,point);
  }
  step(c,off);
}
std::optional<std::uint64_t> Model::smallspider_run(Address c) {return run_monster(c,0);}
std::optional<std::uint64_t> Model::gnaw_run(Address c) {return run_monster(c,1);}
std::optional<std::uint64_t> Model::mosquito_run(Address c) {return run_monster(c,2);}
std::optional<std::uint64_t> Model::scorpion_run(Address c) {return run_monster(c,3);}
std::optional<std::uint64_t> Model::badger_run(Address c) {return run_monster(c,4);}
std::optional<std::uint64_t> Model::run_monster(Address c,unsigned family) {
  Depth guard(depth_);creature_span(c);if ((family==3 && c>UINT32_MAX-0x3cbu) || (family==4 && c>UINT32_MAX-0x3cfu)) throw Unsupported("monster extension raw32 span");
  auto model=[&] {return family==1?static_cast<Raw>(dispatch(c,16).value_or(0)):u32(static_cast<Address>(u32(c+0x24))+12);};
  process_damage(c,model());creature_run(c,model());if (!update_active(c)) return {};
  auto ev=step_anim(c,ONE);call("Creature_applyKnockback",{static_cast<Raw>(c),family==2?0x3fc00000u:family==3?HALF:family==4?0x3e4cccccu:ONE});
  if (family) dispatch(c,23,{0u});auto state=family?static_cast<int>(mem.load(c+0x232,2)):i16(c+0x232);
  auto st=[&](Raw v) {return dispatch(c,27,{v});};
  if (!family) {if (state==1) move_to_dest(c);else if (state==0 && s32(u32(c+0xf4))<s32(u32(c+0x300))) st(1);else if (state==20) dead_update(c,ev,0,0x41200000);return {};}
  auto target=[&] {return u32(c+0x234)!=0;};
  auto ready=[&] {if (!target()) write(c+0x318,2,0);return i16(c+0x318)==0;};
  auto far=family==1?0x40c00000u:family==2?0x42200000u:family==3?0x41a00000u:0x41c80000u;
  auto arc=family==1?8000u:family==4?7000u:6000u;
  switch(state) {
  case 0:if (dispatch(c,25,{0u}).value_or(0)) return st(3);if (away(c)) return st(1);if (family==3 && s32(u32(c+0xf4))<s32(u32(c+0x300))) return st(1);break;
  case 1:if (dispatch(c,25,{0u}).value_or(0)) return st(family==1?3:0);if (family!=2 || !anim_flags(c,0x20)) move_to_dest(c);break;
  case 3:
    if (family==4) {badger_chase(c);break;}
    if (!target()) return st(0);if (family==2 && anim_flags(c,0x10)) return {};
    if (see(c,arc,far)) return st(family==1?5:6);if (see(c,0x7fff,far)) turn_to_target(c);else move_to_target(c);break;
  case 5:
    if (family==2 && !target()) return st(0);
    if (family==4 && !target()) {if (!i16(c+0x314)) return st(3);return {};}
    if (family==2) turn_to_target(c);
    if (see(c,arc,far)) return st(6);
    if (family==4 && call("creature_findEnemyInCone",{static_cast<Raw>(c),static_cast<Raw>(i16(c+0x38)),7000u,0u,far})) return st(6);
    if (family==1 && see(c,0x7fff,far)) turn_to_target(c);else if (!i16(c+0x314)) return st(3);break;
  case 6:
    if (family==3) {scorpion_attack(c,ev);break;}if (family==4) {badger_attack(c,ev);break;}
    if (family==1) turn_to_target(c);else movement(c,false,{});
    if (family==1 || target()) {
      if (ev&1) call(family==1?"Creature_hitTargetInArc":"Creature_hitTargetInArcEdge",{static_cast<Raw>(c),static_cast<Raw>(i16(c+0x38)),arc,static_cast<Raw>(i16(c+0xd0)),static_cast<Raw>(i16(c+0xd2)),static_cast<Raw>(i16(c+0xd4)),family==1?8u:4u,0u,0u,far});
      if (family==2) turn_to_target(c);
    }
    if (!anim_flags(c,8)) return st(family==1?5:3);break;
  case 7:if (family==3) scorpion_attack(c,ev);else if (family==4) badger_attack(c,ev);else return st(0);break;
  case 8:case 9:case 10:if (family==4) badger_attack(c,ev);else if (family<=2) return st(0);break;
  case 16:if (ready()) return st(0);flee(c,0);break;
  case 18:if (family==2) movement(c,false,{});if (!anim_flags(c,0x10)) return st(family==1?5:3);break;
  case 19:if (!i16(c+0x31c)) return st(family<=2?3:0);break;
  case 20:
    if (family==2) movement(c,false,{});dead_update(c,ev,family==1?1:0,family==1?0x41a00000:family==2?0x41700000:family==3?0x41c80000:0x420c0000);
    if (family==1) call("obj_delete",{static_cast<Raw>(c)});if (family==4 && (ev&4)) write(c+12,4,u32(c+12)|1);break;
  case 23:case 24:
    if (family==2 && state==23) {movement(c,false,{});if (!anim_flags(c,0x10)) return st(3);}
    else if (family==3) {movement(c,false,{});if (!anim_flags(c,8)) return st(3);}
    else if (family<=2) return st(0);break;
  default:if (family<=2) return st(0);break;
  }
  return {};
}
Raw Model::spell_random(Address s) {auto seed=u32(s+0x10)*0x19660du+0x3c6ef35fu;write(s+0x10,4,seed);return seed>>16;}
Vec3 Model::spell_ball(Address s) {
  for (unsigned tries=0;tries<4096;++tries) {Vec3 r;for (auto &v:r) {auto u=mul(from_int(spell_random(s)),0x37800000);v=sub(add(u,u),ONE);}if (!lt(ONE,add(add(mul(r[0],r[0]),mul(r[1],r[1])),mul(r[2],r[2])))) return r;}
  throw Unsupported("spell rejection budget");
}
void Model::spell_run(Address s,Raw fn) {
  Depth guard(depth_);
  if (fn==0x2e177c) {if (!u32(s+0x18) && s) table_service("Spell",s,0x14,1,{3u});}
  else tagar_run(s);
}
bool Model::tagar_valid(Address s,Raw obj,bool marker) {
  if (!obj || (u32(static_cast<Address>(obj)+12)&9)) return false;
  if (!dispatch(u32(s+12),20,{obj}).value_or(0)) return false;
  return !marker || !call("creature_findSpell",{obj,0x57u,u32(s+12)});
}
Raw Model::tagar_pick(Address s,bool later) {
  auto swarm=vec(s+0x88);auto heading=static_cast<Raw>(i16(s+0x84));
  auto found=later?service("world_collectInSphere",{swarm,heading,0x42f00000u}):service("world_collectObjectsInArc",{swarm,heading,u32(s+12),0x42f00000u,0x42b40000u,0u});
  Raw best=0,best_d=0x49742400;
  if (found.kind!=creature_movement::ValueKind::None) {
    if (found.kind!=creature_movement::ValueKind::List && found.kind!=creature_movement::ValueKind::Tuple) throw Unsupported("swarm query list shape");
    for (const auto &v:found.items) {
      if (v.kind!=creature_movement::ValueKind::Integer) throw Unsupported("swarm query lane");Raw obj=static_cast<Raw>(v.scalar);if (!tagar_valid(s,obj,true)) continue;
      bool seen=false;for (int k=0;k<std::max(i16(s+0x82),0);++k) if (u32(s+0x18+4*k) && handle(s+0x18+4*k)==obj) {seen=true;break;}if (seen) continue;
      if (!later) {call_value("handle_clear",{std::string("stack")});return obj;}
      auto d=vec(s+0x88),p=vec(static_cast<Address>(obj)+0x10);for (unsigned k=0;k<3;++k) d[k]=sub(d[k],p[k]);auto dist=add(add(mul(d[0],d[0]),mul(d[1],d[1])),mul(d[2],d[2]));if (lt(dist,best_d)) {best=obj;best_d=dist;}
    }
  }
  call_value("handle_clear",{std::string("stack")});return best;
}
bool Model::tagar_victims(Address s) {
  int active=0;
  for (int i=0;i<i16(s+0x82);++i) {
    auto victim=handle(s+0x18+4*i);Address slot=s+0x38+4*i;if (!victim || lt(u32(slot),0)) continue;
    if (le(u32(slot),u32(s+0x78)) || !tagar_valid(s,victim,false)) {
      auto marker=pointer_result(call("creature_findSpell",{victim,0x57u,u32(s+12)}));if (marker) table_service("Spell",marker,0x14,1,{3u});write(slot,4,MINUS_ONE);continue;
    }
    write(static_cast<Address>(victim)+0x98,2,100);write(static_cast<Address>(victim)+0x9e,2,4);write(static_cast<Address>(victim)+0x9a,2,0);write(static_cast<Address>(victim)+0x9c,2,0);++active;
    Address tick=s+0x58+4*i;
    if (le(u32(tick),u32(s+0x78))) {
      auto owner=u32(s+12),level=static_cast<Raw>(i8(s)),skill=u32(s+8);Raw low,high;
      if (owner && (u32(static_cast<Address>(owner)+12)&0x40)) {
        auto value=table_service("Skill",skill,0x60,8,{1u,level,0u,0x3e4cccccu},true);
        if (value.kind!=creature_movement::ValueKind::Tuple || value.items.size()!=2 || value.items[0].kind!=creature_movement::ValueKind::Integer || value.items[1].kind!=creature_movement::ValueKind::Integer) throw Unsupported("swarm skill range shape");low=static_cast<Raw>(value.items[0].scalar);high=static_cast<Raw>(value.items[1].scalar);
      } else {
        auto v=table_service("Skill",skill,0x60,12,{1u,level,owner,0u},true);if (v.kind!=creature_movement::ValueKind::Integer) throw Unsupported("swarm skill integer");auto value=from_int(static_cast<Raw>(v.scalar)),spread=mul(value,0x3e4ccccc);high=static_cast<Raw>(to_int(add(value,spread)));low=static_cast<Raw>(to_int(sub(value,spread)));
      }
      call("damage_apply",{victim,u32(s+12),100000u,0u,0u,0x4000000u,0u,0u,from_int(low),from_int(high)});write(tick,4,add(u32(tick),ONE));
    }
    for (unsigned j=0;j<5;++j) {
      auto frame=u32(0x44fdcc),sign=j&1?MINUS_ONE:ONE;auto r=spell_ball(s);Vec3 velocity;for (unsigned k=0;k<3;++k) velocity[k]=mul(r[k],0x40800000);
      auto swirl=(frame+j*0x2aaa)<<9;auto px=mul(mul(sign,u32(static_cast<Address>(victim)+0x1c)),cos16(swirl));auto py=mul(mul(sign,u32(static_cast<Address>(victim)+0x1c)),sin16(swirl));auto bob=sin16((frame-j*0x2aaa)<<10);auto pz=mul(mul(sign,bob),mul(u32(static_cast<Address>(victim)+0x20),0x3e800000));
      px=add(px,u32(static_cast<Address>(victim)+0x10));py=add(py,u32(static_cast<Address>(victim)+0x14));pz=add(add(pz,u32(static_cast<Address>(victim)+0x18)),mul(u32(static_cast<Address>(victim)+0x20),HALF));call("particle_spawn",{0x8e7598u,Vec3{px,py,pz},velocity,0u});
    }
  }return active>0;
}
void Model::tagar_run(Address s) {
  auto active=tagar_victims(s);auto clock=add(u32(s+0x78),0x3c888888);write(s+0x78,4,clock);
  if (le(u32(s+0x7c),clock)) {if (!active && s) table_service("Spell",s,0x14,1,{3u});return;}
  auto skill=u32(s+8);auto stat=[&](Raw sk,std::vector<Argument> args) {auto v=table_service("Skill",sk,0x60,12,std::move(args),true);if (v.kind!=creature_movement::ValueKind::Integer) throw Unsupported("swarm stat result");return s32(static_cast<Raw>(v.scalar));};
  auto limit=std::min(8,stat(skill,{0u,static_cast<Raw>(i8(s)),u32(s+12),10u}));if (!(i16(s+0x82)<limit)) return;
  Raw target=0;auto current=i16(s+0x80);
  if (current>=0) {auto node=u32(s+0x18+4*current);if (node) target=u32(static_cast<Address>(node)+12);if (!node || !tagar_valid(s,target,false)) write(s+0x80,2,0xffff);}
  if (i16(s+0x80)<0) {
    target=tagar_pick(s,i16(s+0x82)>0);
    if (target) {
      auto count=static_cast<Raw>(mem.load(s+0x82,2));write(s+0x80,2,count);call("handle_link",{static_cast<Raw>(s+0x18+4*s16(count)),target,target});
      auto dy=sub(u32(static_cast<Address>(target)+0x14),u32(s+0x8c)),dx=sub(u32(static_cast<Address>(target)+0x10),u32(s+0x88));auto dist=sqrt(add(mul(dx,dx),mul(dy,dy)));write(s+0x38+4*i16(s+0x80),4,add(u32(s+0x78),mul(dist,0x3ba1605b)));
    }
  }
  Raw speed=0x434b0da1;
  if (target) {
    auto dx=sub(u32(static_cast<Address>(target)+0x10),u32(s+0x88)),dy=sub(u32(static_cast<Address>(target)+0x14),u32(s+0x8c));
    if (lt(ONE,add(mul(dx,dx),mul(dy,dy)))) {auto error=atan2_16(dy,dx)-i16(s+0x84);if (error>=0x4000) error=0x7fff-error;else if (error<-0x3fff) error=-0x8000-error;write(s+0x84,2,mem.load(s+0x84,2)+error/5);}
    dx=sub(u32(static_cast<Address>(target)+0x10),u32(s+0x88));dy=sub(u32(static_cast<Address>(target)+0x14),u32(s+0x8c));Address slot=s+0x38+4*i16(s+0x80);auto dist=sqrt(add(mul(dx,dx),mul(dy,dy))),eta=add(u32(s+0x78),mul(dist,0x3ba1605b));if (lt(eta,u32(slot))) write(slot,4,eta);
    clock=u32(s+0x78);auto left=sub(u32(slot),clock);speed=div(dist,left);
    if (lt(0x3c888888,left)) {auto z=u32(s+0x90),gap=sub(add(u32(static_cast<Address>(target)+0x18),0x42400000),z);write(s+0x90,4,add(z,div(gap,mul(left,0x42700000))));}
    if (le(u32(slot),add(clock,0x3d088888))) {
      write(s+0x82,2,mem.load(s+0x82,2)+1);auto seconds=stat(u32(s+8),{2u,static_cast<Raw>(i8(s)),u32(s+12),10u});current=i16(s+0x80);write(s+0x38+4*current,4,add(from_int(seconds),0x3d088888));write(s+0x58+4*current,4,u32(s+0x78));
      auto marker=pointer_result(call("pool_new",{0x20u}));
      if (marker) {auto owner=u32(s+12);write(marker,1,1);write(static_cast<Address>(marker)+0x14,4,0x515ba0);write(static_cast<Address>(marker)+4,4,0);write(static_cast<Address>(marker)+12,4,target);write(static_cast<Address>(marker)+8,4,0);write(static_cast<Address>(marker)+0x10,4,shared_draw());write(static_cast<Address>(marker)+0x14,4,0x51ba90);write(static_cast<Address>(marker)+0x18,4,owner);write(static_cast<Address>(marker)+0x1c,4,0x57);}
      call("creature_addSpell",{target,marker});speed=0x434b0da1;auto entry=static_cast<Raw>(call("res_find",{0x552428u,0x552460u}));call("sound_playAtPos",{entry,vec(static_cast<Address>(target)+0x10),ONE});write(s+0x80,2,0xffff);write(s+0x7c,4,add(u32(s+0x7c),0x3f0041aa));
    }
  }
  if (spell_random(s)<0x4906) {auto turn=sub(mul(from_int(spell_random(s)),0x37800000),HALF);write(s+0x86,2,static_cast<Raw>(to_int(mul(turn,0x45f81000))));}
  auto angle=s16(static_cast<Raw>(mem.load(s+0x84,2)+mem.load(s+0x86,2)));write(s+0x88,4,add(u32(s+0x88),mul(mul(cos16(angle),speed),0x3c888888)));
  auto sine=sin16(angle),grow=mul(u32(s+0x78),0x40226fa7);write(s+0x8c,4,add(u32(s+0x8c),mul(mul(sine,speed),0x3c888888)));if (lt(ONE,grow)) grow=ONE;
  for (unsigned i=0;i<50;++i) {
    auto r=spell_ball(s);auto k=mul(mul(grow,0x436d90b9),0x3c888888);Vec3 velocity;for (unsigned j=0;j<3;++j) velocity[j]=mul(r[j],k);
    auto p=spell_ball(s);auto inverse=rsqrt(ONE,add(add(mul(p[0],p[0]),mul(p[1],p[1])),mul(p[2],p[2])));for (auto &v:p) v=mul(v,inverse);
    auto reach=mul(mul(grow,0x4151d268),mul(from_int(spell_random(s)),0x37800000));for (unsigned j=0;j<3;++j) p[j]=add(mul(p[j],reach),u32(s+0x88+4*j));
    auto kind=spell_random(s);call("particle_spawn",{kind>=0x7d01?0x8e7598u:kind>=0x3a99?0x8e75e8u:0x8e7638u,p,velocity,0u});
  }
  auto side=neg(sin16(i16(s+0x84))),spin=u32(0x44fdcc)<<10;auto px=mul(mul(side,0x3f0f56fb),mul(cos16(spin),0x4151d268));auto ahead=mul(cos16(i16(s+0x84)),0x3f0f56fb);auto py=mul(ahead,mul(cos16(u32(0x44fdcc)<<10),0x4151d268));auto pz=mul(mul(sin16(u32(0x44fdcc)<<10),0x4151d268),0x3f0f56fb);
  call("particle_spawn",{0x8e7688u,Vec3{add(px,u32(s+0x88)),add(py,u32(s+0x8c)),add(pz,u32(s+0x90))},Vec3{},0u});
}
// Skelight lifecycle: authored from pinned ref_arena_monsters.py, reached by
// ref_monsters_1.Model.vcall. The holder remains a delegated host service.
void Model::init_weapon_holder(Address holder) {
  write(holder,4,0x512148);write(holder+4,1,0);
  write(holder,4,0x50cf40);write(holder+8,1,0);
  auto block=holder+0x50;
  for (unsigned k=0;k<4;++k) write(block+0x24+8*k,2,0xffff);
  write(block+0x21,1,0xff);write(block+0x20,1,0xff);
  for (unsigned k=0;k<4;++k) {write(block+0x10+2*k,2,0);write(block+0x18+2*k,2,0);}
}
Argument Model::weapon_descriptor(Address c) {
  auto holder=u32(c+0x50);if (!holder) return Raw{0};
  auto table=u32(holder);
  auto self=static_cast<Raw>(holder+i16(static_cast<Address>(table)+8*17));
  auto value=call_value("Holder.v17",{self,1u});
  if (auto p=std::get_if<std::uint64_t>(&value)) return static_cast<Raw>(*p);
  if (auto p=std::get_if<Structured>(&value)) return *p;
  if (auto p=std::get_if<BufferResult>(&value)) return TaggedTuple{"buf",{p->payload}};
  throw Unsupported("holder descriptor compound shape");
}
std::optional<std::uint64_t> Model::skelight_vcall(Address c,unsigned slot,std::vector<Argument> args) {
  SkelightScope domain(skelight_domain_);creature_span(c);
  if (slot>37) throw Unsupported("Skelight virtual slot domain");
  return dispatch(c,slot,std::move(args));
}
void Model::skelight_shatter(Address,Raw) {write(0x8e4560,8,0);}
void Model::skelight_ctor(Address c,Vec3 pos,Raw facing,Raw props,Raw level) {
  Depth guard(depth_);SkelightScope domain(skelight_domain_);skelight_span(c);
  creature_ctor(c,0,pos,facing,0x502728,props,0x41200000);
  auto holder=c+0x3c8;write(c+0x48,4,0x5174b0);init_weapon_holder(holder);
  write(c+0x28,1,0x14);write(c+0xb0,4,0x3fc00000);write(c+0x1c,4,0x41400000);
  write(c+0xb8,4,0);write(c+0x232,2,0);write(c+0xec,4,u32(0x44fdcc));
  write(c+0xa8,8,flags(c)|0x10000);write(c+0x68,4,ONE);write(c+0x64,4,ONE);
  write(c+0x50,4,holder);table_service("Holder",holder,0,1,{1u,4u});
  write(c+0x460,4,level);auto saved=u32(0x4f714c);write(0x4f714c,4,from_int(level));
  auto lower=[&](Address a) {
    std::string bytes;
    for (unsigned k=0;k<64;++k) {auto b=mem.load(a+k,1);if (!b) break;
      if (b>='A' && b<='Z') b+='a'-'A';bytes.push_back(static_cast<char>(b));}
    return bytes;
  };
  auto same=lower(0x44fe18)==lower(0x54c080);
  call("Creature_setStats",{static_cast<Raw>(c),1u,0x3faa3d70u,0x3f999999u,0x3f999999u,ONE,same?0u:ONE});
  write(0x4f714c,4,saved);write(c+0xe8,4,0);
  write(c+0x220,8,call("model_buildHideMask",{u32(static_cast<Address>(u32(c+0x24))+12),0u}));
  call("anim_clear",{static_cast<Raw>(c+0x114)});
  call("anim_push",{static_cast<Raw>(c+0x114),u32(static_cast<Address>(u32(c+0x24))+0x3c),0xcu});
  skelight_set_state(c,23);call("obj_runEveryFrame",{static_cast<Raw>(c)});
  event_params(c,3,{1,0,0,0},{0x3e999999,0,0,0});
  event_params(c,4,{1,1,0,0},{0x3e999999,ONE,0,0});
  call("Creature_init",{static_cast<Raw>(c)});write(c+0xb4,1,0x10);
  write(c+0xa8,8,flags(c)|UINT64_C(0x8000000000000000));
}
void Model::skelight_set_state(Address c,Raw requested) {
  Depth guard(depth_);SkelightScope domain(skelight_domain_);skelight_span(c);
  auto [refused,state]=filter(c,requested);if (refused) return;
  auto walk=[&] {start(c,0x44,0x803,0x3dcccccc);call("anim_syncToSimFrame",{static_cast<Raw>(c+0x114)});};
  switch(state) {
  case 0:start(c,0x34,3,0x3dcccccc);write(c+0xf4,4,u32(c+0x300)+60);break;
  case 1:clear_path(c);wander(c);walk();write(c+0xf4,4,u32(c+0x300)+300);break;
  case 3:case 16:walk();break;
  case 5:case 19:start(c,0x34,3,0x3dcccccc);break;
  case 6: {auto pick=random(c)&1;swing(c,0x1c+8*pick,0x808);write(c+0xf4,4,1);break;}
  case 18:state=i16(c+0x232);break;
  case 20: {
    write(c+12,4,u32(c+12)|2);write(c+0xa8,8,flags(c)|UINT64_C(0x20000000000));skelight_shatter(c,8);
    auto model=[&] {return static_cast<Raw>(dispatch(c,16).value_or(0));};
    auto count=s32(static_cast<Raw>(call("model_getSamplePointCount",{model()})));
    for (std::int64_t index=0;index<count;index+=3) {
      if (index/3>=4096) throw Unsupported("Skelight sample iteration budget");
      auto sample=service("model_getSamplePoint",{model(),static_cast<Raw>(c+0x114),vec(c+0x10),static_cast<Raw>(i16(c+0x38)),static_cast<Raw>(index),1u,0u,ONE});
      namespace V=creature_movement;
      if (sample.kind!=V::ValueKind::Tuple || sample.items.size()!=2) throw Unsupported("Skelight sample result shape");
      auto point=sample.items[0];
      if (point.kind==V::ValueKind::List) point=Structured::tuple(point.items);
      if (point.kind!=V::ValueKind::Vector && point.kind!=V::ValueKind::Tuple) throw Unsupported("Skelight sample point shape");
      call("fx_spawn",{2u,point});
    }
    call("obj_delete",{static_cast<Raw>(c)});break;
  }
  case 23:break;
  default:state=0;start(c,0x34,3,0x3dcccccc);break;
  }
  write(c+0x232,2,static_cast<Raw>(state));
}
std::optional<std::uint64_t> Model::skelight_run(Address c) {
  Depth guard(depth_);SkelightScope domain(skelight_domain_);skelight_span(c);
  auto model=[&] {return u32(static_cast<Address>(u32(c+0x24))+12);};
  if (i16(c+0x232)!=23) process_damage(c,model());
  creature_run(c,model());update_active(c); // Skelight deliberately ignores activity.
  auto holder=u32(c+0x50);Raw row=0;
  if (holder) row=static_cast<Raw>(call("Holder.v15",{static_cast<Raw>(holder+i16(static_cast<Address>(u32(holder))+8*15)),1u}))*16u;
  holder=u32(c+0x50);auto item=u32(static_cast<Raw>(0x3e9ce0u+row+8));
  if (holder) table_service("Holder",holder,0,22,{item,static_cast<Raw>(c),HALF});
  auto ev=step_anim(c,ONE);call("Creature_applyKnockback",{static_cast<Raw>(c),0x3f333333u});dispatch(c,23,{0u});
  auto st=[&](Raw state) {return dispatch(c,27,{state});};
  auto state=i16(c+0x232);
  switch(state) {
  case 0:if (dispatch(c,25,{1u}).value_or(0)) return st(3);if (away(c)) return st(1);break;
  case 1:if (dispatch(c,25,{1u}).value_or(0)) return st(3);move_to_dest(c);break;
  case 3:if (!u32(c+0x234)) return st(0);if (see(c,6000,0x42400000)) return st(6);
    if (see(c,0x7fff,0x42100000,0,0)) turn_to_target(c);else move_to_target(c);break;
  case 5:if (see(c,6000,0x42400000)) return st(6);if (!i16(c+0x314)) return st(3);break;
  case 6: {
    bool count=true;
    if (u32(c+0x234)) {
      if (ev&1) {auto desc=weapon_descriptor(c);call("Creature_hitTargetInArc",{static_cast<Raw>(c),static_cast<Raw>(i16(c+0x38)),6000u,static_cast<Raw>(i16(c+0xd0)),static_cast<Raw>(i16(c+0xd2)),static_cast<Raw>(i16(c+0xd4)),8u,desc,0u,0x42400000u});}
      if (anim_flags(c,8)) {turn_to_target(c);count=!anim_flags(c,8);}
    } else count=!anim_flags(c,8);
    if (count) write(c+0xf4,4,u32(c+0xf4)-1);
    if (s32(u32(c+0xf4))<=0) return st(3);break;
  }
  case 16:if (!u32(c+0x234)) write(c+0x318,2,0);if (!i16(c+0x318)) return st(0);flee(c,0);break;
  case 18:return st(3);
  case 19:if (!i16(c+0x31c)) return st(0);break;
  case 20:dead_update(c,ev,1,0x41a00000);break;
  case 23:if (anim_flags(c,0x80)) st(0);call("fx_spawn",{7u,vec(c+0x10)});break;
  default:break;
  }
  return {};
}
// Complete pinned arena cast family. Service callbacks remain host-owned.
Raw Model::cosmetic_draw() {
  auto seed=u32(0x404460)*0x19660du+0x3c6ef35fu;write(0x404460,4,seed);return seed>>16;
}
Vec3 Model::cosmetic_ball() {
  for (unsigned tries=0;tries<4096;++tries) {
    Vec3 r;for (auto &v:r) {auto m=mul(from_int(cosmetic_draw()),TWO_M16);v=sub(add(m,m),ONE);}
    if (!lt(ONE,add(add(mul(r[0],r[0]),mul(r[1],r[1])),mul(r[2],r[2])))) return r;
  }
  throw Unsupported("cosmetic ball retry budget");
}
Vec3 Model::summon_sample(Address caster,std::int32_t count) {
  auto model=static_cast<Raw>(dispatch(caster,16).value_or(0));
  auto divisor=count<0?-static_cast<std::int64_t>(count):static_cast<std::int64_t>(count);
  auto index=cosmetic_draw()%divisor;
  auto sample=service("model_getSamplePoint",{model,static_cast<Raw>(caster+0x114),vec(caster+0x10),static_cast<Raw>(i16(caster+0x38)),static_cast<Raw>(index),0u,0u,ONE});
  namespace V=creature_movement;
  auto point=sample;
  if (point.kind==V::ValueKind::Vector) return point.vec;
  if ((point.kind==V::ValueKind::Tuple || point.kind==V::ValueKind::List) && point.items.size()==3) {
    Vec3 result;for (unsigned k=0;k<3;++k) {
      if (point.items[k].kind!=V::ValueKind::Integer || point.items[k].scalar<0 || point.items[k].scalar>UINT32_MAX) throw Unsupported("summon sample coordinate");
      result[k]=static_cast<Raw>(point.items[k].scalar);
    }return result;
  }
  throw Unsupported("summon sample point shape");
}
Raw Model::arena_skill_call(Address skill,unsigned slot,std::vector<Argument> args) {
  Depth guard(depth_);SkelightScope arena(arena_cast_domain_),skeleton(skelight_domain_);
  if (skill>UINT32_MAX-0x63u || slot>14) throw Unsupported("arena skill raw32 span or slot");
  auto value=table_service("Skill",skill,0x60,slot,std::move(args),true);
  if (value.kind!=creature_movement::ValueKind::Integer) throw Unsupported("arena skill return shape");
  return static_cast<Raw>(value.scalar);
}
Raw Model::summon_can_use(Address,Address caster) {
  Depth guard(depth_);creature_span(caster);Raw name;
  if (u32(caster+12)&0x40) {
    auto slot=u32(caster+0x3c8);if (slot>=4) return 0;name=u32(0x503778+4*slot);
  } else name=u32(0x503788);
  return call("world_findObjectByName",{name})?0:1;
}
void Model::spell_base_ctor(Address spell,Raw level,Raw owner,Raw skill,Raw table) {
  if (spell>UINT32_MAX-0x17u) throw Unsupported("spell raw32 span");
  write(spell,1,level);write(spell+0x14,4,0x515ba0);write(spell+12,4,owner);
  write(spell+4,4,0);write(spell+8,4,skill);write(spell+0x10,4,shared_draw());write(spell+0x14,4,table);
}
void Model::summon_spell_ctor(Address spell,Raw level,Raw caster,Raw skill,Raw skeleton) {
  if (spell>UINT32_MAX-0x1bu) throw Unsupported("summon spell raw32 span");
  spell_base_ctor(spell,level,caster,skill,0x519918);write(spell+0x18,4,0);
  if (skeleton) call("handle_link",{static_cast<Raw>(spell+0x18),skeleton,skeleton});
}
void Model::tagar_spell_ctor(Address spell,Raw level,Raw owner,Raw skill) {
  if (spell>UINT32_MAX-0x93u) throw Unsupported("Tagar spell raw32 span");
  spell_base_ctor(spell,level,owner,skill,0x51ba60);
  for (unsigned k=0;k<8;++k) write(spell+0x18+4*k,4,0);
  write(spell+0x80,2,0xffff);write(spell+0x82,2,0);write(spell+0x7c,4,0x4000101f);write(spell+0x78,4,0);
  owner=u32(spell+12);wvec(spell+0x88,vec(static_cast<Address>(owner)+0x10));write(spell+0x86,2,0);
  write(spell+0x84,2,mem.load(static_cast<Address>(u32(spell+12))+0x38,2));
  write(spell+0x88,4,add(u32(spell+0x88),mul(cos16(i16(spell+0x84)),0x41c00000)));
  auto y=add(u32(spell+0x8c),mul(sin16(i16(spell+0x84)),0x41c00000));
  write(spell+0x90,4,add(u32(spell+0x90),0x42400000));write(spell+0x8c,4,y);
}
Raw Model::tagar_cast(Address skill,Raw level,Address caster,Vec3 pos,Raw) {
  Depth guard(depth_);SkelightScope arena(arena_cast_domain_),skeleton(skelight_domain_);
  auto result=table_service("Skill",skill,0x60,14,{level,0x14u},true);
  if (result.kind!=creature_movement::ValueKind::Integer) throw Unsupported("Tagar level result shape");
  level=static_cast<Raw>(result.scalar);
  auto sound=pointer_result(call("res_find",{0x552428u,0x552430u}));call("sound_playAtPos",{sound,pos,ONE});
  auto spell=pointer_result(call("pool_new",{0x94u}));if (spell) tagar_spell_ctor(spell,level,static_cast<Raw>(caster),static_cast<Raw>(skill));return spell;
}
Raw Model::summon_cast(Address skill,Raw level,Address caster,Vec3 pos,Raw facing) {
  Depth guard(depth_);SkelightScope arena(arena_cast_domain_),skeleton(skelight_domain_);creature_span(caster);
  facing=static_cast<Raw>(s16(facing));
  write(caster+0x9a,2,0xff92);write(caster+0x9c,2,0xff80);write(caster+0x9e,2,0x1e);write(caster+0x98,2,0xff80);
  auto count=s32(static_cast<Raw>(call("model_getSamplePointCount",{static_cast<Raw>(dispatch(caster,16).value_or(0))})));
  if (count) {
    for (unsigned k=0;k<32;++k) {
      auto point=summon_sample(caster,count);auto r=cosmetic_ball();
      for (unsigned j=0;j<3;++j) point[j]=add(point[j],mul(r[j],0x3e72af9a));
      r=cosmetic_ball();for (auto &v:r) v=mul(v,0x3d8b4634);
      call("particle_spawn",{0x8e5c80u,point,r,0u});
    }
    for (unsigned k=0;k<200;++k) {
      auto point=summon_sample(caster,count);auto r=cosmetic_ball();for (auto &v:r) v=mul(v,0x41c000e5);
      call("particle_spawn",{0x8e5cd0u,point,r,0u});
    }
  }
  auto sx=add(pos[0],mul(cos16(facing),0x42900000)),sy=add(pos[1],mul(sin16(facing),0x42900000));
  auto sz=static_cast<Raw>(call("terrain_height",{u32(0x44fdc8),Vec3{sx,sy,pos[2]},0x41400000u}));
  Vec3 spot{sx,sy,sz};
  auto delta=sub(sz,u32(caster+0x18))&0x7fffffffu;
  // Python promotes the already-rounded float to double for abs(x)>24.
  if (caster && delta>0x41c00000 && delta<=0x7f800000) {
    auto result=service("world_findFreeSpot",{vec(caster+0x10),0x48u,0xcu,0u});
    namespace V=creature_movement;
    if (result.kind!=V::ValueKind::Tuple || result.items.size()!=2 || result.items[0].kind!=V::ValueKind::Vector || result.items[1].kind!=V::ValueKind::Integer) throw Unsupported("summon free-spot shape");
    if (signed8(static_cast<std::uint64_t>(result.items[1].scalar))) spot=result.items[0].vec;
  }
  auto d1=mul(from_int(shared_draw()),TWO_M16);
  auto stat=[&](Raw which) {
    auto v=table_service("Skill",skill,0x60,12,{which,level,static_cast<Raw>(caster),10u},true);
    if (v.kind!=creature_movement::ValueKind::Integer) throw Unsupported("summon stat result shape");return static_cast<Raw>(v.scalar);
  };
  auto strength=stat(0);Raw props;
  if (!(u32(caster+12)&0x40)) props=0x503770;
  else if (u32(caster+0x3c8)<4) props=0x503750+8*u32(caster+0x3c8);
  else props=0;
  auto skel=pointer_result(call("pool_new",{0x468u}));
  if (skel) {
    skelight_ctor(skel,spot,facing,props,strength);write(skel+12,4,u32(skel+12)|0x4000);
    auto chance=stat(1);
    if (lt(d1,mul(from_int(chance),0x3c23d70a))) {
      auto pick=shared_draw()%3;auto holder=u32(skel+0x50);
      if (holder) table_service("Holder",holder,0,2+2*pick,{0u});
    }
    if ((u32(caster+12)&0x40) && s32(level)>0) {
      write(skel+0x5a,1,mem.load(caster+0x3c8,1));write(skel+0xa8,8,flags(skel)|0x400000);
      call("Creature_setCharm",{skel,UINT32_MAX,static_cast<Raw>(dispatch(caster,37).value_or(0))});
      write(skel+0x3ac,2,400);dispatch(skel,34,{500u});
    }
  }
  auto sound=pointer_result(call("res_find",{0x54fdd0u,0x54fdd8u}));call("sound_playAtPos",{sound,pos,ONE});
  if (!skel) return 0;
  auto spell=pointer_result(call("pool_new",{0x1cu}));if (spell) summon_spell_ctor(spell,level,static_cast<Raw>(caster),static_cast<Raw>(skill),skel);return spell;
}
// Horsemen lifecycle and inherited target predicates, pinned arena contract.
namespace {
void horsemen_span(Address c) {
  if (c>UINT32_MAX-0x477u) throw Unsupported("Horsemen raw32 span");
}
}
std::optional<std::uint64_t> Model::horsemen_vcall(Address c,unsigned slot,std::vector<Argument> args) {
  SkelightScope horse(horsemen_domain_),arena(arena_cast_domain_),skeleton(skelight_domain_);
  creature_span(c);if (slot>37) throw Unsupported("Horsemen virtual slot domain");
  return dispatch(c,slot,std::move(args));
}
bool Model::target_charging(Address c) {
  auto node=u32(c+0x234);if (!node) return false;
  auto t=u32(static_cast<Address>(node)+12);
  auto bearing=atan2_16(sub(u32(t+0x14),u32(c+0x14)),sub(u32(t+0x10),u32(c+0x10)));
  t=u32(static_cast<Address>(u32(c+0x234))+12);
  if (static_cast<Raw>(bearing-i16(t+0x38)+0x3fff)>=0x7fff) return false;
  auto vy=u32(t+0x30c),vx=u32(t+0x308);
  return lt(0x42c80000,sqrt(add(mul(vx,vx),mul(vy,vy))));
}
std::uint64_t Model::allies_near_target(Address c,Raw radius) {
  auto node=u32(c+0x234);if (!node) return 0;
  return call("creature_countAlliesNear",{static_cast<Raw>(c),vec(static_cast<Address>(u32(static_cast<Address>(node)+12))+0x10),radius});
}
std::uint64_t Model::blocked_by_target(Address c) {
  auto node=u32(c+0x234);if (!node) return 0;
  return call("creature_isBlockedBy",{static_cast<Raw>(c),u32(static_cast<Address>(node)+12)});
}
void Model::horsemen_cast(Address c) {
  Depth guard(depth_);SkelightScope horse(horsemen_domain_),arena(arena_cast_domain_),skeleton(skelight_domain_);horsemen_span(c);
  auto variant=i8(c+0x470);if (variant<0 || variant>3) return;
  auto level=static_cast<Raw>(dispatch(c,35).value_or(0));
  if (variant==0) {
    auto lift=mul(u32(c+0x20),HALF);
    Vec3 pos{add(u32(c+0x10),0),u32(c+0x14),add(u32(c+0x18),lift)};
    arena_skill_call(u32(0x41b180),2,{level,static_cast<Raw>(c),pos,static_cast<Raw>(i16(c+0x38))});
  } else if (variant==1) {
    auto target=u32(static_cast<Address>(u32(c+0x234))+12);
    arena_skill_call(u32(0x41b18c),2,{level,static_cast<Raw>(c),vec(target+0x10),static_cast<Raw>(i16(c+0x38))});
  } else {
    auto spell=arena_skill_call(u32(variant==2?0x41b2cc:0x41b19c),2,{level,static_cast<Raw>(c),vec(c+0x10),static_cast<Raw>(i16(c+0x38))});
    if (variant==2) call("creature_addSpell",{static_cast<Raw>(c),spell});
  }
}
void Model::horsemen_set_state(Address c,Raw requested) {
  Depth guard(depth_);SkelightScope horse(horsemen_domain_),arena(arena_cast_domain_),skeleton(skelight_domain_);horsemen_span(c);
  auto [refused,state]=filter(c,requested);if (refused) return;
  auto mount=u32(c+0x3c8);write(c+0xa8,8,flags(c)&UINT64_C(0xfffffffffffffdff));
  if (mount && state<25) return;
  switch(state) {
  case 0:
    write(c+12,4,u32(c+12)&0xfffffff6u);{auto draw=random(c);write(c+0xf4,4,u32(c+0x300)+draw%120);}
    start(c,0x54,3);if (u32(c+0x3c8)) {call("handle_clear",{static_cast<Raw>(c+0x3c8)});write(c+0x3c8,4,0);}break;
  case 1:case 3:case 16:clear_path(c);if (state==1) wander(c);start(c,0x6c,0x803);call("anim_syncToSimFrame",{static_cast<Raw>(c+0x114)});break;
  case 5:start(c,0x3c,3);break;
  case 28:start(c,0xd4,3);break;case 29:start(c,0xcc,3);break;case 30:start(c,0xbc,3);break;case 31:start(c,0xc4,3);break;
  case 6:swing(c,0x1c,0x808);break;case 7:swing(c,0x24,0x808);break;case 8:swing(c,0x74,0x808);break;
  case 10:swing(c,0x2c,0x808);break;case 11:swing(c,0x34,0x808);break;case 23:swing(c,0x44,0x808);break;case 24:swing(c,0x64,0x808);break;
  case 32:swing(c,0x8c,8);break;case 33:swing(c,0x94,8);break;case 34:swing(c,0x9c,8);break;
  case 17:write(c+0xa8,8,flags(c)|0x200);start(c,0x3c,3);break;
  case 18:if ((i16(c+0x232)==6 && anim_flags(c,8)) || mount) state=i16(c+0x232);else swing(c,0x4c,0x10);break;
  case 19:if (mount) state=i16(c+0x232);else start(c,0x54,3,0x3e999999);break;
  case 20: {
    write(c+0xa8,8,flags(c)|UINT64_C(0x20000000000));
    if (i8(c+0x470)==3) {auto pet=pointer_result(call("world_findObjectByName",{0x547420u}));if (pet && (u32(pet+12)&0x20)) dispatch(pet,27,{20u});}
    call("Creature_dropLoot",{static_cast<Raw>(c),vec(c+0x10),static_cast<Raw>(i16(c+0x38)),0u,0x42100000u,0x40000000u,0x3fe66666u,0x3f199999u,0x40000000u,0x3e800000u});
    auto holder=u32(c+0x50);if (holder) {table_service("Holder",holder,0,11,{1u,static_cast<Raw>(c),0u,0u});holder=u32(c+0x50);}
    if (holder) table_service("Holder",holder,0,11,{2u,static_cast<Raw>(c),0u,0u});
    start(c,0x7c,6);dispatch(c,28,{0u});break;
  }
  case 25:write(c+12,4,u32(c+12)|0xb);start(c,0xa4,3);break;
  case 26:write(c+12,4,u32(c+12)&0xfffffffdu);start(c,0xa4,3);break;
  case 27:write(c+12,4,u32(c+12)|9);start(c,0xa4,3);break;
  case 35:start(c,0xb4,0x80c);if (u32(c+0x3c8)) {call("handle_clear",{static_cast<Raw>(c+0x3c8)});write(c+0x3c8,4,0);auto z=add(u32(c+0x18),0x42400000);write(c+0x3c,4,z);write(c+0x18,4,z);}break;
  default:dispatch(c,27,{3u});break;
  }
  write(c+0x232,2,static_cast<Raw>(state));
}
std::optional<std::uint64_t> Model::horsemen_run(Address c) {
  Depth guard(depth_);SkelightScope horse(horsemen_domain_),arena(arena_cast_domain_),skeleton(skelight_domain_);horsemen_span(c);
  auto holder=u32(c+0x50);if (holder) table_service("Holder",holder,0,21,{u32(c+0x24),static_cast<Raw>(c),HALF});
  bool mounted=u32(c+0x3c8)!=0;
  if (mounted) {
    auto mount=u32(static_cast<Address>(u32(c+0x3c8))+12);wvec(c+0x10,vec(mount+0x10));
    write(c+0x38,2,mem.load(static_cast<Address>(u32(static_cast<Address>(u32(c+0x3c8))+12))+0x38,2));
    call("obj_updateGridCell",{static_cast<Raw>(c)});
  }
  auto model=[&] {return u32(static_cast<Address>(u32(c+0x24))+12);};
  process_damage(c,model());creature_run(c,model());update_active(c);auto ev=step_anim(c,ONE);
  if (!mounted) {call("Creature_applyKnockback",{static_cast<Raw>(c),HALF});dispatch(c,23,{0u});}
  else {
    auto theirs=u32(static_cast<Address>(u32(static_cast<Address>(u32(c+0x3c8))+12))+0x234);
    if (theirs) {auto mine=u32(c+0x234);if (!mine || u32(static_cast<Address>(theirs)+12)!=u32(static_cast<Address>(mine)+12)) {
      call("handle_clear",{static_cast<Raw>(c+0x234)});
      auto target=u32(static_cast<Address>(u32(static_cast<Address>(u32(static_cast<Address>(u32(c+0x3c8))+12))+0x234))+12);
      call("handle_link",{static_cast<Raw>(c+0x234),target,target});
    }}
  }
  auto st=[&](Raw state) {return dispatch(c,27,{state});};
  auto state=i16(c+0x232);
  switch(state) {
  case 0:if (dispatch(c,25,{0u}).value_or(0)) return st(3);if (away(c) || s32(u32(c+0xf4))<s32(u32(c+0x300))) return st(1);break;
  case 1:if (dispatch(c,25,{0u}).value_or(0)) return st(0);move_to_dest(c);break;
  case 3:
    if (!u32(c+0x234)) return st(0);
    if (see(c,10000,0x42100000)) return st(5);
    if (see(c,6000,0x42100000) && target_charging(c)) return st(8);
    if (see(c,10000,0x43c80000) && s32(static_cast<Raw>(allies_near_target(c,0x42100000)))>0) return st(5);
    if (s32(u32(c+0x474))<s32(u32(c+0x300)) && mem.load(c+0x470,1)<3 && call("creature_findEnemyInCone",{static_cast<Raw>(c),static_cast<Raw>(i16(c+0x38)),6000u,0u,0x43c80000u})) return st(23);
    if (i8(c+0x470)==3 && arena_skill_call(u32(0x41b19c),1,{static_cast<Raw>(c)})) return st(23);
    move_to_target(c);break;
  case 5:
    if (i16(c+0x314)) {if (see(c,10000,0x42100000)) return st(6);break;}
    if (see(c,10000,0x42100000)) {if ((random(c)&1) && blocked_by_target(c)) return st(10);return st((random(c)&1)?6:7);}
    if (see(c,3000,0x42900000)) return st(11);
    if (random(c)%60 || s32(static_cast<Raw>(allies_near_target(c,0x42100000)))>0) break;
    return st(3);
  case 6:case 7:case 10:case 11:case 8:
    movement(c,false);
    if (u32(c+0x234)) {
      if (ev&1) {auto desc=weapon_descriptor(c);call("Creature_hitTargetInArc",{static_cast<Raw>(c),static_cast<Raw>(i16(c+0x38)),10000u,static_cast<Raw>(i16(c+0xd0)),static_cast<Raw>(i16(c+0xd2)),static_cast<Raw>(i16(c+0xd4)),state==8?0x40008u:8u,desc,0u,0x42100000u});}
      else if (state!=8 && (ev&2)) call("Creature_hitTargetInArc",{static_cast<Raw>(c),static_cast<Raw>(i16(c+0x38)),10000u,static_cast<Raw>(i16(c+0xd0)/3),static_cast<Raw>(i16(c+0xd2)/3),static_cast<Raw>(i16(c+0xd4)),0x100001u,0u,0u,0x42100000u});
      turn_to_target(c);
    }
    if (!anim_flags(c,8)) return st(3);break;
  case 16:if (!u32(c+0x234)) write(c+0x318,2,0);if (!i16(c+0x318)) return st(0);flee(c,0);break;
  case 17:case 24:if (!anim_flags(c,8)) return st(3);break;
  case 18:if (!anim_flags(c,0x10)) return st(3);break;
  case 19:if (!i16(c+0x31c)) return st(3);break;
  case 20:dead_update(c,ev,0,0x41a00000);break;
  case 23:
    if (u32(c+0x234)) {if (ev&1) {horsemen_cast(c);auto draw=random(c);write(c+0x474,4,u32(c+0x300)+draw%120+480);}turn_to_target(c);}
    if (!anim_flags(c,8)) return st(3);break;
  case 25:movement(c,false);break;
  case 26:if (i16(c+0x472)>0) write(c+0x472,2,mem.load(c+0x472,2)-1);break;
  case 27:case 28:case 29:case 30:break;
  case 31:case 32:case 33:case 34:
    if (u32(c+0x234) && (ev&1)) {auto desc=weapon_descriptor(c);call("Creature_hitAllEnemiesInArc",{static_cast<Raw>(c),static_cast<Raw>(s16(static_cast<Raw>(mem.load(c+0x38,2))-0x4000)),10000u,static_cast<Raw>(i16(c+0xd0)),static_cast<Raw>(i16(c+0xd2)),static_cast<Raw>(i16(c+0xd4)),8u,desc,0u,0x42100000u});}break;
  case 35: {
    movement(c,false);auto af=mem.load(c+0x11a,2);
    if ((af&0x98)==0x88) {write(c+0x118,1,0);write(c+0x11a,2,af&0xff7f);call("anim_push",{static_cast<Raw>(c+0x114),u32(static_cast<Address>(u32(c+0x24))+0x84),0x14u});}
    else if ((af&0x90)==0x90) start(c,0x54,3,0);
    if (!anim_flags(c,0x18)) return st(0);break;
  }
  default:return st(0);
  }
  return {};
}
void Model::horsemen_ctor(Address c,Raw type_name,Vec3 pos,Raw facing,Raw props,Raw variant,Raw mounted) {
  Depth guard(depth_);SkelightScope horse(horsemen_domain_),arena(arena_cast_domain_),skeleton(skelight_domain_);horsemen_span(c);
  creature_ctor(c,type_name,pos,facing,0x5006e8,props,0x41200000);
  auto holder=c+0x3d0;write(c+0x48,4,0x515730);write(c+0x3c8,4,0);init_weapon_holder(holder);
  write(holder+0x98,1,0);write(holder,4,0x50cda0);
  write(c+0x28,1,0x30);write(c+0x1c,4,0x41a00000);write(c+0xb8,4,0);write(c+0xb0,4,0x3f999999);write(c+0x232,2,0);write(c+0xb4,1,9);
  write(c+0xec,4,u32(0x44fdcc));write(c+0x50,4,holder);table_service("Holder",holder,0,1,{1u,(2*variant+0x1b)&0xffu});
  holder=u32(c+0x50);if (holder) table_service("Holder",holder,0,1,{2u,(2*variant+0x23)&0xffu});
  write(c+0xa8,8,flags(c)|UINT64_C(0x2020020000));write(c+0xdc,4,0x3f999999);write(c+0x472,2,0);
  write(c+0x5c,4,0x3e4ccccc);write(c+0x68,4,ONE);write(c+0x64,4,ONE);write(c+0x6c,4,0x3e999999);
  auto draw=random(c);write(c+0x470,1,variant);auto bits=1u<<(variant&31);write(c+0xe8,4,bits);write(c+0x474,4,u32(c+0x300)+draw%120+480);
  write(c+0x220,8,call("model_buildHideMask",{u32(static_cast<Address>(u32(c+0x24))+12),bits}));
  if (i8(c+0x470)==3) call("lump_load",{0x547310u});
  call("anim_clear",{static_cast<Raw>(c+0x114)});call("anim_push",{static_cast<Raw>(c+0x114),u32(static_cast<Address>(u32(c+0x24))+0x54),1u});call("obj_runEvery60",{static_cast<Raw>(c)});
  event_params(c,3,{1,1,0,0},{0x3e4ccccc,0x3f199999,0,0});
  event_params(c,4,{1,1,1,0},{0x3e4ccccc,0x3f199999,ONE,0});
  event_params(c,5,{2,0,0,0},{0x3e999999,0,0,0});
  event_params(c,6,{1,1,1,0},{0x3e999999,0x3f4ccccc,0x3fcccccc,0});
  event_params(c,14,{1,0,0,0},{HALF,0,0,0});event_params(c,8,{1,0,0,0},{0x40000000,0,0,0});
  for (auto entry:{17u,18u,19u,24u}) event_params(c,entry,{1,0,0,0},{0x3e4ccccc,0,0,0});
  event_params(c,15,{0x80,0,0,0},{0x3dcccccc,0,0,0});
  if (mounted) {auto block=pointer_result(call("pool_new",{0x3d8u}));if (block) {
    call("Nightmare_ctor",{block,0x547380u,vec(c+0x10),static_cast<Raw>(i16(c+0x38)),static_cast<Raw>(0x500848+12*i8(c+0x470))});
    call("handle_link",{static_cast<Raw>(c+0x3c8),block,block});call("handle_link",{block+0x3c8,static_cast<Raw>(c),static_cast<Raw>(c)});horsemen_set_state(c,27);
  }}
  call("Creature_init",{static_cast<Raw>(c)});
}
// Complete Zek family from the pinned arena contract. All effects are services.
namespace {
void zek_span(Address c) {
  if (c>UINT32_MAX-0x47fu) throw Unsupported("Zek raw32 span");
}
Raw zek_unit(Raw draw) {
  auto m=mul(from_int(draw),TWO_M16);return sub(add(m,m),ONE);
}
}
Result Model::vcall(Address c,unsigned slot,std::vector<Argument> args) {
  SkelightScope full(full_domain_),zek(zek_domain_),horse(horsemen_domain_),arena(arena_cast_domain_),skeleton(skelight_domain_);
  auto saved=std::move(dispatched_result_);dispatched_result_.reset();
  try {
    // Reference table addressing is not limited to the usual 38 slots.
    auto table=u32(c+0x48),fn=u32(static_cast<Address>(table)+8*slot+4);
    constexpr Raw derived[]={0x2ab5d8,0x267654,0x297b30,0x2a5068,0x244380,0x2ab704,0x267a0c,0x297f54,0x2a55ec,0x244aa4,0x2ab8e0,0x244a9c,0x3c2738,0x3c2708,0x10beb8,0x10e658,0x2a8b84,0x2a91b8,0x27236c,0x272f4c,0x2c2440,0x2c382c,0x2c34e4,0x2c3c54};
    constexpr std::pair<unsigned,Raw> base[]={{2,0x116638},{6,0x11a418},{7,0x11a260},{10,0x11a3f0},{11,0x11a448},{17,0x11a868},{20,0x119328},{23,0x10ded0},{26,0x10bb08},{27,0x3c2688},{30,0x118a58},{31,0x118af8},{35,0x118c50},{36,0x118c78},{37,0x3c2730}};
    bool local=false;for (auto body:derived) local|=fn==body;
    for (auto body:base) local|=slot==body.first && fn==body.second;
    if (!local || external.count("Creature.v"+std::to_string(slot))) {
      args.insert(args.begin(),static_cast<Raw>(c+i16(static_cast<Address>(table)+8*slot)));
      auto value=call_value("Creature.v"+std::to_string(slot),std::move(args));
      if (auto integer=std::get_if<Structured>(&value);integer && integer->kind==creature_movement::ValueKind::Integer) value=static_cast<std::uint64_t>(integer->scalar);
      dispatched_result_=std::move(saved);return value;
    }
    auto result=dispatch(c,slot,std::move(args));
    // Only run bodies forward a final state callback's result. Base numeric
    // bodies own their return even when they consult other virtual callbacks.
    bool forwards=fn==0x2ab5d8 || fn==0x267654 || fn==0x297b30 || fn==0x2a5068 || fn==0x244380 || fn==0x2a8b84 || fn==0x27236c;
    if (!forwards) dispatched_result_.reset();
    Result value=result?(dispatched_result_?*dispatched_result_:Result{*result}):Result{Structured{}};
    if (auto integer=std::get_if<Structured>(&value);integer && integer->kind==creature_movement::ValueKind::Integer) value=static_cast<std::uint64_t>(integer->scalar);
    dispatched_result_=std::move(saved);return value;
  } catch (...) {dispatched_result_=std::move(saved);throw;}
}
std::optional<std::uint64_t> Model::zek_vcall(Address c,unsigned slot,std::vector<Argument> args) {
  SkelightScope zek(zek_domain_),horse(horsemen_domain_),arena(arena_cast_domain_),skeleton(skelight_domain_);
  creature_span(c);if (slot>37) throw Unsupported("Zek virtual slot domain");
  return dispatch(c,slot,std::move(args));
}
std::int32_t Model::zek_slot35(Address) {return to_int(mul(u32(0x4f714c),0x3e999999));}
void Model::take_damage(Address c,Raw attacker,Raw damage_flags,Raw amount) {
  creature_span(c);if (i8(c+0x231)) return;
  write(c+0x3b0,2,0x7fff);
  if (flags(c)&UINT64_C(0x1000000000000)) {
    for (auto off:{0x348,0x344,0x340,0x350,0x34c}) write(c+off,4,0);return;
  }
  write(c+0x304,4,u32(0x44fdcc));write(c+0x2dc,4,u32(c+0x2dc)|damage_flags);
  if (attacker && lt(0,u32(c+0x228))) {
    auto angle=atan2_16(sub(u32(c+0x10),u32(static_cast<Address>(attacker)+0x10)),sub(u32(c+0x14),u32(static_cast<Address>(attacker)+0x14)));
    write(c+0x2e0,2,static_cast<Raw>(0x4000-angle));call("handle_clear",{static_cast<Raw>(c+0x2e4)});
    auto who=attacker;
    if (u32(static_cast<Address>(attacker)+12)&0x80) {auto node=u32(static_cast<Address>(attacker)+0x4c);if (node) who=u32(static_cast<Address>(node)+12);}
    else if (damage_flags&0x10000000) {auto node=u32(c+0x360);if (node) who=u32(static_cast<Address>(node)+12);}
    if (who) call("handle_link",{static_cast<Raw>(c+0x2e4),who,who});
  }
  write(c+0x228,4,sub(u32(c+0x228),amount));write(c+0xc8,4,add(u32(c+0xc8),amount));
}
void Model::zek_take_damage(Address c,Raw attacker,Raw damage_flags,Raw amount) {
  Depth guard(depth_);SkelightScope zek(zek_domain_),horse(horsemen_domain_),arena(arena_cast_domain_),skeleton(skelight_domain_);zek_span(c);
  if ((damage_flags&0x2000) && attacker && (u32(static_cast<Address>(attacker)+12)&0x20)) {
    auto sound=call("res_find",{0x54e558u,0x54e5d8u});call("sound_playOnObject",{static_cast<Raw>(sound),static_cast<Raw>(c),ONE});
    if (!u32(0x503748)) {
      call("particleDef_ctor",{0x8e4ba0u,200u,200u,150u,300u,200u,175u,100u,0x40000000u,0x40a00000u,0xc1a00000u,0u,1u,2u,2u,0u});write(0x503748,4,1);
    }
    auto angle=atan2_16(sub(u32(static_cast<Address>(attacker)+0x14),u32(c+0x14)),sub(u32(static_cast<Address>(attacker)+0x10),u32(c+0x10)));
    Vec3 velocity{mul(cos16(angle),0x41200000),mul(sin16(angle),0x41200000),0x41200000};
    auto pos=vec(c+0x10);pos[2]=add(pos[2],mul(u32(c+0x20),0x3e800000));
    auto ox=mul(cos16(angle),u32(c+0x1c)),oy=mul(sin16(angle),u32(c+0x1c));
    pos[0]=add(pos[0],ox);pos[1]=add(pos[1],oy);pos[2]=add(pos[2],0);
    for (unsigned k=0;k<50;++k) {
      Vec3 ball{};unsigned attempts=0;
      do {
        if (attempts++==4096) throw Unsupported("Zek spark rejection budget");
        for (auto &v:ball) v=zek_unit(random(c));
      } while (lt(ONE,add(add(mul(ball[0],ball[0]),mul(ball[1],ball[1])),mul(ball[2],ball[2]))));
      for (unsigned j=0;j<3;++j) velocity[j]=add(velocity[j],mul(ball[j],0x40a00000));
      call("particle_spawn",{0x8e4ba0u,pos,velocity,0u});
    }
  }
  take_damage(c,attacker,damage_flags,amount);
}
void Model::zek_ctor(Address c,Raw type_name,Vec3 pos,Raw facing,Raw props) {
  Depth guard(depth_);SkelightScope zek(zek_domain_),horse(horsemen_domain_),arena(arena_cast_domain_),skeleton(skelight_domain_);zek_span(c);
  creature_ctor(c,type_name,pos,facing,0x503688,props,0x41200000);
  auto holder=c+0x3c8;write(c+0x48,4,0x5180c8);init_weapon_holder(holder);
  auto frame=u32(0x44fdcc);
  write(c+0x28,1,0x32);write(c+0xb4,1,0xb);write(c+0xb0,4,0x3f999999);write(c+0x1c,4,0x41f00000);
  write(c+0xec,4,frame);write(c+0xf4,4,frame+300);write(c+0xa8,8,flags(c)|UINT64_C(0x2000040000));
  for (auto off:{0x68,0x5c,0x60,0x6c,0x64}) write(c+off,4,HALF);
  for (auto off:{0x460,0xb8,0x464,0x478}) write(c+off,4,0);
  write(c+0x232,2,0);write(c+0x50,4,holder);table_service("Holder",holder,0,1,{1u,0x15u});
  call("anim_clear",{static_cast<Raw>(c+0x114)});zek_set_state(c,0);call("obj_runEvery60",{static_cast<Raw>(c)});
  for (auto entry:{3u,4u,5u,10u}) event_params(c,entry,{1,0,0,0},{0x3f4ccccc,0,0,0});
  event_params(c,17,{1,2,0,0},{0x3e999999,0x3f333333,0,0});
  for (auto entry:{6u,8u,7u,9u}) event_params(c,entry,{1,0,0,0},{0x3f4ccccc,0,0,0});
  event_params(c,16,{1,0,0,0},{0x3fe66666,0,0,0});event_params(c,19,{0x80,0,0,0},{0x3e4ccccc,0,0,0});
  call("Creature_init",{static_cast<Raw>(c)});
}
void Model::zek_set_state(Address c,Raw requested) {
  Depth guard(depth_);SkelightScope zek(zek_domain_),horse(horsemen_domain_),arena(arena_cast_domain_),skeleton(skelight_domain_);zek_span(c);
  auto [refused,state]=filter(c,requested);if (refused) return;
  switch(state) {
  case 0:start(c,0x7c,3);break;case 19:start(c,0x7c,3,0x3e999999);break;
  case 1:case 3:case 16:
    if (state==16) write(c+0x318,2,60);clear_path(c);if (state==1) wander(c);
    start(c,0x94,0x803);call("anim_syncToSimFrame",{static_cast<Raw>(c+0x114)});break;
  case 5:start(c,0x7c,3);call("anim_syncToSimFrame",{static_cast<Raw>(c+0x114)});break;
  case 6:write(c+0x47c,4,6);{auto draw=random(c);swing(c,0x1c+8*(draw&1),0x808);}break;
  case 7:case 8:case 9:case 10:case 11:case 12:case 13:case 14: {
    constexpr Raw clips[]={0x54,0x8c,0x3c,0x4c,0x34,0x44,0x84,0x2c};write(c+0x47c,4,static_cast<Raw>(state));
    swing(c,clips[state-7],state==9 || state==10?0x4808:0x808);break;
  }
  case 18:{auto draw=random(c);swing(c,0x5c+8*(draw&3),0x10);}break;
  case 20: {
    write(c+0xa8,8,flags(c)|UINT64_C(0x20000000000));
    drop_loot(c,0,0x3fcccccc,0x3f199999,0x3fb33333);
    auto text=[&](Address a) {std::string out;for (unsigned k=0;k<64;++k) {auto b=mem.load(a+k,1);if (!b) break;out.push_back(static_cast<char>(b));}return out;};
    if (text(0x44fe18)==text(0x54e5f8)) start(c,0x9c,0x806,0x3e4ccccc);
    else {auto holder=u32(c+0x50);if (holder) table_service("Holder",holder,0,11,{1u,static_cast<Raw>(c),0u,0u});start(c,0xa4,0x806,0x3e4ccccc);}
    dispatch(c,28,{0u});break;
  }
  default:state=0;break;
  }
  write(c+0x232,2,static_cast<Raw>(state));
}
bool Model::zek_specials(Address c) {
  auto clock=s32(u32(c+0x300));
  if (s32(u32(c+0x478))<s32(static_cast<Raw>(clock)-180u)) {
    if (call("creature_findEnemyInCone",{static_cast<Raw>(c),static_cast<Raw>(i16(c+0x38)),5000u,0x42f00000u,0x44480000u})) {dispatch(c,27,{14u});return true;}
  }
  if (s32(u32(c+0x478))<s32(u32(c+0x300))) {
    if (call("creature_findEnemyInCone",{static_cast<Raw>(c),static_cast<Raw>(i16(c+0x38)),0x7fffu,0u,0x43820000u})) {dispatch(c,27,{13u});return true;}
  }
  return false;
}
void Model::zek_switch(Address c,Raw ev) {
  auto st=[&](Raw s) {dispatch(c,27,{s});};
  auto facing=[&](int turn=0) {return static_cast<Raw>(s16(static_cast<Raw>(mem.load(c+0x38,2))+turn));};
  auto sight=[&](Raw face,Raw arc) {return see(c,arc,0x42a80000,0,face);};
  auto sweep=[&](Raw face,Raw f,Raw lo,Raw hi,Argument desc) {
    call("Creature_hitAllEnemiesInArc",{static_cast<Raw>(c),face,8000u,lo,hi,static_cast<Raw>(i16(c+0xd4)),f,std::move(desc),0u,0x42a80000u});
  };
  auto state=i16(c+0x232);
  switch(state) {
  case 0:if (dispatch(c,25,{0u}).value_or(0)) st(3);else if (away(c)) st(1);break;
  case 1:if (dispatch(c,25,{0u}).value_or(0)) st(3);else move_to_dest(c);break;
  case 3:
    if (!u32(c+0x234)) st(0);
    else if (sight(facing(),32000)) st(target_charging(c)?7:5);
    else if (!zek_specials(c)) {if (sight(facing(),0x7fff)) turn_to_target(c);else move_to_target(c);}break;
  case 5:
    if (sight(facing(),8000)) {if (target_charging(c)) st(7);else {auto last=s32(u32(c+0x47c));auto draw=random(c);st(last!=8?draw%3+6:(draw&1)+6);}}
    else if (sight(facing(16000),8000)) st(9);
    else if (sight(facing(-16000),8000)) st(10);
    else if (call("creature_findEnemyInCone",{static_cast<Raw>(c),facing(16000),8000u,0u,0x42a80000u})) st(11);
    else if (call("creature_findEnemyInCone",{static_cast<Raw>(c),facing(-16000),8000u,0u,0x42a80000u})) st(12);
    else if (!zek_specials(c)) {if (sight(facing(),0x7fff)) turn_to_target(c);else if (!i16(c+0x314)) st(3);}break;
  case 6:case 7:
    movement(c,true);if (ev&1) {auto desc=weapon_descriptor(c);call("Creature_hitTargetInArc",{static_cast<Raw>(c),facing(),8000u,static_cast<Raw>(i16(c+0xd0)),static_cast<Raw>(i16(c+0xd2)),static_cast<Raw>(i16(c+0xd4)),0x80008u,desc,0u,0x42a80000u});}
    if (!anim_flags(c,8)) st(5);break;
  case 8:
    movement(c,true);turn_to_target(c);
    if (ev&1) sweep(facing(),0x440001,static_cast<Raw>(i16(c+0xd0)/2),static_cast<Raw>(i16(c+0xd2)/2),0u);
    if (ev&2) sweep(facing(),0x80001,static_cast<Raw>(i16(c+0xd0)/2),static_cast<Raw>(i16(c+0xd2)/2),0u);
    if (!anim_flags(c,8)) st(5);break;
  case 9:case 10:case 11:case 12:
    movement(c,true);if (ev&1) {auto desc=weapon_descriptor(c);sweep(facing(state==9?0:state==11?16000:-16000),0x80008,static_cast<Raw>(i16(c+0xd0)),static_cast<Raw>(i16(c+0xd2)),desc);}
    if (!anim_flags(c,8)) st(5);break;
  case 13:
    movement(c,true);if (ev&1) {
      auto dx=mul(cos16(i16(c+0x38)),0x42a80000),dy=mul(sin16(i16(c+0x38)),0x42a80000);
      Vec3 spot{add(u32(c+0x10),dx),add(u32(c+0x14),dy),u32(c+0x18)};
      auto block=pointer_result(call("pool_new",{0x9cu}));if (block) {auto level=static_cast<Raw>(dispatch(c,35).value_or(0));call("GroundPoundEffect_ctor",{block,spot,static_cast<Raw>(i16(c+0x38)),u32(0x41b208),level,static_cast<Raw>(c)});}
      write(c+0x478,4,u32(c+0x300)+600);
    }
    if (!anim_flags(c,8)) st(5);break;
  case 14:
    movement(c,true);if (ev&1) {
      auto enemy=pointer_result(call("creature_findEnemyInCone",{static_cast<Raw>(c),static_cast<Raw>(i16(c+0x38)),10000u,0x42f00000u,0x44480000u}));
      if (enemy) call("handle_link",{static_cast<Raw>(c+0x460),enemy,enemy});write(c+0x464,4,1);
      auto dx=mul(cos16(i16(c+0x38)),0x42f00000),dy=mul(sin16(i16(c+0x38)),0x42f00000);
      wvec(c+0x46c,{add(u32(c+0x10),dx),add(u32(c+0x14),dy),u32(c+0x18)});write(c+0x468,2,mem.load(c+0x38,2));write(c+0x478,4,u32(c+0x300)+600);
    }
    if (!anim_flags(c,8)) st(5);break;
  case 16:if (!u32(c+0x234)) write(c+0x318,2,0);if (!i16(c+0x318)) st(0);else flee(c,0);break;
  case 18:if (!anim_flags(c,0x10)) st(5);break;
  case 19:if (!i16(c+0x31c)) st(3);break;
  case 20:movement(c,false);dead_update(c,ev,0,0x41a00000);break;
  default:st(0);break;
  }
}
std::optional<std::uint64_t> Model::zek_run(Address c) {
  Depth guard(depth_);SkelightScope zek(zek_domain_),horse(horsemen_domain_),arena(arena_cast_domain_),skeleton(skelight_domain_);zek_span(c);
  process_damage(c,static_cast<Raw>(dispatch(c,16).value_or(0)));
  // The pinned base run always returns zero.
  creature_run(c,static_cast<Raw>(dispatch(c,16).value_or(0)));if (!update_active(c)) return {};
  auto holder=u32(c+0x50);if (holder) table_service("Holder",holder,0,21,{u32(c+0x24),static_cast<Raw>(c),HALF});
  auto ev=step_anim(c,ONE);call("Creature_applyKnockback",{static_cast<Raw>(c),0x3e4cccccu});dispatch(c,23,{0u});
  zek_switch(c,ev);zek_wave(c);return {};
}
void Model::zek_wave(Address c) {
  Depth guard(depth_);SkelightScope zek(zek_domain_),horse(horsemen_domain_),arena(arena_cast_domain_),skeleton(skelight_domain_);zek_span(c);
  if (!u32(c+0x464)) return;
  auto wave=c+0x46c;auto old=vec(wave);auto cs=cos16(i16(c+0x468)),sn=sin16(i16(c+0x468));
  write(wave+4,4,add(u32(wave+4),mul(sn,0x40a00000)));write(wave,4,add(u32(wave),mul(cs,0x40a00000)));
  auto z=add(static_cast<Raw>(call("terrain_height",{u32(0x44fdc8),vec(wave),0x41400000u})),ONE);write(wave+8,4,z);
  auto delta=sub(z,old[2])&0x7fffffffu;bool over=delta>0x41400000 && delta<=0x7f800000;
  if (over) wvec(wave,old);
  auto node=u32(c+0x460);
  if (node) {
    auto enemy=vec(static_cast<Address>(u32(static_cast<Address>(node)+12))+0x10);
    auto bearing=atan2_16(sub(u32(wave+4),enemy[1]),sub(u32(wave),enemy[0]));
    auto heading=static_cast<Raw>(mem.load(c+0x468,2));auto off=s16(static_cast<Raw>(bearing)-heading);
    auto step=to_int(mul(from_int(static_cast<Raw>(off)),0x3ba3d70a));auto damp=to_int(mul(sqrt(0x3ca3d70a),from_int(static_cast<Raw>(step))));
    write(c+0x468,2,heading-(static_cast<Raw>(step)-static_cast<Raw>(damp)));
    auto dx=sub(u32(wave),enemy[0]),dy=sub(u32(wave+4),enemy[1]);if (lt(sqrt(add(mul(dx,dx),mul(dy,dy))),0x42100000)) over=true;
  }
  auto dx=sub(u32(c+0x10),u32(wave)),dy=sub(u32(c+0x14),u32(wave+4));if (lt(0x491c4000,add(mul(dx,dx),mul(dy,dy)))) over=true;
  if (over) {
    write(c+0x464,4,0);call("fx_ringBurst",{vec(wave),0x40800000u});
    auto sound=call("res_find",{0x54e558u,0x54e560u});call("sound_playOnObject",{static_cast<Raw>(sound),static_cast<Raw>(c),ONE});
    call("combat_applyAreaDamage",{static_cast<Raw>(c),vec(wave),static_cast<Raw>(i16(c+0xd0)/2),static_cast<Raw>(i16(c+0xd2)/2),0x80840u,0u,0x42200000u,0x43340000u});
    auto turn=s16(random(c));call("decal_add",{vec(wave),static_cast<Raw>(turn),9u,0u,0x42400000u});
    auto block=pointer_result(call("pool_new",{0xbcu}));if (block) call("LightEffect_ctor",{block,vec(wave),0u,0u,0x43000000u,0x42c80000u,0x429c0000u,0xc2000000u,0u,0u,0u,0u,0x43280000u,0x42700000u,ONE,0x3eb33333u});
    call("handle_clear",{static_cast<Raw>(c+0x460)});write(c+0x460,4,0);
  }
  call("fx_spawn",{7u,vec(wave)});
}
} // namespace champions::simulation::rta::monsters_1
