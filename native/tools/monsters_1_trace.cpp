// Input-only replay. Expected fixture columns are never read by this runner.
#include "../tests/monsters_1_transport.hpp"
#include <fstream>
#include <iostream>
namespace X = champions::test::monsters_1_transport;
namespace M = champions::simulation::rta::monsters_1;
namespace R = champions::simulation::rta;
using X::n; using X::sequence; using X::text;
namespace {
namespace V = champions::simulation::rta::creature_movement;
std::string structured(const M::Structured &v,bool typed=true) {
  switch(v.kind) {
  case V::ValueKind::None:return "_";
  case V::ValueKind::Integer:return n(v.scalar);
  case V::ValueKind::Vector: {
    auto value=X::integers(v.vec);return typed?sequence({text("tuple"),value}):value;
  }
  case V::ValueKind::Buffer: {
    std::vector<std::string> bytes;for (unsigned char c:v.bytes) bytes.push_back(n(c));
    auto value=sequence({text("buf"),sequence({text("bytes"),sequence(bytes)})});
    return typed?sequence({text("tuple"),value}):value;
  }
  default: {
    std::vector<std::string> items;for (const auto &a:v.items) items.push_back(structured(a,false));
    auto value=sequence(items);
    return typed && v.kind==V::ValueKind::Tuple?sequence({text("tuple"),value}):value;
  }
  }
}
std::string result_text(const M::Result &value) {
  if (const auto *integer=std::get_if<std::uint64_t>(&value)) return n(*integer);
  if (const auto *buffer=std::get_if<M::BufferResult>(&value)) {
    auto payload=structured(buffer->payload,false);
    if (buffer->payload.kind==V::ValueKind::Buffer) {
      std::vector<std::string> bytes;for (unsigned char c:buffer->payload.bytes) bytes.push_back(n(c));
      payload=sequence({text("bytes"),sequence(bytes)});
    }
    return sequence({text("buf"),payload});
  }
  if (const auto *compound=std::get_if<M::CompoundResult>(&value)) {
    auto head=std::visit([](const auto &v) -> M::Result {return v;},compound->head);
    std::vector<std::string> items{result_text(head)};
    for (const auto &v:compound->outputs) items.push_back(structured(v,false));
    return sequence(items);
  }
  return structured(std::get<M::Structured>(value),false);
}
M::Structured decode(const X::Value &v,unsigned depth=0) {
  if (depth>4) throw std::runtime_error("structured recipe depth");
  const auto &a=v.bounded_array(17);if (a.empty()) throw std::runtime_error("empty value recipe");
  auto tag=a[0].integer<unsigned>();
  if (tag==0) { v.array(1);return {}; }
  if (tag==1) { v.array(2);return V::Value::integer(a[1].integer<std::int64_t>()); }
  if (tag==2) { v.array(4);return V::Value::vector({a[1].integer<std::uint32_t>(),a[2].integer<std::uint32_t>(),a[3].integer<std::uint32_t>()}); }
  if (tag==5) { std::string bytes;for (unsigned j=1;j<a.size();++j) bytes+=static_cast<char>(a[j].integer<std::uint8_t>());return V::Value::buffer(bytes); }
  if (tag!=3 && tag!=4) throw std::runtime_error("unknown value recipe");
  std::vector<M::Structured> items;for (unsigned j=1;j<a.size();++j) items.push_back(decode(a[j],depth+1));
  return tag==3?V::Value::list(items):V::Value::tuple(items);
}
const std::vector<std::string> names = {"handle_clear","anim_start","decal_add","anim_jumpTopToEnd","Creature.v27","Creature.v28","terrain_heightBelow","model_buildHideMask","pool_new","GutChunk_ctorFromProp","obj_getPropStr","script_runByName","loot_spawnNamedItem","obj_delete","arena_addKill","creature_stopBossMusic","anim_startBlendInOut","anim_syncToSimFrame","Creature_dropLoot","GutChunk_ctor","PhProp_initPhysics","fx_spawn","GameObject_ctor","obj_getPropInt","obj_moveOffGrid","balance_lookup","Creature_setStats","anim_push","anim_randomizeTopTime","obj_runEvery60","Creature_init","anim_clear","obj_runEveryFrame","creature_canSeeObj","Creature_deadUpdate","Creature_hitTargetAtReach","Creature_hitAllEnemiesInArc","Creature.v26","water_disturb","water_test","libc_sprintf","res_find","sound_playOnObject","world_slideMove","obj_updateGridCell","world_collectObjectsInBox","Creature.v6","Creature.v10","Creature.v11","creature_findEnemyInCone","world_raycast","path_requestToPoint","world_findFreeSpot","SpellDrowsy_typeinfo","creature_findSpellByType","SpellDrowsy_getStat3Fraction","Creature.v16","Creature.v17","Creature.v19","Creature.v2","Creature.v20","Creature.v23","Creature.v25","Creature.v30","Creature.v31","Creature.v34","Creature.v35","Creature.v36","Creature.v37","Creature.v7","Creature_applyKnockback","Creature_hitTargetInArc","Creature_hitTargetInArcEdge","Creature_setCharm","Critter_ctor","GroundPoundEffect_ctor","Holder.v1","Holder.v11","Holder.v15","Holder.v17","Holder.v2","Holder.v21","Holder.v22","Holder.v4","Holder.v6","LightEffect_ctor","Nightmare_ctor","OniBall_ctor","Skill.v1","Skill.v11","Skill.v12","Skill.v14","Skill.v2","Skill.v8","SkillLifeTap_getStat0Fraction","Spell.v1","Spell.v2","anim_advance","anim_releaseCaches","anim_snapTopToEnd","combat_applyAreaDamage","creature_addSpell","creature_bossRank","creature_countAlliesNear","creature_findSpell","creature_isBlockedBy","damage_apply","fx_burnModel","fx_disintegrateModel","fx_ringBurst","fx_shatterToLooseIce","fx_shockModel","handle_link","handle_removeNode","iter_begin","iter_end","iter_next","lump_load","model_getAttachPoint","model_getBounds","model_getSamplePoint","model_getSamplePointCount","net_objOrGhost","particleDef_ctor","particle_spawn","pathFollower_init","path_requestType1","path_requestType2","path_requestType3","paths_findByName","player_getSkillRank","player_giveExp","player_speedScale","players_anyWithinRadius","pointList_getPoint","pointList_snapToNearest","sound_playAtPos","terrain_height","terrain_raycast","world_collectInSphere","world_collectObjectsInArc","world_findObjectByName","world_notifyObjectsNear"};
std::string call(const M::Call &c) {
  std::vector<std::string> args;
  for (const auto &a:c.args) {
    if (auto i=std::get_if<std::uint32_t>(&a)) args.push_back(n(*i));
    else if (auto v=std::get_if<M::Vec3>(&a)) args.push_back(sequence({text("tuple"),X::integers(*v)}));
    else if (auto d=std::get_if<M::Descriptor>(&a)) args.push_back(sequence({text("tuple"),sequence({text("desc"),sequence({X::integers(d->effect),"_","_","_"})})}));
    else if (auto tagged=std::get_if<M::TaggedTuple>(&a)) {
      std::vector<std::string> fields{text(tagged->tag)};
      for (const auto &v:tagged->fields) fields.push_back(structured(v,false));
      args.push_back(sequence({text("tuple"),sequence(fields)}));
    }
    else if (auto str=std::get_if<std::string>(&a)) args.push_back(text(*str));
    else args.push_back(structured(std::get<M::Structured>(a)));
  }
  return sequence({text(c.name),sequence(args)});
}
std::string write(const M::Write &w) { return sequence({n(w.address),n(w.size),n(w.value)}); }
std::string snapshot(const M::Model &m,const std::vector<unsigned> &counts) {
  std::vector<std::string> bytes,stores,writes,calls,events;
  for (auto [a,v]:m.mem.bytes()) bytes.push_back(sequence({n(a),n(v)}));
  for (auto w:m.mem.log()) stores.push_back(write(w));
  for (auto w:m.writes) writes.push_back(write(w));
  for (const auto &c:m.calls) calls.push_back(call(c));
  for (const auto &e:m.events) {
    if (e.is_call) {
      auto c=call(e.call);
      events.push_back("["+text("call")+","+c.substr(1));
    } else events.push_back(sequence({text("write"),n(e.write.address),n(e.write.size),n(e.write.value)}));
  }
  return sequence({sequence(bytes),sequence(stores),sequence(writes),sequence(calls),sequence(events),n(m.draws),X::integers(counts)});
}
void poke(M::Memory &mem,const X::Value &v,bool logged=false) {
  const auto &a=v.array(3);
  auto addr=a[0].integer<std::uint32_t>();auto size=a[1].integer<unsigned>();auto value=a[2].integer<std::uint64_t>();
  if (size>8 || static_cast<std::uint64_t>(addr)+size>UINT64_C(0x100000000)) throw std::runtime_error("invalid poke span");
  if (logged) mem.store(addr,size,value); else mem.poke(addr,size,value);
}
std::string execute(unsigned config,const std::string &input) {
  if (config>=16) throw std::runtime_error("invalid FPU configuration");
  R::set_fpu(static_cast<R::Fpu::Model>(config/4),(config&2)!=0,(config&1)!=0);
  auto parsed=X::Parser(input).parse();const auto &fields=parsed.array(4);
  M::Memory memory;M::Model model(memory);std::vector<unsigned> counts(16);
  bool generic=false,ordinary=false;
  for (const auto &command:fields[3].bounded_array(128)) {
    const auto &op=command.bounded_array(8);
    if (!op.empty() && op[0].integer<unsigned>()==22) generic=true;
    if (!op.empty() && op[0].integer<unsigned>()>=23) ordinary=true;
  }
  for (const auto &spec:fields[1].bounded_array(143)) {
    const auto &s=spec.array(4);auto id=s[0].integer<unsigned>();
    if (id>=names.size() || model.callbacks.count(names[id])) throw std::runtime_error("invalid callback id");
    if (counts.size()<=id) counts.resize(id+1);
    auto mutations=s[2].bounded_array(128);auto fail=s[3].integer<std::int32_t>();
    auto returns=s[1].bounded_array(128);
    if (returns.empty()) throw std::runtime_error("empty callback returns");
    if (id>=33 || ((id==4 || id==5) && returns[0].list)) {
      std::vector<M::Structured> values;for (const auto &r:returns) values.push_back(decode(r));
      auto handler=[&,id,values,mutations,fail](M::Model &m,const auto &) {
        auto index=counts[id]++;
        for (const auto &row:mutations) poke(m.mem,row,true);
        if (static_cast<std::int64_t>(index)==fail) throw M::Error("authored callback failure");
        return values[index<values.size()?index:values.size()-1];
      };
      if (id==4 || id==5 || id==40 || id==43 || id==45 || id==52 || id>=56 || generic || ordinary) model.structured_callbacks[names[id]]=handler;
      else model.callbacks[names[id]]=[handler](auto &m,const auto &args) {
        auto v=handler(m,args);if (v.kind!=V::ValueKind::Integer) throw M::Error("expected integer callback return");
        return static_cast<std::uint64_t>(v.scalar);
      };
    } else if (names[id]=="balance_lookup") {
      std::vector<M::Model::Balance> factors;
      for (const auto &r:returns) {
        M::Model::Balance f{};const auto &items=r.array(6);
        for (unsigned k=0;k<6;++k) f[k]=items[k].integer<std::uint32_t>();
        factors.push_back(f);
      }
      model.balance_lookup=[&,id,factors,mutations,fail](M::Model &m,auto) {
        auto index=counts[id]++;
        for (const auto &row:mutations) poke(m.mem,row,true);
        if (static_cast<std::int64_t>(index)==fail) throw M::Error("authored callback failure");
        return factors[index<factors.size()?index:factors.size()-1];
      };
    } else {
      std::vector<std::uint64_t> values;
      for (const auto &r:returns) values.push_back(r.integer<std::uint64_t>());
      model.callbacks[names[id]]=[&,id,values,mutations,fail](M::Model &m,const auto &) {
        auto index=counts[id]++;
        for (const auto &row:mutations) poke(m.mem,row,true);
        if (static_cast<std::int64_t>(index)==fail) throw M::Error("authored callback failure");
        return values[index<values.size()?index:values.size()-1];
      };
    }
  }
  for (const auto &v:fields[2].bounded_array(38)) {
    auto slot=v.integer<unsigned>();if (slot>37 && (slot<100 || slot>114)) throw std::runtime_error("invalid external slot");
    model.external.insert(slot>=100?"Skill.v"+n(slot-100):"Creature.v"+n(slot));
  }
  for (const auto &v:fields[0].bounded_array(512)) poke(memory,v);
  auto initial=snapshot(model,counts);std::vector<std::string> steps;
  for (const auto &v:fields[3].bounded_array(128)) {
    const auto &op=v.bounded_array(8);if (op.empty()) throw std::runtime_error("empty opcode");
    auto code=op[0].integer<unsigned>();std::string status="ok",result="_";
    try {
      if (code<5 || (code>=6 && code<=9)) {
        v.array(3);auto a=op[1].integer<std::uint32_t>();auto b=op[2].integer<std::uint32_t>();
        switch(code) {
        case 0:model.smallspider_set_state(a,b);break;
        case 1:model.smallspider_on_touch(a,b);break;
        case 2:model.mosquito_lose_part(a,b);break;
        case 3:model.scorpion_sever_part(a,b);break;
        case 4:result=n(model.str_prefix_differs(a,b));break;
        case 6:model.gnaw_set_state(a,b);break;
        case 7:model.mosquito_set_state(a,b);break;
        case 8:model.scorpion_set_state(a,b);break;
        case 9:model.badger_set_state(a,b);break;
        }
      } else if (code>=10 && code<=14) {
        v.array(code==14?7:6);
        auto a=op[1].integer<std::uint32_t>();
        unsigned vi=code==12?2:3;const auto &pv=op[vi].array(3);
        M::Vec3 pos{};for (unsigned k=0;k<3;++k) pos[k]=pv[k].integer<std::uint32_t>();
        auto name=op[code==12?5:2].integer<std::uint32_t>();
        auto facing=op[code==12?3:4].integer<std::uint32_t>();
        auto props=op[code==12?4:5].integer<std::uint32_t>();
        switch(code) {
        case 10:model.smallspider_ctor(a,name,pos,facing,props);break;
        case 11:model.gnaw_ctor(a,name,pos,facing,props);break;
        case 12:model.mosquito_ctor(a,pos,facing,props,name);break;
        case 13:model.scorpion_ctor(a,name,pos,facing,props);break;
        case 14:model.badger_ctor(a,name,pos,facing,props,op[6].integer<std::uint32_t>());break;
        }
      } else if (code>=15 && code<=19) {
        auto a=op.at(1).integer<std::uint32_t>();
        if (code==15) {
          if (op.size()!=5 && op.size()!=6) throw std::runtime_error("sight opcode shape");
          result=n(model.see(a,op[2].integer<std::uint32_t>(),op[3].integer<std::uint32_t>(),op[4].integer<std::uint32_t>(),
            op.size()==6 ? std::optional<R::Raw>(op[5].integer<std::uint32_t>()) : std::nullopt));
        } else if (code==16) {
          v.array(6);auto pv=op[5].array(3);M::Vec3 push{};
          for (unsigned k=0;k<3;++k) push[k]=pv[k].integer<std::uint32_t>();
          model.dead_update(a,op[2].integer<std::uint32_t>(),op[3].integer<std::uint32_t>(),op[4].integer<std::uint32_t>(),push);
        } else if (code==17) {v.array(2);result=n(model.badger_pick(a));}
        else {v.array(3);auto ev=op[2].integer<std::uint32_t>();if (code==18) model.scorpion_attack(a,ev);else model.badger_attack(a,ev);}
      } else if (code==20) {
        v.array(2);const auto value=model.badger_chase(op[1].integer<std::uint32_t>());
        result=value ? n(*value) : "_";
      } else if (code>=23 && code<=27) {
        v.array(2);auto c=op[1].integer<std::uint32_t>();std::optional<std::uint64_t> value;
        switch(code) {case 23:value=model.smallspider_run(c);break;case 24:value=model.gnaw_run(c);break;
        case 25:value=model.mosquito_run(c);break;case 26:value=model.scorpion_run(c);break;case 27:value=model.badger_run(c);break;}
        result=value?n(*value):"_";
      } else if (code==28) {
        v.array(6);M::Vec3 pos{};const auto &pv=op[2].array(3);
        for (unsigned k=0;k<3;++k) pos[k]=pv[k].integer<std::uint32_t>();
        model.skelight_ctor(op[1].integer<std::uint32_t>(),pos,op[3].integer<std::uint32_t>(),op[4].integer<std::uint32_t>(),op[5].integer<std::uint32_t>());
      } else if (code==29) {
        v.array(2);auto r=model.skelight_run(op[1].integer<std::uint32_t>());result=r?n(*r):"_";
      } else if (code==30 || code==31) {
        v.array(3);auto c=op[1].integer<std::uint32_t>(),arg=op[2].integer<std::uint32_t>();
        if (code==30) model.skelight_set_state(c,arg);else model.skelight_shatter(c,arg);
      } else if (code==32) {
        v.array(4);std::vector<M::Argument> args;
        for (const auto &a:op[3].bounded_array(8)) {auto value=decode(a);
          if (value.kind==V::ValueKind::Integer) args.emplace_back(static_cast<R::Raw>(value.scalar));
          else if (value.kind==V::ValueKind::Vector) args.emplace_back(value.vec);
          else args.emplace_back(value);
        }
        auto r=model.skelight_vcall(op[1].integer<std::uint32_t>(),op[2].integer<unsigned>(),std::move(args));result=r?n(*r):"_";
      } else if (code>=33 && code<=39) {
        auto word=[&](unsigned k) {return op.at(k).integer<std::uint32_t>();};
        if (code==33) {v.array(3);result=n(model.summon_can_use(word(1),word(2)));}
        else if (code==34 || code==35) {
          v.array(6);M::Vec3 pos{};auto pv=op[4].array(3);for (unsigned k=0;k<3;++k) pos[k]=pv[k].integer<std::uint32_t>();
          result=n(code==34?model.summon_cast(word(1),word(2),word(3),pos,word(5)):model.tagar_cast(word(1),word(2),word(3),pos,word(5)));
        } else if (code==36 || code==37) {
          v.array(6);if (code==36) model.spell_base_ctor(word(1),word(2),word(3),word(4),word(5));
          else model.summon_spell_ctor(word(1),word(2),word(3),word(4),word(5));
        } else if (code==38) {v.array(5);model.tagar_spell_ctor(word(1),word(2),word(3),word(4));}
        else {
          v.array(4);std::vector<M::Argument> args;
          for (const auto &a:op[3].bounded_array(8)) {auto value=decode(a);
            if (value.kind==V::ValueKind::Integer) args.emplace_back(static_cast<R::Raw>(value.scalar));
            else if (value.kind==V::ValueKind::Vector) args.emplace_back(value.vec);
            else args.emplace_back(value);
          }
          result=n(model.arena_skill_call(word(1),word(2),std::move(args)));
        }
      } else if (code>=40 && code<=44) {
        auto word=[&](unsigned k) {return op.at(k).integer<std::uint32_t>();};
        if (code==40) {v.array(2);auto r=model.horsemen_run(word(1));result=r?n(*r):"_";}
        else if (code==41) {v.array(3);model.horsemen_set_state(word(1),word(2));}
        else if (code==42) {v.array(2);model.horsemen_cast(word(1));}
        else if (code==43) {
          v.array(4);std::vector<M::Argument> args;
          for (const auto &a:op[3].bounded_array(8)) {auto value=decode(a);
            if (value.kind==V::ValueKind::Integer) args.emplace_back(static_cast<R::Raw>(value.scalar));
            else if (value.kind==V::ValueKind::Vector) args.emplace_back(value.vec);
            else args.emplace_back(value);
          }
          auto r=model.horsemen_vcall(word(1),word(2),std::move(args));result=r?n(*r):"_";
        } else {
          v.array(8);M::Vec3 pos{};auto pv=op[3].array(3);for (unsigned k=0;k<3;++k) pos[k]=pv[k].integer<std::uint32_t>();
          model.horsemen_ctor(word(1),word(2),pos,word(4),word(5),word(6),word(7));
        }
      } else if (code>=45 && code<=51) {
        auto word=[&](unsigned k) {return op.at(k).integer<std::uint32_t>();};
        if (code==45) {
          v.array(6);M::Vec3 pos{};auto pv=op[3].array(3);for (unsigned k=0;k<3;++k) pos[k]=pv[k].integer<std::uint32_t>();
          model.zek_ctor(word(1),word(2),pos,word(4),word(5));
        } else if (code==46) {v.array(2);auto r=model.zek_run(word(1));result=r?n(*r):"_";}
        else if (code==47) {v.array(3);model.zek_set_state(word(1),word(2));}
        else if (code==48) {v.array(5);model.zek_take_damage(word(1),word(2),word(3),word(4));}
        else if (code==49) {v.array(2);result=n(model.zek_slot35(word(1)));}
        else if (code==50) {v.array(2);model.zek_wave(word(1));}
        else {
          v.array(4);std::vector<M::Argument> args;
          for (const auto &a:op[3].bounded_array(8)) {auto value=decode(a);
            if (value.kind==V::ValueKind::Integer) args.emplace_back(static_cast<R::Raw>(value.scalar));
            else if (value.kind==V::ValueKind::Vector) args.emplace_back(value.vec);
            else args.emplace_back(value);
          }
          auto r=model.zek_vcall(word(1),word(2),std::move(args));
          // The model's virtual ABI retains raw64 signed return patterns.
          result=r?n(*r):"_";
        }
      } else if (code==52) {
        v.array(4);std::vector<M::Argument> args;
        for (const auto &a:op[3].bounded_array(32)) {auto value=decode(a);
          if (value.kind==V::ValueKind::Integer) args.emplace_back(static_cast<R::Raw>(value.scalar));
          else if (value.kind==V::ValueKind::Vector) args.emplace_back(value.vec);
          else args.emplace_back(value);
        }
        result=result_text(model.vcall(op[1].integer<std::uint32_t>(),op[2].integer<unsigned>(),std::move(args)));
      } else if (code==21 || code==22) {
        v.array(code==21?6:3);
        const auto id=op[1].integer<unsigned>();if (id>=names.size()) throw std::runtime_error("ABI name ID");
        std::vector<M::Argument> args;
        for (const auto &arg:op[2].bounded_array(32)) args.emplace_back(decode(arg));
        M::Result value;
        if (code==21) {
          std::optional<std::vector<M::Structured>> outs;
          if (op[5].list) {
            outs.emplace();for (const auto &out:op[5].bounded_array(32)) outs->push_back(decode(out));
          } else if (op[5].integer<unsigned>()!=0) throw std::runtime_error("outs shape: "+op[5].canonical());
          auto head=[](const X::Value &v) -> M::HeadResult {
            if (v.negative) return M::Structured::integer(v.integer<std::int64_t>());
            return v.integer<std::uint64_t>();
          };
          value=M::shape_result(names[id],args,head(op[3]),head(op[4]),outs);
        } else {
          model.policy=M::CallbackPolicy::ReferenceZero;
          value=model.call_value(names[id],std::move(args));
        }
        result=result_text(value);
      } else if (code==5) {
        v.array(4);X::Value row;row.list=true;row.items={op[1],op[2],op[3]};poke(memory,row);
      } else throw std::runtime_error("unknown opcode");
    } catch (const M::Error &) { status="ValueError"; }
    steps.push_back(sequence({text(status),result,snapshot(model,counts)}));
  }
  return sequence({initial,sequence(steps)});
}
}
int main(int argc,char **argv) {
  try {
    if (argc!=2) throw std::runtime_error("usage: monsters-1-trace INPUT.tsv");
    std::ifstream input(argv[1]);if (!input) throw std::runtime_error("cannot open input");
    std::string line;
    while (std::getline(input,line)) {
      if (line.size()>8*1024*1024) throw std::runtime_error("row budget");
      const auto first=line.find('\t'),second=line.find('\t',first+1);
      if (first==line.npos || second==line.npos) throw std::runtime_error("invalid row");
      const auto config=X::Parser(line.substr(0,first)).parse().integer<unsigned>();
      std::cout << execute(config,line.substr(first+1,second-first-1)) << '\n';
    }
    if (!input.eof() || !std::cout) throw std::runtime_error("transport failure");
  } catch(const std::exception &e) {std::cerr << e.what() << '\n';return 1;}
}
