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
  spider.poke(T+8*27+4,4,0x267a0c);
  refuses<M::Unsupported>([&]{s.smallspider_on_touch(C,O);},"unported Gnaw virtual body refuses");
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
