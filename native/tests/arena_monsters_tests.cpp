#include "champions/arena_monsters.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
namespace A = champions::simulation::rta::arena_monsters;
using U = std::uint64_t;
std::vector<std::string> split(const std::string &s,char sep) {
  std::vector<std::string> out; std::size_t p=0;
  for (;;) { auto q=s.find(sep,p);out.push_back(s.substr(p,q-p));if(q==s.npos)break;p=q+1; }
  return out;
}
std::vector<std::vector<U>> rows(const std::string &s) {
  std::vector<std::vector<U>> out;if(s=="-")return out;
  for(auto &row:split(s,';')) { std::vector<U> r;for(auto &n:split(row,','))r.push_back(std::stoull(n));out.push_back(r); }
  return out;
}
std::string write(const A::Write &w) {
  return "w("+std::to_string(w.address)+","+std::to_string(w.size)+","+std::to_string(w.value)+")";
}
std::string val(const A::Value &v) {
  if(auto p=std::get_if<A::CallbackResult>(&v)){std::string out="v";for(const auto &e:p->elements){if(out.size()>1)out+=',';out+=std::to_string(e.magnitude);}return out;}
  if(std::holds_alternative<std::monostate>(v))return "n";
  if(auto p=std::get_if<A::Element>(&v))return "e"+std::to_string(p->low)+","+std::to_string(p->high);
  if(auto p=std::get_if<std::int64_t>(&v))return "i"+std::to_string(*p);
  if(auto p=std::get_if<std::string>(&v))return "s"+*p;
  if(auto p=std::get_if<A::Contacts>(&v)){std::string out="v";for(auto n:p->values){if(out.size()>1)out+=',';out+=std::to_string(n);}return out;}
  const auto &p=std::get<A::Vec3>(v);
  return "v"+std::to_string(p[0])+","+std::to_string(p[1])+","+std::to_string(p[2]);
}
std::string call(const A::Call &c) {
  std::string s="c"+c.name+"(";bool first=true;
  for(auto &v:c.args) {if(!first)s+=';';first=false;s+=val(v);}return s+")";
}
template<class T,class F> std::string join(const T &v,F f,char sep=';') {
  std::string s;bool first=true;for(auto &x:v){if(!first)s+=sep;first=false;s+=f(x);}return s;
}
std::string snapshot(A::Model &m,const std::vector<std::string> &results) {
  return join(m.mem.bytes(),[](const auto &p){return std::to_string(p.first)+":"+std::to_string(p.second);},',')+"|"+
      join(m.mem.log(),write)+"|"+join(m.writes,write)+"|"+join(m.calls,call)+"|"+
      join(m.events,[](const auto &e){if(auto p=std::get_if<A::Write>(&e))return write(*p);return call(std::get<A::Call>(e));})+"|"+
      std::to_string(m.draws)+"|"+join(results,[](const auto &s){return s;});
}
struct CallbackError : std::runtime_error { CallbackError():std::runtime_error("synthetic callback failure"){} };
constexpr U C=0x10000,D=0x20000;
std::int64_t signed64(U n) {return n<=INT64_MAX?std::int64_t(n):-1-std::int64_t(~n);}
void install(A::Model &m,const std::vector<U> &options) {
  auto counter=std::make_shared<unsigned>(0);
  for (auto name:{"handle_clear","anim_start","anim_startBlendInOut","anim_syncToSimFrame","Creature.v16",
                 "model_getSamplePointCount","model_getSamplePoint","fx_spawn","obj_delete",
                 "GameObject_ctor","obj_getPropInt","obj_getPropStr","obj_moveOffGrid","balance_lookup",
                 "Creature_setStats","Creature.v26","creature_canSeeObj","creature_countAlliesNear",
                 "creature_isBlockedBy","script_runByName","loot_spawnNamedItem","anim_jumpTopToEnd",
                 "arena_addKill","creature_stopBossMusic"}) {
    m.callbacks[name]=[options,counter,name](A::Model &model,const auto &args) -> A::Value {
      auto n=++*counter;
      if(options[1] && n==options[1])throw CallbackError();
      if(options[3]) {
        model.mem.poke(C+0x24,4,D);model.mem.poke(D+12,4,0x77770000+n);
        model.mem.poke(C+0x38,2,0xc123+n);model.mem.store(0x50000,4,n);
      }
      std::string s=name;
      auto scenario=options.size()>4?options[4]:0;
      if(s=="GameObject_ctor") {
        model.mem.store(C+0x24,4,U(std::get<std::int64_t>(args[2])));
        auto pos=std::get<A::Vec3>(args[1]);for(unsigned k=0;k<3;++k)model.mem.store(C+0x10+4*k,4,pos[k]);
      }
      if(s=="obj_getPropInt")return std::int64_t(scenario&1?0xffff8001+n:0);
      if(s=="obj_getPropStr") {
        auto prop=std::get<std::int64_t>(args[1]);
        if(prop==0x52fbf8 || prop==0x52fc08 || prop==0x52fa00)return std::int64_t(scenario&4?0x60000+n:0);
        return std::int64_t(U(prop)==std::array<U,3>{0x52fa80,0x52fa88,0x52fa90}[scenario%3] && scenario&8?0x61000:0);
      }
      if(s=="balance_lookup")return A::Balance{std::int64_t(scenario&8),{0x3f000000,0x3f800001,0xbf800000,0x7fc12345,0x40000000}};
      if(s=="Creature_setStats")model.mem.store(C+0x38,2,0x4321);
      if(s=="creature_canSeeObj") {
        if(scenario&16)model.mem.store(C+0x234,4,0);
        if(scenario&32) {
          model.mem.store(C+0x3ae,2,7);model.mem.store(C+0x38,2,0x8765);
          return n==1?std::int64_t(0):signed64(options[0]);
        }
        return signed64(options[0]);
      }
      if(s=="creature_countAlliesNear" || s=="creature_isBlockedBy")return signed64(options[0]);
      if(s=="Creature.v16")return std::int64_t(0x99990000+n);
      if(s=="model_getSamplePointCount")return signed64(options[0]);
      if(s=="model_getSamplePoint")return A::Sample{{0x3f800000,0x80000000,0x7fc12345},{0,1,2}};
      return std::int64_t(0);
    };
  }
  m.external_slot16=options[2]!=0;
  m.external_slot26=options.size()>4 && (options[4]&64);
}
void require(bool ok,const char *what) {if(!ok)throw std::runtime_error(what);}
template<class E,class F> void throws(F f) {bool caught=false;try{f();}catch(const E &){caught=true;}require(caught,"expected refusal");}
std::string script_snapshot(const A::ArenaScript &m) {
  return join(m.q,[](const auto &p){return p.first+":"+std::to_string(p.second);},',')+"|"+
    join(m.g,[](const auto &p){return std::to_string(p.first)+":"+std::to_string(p.second);},',')+"|"+
    join(m.front,[](const auto &v){return v;},',')+"|"+join(m.back,[](const auto &v){return v;},',')+"|"+
    std::to_string(m.paused)+"|"+join(m.log,[](const A::ScriptCall &c){
      return c.name+"("+join(c.args,[](const A::ScriptValue &v){
        if(auto p=std::get_if<std::int64_t>(&v))return "i"+std::to_string(*p);
        return "s"+std::get<std::string>(v);
      })+")";
    });
}
std::string script_case(const std::string &init,const std::string &commands) {
  const std::array<std::string,10> keys={"sawknights","sawcaz","killedAv","endsound","firstmissionever",
    "knight1","knight2","knight3","knight4","extra"};
  const std::array<std::string,12> names={"start","incoming","nosave","setFlag","knightDefeatedPause",
    "knightDefeatedDelay","knightDefeated","avatar_death","bossDefeatedDelay","endmission","levelChange","__init__"};
  std::map<std::string,std::int64_t> quest;
  for(auto &r:rows(init)) {require(r.size()==2 && r[0]<=11,"script init fields");if(r[0]<10)quest[keys[r[0]]]=signed64(r[1]);}
  A::ArenaScript m(quest);
  for(auto &r:rows(init))if(r[0]>=10)m.g[unsigned((r[0]-10)*4)]=signed64(r[1]);
  std::vector<std::string> observations;
  for(auto &r:rows(commands)) {
    require(r.size()>=2,"script command fields");auto arg=signed64(r[1]);std::string result="ok";
    switch(r[0]) {
    case 100:require(r[1]<names.size(),"deferred entry index");m.defer(names[r[1]]);break;
    case 101:require(r.size()==3,"frame count");for(U k=0;k<r[2];++k)m.frame(arg);break;
    case 102:m.start();break;
    case 103:m.incoming();break;
    case 104:m.nosave();break;
    case 105:m.setFlag();break;
    case 106:m.knight_death(arg);break;
    case 107:m.knightDefeatedPause();break;
    case 108:m.knightDefeatedDelay();break;
    case 109:m.knightDefeated();break;
    case 110:m.avatar_death();break;
    case 111:m.bossDefeatedDelay();break;
    case 112:m.endmission();break;
    case 113:m.levelChange();break;
    case 114:result="r"+std::to_string(A::scaled_time(arg));break;
    default:throw std::runtime_error("unknown script operation");
    }
    observations.push_back(result+"~"+script_snapshot(m));
  }
  return join(observations,[](const auto &v){return v;},'#');
}
constexpr U S=0x60000,O=0x61000,K=0x62000,V=0x63000,W=0x64000,N=0x65000,M=0x66000;
void install_tagar(A::Model &m,const std::vector<U> &options) {
  auto counter=std::make_shared<unsigned>(0);
  for (auto name:{"Skill.v14","Skill.v12","Skill.v8","Creature.v20","Spell.v1","res_find",
                 "sound_playAtPos","pool_new","creature_findSpell","world_collectInSphere",
                 "world_collectObjectsInArc","handle_clear","handle_link","damage_apply","particle_spawn","creature_addSpell"}) {
    m.callbacks[name]=[name,options,counter](A::Model &model,const std::vector<A::Value> &args)->A::Value {
      auto count=++*counter,scenario=unsigned(options[0]);
      auto arg=[&](unsigned k){return U(std::get<std::int64_t>(args.at(k)));};
      if(options[1] && count==options[1])throw CallbackError();
      if(scenario&512)model.mem.store(S+0x78,4,0x3f000000);
      if(scenario&1024)model.mem.store(S+0xc,4,O+0x100);
      if(scenario&2048)model.mem.store(S+0x82,2,0);
      if(scenario&4096){model.mem.store(S+0x84,2,0x8765);model.mem.store(0x44fdcc,4,count);}
      if(scenario&8192){model.mem.store(V+0x1c,4,0x41a00000);model.mem.store(V+0x20,4,0x42a00000);}
      std::string s=name;
      if(s=="Skill.v14")return std::int64_t(arg(1)+1);
      if(s=="Skill.v12")return arg(1)==0 ? std::int64_t(scenario&128?0:scenario&256?-1:8) : std::int64_t(arg(1)==1?100:3);
      if(s=="Skill.v8")return A::DamageRange{0xfffffffd,0x7fffffff};
      if(s=="res_find")return std::int64_t(0x77700);
      if(s=="pool_new")return std::int64_t(scenario&1?0:arg(0)==0x94?S:M);
      if(s=="Creature.v20")return std::int64_t(scenario&4?0:-3);
      if(s=="creature_findSpell")return std::int64_t(scenario&8?M:0);
      if(s.rfind("world_collect",0)==0)return A::Objects{scenario&16?std::vector<std::uint32_t>{}:scenario&32?std::vector<std::uint32_t>{W,V}:std::vector<std::uint32_t>{0,V,W}};
      if(s=="handle_link"){model.mem.store(arg(0),4,N);model.mem.store(N+12,4,arg(1));}
      return std::int64_t(0);
    };
  }
}
std::string tagar_case(const std::string &init,const std::string &commands,const std::string &options) {
  A::Memory mem;for(auto &p:rows(init)){require(p.size()==3,"tagar poke fields");mem.poke(p[0],std::size_t(p[1]),p[2]);}
  A::Model m(mem);auto opts=rows(options);require(opts.size()==1&&opts[0].size()==2,"tagar options");install_tagar(m,opts[0]);
  std::vector<std::string> results;
  for(auto &cmd:rows(commands)) {
    require(cmd.size()>=2,"tagar command fields");auto arg=std::uint32_t(cmd[1]);
    try {
      switch(cmd[0]) {
      case 200:results.push_back("r"+std::to_string(m.spell_rand(S)));break;
      case 201:results.push_back(val(m.spell_ball(S)));break;
      case 202:m.tagar_spell_ctor(S,arg,O,K);results.push_back("ok");break;
      case 1203:m.reference_defaults=true;results.push_back("r"+std::to_string(std::get<std::int64_t>(m.skill_call(K,2,{std::int64_t(arg),std::int64_t(O),A::Vec3{0x3f800000,0x80000000,0x7fc12345},std::int64_t(0xffff)}))));break;
      case 1207:m.reference_defaults=true;m.table_call("Spell",S,0x14,2);results.push_back("ok");break;
      case 203:results.push_back("r"+std::to_string(m.tagar_cast(K,arg,O,{0x3f800000,0x80000000,0x7fc12345},0xffff)));break;
      case 204:require(cmd.size()==3,"validity fields");results.push_back("r"+std::to_string(m.tagar_is_valid(S,arg,std::uint32_t(cmd[2]))));break;
      case 205:results.push_back("r"+std::to_string(m.tagar_pick_target(S,arg)));break;
      case 206:results.push_back("r"+std::to_string(m.tagar_tick_victims(S)));break;
      case 207:m.tagar_spell_run(S);results.push_back("ok");break;
      case 209:results.push_back("r"+std::to_string(A::Model::unit(arg)));break;
      case 210:require(cmd.size()==5,"spell base fields");m.spell_base_ctor(S,arg,std::uint32_t(cmd[2]),std::uint32_t(cmd[3]),std::uint32_t(cmd[4]));results.push_back("ok");break;
      default:throw std::runtime_error("unknown tagar operation");
      }
    } catch(const CallbackError &){results.push_back("callback_error");break;}
  }
  return snapshot(m,results);
}
constexpr U B=0x70000;
void install_summon(A::Model &m,const std::vector<U> &options) {
  auto counter=std::make_shared<unsigned>(0);
  for(auto name:{"world_findObjectByName","GameObject_ctor","obj_getPropInt","obj_getPropStr",
      "model_getSamplePointCount","model_getSamplePoint","particle_spawn","terrain_height","world_findFreeSpot",
      "Skill.v12","pool_new","Holder.v1","Holder.v2","Holder.v4","Holder.v6","Creature_setStats",
      "model_buildHideMask","anim_clear","anim_push","anim_start","anim_syncToSimFrame","handle_clear",
      "obj_runEveryFrame","Creature_init","Creature_setCharm","res_find","sound_playAtPos","handle_link","Spell.v1"}) {
    m.callbacks[name]=[name,options,counter](A::Model &model,const std::vector<A::Value> &args)->A::Value {
      auto count=++*counter,scenario=unsigned(options[0]);
      auto arg=[&](unsigned k){return U(std::get<std::int64_t>(args.at(k)));};
      if(options[4] && count==options[4])throw CallbackError();
      if(scenario&512)model.mem.store(O+0x38,2,count+0x8123);
      if(scenario&1024)model.mem.store(0x44fdcc,4,count);
      if(scenario&2048)model.mem.store(B+0x50,4,0);
      if(scenario&4096)model.mem.store(O+12,4,0);
      if(scenario&8192)model.mem.store(0x44fe18,1,'!');
      std::string s=name;
      if(s=="world_findObjectByName")return std::int64_t(scenario>>16);
      if(s=="GameObject_ctor") {
        model.mem.store(arg(0)+0x24,4,arg(2));auto pos=std::get<A::Vec3>(args.at(1));
        for(unsigned k=0;k<3;++k)model.mem.store(arg(0)+0x10+4*k,4,pos[k]);
      }
      if(s=="model_getSamplePointCount")return options[1]?std::int64_t(champions::simulation::rta::s32(std::uint32_t(options[1]))):std::int64_t(scenario&32?7:0);
      if(s=="model_getSamplePoint")return A::Vec3{0x3f800000,0x80000000,0x7fc12345};
      if(s=="terrain_height")return std::int64_t(scenario&16384?options[2]:scenario&8?0x42000000:0);
      if(s=="world_findFreeSpot")return A::FreeSpot{{0x3f800000,0x40000000,0x40400000},std::uint32_t(scenario&32768?options[3]:scenario&16?0:1)};
      if(s=="Skill.v12")return std::int64_t(arg(1)==0?(scenario&64?-1:3):(scenario&65536?champions::simulation::rta::s32(std::uint32_t(options[5])):scenario&128?0:scenario&256?-1:100));
      if(s=="pool_new")return std::int64_t(arg(0)==0x468?(scenario&1?0:B):(scenario&2?0:S));
      if(s=="model_buildHideMask")return signed64(UINT64_C(0xfedcba9876543210));
      if(s=="res_find")return std::int64_t(0x77700);
      if(s=="handle_link"){model.mem.store(arg(0),4,N);model.mem.store(N+12,4,arg(1));}
      return std::int64_t(0);
    };
  }
}
std::string summon_case(const std::string &init,const std::string &commands,const std::string &options) {
  A::Memory mem;for(auto &p:rows(init)){require(p.size()==3,"summon poke fields");mem.poke(p[0],std::size_t(p[1]),p[2]);}
  A::Model m(mem);auto opts=rows(options);require(opts.size()==1 && opts[0].size()==6,"summon options");install_summon(m,opts[0]);
  std::vector<std::string> results;
  for(auto &cmd:rows(commands)) {
    require(cmd.size()>=2,"summon command fields");auto arg=std::uint32_t(cmd[1]);
    try {
      switch(cmd[0]) {
      case 1300:m.reference_defaults=true;results.push_back("r"+std::to_string(std::get<std::int64_t>(m.skill_call(K,1,{std::int64_t(O)}))));break;
      case 1305:m.reference_defaults=true;m.table_call("Spell",S,0x14,2);results.push_back("ok");break;
      case 1306:m.reference_defaults=true;results.push_back("r"+std::to_string(std::get<std::int64_t>(m.skill_call(K,2,{std::int64_t(arg),std::int64_t(O),A::Vec3{0x3f800000,0x80000000,0x7fc12345},std::int64_t(cmd.size()>2?cmd[2]:0xffff)}))));break;
      case 300:results.push_back("r"+std::to_string(m.summon_can_use(K,O)));break;
      case 301:results.push_back("r"+std::to_string(m.cosmetic_draw()));break;
      case 302:results.push_back(val(m.cosmetic_ball()));break;
      case 303:results.push_back(val(m.summon_sample(O,champions::simulation::rta::s32(arg))));break;
      case 304:require(cmd.size()==3,"summon ctor fields");m.summon_spell_ctor(S,arg,O,K,std::uint32_t(cmd[2]));results.push_back("ok");break;
      case 305:m.summon_spell_run(S);results.push_back("ok");break;
      case 306:results.push_back("r"+std::to_string(m.summon_cast(K,arg,O,{0x3f800000,0x80000000,0x7fc12345},cmd.size()>2?std::uint32_t(cmd[2]):0xffff)));break;
      case 307:m.skelight_ctor(B,{0x3f800000,0x80000000,0x7fc12345},0xffff,0x78000,arg);results.push_back("ok");break;
      default:throw std::runtime_error("unknown summon operation");
      }
    } catch(const CallbackError &){results.push_back("callback_error");break;}
  }
  return snapshot(m,results);
}

std::string helper_case(const std::string &init,const std::string &commands,const std::string &options) {
  A::Memory mem;for(auto &p:rows(init))mem.poke(p[0],std::size_t(p[1]),p[2]);
  A::Model m(mem);auto opts=rows(options).at(0);require(opts.size()==3,"helper options");
  unsigned calls=0;
  auto callback=[&](A::Model &,const std::vector<A::Value> &)->A::Value {
    ++calls;if(opts[2] && calls==opts[2])throw CallbackError();
    if(opts[1]==1)mem.store(C+0x50,4,0);
    if(opts[1]==2){mem.store(0x40000,4,0x51000);mem.store(0x51000+136,2,0xfffe);}
    return signed64(opts[0]);
  };
  for(auto slot:{0,1,11,15,17,21,22,255})m.callbacks["Holder.v"+std::to_string(slot)]=callback;
  m.callbacks["obj_getPropInt"]=callback;
  std::vector<std::string> results;
  for(auto &cmd:rows(commands)) {
    auto arg=cmd.at(1);std::string result="ok";
    try {
      switch(cmd[0]) {
      case 400: {
        auto bytes=m.cstr(arg,std::size_t(cmd.at(2)));result="b";
        constexpr char hex[]="0123456789abcdef";
        for(unsigned char b:bytes){result+=hex[b>>4];result+=hex[b&15];}break;
      }
      case 401:result="r"+std::to_string(m.class_model(arg));break;
      case 402:result="r"+std::to_string(m.clip(arg,std::uint32_t(cmd.at(2))));break;
      case 403:result="r"+std::to_string(std::get<std::int64_t>(m.holder_call(arg,unsigned(cmd.at(2)),
        {std::int64_t(0xffffffff),std::int64_t(0x100000001),A::Vec3{0,0x80000000,0x7fc12345}})));break;
      case 404:result="r"+std::to_string(std::get<std::int64_t>(m.weapon_descriptor(arg)));break;
      case 405:result="r"+std::to_string(std::get<std::int64_t>(m.prop_int(arg,std::uint32_t(cmd.at(2)))));break;
      case 406: {
        auto table=std::uint32_t(cmd.at(3));m.init_holder(arg,cmd.at(2),cmd.at(4)?&table:nullptr);break;
      }
      case 407:result="r"+std::to_string(A::half_toward_zero(std::uint32_t(arg)));break;
      case 408:result="r"+std::to_string(A::third_toward_zero(std::uint32_t(arg)));break;
      default:throw std::runtime_error("unknown helper op");
      }
      results.push_back(result);
    } catch(const CallbackError &) {results.push_back("callback_error");break;}
  }
  return snapshot(m,results);
}
void helper_host_tests() {
  using Limit=champions::simulation::rta::creature_hp::ResourceLimit;
  A::Memory mem;A::Model m(mem);
  require(std::get<std::int64_t>(m.weapon_descriptor(C))==0 && m.events.empty(),"absent holder has no callback");
  throws<A::MissingHandler>([&]{m.prop_int(C,0);});
  mem.poke(C+0x50,4,0x40000);
  throws<A::MissingHandler>([&]{m.weapon_descriptor(C);});
  A::Memory raw;A::Model rm(raw);
  throws<Limit>([&]{rm.cstr(UINT64_MAX);});
  throws<Limit>([&]{rm.class_model(UINT64_MAX);});
  throws<Limit>([&]{rm.clip(UINT64_MAX,0);});
  throws<Limit>([&]{rm.holder_call(UINT64_MAX,0);});
  throws<Limit>([&]{rm.weapon_descriptor(UINT64_MAX);});
  throws<Limit>([&]{rm.prop_int(UINT64_MAX,0);});
  throws<Limit>([&]{rm.init_holder(UINT64_MAX,0);});
  throws<Limit>([&]{rm.init_holder(0,UINT64_MAX);});
  throws<Limit>([&]{rm.holder_call(0,UINT32_MAX);});
  throws<Limit>([&]{rm.clip(0,UINT32_MAX);});
  raw.poke(UINT32_MAX,1,1);
  throws<Limit>([&]{rm.cstr(UINT32_MAX,2);});
  require(rm.events.empty() && raw.log().empty(),"helper address refusal before effects");
  A::Memory bounded;A::Model bm(bounded,{1048576,3,64,4});
  throws<Limit>([&]{bm.init_holder(C,0x40000);});
  require(bm.writes.size()==3 && bounded.log().size()==3,"holder history refusal prefix");
  throws<Limit>([&]{bm.cstr(0,1048577);});
  A::Memory recmem;A::Model rec(recmem,{1048576,65536,64,4});
  rec.callbacks["Holder.v17"]=[](auto &model,const auto &)->A::Value{return model.holder_call(0,17);};
  throws<Limit>([&]{rec.holder_call(0,17);});
  require(rec.calls.size()==4,"holder recursive calls bounded");
  rec.callbacks["Holder.v17"]=[](auto &,const auto &)->A::Value{return A::Vec3{1,2,3};};
  recmem.poke(C+0x50,4,0x40000);
  require(std::get<A::Vec3>(rec.weapon_descriptor(C))==A::Vec3{1,2,3},"descriptor forwards callback value unchanged");
  rec.callbacks["obj_getPropInt"]=[](auto &,const auto &)->A::Value{return std::string("opaque");};
  require(std::get<std::string>(rec.prop_int(C,0))=="opaque","property helper forwards result unchanged");
}
void summon_host_tests() {
  using Limit=champions::simulation::rta::creature_hp::ResourceLimit;
  A::Memory mem;A::Model m(mem);
  throws<A::MissingHandler>([&]{m.summon_cast(K,1,O,{0,0,0},0);});
  require(m.writes.size()==4 && m.calls.size()==1,"missing summon query preserves flash prefix");
  A::Memory raw;A::Model rm(raw);
  throws<Limit>([&]{rm.summon_can_use(K,UINT64_MAX);});
  throws<Limit>([&]{rm.summon_cast(K,1,UINT64_MAX,{},0);});
  throws<Limit>([&]{rm.summon_cast(UINT64_MAX,1,O,{},0);});
  throws<Limit>([&]{rm.summon_sample(UINT64_MAX,1);});
  throws<Limit>([&]{rm.skelight_ctor(UINT64_MAX,{},0,0,0);});
  throws<Limit>([&]{rm.summon_spell_ctor(UINT64_MAX,0,0,0,0);});
  throws<Limit>([&]{rm.summon_spell_run(UINT64_MAX);});
  require(rm.events.empty() && raw.bytes().empty(),"summon raw32 refusal before effects");
  A::Memory malformed;A::Model mm(malformed);install_summon(mm,{8,0,0,0,0});
  malformed.poke(O+0x48,4,0x71000);malformed.poke(0x71000+132,4,0x3c2738);
  mm.callbacks["world_findFreeSpot"]=[](auto &,const auto &)->A::Value{return std::int64_t(0);};
  throws<A::Error>([&]{mm.summon_cast(K,1,O,{},0);});
  require(mm.draws==0 && mm.calls.back().name=="world_findFreeSpot","free spot malformed prefix before draw");
  mm.callbacks["model_getSamplePoint"]=[](auto &,const auto &)->A::Value{return A::Sample{};};
  throws<A::Error>([&]{mm.summon_sample(O,1);});
  A::Memory limited;A::Model lm(limited,{1048576,2,64,64});
  throws<Limit>([&]{lm.cosmetic_ball();});require(lm.writes.size()==2 && lm.draws==0,"cosmetic history prefix");
  A::Memory recmem;A::Model rec(recmem,{1048576,65536,64,4});
  recmem.poke(S+20,4,0x71000);recmem.poke(0x71000+12,4,0x2e177c);
  throws<Limit>([&]{rec.summon_spell_run(S);});require(rec.events.empty(),"known summon run recursion depth");
  rec.external.insert("Spell.v1");rec.callbacks["Spell.v1"]=[](auto &,const auto &)->A::Value{return std::int64_t(0);};
  rec.summon_spell_run(S);require(rec.calls.size()==1,"reuse with explicit external spell dispatch");
}

void tagar_host_tests() {
  using Limit=champions::simulation::rta::creature_hp::ResourceLimit;
  A::Memory mem;A::Model m(mem);
  throws<A::MissingHandler>([&]{m.tagar_cast(K,1,O,{0,0,0});});
  require(m.calls.size()==1 && m.calls[0].name=="Skill.v14" && mem.bytes().empty(),"cast missing skill prefix");
  install_tagar(m,{0,0});mem.poke(S+0xc,4,O);mem.poke(O+0x48,4,0x67000);mem.poke(0x67000+20*8+4,4,0x119328);
  throws<A::Unsupported>([&]{m.tagar_is_valid(S,V,0);});
  m.external.insert("Creature.v20");require(m.tagar_is_valid(S,V,0)==1,"explicit enemy delegate");
  m.callbacks["world_collectInSphere"]=[](auto &,const auto &)->A::Value{return std::int64_t(0);};
  throws<A::Error>([&]{m.tagar_pick_target(S,1);});
  require(m.calls.back().name=="world_collectInSphere","malformed list retains query prefix");
  A::Memory player;A::Model pm(player);install_tagar(pm,{0,0});
  player.poke(S+0xc,4,O);player.poke(S+8,4,K);player.poke(O+12,4,0x40);
  player.poke(S+0x82,2,1);player.poke(S+0x18,4,N);player.poke(N+12,4,V);player.poke(S+0x38,4,0x41200000);
  pm.callbacks["Skill.v8"]=[](auto &,const auto &)->A::Value{return A::Vec3{};};
  throws<A::Error>([&]{pm.tagar_tick_victims(S);});
  require(player.load(V+0x98,2)==100 && player.load(S+0x58,4)==0,"damage range failure prefix");
  A::Memory limited;A::Model lm(limited,{1048576,2,64,64});
  throws<Limit>([&]{lm.spell_ball(S);});require(lm.writes.size()==2,"ball rejection history prefix");
  A::Memory addresses;A::Model am(addresses);
  throws<Limit>([&]{am.spell_rand(UINT64_MAX);});throws<Limit>([&]{am.spell_ball(UINT64_MAX);});
  throws<Limit>([&]{am.spell_base_ctor(UINT64_MAX,0,0,0,0);});
  throws<Limit>([&]{am.tagar_cast(UINT64_MAX,0,0,{0,0,0});});
  throws<Limit>([&]{am.tagar_spell_ctor(UINT64_MAX,0,0,0);});
  throws<Limit>([&]{am.tagar_is_valid(UINT64_MAX,0,0);});
  throws<Limit>([&]{am.tagar_pick_target(UINT64_MAX,0);});
  throws<Limit>([&]{am.tagar_tick_victims(UINT64_MAX);});
  throws<Limit>([&]{am.tagar_spell_run(UINT64_MAX);});
  require(am.events.empty() && addresses.bytes().empty(),"tagar raw32 address refusal before effects");
  // Known skill redirection is not silently turned into a statistics callback.
  A::Memory red;A::Model rm(red);install_tagar(rm,{0,0});red.poke(K+0x60,4,0x68000);red.poke(0x68000+14*8+4,4,0x2e18a4);
  throws<A::Unsupported>([&]{rm.tagar_cast(K,0,O,{0,0,0});});require(rm.events.empty(),"skill redirect refusal");
  rm.external.insert("Skill.v14");rm.tagar_cast(K,0,O,{0,0,0});
  // Spell dispatch can re-enter the known Tagar run, bounded by operation depth.
  A::Memory recursive;A::Model rec(recursive,{1048576,65536,64,4});
  recursive.poke(S+20,4,0x70000);recursive.poke(0x70000+12,4,0x30cbc8);
  throws<Limit>([&]{rec.tagar_spell_run(S);});require(rec.writes.size()==3,"internal spell recursion bound");
  recursive.poke(0x70000+12,4,0);install_tagar(rec,{0,0});rec.tagar_spell_run(S);
}

void install_zek(A::Model &m,const std::vector<U> &opts) {
  auto count=std::make_shared<unsigned>(0),cone=std::make_shared<unsigned>(0);
  for(auto name:{"GameObject_ctor","obj_getPropInt","obj_getPropStr","obj_moveOffGrid","balance_lookup",
    "Creature_setStats","Holder.v1","Holder.v11","handle_clear","anim_start",
    "anim_startBlendInOut","anim_syncToSimFrame","anim_clear","obj_runEvery60","Creature_init",
    "terrain_heightBelow","Creature_dropLoot","Creature.v28","creature_findEnemyInCone",
    "terrain_height","fx_ringBurst","res_find","sound_playOnObject","combat_applyAreaDamage",
    "decal_add","pool_new","LightEffect_ctor","fx_spawn","state_request","script_runByName",
    "loot_spawnNamedItem","anim_jumpTopToEnd","obj_delete","arena_addKill","creature_stopBossMusic"}) {
    m.callbacks[name]=[opts,count,cone,name](auto &model,const auto &args)->A::Value {
      auto n=++*count;auto scenario=opts[0];std::string s=name;
      if(opts[1] && n==opts[1])throw CallbackError();
      if(scenario&1024) {
        model.mem.store(C+0x300,4,0x80000000+n);model.mem.store(C+0x38,2,0x1234+n);
        model.mem.store(C+0x46c,4,0x44200000);model.mem.store(C+0x24,4,0x503688);
        model.mem.store(0x503688+0x7c,4,0x90000+n);
      }
      if(scenario&2048) {
        model.mem.store(C+0x300,4,0xffffff00+n);model.mem.store(C+0x24,4,D);
        model.mem.store(D+0x44,4,0x91000+n);
      }
      if((scenario&4096) && s=="terrain_heightBelow") {
        model.mem.store(C+0x3b4,4,0x41200000+n);model.mem.store(C+0x3b8,4,0xc1200000+n);
        model.mem.store(C+0x3bc,4,0x3f800000);model.mem.store(C+0x18,4,0x40000000);
        model.mem.store(C+0x44,4,0x80000000+n);model.mem.store(C+0xa8,8,0x8000000);
      }
      if(s=="GameObject_ctor") {
        model.mem.store(C+0x24,4,U(std::get<std::int64_t>(args[2])));
        auto pos=std::get<A::Vec3>(args[1]);for(unsigned k=0;k<3;++k)model.mem.store(C+0x10+4*k,4,pos[k]);
      }
      if(s=="obj_getPropInt")return std::int64_t(scenario&1?1:0);
      if(s=="balance_lookup")return A::Balance{std::int64_t(scenario&2),{0x3f000000,0x3f800001,0xbf800000,0x7fc12345,0x40000000}};
      if(s=="terrain_heightBelow")return std::int64_t(scenario&8192?opts[2]:scenario&64?0x7fc12345:0x40400000);
      if(s=="terrain_height")return std::int64_t(opts[2]);
      if(s=="res_find")return std::int64_t(0x70001+n);
      if(s=="pool_new")return std::int64_t(scenario&1?0:0x71000);
      if(s=="creature_findEnemyInCone")return std::int64_t((scenario>>((*cone)++))&1);
      return std::int64_t(0);
    };
  }
}
void install_zek_damage(A::Model &m,const std::vector<U> &opts) {
  auto count=std::make_shared<unsigned>(0);
  constexpr U attacker=0x41000,node=0x40000,who=0x42000;
  for(auto name:{"res_find","sound_playOnObject","particleDef_ctor","particle_spawn","handle_clear","handle_link"}) {
    m.callbacks[name]=[opts,count,name](auto &model,const auto &args)->A::Value {
      auto n=++*count;auto scenario=opts[0];std::string s=name;
      if(opts[1] && n==opts[1])throw CallbackError();
      if(scenario&2048)model.mem.store(C+0x231,1,1);
      if(scenario&4096)model.mem.store(C+0xa8,8,UINT64_C(0x1000000000000));
      if(scenario&8192) {
        model.mem.store(C+0x10,4,0x42a00000+n);model.mem.store(attacker+0x14,4,0x41a00000);
        model.mem.store(C+0x1c,4,0x3f800000);model.mem.store(C+0x20,4,0x40000000);
      }
      if(scenario&16384)model.mem.store(C+0x44,4,0x80000000+n);
      if(scenario&32768) {
        model.mem.store(C+0x228,4,0x3f800000+n);model.mem.store(C+0xc8,4,0xbf800000);
        model.mem.store(0x44fdcc,4,n);model.mem.store(C+0x2dc,4,n);
      }
      if((scenario&65536) && s=="handle_clear") {
        model.mem.store(attacker+0xc,4,0xa0);model.mem.store(attacker+0x4c,4,node);model.mem.store(node+12,4,0);
      }
      if((scenario&131072) && s=="handle_clear") {
        model.mem.store(C+0x360,4,node);model.mem.store(node+12,4,who);
      }
      if(s=="res_find")return std::int64_t(0x70001+n);
      if(s=="handle_clear")model.mem.store(U(std::get<std::int64_t>(args[0])),4,0);
      if(s=="handle_link")model.mem.store(U(std::get<std::int64_t>(args[0])),4,node);
      return std::int64_t(0);
    };
  }
}
std::string zek_case(const std::string &init,const std::string &commands,const std::string &options) {
  A::Memory mem;for(auto &p:rows(init)){require(p.size()==3,"Zek poke fields");mem.poke(p[0],std::size_t(p[1]),p[2]);}
  A::Model m(mem);auto opts=rows(options);require(opts.size()==1&&opts[0].size()==3,"Zek options");install_zek(m,opts[0]);
  if(rows(commands).at(0).at(0)==505)install_zek_damage(m,opts[0]);
  std::vector<std::string> results;
  for(auto &cmd:rows(commands)) {
    require(cmd.size()==(cmd[0]==505?4u:2u),"Zek command fields");
    try {
      switch(cmd[0]) {
      case 500:m.zek_ctor(C,0x60000,{0x3f800000,0x40000000,0x40400000},0xffff8123,0x70000);results.push_back("ok");break;
      case 501:m.zek_set_state(C,std::uint32_t(cmd[1]));results.push_back("ok");break;
      case 502:results.push_back("r"+std::to_string(m.zek_specials(C,[&](auto c,auto state){
        // The supplied state function is an observation, as in the Python input.
        A::Call event{"state_request",{std::int64_t(c),std::int64_t(state)}};
        m.calls.push_back(event);m.events.emplace_back(event);m.callbacks.at("state_request")(m,event.args);
      })));break;
      case 503:m.zek_wave(C);results.push_back("ok");break;
      case 504:results.push_back("r"+std::to_string(m.zek_slot35(C)));break;
      case 505:m.zek_take_damage(C,std::uint32_t(cmd[1]),std::uint32_t(cmd[2]),std::uint32_t(cmd[3]));results.push_back("ok");break;
      case 506:m.skelight_set_state(C,std::uint32_t(cmd[1]));results.push_back("ok");break;
      default:throw std::runtime_error("unknown Zek operation");
      }
    }catch(const CallbackError &){results.push_back("callback_error");break;}
  }
  return snapshot(m,results);
}
void install_horsemen(A::Model &m,const std::vector<U> &opts) {
  auto count=std::make_shared<unsigned>(0);
  for(auto name:{"GameObject_ctor","obj_getPropInt","obj_getPropStr","obj_moveOffGrid","balance_lookup",
    "Creature_setStats","Holder.v1","Holder.v11","handle_clear","handle_link","anim_start",
    "anim_startBlendInOut","anim_syncToSimFrame","anim_clear","anim_push","obj_runEvery60",
    "Creature_init","terrain_heightBelow","Creature_dropLoot","Creature.v28","Creature.v27",
    "Creature.v35","model_buildHideMask","lump_load","pool_new","Nightmare_ctor",
    "world_findObjectByName","Creature.v16","Skill.v2","creature_addSpell","model_getSamplePointCount",
    "terrain_height","res_find","sound_playAtPos","Skill.v12","Skill.v14",
    "obj_delete","creature_stopBossMusic"}) {
    m.callbacks[name]=[opts,count,name](auto &model,const auto &args)->A::Value {
      auto n=++*count;auto scenario=opts[0];std::string s=name;
      if(opts[1] && n==opts[1])throw CallbackError();
      if(scenario&1024) {
        model.mem.store(C+0x300,4,0x80000000+n);model.mem.store(C+0x38,2,0x1234+n);
        model.mem.store(C+0x24,4,0x5006e8);model.mem.store(0x5006e8+0x54,4,0x90000+n);
        if(s=="Holder.v11")model.mem.store(C+0x50,4,0);
        if(s=="anim_start")model.mem.store(C+0x3c8,4,0x72000);
      }
      if(s=="GameObject_ctor") {
        model.mem.store(C+0x24,4,U(std::get<std::int64_t>(args[2])));
        auto pos=std::get<A::Vec3>(args[1]);for(unsigned k=0;k<3;++k)model.mem.store(C+0x10+4*k,4,pos[k]);
      }
      if(s=="obj_getPropInt")return std::int64_t(scenario&2?1:0);
      if(s=="balance_lookup")return A::Balance{std::int64_t(scenario&2),{0x3f000000,0x3f800001,0xbf800000,0x7fc12345,0x40000000}};
      if(s=="terrain_heightBelow")return std::int64_t(0x40400000);
      if(s=="terrain_height")return std::int64_t(opts[2]);
      if(s=="model_buildHideMask")return std::int64_t(0x123456789abcdef0);
      if(s=="pool_new")return std::int64_t(scenario&1?0:0x72000);
      if(s=="world_findObjectByName")return std::int64_t(0x41000);
      if(s=="handle_link")model.mem.store(U(std::get<std::int64_t>(args[0])),4,U(std::get<std::int64_t>(args[1])));
      if(s=="Skill.v2")return std::int64_t(0x73000);
      if(s=="Skill.v14")return std::int64_t(1);
      return std::int64_t(0);
    };
  }
}
std::string horsemen_case(const std::string &init,const std::string &commands,const std::string &options) {
  A::Memory mem;for(auto &p:rows(init)){require(p.size()==3,"Horsemen poke fields");mem.poke(p[0],std::size_t(p[1]),p[2]);}
  A::Model m(mem);auto opts=rows(options);require(opts.size()==1&&opts[0].size()==3,"Horsemen options");install_horsemen(m,opts[0]);
  std::vector<std::string> results;auto cmds=rows(commands);
  for(std::size_t index=0;index<cmds.size();++index) {
    auto &cmd=cmds[index];require(cmd.size()==2,"Horsemen command fields");
    try {
      switch(cmd[0]) {
      case 600:
        require(index+1<cmds.size() && cmds[index+1][0]==603,"Horsemen mounted parameter");
        m.horsemen_ctor(C,0x60000,{0x80000000,0x40000000,0x40400000},0xffff8123,0x70000,std::uint32_t(cmd[1]),std::uint32_t(cmds[index+1][1]));results.push_back("ok");break;
      case 601:m.horsemen_set_state(C,std::uint32_t(cmd[1]));results.push_back("ok");break;
      case 602:m.horsemen_cast(C);results.push_back("ok");break;
      case 603:break;
      default:throw std::runtime_error("unknown Horsemen operation");
      }
    }catch(const CallbackError &){results.push_back("callback_error");break;}
  }
  return snapshot(m,results);
}
void horsemen_host_tests() {
  using Limit=champions::simulation::rta::creature_hp::ResourceLimit;
  A::Memory mem;A::Model m(mem);
  throws<Limit>([&]{m.horsemen_ctor(UINT64_MAX,0,{},0,0,0,0);});
  throws<Limit>([&]{m.horsemen_set_state(UINT64_MAX,0);});
  throws<Limit>([&]{m.horsemen_cast(UINT64_MAX);});require(m.events.empty(),"Horsemen upfront address refusal");
  mem.poke(C+0x470,1,0);throws<A::MissingHandler>([&]{m.horsemen_cast(C);});
  m.callbacks["Creature.v35"]=[](auto &,const auto &)->A::Value{return A::Vec3{};};
  throws<A::Error>([&]{m.horsemen_cast(C);});
  m.callbacks["Creature.v35"]=[](auto &,const auto &)->A::Value{return std::int64_t(1);};
  mem.poke(0x41b180,4,0x60000);mem.poke(0x60060,4,0x30000);mem.poke(0x30014,4,0x2e2368);
  throws<A::Unsupported>([&]{m.horsemen_cast(C);});
  m.external.insert("Skill.v2");m.callbacks["Skill.v2"]=[](auto &,const auto &)->A::Value{return std::int64_t(0);};m.horsemen_cast(C);
  A::Memory recursion;A::Model rm(recursion,{1048576,65536,64,4});install_horsemen(rm,{0,0,0});
  rm.callbacks["anim_start"]=[](auto &model,const auto &)->A::Value{model.horsemen_set_state(C,5);return std::int64_t(0);};
  throws<Limit>([&]{rm.horsemen_set_state(C,5);});install_horsemen(rm,{0,0,0});rm.horsemen_set_state(C,5);
}

void zek_host_tests() {
  using Limit=champions::simulation::rta::creature_hp::ResourceLimit;
  A::Memory mem;A::Model m(mem);
  throws<Limit>([&]{m.zek_ctor(UINT64_MAX,0,{},0,0);});
  throws<Limit>([&]{m.zek_set_state(UINT64_MAX,0);});
  throws<Limit>([&]{m.zek_slot35(UINT64_MAX);});
  throws<Limit>([&]{m.zek_specials(UINT64_MAX,{});});
  throws<Limit>([&]{m.zek_wave(UINT64_MAX);});
  throws<Limit>([&]{m.zek_take_damage(UINT64_MAX,0,0,0);});
  require(m.events.empty() && mem.bytes().empty(),"Zek invalid addresses before effects");
  mem.poke(C+0x464,4,1);
  throws<A::MissingHandler>([&]{m.zek_wave(C);});
  require(m.writes.size()==2 && m.calls.back().name=="terrain_height","wave missing terrain prefix");
  m.callbacks["terrain_height"]=[](auto &,const auto &)->A::Value{return A::Vec3{};};
  throws<A::Error>([&]{m.zek_wave(C);});
  mem.poke(C+0x300,4,181);
  m.callbacks["creature_findEnemyInCone"]=[](auto &,const auto &)->A::Value{return std::int64_t(1);};
  throws<A::MissingHandler>([&]{m.zek_specials(C,{});});
  A::Memory limited;A::Model lm(limited,{1048576,2,64,64});limited.poke(C+0x464,4,1);
  throws<Limit>([&]{lm.zek_wave(C);});require(lm.writes.size()==2 && lm.calls.empty(),"wave history prefix");
  A::Memory recursion;A::Model rm(recursion,{1048576,65536,64,4});install_zek(rm,{0,0,0});
  rm.callbacks["anim_start"]=[](auto &model,const auto &)->A::Value{model.zek_set_state(C,5);return std::int64_t(0);};
  throws<Limit>([&]{rm.zek_set_state(C,5);});install_zek(rm,{0,0,0});rm.zek_set_state(C,5);
  require(recursion.load(C+0x232,2)==5,"Zek reuse after recursive refusal");
}

void zek_damage_host_tests() {
  using Limit=champions::simulation::rta::creature_hp::ResourceLimit;
  A::Memory mem;A::Model m(mem);
  mem.poke(0x4100c,4,0x20);
  throws<A::MissingHandler>([&]{m.zek_take_damage(C,0x41000,0x2000,0);});
  require(m.writes.empty() && m.calls.back().name=="res_find","damage missing resource prefix");
  m.callbacks["res_find"]=[](auto &,const auto &)->A::Value{return A::Vec3{};};
  throws<A::Error>([&]{m.zek_take_damage(C,0x41000,0x2000,0);});
  require(m.writes.empty() && m.calls.size()==2,"damage malformed resource prefix");
  A::Memory base;A::Model bm(base);base.poke(C+0x228,4,0x3f800000);
  throws<A::MissingHandler>([&]{bm.zek_take_damage(C,0x41000,0,0);});
  require(bm.writes.size()==4 && bm.calls.back().name=="handle_clear","base damage missing handle prefix");
  A::Memory limited;A::Model lm(limited,{1048576,8,64,64});install_zek_damage(lm,{0,0,0});
  limited.poke(0x4100c,4,0x20);
  throws<Limit>([&]{lm.zek_take_damage(C,0x41000,0x2000,0);});
  require(limited.load(0x503748,4)==1 && !limited.load(C+0x3b0,2),"sparks history refusal before damage");
  A::Memory recursive;A::Model rm(recursive,{1048576,65536,64,4});install_zek_damage(rm,{0,0,0});
  recursive.poke(0x4100c,4,0x20);
  rm.callbacks["res_find"]=[](auto &model,const auto &)->A::Value {
    model.zek_take_damage(C,0x41000,0x2000,0);return std::int64_t(0);
  };
  throws<Limit>([&]{rm.zek_take_damage(C,0x41000,0x2000,0);});
  require(rm.calls.size()==4 && rm.writes.empty(),"damage recursive prefix");
  install_zek_damage(rm,{0,0,0});rm.zek_take_damage(C,0,0,0);
  require(recursive.load(C+0x3b0,2)==0x7fff,"damage reuse after recursive refusal");
  // The attacker field is raw32; nonwrapping field spans still refuse.
  A::Memory spans;A::Model sm(spans);
  throws<Limit>([&]{sm.zek_take_damage(C,UINT32_MAX,0x2000,0);});
  require(sm.events.empty(),"damage invalid attacker span before effects");
}
void host_tests() {
  zek_damage_host_tests();
  zek_host_tests();
  horsemen_host_tests();
  helper_host_tests();
  tagar_host_tests();
  summon_host_tests();
  A::ArenaScript script({},1);
  throws<champions::simulation::rta::creature_hp::ResourceLimit>([&]{script.start();});
  require(script.log.size()==1 && script.log[0].name=="hideMonster","script history prefix");
  A::ArenaScript unknown;
  unknown.defer("unknown");unknown.frame();
  throws<A::Unsupported>([&]{unknown.frame(1);});
  require(unknown.paused && unknown.front.size()==1 && unknown.back.empty(),"unknown defer preserves queue prefix");
  unknown.front.clear();unknown.start();require(unknown.log.size()==6,"reuse script after refusal");
  A::ArenaScript overflow;overflow.g[0]=INT64_MAX;
  throws<champions::simulation::rta::creature_hp::ResourceLimit>([&]{overflow.bossDefeatedDelay();});
  require(overflow.g[0]==INT64_MAX && overflow.log.empty(),"counter limit before effects");
  A::ArenaScript recursive;recursive.defer("frame");recursive.frame();
  throws<champions::simulation::rta::creature_hp::ResourceLimit>([&]{recursive.frame();});
  require(recursive.front.size()==1 && recursive.back.empty(),"recursive frame prefix");
  recursive.front.clear();recursive.frame();
  // Every no-argument entry can also run through the deferred dispatcher.
  for (auto name:{"start","incoming","nosave","setFlag","knightDefeatedPause","knightDefeatedDelay",
                 "knightDefeated","avatar_death","bossDefeatedDelay","endmission","levelChange"}) {
    A::ArenaScript queued;queued.defer(name);queued.frame();queued.frame();
  }

  // Missing handlers preserve exact filter prefix, never assume callback success.
  A::Memory mem;A::Model m(mem);
  throws<A::MissingHandler>([&]{m.skelight_set_state(C,0);});
  require(m.writes.size()==1 && m.calls.size()==1 && mem.load(C+0x232,2)==0,"missing handler prefix");
  A::Memory wmem;A::Model wander(wmem);
  throws<A::MissingHandler>([&]{wander.skelight_set_state(C,1);});
  require(wander.writes.size()==16 && wander.calls.size()==1 &&
          wander.calls.back().name=="anim_start", "wander missing animation preserves path/destination prefix");
  // An inert wander request is a modeled refusal, rather than unsupported.
  wmem.poke(C+0x231,1,1);wander.skelight_set_state(C,1);
  require(wander.writes.size()==16,"inert refusal has no added writes");
  // Terrain is a required service only on the random-point path. Missing or
  // malformed answers preserve the two own draws, never install a destination.
  A::Memory terrain;A::Model tm(terrain);
  terrain.poke(C+0x3c0,4,0x43960000);terrain.poke(C+0x232,2,6);
  throws<A::MissingHandler>([&]{tm.skelight_set_state(C,1);});
  require(tm.writes.size()==16 && tm.calls.size()==1 && tm.draws==0 &&
          tm.calls.back().name=="terrain_heightBelow" && terrain.load(C+0x232,2)==6,
          "missing terrain retains path and seed prefix");
  tm.callbacks["terrain_heightBelow"]=[](auto &,const auto &)->A::Value{return A::Vec3{};};
  throws<A::Error>([&]{tm.skelight_set_state(C,1);});
  require(tm.calls.size()==2 && terrain.load(C+0x104,8)==0,"malformed terrain before destination store");
  install_zek(tm,{0,0,0});tm.skelight_set_state(C,1);
  require(terrain.load(C+0x232,2)==1 && terrain.load(C+0xf4,4)==300,"reuse wander after service failure");
  A::Memory bounded;A::Model bw(bounded,{1048576,5,64,64});
  throws<champions::simulation::rta::creature_hp::ResourceLimit>([&]{bw.skelight_set_state(C,1);});
  require(bw.writes.size()==5 && bw.calls.empty(),"wander path history prefix");
  A::Memory recursive_wander;A::Model rw(recursive_wander,{1048576,65536,64,6});
  recursive_wander.poke(C+0x3c0,4,0x43960000);
  rw.callbacks["terrain_heightBelow"]=[](auto &model,const auto &)->A::Value {
    model.skelight_set_state(C,1);return std::int64_t(0);
  };
  throws<champions::simulation::rta::creature_hp::ResourceLimit>([&]{rw.skelight_set_state(C,1);});
  require(rw.calls.size()==5 && recursive_wander.load(C+0x232,2)==0,"wander recursive prefix bounded");
  install_zek(rw,{0,0,0});rw.skelight_set_state(C,1);
  require(recursive_wander.load(C+0x232,2)==1,"wander reuse after recursive refusal");
  A::Memory invalid;A::Model bad(invalid);install(bad,{1,0,0,0});
  bad.callbacks["model_getSamplePoint"]=[](auto &,const auto &) -> A::Value {return std::int64_t(0);};
  throws<A::Error>([&]{bad.skelight_set_state(C,20);});
  require(invalid.load(C+0xc,4)==2 && invalid.load(C+0x232,2)==0,"invalid sample retains death prefix");
  A::Memory limitmem;A::Model limited(limitmem,{1048576,3,64,64});install(limited,{0,0,0,0});
  throws<champions::simulation::rta::creature_hp::ResourceLimit>([&]{limited.skelight_set_state(C,20);});
  require(limited.writes.size()==3,"history budget bounded");
  throws<champions::simulation::rta::creature_hp::ResourceLimit>([&]{m.skelight_set_state(UINT64_MAX,20);});
  // Handler replacement is supported and the current callable stays alive.
  A::Memory replace;A::Model repl(replace);install(repl,{0,0,0,0});unsigned ran=0;
  repl.callbacks["anim_start"]=[&](auto &model,const auto &) -> A::Value {
    ++ran;model.callbacks["anim_start"]=[&](auto &,const auto &) -> A::Value {ran+=10;return std::int64_t(0);};
    return std::int64_t(0);
  };
  repl.skelight_set_state(C,5);repl.skelight_set_state(C,5);require(ran==11,"callback replacement");
  A::Memory redirected;A::Model red(redirected);install(red,{0,0,0,0});
  redirected.poke(C+0x48,4,0x30000);redirected.poke(0x30084,4,0x2c2440);
  throws<A::Unsupported>([&]{red.skelight_set_state(C,20);});
  A::Memory base;A::Model bm(base);install(bm,{0,0,0,0,0});
  bm.callbacks["balance_lookup"]=[](auto &,const auto &) -> A::Value {return std::int64_t(0);};
  throws<A::Error>([&]{bm.creature_ctor(C,1,{0,0,0},0,D,0,0);});
  require(base.load(C+0x34,4)==1 && base.load(C+0x2fa,2)==0,"malformed balance prefix");
  base.poke(C+0x234,4,0x40000);base.poke(0x4000c,4,0x41000);
  base.poke(C+0x48,4,0x30000);base.poke(0x30000+26*8+4,4,0x10bb08);
  throws<A::Unsupported>([&]{bm.turn_to_target(C);});
  bm.external_slot26=true;bm.turn_to_target(C);
  require(bm.calls.back().name=="Creature.v26","explicit base turning delegate");
  bm.external_slot26=false;base.poke(0x30000+26*8+4,4,0x3c2738);
  throws<A::Unsupported>([&]{bm.turn_to_target(C);});
  A::Memory death;A::Model dm(death);install(dm,{0,0,0,0,4});
  dm.callbacks["script_runByName"]=[](auto &,const auto &) -> A::Value {throw CallbackError();};
  throws<CallbackError>([&]{dm.slot28_on_death(C,0);});
  require(death.load(0x4039f8,4)==C,"death script exception keeps selection");
  A::Memory partial;A::Model pm(partial,{1048576,1,64,64});
  partial.poke(C+0x24,4,D);partial.poke(D+4,4,0x60000);
  throws<champions::simulation::rta::creature_hp::ResourceLimit>([&]{pm.set_def_entry_params(C,0,{1,2,3,4},{5,6,7,8});});
  require(partial.load(0x60018,1)==1 && partial.load(0x60014,4)==0,"event write refusal prefix");
  // Validate the creature before adding field offsets or invoking services.
  A::Memory addresses;A::Model am(addresses);
  using Limit=champions::simulation::rta::creature_hp::ResourceLimit;
  throws<Limit>([&]{am.creature_ctor(UINT64_MAX,0,{0,0,0},0,0,0,0);});
  throws<Limit>([&]{am.set_def_entry_params(UINT64_MAX,0,{0,0,0,0},{0,0,0,0});});
  throws<Limit>([&]{am.turn_to_target(UINT64_MAX);});
  throws<Limit>([&]{am.can_see_target_in(UINT64_MAX,0,0,0,0,0,0);});
  throws<Limit>([&]{am.is_target_charging(UINT64_MAX);});
  throws<Limit>([&]{am.count_allies_near_target(UINT64_MAX,0);});
  throws<Limit>([&]{am.is_blocked_by_target(UINT64_MAX);});
  throws<Limit>([&]{am.is_not_rooted(UINT64_MAX);});
  throws<Limit>([&]{am.is_attack_ready(UINT64_MAX);});
  throws<Limit>([&]{am.is_not_stunned(UINT64_MAX);});
  throws<Limit>([&]{am.slot16_model(UINT64_MAX);});
  throws<Limit>([&]{am.slot25_can_see_target(UINT64_MAX,0);});
  throws<Limit>([&]{am.slot28_on_death(UINT64_MAX,0);});
  throws<Limit>([&]{am.slot34_set_home_range(UINT64_MAX,0);});
  require(am.events.empty() && addresses.bytes().empty(),"invalid addresses refuse before effects");
  // Reentrant callbacks are bounded without a permanent failure latch.
  A::Memory recursion;A::Model rec(recursion,{1048576,65536,64,4});install(rec,{0,0,0,0});
  rec.callbacks["anim_start"]=[&](auto &model,const auto &) -> A::Value {model.skelight_set_state(C,5);return std::int64_t(0);};
  throws<champions::simulation::rta::creature_hp::ResourceLimit>([&]{rec.skelight_set_state(C,5);});
  install(rec,{0,0,0,0});rec.skelight_set_state(C,23);require(recursion.load(C+0x232,2)==23,"reuse after failure");
}
using Result = A::CallbackResult;
std::string shape_text(const Result &v) {
  switch(v.kind) {
  case Result::Kind::none:return "n";
  case Result::Kind::integer:return "i"+std::string(v.negative?"-":"")+std::to_string(v.magnitude)+";";
  case Result::Kind::text:case Result::Kind::bytes: {
    const char *hex="0123456789abcdef";std::string out=v.kind==Result::Kind::text?"s":"b";
    for(unsigned char c:v.data){out+=hex[c>>4];out+=hex[c&15];}return out+";";
  }
  case Result::Kind::list:case Result::Kind::tuple: {
    std::string out=v.kind==Result::Kind::list?"l":"t";out+=std::to_string(v.elements.size())+":";
    for(const auto &e:v.elements)out+=shape_text(e);return out;
  }
  }
  throw std::runtime_error("invalid shape kind");
}
Result parse_shape(const std::string &s,std::size_t &p,unsigned depth=0) {
  require(p<s.size() && depth<64,"shape bounds");char tag=s[p++];
  if(tag=='n')return {};
  if(tag=='i'||tag=='s'||tag=='b') {
    auto end=s.find(';',p);require(end!=s.npos,"shape scalar terminator");auto data=s.substr(p,end-p);p=end+1;
    if(tag=='i')return !data.empty() && data[0]=='-'?Result::integer(std::stoll(data)):Result::unsigned_integer(std::stoull(data));
    require(data.size()%2==0,"shape hex length");std::string bytes;
    for(std::size_t k=0;k<data.size();k+=2)bytes+=char(std::stoul(data.substr(k,2),nullptr,16));
    return tag=='s'?Result::text(bytes):Result::bytes(bytes);
  }
  require(tag=='l'||tag=='t',"shape sequence tag");auto end=s.find(':',p);require(end!=s.npos,"shape sequence count");
  auto count=std::stoull(s.substr(p,end-p));require(count<=100,"shape sequence bound");p=end+1;
  std::vector<Result> values;for(U k=0;k<count;++k)values.push_back(parse_shape(s,p,depth+1));
  return tag=='l'?Result::list(values):Result::tuple(values);
}
Result parse_shape(const std::string &s) {std::size_t p=0;auto v=parse_shape(s,p);require(p==s.size(),"complete shape value");return v;}
std::string shape_case(const std::string &name,const std::string &arguments,const std::string &options) {
  auto args=parse_shape(arguments),opts=parse_shape(options);
  require(args.kind==Result::Kind::list && opts.kind==Result::Kind::tuple && opts.elements.size()==3,"shape input kinds");
  const auto &out=opts.elements[2];require(out.kind==Result::Kind::none || out.kind==Result::Kind::list,"shape output kind");
  try {return shape_text(A::shape_result(name,args.elements,opts.elements[0],opts.elements[1],
      out.kind==Result::Kind::none?nullptr:&out.elements));}
  catch(const A::Error &){return "argument_error";}
}
void shape_host_tests() {
  throws<A::Error>([]{A::shape_result("unknown",{});});
  require(shape_text(Result::integer(INT64_MIN))=="i-9223372036854775808;","minimum result integer");
  require(shape_text(Result::unsigned_integer(UINT64_MAX))=="i18446744073709551615;","raw64 result integer");
  // Null pointers suppress optional outputs after consuming their supplied value.
  std::vector<Result> args(7,Result::integer(1));args[5]=Result::integer(0);
  std::vector<Result> out={Result::integer(11),Result::integer(22)};
  require(shape_text(A::shape_result("model_getSamplePoint",args,{}, {},&out))=="i11;","optional sample consumption");
  require(shape_text(A::shape_result("obj_delete",{}))=="n","void remains None");
  A::Memory mem;A::Model m(mem);
  throws<A::MissingHandler>([&]{m.prop_int(C,0);});
  require(m.calls.size()==1,"marshaller must not install engine defaults");
}

void install_navigation(A::Model &m,const std::vector<U> &opts) {
  auto count=std::make_shared<unsigned>(0);auto per=std::make_shared<std::map<std::string,unsigned>>();
  if(opts[0]&64)for(auto name:{"Creature.v25","Creature.v26","Creature.v27","Creature.v35"})m.external.insert(name);
  for(auto name:{"state_request","Creature.v25","Creature.v26","Creature.v27","Creature.v35","Creature.v6","Creature.v10","Creature.v11",
    "Holder.v17","creature_canSeeObj","creature_findEnemyInCone","Creature_hitTargetInArc","Creature_hitAllEnemiesInArc",
    "pool_new","GroundPoundEffect_ctor","handle_link","handle_clear","Creature_deadUpdate",
    "world_slideMove","world_collectObjectsInBox","terrain_heightBelow","obj_updateGridCell","world_raycast",
    "world_findFreeSpot","water_test","water_disturb","libc_sprintf","res_find","sound_playOnObject",
    "path_requestType1","path_requestType2","path_requestType3","path_requestToPoint",
    "SpellDrowsy_typeinfo","creature_findSpellByType","SpellDrowsy_getStat3Fraction",
    "anim_start","anim_startBlendInOut","anim_syncToSimFrame","Creature_dropLoot","Creature.v28","creature_stopBossMusic"}) {
    m.callbacks[name]=[opts,count,per,name](auto &model,const auto &args)->A::Value {
      auto c=++*count;std::string str=name;auto n=(*per)[str]++;auto s=opts[0];
      if(opts[1]&&c==opts[1])throw CallbackError();
      if(s&1) {
        model.mem.store(C+0x38,2,0x8123+c);model.mem.store(C+0x300,4,700+c);
        model.mem.store(C+0x11a,2,c&1?8:0);model.mem.store(C+0xd0,2,0x8000+c);
        model.mem.store(C+0x44,4,0x87654321+c);model.mem.store(C+0x47c,4,c&1?8:6);
      }
      if(s&1024) {
        model.mem.store(0x44fdcc,4,64+c);model.mem.store(C+0x230,1,c);
        model.mem.store(C+0x2bc,4,16);model.mem.store(C+0x234,4,c&1?0x40000:0);
      }
      if(s&4096) {
        model.mem.store(0x30000+8*26+4,4,0x10bb08);model.mem.store(0x30000+8*25+4,4,0x10e658);
        model.mem.store(0x30000+8*27+4,4,0x3c2688);model.mem.store(C+0x3ac,2,200);
        model.mem.store(C+0x3ae,2,400);model.mem.store(C+0x3b0,2,0x7fff);
      }
      if((s&8192)&&str=="terrain_heightBelow") {
        model.mem.store(C+0x3b4,4,0x41a00000);model.mem.store(C+0x3b8,4,0xc1a00000);
        model.mem.store(C+0x18,4,0x40000000);model.mem.store(C+0x44,4,0x80000000+c);
      }
      if(str=="state_request") {
        if(s&16)model.zek_set_state(U(std::get<std::int64_t>(args[0])),std::uint32_t(std::get<std::int64_t>(args[1])));
        else model.mem.store(C+0x232,2,U(std::get<std::int64_t>(args[1])));
      }
      if(str=="creature_canSeeObj"||str=="Creature.v25")return std::int64_t((opts[2]>>(n%32))&1);
      if(str=="creature_findEnemyInCone")return std::int64_t((opts[3]>>(n%32))&1?0x50000:0);
      if(str=="Creature.v26")return std::int64_t(champions::simulation::rta::s32(std::uint32_t(opts[6])));
      if(str=="Creature.v35")return std::int64_t(3);
      if(str=="Holder.v17")return std::int64_t(0x75000+c);
      if(str=="pool_new")return std::int64_t(opts[7]?0x76000:0);
      if(str=="world_collectObjectsInBox")return s&8?A::Objects{{0x60000,std::uint32_t(C),0x60100}}:A::Objects{};
      if(str=="world_slideMove") {
        if(s&4) {
          auto a=U(std::get<std::int64_t>(args[0]));auto d=std::get<A::Vec3>(args[1]);
          for(unsigned k=0;k<3;++k)model.mem.store(a+4*k,4,champions::simulation::rta::add(std::uint32_t(model.mem.load(a+4*k,4)),d[k]));
        }
        if(std::get<std::int64_t>(args[3]))return A::Slide{1,{0x3f800000,0x80000000,0}};
        return std::int64_t(0);
      }
      if(str=="terrain_heightBelow")return std::int64_t(opts[5]);
      if(str=="world_raycast") {
        auto t=std::array<std::uint32_t,5>{0x3f800000,0xbf800000,0x3e800000,0x40000000,std::array<std::uint32_t,3>{0x3e800000,0x3f000000,0x3f400000}[n%3]}[opts[4]];
        if(std::get<std::int64_t>(args[4]))return A::RayHit{t,n&1?0x60000u:0u};
        return std::int64_t(t);
      }
      if(str=="world_findFreeSpot")return A::FreeSpot{{0x41200000,0xc1200000,0x40400000},s&512?0xffu:0u};
      if(str=="water_test"||str=="water_disturb")return std::int64_t(s&256?1:0);
      if(str=="libc_sprintf")return std::string("synthetic-water");
      if(str=="res_find")return std::int64_t(0x77000);
      if(str=="SpellDrowsy_typeinfo"||str=="creature_findSpellByType")return std::int64_t(s&128?0x78000:0);
      if(str=="SpellDrowsy_getStat3Fraction")return std::int64_t(0x3f000000);
      if((s&2048)&&str=="Creature.v6") {
        model.mem.store(C+0x1c,4,0x41a00000);model.mem.store(C+12,4,0x8000000);model.mem.store(C+0x234,4,0x40000);model.mem.store(0x4000c,4,0x60000);
      }
      return std::int64_t(0);
    };
  }
}
std::string navigation_case(const std::string &init,const std::string &commands,const std::string &options) {
  A::Memory mem;for(auto &p:rows(init)){require(p.size()==3,"navigation poke fields");mem.poke(p[0],std::size_t(p[1]),p[2]);}
  A::Model m(mem);auto opts=rows(options);require(opts.size()==1&&opts[0].size()==8,"navigation options");install_navigation(m,opts[0]);
  std::vector<std::string> results;
  for(auto &cmd:rows(commands)) {
    require(cmd.size()==2,"navigation command fields");
    try {
      switch(cmd[0]) {
      case 700:m.zek_switch(C,std::uint32_t(cmd[1]),[&](auto c,auto state){
        // Same caller-supplied setter as the reference's lambda. Model.call is
        // private; record its canonical call before invoking the test handler.
        A::Call entry{"state_request",{std::int64_t(c),std::int64_t(state)}};
        m.calls.push_back(entry);m.events.emplace_back(entry);m.callbacks.at("state_request")(m,entry.args);
      });results.push_back("ok");break;
      case 701:results.push_back("r"+std::to_string(m.move_to(C,{0x43480000,0x42c80000,0x40400000})));break;
      case 702:m.move_to_target(C);results.push_back("ok");break;
      case 703:results.push_back("r"+std::to_string(m.move_to_dest(C)));break;
      case 704:m.flee_from_target(C,std::uint32_t(cmd[1]));results.push_back("ok");break;
      case 705:results.push_back("r"+std::to_string(m.is_away_from_home(C)));break;
      case 706:results.push_back("r"+std::to_string(m.move_by(C,{0x42700000,0xc2700000,0})));break;
      case 707:m.settle(C);results.push_back("ok");break;
      default:throw std::runtime_error("unknown navigation operation");
      }
    }catch(const CallbackError &){results.push_back("callback_error");break;}
    catch(const std::exception &){std::cerr<<"last callback "<<(m.calls.empty()?"none":m.calls.back().name)<<"\n";throw;}
  }
  return snapshot(m,results);
}
void navigation_host_tests() {
  using L=champions::simulation::rta::creature_hp::ResourceLimit;
  A::Memory mem;A::Model m(mem);auto setter=[](auto,auto){};
  throws<A::MissingHandler>([&]{m.zek_switch(C,0,{});});require(m.calls.empty()&&m.writes.empty(),"missing setter before effects");
  throws<L>([&]{m.zek_switch(UINT64_MAX,0,setter);});
  throws<L>([&]{m.move_by(UINT64_MAX,{});});throws<L>([&]{m.move_to(UINT64_MAX,{});});
  throws<L>([&]{m.move_to_target(UINT64_MAX);});throws<L>([&]{m.move_to_dest(UINT64_MAX);});
  throws<L>([&]{m.flee_from_target(UINT64_MAX,0);});throws<L>([&]{m.settle(UINT64_MAX);});throws<L>([&]{m.is_away_from_home(UINT64_MAX);});
  mem.poke(C+0x232,2,6);mem.poke(C+0xa8,8,0x100000);
  throws<A::MissingHandler>([&]{m.zek_switch(C,1,setter);});require(mem.load(C+12,4)&0x8000000,"settle failure retains flag");
  install_navigation(m,{0,0,0,0,0,0,0,0});m.callbacks["world_collectObjectsInBox"]=[](auto &,const auto &)->A::Value{return std::int64_t(0);};
  throws<A::Error>([&]{m.zek_switch(C,1,setter);});
  m.callbacks["world_collectObjectsInBox"]=[](auto &,const auto &)->A::Value{return A::Objects{std::vector<std::uint32_t>(17,0)};};
  throws<L>([&]{m.zek_switch(C,1,setter);});
  install_navigation(m,{0,0,0,0,0,0,0,0});m.zek_switch(C,1,setter);require(!(mem.load(C+12,4)&0x8000000),"reuse after failure finishes settle");
  A::Memory rm;A::Model rec(rm);rm.poke(C+0x232,2,0);
  rec.callbacks["Creature.v25"]=[&](auto &model,const auto &)->A::Value{model.zek_switch(C,0,setter);return std::int64_t(0);};
  throws<L>([&]{rec.zek_switch(C,0,setter);});install_navigation(rec,{0,0,0,0,0,0,0,0});rec.zek_switch(C,0,setter);
  champions::simulation::rta::creature_hp::Limits limits;limits.history_entries=2;A::Memory lm;A::Model limited(lm,limits);lm.poke(C+0x232,2,6);
  throws<L>([&]{limited.zek_switch(C,0,setter);});require(limited.writes.size()==2,"bounded prefix");
}

void install_tick(A::Model &m,const std::vector<U> &opts) {
  auto count=std::make_shared<unsigned>(0);auto per=std::make_shared<std::map<std::string,unsigned>>();
  if(opts[0]&16)for(auto name:{"Creature.v16","Creature.v17","Creature.v20","Creature.v23","Creature.v25","Creature.v26","Creature.v27"})m.external.insert(name);
  const std::string names=
    "world_findObjectByName model_getSamplePointCount model_getSamplePoint Spell.v1 Spell.v2 terrain_height world_collectObjectsInArc Creature.v16 Creature.v17 Creature.v19 Creature.v20 Creature.v23 Creature.v25 Creature.v26 Creature.v27 Creature.v28 Creature.v35 Creature.v37 Holder.v15 Holder.v17 Holder.v21 Holder.v22 Holder.v11 Skill.v1 Skill.v2 Skill.v11 Skill.v12 Skill.v14 SkillLifeTap_getStat0Fraction world_notifyObjectsNear model_getBounds fx_spawn libc_sprintf res_find sound_playOnObject model_getAttachPoint particle_spawn pool_new GutChunk_ctor PhProp_initPhysics decal_add fx_shatterToLooseIce fx_disintegrateModel obj_getPropInt obj_getPropStr anim_snapTopToEnd player_getSkillRank player_giveExp OniBall_ctor iter_begin iter_next iter_end pathFollower_init paths_findByName pointList_snapToNearest pointList_getPoint terrain_raycast players_anyWithinRadius obj_updateGridCell obj_runEveryFrame net_objOrGhost fx_burnModel fx_shockModel damage_apply creature_bossRank Critter_ctor handle_clear handle_link handle_removeNode script_runByName anim_releaseCaches player_speedScale anim_advance Creature_applyKnockback creature_canSeeObj creature_findEnemyInCone creature_countAlliesNear creature_isBlockedBy Creature_hitTargetInArc Creature_hitAllEnemiesInArc Creature_deadUpdate world_collectObjectsInBox world_slideMove terrain_heightBelow world_raycast world_findFreeSpot water_test water_disturb path_requestType1 path_requestType2 path_requestType3 path_requestToPoint SpellDrowsy_typeinfo creature_findSpellByType SpellDrowsy_getStat3Fraction anim_start anim_startBlendInOut anim_syncToSimFrame anim_push anim_jumpTopToEnd Creature_dropLoot creature_stopBossMusic GroundPoundEffect_ctor fx_ringBurst LightEffect_ctor";
  for(auto name:split(names,' ')) {
    m.callbacks[name]=[opts,count,per,name](auto &model,const auto &args)->A::Value {
      auto c=++*count;auto n=(*per)[name]++;auto s=opts[0];
      if(opts[1]&&c==opts[1])throw CallbackError();
      if(s&4){model.mem.store(C+0x38,2,0x8123+c);model.mem.store(C+0x300,4,700+c);model.mem.store(C+0x11a,2,c&1?8:0);model.mem.store(C+0x44,4,0x87654321+c);model.mem.store(C+0x2dc,4,c&1?0x400000:0x80000);model.mem.store(C+0x228,4,0x42c80000);}
      if(s&32){model.mem.store(0x44fdcc,4,c*32);model.mem.store(C+0x230,1,c);model.mem.store(C+0x234,4,c&1?0x40000:0);model.mem.store(C+0x3c8,4,0x40100);}
      auto arg=[&](unsigned k){return U(std::get<std::int64_t>(args.at(k)));};
      if((s&8192)&&name=="player_giveExp")model.mem.store(0x4d8ae0,4,1);
      if(name=="Creature.v27"){model.mem.store(arg(0)+0x232,2,arg(1));if(s&65536)return Result::tuple({Result::integer(-7),Result::bytes(std::string("\0\xff",2)),Result::list({Result::integer(0),Result{}})});}
      if(name=="Creature.v16")return std::int64_t(0xa0000);
      if(name=="Creature.v20"||name=="Creature.v25"||name=="creature_canSeeObj")return std::int64_t((opts[3]>>(n%32))&1);
      if(name=="Creature.v26")return std::int64_t(0);
      if(name=="creature_findEnemyInCone")return std::int64_t(opts[4]?0x50000:0);
      if(name=="creature_countAlliesNear"||name=="creature_isBlockedBy")return std::int64_t(opts[4]);
      if(name=="Creature.v35")return std::int64_t(3);
      if(name=="Creature.v37")return std::int64_t(1);
      if(name=="pool_new")return std::int64_t(opts[5]?0xf0000+n*0x1000:0);
      if(name=="model_getBounds")return A::Bounds{{0,0,0xfffffff6,0,0,20}};
      if(name=="model_getAttachPoint")return A::Vec3{0x3f800000,0x40000000,0x40400000};
      if(name=="libc_sprintf")return std::string("synthetic-effect");
      if(name=="res_find")return std::int64_t(0xf8000);
      if(name=="obj_getPropInt")return std::int64_t(opts[7]);
      if(name=="obj_getPropStr")return std::int64_t(opts[7]?0xfa000:0);
      if(name=="net_objOrGhost")return std::int64_t(C);
      if(name=="creature_bossRank")return std::int64_t(s&128?1:0);
      if(name=="player_getSkillRank")return std::int64_t(3);
      if(name=="SkillLifeTap_getStat0Fraction")return std::int64_t(0x3f000000);
      if(name=="Skill.v11"||name=="Skill.v12")return std::int64_t(10);
      if(name=="Skill.v14")return std::int64_t(3);
      if(name=="Skill.v1")return std::int64_t(opts[4]);
      if(name=="iter_begin")return std::int64_t(s&2048?0x90000:0);
      if(name=="iter_next")return std::int64_t(0);
      if(name=="paths_findByName")return std::int64_t(0xfb000);
      if(name=="pointList_snapToNearest")return std::int64_t(0);
      if(name=="pointList_getPoint")return std::int64_t(n==0?0xfc000:0);
      if(name=="terrain_raycast")return std::int64_t(opts[6]);
      if(name=="players_anyWithinRadius")return std::int64_t(s&64?1:0);
      if(name=="player_speedScale")return std::int64_t(0x3f000000);
      if(name=="anim_advance"){
        if(s&64){model.mem.store(C+0xbc,4,0x3f800000);model.mem.store(C+0xc0,4,0xbf800000);model.mem.store(C+0xc4,2,0xffff);model.mem.store(C+0x2fc,1,60);model.mem.store(C+0xdc,4,0x40000000);}return std::int64_t(opts[2]);}
      if(name=="Holder.v15")return std::int64_t(0xffffffff);
      if(name=="Holder.v17")return std::int64_t(0xfd000);
      if(name=="world_collectObjectsInArc")return A::Objects{};
      if(name=="world_collectObjectsInBox")return arg(2)==0xc0?A::Objects{{0x50000,0,0x50000}}:A::Objects{};
      if(name=="world_slideMove"){if(arg(3))return A::Slide{};return std::int64_t(0);}
      if(name=="terrain_heightBelow")return std::int64_t(0x40400000);
      if(name=="world_raycast"){if(arg(4))return A::RayHit{0x3f800000,0};return std::int64_t(0x3f800000);}
      if(name=="world_findFreeSpot")return A::FreeSpot{};
      if(name=="SpellDrowsy_typeinfo"||name=="creature_findSpellByType")return std::int64_t(0xfe000);
      if(name=="SpellDrowsy_getStat3Fraction")return std::int64_t(0x3f000000);
      return std::int64_t(0);
    };
  }
}
std::string tick_case(const std::string &init,const std::string &commands,const std::string &options) {
  A::Memory mem;for(auto &p:rows(init)){require(p.size()==3,"tick poke fields");mem.poke(p[0],std::size_t(p[1]),p[2]);}
  champions::simulation::rta::creature_hp::Limits limits;limits.callback_entries=128;
  A::Model m(mem,limits);auto opts=rows(options);require(opts.size()==1&&opts[0].size()==8,"tick options");install_tick(m,opts[0]);
  std::vector<std::string> results;
  for(auto &cmd:rows(commands))try {
    switch(cmd[0]){
    case 900:case 901:case 902:m.reference_defaults=true;results.push_back(shape_text(A::callback_value(m.vcall(C,4))));break;
    case 800:m.horsemen_run(C);results.push_back("ok");break;
    case 801:m.skelight_run(C);results.push_back("ok");break;
    case 802:m.zek_run(C);results.push_back("ok");break;
    case 803:m.process_damage(C,m.class_model(C));results.push_back("ok");break;
    case 804:results.push_back("r"+std::to_string(m.creature_run(C,m.class_model(C))));break;
    case 805:results.push_back("r"+std::to_string(m.update_active(C)));break;
    case 806:results.push_back("r"+std::to_string(m.step_anim(C,0x3f800000)));break;
    default:throw std::runtime_error("unknown tick operation");
    }
  }catch(const CallbackError &){results.push_back("callback_error");break;}
  return snapshot(m,results);
}
void tick_host_tests() {
  using L=champions::simulation::rta::creature_hp::ResourceLimit;
  A::Memory mem;A::Model m(mem);
  throws<L>([&]{m.horsemen_run(UINT64_MAX);});throws<L>([&]{m.skelight_run(UINT64_MAX);});throws<L>([&]{m.zek_run(UINT64_MAX);});
  throws<L>([&]{m.process_damage(UINT64_MAX,0);});throws<L>([&]{m.creature_run(UINT64_MAX,0);});throws<L>([&]{m.update_active(UINT64_MAX);});throws<L>([&]{m.step_anim(UINT64_MAX,0);});require(m.writes.empty()&&m.calls.empty(),"tick address guard before effects");
  mem.poke(C+0xc8,4,0x3f800000);mem.poke(C+0x228,4,0x3f800000);mem.poke(C+0x2dc,4,1);
  throws<A::MissingHandler>([&]{m.process_damage(C,1);});require(mem.load(C+0xc8,4)==0&&mem.load(C+12,4)==0x100,"missing bounds retains damage prefix");
  mem.poke(C+0xc8,4,0x3f800000);m.callbacks["model_getBounds"]=[](auto &,const auto &)->A::Value{return A::Vec3{};};
  throws<A::Error>([&]{m.process_damage(C,1);});
  m.callbacks["model_getBounds"]=[](auto &,const auto &)->A::Value{return A::Bounds{};};mem.poke(C+0xc8,4,0x3f800000);mem.poke(C+0x2dc,4,0x400001);mem.poke(C+0xa8,8,0x4000);m.callbacks["fx_spawn"]=[](auto &,const auto &)->A::Value{return std::int64_t(0);};m.process_damage(C,1);require(mem.load(C+0x2dc,4)==0,"damage reusable after malformed callback");
  A::Memory rm;A::Model rec(rm);rm.poke(C+0x48,4,0x30000);rm.poke(0x30000+8*17+4,4,0x11a868);
  rec.callbacks["iter_begin"]=[](auto &model,const auto &)->A::Value{model.creature_run(C,0);return std::int64_t(0);};throws<L>([&]{rec.creature_run(C,0);});
  rec.callbacks["iter_begin"]=[](auto &,const auto &)->A::Value{return std::int64_t(0);};rec.callbacks["iter_end"]=[](auto &,const auto &)->A::Value{return std::int64_t(0);};rec.creature_run(C,0);
  A::Memory am;A::Model active(am);throws<A::MissingHandler>([&]{active.update_active(C);});require(active.writes.size()==1,"activity failure prefix");
  champions::simulation::rta::creature_hp::Limits limits;limits.history_entries=1;A::Memory lm;A::Model limited(lm,limits);lm.poke(C+0x2dc,4,0x40000);throws<L>([&]{limited.process_damage(C,0);});require(limited.writes.size()==1,"tick history prefix");
}

A::Value dispatch_value(const Result &r) {
  if(r.kind==Result::Kind::integer && (r.negative||r.magnitude<=INT64_MAX))return r.negative?(-1-std::int64_t(r.magnitude-1)):std::int64_t(r.magnitude);
  if(r.kind==Result::Kind::none)return std::monostate{};
  if(r.kind==Result::Kind::text)return r.data;
  if(r.kind==Result::Kind::tuple && r.elements.size()==3) {
    A::Vec3 v{};bool words=true;
    for(unsigned k=0;k<3;++k){words&=r.elements[k].kind==Result::Kind::integer;v[k]=std::uint32_t(r.elements[k].magnitude);}
    if(words)return v;
  }
  return r;
}
std::string dispatch_call_case(const std::string &name,const std::string &arguments,unsigned cb) {
  A::Memory mem;A::Model m(mem);m.reference_defaults=true;auto parsed=parse_shape(arguments);
  std::vector<A::Value> args;for(const auto &r:parsed.elements)args.push_back(dispatch_value(r));
  if(cb)m.callbacks[name]=[cb](auto &,const auto &)->A::Value {
    if(cb==2)throw CallbackError();
    return Result::tuple({Result::integer(-1),Result::bytes(std::string("\0\xff",2)),Result::list({Result::integer(0),Result{}})});
  };
  std::string result;
  try{result=shape_text(A::callback_value(m.reference_call(name,args)));}
  catch(const A::Error &){result="error";}catch(const CallbackError &){result="error";}
  std::vector<Result> calls;
  for(const auto &c:m.calls){std::vector<Result> a;for(const auto &v:c.args)a.push_back(A::callback_value(v));calls.push_back(Result::tuple({Result::text(c.name),Result::tuple(a)}));}
  return result+"|"+shape_text(Result::list(calls));
}
std::string dispatch_case(const std::string &init,const std::string &options,const std::string &arguments) {
  A::Memory mem;for(const auto &p:rows(init))mem.poke(p[0],p[1],p[2]);A::Model m(mem);m.reference_defaults=true;
  auto opts=rows(options).at(0);auto kind=opts[0];auto slot=unsigned(opts[1]);
  const std::string cls=std::array<std::string,6>{"Creature","Skill","Spell","Creature","Holder","Custom"}.at(kind);
  auto name=cls+".v"+std::to_string(slot);
  if(opts[2])m.external.insert(name);
  if(opts[3])m.callbacks[name]=[cb=opts[3],name](auto &model,const auto &)->A::Value{
    if(cb==2)throw CallbackError();
    if(cb==3){model.mem.store(C+0x232,2,0x1234);model.callbacks[name]=[](auto &,const auto &)->A::Value{return std::int64_t(-7);};return std::int64_t(-9);}
    return std::int64_t(-1);
  };
  auto parsed=parse_shape(arguments);std::vector<A::Value> args;
  for(const auto &r:parsed.elements)args.push_back(dispatch_value(r));
  std::string result;
  try {
    A::Value v;
    if(kind==0)v=m.vcall(C,slot,args);
    else if(kind==1)v=m.skill_call(C,slot,args);
    else if(kind==2)v=m.table_call("Spell",C,0x14,slot,args);
    else if(kind==3)v=m.set_state(C,std::uint32_t(std::get<std::int64_t>(args[0])));
    else v=m.table_call(cls,C,0x20,slot,args);
    result=shape_text(A::callback_value(v));
  }catch(const A::Error &){result="error";}catch(const CallbackError &){result="error";}
  return snapshot(m,{result});
}
void dispatch_host_tests() {
  using L=champions::simulation::rta::creature_hp::ResourceLimit;
  A::Memory mem;A::Model m(mem);
  throws<L>([&]{m.vcall(UINT64_MAX,1);});throws<L>([&]{m.skill_call(UINT64_MAX,1);});throws<L>([&]{m.table_call("Spell",UINT64_MAX,0x14,2);});throws<L>([&]{m.set_state(UINT64_MAX,0);});
  throws<A::MissingHandler>([&]{m.reference_call("obj_delete",{std::int64_t(C)});});
  throws<A::MissingHandler>([&]{m.vcall(C,16);});
  m.reference_defaults=true;
  // A contract scope unwinds on exceptions; legacy required-handler behavior survives.
  throws<A::Error>([&]{m.reference_call("unknown");});
  throws<A::MissingHandler>([&]{m.prop_int(C,0);});
  m.callbacks["Creature.v42"]=[&](auto &model,const auto &)->A::Value{return model.vcall(C,42);};
  throws<L>([&]{m.vcall(C,42);});m.callbacks.erase("Creature.v42");
  require(std::holds_alternative<std::monostate>(m.reference_call("obj_delete",{std::int64_t(C)})),"reference None");
  // Handler replacement is synchronous and the return is never masked.
  m.callbacks["Creature.v42"]=[](auto &model,const auto &)->A::Value{model.callbacks["Creature.v42"]=[](auto &,const auto &)->A::Value{return std::int64_t(-7);};return std::int64_t(-9);};
  require(std::get<std::int64_t>(m.vcall(C,42))==-9,"original handler return");require(std::get<std::int64_t>(m.vcall(C,42))==-7,"replacement handler return");
}

int main(int argc,char **argv) {
  try {
    host_tests();shape_host_tests();navigation_host_tests();tick_host_tests();dispatch_host_tests();require(argc==2 || argc==3,"expected vector path and optional trace path");
    std::ofstream trace;if(argc==3){trace.open(argv[2]);require(bool(trace),"open state trace");}
    std::ifstream in(argv[1]);require(bool(in),"open vectors");
    std::string line;unsigned count=0;
    while(std::getline(in,line)) {
      if(line.empty()||line[0]=='#')continue;
      auto fields=split(line,'\t');require(fields.size()==5,"vector columns");
      if(fields[1].rfind("dispatch:",0)==0 || fields[1].rfind("dispatchcall:",0)==0) {
        const bool direct=fields[1].rfind("dispatchcall:",0)==0;
        auto actual=direct?dispatch_call_case(fields[1].substr(13),fields[2],unsigned(std::stoul(fields[3]))):dispatch_case(fields[1].substr(9),fields[2],fields[3]);
        if(actual!=fields[4]){std::cerr<<"dispatch case "<<fields[0]<<" "<<fields[1]<<"\nactual: "<<actual<<"\nexpected: "<<fields[4]<<'\n';return 1;}
        if(argc==3)trace<<fields[0]<<'\t'<<actual<<'\n';++count;continue;
      }
      if(fields[1].rfind("tick:",0)==0) {
        std::string actual;try {actual=tick_case(fields[1].substr(5),fields[2],fields[3]);}
        catch(const std::exception &e){std::cerr<<"tick case "<<fields[0]<<": "<<e.what()<<'\n';throw;}
        if(actual!=fields[4]){std::cerr<<"tick case "<<fields[0]<<"\nactual: "<<actual<<"\nexpected: "<<fields[4]<<'\n';return 1;}
        if(argc==3)trace<<fields[0]<<'\t'<<actual<<'\n';
        ++count;continue;
      }
      if(fields[1].rfind("navigation:",0)==0) {
        std::string actual;try {actual=navigation_case(fields[1].substr(11),fields[2],fields[3]);}
        catch(const std::exception &e){std::cerr<<"navigation case "<<fields[0]<<": "<<e.what()<<'\n';throw;}
        if(actual!=fields[4]){std::cerr<<"navigation case "<<fields[0]<<"\nactual: "<<actual<<"\nexpected: "<<fields[4]<<'\n';return 1;}
        if(argc==3)trace<<fields[0]<<'\t'<<actual<<'\n';
        ++count;continue;
      }
      if(fields[1].rfind("shape:",0)==0) {
        auto actual=shape_case(fields[1].substr(6),fields[2],fields[3]);
        if(actual!=fields[4]){std::cerr<<"shape case "<<fields[0]<<" "<<fields[1]<<" actual "<<actual<<" expected "<<fields[4]<<'\n';return 1;}
        if(argc==3)trace<<fields[0]<<'\t'<<actual<<'\n';
        ++count;continue;
      }
      if(fields[1].rfind("horsemen:",0)==0) {
        auto actual=horsemen_case(fields[1].substr(9),fields[2],fields[3]);
        if(actual!=fields[4]){std::cerr<<"Horsemen case "<<fields[0]<<"\nactual: "<<actual<<"\nexpected: "<<fields[4]<<'\n';return 1;}
        if(argc==3)trace<<fields[0]<<"\t"<<actual<<"\n";
        ++count;continue;
      }
      if(fields[1].rfind("zek:",0)==0) {
        auto actual=zek_case(fields[1].substr(4),fields[2],fields[3]);
        if(actual!=fields[4]){std::cerr<<"Zek case "<<fields[0]<<"\nactual: "<<actual<<"\nexpected: "<<fields[4]<<'\n';return 1;}
        if(argc==3)trace<<fields[0]<<'\t'<<actual<<'\n';
        ++count;continue;
      }
      if(fields[1].rfind("helper:",0)==0) {
        auto actual=helper_case(fields[1].substr(7),fields[2],fields[3]);
        if(actual!=fields[4]){std::cerr<<"helper case "<<fields[0]<<"\nactual: "<<actual<<"\nexpected: "<<fields[4]<<'\n';return 1;}
        if(argc==3)trace<<fields[0]<<'\t'<<actual<<'\n';
        ++count;continue;
      }
      if(fields[1].rfind("script:",0)==0) {
        auto actual=script_case(fields[1].substr(7),fields[2]);
        if(actual!=fields[4]){std::cerr<<"script case "<<fields[0]<<"\nactual: "<<actual<<"\nexpected: "<<fields[4]<<'\n';return 1;}
        if(argc==3)trace<<fields[0]<<'\t'<<actual<<'\n';
        ++count;continue;
      }
      if(fields[1].rfind("summon:",0)==0) {
        auto actual=summon_case(fields[1].substr(7),fields[2],fields[3]);
        if(actual!=fields[4]){std::cerr<<"summon case "<<fields[0]<<"\nactual: "<<actual<<"\nexpected: "<<fields[4]<<'\n';return 1;}
        if(argc==3)trace<<fields[0]<<'\t'<<actual<<'\n';
        ++count;continue;
      }
      if(fields[1].rfind("tagar:",0)==0) {
        auto actual=tagar_case(fields[1].substr(6),fields[2],fields[3]);
        if(actual!=fields[4]){std::cerr<<"tagar case "<<fields[0]<<"\nactual: "<<actual<<"\nexpected: "<<fields[4]<<'\n';return 1;}
        if(argc==3)trace<<fields[0]<<'\t'<<actual<<'\n';
        ++count;continue;
      }
      A::Memory mem;for(auto &p:rows(fields[1])){require(p.size()==3,"poke fields");mem.poke(p[0],std::size_t(p[1]),p[2]);}
      A::Model m(mem);auto opts=rows(fields[3]);require(opts.size()==1&&(opts[0].size()==4||opts[0].size()==5),"options");install(m,opts[0]);
      std::vector<std::string> results;
      for(auto &cmd:rows(fields[2])) {
        require(cmd.size()>=2,"command fields");
        try {
          if(cmd[0]==0){m.skelight_set_state(C,std::uint32_t(cmd[1]));results.push_back("ok");}
          else if(cmd[0]==1){auto r=m.filter_state(C,std::uint32_t(cmd[1]));results.push_back("f"+std::to_string(r.refused)+","+std::to_string(r.state));}
          else if(cmd[0]==2){m.skelight_shatter(C,std::uint32_t(cmd[1]));results.push_back("ok");}
          else if(cmd[0]==3) {
            require(cmd.size()==9,"constructor command fields");
            m.creature_ctor(C,std::uint32_t(cmd[1]),{std::uint32_t(cmd[2]),std::uint32_t(cmd[3]),std::uint32_t(cmd[4])},
                            std::uint32_t(cmd[5]),std::uint32_t(cmd[6]),std::uint32_t(cmd[7]),std::uint32_t(cmd[8]));results.push_back("ok");
          }
          else if(cmd[0]==4) {
            require(cmd.size()==10,"event command fields");
            m.set_def_entry_params(C,std::uint32_t(cmd[1]),
                {std::uint32_t(cmd[2]),std::uint32_t(cmd[3]),std::uint32_t(cmd[4]),std::uint32_t(cmd[5])},
                {std::uint32_t(cmd[6]),std::uint32_t(cmd[7]),std::uint32_t(cmd[8]),std::uint32_t(cmd[9])});results.push_back("ok");
          }
          else if(cmd[0]==9){m.slot34_set_home_range(C,std::uint32_t(cmd[1]));results.push_back("ok");}
          else if(cmd[0]==16){m.slot28_on_death(C,std::uint32_t(cmd[1]));results.push_back("ok");}
          else {
            std::int64_t r=0;
            switch(cmd[0]) {
            case 5:r=m.is_not_rooted(C);break;
            case 6:r=m.is_attack_ready(C);break;
            case 7:r=m.is_not_stunned(C);break;
            case 8:r=m.slot16_model(C);break;
            case 10:r=m.turn_to_target(C);break;
            case 11:
              require(cmd.size()==7,"visibility command fields");
              r=m.can_see_target_in(C,std::uint32_t(cmd[1]),std::uint32_t(cmd[2]),std::uint32_t(cmd[3]),
                                    std::uint32_t(cmd[4]),std::uint32_t(cmd[5]),std::uint32_t(cmd[6]));break;
            case 12:r=m.is_target_charging(C);break;
            case 13:r=m.count_allies_near_target(C,std::uint32_t(cmd[1]));break;
            case 14:r=m.is_blocked_by_target(C);break;
            case 15:r=m.slot25_can_see_target(C,std::uint32_t(cmd[1]));break;
            default:throw std::runtime_error("unknown operation");
            }
            results.push_back("r"+std::to_string(r));
          }
        } catch(const CallbackError &){results.push_back("callback_error");break;}
      }
      auto actual=snapshot(m,results);
      if(actual!=fields[4]){std::cerr<<"case "<<fields[0]<<"\nactual: "<<actual<<"\nexpected: "<<fields[4]<<'\n';return 1;}
      if(argc==3)trace<<fields[0]<<'\t'<<actual<<'\n';
      ++count;
    }
    if(argc==3){trace.flush();require(bool(trace),"write state trace");}
    require(in.eof()&&count>0,"complete nonempty vectors");std::cout<<count<<" exact cases plus host checks passed\n";
    return 0;
  } catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
