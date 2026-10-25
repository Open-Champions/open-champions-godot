// Authored from ref_arena_monsters.py at 356add27d50e50972b2a671b31e8f4ec93e6778c
// SHA256 84911d47d9471fc5ac3c82b927b2f1d272922554342185af299e1e5744e16f16.
#include "champions/arena_monsters.hpp"
#include <utility>
#include "champions/core_math.hpp"
#include "champions/fx_trig.hpp"
namespace champions::simulation::rta::arena_monsters {
namespace trig = champions::rta::fx::trig;
namespace {
struct Depth {
  std::size_t &n;
  Depth(std::size_t &v, std::size_t max) : n(v) {
    if (n >= max) throw creature_hp::ResourceLimit("arena operation depth limit");
    ++n;
  }
  ~Depth() { --n; }
};
bool truth(const Value &v) {
  if(auto p=std::get_if<CallbackResult>(&v))return p->truth();
  if(auto p=std::get_if<std::int64_t>(&v))return *p!=0;
  if(auto p=std::get_if<std::string>(&v))return !p->empty();
  return !std::holds_alternative<std::monostate>(v);
}
std::int64_t integer(const Value &v) {
  if (auto p = std::get_if<std::int64_t>(&v)) return *p;
  throw Error("arena callback requires integer result");
}
Value i(std::uint64_t v) { return std::int64_t(v & UINT32_MAX); }
void raw_creature(Address c) {
  if (c > UINT32_MAX) throw creature_hp::ResourceLimit("arena creature outside raw32 domain");
}
}
CallbackResult CallbackResult::integer(std::int64_t n) {
  CallbackResult v;v.kind=Kind::integer;v.negative=n<0;
  v.magnitude=n<0?std::uint64_t(-(n+1))+1:std::uint64_t(n);return v;
}
CallbackResult CallbackResult::unsigned_integer(std::uint64_t n) { CallbackResult v;v.kind=Kind::integer;v.magnitude=n;return v; }
CallbackResult CallbackResult::text(std::string s) { CallbackResult v;v.kind=Kind::text;v.data=std::move(s);return v; }
CallbackResult CallbackResult::bytes(std::string s) { CallbackResult v;v.kind=Kind::bytes;v.data=std::move(s);return v; }
CallbackResult CallbackResult::list(std::vector<CallbackResult> a) { CallbackResult v;v.kind=Kind::list;v.elements=std::move(a);return v; }
CallbackResult CallbackResult::tuple(std::vector<CallbackResult> a) { CallbackResult v;v.kind=Kind::tuple;v.elements=std::move(a);return v; }
bool CallbackResult::truth() const {
  switch(kind) {
  case Kind::none:return false;
  case Kind::integer:return magnitude!=0;
  case Kind::text:case Kind::bytes:return !data.empty();
  case Kind::list:case Kind::tuple:return !elements.empty();
  }
  throw Error("invalid callback result kind");
}
namespace {
struct ResultSignature { const char *inputs, *result, *names; };
// Grouped authored schema metadata from the pinned public callback contracts.
// Float-register and stack counts do not affect Python result shaping.
constexpr ResultSignature result_signatures[] = {
  {"","i","SpellDrowsy_typeinfo"},
  {"-","i","pointList_snapToNearest iter_next"},
  {"-i","n","iter_end"},
  {"-pi","i","iter_begin"},
  {"-pi","s","libc_sprintf"},
  {"-pv","n","pathFollower_init"},
  {"i","f","player_speedScale"},
  {"i","i","pool_new"},
  {"ii","i","player_getSkillRank"},
  {"ii","n","player_giveExp"},
  {"iv","n","fx_spawn"},
  {"ivp","n","world_notifyObjectsNear"},
  {"oviibi","n","world_findFreeSpot"},
  {"p","f","SpellDrowsy_getStat3Fraction"},
  {"p","i","paths_findByName net_objOrGhost creature_bossRank handle_removeNode Creature.v35 Creature.v36 Creature.v37 world_findObjectByName model_getSamplePointCount Creature.v16"},
  {"p","n","obj_updateGridCell obj_runEveryFrame obj_delete script_runByName anim_releaseCaches anim_snapTopToEnd handle_clear Creature_applyKnockback creature_stopBossMusic Creature_init obj_moveOffGrid obj_runEvery60 lump_load anim_clear anim_syncToSimFrame anim_jumpTopToEnd"},
  {"pS","i","res_find"},
  {"pi","f","SkillLifeTap_getStat0Fraction"},
  {"pi","i","pointList_getPoint Creature.v25 Holder.v15 Holder.v17"},
  {"pi","n","PhProp_initPhysics Creature.v7 Creature.v23 Creature.v27 Creature.v31 Creature_setStats arena_addKill Creature.v28 Creature.v34 Holder.v2 Holder.v4 Holder.v6 Spell.v1"},
  {"pi","q","model_buildHideMask"},
  {"pii","i","creature_findEnemyInCone Skill.v14"},
  {"pii","n","Creature.v30 Creature_setCharm Holder.v1"},
  {"piii","i","Skill.v11"},
  {"piiiiiii","i","Creature_hitTargetInArc Creature_hitAllEnemiesInArc"},
  {"piiiiiii","n","particleDef_ctor"},
  {"piiiww","n","Skill.v8"},
  {"piipi","i","Skill.v12"},
  {"pip","i","creature_findSpell"},
  {"pipii","n","Holder.v11"},
  {"pipvi","i","Skill.v2"},
  {"piv","i","Creature.v26"},
  {"pivi","n","Creature_deadUpdate"},
  {"piwwwwww","n","model_getBounds"},
  {"pl","n","Creature.v11"},
  {"pp","i","creature_findSpellByType sound_playOnObject Creature.v20 creature_isBlockedBy Skill.v1"},
  {"pp","n","Creature.v6 Creature.v19 creature_addSpell"},
  {"ppi","i","obj_getPropStr obj_getPropInt"},
  {"ppi","n","Creature.v2 anim_start anim_startBlendInOut anim_push"},
  {"ppiii","i","creature_canSeeObj"},
  {"ppiiiiei","i","damage_apply"},
  {"ppp","n","handle_link Holder.v21 Holder.v22"},
  {"pppp","i","anim_advance"},
  {"pppvii","n","fx_disintegrateModel"},
  {"ppviioOi","n","model_getSamplePoint"},
  {"ppvip","n","fx_burnModel fx_shatterToLooseIce Critter_ctor Nightmare_ctor"},
  {"ppvipi","n","fx_shockModel"},
  {"pv","f","terrain_heightBelow terrain_height"},
  {"pv","i","water_disturb creature_countAlliesNear"},
  {"pv","n","Creature.v10 Creature.v17 sound_playAtPos"},
  {"pvi","i","water_test"},
  {"pvi","n","Spell.v2"},
  {"pviO","i","world_slideMove"},
  {"pvii","n","GutChunk_ctor Creature_dropLoot LightEffect_ctor"},
  {"pviiii","n","combat_applyAreaDamage"},
  {"pvipi","n","OniBall_ctor"},
  {"pvipioO","n","model_getAttachPoint"},
  {"pvipip","n","GroundPoundEffect_ctor"},
  {"pvpp","n","GameObject_ctor"},
  {"pvv","n","particle_spawn"},
  {"pvvi","f","terrain_raycast"},
  {"pvvpWii","f","world_raycast"},
  {"pvvppi","n","path_requestToPoint path_requestType1 path_requestType2 path_requestType3"},
  {"pwwwww","i","balance_lookup"},
  {"v","i","players_anyWithinRadius"},
  {"v","n","fx_ringBurst"},
  {"viL","n","world_collectInSphere"},
  {"viLp","n","world_collectObjectsInArc"},
  {"viii","n","decal_add"},
  {"vp","n","loot_spawnNamedItem"},
  {"vvni","n","world_collectObjectsInBox"},
  // The Nightmare mount's added services (research tools/ref_horsemen_mount.py EXTERNALS).
  {"p","i","creature_inKnockdownAnim"},
  {"ppi","n","anim_pushChained"},
  {"ppii","i","Creature_draw"},
  {"i","n","model_setFade"},
  {"","n","model_resetDrawState"},
  {"pvipip","n","model_getAttachWorldMatrix"}
};
const ResultSignature &result_signature(const std::string &name) {
  for(const auto &row:result_signatures) {
    const std::string names=row.names;
    std::size_t start=0;
    while(start<names.size()) {
      auto end=names.find(' ',start);
      if(names.substr(start,end-start)==name)return row;
      if(end==std::string::npos)break;
      start=end+1;
    }
  }
  throw Error("unknown arena callback result signature: "+name);
}
}
CallbackResult shape_result(const std::string &name,const std::vector<CallbackResult> &args,
    CallbackResult ret,CallbackResult fret,const std::vector<CallbackResult> *outs) {
  const auto &signature=result_signature(name);
  std::vector<CallbackResult> values;
  std::size_t position=0,given=0;
  for(const char *p=signature.inputs;*p;++p) {
    const char kind=*p;
    if(std::string("oOwWbnL").find(kind)!=std::string::npos) {
      CallbackResult value;
      if(outs && given<outs->size())value=(*outs)[given++];
      else if(kind=='o'||kind=='O')value=CallbackResult::tuple({CallbackResult::integer(0),CallbackResult::integer(0),CallbackResult::integer(0)});
      else if(kind=='n'||kind=='L')value=CallbackResult::list({});
      else value=CallbackResult::integer(0);
      bool keep=true;
      if(kind=='O'||kind=='W') {
        if(position>=args.size())throw Error("missing optional callback output argument");
        keep=args[position].truth();
      }
      if(keep)values.push_back(std::move(value));
    }
    if(std::string("ipvelsSOW").find(kind)!=std::string::npos)++position;
  }
  if(*signature.result=='s')return CallbackResult::tuple({CallbackResult::text("buf"),
      outs && given<outs->size()?(*outs)[given]:CallbackResult::bytes("")});
  std::vector<CallbackResult> result;
  if(*signature.result!='n')result.push_back(*signature.result=='f'?std::move(fret):std::move(ret));
  result.insert(result.end(),values.begin(),values.end());
  if(result.empty())return {};
  if(result.size()==1)return result.front();
  return CallbackResult::tuple(std::move(result));
}

Model::Model(Memory &m, creature_hp::Limits limits) : limits_(limits), mem(m) {}
Value Model::external_call(const std::string &name, std::vector<Value> args) {
  Depth guard(depth_,limits_.depth);
  return call(name,std::move(args));
}
void Model::store(Address a, std::size_t size, std::uint64_t value) { write(a,size,value); }
void Model::base_take_damage(Address c, Raw attacker, Raw flags, Raw amount) {
  Depth guard(depth_,limits_.depth); take_damage(c,attacker,flags,amount);
}
std::int64_t Model::base_is_enemy(Address c, Raw other) {
  Depth guard(depth_,limits_.depth); return enemy(c,other);
}
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
Value Model::call(const std::string &name, std::vector<Value> args, bool adapt) {
  if (callbacks.size() > limits_.callback_entries || calls.size() >= limits_.history_entries ||
      events.size() >= limits_.history_entries)
    throw creature_hp::ResourceLimit("arena callback/history limit");
  for (auto &v : args) if (auto p = std::get_if<std::int64_t>(&v)) *p = std::int64_t(std::uint64_t(*p)&UINT32_MAX);
  for(auto &v:args)if(auto p=std::get_if<CallbackResult>(&v))if(p->kind==CallbackResult::Kind::integer){
    p->magnitude=(p->negative?std::uint64_t(0)-p->magnitude:p->magnitude)&UINT32_MAX;p->negative=false;
  }
  Call c{name,std::move(args)}; calls.push_back(c); events.emplace_back(c);
  auto it = callbacks.find(name);
  if (it == callbacks.end()) {
    if (!contract_depth_ || !reference_defaults) throw MissingHandler("missing arena handler: " + name);
    std::vector<CallbackResult> shaped;for(const auto &a:c.args)shaped.push_back(callback_value(a));
    auto result=shape_result(name,shaped);
    if(result.kind==CallbackResult::Kind::integer)return std::int64_t(result.magnitude);
    if(result.kind==CallbackResult::Kind::none)return std::monostate{};
    if(!adapt)return result;
    // Adapt the contract's default composite results for existing body consumers.
    if(name=="balance_lookup")return Balance{};
    if(name=="model_getBounds")return Bounds{};
    if(name=="model_getSamplePoint")return result.elements.size()==2?Value(Sample{}):Value(Vec3{});
    if(name=="model_getAttachPoint")return Vec3{};
    if(name=="world_findFreeSpot")return FreeSpot{};
    if(name=="world_slideMove")return Slide{};
    if(name=="world_raycast")return RayHit{};
    if(name=="Skill.v8")return DamageRange{};
    if(name=="world_collectObjectsInBox"||name=="world_collectInSphere"||name=="world_collectObjectsInArc")return Objects{};
    return result;
  }
  auto fn = it->second;
  return fn(*this,c.args);
}
CallbackResult callback_value(const Value &v) {
  using R=CallbackResult;
  if(auto p=std::get_if<R>(&v))return *p;
  if(auto p=std::get_if<std::int64_t>(&v))return R::integer(*p);
  if(std::holds_alternative<std::monostate>(v))return {};
  if(auto p=std::get_if<std::string>(&v))return R::text(*p);
  auto words=[](const auto &a){std::vector<R> out;for(auto n:a)out.push_back(R::unsigned_integer(n));return R::tuple(std::move(out));};
  if(auto p=std::get_if<Vec3>(&v))return words(*p);
  if(auto p=std::get_if<Sample>(&v))return R::tuple({words(p->point),words(p->second)});
  if(auto p=std::get_if<Balance>(&v)){auto r=words(p->factors);r.elements.insert(r.elements.begin(),R::integer(p->found));return r;}
  if(auto p=std::get_if<Objects>(&v)){auto r=words(p->values);r.kind=R::Kind::list;return r;}
  if(auto p=std::get_if<Contacts>(&v))return words(p->values);
  if(auto p=std::get_if<DamageRange>(&v))return R::tuple({R::integer(p->low),R::integer(p->high)});
  if(auto p=std::get_if<FreeSpot>(&v))return R::tuple({words(p->point),R::integer(p->found)});
  if(auto p=std::get_if<RayHit>(&v))return R::tuple({R::integer(p->fraction),R::integer(p->object)});
  if(auto p=std::get_if<Slide>(&v))return R::tuple({R::integer(p->hit),words(p->normal)});
  if(auto p=std::get_if<Bounds>(&v))return words(p->values);
  if(auto p=std::get_if<Element>(&v))return R::tuple({R::text("elem"),R::integer(p->low),R::integer(p->high)});
  throw Error("invalid arena value");
}
Value Model::reference_call(const std::string &name,std::vector<Value> args) {
  Depth guard(depth_,limits_.depth);Depth contract(contract_depth_,limits_.depth);
  return call(name,std::move(args),false);
}
Value Model::vcall(Address obj,unsigned slot,std::vector<Value> args) {
  Depth guard(depth_,limits_.depth);Depth contract(contract_depth_,limits_.depth);raw_creature(obj);
  auto table=u32(obj+0x48),fn=u32(Address(table)+8*Address(slot)+4);
  auto self=Raw(obj+std::uint64_t(std::int64_t(s16(mem.load(Address(table)+8*Address(slot),2)))));
  const auto name="Creature.v"+std::to_string(slot);
  auto arg=[&](unsigned k){return Raw(integer(args.at(k)));};
  auto arity=[&](std::size_t n){if(args.size()!=n)throw Error("creature virtual argument count");};
  // Arena bodies resolve by function identity even in a different virtual slot.
  if(!external.count(name)) {
    auto body=class_bodies.find(fn);
    if(body!=class_bodies.end()){auto run=body->second;return run(*this,self,args);}
    switch(fn) {
    case 0x27236c:arity(0);return horsemen_tick(self);
    case 0x2a8b84:arity(0);return skelight_tick(self);
    case 0x2c2440:arity(0);zek_run(self);return std::monostate{};
    case 0x270bbc:arity(0);nightmare_run(self);return std::monostate{};
    case 0x270b4c:arity(0);nightmare_draw(self);return std::monostate{};
    case 0x27142c:nightmare_on_hit_terrain(self);return std::monostate{};
    case 0x271484:{std::vector<Raw> touched;if(!args.empty()){
      if(auto o=std::get_if<Objects>(&args[0]))touched=o->values;else if(auto k=std::get_if<Contacts>(&args[0]))touched=k->values;}
      nightmare_on_touch_list(self,touched);return std::monostate{};}
    case 0x27155c:arity(1);nightmare_set_state(self,arg(0));return std::monostate{};
    case 0x272f4c:arity(1);horsemen_set_state(self,arg(0));return std::monostate{};
    case 0x2a91b8:arity(1);skelight_set_state(self,arg(0));return std::monostate{};
    case 0x2c382c:arity(1);zek_set_state(self,arg(0));return std::monostate{};
    case 0x2c34e4:arity(3);zek_take_damage(self,arg(0),arg(1),arg(2));return std::monostate{};
    case 0x2c3c54:arity(0);return std::int64_t(zek_slot35(self));
    case 0x3c2738:arity(0);return std::int64_t(slot16_model(self));
    case 0x10e658:arity(1);return slot25_can_see_target(self,arg(0));
    case 0x10beb8:arity(1);slot28_on_death(self,arg(0));return std::monostate{};
    case 0x3c2708:arity(1);slot34_set_home_range(self,arg(0));return std::monostate{};
    default:break;
    }
    // Inherited bodies resolve only when both slot and function match.
    constexpr std::pair<unsigned,Raw> bases[]={{2,0x116638},{6,0x11a418},{7,0x11a260},{10,0x11a3f0},{11,0x11a448},{17,0x11a868},{20,0x119328},{23,0x10ded0},{26,0x10bb08},{27,0x3c2688},{30,0x118a58},{31,0x118af8},{35,0x118c50},{36,0x118c78},{37,0x3c2730}};
    for(auto b:bases)if(slot==b.first&&fn==b.second) {
      if(slot==10||slot==11)return std::monostate{}; // variadic empty bodies
      arity(slot==2?3:slot==26||slot==30?2:slot==35||slot==36||slot==37?0:1);
      if(slot==35)return std::int64_t(zek_slot35(self));
      auto result=tick_body(obj,slot,std::move(args));
      return slot==20||slot==26||slot==36||slot==37?result:Value(std::monostate{});
    }
  }
  args.insert(args.begin(),i(self));return call(name,std::move(args));
}
Value Model::skill_call(Address obj,unsigned slot,std::vector<Value> args) {
  Depth guard(depth_,limits_.depth);Depth contract(contract_depth_,limits_.depth);raw_creature(obj);
  auto table=u32(obj+0x60),fn=u32(Address(table)+8*Address(slot)+4);
  auto self=Raw(obj+std::uint64_t(std::int64_t(s16(mem.load(Address(table)+8*Address(slot),2)))));
  auto name="Skill.v"+std::to_string(slot);
  if(!external.count(name)) {
    if(fn==0x2e2368){if(args.size()!=1)throw Error("skill can-use argument count");return summon_can_use(self,Raw(integer(args[0])));}
    if(fn==0x2e18a4||fn==0x30c708) {
      if(args.size()<3||args.size()>4||(fn==0x2e18a4&&args.size()!=4))throw Error("skill cast argument count");
      auto pos=std::get_if<Vec3>(&args[2]);if(!pos)throw Error("skill cast requires position");
      auto level=Raw(integer(args[0])),caster=Raw(integer(args[1]));
      auto facing=fn==0x2e18a4?Raw(integer(args[3])):0; // Tagar ignores facing.
      return std::int64_t(fn==0x2e18a4?summon_cast(self,level,caster,*pos,facing):tagar_cast(self,level,caster,*pos,facing));
    }
  }
  args.insert(args.begin(),i(self));return call(name,std::move(args));
}
Value Model::table_call(const std::string &cls,Address obj,Raw offset,unsigned slot,std::vector<Value> args) {
  Depth guard(depth_,limits_.depth);Depth contract(contract_depth_,limits_.depth);raw_creature(obj);
  auto table=u32(obj+offset),fn=u32(Address(table)+8*Address(slot)+4);
  auto self=Raw(obj+std::uint64_t(std::int64_t(s16(mem.load(Address(table)+8*Address(slot),2)))));
  auto name=cls+".v"+std::to_string(slot);
  if(cls=="Spell"&&!external.count(name)&&(fn==0x2e177c||fn==0x30cbc8)) {
    if(args.size()>2)throw Error("spell run argument count");
    Vec3 pos{};Raw facing=0;
    // These two reference bodies accept pos but never inspect it.
    if(!args.empty())if(auto p=std::get_if<Vec3>(&args[0]))pos=*p;
    // Both optional arguments are ignored by these exact reference bodies.
    if(fn==0x2e177c)summon_spell_run(self,pos,facing);else tagar_spell_run(self,pos,facing);
    return std::monostate{};
  }
  args.insert(args.begin(),i(self));return call(name,std::move(args));
}
Value Model::set_state(Address c,Raw state) {return vcall(c,27,{std::int64_t(state)});}

Raw Model::model_slot16(Address c) {
  if(contract_depth_)return Raw(integer(vcall(c,16)));
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
  case 1:
    clear_path(c); pick_wander_dest(c);
    call("anim_start",{i(anim),i(clip(c,0x44)),i(0x803),i(0x3dcccccc)});
    call("anim_syncToSimFrame",{i(anim)});
    write(c+0xf4,4,std::uint64_t(u32(c+0x300))+300); break;
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
  if(contract_depth_)return vcall(c,slot,std::move(args));
  if(tick_depth_)return tick_virtual(c,slot,std::move(args));
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
  if(contract_depth_)return cls=="Creature"?vcall(obj,slot,std::move(args)):cls=="Skill"?skill_call(obj,slot,std::move(args)):table_call(cls,obj,0x14,slot,std::move(args));
  if(tick_depth_ && cls=="Creature")return tick_virtual(obj,slot,std::move(args));
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
  if(contract_depth_)return table_call("Holder",h,0,slot,std::move(args));
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
// Inherited ref_creature.take_damage, using this model's borrowed memory and
// chronological observation sink (the standalone HP model owns separate memory).
void Model::take_damage(Address c, Raw attacker, Raw flags, Raw amount) {
  if (mem.load(c+0x231,1)) return;
  write(c+0x3b0,2,0x7fff);
  if (mem.load(c+0xa8,8)&UINT64_C(0x1000000000000)) {
    for (auto off : {0x348,0x344,0x340,0x350,0x34c}) write(c+off,4,0);
    return;
  }
  write(c+0x304,4,u32(creature_hp::G_SIM_FRAME));
  write(c+0x2dc,4,u32(c+0x2dc)|flags);
  if (attacker && lt(ZERO,u32(c+0x228))) {
    auto angle=atan2_16(sub(u32(c+0x10),u32(Address(attacker)+0x10)),
                        sub(u32(c+0x14),u32(Address(attacker)+0x14)));
    write(c+0x2e0,2,UINT64_C(0x4000)-std::uint64_t(std::int64_t(angle)));
    call("handle_clear",{i(c+0x2e4)});
    Raw who=attacker;
    if (u32(Address(attacker)+0xc)&0x80) {
      auto node=u32(Address(attacker)+0x4c);
      if (node) who=u32(Address(node)+0xc);
    } else if (flags&0x10000000) {
      auto node=u32(c+0x360);
      if (node) who=u32(Address(node)+0xc);
    }
    if (who) call("handle_link",{i(c+0x2e4),i(who),i(who)});
  }
  write(c+0x228,4,sub(u32(c+0x228),amount));
  write(c+0xc8,4,add(u32(c+0xc8),amount));
}
void Model::zek_take_damage(Address c, Raw attacker, Raw flags, Raw amount) {
  Depth guard(depth_,limits_.depth); raw_creature(c);
  if ((flags&0x2000) && attacker && (u32(Address(attacker)+0xc)&0x20)) {
    auto sound=integer(call("res_find",{i(0x54e558),i(0x54e5d8)}));
    call("sound_playOnObject",{i(std::uint64_t(sound)),i(c),i(ONE)});
    if (!u32(0x503748)) {
      call("particleDef_ctor",{i(0x8e4ba0),i(200),i(200),i(150),i(300),i(200),i(175),i(100),
           i(0x40000000),i(0x40a00000),i(0xc1a00000),i(0),i(1),i(2),i(2),i(0)});
      write(0x503748,4,1);
    }
    auto angle=atan2_16(sub(u32(Address(attacker)+0x14),u32(c+0x14)),
                        sub(u32(Address(attacker)+0x10),u32(c+0x10)));
    Raw vx=mul(cos16(angle),0x41200000),vy=mul(sin16(angle),0x41200000),vz=0x41200000;
    auto p=vec(c+0x10);
    p[2]=add(p[2],mul(u32(c+0x20),0x3e800000));
    auto ox=mul(cos16(angle),u32(c+0x1c)),oy=mul(sin16(angle),u32(c+0x1c));
    p[0]=add(p[0],ox); p[1]=add(p[1],oy); p[2]=add(p[2],ZERO);
    for (unsigned n=0;n<50;++n) {
      Raw x,y,z;
      do {
        x=unit(own_draw(c)); y=unit(own_draw(c)); z=unit(own_draw(c));
      } while (lt(ONE,add(add(mul(x,x),mul(y,y)),mul(z,z))));
      vx=add(vx,mul(x,0x40a00000));vy=add(vy,mul(y,0x40a00000));vz=add(vz,mul(z,0x40a00000));
      call("particle_spawn",{i(0x8e4ba0),p,Vec3{vx,vy,vz},i(ZERO)});
    }
  }
  take_damage(c,attacker,flags,amount);
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
  if(contract_depth_)return vcall(c,slot,std::move(args));
  if(tick_depth_)return tick_virtual(c,slot,std::move(args));
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
  if(contract_depth_)return skill_call(skill,2,{i(level),i(c),pos,i(facing)});
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
// Zek's AI switch uses the following inherited navigation bodies internally.
// All services outside ref_creature remain explicit synchronous callbacks.
Raw Model::handle_object(Address head) const {
  auto node=u32(head);return node?u32(Address(node)+12):0;
}
bool Model::is_away_from_home(Address c) const {
  raw_creature(c);auto cf=mem.load(c+0xa8,8);
  auto dx=sub(u32(c+0x10),u32(c+0x3b4)),dy=sub(u32(c+0x14),u32(c+0x3b8));
  auto d2=add(mul(dx,dx),mul(dy,dy));
  if(cf&0x8000000)return lt(0x45100000,d2);
  auto dz=sub(u32(c+0x18),u32(c+0x3bc));d2=add(d2,mul(dz,dz));
  if(cf&0xc00000)return lt(0x46610000,d2);
  auto limit=mul(u32(c+0x3c0),0x3f2a7ef9);return lt(mul(limit,limit),d2);
}
Vec3 Model::random_home_point(Address c,std::int32_t reach) {
  if(lt(u32(c+0x3c0),from_int(reach)))reach=to_int(u32(c+0x3c0));
  auto home=vec(c+0x3b4),point=home;
  auto dx=sub(u32(c+0x10),home[0]),dy=sub(u32(c+0x14),home[1]),dz=sub(u32(c+0x18),home[2]);
  auto d2=add(add(mul(dx,dx),mul(dy,dy)),mul(dz,dz));
  if(le(from_int(Raw(reach)*Raw(reach)),d2) || ((mem.load(c+0xa8,8)&0x400000)&&lt(0x46610000,d2)))return home;
  for(unsigned k=0;k<10;++k) {
    auto x=add(u32(c+0x3b4),from_int((own_draw(c)&(Raw(reach)*2))-Raw(reach)));
    auto y=add(u32(c+0x3b8),from_int((own_draw(c)&(Raw(reach)*2))-Raw(reach)));
    point={x,y,u32(c+0x3bc)};
    auto ground=Raw(integer(call("terrain_heightBelow",{i(u32(0x44fdc8)),point,i(u32(c+0x1c))})));
    auto z=u32(c+0x18);if(lt(sub(z,0x41400000),ground)&&lt(ground,add(z,0x41400000)))break;
  }
  return point;
}
Value Model::navigation_virtual(Address c,unsigned slot,std::vector<Value> args) {
  raw_creature(c);auto table=u32(c+0x48),fn=u32(Address(table)+8*slot+4);
  auto self=Raw(c+std::uint64_t(std::int64_t(s16(mem.load(Address(table)+8*slot,2)))));
  auto name="Creature.v"+std::to_string(slot);
  if(contract_depth_ && !((slot==26&&fn==0x10bb08)||(slot==6&&fn==0x11a418)||(slot==10&&fn==0x11a3f0)||(slot==11&&fn==0x11a448)))return vcall(c,slot,std::move(args));
  if(!external.count(name) && !(slot==26&&external_slot26&&!contract_depth_)) {
    if(slot==26 && fn==0x10bb08) {
      auto mode=Raw(integer(args.at(0)));auto point=std::get_if<Vec3>(&args.at(1));
      if(!point)throw Error("turning requires vector argument");
      auto limit=to_int(mul(u32(Address(self)+0xb0),0x44888888));
      auto target=atan2_16(sub((*point)[1],u32(Address(self)+0x14)),sub((*point)[0],u32(Address(self)+0x10)));
      if(s32(mode)==1)target+=0x8000;
      std::int64_t facing=mem.load(Address(self)+0x38,2);auto delta=s16(Raw(target-facing));
      std::int64_t av=mem.load(Address(self)+0x110,2)+std::int64_t(to_int(mul(from_int(delta),0x3d4ccccc)));
      av-=to_int(mul(rta::sqrt(0x3e4ccccc),from_int(s16(Raw(av)))));
      write(Address(self)+0x110,2,std::uint64_t(av));
      if(limit<s16(Raw(av)))write(Address(self)+0x110,2,Raw(limit));
      else if(s16(Raw(av))<s32(Raw(0)-Raw(limit)))write(Address(self)+0x110,2,Raw(0)-Raw(limit));
      auto slow=mem.load(Address(self)+0x2fc,1);
      if(slow>=1&&slow<=60)facing+=to_int(mul(from_int(s16(mem.load(Address(self)+0x110,2))),mul(from_int(60-signed_byte(Raw(slow))),0x3c888888)));
      else facing+=mem.load(Address(self)+0x110,2);
      write(Address(self)+0x38,2,std::uint64_t(facing));return std::int64_t(delta);
    }
    if(slot==6&&fn==0x11a418) {
      if(u32(Address(Raw(integer(args.at(0))))+12)&0x40)write(Address(self)+0xa8,8,mem.load(Address(self)+0xa8,8)|UINT64_C(0x800000000000000));
      return std::int64_t(0);
    }
    if((slot==10&&fn==0x11a3f0)||(slot==11&&fn==0x11a448))return std::int64_t(0);
  }
  // All arena signatures that the switch needs are already implemented here.
  if(slot!=26 && slot!=6 && slot!=10 && slot!=11)return horsemen_virtual(c,slot,std::move(args));
  if(!external.count(name)&&!external_slot26) {
    for(auto known:{0x27236cu,0x272f4cu,0x2a8b84u,0x2a91b8u,0x2c2440u,0x2c382cu,0x2c34e4u,0x2c3c54u,0x3c2738u,0x10e658u,0x10beb8u,0x3c2708u})
      if(fn==known)throw Unsupported("navigation turn redirected to incompatible arena signature");
  }
  args.insert(args.begin(),i(self));return call(name,std::move(args));
}
void Model::water(Address c) {
  auto cf=mem.load(c+0xa8,8);
  if(cf&0x100000){write(c+0xa8,8,cf&UINT64_C(0xdfffffffffffffff));return;}
  if(cf&UINT64_C(0x2000000000000000)) {
    bool wet=true;
    if(!lt(u32(c+0xb8),0x3c23d70a)||!(u32(0x44fdcc)&15))wet=integer(call("water_disturb",{i(u32(0x44fdc8)),vec(c+0x10),i(0x43960000),i(0x41400000)}))!=0;
    if(!wet)write(c+0xa8,8,mem.load(c+0xa8,8)&UINT64_C(0xdfffffffffffffff));return;
  }
  if((u32(0x44fdcc)^Raw(signed_byte(Raw(mem.load(c+0x230,1)))))&7)return;
  if(!integer(call("water_test",{i(u32(0x44fdc8)),vec(c+0x10),i(0),i(0x41400000)})))return;
  write(c+0xa8,8,mem.load(c+0xa8,8)|UINT64_C(0x2000000000000000));own_draw(c);
  auto text=call("libc_sprintf",{i(0x530058),i(1)});auto entry=call("res_find",{i(0x52fc18),text});
  call("sound_playOnObject",{entry,i(c),i(0x3f199999)});
  call("water_disturb",{i(u32(0x44fdc8)),vec(c+0x10),i(0x44bb8000),i(0x41c00000)});
}
Raw Model::move_by(Address c,const Vec3 &velocity) {
  Depth guard(depth_,limits_.depth);raw_creature(c);if(mem.load(c+0x231,1)&8)return 0;
  auto old=vec(c+0x10);auto vx=add(velocity[0],mul(u32(c+0xbc),0x42700000)),vy=add(velocity[1],mul(u32(c+0xc0),0x42700000)),vz=velocity[2];
  write(c+0x38,2,mem.load(c+0x38,2)+mem.load(c+0xc4,2));if(mem.load(c+0x314,2))vx=vy=vz=0;
  write(c+0x308,4,vx);write(c+0x30c,4,vy);write(c+0xbc,8,0);write(c+0xc4,2,0);water(c);
  Vec3 dt{mul(vx,0x3c888888),mul(vy,0x3c888888),mul(vz,0x3c888888)},normal{};Raw bits=0;
  if(lt(0x3a83126e,add(mul(dt[0],dt[0]),mul(dt[1],dt[1])))) {
    auto value=call("world_slideMove",{i(c+0x10),dt,i(3),i(1),i(u32(c+0x1c))});auto p=std::get_if<Slide>(&value);
    if(!p)throw Error("slide requires hit and normal");normal=p->normal;bits=p->hit?1:0;call("obj_updateGridCell",{i(c)});
  }
  auto r=u32(c+0x1c);auto pos=vec(c+0x10);
  auto value=call("world_collectObjectsInBox",{Vec3{sub(pos[0],r),sub(pos[1],r),sub(pos[2],0x447a0000)},Vec3{add(pos[0],r),add(pos[1],r),add(pos[2],0x447a0000)},i(16)});
  auto found=std::get_if<Objects>(&value);if(!found)throw Error("collect requires object list");
  if(found->values.size()>16)throw creature_hp::ResourceLimit("arena contact list limit");
  auto top=add(u32(c+0x18),u32(c+0x20));Raw stand=0xc7c35000,radius=u32(c+0x1c);Contacts touched;
  for(auto o:found->values) {
    Address other=o;auto oflags=u32(other+12);if((oflags&1)||other==c)continue;
    auto dx=sub(u32(c+0x10),u32(other+0x10)),dy=sub(u32(c+0x14),u32(other+0x14));auto oradius=u32(other+0x1c);
    auto dist=rta::sqrt(add(mul(dx,dx),mul(dy,dy)));if(!lt(add(dist,0x3a83126e),add(radius,oradius)))continue;
    auto cflags=u32(c+12);bool push=true;
    if((oflags|cflags)&16) {
      auto oz=u32(other+0x18);if(lt(top,add(oz,0x40400000)))continue;
      auto otop=add(oz,u32(other+0x20));if(lt(otop,u32(c+0x18)))continue;
      if(lt(sub(otop,u32(c+0x18)),0x40a00000)) {
        if(oflags&0x200)push=false;
        else if(!(cflags&0x400)){if(lt(stand,otop))stand=otop;continue;}
      }
    }
    if(push&&!(oflags&0x200)&&!(cflags&0x401)&&lt(0x3a83126e,rta::fabs(dist))) {
      auto k=rta::div(sub(add(radius,oradius),dist),dist);
      call("world_slideMove",{i(c+0x10),Vec3{mul(dx,k),mul(dy,k),0},i(3),i(0),i(radius)});call("obj_updateGridCell",{i(c)});
    }
    bits|=2;navigation_virtual(other,6,{i(c)});touched.values.push_back(o);
    if((u32(c+12)&0x8000000)&&u32(c+0x234)&&handle_object(c+0x234)==other){radius=u32(c+0x1c);break;}
    radius=u32(c+0x1c);
  }
  auto ground=Raw(integer(call("terrain_heightBelow",{i(u32(0x44fdc8)),vec(c+0x10),i(radius)})));
  if(lt(ground,stand))ground=stand;
  auto fall=sub(u32(c+0x30),0x410fffff);write(c+0x30,4,fall);auto z=add(u32(c+0x18),mul(fall,0x3c888888));write(c+0x18,4,z);
  if(le(sub(z,0x3c23d70a),ground)){write(c+0x18,4,ground);if(lt(fall,0)){write(c+0x30,4,0);bits|=4;}}
  if(bits&1)navigation_virtual(c,10,{normal});
  if(bits&2){if(u32(c+12)&0x8000000){write_vec(c+0x10,old);call("obj_updateGridCell",{i(c)});}navigation_virtual(c,11,{touched});}
  return bits;
}
void Model::settle(Address c) {
  Depth guard(depth_,limits_.depth);raw_creature(c);write(c+12,4,u32(c+12)|0x8000000);move_by(c,{});write(c+12,4,u32(c+12)&0xf7ffffff);
}
Vec3 Model::waypoint(Address c,int ahead) const {
  auto index=std::int64_t(s32(u32(c+0x2bc)))+ahead;
  if(index<16)return {from_int(u32(c+0x23c+8*index)),from_int(u32(c+0x240+8*index)),0xbf800000};
  return {0xbf800000,0xbf800000,0xbf800000};
}
void Model::request_path(Address c,const Vec3 &dest,Raw kind,Raw radius,Raw distance) {
  auto name=s32(kind)==1?"path_requestType2":s32(kind)==2?"path_requestType3":!eq(distance,0)?"path_requestType1":"path_requestToPoint";
  call(name,{i(u32(0x44fdc8)),vec(c+0x10),dest,i(c),i(c+0x23c),i(16),i(s32(kind)==1||s32(kind)==2||!eq(distance,0)?distance:radius)});write(c+0x2bc,4,16);
}
Vec3 Model::advance_waypoint(Address c,Vec3 point,const Vec3 &dest,Raw kind,Raw radius,Raw distance) {
  auto index=std::int64_t(s32(u32(c+0x2bc)));point[0]=point[1]=0xbf800000;
  if(index>=16)return point;
  if(s32(u32(c+0x23c+8*index))!=-1) {
    write(c+0x2bc,4,std::uint64_t(index+1));
    if(index+1!=16) {auto x=u32(c+0x23c+8*(index+1));if(s32(x)!=-1)return {from_int(x),from_int(u32(c+0x240+8*(index+1))),point[2]};}
  }
  request_path(c,dest,kind,radius,distance);return point;
}
Raw Model::probe_rays(Address c,const Vec3 &direction) {
  auto world=u32(0x44fdc8);Raw t=0;
  for(auto side:{0,0x4000,-0x4000}) {
    auto x=side?u32(c+0x10):add(u32(c+0x10),0),y=u32(c+0x14);
    if(side){auto ox=mul(cos16(s16(mem.load(c+0x38,2))+side),u32(c+0x1c)),oy=mul(sin16(s16(mem.load(c+0x38,2))+side),u32(c+0x1c));x=add(u32(c+0x10),ox);y=add(u32(c+0x14),oy);}
    t=Raw(integer(call("world_raycast",{i(world),Vec3{x,y,add(u32(c+0x18),0x41300000)},direction,i(c),i(0),i(3),i(1),i(0x3f8ccccc)})));
    if(!lt(t,0)&&!le(0x3f800000,t))return t;
  }
  return lt(t,0)?0x3f800000:t;
}
Vec3 Model::avoidance_target(Address c,const Vec3 &dest) {
  auto p=vec(c+0x10);Vec3 d{sub(dest[0],p[0]),sub(dest[1],p[1]),sub(dest[2],p[2])};auto t=probe_rays(c,d);
  Vec3 straight{add(p[0],mul(d[0],0x44960000)),add(p[1],mul(d[1],0x44960000)),add(p[2],mul(d[2],0x44960000))};
  if(le(0x3f800000,t))return straight;
  auto two_r=add(u32(c+0x1c),u32(c+0x1c)),near=mul(t,add(add(mul(d[0],d[0]),mul(d[1],d[1])),mul(d[2],d[2])));
  if(!lt(near,mul(two_r,two_r)))return straight;
  auto world=u32(0x44fdc8);Vec3 origin{add(u32(c+0x10),0),u32(c+0x14),add(u32(c+0x18),0x41400000)};
  auto ray=[&](int side,Raw factor){return Raw(integer(call("world_raycast",{i(world),origin,Vec3{mul(cos16(s16(mem.load(c+0x38,2))+side),two_r),mul(sin16(s16(mem.load(c+0x38,2))+side),two_r),0},i(c),i(0),i(3),i(1),i(factor)})));};
  auto tl=ray(0x4000,0x3f8ccccc);int turn;
  if(!le(0,tl)||!lt(tl,0x3f800000))turn=0x2000;
  else {auto tr=ray(-0x4000,0x3f800000);if(!le(0,tr)||!lt(tr,0x3f800000))turn=-0x2000;
    else if(lt(mul(tl,tl),near)&&lt(near,mul(tr,tr)))turn=0;else turn=lt(tr,tl)?0x2000:-0x2000;}
  auto cs=cos16(s16(mem.load(c+0x38,2))+turn),sn=sin16(s16(mem.load(c+0x38,2))+turn);
  return {add(u32(c+0x10),mul(cs,0x44960000)),add(u32(c+0x14),mul(sn,0x44960000)),u32(c+0x18)};
}
Vec3 Model::avoidance_offset(Address c,Raw ignore,Raw length) {
  auto world=u32(0x44fdc8);
  auto cast=[&](const Vec3 &origin,const Vec3 &direction){auto v=call("world_raycast",{i(world),origin,direction,i(c),i(1),i(3),i(1),i(0x3f800000)});auto r=std::get_if<RayHit>(&v);if(!r)throw Error("ray with hit output requires fraction and object");return RayHit{lt(r->fraction,0)?0x3f800000:r->fraction,r->object};};
  auto beside=[&](int turn,Raw reach){return Vec3{mul(cos16(s16(mem.load(c+0x38,2))+turn),reach),mul(sin16(s16(mem.load(c+0x38,2))+turn),reach),0};};
  Vec3 forward{mul(cos16(s16(mem.load(c+0x38,2))),length),mul(sin16(s16(mem.load(c+0x38,2))),length),u32(c+0x1c)};
  Vec3 centre{u32(c+0x10),u32(c+0x14),add(u32(c+0x18),0x41500000)};auto tc=cast(centre,forward);
  auto o=beside(0x4000,u32(c+0x1c));auto tl=cast({add(u32(c+0x10),o[0]),add(u32(c+0x14),o[1]),add(u32(c+0x18),0x41500000)},forward);
  o=beside(-0x4000,u32(c+0x1c));auto tr=cast({add(u32(c+0x10),o[0]),add(u32(c+0x14),o[1]),add(u32(c+0x18),0x41500000)},forward);
  if(!(lt(tc.fraction,0x3f800000)||lt(tl.fraction,0x3f800000)||lt(tr.fraction,0x3f800000))){write(c+0x2f8,1,0);return {};}
  auto side=signed_byte(Raw(mem.load(c+0x2f8,1)));
  if(side==0) {
    auto nearest=fmin(fmin(tc.fraction,tl.fraction),tr.fraction);centre={u32(c+0x10),u32(c+0x14),add(u32(c+0x18),0x41500000)};
    o=beside(0x4000,add(u32(c+0x1c),0x40a00000));auto tsl=cast(centre,{o[0],o[1],u32(c+0x1c)});
    o=beside(-0x4000,add(u32(c+0x1c),0x40a00000));auto tsr=cast(centre,{o[0],o[1],u32(c+0x1c)});
    if(ignore&&((tc.object==ignore&&eq(tc.fraction,nearest))||(tl.object==ignore&&eq(tl.fraction,nearest))||(tr.object==ignore&&eq(tr.fraction,nearest))))return {};
    auto blocked=lt(tc.fraction,0x3f800000)||lt(tr.fraction,0x3f800000)||lt(tl.fraction,0x3f800000);
    if(tsl.object==ignore&&lt(tsl.fraction,nearest))side=2;
    else if(tsr.object==ignore&&lt(tsr.fraction,nearest))side=1;
    else if(blocked&&lt(tsr.fraction,0x3f800000))side=2;
    else if(blocked&&lt(tsl.fraction,0x3f800000))side=1;
    else side=lt(tr.fraction,0x3f800000)?2:1;
    write(c+0x2f8,1,side);
  }
  if(side!=1&&side!=2)return {};
  auto o2=beside(side==1?-12000:12000,u32(c+0x1c));return {add(o2[0],o2[0]),add(o2[1],o2[1]),0};
}
Vec3 Model::steer_without_path(Address c,const Vec3 &dest,Vec3 point) {
  if(!((u32(0x44fdcc)^Raw(signed_byte(Raw(mem.load(c+0x230,1)))))&63)) {
    if(lt(probe_rays(c,{sub(dest[0],u32(c+0x10)),sub(dest[1],u32(c+0x14)),sub(dest[2],u32(c+0x18))}),0x3f800000))advance_waypoint(c,point,dest,0,u32(c+0x1c),0);
  }
  auto off=atan2_16(sub(u32(c+0x2f0),u32(c+0x14)),sub(u32(c+0x2ec),u32(c+0x10)))-s16(mem.load(c+0x38,2));
  if(std::abs(off)<1000) {
    if(!((u32(0x44fdcc)^Raw(signed_byte(Raw(mem.load(c+0x230,1)))))&7))write_vec(c+0x2ec,avoidance_target(c,dest));
    else write_vec(c+0x2ec,dest);
  } else {
    auto sx=sub(u32(c+0x10),u32(c+0x2ec)),sy=sub(u32(c+0x14),u32(c+0x2f0)),ex=sub(u32(c+0x10),dest[0]),ey=sub(u32(c+0x14),dest[1]);
    if(lt(add(mul(sx,sx),mul(sy,sy)),add(mul(ex,ex),mul(ey,ey))))write_vec(c+0x2ec,dest);
  }
  return vec(c+0x2ec);
}
Vec3 Model::steer_along_path(Address c,const Vec3 &dest,Vec3 point) {
  if(le(0x3f800000,probe_rays(c,{sub(dest[0],u32(c+0x10)),sub(dest[1],u32(c+0x14)),sub(dest[2],u32(c+0x18))}))){clear_path(c);write_vec(c+0x2ec,dest);return vec(c+0x2ec);}
  for(unsigned k=0;k<16;++k) {
    auto next=waypoint(c,1);auto nx=sub(next[0],point[0]),ny=sub(next[1],point[1]),mx=sub(u32(c+0x10),point[0]),my=sub(u32(c+0x14),point[1]);
    if(!eq(point[0],0xbf800000)) {
      auto wx=sub(point[0],u32(c+0x10)),wy=sub(point[1],u32(c+0x14));
      if(!lt(add(mul(wx,wx),mul(wy,wy)),0x44100000)&&!(s32(u32(c+0x2c0))<s32(u32(0x44fdcc)))) {
        if(eq(next[0],0xbf800000)||!lt(0,add(mul(nx,mx),mul(ny,my))))break;
      }
    }
    write(c+0x2c0,4,u32(0x44fdcc)+30);point=advance_waypoint(c,point,dest,0,u32(c+0x1c),0);if(eq(point[0],0xbf800000))break;
  }
  return eq(point[0],0xbf800000)?dest:point;
}
Raw Model::drowsy(Address c,Raw value) {
  if(s16(mem.load(c+0x31a,2))>0) {
    auto type=call("SpellDrowsy_typeinfo",{});auto spell=call("creature_findSpellByType",{i(c),type});
    if(integer(spell))value=mul(value,Raw(integer(call("SpellDrowsy_getStat3Fraction",{spell}))));
  }
  return value;
}
void Model::navigation_step(Address c,std::int32_t off) {
  auto speed=drowsy(c,u32(c+0xb8));
  if(std::abs(off)>=16001){write(c+0xbc,4,mul(u32(c+0xbc),0x3f000000));write(c+0xc0,4,mul(u32(c+0xc0),0x3f000000));speed=mul(u32(c+0xb8),0x3f000000);}
  auto slow=mem.load(c+0x2fc,1);if(slow>=1&&slow<=60)speed=mul(speed,mul(from_int(60-signed_byte(Raw(slow))),0x3c888888));
  auto vx=mul(cos16(s16(mem.load(c+0x38,2))),speed);move_by(c,{vx,mul(sin16(s16(mem.load(c+0x38,2))),speed),0});
}
void Model::turn_and_step(Address c,const Vec3 &aim) {navigation_step(c,s16(Raw(integer(navigation_virtual(c,26,{i(0),aim})))));}
bool Model::move_to(Address c,const Vec3 &dest) {
  Depth guard(depth_,limits_.depth);raw_creature(c);
  if(eq(u32(c+0x2ec),0)&&eq(u32(c+0x2f0),0))write_vec(c+0x2ec,dest);
  auto dx=sub(dest[0],u32(c+0x10)),dy=sub(dest[1],u32(c+0x14)),dz=sub(dest[2],u32(c+0x18));auto r2=mul(u32(c+0x1c),u32(c+0x1c)),d2=add(add(mul(dx,dx),mul(dy,dy)),mul(dz,dz));
  if(lt(d2,r2))return true;
  if(!((u32(0x44fdcc)^Raw(signed_byte(Raw(mem.load(c+0x230,1)))))&7)&&lt(d2,add(r2,r2))) {
    if(std::abs(atan2_16(dy,dx)-s16(mem.load(c+0x38,2)))>=0x4001)return true;
  }
  auto aim=dest;
  if(s32(u32(c+0x300))<s32(u32(c+0x2d8)))aim=vec(c+0x2ec);
  else {auto point=waypoint(c);aim=eq(point[0],0xbf800000)?steer_without_path(c,dest,point):steer_along_path(c,dest,point);}
  turn_and_step(c,aim);
  auto px=sub(u32(c+0x10),u32(c+0x2c8)),py=sub(u32(c+0x14),u32(c+0x2cc));
  if(mem.load(0x44fcd0,1)||lt(0x41800000,add(mul(px,px),mul(py,py)))){write_vec(c+0x2c8,vec(c+0x10));write(c+0x2d4,4,u32(c+0x300));return false;}
  auto until=s32(u32(c+0x2d8)),clock=s32(u32(c+0x300));
  // ref_core.from_int interprets the low raw32 as signed, even for the u32 timer.
  if(until<clock&&lt(from_int(u32(c+0x2d4)),sub(from_int(clock),0x41f00000))) {
    write(c+0x2d8,4,Raw(clock)+30);auto angle=s16(Raw(mem.load(c+0x38,2))-0x8000);angle=s16(Raw(angle)+(own_draw(c)&1?0x2000:Raw(0)-0x2000));
    write(c+0x2ec,4,add(u32(c+0x10),mul(cos16(angle),0x44960000)));write(c+0x2f0,4,add(u32(c+0x14),mul(sin16(angle),0x44960000)));return false;
  }
  if(until==clock&&std::int64_t(s32(u32(c+0x2d4)))<std::int64_t(until)-300) {
    auto v=call("world_findFreeSpot",{vec(c+0x10),i(0x48),i(to_int(u32(c+0x1c))),i(0)});auto p=std::get_if<FreeSpot>(&v);if(!p)throw Error("free spot requires point and found byte");
    if(signed_byte(p->found)){write_vec(c+0x10,p->point);call("obj_updateGridCell",{i(c)});}
  }
  return false;
}
void Model::move_to_target(Address c) {
  Depth guard(depth_,limits_.depth);raw_creature(c);auto node=u32(c+0x234);
  if(!node){navigation_virtual(c,27,{i(0)});return;}
  auto target=vec(Address(u32(Address(node)+12))+0x10);auto bearing=atan2_16(sub(u32(c+0x14),target[1]),sub(u32(c+0x10),target[0]));
  auto ox=mul(cos16(bearing),add(u32(Address(handle_object(c+0x234))+0x1c),0x40a00000));
  auto oy=mul(sin16(bearing),add(u32(Address(handle_object(c+0x234))+0x1c),0x40a00000));move_to(c,{add(target[0],ox),add(target[1],oy),target[2]});
}
Raw Model::move_to_dest(Address c) {
  Depth guard(depth_,limits_.depth);raw_creature(c);if(move_to(c,vec(c+0x104)))navigation_virtual(c,27,{i(0)});
  if((mem.load(c+0xa8,8)&0x8000000)||s32(u32(c+0x2c4))<4)return 1;
  clear_path(c);write_vec(c+0x104,random_home_point(c,300));return 0;
}
void Model::flee_from(Address c,Raw threat,Raw kind) {
  if(!threat)return;auto fx=u32(c+0xf8),fy=u32(c+0xfc);bool pick=eq(fx,0xbf800000)&&eq(fy,0xbf800000);
  if(!pick){auto dx=sub(fx,u32(c+0x10)),dy=sub(fy,u32(c+0x14));pick=lt(rta::sqrt(add(mul(dx,dx),mul(dy,dy))),u32(c+0x1c));}
  if(pick)write_vec(c+0xf8,random_home_point(c,300));
  else {auto hx=sub(u32(c+0x10),u32(c+0x3b4)),hy=sub(u32(c+0x14),u32(c+0x3b8)),hz=sub(u32(c+0x18),u32(c+0x3bc));
    if(lt(sub(mul(u32(c+0x3c0),u32(c+0x3c0)),0x43c80000),add(add(mul(hx,hx),mul(hy,hy)),mul(hz,hz))))write_vec(c+0xf8,vec(c+0x3b4));}
  if(!((u32(0x44fdcc)^Raw(signed_byte(Raw(mem.load(c+0x230,1)))))&7))write_vec(c+0x2ec,avoidance_offset(c,0,add(u32(c+0x1c),0x41200000)));
  std::int32_t off;
  if(!eq(u32(c+0x2ec),0)&&!eq(u32(c+0x2f0),0))off=s16(Raw(integer(navigation_virtual(c,26,{i(0),Vec3{add(u32(c+0x2ec),u32(c+0x10)),add(u32(c+0x2f0),u32(c+0x14)),add(u32(c+0x2f4),u32(c+0x18))}}))));
  else {
    auto point=waypoint(c);
    for(unsigned k=0;k<16;++k) {
      auto next=waypoint(c,1);auto nx=sub(next[0],point[0]),ny=sub(next[1],point[1]),mx=sub(u32(c+0x10),point[0]),my=sub(u32(c+0x14),point[1]);
      if(!eq(point[0],0xbf800000)) {auto wx=sub(point[0],u32(c+0x10)),wy=sub(point[1],u32(c+0x14));
        if(!lt(rta::sqrt(add(mul(wx,wx),mul(wy,wy))),u32(c+0x1c))&&(eq(next[0],0xbf800000)||!lt(0,add(mul(nx,mx),mul(ny,my)))))break;}
      point=advance_waypoint(c,point,vec(Address(threat)+0x10),signed_byte(kind)?2:1,0,0x44160000);if(eq(point[0],0xbf800000))break;
    }
    if(eq(point[0],0xbf800000))point=vec(c+0xf8);off=s16(Raw(integer(navigation_virtual(c,26,{i(0),point}))));write_vec(c+0xf8,point);
  }
  navigation_step(c,off);
}
void Model::flee_from_target(Address c,Raw kind) {Depth guard(depth_,limits_.depth);raw_creature(c);auto target=handle_object(c+0x234);if(u32(c+0x234))flee_from(c,target,Raw(signed_byte(kind)));}
void Model::zek_switch(Address c,Raw ev,const StateSetter &st) {
  Depth guard(depth_,limits_.depth);raw_creature(c);if(!st)throw MissingHandler("Zek switch requires state setter");
  auto state=s16(mem.load(c+0x232,2));
  auto facing=[&](int turn=0){return s16(Raw(mem.load(c+0x38,2))+Raw(turn));};
  auto see=[&](int face,int arc){return can_see_target_in(c,Raw(face),Raw(arc),0,0,0x42a80000,0)!=0;};
  auto playing=[&](Raw mask=8){return (mem.load(c+0x11a,2)&mask)!=0;};
  auto turn=[&]{auto target=handle_object(c+0x234);if(u32(c+0x234))navigation_virtual(c,26,{i(0),vec(Address(target)+0x10)});};
  auto sweep=[&](int face,Raw flags,int lo,int hi,const Value &desc){call("Creature_hitAllEnemiesInArc",{i(c),i(face),i(8000),i(lo),i(hi),i(s16(mem.load(c+0xd4,2))),i(flags),desc,i(0),i(0x42a80000)});};
  switch(state) {
  case 0:if(integer(navigation_virtual(c,25,{i(0)})))st(c,3);else if(is_away_from_home(c))st(c,1);break;
  case 1:if(integer(navigation_virtual(c,25,{i(0)})))st(c,3);else move_to_dest(c);break;
  case 3:
    if(!u32(c+0x234))st(c,0);
    else if(see(facing(),32000))st(c,is_target_charging(c)?7:5);
    else if(!zek_specials(c,st)){if(see(facing(),0x7fff))turn();else move_to_target(c);}break;
  case 5:
    if(see(facing(),8000)){if(is_target_charging(c))st(c,7);else if(s32(u32(c+0x47c))!=8)st(c,own_draw(c)%3+6);else st(c,(own_draw(c)&1)+6);}
    else if(see(facing(16000),8000))st(c,9);
    else if(see(facing(-16000),8000))st(c,10);
    else if(integer(call("creature_findEnemyInCone",{i(c),i(facing(16000)),i(8000),i(0),i(0x42a80000)})))st(c,11);
    else if(integer(call("creature_findEnemyInCone",{i(c),i(facing(-16000)),i(8000),i(0),i(0x42a80000)})))st(c,12);
    else if(!zek_specials(c,st)){if(see(facing(),0x7fff))turn();else if(is_not_rooted(c))st(c,3);}break;
  case 6:case 7:
    settle(c);if(ev&1){auto desc=weapon_descriptor(c);call("Creature_hitTargetInArc",{i(c),i(facing()),i(8000),i(s16(mem.load(c+0xd0,2))),i(s16(mem.load(c+0xd2,2))),i(s16(mem.load(c+0xd4,2))),i(0x80008),desc,i(0),i(0x42a80000)});}if(!playing())st(c,5);break;
  case 8:
    settle(c);turn();if(ev&1)sweep(facing(),0x440001,half_toward_zero(Raw(mem.load(c+0xd0,2))),half_toward_zero(Raw(mem.load(c+0xd2,2))),i(0));
    if(ev&2)sweep(facing(),0x80001,half_toward_zero(Raw(mem.load(c+0xd0,2))),half_toward_zero(Raw(mem.load(c+0xd2,2))),i(0));if(!playing())st(c,5);break;
  case 9:case 10:case 11:case 12:
    settle(c);if(ev&1){auto desc=weapon_descriptor(c);sweep(facing(state==9?0:state==11?16000:-16000),0x80008,s16(mem.load(c+0xd0,2)),s16(mem.load(c+0xd2,2)),desc);}if(!playing())st(c,5);break;
  case 13:
    settle(c);if(ev&1){auto dx=mul(cos16(facing()),0x42a80000),dy=mul(sin16(facing()),0x42a80000);Vec3 spot{add(u32(c+0x10),dx),add(u32(c+0x14),dy),u32(c+0x18)};
      auto block=call("pool_new",{i(0x9c)});if(integer(block)){auto level=navigation_virtual(c,35,{});call("GroundPoundEffect_ctor",{block,spot,i(facing()),i(u32(0x41b208)),level,i(c)});}write(c+0x478,4,u32(c+0x300)+600);}if(!playing())st(c,5);break;
  case 14:
    settle(c);if(ev&1){auto enemy=call("creature_findEnemyInCone",{i(c),i(facing()),i(10000),i(0x42f00000),i(0x44480000)});if(integer(enemy))call("handle_link",{i(c+0x460),enemy,enemy});
      write(c+0x464,4,1);auto dx=mul(cos16(facing()),0x42f00000),dy=mul(sin16(facing()),0x42f00000);write_vec(c+0x46c,{add(u32(c+0x10),dx),add(u32(c+0x14),dy),u32(c+0x18)});write(c+0x468,2,mem.load(c+0x38,2));write(c+0x478,4,u32(c+0x300)+600);}if(!playing())st(c,5);break;
  case 16:if(is_attack_ready(c))st(c,0);else flee_from_target(c,0);break;
  case 18:if(!playing(16))st(c,5);break;
  case 19:if(is_not_stunned(c))st(c,3);break;
  case 20:move_by(c,{});call("Creature_deadUpdate",{i(c),i(ev),Vec3{},i(0),i(0x41a00000)});break;
  default:st(c,0);break;
  }
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
      if(nightmare_native&&!external.count("Nightmare_ctor"))
        nightmare_ctor(block,0x547380,vec(c+0x10),Raw(std::int64_t(s16(mem.load(c+0x38,2)))),
          Raw(0x500848+12*std::int64_t(signed_byte(Raw(mem.load(c+0x470,1))))));
      else call("Nightmare_ctor",{i(block),i(0x547380),vec(c+0x10),i(std::int64_t(s16(mem.load(c+0x38,2)))),
        i(0x500848+12*std::int64_t(signed_byte(Raw(mem.load(c+0x470,1)))))});
      call("handle_link",{i(c+0x3c8),i(block),i(block)});call("handle_link",{i(Address(block)+0x3c8),i(c),i(c)});
      horsemen_set_state(c,0x1b);
    }
  }
  call("Creature_init",{i(c)});
}
// Shared tick bodies authored from the pinned creature contract. Every service
// delegated by that contract remains synchronous and required here.
Raw Model::player_object(std::int32_t slot) const {
  return u32(Raw(std::int64_t(0x4d8af0)+std::int64_t(slot)*0x4c30+0x43d8));
}
bool Model::near_view(Address a,Raw radius) const {
  auto px=u32(a),py=u32(a+4);
  auto inside=[&](Address b){auto x=u32(b),y=u32(b+4);return le(px,add(x,radius))&&le(sub(x,radius),px)&&le(py,add(y,radius))&&le(sub(y,radius),py);};
  if(mem.load(0x4039c1,1))return inside(0x44f048);
  auto n=s32(u32(0x4d8ae0));std::int64_t k=0;
  if(n>std::int64_t(limits_.history_entries))throw creature_hp::ResourceLimit("player scan limit");
  for(;k<n;++k){auto p=player_object(std::int32_t(k));if(p&&inside(Address(p)+0x10))break;}
  return k!=s32(u32(0x4d8ae0));
}
bool Model::update_active(Address c) {
  Depth guard(depth_,limits_.depth);raw_creature(c);Depth tick(tick_depth_,limits_.depth);
  auto cf=mem.load(c+0xa8,8);auto was=cf>>63;
  write(c+0xa8,8,was?cf|UINT64_C(0x4000000000000000):cf&UINT64_C(0xbfffffffffffffff));
  Raw radius=Raw(std::max({mem.load(c+0x3c4,2),mem.load(c+0x3ac,2),mem.load(c+0x3ae,2)}));
  if(u32(c+12)&0x80000)radius=0xabe0;
  auto r2=from_int(radius*radius);auto pos=vec(c+0x10);
  auto near=[&](Address a){auto d=vec(a);for(unsigned k=0;k<3;++k)d[k]=sub(pos[k],d[k]);return lt(add(add(mul(d[0],d[0]),mul(d[1],d[1])),mul(d[2],d[2])),r2);};
  bool now=false;
  if(mem.load(0x4039c1,1))now=near(0x44f048);
  else for(int k=0;k<std::max(0,signed_byte(Raw(mem.load(0x403a08,1))));++k)if(near(0x4039c8+12*k)){now=true;break;}
  if(now!=bool(was)) {
    auto node=u32(c+4);if(node)call("handle_removeNode",{i(node)});
    if(now)call("handle_link",{i(0x5db3a8),i(c+4),i(c)});
    else {auto cursor=std::int64_t(s32(u32(0x4039fc)))+1;write(0x4039fc,4,std::uint64_t(cursor));if(cursor>=60)write(0x4039fc,4,0);
      call("handle_link",{i(std::int64_t(0x5db3f0)+4*std::int64_t(s32(u32(0x4039fc)))),i(c+4),i(c)});}
  }
  if((u32(c+12)&0xc00000)&&bool(was)!=now){auto script=call("obj_getPropStr",{i(c),i(now?0x52fc60:0x52fc48),i(0)});if(truth(script))call("script_runByName",{script});}
  if(now)write(c+0xa8,8,mem.load(c+0xa8,8)|UINT64_C(0x8000000000000000));
  else {call("anim_releaseCaches",{i(c+0x114)});write(c+0xa8,8,mem.load(c+0xa8,8)&UINT64_C(0x7fffffffffffffff));}
  return now;
}
Value Model::tick_table(const std::string &cls,Address obj,Raw off,unsigned slot,std::vector<Value> args) {
  if(contract_depth_)return table_call(cls,obj,off,slot,std::move(args));
  Depth guard(depth_,limits_.depth);raw_creature(obj);auto table=u32(obj+off),fn=u32(Address(table)+8*slot+4);
  auto self=Raw(obj+std::uint64_t(std::int64_t(s16(mem.load(Address(table)+8*slot,2)))));
  auto name=cls+".v"+std::to_string(slot);
  if(cls=="Spell"&&!external.count(name)&&(fn==0x2e177c||fn==0x30cbc8)) {
    Vec3 pos{};Raw facing=0;
    if(!args.empty()){auto p=std::get_if<Vec3>(&args[0]);if(!p)throw Error("spell tick requires position");pos=*p;}
    if(args.size()>1)facing=Raw(integer(args[1]));
    if(args.size()>2)throw Unsupported("spell tick signature");
    if(fn==0x2e177c)summon_spell_run(self,pos,facing);else tagar_spell_run(self,pos,facing);
    return std::int64_t(0);
  }
  args.insert(args.begin(),i(self));return call(name,std::move(args));
}
Value Model::tick_virtual(Address c,unsigned slot,std::vector<Value> args) {
  if(contract_depth_)return vcall(c,slot,std::move(args));
  return tick_body(c,slot,std::move(args));
}
Value Model::tick_body(Address c,unsigned slot,std::vector<Value> args) {
  Depth guard(depth_,limits_.depth);raw_creature(c);auto t=u32(c+0x48),fn=u32(Address(t)+8*slot+4);
  auto self=Raw(c+std::uint64_t(std::int64_t(s16(mem.load(Address(t)+8*slot,2)))));
  auto name="Creature.v"+std::to_string(slot);
  auto arg=[&](unsigned k){return Raw(integer(args.at(k)));};
  if(!external.count(name)&&!(slot==16&&external_slot16&&!contract_depth_)&&!(slot==26&&external_slot26&&!contract_depth_)) {
    if(args.empty()) {
      if(fn==0x27236c){horsemen_run(self);return std::int64_t(0);}
      if(fn==0x270bbc){nightmare_run(self);return std::int64_t(0);}
      if(fn==0x270b4c){nightmare_draw(self);return std::int64_t(0);}
      if(fn==0x2a8b84){skelight_run(self);return std::int64_t(0);}
      if(fn==0x2c2440){zek_run(self);return std::int64_t(0);}
      if(fn==0x3c2738)return std::int64_t(slot16_model(self));
      if(fn==0x2c3c54||(slot==35&&fn==0x118c50))return std::int64_t(zek_slot35(self));
      if(slot==37&&fn==0x3c2730)return std::int64_t(signed_byte(Raw(mem.load(Address(self)+0x310,1))));
      if(slot==36&&fn==0x118c78){auto rank=s32(Raw(integer(call("creature_bossRank",{i(self)}))));return std::int64_t(rank==1?4:rank==2?10:0);}
    }
    if(args.size()==1&&std::holds_alternative<std::int64_t>(args[0])) {
      if(fn==0x272f4c){horsemen_set_state(self,arg(0));return std::int64_t(0);}
      if(fn==0x27155c){nightmare_set_state(self,arg(0));return std::int64_t(0);}
      if(fn==0x2a91b8){skelight_set_state(self,arg(0));return std::int64_t(0);}
      if(fn==0x2c382c){zek_set_state(self,arg(0));return std::int64_t(0);}
      if(fn==0x10e658)return slot25_can_see_target(self,arg(0));
      if(fn==0x10beb8){slot28_on_death(self,arg(0));return std::int64_t(0);}
      if(fn==0x3c2708){slot34_set_home_range(self,arg(0));return std::int64_t(0);}
      if(slot==27&&fn==0x3c2688){write(Address(self)+0x232,2,arg(0));return std::int64_t(0);}
      if(slot==23&&fn==0x10ded0){pick_target(self,arg(0));return std::int64_t(0);}
      if(slot==20&&fn==0x119328)return enemy(self,arg(0));
      if(slot==31&&fn==0x118af8){if(!u32(0x4f711c)&&!integer(call("creature_bossRank",{i(self)}))){write(Address(self)+0x316,2,arg(0));call("handle_clear",{i(Address(self)+0x234)});write(Address(self)+0x234,4,0);}return std::int64_t(0);}
      if(slot==7&&fn==0x11a260){write(Address(self)+0x2bc,4,0);if(arg(0))write(Address(self)+0x2c4,4,0);else {auto failures=u32(Address(self)+0x2c4);write(Address(self)+0x240,4,UINT32_MAX);write(Address(self)+0x23c,4,UINT32_MAX);write(Address(self)+0x2c4,4,failures+1);}return std::int64_t(0);}
    }
    if(slot==17&&fn==0x11a868&&args.size()==1&&std::holds_alternative<Vec3>(args[0])) {
      auto node=Raw(integer(call("iter_begin",{i(Address(self)+0x364),i(0)})));std::size_t n=0;
      while(node){if(++n>limits_.history_entries)throw creature_hp::ResourceLimit("spell iteration limit");tick_table("Spell",u32(Address(node)+12),0x14,2,{args[0],i(s16(mem.load(Address(self)+0x38,2)))});node=Raw(integer(call("iter_next",{})));}
      call("iter_end",{i(2)});return std::int64_t(0);
    }
    if(args.size()==3&&std::holds_alternative<std::int64_t>(args[0])) {
      if(fn==0x2c34e4){zek_take_damage(self,arg(0),arg(1),arg(2));return std::int64_t(0);}
      if(slot==2&&fn==0x116638){take_damage(self,arg(0),arg(1),arg(2));return std::int64_t(0);}
    }
    if(slot==30&&fn==0x118a58&&args.size()==2){if(!integer(call("creature_bossRank",{i(self)}))&&s16(mem.load(Address(self)+0x232,2))!=20){write(Address(self)+0x31c,2,arg(0));tick_virtual(self,27,{i(19)});if(arg(1))for(auto p:std::array<std::pair<Raw,Raw>,7>{{{0x90,0x3a},{0x96,0x48},{0x92,0x9b},{0x94,0x9a},{0x98,0x3a},{0x9a,0x9b},{0x9c,0x9a}}})write(Address(self)+p.first,2,p.second);}return std::int64_t(0);}
    // Existing movement's adapter contains the remaining contact/turn bodies.
    if((slot==6&&fn==0x11a418)||(slot==10&&fn==0x11a3f0)||(slot==11&&fn==0x11a448)||(slot==26&&fn==0x10bb08))return navigation_virtual(c,slot,std::move(args));
    for(auto known:{0x27236cu,0x272f4cu,0x2a8b84u,0x2a91b8u,0x2c2440u,0x2c382cu,0x2c34e4u,0x2c3c54u,0x3c2738u,0x10e658u,0x10beb8u,0x3c2708u})if(fn==known)throw Unsupported("tick virtual signature mismatch");
  }
  args.insert(args.begin(),i(self));return call(name,std::move(args));
}
void Model::follow_path(Address c,const Value &name) {
  call("pathFollower_init",{name,vec(c+0x10)});auto points=call("paths_findByName",{name});auto index=std::int64_t(s16(mem.load(c+0x58,2)));
  if(index==-1)index=s32(Raw(integer(call("pointList_snapToNearest",{}))));
  auto point=Raw(integer(call("pointList_getPoint",{points,i(index)})));if(!point)return;
  auto dx=sub(u32(c+0x10),u32(point)),dy=sub(u32(c+0x14),u32(Address(point)+4));
  if(lt(sqrt(add(mul(dx,dx),mul(dy,dy))),0x42400000)){auto next=Raw(integer(call("pointList_getPoint",{points,i(index+1)})));write_vec(c+0x104,vec(next));write_vec(c+0x3b4,vec(c+0x104));write(c+0x54,4,u32(c+0x300)+600);write(c+0x58,2,mem.load(Address(next)+0x3c,2));return;}
  write_vec(c+0x104,vec(point));write_vec(c+0x3b4,vec(c+0x104));auto mine=s16(mem.load(c+0x58,2));auto number=mem.load(Address(point)+0x3c,1);
  if(mine!=std::int64_t(number)){write(c+0x54,4,u32(c+0x300)+600);write(c+0x58,2,mem.load(Address(point)+0x3c,2));mine=s16(mem.load(c+0x58,2));}
  if(std::int64_t(number)==mine&&s32(u32(c+0x54))<s32(u32(c+0x300))&&!near_view(c+0x10,0x447a0000)){write_vec(c+0x10,vec(c+0x3b4));call("obj_updateGridCell",{i(c)});}
}
void Model::follow_player(Address c) {
  auto slot=signed_byte(Raw(mem.load(c+0x5a,1)));Raw leader=0;if(slot>=0&&slot<s32(u32(0x4d8ae0)))leader=player_object(slot);
  if(!leader||(u32(Address(leader)+12)&8)) {
    auto pos=vec(c+0x10);auto team=signed_byte(Raw(mem.load(c+0x310,1)));Raw reach=0x43fa0000;int best=-1;auto count=s32(u32(0x4d8ae0));
    if(count>std::int64_t(limits_.history_entries))throw creature_hp::ResourceLimit("player scan limit");
    for(int k=0;k<count;++k){auto p=player_object(k);if(!p||(u32(Address(p)+12)&8))continue;auto d=vec(Address(p)+0x10);for(unsigned j=0;j<3;++j)d[j]=sub(pos[j],d[j]);auto dist=sqrt(add(add(mul(d[0],d[0]),mul(d[1],d[1])),mul(d[2],d[2])));if(lt(reach,dist))continue;
      if(team!=-1&&signed_byte(Raw(mem.load(0x4d8af0+Address(k)*0x4c30+0x4c24,1)))!=team)continue;reach=dist;best=k;}
    write(c+0x5a,1,Raw(signed_byte(Raw(best))));
  } else {
    write_vec(c+0x3b4,vec(Address(leader)+0x10));leader=player_object(signed_byte(Raw(mem.load(c+0x5a,1))));auto ox=mul(cos16(s16(mem.load(Address(leader)+0x38,2))),0x41f00000);
    leader=player_object(signed_byte(Raw(mem.load(c+0x5a,1))));auto oy=mul(sin16(s16(mem.load(Address(leader)+0x38,2))),0x41f00000);
    bool ahead=mem.load(c+0xa8,8)&0x400000;auto hx=ahead?add(u32(c+0x3b4),ox):sub(u32(c+0x3b4),ox);auto hy=ahead?add(u32(c+0x3b8),oy):sub(u32(c+0x3b8),oy);
    write(c+0x3b4,4,hx);write(c+0x3b8,4,hy);if(is_away_from_home(c))write_vec(c+0x104,vec(c+0x3b4));
  }
  if(mem.load(c+0xa8,8)>>63)return;
  auto home=vec(c+0x3b4);Vec3 origin{home[0],home[1],add(home[2],0x41300000)};auto bearing=atan2_16(sub(u32(c+0x14),home[1]),sub(u32(c+0x10),home[0]));
  for(int k=0;k<32;++k){auto offset=s16(Raw((k>>1)<<11));if(k&1)offset=s16(Raw(-offset));auto angle=s16(Raw(bearing+offset));auto length=u32(0x40278c);Vec3 direction{mul(cos16(angle),length),mul(sin16(angle),length),mul(0,length)};
    auto t=Raw(integer(call("terrain_raycast",{i(u32(0x44fdc8)),origin,direction,i(3),i(0x3f8ccccc)})));if(lt(t,0))t=ONE;if(!le(ONE,t))continue;
    Vec3 spot{add(u32(c+0x3b4),mul(direction[0],t)),add(u32(c+0x3b8),mul(direction[1],t)),add(u32(c+0x3bc),mul(direction[2],t))};if(integer(call("players_anyWithinRadius",{spot,i(0x43e10000)})))continue;
    write_vec(c+0x10,spot);call("obj_updateGridCell",{i(c)});call("obj_runEveryFrame",{i(c)});write(c+0xa8,8,mem.load(c+0xa8,8)|UINT64_C(0x8000000000000000));write_vec(c+0x104,vec(c+0x3b4));clear_path(c);return;}
}
void Model::chill(Address c) {
  auto left=sub(u32(c+0x344),0x3d888888);write(c+0x344,4,left);if(le(left,0)){write(c+0x344,4,0);write(c+0x2fc,1,0);}
  auto flags=u32(c+12);if(flags&8)write(c+0x2fc,1,0);else {left=u32(c+0x344);write(c+0x2fc,1,le(left,0x42700000)?Raw(to_int(left)):0x3b);}
  auto limit=sub(0x42700000,mul(u32(c+0x60),0x42700000));if(lt(limit,from_int(signed_byte(Raw(mem.load(c+0x2fc,1))))))write(c+0x2fc,1,Raw(to_int(limit)));
  if(flags&0x40){if(signed_byte(Raw(mem.load(c+0x2fc,1)))>=31)write(c+0x2fc,1,30);write(c+0x344,4,sub(u32(c+0x344),0x3d088888));}
  else if(integer(call("creature_bossRank",{i(c)}))&&signed_byte(Raw(mem.load(c+0x2fc,1)))>=16)write(c+0x2fc,1,15);
  else if(signed_byte(Raw(mem.load(c+0x2fc,1)))>=31)write(c+0x2fc,1,30);
}
void Model::boss_meter(Address c,std::uint64_t cf) {
  bool shown=false;if((cf>>63)&&!(u32(c+12)&8)&&s32(u32(0x4f8424))>0){auto pos=vec(c+0x10);std::int64_t k=0;
    do {if(k>=std::int64_t(limits_.history_entries))throw creature_hp::ResourceLimit("alive player scan limit");auto p=player_object(s32(u32(0x4f8430+4*k)));if(p){auto d=vec(Address(p)+0x10);for(unsigned j=0;j<3;++j)d[j]=sub(d[j],pos[j]);auto reach=mul(u32(c+0x3c0),u32(c+0x3c0));if(lt(add(add(mul(d[0],d[0]),mul(d[1],d[1])),mul(d[2],d[2])),reach)){shown=true;
      if(!(cf&0x80000000)){write(c+0xa8,8,cf|0x80000000);write(0x41b6a8,4,0);write(0x41b6a4,4,0);write(0x41b6a0,4,0);}else {tick_virtual(c,19,{i(0x41ba40)});write(0x41b6a4,4,Raw(to_int(u32(c+0x22c))));write(0x41b6a0,4,Raw(to_int(u32(c+0x228))));}break;}}
    ++k;}while(k<s32(u32(0x4f8424)));}
  if(!shown){cf=mem.load(c+0xa8,8);if(cf&0x80000000){write(0x41b6a0,4,0);write(0x41b6a4,4,0);write(c+0xa8,8,cf&UINT64_C(0xffffffff7fffffff));}}
}
void Model::scavenger(Address c) {
  if(s16(mem.load(c+0x232,2))!=20||!(mem.load(c+0x11a,2)&0x80))return;
  if((mem.load(c+0xa8,8)&UINT64_C(0xc000000000000000))!=UINT64_C(0x8000000000000000))return;
  auto ally=handle_object(c+0x238);if(ally&&(mem.load(Address(ally)+0xa8,8)&UINT64_C(0x40000000000)))return;if(u32(c+12)&2)return;
  auto turn=s16(shared_draw());auto angle=s16(mem.load(c+0x38,2))+turn;auto x=add(u32(c+0x10),mul(cos16(angle),0x41f00000)),y=add(u32(c+0x14),mul(sin16(angle),0x41f00000));Vec3 spot{x,y,u32(c+0x18)};
  auto facing=atan2_16(sub(u32(c+0x14),y),sub(u32(c+0x10),x));auto kind=s32(u32(0x402768));Raw critter=0;
  if(kind==1||kind==2){critter=Raw(integer(call("pool_new",{i(0x448)})));if(critter)call("Critter_ctor",{i(critter),i(kind==1?0x530020:0x530040),spot,i(facing),i(kind==1?0x402770:0x402780)});}
  if(!critter)return;angle=s16(mem.load(Address(critter)+0x38,2));int best=0;Raw best_t=0;
  for(unsigned k=0;k<32;++k){Vec3 dir{mul(cos16(angle),0x43700000),mul(sin16(angle),0x43700000),mul(0x3e4ccccc,0x43700000)};auto origin=vec(Address(critter)+0x10);origin[2]=add(origin[2],0x41c00000);
    auto t=Raw(integer(call("terrain_raycast",{i(u32(0x44fdc8)),origin,dir,i(0),i(ONE)})));if(lt(t,0)||lt(ONE,t))t=ONE;if(lt(best_t,t)){best_t=t;best=angle;}if(eq(best_t,ONE))break;angle=s16(Raw(angle+0x800));}
  write(Address(critter)+0x38,2,Raw(best));call("handle_clear",{i(c+0x238)});write(c+0x238,4,0);call("handle_link",{i(c+0x238),i(critter),i(critter)});
}
Raw Model::creature_run(Address c,Raw model) {
  Depth guard(depth_,limits_.depth);raw_creature(c);Depth tick(tick_depth_,limits_.depth);constexpr Raw dt=0x3c888888;
  if(!(u32(c+12)&0x40)){auto z=add(u32(c+0x18),mul(u32(c+0x20),HALF));tick_virtual(c,17,{Vec3{add(u32(c+0x10),0),u32(c+0x14),z}});}
  for(auto off:{0x98,0x90})for(unsigned j=0;j<8;j+=2){auto v=s16(mem.load(c+off+j,2));if(v>0){write(c+off+j,2,Raw(v-2));if(v-2<0)write(c+off+j,2,0);}else if(v<0){write(c+off+j,2,Raw(v+2));if(v+2>0)write(c+off+j,2,0);}}
  if(mem.load(c+0xa8,8)&0x8000000){auto name=call("obj_getPropStr",{i(c),i(0x530048),i(0)});if(truth(name))follow_path(c,name);}
  auto frame=u32(0x44fdcc);
  if(s16(mem.load(c+0x316,2))){write(c+0x316,2,mem.load(c+0x316,2)-1);if(!(frame&3))call("fx_spawn",{i(0x12),Vec3{u32(c+0x10),u32(c+0x14),add(u32(c+0x18),u32(c+0x20))}});}
  auto root=mem.load(c+0x314,2);if(((root+1)&0xffff)>=2)write(c+0x314,2,root-1);
  for(auto off:{0x318,0x31a,0x31c})if(s16(mem.load(c+off,2))){write(c+off,2,mem.load(c+off,2)-1);if(off==0x31c&&!(frame&3))call("fx_spawn",{i(0x10),Vec3{u32(c+0x10),u32(c+0x14),add(u32(c+0x18),u32(c+0x20))}});}
  auto charm=s16(mem.load(c+0x312,2));if(charm==0){if((mem.load(c+0xa8,8)&UINT64_C(0x2000000000000))&&s16(mem.load(c+0x232,2))!=20)tick_virtual(c,27,{i(20)});}
  else {if(charm!=-1){--charm;write(c+0x312,2,Raw(charm));}if(charm==0){write(c+0x310,1,0xff);write(c+0xa8,8,mem.load(c+0xa8,8)&UINT64_C(0xffffffffff3fffff));}if(mem.load(c+0xa8,8)&0xc00000)follow_player(c);}
  if(signed_byte(Raw(mem.load(c+0x2fc,1)))!=60)write(c+0x300,4,u32(c+0x300)+1);
  if(u32(0x44fe08)&0x100)write(c+0x2fc,1,u32(c+12)&0x40?10:15);
  auto glow=u32(c+0x354);if(lt(0,glow)){auto level=to_int(mul(fmin(ONE,glow),0x43000000));write(c+0x96,2,0x20);write(c+0x354,4,sub(glow,dt));for(auto off:{0x90,0x9c,0x98,0x94})write(c+off,2,Raw(level));}
  if(lt(0,u32(c+0x340))){auto flags=u32(c+12);if(integer(call("net_objOrGhost",{i(c)}))==std::int64_t(c)&&!(flags&0x4000000)){call("fx_burnModel",{i(model),i(c+0x114),vec(c+0x10),i(s16(mem.load(c+0x38,2))),i(c+0x320),i(u32(c+0x340)),i(u32(c+0xdc)),i(u32(c+0xe0)),i(from_int(Raw(mem.load(c+0x28,1))))});flags=u32(c+12);}
    write(c+0x340,4,sub(u32(c+0x340),dt));if(!(flags&0x20000)&&!(u32(0x44fdcc)&31)){Raw low=1,high=1;if(flags&0x40){auto number=Raw(integer(tick_table("Skill",u32(0x41b1a4),0x60,11,{i(0),i(to_int(u32(0x4f714c))),i(0)})));auto base=mul(from_int(number),0x3e199999);auto roll=shared_draw();low=Raw(to_int(base));high=Raw(to_int(add(base,mul(mul(base,0x3e4ccccc),mul(from_int(roll),TWO_M16)))));}
      call("damage_apply",{i(c),i(c),i(100000),i(0),i(0),i(0x1180c040),Element{low&0xffff,high&0xffff},i(0),i(0),i(0)});}}
  if(lt(0,u32(c+0x348))){if(integer(call("net_objOrGhost",{i(c)}))==std::int64_t(c)&&!(u32(c+12)&0x4000000))call("fx_shockModel",{i(model),i(c+0x114),vec(c+0x10),i(s16(mem.load(c+0x38,2))),i(c+0x320),i(0x5e808080),i(u32(c+0x348)),i(u32(c+0xdc)),i(u32(c+0xe0))});auto left=sub(u32(c+0x348),dt);write(c+0x348,4,left);if(le(left,0))for(auto off:{0x33c,0x334,0x32c})write(c+off,4,0);}
  if(lt(0,u32(c+0x344)))chill(c);
  auto count=s32(u32(c+0x388));if(count!=0){if(count<=0||(u32(c+12)&8)){write(c+0x388,4,0);write(c+0x2fc,1,0);}else {auto elapsed=s32(u32(0x44fdcc)-u32(c+0xf0));if(elapsed<31){write(c+0x2fc,1,Raw(elapsed)*2);if(signed_byte(Raw(elapsed)*2)>=61)write(c+0x2fc,1,60);}else write(c+0x2fc,1,count<31?mem.load(c+0x388,1):60);}auto limit=sub(0x42700000,mul(u32(c+0x6c),0x42700000));if(lt(limit,from_int(signed_byte(Raw(mem.load(c+0x2fc,1))))))write(c+0x2fc,1,Raw(to_int(limit)));}
  for(auto p:std::array<std::array<Raw,3>,2>{{{0x34c,0x358,0x35a},{0x350,0x35c,0x35e}}})if(lt(0,u32(c+p[0]))){write(c+p[0],4,sub(u32(c+p[0]),dt));if(!(u32(c+12)&0x20000)&&!(u32(0x44fdcc)&31))call("damage_apply",{i(c),i(c),i(100000),i(0),i(0),i(0x1080c040),i(0),i(0),i(mul(from_int(s16(mem.load(c+p[1],2))),0x3dcccccc)),i(mul(from_int(s16(mem.load(c+p[2],2))),0x3dcccccc))});}
  auto cf=mem.load(c+0xa8,8);if((cf&1)&&!mem.load(c+0x231,1))boss_meter(c,cf);scavenger(c);
  auto z=u32(c+0x3c);auto v=add(u32(c+0x40),mul(sub(u32(c+0x18),z),0x40400000));v=sub(v,mul(sqrt(0x3db851eb),v));write(c+0x40,4,v);write(c+0x3c,4,add(z,mul(v,dt)));
  auto state=s16(mem.load(c+0x232,2));if((state==0||state==5)&&s16(mem.load(c+0x312,2))){cf=mem.load(c+0xa8,8);if(cf&UINT64_C(0x800000000000000)){tick_virtual(c,27,{i(1)});cf=mem.load(c+0xa8,8);}write(c+0xa8,8,cf&UINT64_C(0xf7ffffffffffffff));}
  if((mem.load(c+0xa8,8)&UINT64_C(0x8000000001000000))==0x1000000&&s16(mem.load(c+0x232,2))==20){write(c+12,4,u32(c+12)|0x4002);call("obj_delete",{i(c)});}return 0;
}
Raw Model::step_anim(Address c,Raw speed) {
  Depth guard(depth_,limits_.depth);raw_creature(c);Depth tick(tick_depth_,limits_.depth);Raw events=0;if(s16(mem.load(c+0x232,2))==3)speed=mul(speed,Raw(integer(call("player_speedScale",{i(UINT32_MAX)}))));speed=drowsy(c,speed);auto cf=mem.load(c+0xa8,8);
  if(cf>>63){auto sound=cf&0x2000000?0:c+0x10;auto slow=mem.load(c+0x2fc,1);if(slow>=1&&slow<=60)speed=mul(mul(speed,from_int(60-signed_byte(Raw(slow)))),0x3c888888);events=Raw(integer(call("anim_advance",{i(c+0x114),i(sound),i(c+0xbc),i(c+0xc4),i(speed)})));}
  auto scale=u32(c+0xdc);auto rx=mul(u32(c+0xbc),scale),ry=mul(u32(c+0xc0),scale);write(c+0xbc,4,rx);write(c+0xc0,4,ry);auto cs=cos16(s16(mem.load(c+0x38,2))-0x4000),sn=sin16(s16(mem.load(c+0x38,2))-0x4000);rx=neg(u32(c+0xbc));ry=u32(c+0xc0);write(c+0xbc,4,add(mul(rx,cs),mul(ry,sn)));write(c+0xc0,4,sub(mul(rx,sn),mul(ry,cs)));
  auto slow=mem.load(c+0x2fc,1);if(slow>=1&&slow<=60){auto k=mul(from_int(60-signed_byte(Raw(slow))),0x3c888888);auto yaw=s16(mem.load(c+0xc4,2));write(c+0xc0,4,mul(u32(c+0xc0),k));write(c+0xbc,4,mul(u32(c+0xbc),k));write(c+0xc4,2,Raw(yaw*to_int(mul(from_int(60-signed_byte(Raw(mem.load(c+0x2fc,1)))),0x3c888888))));}return events;
}
void Model::knockback_speed(Address c,Raw flags,const std::array<Raw,4> &speeds) {for(unsigned k=0;k<4;++k)if(flags&(0x40000u<<k)){write(c+0x2e8,4,speeds[k]);return;}}
void Model::hit_reaction(Address c) {
  if(u32(c+0xa8)&1){if(s32(u32(0x4d8ae0))>=2)return;if(!(own_draw(c)&1))return;}
  if(!(u32(c+0x2dc)&0x4380000)){if(!((mem.load(c+0xa8,8)&0x400)&&own_draw(c)%3!=0)){if(own_draw(c)%3!=0)return;}}
  tick_virtual(c,27,{i(18)});
}
Raw Model::gib(Address c,const Vec3 &pos,int variant,Raw kind) {auto block=Raw(integer(call("pool_new",{i(0x268)})));if(block){if(variant<0)variant=int(own_draw(c)%3);call("GutChunk_ctor",{i(block),pos,i(variant),i(kind),i(ONE)});}return block;}
void Model::hit_effect(Address c,Raw model) {
  Bounds bounds;if(model){auto v=call("model_getBounds",{i(model),i(UINT32_MAX)});auto p=std::get_if<Bounds>(&v);if(!p)throw Error("bounds requires six integers");bounds=*p;}
  auto z0=u32(c+0x18);Raw x,y;
  if(le(0x41c80000,u32(c+0x1c))){auto angle=trig::process_damage_hit_angle(static_cast<std::uint32_t>(mem.load(c+0x2e0,2)));x=add(u32(c+0x10),mul(trig::cos16(angle),mul(u32(c+0x1c),0x3f400000)));y=add(u32(c+0x14),mul(trig::sin16(angle),mul(u32(c+0x1c),0x3f400000)));}else{x=u32(c+0x10);y=u32(c+0x14);}
  auto z=add(z0,0x42100000);auto top=s32(bounds.values[5]),bottom=s32(bounds.values[2]);auto height=s32(Raw(top)-Raw(bottom));if(top<36&&height>0)z=add(add(z0,from_int(height/2)),0x41400000);Vec3 pos{x,y,z};auto cf=mem.load(c+0xa8,8);
  if(lt(0,u32(c+0x344))||(cf&0x4000))call("fx_spawn",{i(10),pos});
  else if(cf&0x8000)call("fx_spawn",{i(11),pos});else if(cf&0x10000)call("fx_spawn",{i(2),pos});
  else if(cf&0x20000){auto text=call("libc_sprintf",{i(0x52ffe0),std::monostate{}});auto entry=call("res_find",{i(0x52fc18),text});call("sound_playOnObject",{entry,i(c),i(ONE)});
    auto get=[&](Raw index){auto v=call("model_getAttachPoint",{i(model),vec(c+0x10),i(s16(mem.load(c+0x38,2))),i(c+0x114),i(index),i(0),i(ONE)});auto p=std::get_if<Vec3>(&v);if(!p)throw Error("attach point requires vector");return *p;};auto a=get(0),b=get(1);Vec3 mid;for(unsigned k=0;k<3;++k)mid[k]=mul(add(a[k],b[k]),HALF);
    auto facing=trig::process_damage_facing_angle(static_cast<std::uint32_t>(mem.load(c+0x38,2)));
    Vec3 velocity{mul(trig::cos16(facing),0x41200000),mul(trig::sin16(facing),0x41200000),0x41200000};
    for(unsigned n=0;n<50;++n){Vec3 r;do {for(auto &v:r)v=unit(own_draw(c));}while(lt(ONE,add(add(mul(r[0],r[0]),mul(r[1],r[1])),mul(r[2],r[2]))));for(unsigned k=0;k<3;++k)velocity[k]=add(velocity[k],mul(r[k],0x40a00000));call("particle_spawn",{i(0x5bf608),mid,velocity,i(0)});}
  }else if(cf&0x3000){auto kind=cf&0x2000?5u:3u;call("fx_spawn",{i(cf&0x2000?3:9),pos});gib(c,pos,2,kind);if(own_draw(c)%3==0)gib(c,pos,2,kind);if(le(u32(c+0x228),0)&&own_draw(c)%3==0)gib(c,pos,2,kind);}
  else if((s32(u32(0x44fdc0))>0||mem.load(0x402791,1))&&(cf&0x40000)){call("fx_spawn",{i(0),pos});if(own_draw(c)%3==0)gib(c,pos,2,1);}
}
Raw Model::remove_body(Address c) {auto cf=mem.load(c+0xa8,8);auto flags=u32(c+0x2dc);write(c+12,4,u32(c+12)|0x4002);write(c+0xa8,8,cf|UINT64_C(0x20000000000));return flags;}
Raw Model::shatter_or_gib(Address c,Raw model,Raw flags) {
  if(!(u32(c+0xa8)&1)){auto cf=mem.load(c+0xa8,8);if(!(cf&0x20000000)&&lt(0,u32(c+0x344))&&model){if(!(own_draw(c)&7)){call("fx_shatterToLooseIce",{i(model),i(c+0x114),vec(c+0x10),i(s16(mem.load(c+0x38,2))),i(c+0x320),i(u32(c+0x1c)),i(u32(c+0xdc)),i(u32(c+0xe0))});return remove_body(c);}flags=u32(c+0x2dc);}}
  auto cf=mem.load(c+0xa8,8);if((cf&0x20000001)||!(flags&0x800)||!(flags&0x3c0000))return flags;
  if(!(cf&0x3000)&&(!(cf&0x40000)||s32(u32(0x44fdc0))<=0||mem.load(0x402791,1)))return flags;auto pos=vec(c+0x10);
  if(s32(u32(0x44fdc0))>0){auto seed=Raw(std::uint64_t(u32(0x404468))*0x19660d+0x3c6ef35f);auto angle=s16(seed>>16);seed=Raw(std::uint64_t(seed)*0x19660d+0x3c6ef35f);write(0x404468,4,seed);auto kind=((seed>>16)&1)+(cf&0x2000?4:cf&0x1000?7:1);call("decal_add",{pos,i(angle),i(kind),i(0),i(0x42300000)});
    for(unsigned n=0;n<3;++n){seed=u32(0x404468);Vec3 r;std::size_t attempts=0;do {if(++attempts>limits_.history_entries)throw creature_hp::ResourceLimit("cosmetic rejection limit");for(auto &v:r){seed=Raw(std::uint64_t(seed)*0x19660d+0x3c6ef35f);v=unit(seed>>16);}}while(lt(ONE,add(add(mul(r[0],r[0]),mul(r[1],r[1])),mul(r[2],r[2]))));cf=mem.load(c+0xa8,8);write(0x404468,4,seed);Vec3 base{add(pos[0],0),pos[1],add(pos[2],0x42100000)},spot;for(unsigned k=0;k<3;++k)spot[k]=add(mul(r[k],0x41200000),base[k]);call("fx_spawn",{i(cf&0x1000?9:cf&0x2000?3:1),spot});}}
  for(unsigned group=0;group<2;++group)for(unsigned n=0;n<(group?4u:2u);++n){cf=mem.load(c+0xa8,8);auto chunk=gib(c,pos,-1,cf&0x1000?2:cf&0x2000?5:group);if(!chunk)continue;
    auto push=add(mul(mul(from_int(own_draw(c)),TWO_M16),group?0x42480000:0x42a00000),group?0x41200000:0x41a00000);auto heading=trig::process_damage_gib_angle(static_cast<std::uint32_t>(mem.load(c+0x2e0,2)),own_draw(c),group?40000u:16000u,group?0xb1e0u:0xe0c0u);write(Address(chunk)+0xfc,4,add(mul(u32(Address(chunk)+0xfc),0x3e999999),mul(trig::cos16(heading),push)));write(Address(chunk)+0x100,4,add(mul(u32(Address(chunk)+0x100),0x3e999999),mul(trig::sin16(heading),push)));call("PhProp_initPhysics",{i(chunk),i(18)});}
  return remove_body(c);
}
void Model::award_kill(Address c) {
  auto players=s32(u32(0x4d8ae0));if(players>std::int64_t(limits_.history_entries))throw creature_hp::ResourceLimit("XP player scan limit");std::int64_t levels=0;for(int k=0;k<players;++k)levels+=signed_byte(Raw(mem.load(0x4d8af0+Address(k)*0x4c30+0x59,1)));
  std::int64_t exp=to_int(powf(div(u32(c+0xd8),from_int(Raw(levels))),0x3fb33333));int killer=-1;auto node=u32(c+0x2e4);if(node){auto who=u32(Address(node)+12);if(player_object(0)==who)killer=0;else for(int k=1;k<s32(u32(0x4d8ae0));++k)if(player_object(k)==who)killer=k;}
  if(killer!=-1){auto rank=call("player_getSkillRank",{i(killer),i(0x3b)});auto chance=mul(Raw(integer(call("SkillLifeTap_getStat0Fraction",{i(u32(0x41b25c)),rank}))),0x47800000);if(lt(from_int(own_draw(c)),chance)){auto ball=call("pool_new",{i(0xb8)});if(integer(ball))call("OniBall_ctor",{ball,vec(c+0x10),i(s16(mem.load(c+0x38,2))),i(c),i(0)});}}
  exp=to_int(mul(from_int(Raw(exp)),0x3f400000));
  auto cap=[&](Address record){auto level=signed_byte(Raw(mem.load(record+0x59,1)));auto a=Raw(std::int64_t(0x41b558)+4*level);return level<80?std::int64_t(s32(u32(Address(a)+4)))-s32(u32(a)):std::int64_t(s32(u32(a)));};
  auto mana=[&](Address record){auto m=add(u32(record+0xc0),mul(u32(c+0x22c),0x3d088888));write(record+0xc0,4,m);if(lt(u32(record+0xc4),m))write(record+0xc0,4,u32(record+0xc4));};
  const bool single=s32(u32(0x4d8ae0))<2;
  std::size_t guard=0;for(int k=0;k<s32(u32(0x4d8ae0));++k){if(++guard>limits_.history_entries)throw creature_hp::ResourceLimit("XP player scan limit");auto record=0x4d8af0+Address(k)*0x4c30;
    if(single){if(lt(0,u32(record+0xac))){exp=std::min(exp,cap(record));call("player_giveExp",{i(k),i(exp)});if(!u32(record+0x50))mana(record);}}
    else {auto share=killer==-1?HALF:killer==k?0x3f199999:0x3ecccccc;auto amount=std::int64_t(to_int(mul(from_int(Raw(exp)),share)));if(le(u32(record+0xac),0))amount=to_int(mul(from_int(Raw(amount)),HALF));call("player_giveExp",{i(k),i(std::min(amount,cap(record)))});if(!u32(record+0x50)&&killer==k)mana(record);}}
}
void Model::die(Address c,Raw model) {
  write(c+12,4,u32(c+12)|9);for(auto off:{0x340,0x344,0x348,0x34c,0x350})if(lt(ONE,u32(c+off)))write(c+off,4,ONE);auto flags=u32(c+0x2dc);
  if((flags&0x20)||lt(ONE,u32(c+0x340)))write(c+12,4,u32(c+12)|0x2000000);
  else if(((flags&0x10)||(flags&0x2000000))&&model){call("fx_disintegrateModel",{i(c),i(model),i(c+0x114),vec(c+0x10),i(s16(mem.load(c+0x38,2))),i(flags&0x10?0:1),i(u32(c+0xdc)),i(u32(c+0xe0))});flags=u32(c+0x2dc);}
  else flags=shatter_or_gib(c,model,flags);
  knockback_speed(c,flags,{0x42c80000,0x43160000,0x43480000,0x439c4000});tick_virtual(c,27,{i(20)});write(c+12,4,u32(c+12)|0x2000);
  if(integer(call("obj_getPropInt",{i(c),i(0x52fa10),i(0)}))){call("anim_snapTopToEnd",{i(c+0x114)});return;}if(mem.load(c+0xa8,8)&UINT64_C(0x100000000))return;award_kill(c);
}
void Model::process_damage(Address c,Raw model) {
  Depth guard(depth_,limits_.depth);raw_creature(c);Depth tick(tick_depth_,limits_.depth);
  if(!lt(0,u32(c+0xc8))){if(s16(mem.load(c+0x232,2))!=20){auto flags=u32(c+0x2dc);knockback_speed(c,flags,{0x42480000,0x42960000,0x42c80000,0x431c4000});if(flags==0x2000&&s16(mem.load(c+0x232,2))!=18)hit_reaction(c);}write(c+0x2dc,4,0);return;}
  if(s32(u32(0x4d8ae0))==1)call("world_notifyObjectsNear",{i(0),vec(c+0x10),i(c),i(0x44160000)});write(c+0xc8,4,0);if(s16(mem.load(c+0x232,2))==20)return;write(c+12,4,u32(c+12)|0x100);if(u32(c+0x2dc)&13)hit_effect(c,model);
  if(!le(u32(c+0x228),0)){if(s16(mem.load(c+0x232,2))!=20){auto flags=u32(c+0x2dc);knockback_speed(c,flags,{0x42c80000,0x43160000,0x43610000,0x43bb8000});if(!(flags&0x400000)&&s16(mem.load(c+0x232,2))!=18)hit_reaction(c);}write(c+0x2dc,4,0);return;}
  die(c,model);write(c+0x2dc,4,0);
}
std::int64_t Model::enemy(Address c,Raw other) {
  if(!other||other==c)return 0;auto flags=u32(Address(other)+12);if(!(flags&0x60)||(flags&0x20008))return 0;if(mem.load(Address(other)+0xa8,8)&0x4000800)return 0;
  auto mine=Raw(integer(tick_virtual(c,37))),theirs=Raw(integer(tick_virtual(other,37)));if(mine==theirs)return 0;return mem.load(Address(other)+0xa8,8)&UINT64_C(0x100000000000)?4:1;
}
void Model::pick_target(Address c,Raw force) {
  if(mem.load(c+0x231,1))return;if(s16(mem.load(c+0x316,2))||(mem.load(c+0xa8,8)&0x4000000)){call("handle_clear",{i(c+0x234)});write(c+0x234,4,0);return;}auto node=u32(c+0x234);
  if(s16(mem.load(c+0x232,2))!=20&&node&&(u32(Address(u32(Address(node)+12))+12)&8)){tick_virtual(c,27,{i(0)});return;}Raw best=0;if(node&&integer(tick_virtual(c,20,{i(u32(Address(node)+12))})))best=u32(Address(u32(c+0x234))+12);
  if(!((u32(0x44fdcc)^Raw(signed_byte(Raw(mem.load(c+0x230,1)))))&31)||force){auto reach=u32(c+0x3c0);auto p=vec(c+0x10);Vec3 low,high;for(unsigned k=0;k<3;++k){low[k]=sub(p[k],reach);high[k]=add(p[k],reach);}auto saved=u32(0x4039b8);write(0x4039b8,4,8);auto v=call("world_collectObjectsInBox",{low,high,i(0xc0)});auto found=std::get_if<Objects>(&v);if(!found)throw Error("target scan requires object list");if(found->values.size()>limits_.history_entries)throw creature_hp::ResourceLimit("target scan limit");write(0x4039b8,4,saved);best=0;std::int64_t best_score=0;auto best_d2=mul(u32(c+0x3c0),u32(c+0x3c0));
    for(auto other:found->values){auto score=std::int64_t(s32(Raw(integer(tick_virtual(c,20,{i(other)})))));if(!score)continue;auto dy=sub(u32(c+0x14),u32(Address(other)+0x14)),dx=sub(u32(c+0x10),u32(Address(other)+0x10));auto d2=add(mul(dx,dx),mul(dy,dy));if(handle_object(c+0x2e4)==other&&u32(c+0x2e4))++score;node=u32(c+0x234);auto current=node?u32(Address(node)+12):0;if(node&&current==other)++score;bool take=best_score<score||(score==best_score&&lt(d2,best_d2));if(!take)take=force&&node&&current!=other;if(best&&force&&node&&current==other)take=false;if(!take)continue;
      auto hx=u32(c+0x3b4),hy=u32(c+0x3b8);auto oy=sub(hy,u32(Address(other)+0x14)),ox=sub(hx,u32(Address(other)+0x10));auto home=add(mul(ox,ox),mul(oy,oy));if(!(s32(u32(0x44fdcc)-480)<s32(u32(c+0x304)))&&lt(mul(u32(c+0x3c0),u32(c+0x3c0)),home)){auto my=sub(u32(c+0x14),hy),mx=sub(u32(c+0x10),hx);if(lt(add(mul(mx,mx),mul(my,my)),home)){auto near=add(add(u32(Address(other)+0x1c),u32(c+0x1c)),0x42700000);if(lt(mul(near,near),d2))continue;}}
      best=other;best_d2=d2;best_score=score;}}
  node=u32(c+0x234);if(!best||(node&&u32(Address(node)+12)!=best)){call("handle_clear",{i(c+0x234)});write(c+0x234,4,0);}if(best&&!u32(c+0x234))call("handle_link",{i(c+0x234),i(best),i(best)});
}
void Model::zek_run(Address c) {
  Depth guard(depth_,limits_.depth);raw_creature(c);Depth tick(tick_depth_,limits_.depth);process_damage(c,Raw(integer(tick_virtual(c,16))));if(creature_run(c,Raw(integer(tick_virtual(c,16)))))return;if(!update_active(c))return;auto holder=u32(c+0x50);if(holder)holder_call(holder,21,{i(u32(c+0x24)),i(c),i(HALF)});auto ev=step_anim(c,ONE);call("Creature_applyKnockback",{i(c),i(0x3e4ccccc)});tick_virtual(c,23,{i(0)});zek_switch(c,ev,[&](Address a,Raw state){tick_virtual(a,27,{i(state)});});zek_wave(c);
}
void Model::skelight_run(Address c) {(void)skelight_tick(c);}
Value Model::skelight_tick(Address c) {
  Depth guard(depth_,limits_.depth);raw_creature(c);Depth tick(tick_depth_,limits_.depth);Value result=std::monostate{};auto st=[&](Raw state){result=tick_virtual(c,27,{i(state)});};
  if(s16(mem.load(c+0x232,2))!=23)process_damage(c,class_model(c));if(creature_run(c,class_model(c)))return result;update_active(c);auto holder=u32(c+0x50);std::int64_t row=holder?std::int64_t(s32(Raw(integer(holder_call(holder,15,{i(1)})))))*16:0;holder=u32(c+0x50);auto item=u32(Raw(std::int64_t(0x3e9ce0)+row+8));if(holder)holder_call(holder,22,{i(item),i(c),i(HALF)});
  auto ev=step_anim(c,ONE);call("Creature_applyKnockback",{i(c),i(0x3f333333)});tick_virtual(c,23,{i(0)});auto state=s16(mem.load(c+0x232,2));auto see=[&](Raw facing,Raw arc,Raw far){return can_see_target_in(c,facing,arc,0,0,far,0);};
  switch(state){case 0:if(integer(tick_virtual(c,25,{i(1)})))st(3);else if(is_away_from_home(c))st(1);break;
  case 1:if(integer(tick_virtual(c,25,{i(1)})))st(3);else move_to_dest(c);break;
  case 3:if(!u32(c+0x234))st(0);else if(see(Raw(s16(mem.load(c+0x38,2))),6000,0x42400000))st(6);else if(see(0,0x7fff,0x42100000))turn_to_target(c);else move_to_target(c);break;
  case 5:if(see(Raw(s16(mem.load(c+0x38,2))),6000,0x42400000))st(6);else if(is_not_rooted(c))st(3);break;
  case 6:{bool down=true;if(u32(c+0x234)){if(ev&1){auto desc=weapon_descriptor(c);call("Creature_hitTargetInArc",{i(c),i(s16(mem.load(c+0x38,2))),i(6000),i(s16(mem.load(c+0xd0,2))),i(s16(mem.load(c+0xd2,2))),i(s16(mem.load(c+0xd4,2))),i(8),desc,i(0),i(0x42400000)});}if(mem.load(c+0x11a,2)&8){turn_to_target(c);down=!(mem.load(c+0x11a,2)&8);}}else down=!(mem.load(c+0x11a,2)&8);if(down)write(c+0xf4,4,u32(c+0xf4)-1);if(s32(u32(c+0xf4))<=0)st(3);break;}
  case 16:if(is_attack_ready(c))st(0);else flee_from_target(c,0);break;case 18:st(3);break;case 19:if(is_not_stunned(c))st(0);break;
  case 20:call("Creature_deadUpdate",{i(c),i(ev),Vec3{},i(1),i(0x41a00000)});break;
  case 23:if(mem.load(c+0x11a,2)&0x80)st(0);call("fx_spawn",{i(7),vec(c+0x10)});result=std::monostate{};break;default:break;}
  return result;
}
void Model::horsemen_run(Address c) {(void)horsemen_tick(c);}
Value Model::horsemen_tick(Address c) {
  Depth guard(depth_,limits_.depth);raw_creature(c);Depth tick(tick_depth_,limits_.depth);Value result=std::monostate{};auto st=[&](Raw state){result=tick_virtual(c,27,{i(state)});};auto holder=u32(c+0x50);if(holder)holder_call(holder,21,{i(u32(c+0x24)),i(c),i(HALF)});Raw ev;
  if(!u32(c+0x3c8)){process_damage(c,class_model(c));if(creature_run(c,class_model(c)))return result;update_active(c);ev=step_anim(c,ONE);call("Creature_applyKnockback",{i(c),i(HALF)});tick_virtual(c,23,{i(0)});}
  else {auto mount=u32(Address(u32(c+0x3c8))+12);write_vec(c+0x10,vec(Address(mount)+0x10));write(c+0x38,2,mem.load(Address(u32(Address(u32(c+0x3c8))+12))+0x38,2));call("obj_updateGridCell",{i(c)});process_damage(c,class_model(c));if(creature_run(c,class_model(c)))return result;update_active(c);ev=step_anim(c,ONE);auto theirs=u32(Address(u32(Address(u32(c+0x3c8))+12))+0x234);if(theirs){auto mine=u32(c+0x234);if(!mine||u32(Address(theirs)+12)!=u32(Address(mine)+12)){call("handle_clear",{i(c+0x234)});auto target=u32(Address(u32(Address(u32(Address(u32(c+0x3c8))+12))+0x234))+12);call("handle_link",{i(c+0x234),i(target),i(target)});}}}
  auto state=s16(mem.load(c+0x232,2));auto facing=[&]{return s16(mem.load(c+0x38,2));};auto see=[&](Raw arc,Raw far){return can_see_target_in(c,Raw(facing()),arc,0,0,far,0);};auto playing=[&](Raw mask=8){return mem.load(c+0x11a,2)&mask;};
  auto hit=[&](Raw flags,bool third){Value desc=std::int64_t(0);if(!third)desc=weapon_descriptor(c);auto lo=s16(mem.load(c+0xd0,2)),hi=s16(mem.load(c+0xd2,2));call("Creature_hitTargetInArc",{i(c),i(facing()),i(10000),i(third?third_toward_zero(Raw(lo)):lo),i(third?third_toward_zero(Raw(hi)):hi),i(s16(mem.load(c+0xd4,2))),i(flags),desc,i(0),i(0x42100000)});};
  switch(state){case 0:if(integer(tick_virtual(c,25,{i(0)})))st(3);else if(is_away_from_home(c)||s32(u32(c+0xf4))<s32(u32(c+0x300)))st(1);break;
  case 1:if(integer(tick_virtual(c,25,{i(0)})))st(0);else move_to_dest(c);break;
  case 3:if(!u32(c+0x234)){st(0);break;}if(see(10000,0x42100000)){st(5);break;}if(see(6000,0x42100000)&&is_target_charging(c)){st(8);break;}if(see(10000,0x43c80000)&&s32(Raw(count_allies_near_target(c,0x42100000)))>0){st(5);break;}
    if(s32(u32(c+0x474))<s32(u32(c+0x300))&&mem.load(c+0x470,1)<3&&integer(call("creature_findEnemyInCone",{i(c),i(facing()),i(6000),i(0),i(0x43c80000)}))){st(23);break;}
    if(signed_byte(Raw(mem.load(c+0x470,1)))==3){auto skill=u32(0x41b19c);auto table=u32(Address(skill)+0x60),fn=u32(Address(table)+12);auto self=Raw(skill+std::uint64_t(std::int64_t(s16(mem.load(Address(table)+8,2)))));if(!external.count("Skill.v1")&&(fn==0x2e18a4||fn==0x30c708))throw Unsupported("can-use redirects to incompatible skill cast");auto can=fn==0x2e2368&&!external.count("Skill.v1")?summon_can_use(self,c):integer(tick_table("Skill",skill,0x60,1,{i(c)}));if(can){st(23);break;}}move_to_target(c);break;
  case 5:if(!is_not_rooted(c)){if(see(10000,0x42100000))st(6);break;}if(see(10000,0x42100000)){if((own_draw(c)&1)&&is_blocked_by_target(c))st(10);else st(own_draw(c)&1?6:7);break;}if(see(3000,0x42900000)){st(11);break;}if(own_draw(c)%60)break;if(s32(Raw(count_allies_near_target(c,0x42100000)))>0)break;st(3);break;
  case 6:case 7:case 10:case 11:move_by(c,{});if(u32(c+0x234)){if(ev&1)hit(8,false);else if(ev&2)hit(0x100001,true);turn_to_target(c);}if(!playing())st(3);break;
  case 8:move_by(c,{});if(u32(c+0x234)){if(ev&1)hit(0x40008,false);turn_to_target(c);}if(!playing())st(3);break;
  case 16:if(is_attack_ready(c))st(0);else flee_from_target(c,0);break;case 17:case 24:if(!playing())st(3);break;case 18:if(!playing(16))st(3);break;case 19:if(is_not_stunned(c))st(3);break;
  case 20:call("Creature_deadUpdate",{i(c),i(ev),Vec3{},i(0),i(0x41a00000)});break;
  case 23:if(u32(c+0x234)){if(ev&1){horsemen_cast(c);auto clock=u32(c+0x300);auto roll=own_draw(c);write(c+0x474,4,clock+roll%120+480);}turn_to_target(c);}if(!playing())st(3);break;
  case 25:move_by(c,{});break;case 26:if(s16(mem.load(c+0x472,2))>0)write(c+0x472,2,mem.load(c+0x472,2)-1);break;
  case 27:case 28:case 29:case 30:break;
  case 31:case 32:case 33:case 34:if(u32(c+0x234)&&(ev&1)){auto desc=weapon_descriptor(c);call("Creature_hitAllEnemiesInArc",{i(c),i(s16(Raw(mem.load(c+0x38,2))-0x4000)),i(10000),i(s16(mem.load(c+0xd0,2))),i(s16(mem.load(c+0xd2,2))),i(s16(mem.load(c+0xd4,2))),i(8),desc,i(0),i(0x42100000)});}break;
  case 35:move_by(c,{});{auto flags=mem.load(c+0x11a,2);if((flags&0x98)==0x88){write(c+0x118,1,0);write(c+0x11a,2,flags&0xff7f);call("anim_push",{i(c+0x114),i(clip(c,0x84)),i(20)});}else if((flags&0x90)==0x90)call("anim_start",{i(c+0x114),i(clip(c,0x54)),i(3),i(0)});if(!playing(0x18))st(0);}break;
  default:st(0);break;}
  return result;
}

// ---------------------------------------------------------------------------
// The Nightmare mount. Authored from docs/spec/horsemen-mount.md and
// tools/ref_horsemen_mount.py (research t3/research-horsemen-mount 397c35c).
// Every address and constant below is named in that document's sections 2-5
// and 8. The contents of the mount's property list (0x500848 + 12 v) are
// Unknown there; this code passes the pointer on and never reads the list.
// ---------------------------------------------------------------------------
namespace {
constexpr Raw DEF_NIGHTMARE = 0x500680;
constexpr Raw N_F_0_1 = 0x3dcccccc, N_F_0_2 = 0x3e4ccccc, N_F_0_3 = 0x3e999999, N_F_0_6 = 0x3f199999;
constexpr Raw N_F_0_8 = 0x3f4ccccc, N_F_0_9 = 0x3f666666, N_F_1_2 = 0x3f999999, N_F_1_5 = 0x3fc00000;
constexpr Raw N_F_10 = 0x41200000, N_F_24 = 0x41c00000, N_F_30 = 0x41f00000, N_F_36 = 0x42100000;
constexpr Raw N_F_42 = 0x42280000, N_F_120 = 0x42f00000, N_F_240 = 0x43700000, N_F_360 = 0x43b40000;
constexpr Raw N_F_480 = 0x43f00000, N_F_MINUS_20 = 0xc1a00000, N_F_GALLOP = 0x413aaaa9;
constexpr Raw G_WORLD = 0x44fdc8, G_NUM_PLAYERS = 0x4d8ae0;
}
void Model::in_reference_mode(const std::function<void()> &body) {
  Depth guard(depth_,limits_.depth);Depth contract(contract_depth_,limits_.depth);body();
}
Raw Model::linked_object(Address c) const {
  auto node=u32(c+0x3c8);return node?u32(Address(node)+0xc):0;
}
Raw Model::angle_to_target(Address c) {
  Depth guard(depth_,limits_.depth);raw_creature(c);
  auto node=u32(c+0x234);if(!node)return 0;
  auto t=Address(u32(Address(node)+0xc));
  auto dx=sub(u32(t+0x10),u32(c+0x10)),dy=sub(u32(t+0x14),u32(c+0x14));
  return Raw(std::int64_t(s16(Raw(std::int64_t(atan2_16(dy,dx))-std::int64_t(mem.load(c+0x38,2))))));
}
void Model::nightmare_ctor(Address c,Raw type_name,const Vec3 &pos,Raw facing,Raw props) {
  Depth guard(depth_,limits_.depth);raw_creature(c);
  creature_ctor(c,type_name,pos,Raw(std::int64_t(s16(facing))),DEF_NIGHTMARE,props,N_F_10);
  write(c+0x48,4,0x5155e8);write(c+0xb0,4,N_F_0_6);write(c+0x1c,4,N_F_30);write(c+0x28,1,0x32);
  write(c+0x3c8,4,0);write(c+0xb8,4,0);write(c+0x232,2,0);write(c+0xec,4,u32(FRAME));
  write(c+0x3c0,4,from_int(2400));write(c+0x5c,4,ONE);write(c+0xe0,4,N_F_MINUS_20);write(c+0xb4,1,10);
  write(c+0x3ae,2,2400);write(c+0xa8,8,mem.load(c+0xa8,8)|UINT64_C(0x20200040000));
  write(c+0xdc,4,N_F_1_2);write(c+0xf4,4,0);write(c+0x3cc,2,0);
  call("anim_clear",{i(c+0x114)});call("anim_push",{i(c+0x114),i(clip(c,0x1c)),i(1)});
  nightmare_set_state(c,0);
  for(Raw entry:{4u,8u})set_def_entry_params(c,entry,{1,2,2,4},{HALF,N_F_0_8,N_F_1_2,N_F_1_5});
  call("obj_runEvery60",{i(c)});call("Creature_init",{i(c)});
}
void Model::nightmare_draw(Address c) {
  Depth guard(depth_,limits_.depth);raw_creature(c);
  auto fade=[&]{return s16(mem.load(c+0x3cc,2));};
  if(fade())call("model_setFade",{i(std::uint64_t(std::int64_t(signed_byte(Raw(mem.load(c+0x3cc,1))))))});
  if(fade()<0x62){auto d=Address(u32(c+0x24));call("Creature_draw",{i(c),i(u32(d+0xc)),i(u32(d+0x14)),i(0)});}
  if(fade())call("model_resetDrawState",{});
}
void Model::nightmare_on_hit_terrain(Address c) {
  Depth guard(depth_,limits_.depth);raw_creature(c);
  if(s16(mem.load(c+0x232,2))==7)tick_virtual(c,27,{i(3)});
}
void Model::nightmare_on_touch_list(Address c,const std::vector<Raw> &touched) {
  Depth guard(depth_,limits_.depth);raw_creature(c);
  if(s16(mem.load(c+0x232,2))!=7)return;
  for(auto obj:touched) {
    if(!(u32(Address(obj)+0xc)&0x40))continue;
    if(integer(call("creature_inKnockdownAnim",{i(obj)})))continue;
    auto lo=from_int(Raw(std::int64_t(s32(Raw(2*std::int64_t(s16(mem.load(c+0xd0,2)))))))),
         hi=from_int(Raw(std::int64_t(s32(Raw(2*std::int64_t(s16(mem.load(c+0xd2,2))))))));
    call("damage_apply",{i(obj),i(c),i(100000),i(0),i(0),i(0x100041),i(0),i(0),i(lo),i(hi)});
    write(c+0xc,4,u32(c+0xc)|1);write(c+0x3d0,4,Raw(u32(c+0x300)+120));
  }
}
Raw Model::nightmare_pick_charge_target(Address c) {
  Depth guard(depth_,limits_.depth);raw_creature(c);
  // The table capacity / maximum player count is Unknown in the research.
  // history_entries is this host's chosen bound, not a game value.
  for(std::int32_t slot=0;slot<s32(u32(G_NUM_PLAYERS));++slot) {
    if(slot>=std::int32_t(limits_.history_entries))throw creature_hp::ResourceLimit("player scan limit");
    auto p=player_object(slot);
    if(!p||(u32(Address(p)+0xc)&8))continue;
    std::int32_t angle=0;bool hit=false;int k=0;
    for(;;) {
      p=player_object(slot);auto pos=vec(Address(p)+0x10);
      Vec3 start{add(pos[0],ZERO),pos[1],add(pos[2],N_F_24)};
      auto cs=cos16(Raw(angle)),sn=sin16(Raw(angle));
      Vec3 dir{mul(cs,N_F_360),mul(sn,N_F_360),ZERO};
      auto t=Raw(integer(call("terrain_raycast",{i(u32(G_WORLD)),start,dir,i(0),i(N_F_480)})));
      angle=s16(Raw(angle+0x2000));
      if(lt(ZERO,t)&&lt(t,ONE))hit=true;
      if(++k>=8||hit)break;
    }
    if(!hit) {
      call("handle_clear",{i(c+0x234)});p=player_object(slot);
      call("handle_link",{i(c+0x234),i(p),i(p)});return 1;
    }
  }
  return 0;
}
void Model::nightmare_seat_transform(Address c,Raw out) {
  Depth guard(depth_,limits_.depth);raw_creature(c);
  auto f=Raw(s16(mem.load(c+0x38,2)));
  auto x=add(u32(c+0x10),mul(u32(c+0xe0),cos16(f)));
  auto y=add(u32(c+0x14),mul(u32(c+0xe0),sin16(Raw(s16(mem.load(c+0x38,2))))));
  call("model_getAttachWorldMatrix",{i(class_model(c)),Vec3{x,y,u32(c+0x18)},i(std::uint64_t(std::int64_t(s16(mem.load(c+0x38,2))))),
    i(c+0x114),i(10),i(out),i(u32(c+0xdc))});
}
void Model::nightmare_set_state(Address c,Raw requested) {
  Depth guard(depth_,limits_.depth);raw_creature(c);
  auto filtered=filter_state(c,requested);if(filtered.refused)return;
  auto state=Raw(filtered.state);auto anim=c+0x114;
  auto start=[&](Raw off,Raw flags,Raw blend){call("anim_start",{i(anim),i(clip(c,off)),i(flags),i(blend)});};
  auto ride=[&](Raw rider_state){if(u32(c+0x3c8))tick_virtual(Address(linked_object(c)),27,{i(rider_state)});};
  switch(state) {
  case 0x19:write(c+0xc,4,u32(c+0xc)|0xb);write(c+0xf4,4,Raw(u32(c+0x300)+300));ride(0x19);break;
  case 0x1a:write(c+0xc,4,u32(c+0xc)&0xfffffff4);write(c+0x3cc,2,120);start(0x1c,3,N_F_0_1);ride(0x1a);break;
  case 0:{auto d=own_draw(c);write(c+0xf4,4,Raw(u32(c+0x300)+d%120));start(0x1c,3,N_F_0_3);ride(0x1b);break;}
  case 1:{clear_path(c);pick_wander_dest(c);auto d=own_draw(c);write(c+0xf4,4,Raw(u32(c+0x300)+d%120+300));
    start(0x3c,0x803,N_F_0_3);call("anim_syncToSimFrame",{i(anim)});ride(0x1c);break;}
  case 3:start(0x3c,0x803,N_F_0_1);call("anim_syncToSimFrame",{i(anim)});ride(0x1c);break;
  case 5:start(0x1c,6,N_F_0_3);ride(0x1b);break;
  case 6:case 0x17:case 0x18:
    call("anim_startBlendInOut",{i(anim),i(clip(c,0x44)),i(8),i(N_F_0_1),i(N_F_0_1)});
    ride(state==0x18?0x1f:0x1e);write(c+0x3d4,4,0);break;
  case 7:
    call("anim_startBlendInOut",{i(anim),i(clip(c,0x44)),i(0x24),i(N_F_0_1),i(ZERO)});
    call("anim_pushChained",{i(anim),i(clip(c,0x2c)),i(0x80b)});ride(0x1d);break;
  case 8:if(u32(c+0x3c8)){auto d=own_draw(c);tick_virtual(Address(linked_object(c)),27,{i(d%3+0x20)});}break;
  case 0x12:state=Raw(s16(mem.load(c+0x232,2)));ride(0x1b);break;
  case 0x13:start(0x1c,3,N_F_0_1);ride(0x1b);break;
  case 0x10:write(c+0x318,2,60);clear_path(c);start(0x3c,0x803,N_F_0_1);call("anim_syncToSimFrame",{i(anim)});ride(0x1d);break;
  case 0x14:
    write(c+0xc,4,u32(c+0xc)|0x4000);tick_virtual(c,28,{i(0)});start(0x2c,0x806,N_F_0_3);
    if(u32(c+0x3c8)){tick_virtual(Address(linked_object(c)),27,{i(0x23)});call("handle_clear",{i(c+0x3c8)});write(c+0x3c8,4,0);}
    break;
  default:break;
  }
  write(c+0x232,2,state);
}
void Model::nightmare_run(Address c) {
  Depth guard(depth_,limits_.depth);raw_creature(c);Depth tick(tick_depth_,limits_.depth);
  auto st=[&](Raw state){tick_virtual(c,27,{i(state)});};
  process_damage(c,class_model(c));
  if(creature_run(c,class_model(c)))return;
  update_active(c);
  auto speed=s16(mem.load(c+0x232,2))==7&&(mem.load(c+0x11a,2)&0x20)?N_F_0_9:ONE;
  auto ev=step_anim(c,speed);
  call("Creature_applyKnockback",{i(c),i(N_F_0_2)});tick_virtual(c,23,{i(0)});
  if(s16(mem.load(c+0x232,2))!=0x14&&s32(u32(c+0x3d0))<s32(u32(c+0x300)))write(c+0xc,4,u32(c+0xc)&0xfffffffe);
  auto state=s16(mem.load(c+0x232,2));
  auto facing=[&]{return Raw(std::int64_t(s16(mem.load(c+0x38,2))));};
  auto see=[&](Raw f,Raw arc,Raw far){return can_see_target_in(c,f,arc,0,ZERO,far,ZERO)!=0;};
  auto cone=[&](Raw arc,Raw far){return integer(call("creature_findEnemyInCone",{i(c),i(facing()),i(arc),i(0),i(far)}))!=0;};
  auto flags=[&]{return mem.load(c+0x11a,2);};
  auto kick=[&](Raw hit_flags){
    call("Creature_hitAllEnemiesInArc",{i(c),i(facing()),i(0x1194),i(std::uint64_t(2*std::int64_t(s16(mem.load(c+0xd0,2))))),
      i(std::uint64_t(2*std::int64_t(s16(mem.load(c+0xd2,2))))),i(std::uint64_t(std::int64_t(s16(mem.load(c+0xd4,2))))),i(hit_flags),i(0),i(ZERO),i(N_F_42)});};
  if(state<0||state>=0x1b)return;
  switch(state) {
  case 0:if(integer(tick_virtual(c,25,{i(0)})))return st(3);if(is_away_from_home(c))return st(1);return;
  case 1:if(integer(tick_virtual(c,25,{i(0)})))return st(3);if(s32(u32(c+0xf4))<s32(u32(c+0x300)))return st(0);move_to_dest(c);return;
  case 3:
    if(!u32(c+0x234))return st(0);
    if(cone(4000,N_F_36))return st(5);
    if(see(facing(),0x7fff,N_F_36))return st(5);
    if(nightmare_pick_charge_target(c)&&cone(2000,N_F_240))return st(7);
    if(see(facing(),0x7fff,N_F_120)&&!see(facing(),6000,N_F_120))return st(0x17);
    move_to_target(c);return;
  case 5: {
    if(!u32(c+0x234))return st(0);
    if(cone(4000,N_F_36))return st(6);
    if(see(0,0x7fff,N_F_36)) {
      auto a=[&]{return std::int64_t(s32(angle_to_target(c)));};
      if(std::llabs(a())<4000)return st(6);
      if(std::llabs(a()+0x4000)<6000)return st(8);
      if(std::llabs(a()+20000)<6000)return st(0x18);
      return st(0x17);
    }
    if(see(facing(),0x7fff,N_F_120)&&!see(facing(),6000,N_F_120))return st(0x17);
    if(is_not_rooted(c))return st(3);
    return;
  }
  case 6:case 0x17:case 0x18:
    settle(c);
    if(ev&3)kick(1);
    if(ev&4)kick(0x100041);
    if(ev&1)write(c+0x3d4,4,1);
    if(ev&4)write(c+0x3d4,4,0);
    if(u32(c+0x3d4))turn_to_target(c);
    if(flags()&8)return;
    return st(3);
  case 7:
    if(flags()&0x20)turn_to_target(c);else if(see(facing(),6000,0x45160000))turn_to_target(c);
    move_by(c,{});
    if((mem.load(c+0x118,8)&0x180000)==0x80000) {
      auto pos=vec(c+0x10);Vec3 start{add(pos[0],ZERO),pos[1],add(pos[2],N_F_24)};
      auto cs=cos16(Raw(s16(mem.load(c+0x38,2)))),sn=sin16(Raw(s16(mem.load(c+0x38,2))));
      Vec3 dir{mul(cs,N_F_360),mul(sn,N_F_360),ZERO};
      auto t=Raw(integer(call("terrain_raycast",{i(u32(G_WORLD)),start,dir,i(0),i(N_F_480)})));
      if(lt(ZERO,t)&&lt(t,ONE)) {
        call("anim_start",{i(c+0x114),i(clip(c,0x3c)),i(0x803),i(N_F_0_1)});call("anim_syncToSimFrame",{i(c+0x114)});
        call("anim_startBlendInOut",{i(c+0x114),i(clip(c,0x34)),i(0x810),i(N_F_0_1),i(N_F_0_1)});
      }
    }
    if(flags()&0x18)return;
    return st(3);
  case 8:
    if(!u32(c+0x3c8))return st(0);
    if(mem.load(Address(linked_object(c))+0x11a,2)&8)return;
    return st(5);
  case 0x10:if(is_attack_ready(c))return st(3);flee_from_target(c,0);return;
  case 0x13:if(is_not_stunned(c))return st(0);return;
  case 0x14: {
    auto f=Raw(s16(mem.load(c+0x38,2)));auto cs=cos16(f),sn=sin16(Raw(s16(mem.load(c+0x38,2))));
    auto dy=mul(sn,N_F_GALLOP),dx=mul(cs,N_F_GALLOP);
    write(c+0x14,4,add(u32(c+0x14),dy));write(c+0x10,4,add(u32(c+0x10),dx));
    call("obj_updateGridCell",{i(c)});write(c+0x3cc,2,mem.load(c+0x3cc,2)+1);
    call("Creature_deadUpdate",{i(c),i(ev),Vec3{},i(1),i(N_F_30)});
    if(s16(mem.load(c+0x3cc,2))>=100){write(c+0xc,4,u32(c+0xc)|0x4002);call("obj_delete",{i(c)});}
    return;
  }
  case 0x1a:
    if(s16(mem.load(c+0x3cc,2))>0){write(c+0x3cc,2,mem.load(c+0x3cc,2)-1);return;}
    return st(0);
  default:return;
  }
}

} // namespace champions::simulation::rta::arena_monsters
