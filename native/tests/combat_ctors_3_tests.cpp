// Authored replay of the synthetic cases in research-combat-ctors-3
// tools/test_ref_weapon_holder.py and tools/test_ref_soul_run.py at 9ff6ae0.
// Each expected result is from those reference cases, never from native output.
#include "champions/combat_ctors.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace A=champions::simulation::rta::arena_monsters;
namespace CT=champions::simulation::rta::combat_ctors;
using champions::simulation::rta::Raw;
namespace {
constexpr A::Address H=0x100000,C=0x110000,S=0x130000,T=0x140000;
int checks=0;
void check(bool yes,const char *label) {
  ++checks;
  if (!yes) throw std::runtime_error(label);
}
void holder_cases(Raw table) {
  A::Memory mem;A::Model m(mem);
  auto put=[&](A::Address a,unsigned n,std::uint64_t v){mem.poke(a,n,v);};
  auto get=[&](A::Address a,unsigned n){return mem.load(a,n);};
  put(H,4,table);put(S+0x60,4,T);
  for (auto i:{12u,13u,14u,25u,26u}) put(0x41b170+4*i,4,S);
  put(0x4f714c,4,0x41200000);put(0x404464,4,0);
  m.callbacks["Skill.v11"] = [](A::Model &,const std::vector<A::Value> &)->A::Value {
    return std::int64_t(100);
  };
  // Fixed setters: the reference case checks the id, shared draw and tier.
  constexpr unsigned slot[6]={2,3,4,5,6,7};
  constexpr Raw ids[6]={0x15,0x1f,0x16,0x17,0x18,0x26};
  for (unsigned i=0;i<6;++i) {
    CT::holder_slot(m,H,slot[i],{std::int64_t(1)});
    check(get(H+0x74,2)==ids[i],"element id from fixed setter");
    check(get(H+0x76,2)==100,"element tier from skill slot 11");
    check(m.draws==i+1,"one shared draw per element setter");
  }
  CT::holder_slot(m,H,8,{std::int64_t(1),std::int64_t(0)});
  check(get(H+0x74,2)==0xffff,"kind zero frees effect slots");
  check(m.draws==6,"kind zero has no shared draw");
  CT::holder_slot(m,H,3,{std::int64_t(-1)});
  check(m.draws==6,"negative hand is excluded by unsigned hand test");
  CT::holder_slot(m,H,1,{std::int64_t(1),std::int64_t(0x60)});
  check(get(H+8,1)==0x60&&(get(H+4,1)&1),"hand one part and carried bit");
  CT::holder_slot(m,H,1,{std::int64_t(2),std::int64_t(0x62)});
  check(get(H+0x98,1)==(table==CT::VT_WEAPONSHIELD?0x62:0),"shield hand two part");
  check(CT::holder_slot(m,H,15,{std::int64_t(1)})==0x60,"slot 15 hand one");
  check(CT::holder_slot(m,H,15,{std::int64_t(2)})==(table==CT::VT_WEAPONSHIELD?0x62:0),
        "slot 15 hand two");
  check(CT::holder_slot(m,H,17,{std::int64_t(1)})==H+0x50,"slot 17 descriptor");
  CT::holder_slot(m,H,13,{std::int64_t(0)});
  check((get(H+4,1)&1)==(table==CT::VT_WEAPONSHIELD?1u:0u),"slot 13 class difference");
  check((get(H+4,1)&2)==0,"slot 13 hides shield hand");
}
void charm_cases() {
  A::Memory mem;A::Model m(mem);
  int cleared=0;
  m.callbacks["handle_clear"]=[&](A::Model &,const std::vector<A::Value> &a)->A::Value {
    if (std::get<std::int64_t>(a.at(0))==C+0x234) ++cleared;
    return std::int64_t(0);
  };
  auto put=[&](A::Address a,unsigned n,std::uint64_t v){mem.poke(a,n,v);};
  auto get=[&](A::Address a,unsigned n){return mem.load(a,n);};
  put(C+0xc,4,0x20);put(C+0x234,4,0x200000);
  CT::creature_set_charm(m,C,0xffffffff,0x123);
  check(get(C+0x310,1)==0x23&&get(C+0x312,2)==0xffff,"charm truncates team and timer");
  check(get(C+0x234,4)==0&&cleared==1,"charm clears the target handle");
  put(C+0x312,2,0);put(C+0xc,4,0x60);
  CT::creature_set_charm(m,C,7,1);
  check(cleared==1&&get(C+0x312,2)==0,"player flag blocks charm");
  put(C+0xc,4,0x20);put(C+0x312,2,0xffff);
  CT::creature_set_charm(m,C,7,1);
  check(cleared==1&&get(C+0x312,2)==0xffff,"permanent charm blocks replacement");
}
void shatter_cases() {
  A::Memory mem;A::Model m(mem);
  constexpr std::uint64_t whole=UINT64_C(0xfe0000000000c0);
  mem.poke(C+0x220,8,whole);
  int pools=0;
  m.callbacks["pool_new"]=[&](A::Model &,const std::vector<A::Value> &a)->A::Value {
    check(std::get<std::int64_t>(a.at(0))==0x248,"shatter prop block size");
    ++pools;return std::int64_t(0);
  };
  CT::skeleton_shatter(m,C,1);
  check(pools==2,"group one has two visible masks");
  check(mem.load(C+0x220,8)==0,"shatter clears visible bits when allocation fails");
  check(mem.load(0x8e44c0,8)==UINT64_C(0xfe000000000000),"group one first mask");
  check(mem.load(0x8e44d0,8)==0,"group one terminator");
  CT::skeleton_shatter(m,C,6);
  check(mem.load(0x8e44c0,8)==0,"group six stores only the terminator");
}
}
int main() {
  try {
    holder_cases(0x50cf40);
    holder_cases(CT::VT_WEAPONSHIELD);
    charm_cases();
    shatter_cases();
    std::cout<<"combat-ctors-3 reference cases: "<<checks<<" checks\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;
  }
}
