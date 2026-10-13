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
  if(auto p=std::get_if<std::int64_t>(&v))return "i"+std::to_string(*p);
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
void host_tests() {
  // Missing handlers preserve exact filter prefix, never assume callback success.
  A::Memory mem;A::Model m(mem);
  throws<A::MissingHandler>([&]{m.skelight_set_state(C,0);});
  require(m.writes.size()==1 && m.calls.size()==1 && mem.load(C+0x232,2)==0,"missing handler prefix");
  A::Memory wmem;A::Model wander(wmem);
  throws<A::Unsupported>([&]{wander.skelight_set_state(C,1);});
  require(wander.writes.size()==1 && wander.calls.empty(),"wander refuses after filter only");
  // An inert wander request is a modeled refusal, rather than unsupported.
  wmem.poke(C+0x231,1,1);wander.skelight_set_state(C,1);
  require(wander.writes.size()==1,"inert refusal has no added writes");
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
int main(int argc,char **argv) {
  try {
    host_tests();require(argc==2 || argc==3,"expected vector path and optional trace path");
    std::ofstream trace;if(argc==3){trace.open(argv[2]);require(bool(trace),"open state trace");}
    std::ifstream in(argv[1]);require(bool(in),"open vectors");
    std::string line;unsigned count=0;
    while(std::getline(in,line)) {
      if(line.empty()||line[0]=='#')continue;
      auto fields=split(line,'\t');require(fields.size()==5,"vector columns");
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
