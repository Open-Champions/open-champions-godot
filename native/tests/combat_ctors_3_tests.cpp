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
void holder_round4_cases() {
  // Replay tools/test_ref_holder_slots.py: drawing, the effect gates and
  // serialized layouts (research-combat-ctors-4 6baf4cd).
  constexpr A::Address def=0x120000,rec=0x180000,buf=0x170000;
  A::Memory mem;A::Model m(mem);
  auto put=[&](A::Address a,unsigned n,std::uint64_t v){mem.poke(a,n,v);};
  auto get=[&](A::Address a,unsigned n){return mem.load(a,n);};
  put(H,4,0x50cf40);put(H+4,1,1);put(H+8,1,5);
  put(def+8*5+4,4,rec+0xa0);put(def+8*5+0xc,4,rec+0xc0);
  m.callbacks["model_drawPart"] = [](A::Model &,const std::vector<A::Value> &)->A::Value {
    return std::int64_t(0);
  };
  CT::holder_slot(m,H,18,{std::int64_t(def),std::int64_t(0x77),std::int64_t(0x88)});
  check(m.calls.size()==1&&m.calls[0].name=="model_drawPart", "slot 18 draws one part");
  check(std::get<std::int64_t>(m.calls[0].args[0])==rec+0xa0&&
        std::get<std::int64_t>(m.calls[0].args[1])==rec+0xc0,
        "slot 18 reads part and next-row texture");
  m.calls.clear();
  CT::holder_slot(m,H,18,{std::int64_t(def),std::int64_t(0),std::int64_t(0x88)});
  check(m.calls.empty(),"slot 18 requires parent and chain");
  CT::holder_slot(m,H,19,{std::int64_t(0x11),std::int64_t(0x22),
                          std::int64_t(0x33),std::int64_t(0x44)});
  check(m.calls.size()==1&&std::get<std::int64_t>(m.calls[0].args[0])==0x11,
        "slot 19 uses given part");
  m.calls.clear();
  put(H,4,CT::VT_WEAPONSHIELD);put(H+4,1,2);put(H+0x98,1,3);
  put(def+8*3+4,4,rec+0x60);
  CT::holder_slot(m,H,18,{std::int64_t(def),std::int64_t(1),std::int64_t(2)});
  check(m.calls.size()==1&&std::get<std::int64_t>(m.calls[0].args[0])==rec+0x60,
        "shield slot 18 draws second hand");
  m.calls.clear();
  put(H+4,1,1);put(H+8,1,2);put(def+8*2+4,4,rec+0x40);
  put(C+0x10,4,0x41200000);put(C+0x14,4,0x41a00000);put(C+0x18,4,0x41f00000);
  put(C+0x38,2,0xc000);put(C+0xdc,4,0x3fa00000);
  put(C+0x232,2,8);put(C+0x48,4,0x150000);
  m.external.insert("Creature.v16");
  m.callbacks["Creature.v16"] = [](A::Model &,const std::vector<A::Value> &)->A::Value {
    return std::int64_t(0xc00080);
  };
  m.callbacks["isNearView"] = [](A::Model &,const std::vector<A::Value> &)->A::Value {
    return std::int64_t(1);
  };
  m.callbacks["fx_modelStatusEffects"] = [](A::Model &,const std::vector<A::Value> &)->A::Value {
    return std::int64_t(0);
  };
  CT::holder_slot(m,H,21,{std::int64_t(def),std::int64_t(C),std::int64_t(0x3f000000)});
  check(m.calls.size()==3&&m.calls[0].name=="isNearView"&&
        m.calls[1].name=="Creature.v16"&&m.calls[2].name=="fx_modelStatusEffects",
        "slot 21 effect callback order");
  check(std::get<std::int64_t>(m.calls[2].args[2])==H+0x50&&
        std::get<std::int64_t>(m.calls[2].args[3])==rec+0x40&&
        std::get<std::int64_t>(m.calls[2].args[9])==1,
        "slot 21 descriptor model and attacking state");
  m.calls.clear();put(C+0xc,4,2);
  CT::holder_slot(m,H,21,{std::int64_t(def),std::int64_t(C),std::int64_t(0)});
  check(m.calls.empty(),"slot 21 hidden creature gate");put(C+0xc,4,0);
  put(H,4,0x50ce70);put(0x404464,4,0);put(0x4f714c,4,0x41200000);
  for (auto i:{12u,13u,14u,25u,26u}) put(0x41b170+4*i,4,0x130000);
  put(0x130060,4,0x140000);
  m.callbacks["Skill.v11"] = [](A::Model &,const std::vector<A::Value> &)->A::Value {
    return std::int64_t(100);
  };
  constexpr unsigned saved[5]={3,0x52,1,0x54,3};
  for (unsigned k=0;k<5;++k) put(buf+k,1,saved[k]);
  check(CT::holder_slot(m,H,24,{std::int64_t(buf)})==5,"Weapon2 load size");
  check(get(H+0x74,2)==0x15&&get(H+0x104,2)==0x17&&m.draws==2,
        "Weapon2 load rebuilds both elements with two shared draws");
  check(CT::holder_slot(m,H,23,{std::int64_t(buf+8)})==5,"Weapon2 save size");
  for (unsigned k=0;k<5;++k)
    check(get(buf+8+k,1)==saved[k],"Weapon2 round trip byte");
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
void prop_cases() {
  A::Memory mem;A::Model m(mem);
  auto put=[&](A::Address a,unsigned n,std::uint64_t v){mem.poke(a,n,v);};
  auto get=[&](A::Address a,unsigned n){return mem.load(a,n);};
  for (const char *name:{"handle_link","obj_getPropInt","res_find","obj_updateGridCell",
      "obj_runEveryFrame","obj_stopRunning","rigidbody_init","quat_fromMat3",
      "quat_normalize","rigidbody_updateDerived","anim_clear","anim_push","resref_get"})
    m.callbacks[name]=[](A::Model &,const std::vector<A::Value> &)->A::Value {
      return std::int64_t(0);
    };
  m.callbacks["model_getBounds"]=[](A::Model &,const std::vector<A::Value> &)->A::Value {
    return A::Bounds{{0,0,0,0,0,0}};
  };
  constexpr A::Address obj=0x180000;
  auto word=[](const char *s,A::Address a,A::Memory &mem) {
    for (unsigned k=0;;++k) {mem.poke(a+k,1,std::uint8_t(s[k]));if (!s[k]) break;}
  };
  word("fx.lmp",0x190000,mem);word("gutchunks.vif",0x190100,mem);
  word("gutchunks.tex",0x190200,mem);
  put(0x4f86a0,4,0x190000);put(0x4f86f0,4,0x190100);
  put(0x4f86f8,4,0x190200);put(0x404464,4,0x1234);
  CT::gutchunk_ctor(m,obj,{0x3f800000,0x40000000,0x40400000},2,0);
  check(m.draws==1,"gut chunk consumes one shared draw");
  auto seed=Raw((std::uint64_t(0x1234)*0x19660d+0x3c6ef35f)&0xffffffffu)>>16;
  for (int i=0;i<7;++i) seed=Raw(std::uint64_t(seed)*0x19660d+0x3c6ef35f);
  check(get(obj+0x44,4)==seed,"gut chunk consumes seven own draws");
  check(get(obj+0x48,4)==0x50f3b8&&get(obj+0x140,2)==6,"gut chunk table and launch flags");
  check(get(obj+0x23c,4)==0xbdcccccc,"gut chunk damping");
  constexpr A::Address gold=0x181000;
  put(0x44fdcc,4,1234);
  CT::gold_ctor2(m,gold,{0x3f800000,0x40000000,0x40400000},25);
  check(m.draws==2,"gold pile consumes one shared draw");
  check(get(gold+0x48,4)==0x511a88&&get(gold+0x80,4)==25,"gold table and amount");
  check(get(gold+0x4c,1)==0x29&&get(gold+0x50,4)==1234,"gold countdown and frame");
  check((get(gold+0xc,4)&0x220c)==0x220c,"gold object flags");
}
}
int main() {
  try {
    holder_cases(0x50cf40);
    holder_cases(CT::VT_WEAPONSHIELD);
    holder_round4_cases();
    charm_cases();
    shatter_cases();
    prop_cases();
    std::cout<<"combat-ctors-3 reference cases: "<<checks<<" checks\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;
  }
}
