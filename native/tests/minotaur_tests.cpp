#include "champions/minotaur.hpp"
#include "champions/combat_ctors.hpp"
#include "champions/combat_stats.hpp"
#include "champions/campaign_actors.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace R=champions::simulation::rta;
namespace M=R::minotaur;
namespace A=R::arena_monsters;
using R::Raw;
namespace {
std::size_t checks=0,cases=0;
void check(bool ok,const char *why) {
  ++checks;
  if (!ok) throw std::runtime_error(why);
}
Raw num(const A::Value &v) { return Raw(std::get<std::int64_t>(v)); }
Raw next(Raw x) { return x*0x19660du+0x3c6ef35fu; }
constexpr Raw C=0x100000, TARGET=0x110000, HANDLE=0x120000;
struct Rig {
  A::Memory mem;
  A::Model model{mem};
  Raw events=0,visible=0,away=0,stopped=0,active=1,attacking=0,charging=0;
  Raw moving=0,overlay=0;
  std::function<bool(const std::vector<A::Value>&)> sight;
  std::vector<Raw> transitions;
  std::vector<std::pair<Raw,Raw>> parts;
  Rig() {
    for (const char *name:{"anim_start","anim_startBlendInOut","anim_syncToSimFrame",
        "anim_clear","creature_setDefEntryParams","Creature_init","obj_runEvery60",
        "Creature_processDamage","Creature_applyKnockback","Creature_turnToTarget",
        "Creature_settle","Creature_moveBy","Creature_moveToTarget","Creature_moveToDest",
        "Creature_fleeFromTarget","Creature_hitTargetInArc","Creature_deadUpdate","Creature_draw",
        "handle_clear","terrain_heightBelow","Creature_dropLoot","Holder.v21","Holder.v18"})
      model.callbacks[name]=[](auto &,const auto &)->A::Value { return std::int64_t(0); };
    for (const char *name:{"Creature.v16","Creature.v23","Creature.v25","Creature.v27","Creature.v28"})
      model.external.insert(name);
    model.callbacks["Creature.v16"]=[](auto &,const auto &)->A::Value { return std::int64_t(0x730000); };
    model.callbacks["Creature.v23"]=[](auto &,const auto &)->A::Value { return std::int64_t(0); };
    model.callbacks["Creature.v25"]=[this](auto &,const auto &)->A::Value { return std::int64_t(visible); };
    model.callbacks["Creature.v27"]=[this](auto &,const auto &a)->A::Value {
      transitions.push_back(num(a.at(1))); return std::int64_t(0);
    };
    model.callbacks["Creature.v28"]=[](auto &,const auto &)->A::Value { return std::int64_t(0); };
    model.callbacks["Creature_run"]=[this](auto &,const auto &)->A::Value { return std::int64_t(stopped); };
    model.callbacks["Creature_updateActive"]=[this](auto &,const auto &)->A::Value { return std::int64_t(active); };
    model.callbacks["Creature_stepAnim"]=[this](auto &,const auto &)->A::Value { return std::int64_t(events); };
    model.callbacks["Creature_isAwayFromHome"]=[this](auto &,const auto &)->A::Value { return std::int64_t(away); };
    model.callbacks["Creature_isTargetCharging"]=[this](auto &,const auto &)->A::Value { return std::int64_t(charging); };
    model.callbacks["Creature_isTargetAttackingMe"]=[this](auto &,const auto &)->A::Value { return std::int64_t(attacking); };
    model.callbacks["Creature_isNotRooted"]=[](auto &,const auto &)->A::Value { return std::int64_t(0); };
    model.callbacks["Creature_isAttackReady"]=[](auto &,const auto &)->A::Value { return std::int64_t(0); };
    model.callbacks["Creature_isNotStunned"]=[](auto &,const auto &)->A::Value { return std::int64_t(0); };
    model.callbacks["Creature_canSeeTargetIn"]=[this](auto &,const auto &a)->A::Value {
      return std::int64_t(sight && sight(a));
    };
    model.callbacks["anim_findTrack"]=[this](auto &,const auto &a)->A::Value {
      return std::int64_t(num(a.at(1))==clip(16)?moving:overlay);
    };
    model.callbacks["model_buildHideMask"]=[](auto &,const auto &)->A::Value {
      return std::int64_t(UINT64_C(0x123456789abcdef0)); // authored result
    };
    for (unsigned slot:{1u,11u,14u,17u}) {
      model.callbacks["Holder.v"+std::to_string(slot)]=[this,slot](auto &m,const auto &a)->A::Value {
        std::vector<A::Value> rest(a.begin()+1,a.end());
        return std::int64_t(R::combat_ctors::holder_slot(m,num(a.at(0)),slot,rest,
          [this](Raw part,A::Address c,A::Address def,Raw flags) {
            check(c==C && def==0 && flags==0,"part-drop arguments");
            parts.emplace_back(part,Raw(mem.load(C+0x232,2)));
          }));
      };
    }
    reset();
  }
  static Raw clip(unsigned i) { return 0x700000+0x100*i; }
  void w(Raw off,unsigned n,std::uint64_t value) { mem.store(C+off,n,value); }
  Raw u(Raw off,unsigned n=4) { return Raw(mem.load(C+off,n)); }
  void reset(Raw state=0,bool target=true,bool holder=true,Raw seed=20973) {
    for (unsigned off=0;off<M::ALLOCATION;++off) mem.store(C+off,1,0);
    w(0x24,4,M::DEFINITION);w(0x48,4,M::VTABLE);
    w(0x50,4,holder?C+0x3c8:0);w(0x3c8,4,0x50cda0);w(0x3cc,1,3);
    w(0x3d0,1,21);w(0x460,1,25);
    w(0x44,4,seed);w(0x228,4,R::from_int(90));w(0x22c,4,R::from_int(100));
    w(0x234,4,target?HANDLE:0);mem.store(HANDLE+12,4,TARGET);
    w(0x300,4,1000);w(0x232,2,state);w(0x3ae,2,500);
    mem.store(0x404464,4,0x12345678);mem.store(0x44fdcc,4,0x7ffffffe);
    for (unsigned i=0;i<26;++i) mem.store(M::DEFINITION+12+8*i,4,clip(i));
    events=visible=away=stopped=attacking=charging=moving=overlay=0;active=1;sight={};
    clear();
  }
  void clear() { mem.clear_log();model.calls.clear();model.writes.clear();model.events.clear();transitions.clear();parts.clear(); }
  std::vector<A::Call> calls(const std::string &name) const {
    std::vector<A::Call> result;
    for (const auto &c:model.calls) if (c.name==name) result.push_back(c);
    return result;
  }
  void tick() { M::run(model,C);check(mem.load(0x404464,4)==0x12345678,"class preserves shared seed");++cases; }
};
void constructors(Raw seed) {
  Rig r;
  for (unsigned i=0;i<100;++i) {
    seed=next(seed);r.reset(0,false,true,seed);
    const bool boss=i%2==0;
    r.w(0xa8,8,UINT64_C(0x1000000000080));
    M::finish_constructor(r.model,C,boss?"mInOtAuRbOsS":"Minotaur");
    const auto own=boss?seed:next(seed);
    const auto variant=boss?4u:1u<<((own>>16)&1);
    check(r.u(0x44)==own,"constructor own draw count");
    check(r.u(0x48)==M::VTABLE && r.u(0x50)==C+0x3c8 && r.u(0x3c8)==0x50cda0,"embedded initialized holder");
    check(r.u(0x3cc,1)==3 && r.u(0x3d0,1)==21 && r.u(0x460,1)==(boss?23u:25u),"definition hand entries");
    check(r.u(0x1c)==0x41c80000 && r.u(0xb0)==0x3f999999 && r.u(0xb4,1)==11 && !r.u(0xb8),"class numeric stores");
    check(r.u(0xec)==0x7ffffffe && r.u(0xf4)==0x8000012a && !r.u(0x46c),"frame stores and marker");
    check(r.u(0xe8)==variant && r.mem.load(C+0x220,8)==UINT64_C(0x123456789abcdef0),"variant mask stored in shared bytes");
    check(r.mem.load(C+0xa8,8)==UINT64_C(0x1000200040080),"base flags preserved and block cleared");
    for (unsigned k=0;k<4;++k) {
      check(r.mem.load(C+0x43c+8*k,2)==0xffff,"holder effect sentinels");
      check(!r.mem.load(C+0x428+2*k,2)&&!r.mem.load(C+0x430+2*k,2),"holder descriptor fields reset");
    }
    const auto registered=r.calls("creature_setDefEntryParams");
    check(registered.size()==8,"eight event registrations");
    for (unsigned k=0;k<8;++k) {
      const auto &a=registered[k].args;const auto &e=M::EVENTS[k];
      check(num(a[1])==e.entry,"event API is one based");
      for (unsigned j=0;j<4;++j) check(num(a[2+j])==e.masks[j]&&num(a[6+j])==e.times[j],"exact event slots");
    }
    check(r.model.calls.back().name=="Creature_init","init is last constructor service");
    ++cases;
  }
}
void states(Raw seed) {
  Rig r;
  for (Raw state: {0u,1u,2u,3u,4u,5u,6u,7u,8u,9u,10u,11u,12u,13u,14u,15u,16u,17u,18u,19u,20u,21u,22u,23u,24u,25u,UINT32_MAX,65536u})
    for (Raw hp:{20u,50u,90u}) for (Raw marker:{0u,1u}) {
      seed=next(seed);r.reset(5,true,true,seed);r.w(0x228,4,R::from_int(hp));r.w(0x46c,4,marker);r.w(0xa8,8,0x200);
      M::set_state(r.model,C,state);
      const bool valid=state==0||state==1||state==3||state==5||(state>=6&&state<=12)||state==16||state==18||state==19||state==20||state==23;
      check(r.u(0x232,2)==(valid?state:0),"stored state switch default");
      check(r.mem.load(C+0xa8,8)==(state==23?0x200u:0u),"blocking flag lifecycle");
      auto start=r.calls("anim_start"),blend=r.calls("anim_startBlendInOut");
      int index=-1;Raw flags=0,time=0x3dcccccc;
      switch(state) {
      case 0:case 19:index=14;flags=3;if(state==19)time=0x3e999999;break;
      case 1:index=19;flags=0x803;break;
      case 3:index=hp==50?19:16;flags=0x803;break;
      case 5:index=15;flags=3;break;
      case 12:case 16:index=16;flags=state==12?0x80b:0x803;break;
      case 20:index=10+((next(seed)>>16)&1);flags=0x806;time=0x3e4ccccc;break;
      case 23:index=9;flags=3;break;
      default:break;
      }
      check(start.size()==(index<0?0u:1u),"change call count");
      if(index>=0)check(num(start[0].args[1])==Rig::clip(unsigned(index))&&num(start[0].args[2])==flags&&num(start[0].args[3])==time,"state animation binding");
      int overlay_index=-1;Raw overlay_flags=0;
      if(state>=6&&state<=10){overlay_index=int(state-2);overlay_flags=state>=9?0x4008:0x808;}
      if(state==11){overlay_index=3;overlay_flags=0x4808;}
      if(state==12&&!marker){overlay_index=18;overlay_flags=8;}
      if(state==18){overlay_index=13;overlay_flags=0x810;}
      check(blend.size()==(overlay_index<0?0u:1u),"blend call count");
      if(overlay_index>=0)check(num(blend[0].args[1])==Rig::clip(unsigned(overlay_index))&&num(blend[0].args[2])==overlay_flags&&num(blend[0].args[3])==0x3dcccccc&&num(blend[0].args[4])==0x3dcccccc,"both exact blend arguments");
      if(state==12)check(r.u(0x46c)==1,"once-only charge overlay marker");
      if(state==16)check(r.u(0x318,2)==60,"flee cooldown");
      if(state==23)check(r.u(0xf4)==1000+(next(seed)>>16)%120+60,"guard deadline");
      if(state==20) {
        check(r.parts==std::vector<std::pair<Raw,Raw>>{{21,5},{25,5}},"ordered held drops observe old state");
        const auto loot=r.calls("Creature_dropLoot");
        check(loot.size()==1,"one class loot call");
        const Raw words[]={0x42100000,0x40000000,0x3fcccccc,0x3f199999,0x3fb33333,0x3e800000};
        for(unsigned k=0;k<6;++k)check(num(loot[0].args[4+k])==words[k],"six raw loot arguments, including Gold chance");
        check(r.calls("Creature.v28").size()==1,"onDeath forwarded once");
      }
      ++cases;
    }
}
void attacks() {
  Rig r;
  for (Raw state=6;state<=11;++state) for(Raw event:{0u,1u,4u,5u})
    for(Raw flags:{0u,8u,16u,24u}) {
      r.reset(state);r.events=event;r.w(0x11a,2,flags);r.w(0x38,2,0xf123);
      r.w(0xd0,2,0xfff9);r.w(0xd2,2,117);r.w(0xd4,2,311);
      r.tick();
      const auto hits=r.calls("Creature_hitTargetInArc");
      check(hits.size()==((event&1)?1u:0u),"event bit1 exclusively gates attacks");
      if (!hits.empty()) {
        const auto &a=hits[0].args;
        const Raw want[]={C,Raw(-3805),8000,Raw(-7),117,311,
                          state==6?0x440001u:state<9?0x440008u:8u,
                          state==6?0u:C+0x418,0,0x42100000};
        for(unsigned k=0;k<10;++k)check(num(a[k])==want[k],"signed attack arguments and holder descriptor");
      }
      check(r.transitions==((flags&8)?std::vector<Raw>{}:std::vector<Raw>{5}),"attack completion gate");
      check(r.calls(state<9?"Creature_settle":"Creature_moveBy").size()==1,"attack movement mode");
      check(r.calls("Creature_turnToTarget").size()==(state<9?1u:0u),"side/back do not turn");
    }
}
void charge(Raw seed) {
  Rig r;
  for(Raw moving:{0u,1u}) for(Raw overlay:{0u,1u}) for(Raw close:{0u,1u})
    for(Raw far:{0u,1u}) for(Raw target:{0u,1u}) for(Raw event:{0u,1u})
      for(Raw flags:{0u,8u,16u,24u}) {
        seed=next(seed);r.reset(12,target!=0,true,seed);r.moving=moving;r.overlay=overlay;
        r.events=event;r.w(0x11a,2,flags);r.w(0x468,4,777);
        r.sight=[close,far](const auto &a){return num(a[5])==0x42100000?close:far;};
        r.tick();
        const bool start=moving&&!overlay&&close;
        check(r.calls("anim_findTrack").size()==(moving?2u:1u),"charge track query short circuit");
        check(r.calls("anim_start").size()==unsigned(start)&&r.calls("anim_startBlendInOut").size()==unsigned(start),"charge overlay conditions");
        if(start) {
          auto change=r.calls("anim_start")[0],blend=r.calls("anim_startBlendInOut")[0];
          check(num(change.args[1])==Rig::clip(15)&&num(change.args[2])==0x803,"charge base change");
          check(num(blend.args[1])==Rig::clip(12)&&num(blend.args[2])==0x818,"charge strike overlay");
        }
        const bool continues=(flags&8)&&((flags&16)||far);
        check(r.transitions==(continues?std::vector<Raw>{}:std::vector<Raw>{3}),"charge exit flags/range");
        check(r.u(0x468)==(continues?777u:1000+(next(seed)>>16)%180+300),"charge own deadline");
        check(r.u(0x44)==(continues?seed:next(seed)),"charge exact own draw count");
        const auto hits=r.calls("Creature_hitTargetInArc");
        check(hits.size()==unsigned(target&&event),"charge target and event gate");
        if(!hits.empty())check(num(hits[0].args[6])==0x100041&&!num(hits[0].args[7]),"charge flags and null descriptor");
      }
}
void decisions() {
  Rig r;
  for(Raw visible:{0u,1u}) for(Raw away:{0u,1u}) {
    r.reset(0);r.visible=visible;r.away=away;r.w(0x300,4,100000);r.tick();
    check(r.transitions==((visible||away)?std::vector<Raw>{1}:std::vector<Raw>{}),"idle has no boar timeout");
    check(r.calls("Creature_isAwayFromHome").size()==unsigned(!visible),"idle query ordering");
  }
  for(Raw state:{3u,5u}) for(bool holder:{false,true}) for(Raw hand:{0u,1u})
    for(Raw deadline:{999u,1000u,1001u}) {
      r.reset(state,true,holder);r.w(0x3cc,1,hand);r.w(0x468,4,deadline);
      const Raw near=state==3?0x42700000:0x43960000,far=state==3?0x43960000:0x42700000;
      r.sight=[=](const auto &a){return num(a[4])==near&&num(a[5])==far;};
      r.tick();const bool eligible=!(holder&&hand)&&deadline<1000;
      check(r.transitions==(eligible?std::vector<Raw>{12}:std::vector<Raw>{}),"charge entry reversed ranges, strict cooldown and holder");
    }
  const auto lower=R::mul(R::from_int(100),0x3ea8f5c2),upper=R::mul(R::from_int(100),0x3f28f5c2);
  for(Raw hp:{lower-1,lower,lower+1,upper-1,upper,upper+1})
    for(Raw attacking:{0u,1u}) for(Raw shield:{0u,1u}) for(Raw residue:{0u,1u,2u}) {
      Raw seed=7;while((next(seed)>>16)%3!=residue)++seed;
      r.reset(5,true,true,seed);r.w(0x228,4,hp);r.w(0x3cc,1,1|(shield<<1));r.attacking=attacking;
      r.sight=[](const auto &a){return num(a[4])==0&&num(a[5])==0x42100000;};
      r.tick();const bool mid=hp>lower&&hp<upper;
      const Raw want=mid?(attacking&&residue&&shield?23:6):7+((next(seed)>>16)&1);
      check(r.transitions==std::vector<Raw>{want},"strict mid-HP guard decisions");
      check(r.u(0x44)==(mid&&!attacking?seed:next(seed)),"guard own draw short circuit");
    }
  for(const auto &edge:std::vector<std::array<Raw,3>>{{100,100,0},{100,101,1},{UINT32_MAX,0,1},{0,0x80000000,0}}) {
    r.reset(23);r.w(0xf4,4,edge[0]);r.w(0x300,4,edge[1]);r.tick();
    check(r.transitions==(edge[2]?std::vector<Raw>{3}:std::vector<Raw>{}),"signed guard timer boundary");
  }
}
void filter_and_order() {
  Rig r;
  const std::array<Raw,7> rows[]={
    {3,0,0,0,1,0,3},{3,1,0,0,1,0,5},{3,1,0,0,0,0,0},
    {3,0,1,0,1,0,19},{3,0,0,1,1,0,16},{3,0,0,1,0,0,3},
    {3,1,1,1,1,0,5},{6,1,1,0,1,0,19},{20,1,1,1,1,0,20},
    {0,0,0,0,1,0,0},{1,0,0,0,1,1,5}};
  for(const auto &a:rows) {
    r.reset(5,a[4]!=0);r.w(0x314,2,a[1]);r.w(0x31c,2,a[2]);r.w(0x318,2,a[3]);r.w(0x231,1,a[5]);r.w(0xa8,8,0x200);
    M::set_state(r.model,C,a[0]);check(r.u(0x232,2)==a[6],"shared filter precedence/refusal");
    check(r.mem.load(C+0xa8,8)==(a[5]?0x200u:0u),"filter before class flag clear");++cases;
  }
  for(bool holder:{false,true}) for(Raw stopped:{0u,1u}) for(Raw active:{0u,1u}) {
    r.reset(23,true,holder);r.stopped=stopped;r.active=active;r.w(0xf4,4,2000);r.tick();
    std::vector<std::string> want={"Creature.v16","Creature_processDamage","Creature.v16","Creature_run"};
    if(!stopped){want.push_back("Creature_updateActive");if(active){if(holder)want.push_back("Holder.v21");for(const char *n:{"Creature_stepAnim","Creature_applyKnockback","Creature.v23"})want.push_back(n);}}
    std::vector<std::string> got;for(const auto &c:r.model.calls)got.push_back(c.name);
    check(got==want,"ordered common tick and early exits");
  }
  for(Raw hp:{R::mul(R::from_int(100),0x3ea8f5c2)-1,R::mul(R::from_int(100),0x3ea8f5c2),R::mul(R::from_int(100),0x3ea8f5c2)+1}) {
    r.reset(5);r.w(0x228,4,hp);M::set_state(r.model,C,18);
    check(r.parts.size()==(hp<R::mul(R::from_int(100),0x3ea8f5c2)?2u:0u),"strict reaction part-loss threshold");++cases;
  }
  for(Raw event:{0u,1u,0x135u}) {
    r.reset(20);r.events=event;r.tick();const auto calls=r.calls("Creature_deadUpdate");
    check(calls.size()==1&&num(calls[0].args[1])==event&&num(calls[0].args[3])==0&&num(calls[0].args[4])==0x41a00000,"corpse forwards events and height");
  }
  for(bool holder:{false,true}) {
    r.reset(0,true,holder);M::draw(r.model,C);
    std::vector<std::string> names;for(const auto &c:r.model.calls)names.push_back(c.name);
    auto want=std::vector<std::string>{"Creature.v16","Creature_draw","Creature.v16"};if(holder)want.push_back("Holder.v18");
    check(names==want,"draw queries model twice and forwards holder");++cases;
  }
}
void masks() {
  std::vector<std::uint8_t> bytes(0x80);
  auto put=[&](unsigned off,unsigned n,std::uint64_t v){for(unsigned k=0;k<n;++k)bytes.at(off+k)=std::uint8_t(v>>(8*k));};
  put(8,8,0x8000000000000003);put(0x10,2,2);put(0x21,1,4);
  put(0x40,8,1);put(0x48,8,8);put(0x50,8,2);put(0x58,8,16);
  check(M::hide_mask(bytes.data(),bytes.size(),0)==UINT64_C(0x7ffffffffffffffc),"default 64-bit mask");
  check(M::hide_mask(bytes.data(),bytes.size(),1)==UINT64_C(0x7ffffffffffffff5),"selector one");
  check(M::hide_mask(bytes.data(),bytes.size(),3)==UINT64_C(0x7fffffffffffffe7),"ordered multi-bit selectors");
  bool refused=false;try{M::hide_mask(bytes.data(),0x50,2);}catch(const A::Unsupported &){refused=true;}
  check(refused,"truncated selector refuses");
  std::vector<std::uint8_t> balance(14);balance[0]=1;balance[2]=0x70;balance[3]=0xec;
  for(unsigned k=0;k<5;++k){balance[4+2*k]=0xff;balance[5+2*k]=0xff;}
  auto factors=R::campaign_actors::balance_lookup(balance.data(),balance.size(),"Minotaur");
  check(factors[0]==R::combat::fmul(R::combat::itof(65535),0x3c23d70a),"balance percentages are unsigned");
}
void stats() {
  Raw level,difficulty,seed,draw,end,hp,maxhp,xp;int armor,low,high,attack;
  R::combat::Tables tables;tables.fixed_levels={U"stomedal",U"innpit"};
  const auto pct=std::array<Raw,5>{R::combat::fmul(R::combat::itof(600),0x3c23d70a),R::combat::fmul(R::combat::itof(200),0x3c23d70a),R::combat::fmul(R::combat::itof(250),0x3c23d70a),R::combat::fmul(R::combat::itof(100),0x3c23d70a),R::combat::fmul(R::combat::itof(250),0x3c23d70a)};
  while(std::cin>>level>>difficulty>>seed>>draw>>end>>hp>>maxhp>>armor>>low>>high>>attack>>xp) {
    A::Memory memory;A::Model model(memory);memory.store(C+0x44,4,seed);
    const auto own=model.own_draw(C);
    check(own==draw&&memory.load(C+0x44,4)==end,"stats own RNG vector");
    R::combat::World world;world.monster_level=level;world.difficulty=difficulty;world.num_players=1;world.level_name=U"valor";
    const auto actual=R::combat::monster_stats(tables,world,pct,0,U"Minotaur",true,own);
    check(actual.hp==hp&&actual.max_hp==maxhp&&actual.armor==armor&&actual.dmg_low==low&&actual.dmg_high==high&&actual.attack==attack&&actual.exp_value==xp,"pinned monster stats exact vector");++cases;
  }
  check(cases==8250,"both seeds, 165 levels, five difficulties, five HP rolls");
}
} // namespace
int main(int argc,char **argv) {
  try {
    if(argc>1&&std::string(argv[1])=="--stats")stats();
    else{for(Raw seed:{20973u,7u}){constructors(seed);states(seed);attacks();charge(seed);decisions();filter_and_order();}masks();}
    std::cout<<"Minotaur: "<<cases<<" synthetic cases, "<<checks<<" checks, zero failures\n";
    return 0;
  } catch(const std::exception &e){std::cerr<<"Minotaur: "<<e.what()<<" (after "<<checks<<" checks)\n";return 1;}
}
