#include "champions/monsters_1.hpp"
#include <iostream>
namespace M = champions::simulation::rta::monsters_1;
namespace H = champions::simulation::rta::creature_hp;
namespace {
int failures=0,checks=0;
void check(bool ok,const char *message) {
  ++checks;if (!ok) {++failures;std::cerr << message << '\n';}
}
template<class Exception,class Fn> void refuses(Fn f,const char *message) {
  bool caught=false;try {f();} catch(const Exception &) {caught=true;}check(caught,message);
}
constexpr unsigned C=0x10000,D=0x20000,T=0x30000,B=0x40000,O=0x50000;
void setup(M::Memory &mem) {
  mem.poke(C+0x24,4,D);mem.poke(C+0x48,4,T);mem.poke(C+0x44,4,0x12345678);
  mem.poke(T+8*27+4,4,0x2ab704);mem.poke(T+8*28+4,4,0x10beb8);
  mem.poke(O+12,4,0x40);
}
void checks_run() {
  M::Memory mem;setup(mem);M::Model m(mem);
  refuses<H::MissingHandler>([&]{m.mosquito_lose_part(C,1);},"missing model service must refuse");
  check(m.calls.size()==1 && m.writes.empty(),"missing service retains call and no fabricated writes");
  int hide_calls=0;
  m.callbacks["model_buildHideMask"]=[&](auto &,const auto &) {
    return ++hide_calls==1 ? UINT64_C(0x1111111122222222) : UINT64_C(0xfedcba9876543210);
  };
  m.callbacks["pool_new"]=[](auto &,const auto &){return 0;};
  m.mosquito_lose_part(C,1);
  check(mem.load(C+0xe8,4)==0x10 && mem.load(C+0x220,8)==UINT64_C(0xfedcba9876543210),"part mapping and full-width mask");
  auto count=m.calls.size();m.mosquito_lose_part(C,1);
  check(m.calls.size()==count+3,"repeated part still invokes pool");
  auto before=m.writes.size();m.mosquito_lose_part(C,0xffffffff);
  check(m.writes.size()==before+1,"invalid mosquito part writes only show mask");
  m.scorpion_sever_part(C,63);
  check(mem.load(C+0xe8,4)==0x80000010,"scorpion low-five shift");
  check(m.draws==0 && mem.load(C+0x44,4)==0x12345678,"part loss has no random draws");
  M::Memory spider;setup(spider);M::Model s(spider);
  refuses<H::MissingHandler>([&]{s.smallspider_set_state(C,0);},"idle missing handle clear refuses");
  check(s.writes.size()==1 && s.writes[0].address==C+0x318,"filter partial prefix preserved");
  s.callbacks["handle_clear"]=[](auto &,const auto &){return 0;};
  refuses<H::MissingHandler>([&]{s.smallspider_set_state(C,0);},"idle missing animation refuses");
  check(spider.load(C+0x44,4)==0x75432777 && spider.load(C+0xf4,4)==139,"idle exact RNG and timer prefix");
  check(spider.load(C+0x232,2)==0,"state not stored before failed animation");
  auto call_count=s.calls.size();
  s.callbacks["anim_start"]=[](auto &model,const auto &) -> std::uint64_t {
    model.callbacks["anim_start"]=[](auto &,const auto &){return 0;};return 0;
  };
  s.smallspider_set_state(C,0);s.smallspider_set_state(C,0);
  check(s.calls.size()==call_count+4,"callback replacement safely persists");
  spider.poke(T+8*27+4,4,0x267654);
  refuses<M::Unsupported>([&]{s.smallspider_on_touch(C,O);},"unported Gnaw run virtual body refuses");
  check(!(spider.load(C+0xa8,8)&UINT64_C(0x800000000000000)),"touch base not reached after refusal");
  s.external.insert("Creature.v27");
  s.callbacks["Creature.v27"]=[](auto &,const auto &){return 0;};
  s.smallspider_on_touch(C,O);
  check((spider.load(C+0xa8,8)&UINT64_C(0x800000000000000))!=0,"explicit virtual callback then base touch");
  M::Memory prefix;M::Model p(prefix);
  for (unsigned i=0;i<4097;++i) {prefix.poke(0x60000+i,1,'a');prefix.poke(0x70000+i,1,'a');}
  refuses<M::Unsupported>([&]{p.str_prefix_differs(0x60000,0x70000);},"unbounded prefix refuses");
  check(p.calls.empty() && p.writes.empty(),"prefix refusal leaves state untouched");
  prefix.poke(0x70000,1,0);
  check(p.str_prefix_differs(0x60000,0x70000)==0,"empty prefix matches");
  M::Memory chunks;setup(chunks);M::Model chunk(chunks,M::CallbackPolicy::ReferenceZero);
  chunk.callbacks["pool_new"]=[](auto &,const auto &){return B;};
  chunk.callbacks["GutChunk_ctorFromProp"]=[](auto &model,const auto &){model.mem.store(B+0x1c4,4,0xbf800000);return 0;};
  chunk.mosquito_lose_part(C,5);
  check(chunks.load(B+0x1c4,4)==0x3f800000,"chunk callback state lifted by original raw arithmetic");
  chunk.callbacks["model_buildHideMask"]=[&](auto &model,const auto &) -> std::uint64_t {
    model.mem.store(C+0xe8,4,0x40);throw M::Error("failure");
  };
  refuses<M::Error>([&]{chunk.scorpion_sever_part(C,2);},"callback exception propagates");
  check(chunks.load(C+0xe8,4)==0x40,"callback exception keeps mutation");
  chunk.callbacks["model_buildHideMask"]=[](auto &,const auto &){return 0;};
  chunk.callbacks["pool_new"]=[](auto &,const auto &){return UINT64_C(0x100000000);};
  refuses<M::Unsupported>([&]{chunk.mosquito_lose_part(C,2);},"oversized pool pointer refuses instead of wrapping");
  check(chunks.load(C+0xe8,4)==0x50,"pointer refusal preserves earlier variant update");
  refuses<M::Unsupported>([&]{chunk.smallspider_set_state(UINT64_MAX,0);},"invalid creature span refuses before address overflow");
  M::Memory states;setup(states);M::Model next(states,M::CallbackPolicy::ReferenceZero);
  next.mosquito_set_state(C,6);
  check(states.load(C+0xa8,8)&0x100000,"mosquito attacks disable water test");
  next.mosquito_set_state(C,0xffffffff);
  check(states.load(C+0x232,2)==0 && !(states.load(C+0xa8,8)&0x100000),"unknown mosquito state becomes idle and enables water test");
  states.poke(C+0x232,2,6);states.poke(C+0x11a,2,8);
  auto seed=states.load(C+0x44,4);
  next.badger_set_state(C,18);
  check(states.load(C+0x232,2)==6 && states.load(C+0x44,4)==seed,"playing badger front attack ignores reaction without RNG");
  next.badger_set_state(C,8);
  check(states.load(C+0x3c8,4)==0,"badger attack clears extension latch");
  check(std::get<std::uint32_t>(next.calls.back().args[2])==0x4808,"badger side attack retains swing flags");
  refuses<M::Unsupported>([&]{next.badger_set_state(UINT32_MAX-0x3c7u,6);},"badger extension span refuses before writes");
  next.callbacks["Creature_dropLoot"]=[](auto &model,const auto &) -> std::uint64_t {
    model.mem.store(C+0xa8,8,0x100001);throw M::Error("loot failure");
  };
  states.poke(C+0x232,2,3);
  refuses<M::Error>([&]{next.mosquito_set_state(C,20);},"death loot failure propagates");
  check(states.load(C+0x232,2)==3 && states.load(C+0xa8,8)==0x100001,"loot failure preserves mutation before final state and water flag writes");
  M::Memory chunks2;setup(chunks2);M::Model gnaw(chunks2,M::CallbackPolicy::ReferenceZero);
  gnaw.gnaw_set_state(C,20);
  check(chunks2.load(C+0x44,4)==0x12345678,"failed Gnaw allocations draw no RNG");
  unsigned effects=0,pools=0;
  for (const auto &c:gnaw.calls) {effects+=c.name=="fx_spawn";pools+=c.name=="pool_new";}
  check(effects==6 && pools==2,"Gnaw death emits six effects and attempts two allocations");
  M::Memory strict;setup(strict);M::Model required(strict);
  refuses<H::MissingHandler>([&]{required.gnaw_set_state(C,6);},"missing swing refuses with RNG prefix");
  check(strict.load(C+0x44,4)!=0x12345678 && strict.load(C+0x232,2)==0,"swing refusal keeps draw before state commit");
  states.poke(C+0x11a,2,0xabcd);
  check(next.anim_flags(C,0xffffffff)==0xabcd && next.anim_flags(C,0x10000)==0,"animation flags use exactly the unsigned16 field and full supplied mask");
  M::Memory ctor_mem;setup(ctor_mem);M::Model ctor(ctor_mem);
  refuses<H::MissingHandler>([&]{ctor.smallspider_ctor(C,0,{0x3f800000,0,0},0,0);},"constructor requires GameObject service");
  check(ctor.writes.empty() && ctor.calls.back().name=="GameObject_ctor","missing base service has no fabricated initialization");
  ctor.policy=M::CallbackPolicy::ReferenceZero;
  ctor.callbacks["GameObject_ctor"]=[](auto &model,const auto &) {
    model.mem.store(C+0x18,4,0x12345678);return 0;
  };
  ctor.smallspider_ctor(C,0,{0x3f800000,0,0},0x12348001,0);
  check(ctor_mem.load(C+0x3c,4)==0x12345678 && ctor_mem.load(C+0x38,2)==0x8001,"constructor reads host position and stores low facing");
  check(ctor_mem.load(C+0x1c,4)==0x40a00000 && ctor_mem.load(C+0xdc,4)==0x3e4ccccc,"SmallSpider radius and scale");
  check(ctor_mem.load(C+0x44,4)==0x12345678 && ctor.draws==0,"SmallSpider construction has no model RNG draw");
  check(ctor_mem.load(0x4f85c4,1)==1 && ctor_mem.load(C+0x230,1)==0,"serial advances through borrowed storage");
  ctor.balance_lookup=[](auto &model,auto) {
    model.balance_lookup={};return M::Model::Balance{1,0x3f800000,0x3f000000,0x40000000,0x3f800000,0x3f000000};
  };
  bool factors_seen=false;
  ctor.callbacks["Creature_setStats"]=[&](auto &,const auto &args) {
    factors_seen=std::get<std::uint32_t>(args[3])==0x3f000000 && std::get<std::uint32_t>(args[4])==0x40000000;return 0;
  };
  ctor.badger_ctor(C,0x70000,{0,0,0},0,0,0x101);
  check(factors_seen && !ctor.balance_lookup,"typed balance factors and callback replacement");
  check(ctor_mem.load(C+0x3cc,1)==1 && ctor_mem.load(C+0x1c,4)==0x41c80000,"Badger signed low-byte variant selects snow rules");
  ctor.policy=M::CallbackPolicy::RequireHandlers;
  ctor.callbacks["obj_getPropInt"]=[](auto &,const auto &){return 0;};
  ctor.callbacks["obj_getPropStr"]=[](auto &,const auto &){return 0;};
  refuses<H::MissingHandler>([&]{ctor.gnaw_ctor(C,0x70000,{0,0,0},0,0);},"missing typed balance callback refuses");
  check(ctor.calls.back().name=="balance_lookup" && ctor_mem.load(C+0x34,4)==0x70000,"balance refusal retains named-object prefix");
  auto ctor_writes=ctor.writes.size();
  refuses<M::Unsupported>([&]{ctor.scorpion_ctor(UINT32_MAX-0x3c7u,0,{0,0,0},0,0);},"Scorpion extension checked before base callback");
  refuses<M::Unsupported>([&]{ctor.badger_ctor(UINT32_MAX-0x3cbu,0,{0,0,0},0,0,0);},"Badger variant span checked before base callback");
  check(ctor.writes.size()==ctor_writes,"extension refusal leaves model writes untouched");
  M::Memory recursion;setup(recursion);M::Model recursive(recursion,M::CallbackPolicy::ReferenceZero);
  recursive.callbacks["anim_start"]=[](auto &model,const auto &) -> std::uint64_t {
    model.smallspider_set_state(C,0);return 0;
  };
  refuses<M::Unsupported>([&]{recursive.smallspider_set_state(C,0);},"recursive callback depth refuses");
  recursive.callbacks.erase("anim_start");
  recursive.smallspider_set_state(C,0);
  check(recursion.load(C+0x232,2)==0,"depth restored after callback exception");
}
}
int main() {
  checks_run();std::cout << checks << " monster checks; " << failures << " failures\n";return failures?1:0;
}
