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
void gold_round4_cases() {
  // Replay test_ref_gold.py's whole-life, clip-end, refusal and credit edges.
  constexpr A::Address gold=0x180000,player=0x190000;
  A::Memory mem;A::Model m(mem);
  auto put=[&](A::Address a,unsigned n,std::uint64_t v){mem.poke(a,n,v);};
  auto get=[&](A::Address a,unsigned n){return mem.load(a,n);};
  for (const char *n:{"res_find","sound_playOnObject","anim_advanceUnitSpeed",
                       "obj_stopRunning","obj_delete"})
    m.callbacks[n]=[](A::Model &,const std::vector<A::Value> &)->A::Value {
      return std::int64_t(0);
    };
  put(0x44fdcc,4,1000);put(0x404460,4,0x5678);
  for (unsigned k=0;k<16;++k) put(0x44f140+4*k,4,
      k==0||k==5||k==10?0x3f800000:k==15?0x42c80000:0);
  for (unsigned k=0;k<4;++k) {
    put(0x4f6710+4*k,4,k<2?200:250);
    put(0x4f6730+4*k,4,(k&1)?590:50);
  }
  put(gold+0x4c,1,41);put(gold+0x50,4,1000);put(gold+0x80,4,120);
  put(gold+0x10,4,0x41200000);put(gold+0x14,4,0x41a00000);
  for (unsigned tick=1;tick<=41;++tick) {
    put(0x44fdcc,4,1000+tick);
    CT::gold_run(m,gold);
  }
  check(get(gold+0x4c,1)==0,"gold countdown opens after 41 ticks");
  unsigned clinks=0;
  for (const auto &call:m.calls)
    if (call.name=="res_find"&&std::get<std::int64_t>(call.args[1])==0x532838)
      ++clinks;
  check(clinks==1,"gold clinks at 18 ticks");
  put(player+0xc,4,0x4060);put(player+0x3c8,4,2);
  CT::gold_on_touch(m,gold,player);
  check(get(gold+0x80,4)==0&&get(0x86a900+0x1c,4)==120,
        "touch empties pile and fills first floater");
  check(get(0x86a900+0x20,1)==2&&get(0x86a900+0x18,4)==1041,
        "floater belongs to toucher slot and current frame");
  for (unsigned tick=1;tick<=42;++tick) {
    put(0x44fdcc,4,1041+tick);CT::hud_tick_gold(m);
  }
  check(get(0x4d8af0+2*0x4c30+0xe4,4)==0,"gold waits 43 ticks");
  put(0x44fdcc,4,1084);CT::hud_tick_gold(m);
  check(get(0x4d8af0+2*0x4c30+0xe4,4)==15,"first gold payment is one eighth");
  for (unsigned tick=44;tick<130;++tick) {
    put(0x44fdcc,4,1041+tick);CT::hud_tick_gold(m);
  }
  check(get(0x4d8af0+2*0x4c30+0xe4,4)==120,
        "full amount goes to toucher after pending payments");
  check(get(0x4d8af0+0xe4,4)==0,"other player gets none");
  put(gold+0x80,4,50);put(player+0xc,4,0x20060);
  CT::gold_on_touch(m,gold,player);
  check(get(gold+0x80,4)==50,"prediction copy cannot pick up gold");
  put(player+0xc,4,0x4060);
  for (unsigned k=0;k<8;++k) put(0x86a900+0x24*k+0x1c,4,9);
  put(player+0x3c8,4,3);put(0x4d8af0+3*0x4c30+0xe4,4,99999989);
  CT::gold_on_touch(m,gold,player);
  check(get(0x4d8af0+3*0x4c30+0xe4,4)==99999999,
        "busy floaters credit immediately with cap");
}
void prop_dtor_round4_cases() {
  // Replay tools/test_ref_prop_dtors.py: table write, base call, free bit,
  // Gold's animation release order, and the 1.0-speed wrapper.
  for (Raw fn:{0x3c2fc8u,0x3c4d48u,0x3c2d30u,0x3c2ed8u,0x3c31d8u})
    for (Raw flags:{0u,1u,2u,3u}) {
      A::Memory mem;A::Model m(mem);
      for (const char *n:{"AnimState_dtor","GameObject_dtor","pool_free"})
        m.callbacks[n]=[](A::Model &,const std::vector<A::Value> &)->A::Value {
          return std::int64_t(0);
        };
      CT::prop_dtor(m,fn,C,flags);
      bool gold=fn==0x3c31d8;
      check(mem.load(C+0x48,4)==(gold?0x511a88u:0x511d58u),
            "prop destructor table");
      unsigned n=0;
      if (gold) check(m.calls[n++].name=="AnimState_dtor",
                      "Gold releases clip state first");
      check(m.calls[n++].name=="GameObject_dtor","prop base destructor order");
      if (flags&1) check(m.calls[n++].name=="pool_free","prop free bit");
      check(m.calls.size()==n,"prop destructor call count");
    }
  A::Memory mem;A::Model m(mem);
  m.callbacks["anim_advance"] = [](A::Model &,const std::vector<A::Value> &)->A::Value {
    return std::int64_t(0x81);
  };
  auto answer=CT::anim_advance_unit_speed(m,0x2000,0,0x3000,0x3004);
  check(std::get<std::int64_t>(answer)==0x81,"unit speed returns anim result");
  check(m.calls.size()==1&&m.calls[0].name=="anim_advance"&&
        std::get<std::int64_t>(m.calls[0].args[4])==0x3f800000,
        "unit speed passes exact one and four arguments");
}
void attach_matrix_round4_cases() {
  // Replay tools/test_ref_attach_matrix.py's quantisation, record addressing,
  // model-space scaling and creature world placement.
  constexpr A::Address model=0x100000,pose=0x110000,state=0x120000;
  constexpr A::Address creature=0x130000,definition=0x140000;
  A::Memory mem;A::Model m(mem);
  auto put=[&](A::Address a,unsigned n,std::uint64_t v){mem.poke(a,n,v);};
  put(state,4,pose);put(pose+8,2,1);
  constexpr int rows[3][4]={{4096,2048,0,64},{0,4096,-1024,128},
                            {1,0,4096,-640}};
  for (unsigned k=0;k<3;++k)
    for (unsigned r=0;r<4;++r)
      put(pose+0x10+8*k+2*r,2,Raw(rows[k][r]));
  auto joint=CT::anim_get_joint_matrix(m,state,0);
  check(joint[0]==0x3f800000&&joint[3]==0x3f000000&&
        joint[4]==0x3f800000&&joint[7]==0xbe800000,
        "joint matrix quantisation and row transpose");
  check(joint[9]==0x3f800000&&joint[10]==0x40000000&&joint[11]==0xc1200000,
        "joint position uses one sixty fourth");
  put(model+0x22,1,3);
  auto rec=model+0x30;
  put(rec,4,0);
  constexpr Raw eye[12]={0x3f800000,0,0,0,0x3f800000,0,
                         0,0,0x3f800000,0,0,0};
  for (unsigned k=0;k<12;++k) put(rec+4+4*k,4,eye[k]);
  auto local=CT::model_get_attach_matrix(m,0x40000000,model,0,0);
  check(local[0]==0x40000000&&local[4]==0x40000000&&
        local[8]==0x40000000,"model attach scale without state");
  put(rec+4+9*4,4,0x41200000);
  auto world=CT::model_get_attach_world_matrix(m,0x40000000,model,
        {0x42c80000,0x43480000,0x40a00000},0,0,0);
  check((world[0]&0x7fffffffu)<0x3a83126e&&world[1]==0x3f800000&&
        world[3]==0xbf800000,
        "world attach rotation unscaled and quarter turned");
  using champions::simulation::rta::fabs;
  using champions::simulation::rta::lt;
  using champions::simulation::rta::sub;
  check(lt(fabs(sub(world[9],0x42c80000)),0x3c23d70a)&&
        lt(fabs(sub(world[10],0x435c0000)),0x3c23d70a)&&
        lt(fabs(sub(world[11],0x40a00000)),0x3c23d70a),
        "world attach offset scaled turned and placed");
  put(creature+0x24,4,definition);put(definition+0xc,4,model);
  put(creature+0xdc,4,0x3f000000);
  put(creature+0x10,4,0x3f800000);put(creature+0x14,4,0x40000000);
  put(creature+0x18,4,0x40400000);put(creature+0x38,2,0xc000);
  put(creature+0x114,4,pose);
  auto owned=CT::creature_get_attach_matrix0(m,creature);
  check(owned[9]!=0&&owned[10]!=0,"held Soul owner matrix uses creature fields");
  check(m.draws==0,"attach matrices consume no shared draws");
}
void antwave_round4_cases() {
  // Replay test_ref_antwave.py's run exits, strict hit radius, lifetime and
  // draw period, with the already executed Creature base bodies as recorders.
  constexpr A::Address wave=0x180000,target=0x190000,node=0x1a0000;
  constexpr A::Address wave_table=0x50d298,target_table=0x1b0000;
  A::Memory mem;A::Model m(mem);
  auto put=[&](A::Address a,unsigned n,std::uint64_t v){mem.poke(a,n,v);};
  auto get=[&](A::Address a,unsigned n){return mem.load(a,n);};
  put(wave+0x48,4,wave_table);put(target+0x48,4,target_table);
  put(wave_table+8*1+4,4,0x238750);
  put(target_table+8*2+4,4,0x00dead00);
  put(node+0xc,4,target);
  m.external.insert("Creature.v1");m.external.insert("Creature.v2");
  for (const char *name:{"Creature.v1","Creature.v2","AntWave_Creature_moveTo",
                         "particle_spawn"})
    m.callbacks[name]=[](A::Model &,const std::vector<A::Value> &)->A::Value {
      return std::int64_t(0);
    };
  m.callbacks["AntWave_Creature_run"]=[&](A::Model &a,const std::vector<A::Value> &)->A::Value {
    a.store(wave+0x300,4,Raw(get(wave+0x300,4)+1));return std::int64_t(0);
  };
  put(wave+0x300,4,10);put(wave+0x3cc,4,600);put(wave+0x3d0,4,0);
  CT::antwave_run(m,wave);
  check(m.calls.size()==2&&m.calls[0].name=="AntWave_Creature_run"&&
        m.calls[1].name=="Creature.v1","AntWave lost target destroys before move");
  m.calls.clear();put(wave+0x300,4,59);put(wave+0x3d0,4,node);
  put(wave+0x1c,4,0x41a00000);put(target+0x10,4,0x41200000);
  put(wave+0x10,4,0);put(wave+0x14,4,0);put(wave+0x18,4,0);
  CT::antwave_run(m,wave);
  check(m.calls.size()==3&&m.calls[1].name=="AntWave_Creature_moveTo"&&
        m.calls[2].name=="Creature.v2","AntWave hits at clock 60 inside radius");
  m.calls.clear();put(wave+0x300,4,119);put(target+0x10,4,0x41400000);
  put(target+0x14,4,0x41800000);
  CT::antwave_run(m,wave);
  check(m.calls.size()==2,"AntWave radius comparison is strict");
  m.calls.clear();put(wave+0x300,4,600);put(wave+0x3cc,4,600);
  CT::antwave_run(m,wave);
  check(m.calls.back().name=="Creature.v1","AntWave expires on clock 601");
  m.calls.clear();put(wave+0x300,4,4);put(wave+0x44,4,0x1234);
  CT::antwave_draw(m,wave);
  check(m.calls.empty(),"AntWave draw period gate");
  put(wave+0x300,4,6);
  CT::antwave_draw(m,wave);
  check(m.calls.size()==1&&m.calls[0].name=="particle_spawn"&&
        std::get<std::int64_t>(m.calls[0].args[0])==0x4febe0,
        "AntWave draws its particle from own seed");
  check(m.draws==0,"AntWave draw consumes no shared seed");
}
void phprop_run_round4_cases() {
  // Replay the gravity, life/rest and GutChunk decal cases in
  // research-combat-ctors-4 tools/test_ref_phprop_run.py.
  constexpr A::Address obj=0x180000,block=0x181000;
  A::Memory mem;A::Model m(mem);
  auto put=[&](A::Address a,unsigned n,std::uint64_t v){mem.poke(a,n,v);};
  auto get=[&](A::Address a,unsigned n){return mem.load(a,n);};
  for (const char *name:{"obj_updateGridCell","obj_delete","handle_link",
      "obj_getPropInt","res_find","obj_runEveryFrame","decal_add","sound_playOnObject",
      "GutChunk_dtor","PhProp_dtor"})
    m.callbacks[name]=[](A::Model &,const std::vector<A::Value> &)->A::Value {
      return std::int64_t(0);
    };
  m.callbacks["water_test"]=[](A::Model &,const std::vector<A::Value> &)->A::Value {
    return std::int64_t(0);
  };
  m.callbacks["terrain_heightBelow"]=[](A::Model &,const std::vector<A::Value> &)->A::Value {
    return std::int64_t(0);
  };
  m.callbacks["terrain_height"]=m.callbacks["terrain_heightBelow"];
  m.callbacks["pool_new"]=[&](A::Model &,const std::vector<A::Value> &)->A::Value {
    return std::int64_t(block);
  };
  auto init=[&] {
    for (unsigned i=0;i<0x270;i+=4) put(obj+i,4,0);
    put(obj+0x10,4,0x41200000);put(obj+0x14,4,0x41a00000);
    put(obj+0x18,4,0x42480000);
    put(obj+0x1c,4,0x40800000);
    put(obj+0x48,4,0x511ba8);
    put(obj+0x10e,2,180);
    for (unsigned off:{0x190u,0x194u,0x198u,0x19cu,0x1b8u,
                       0x1d4u,0x1e4u,0x1f4u}) put(obj+off,4,champions::simulation::rta::ONE);
    put(obj+0x1a0,4,0x41200000);put(obj+0x1a4,4,0x41a00000);
    put(obj+0x1a8,4,0x42480000);
    put(obj+0x228,4,0xc2c80000);
    put(obj+0x23c,4,0xbecccccc);put(obj+0x240,4,champions::simulation::rta::ONE);
    const char *names[]={"fx.lmp","gutchunks.vif","gutchunks.tex"};
    const A::Address addresses[]={obj+0x54,obj+0x64,obj+0xa4};
    for (unsigned k=0;k<3;++k)
      for (unsigned j=0;;++j) {
        put(addresses[k]+j,1,std::uint8_t(names[k][j]));
        if (!names[k][j]) break;
      }
    put(0x44fdcc,4,1);put(0x404464,4,0x1234);
    m.calls.clear();m.draws=0;
  };
  init();
  CT::phprop_run(m,obj);
  check(get(obj+0x1c4,4)==0xbe3851e9,"PhProp gravity momentum");
  check(get(obj+0x18,4)==0x42480000,"PhProp position uses old body z");
  check(get(obj+0x10e,2)==179,"PhProp life decrements");
  check(get(obj+0x228,4)==0,"PhProp ground service result");
  check(m.calls.size()==2&&m.calls[0].name=="terrain_heightBelow"&&
        m.calls[1].name=="obj_updateGridCell","PhProp flight call order");
  init();put(obj+0x10e,2,0);
  CT::phprop_run(m,obj);
  check(get(block+0x48,4)==0x511d58,"PhProp rest builds named Prop");
  check(m.draws==1,"PhProp rest consumes one shared draw");
  check(get(obj+0x10e,2)==0xffff,"PhProp rest decrements life first");
  check(m.calls.back().name=="obj_delete","PhProp rest deletes flying prop");
  init();put(obj+0x248,4,1);put(obj+0x24c,4,0xbf000000);
  put(obj+0x1c4,4,0xbf8ccccd);put(obj+0x48,4,0x50f3b8);
  CT::gutchunk_run(m,obj);
  auto decals=0;
  for (const auto &c:m.calls) if (c.name=="decal_add") ++decals;
  check(decals==1,"GutChunk downward threshold decal");
  check(m.draws==0,"GutChunk decal uses own seed only");
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
    gold_round4_cases();
    prop_dtor_round4_cases();
    attach_matrix_round4_cases();
    antwave_round4_cases();
    phprop_run_round4_cases();
    std::cout<<"combat-ctors-3 reference cases: "<<checks<<" checks\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;
  }
}
