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
  M::Memory attacks;setup(attacks);M::Model attack(attacks,M::CallbackPolicy::ReferenceZero);
  attacks.poke(C+0x232,2,7);attacks.poke(C+0x234,4,B);attacks.poke(B+12,4,O);
  attacks.poke(C+0xd0,2,0xfff9);attacks.poke(C+0xd2,2,0x8000);attacks.poke(0x4f719c,4,2);
  attacks.poke(C+0xf4,4,3);
  bool poison=false;
  attack.callbacks["Creature_hitTargetAtReach"]=[&](auto &,const auto &args) {
    const auto &d=std::get<M::Descriptor>(args[5]);
    poison=d.effect==std::array<champions::simulation::rta::Raw,4>{0x17,0xffff,0xe000,0};return 0;
  };
  attack.scorpion_attack(C,1);
  check(poison && attacks.load(C+0xd0,2)==0xffff && attacks.load(C+0xd2,2)==0xe000,"poison divides signed damage toward zero and retains unsigned descriptor slots");
  check(attacks.load(C+0xf4,4)==2,"stopped Scorpion attack counts down once");
  attacks.poke(C+0x234,4,0);auto call_total=attack.calls.size();
  check(attack.see(C,7000,0x41c80000)==0 && attack.calls.size()==call_total,"sight without target makes no service call");
  attack.policy=M::CallbackPolicy::RequireHandlers;
  attacks.poke(C+0x231,1,0);attacks.poke(C+0xa8,8,0x100000);attacks.poke(C+0xbc,4,0);
  refuses<H::MissingHandler>([&]{attack.badger_attack(C,1);},"Badger settling requires structured collision service");
  check(attacks.load(C+0xc,4)&0x8000000,"failed settling retains the object flag prefix");
  check(attack.calls.back().name=="world_collectObjectsInBox" && attack.writes.back().address==C+0xa8,"inherited writes appear before missing collision callback");
  attack.structured_callbacks["world_collectObjectsInBox"]=[](auto &model,const auto &) -> M::Structured {
    model.mem.store(C+0x3c8,4,99);throw M::Error("collision failure");
  };
  refuses<M::Error>([&]{attack.badger_attack(C,1);},"structured collision failure propagates");
  check(attacks.load(C+0x3c8,4)==99,"collision callback mutation remains before hit latch");
  attacks.poke(C+0x231,1,8);attacks.poke(C+0x232,2,8);attacks.poke(C+0x11a,2,8);
  attack.callbacks["Creature_hitAllEnemiesInArc"]=[](auto &model,const auto &) -> std::uint64_t {
    model.callbacks.erase("Creature_hitAllEnemiesInArc");throw M::Error("hit failure");
  };
  refuses<M::Error>([&]{attack.badger_attack(C,1);},"hit callback safely removes itself");
  check(attacks.load(C+0x3c8,4)==99 && !(attacks.load(C+0xc,4)&0x8000000),"hit failure occurs after settling and before latch store");
  attack.policy=M::CallbackPolicy::ReferenceZero;
  attacks.poke(C+0x234,4,B);attacks.poke(C+0x232,2,6);attacks.poke(C+0x11a,2,8);attacks.poke(C+0xf4,4,2);
  attacks.poke(T+8*26+4,4,0x10bb08);attacks.poke(T+8*26,2,0xfffe);
  attack.external.insert("Creature.v26");bool adjusted=false;
  attack.callbacks["Creature.v26"]=[&](auto &model,const auto &args) {
    adjusted=std::get<std::uint32_t>(args[0])==C-2;
    model.mem.store(C+0x11a,2,0);return 0;
  };
  attack.scorpion_attack(C,0);
  check(adjusted && attacks.load(C+0xf4,4)==1,"external base turn uses signed adjustment and flags are reread after callback");
  M::Memory chase_mem;setup(chase_mem);M::Model chase(chase_mem);
  chase_mem.poke(C+0x234,4,B);chase_mem.poke(B+12,4,O);
  chase_mem.poke(T+8*27+4,4,0x3c2688);
  refuses<H::MissingHandler>([&]{chase.badger_chase(C);},"chase requires sight callback before decisions");
  check(chase.writes.empty() && chase.calls.back().name=="creature_canSeeObj","missing sight has no invented chase movement");
  unsigned sight_calls=0;
  chase.callbacks["creature_canSeeObj"]=[&](auto &,const auto &) {++sight_calls;return 1;};
  chase.badger_chase(C);
  check(sight_calls==1 && chase_mem.load(C+0x232,2)==6,"first chase match picks front attack and stops sight tests");
  check(chase_mem.load(C+0x44,4)==0x75432777,"first chase match takes exactly one own draw");
  sight_calls=0;auto chase_seed=chase_mem.load(C+0x44,4);
  chase.callbacks["creature_canSeeObj"]=[&](auto &,const auto &) {return ++sight_calls==2;};
  chase.badger_chase(C);
  check(sight_calls==2 && chase_mem.load(C+0x232,2)==10 && chase_mem.load(C+0x44,4)==chase_seed,"long range chase takes no own draw");
  sight_calls=0;bool side_face=false;
  chase.callbacks["creature_canSeeObj"]=[&](auto &model,const auto &args) {
    ++sight_calls;if (sight_calls==2) model.mem.store(C+0x38,2,0x7fff);
    if (sight_calls==3) side_face=std::get<std::uint32_t>(args[2])==0xffff9f3f;
    return sight_calls==3;
  };
  chase.badger_chase(C);
  check(side_face && chase_mem.load(C+0x232,2)==8,"side facing is reread and signed16 wrapped after sight callback");
  chase.callbacks["creature_canSeeObj"]=[](auto &,const auto &){return 0;};
  refuses<H::MissingHandler>([&]{chase.badger_chase(C);},"chase cone search needs supplied enemy service");
  check(chase.calls.back().name=="creature_findEnemyInCone","cone refusal follows four target tests");
  chase.callbacks["creature_findEnemyInCone"]=[](auto &model,const auto &) -> std::uint64_t {
    model.mem.store(C+0x234,4,0);return 0;
  };
  chase.badger_chase(C);
  check(chase_mem.load(C+0x232,2)==0,"target removed by cone service enters inherited no-target idle");
  chase.external.insert("Creature.v27");
  chase.callbacks["Creature.v27"]=[](auto &,const auto &){return UINT64_C(0xfedcba9876543210);};
  const auto external_result=chase.badger_chase(C);
  check(external_result && *external_result==UINT64_C(0xfedcba9876543210),"chase forwards external transition return");
  M::Memory nav_mem;setup(nav_mem);M::Model nav(nav_mem,M::CallbackPolicy::ReferenceZero);
  nav_mem.poke(C+0x234,4,B);nav_mem.poke(B+12,4,O);nav_mem.poke(O+0x10,4,0x43480000);
  nav_mem.poke(C+0x231,1,8);nav_mem.poke(C+0x300,4,301);nav_mem.poke(C+0x2d8,4,301);
  nav_mem.poke(C+0x2d4,4,0);nav_mem.poke(T+8*26+4,4,0x1234);
  nav.structured_callbacks["world_findFreeSpot"]=[](auto &model,const auto &) -> M::Structured {
    model.mem.store(C+0x3c8,4,77);throw M::Error("free spot failure");
  };
  refuses<M::Error>([&]{nav.badger_chase(C);},"free spot failure keeps navigation prefix");
  check(nav_mem.load(C+0x3c8,4)==77 && nav.calls.back().name=="world_findFreeSpot","free spot mutation survives before teleport writes");
  nav.structured_callbacks["world_findFreeSpot"]=[](auto &,const auto &) {
    return M::Structured::tuple({M::Structured::vector({0x42c80000,0x42480000,0}),M::Structured::integer(0x100)});
  };
  nav.badger_chase(C);
  check(nav_mem.load(C+0x10,4)==0,"free spot found uses low signed byte rather than full integer");
  nav.structured_callbacks["world_findFreeSpot"]=[](auto &model,const auto &) {
    model.structured_callbacks.erase("world_findFreeSpot");
    return M::Structured::tuple({M::Structured::vector({0x42c80000,0x42480000,0}),M::Structured::integer(0x80)});
  };
  nav.badger_chase(C);
  check(nav_mem.load(C+0x10,4)==0x42c80000 && nav.calls.back().name=="obj_updateGridCell","negative found byte teleports and updates grid after safe callback removal");
  nav_mem.poke(C+0x10,4,0);nav_mem.poke(C+0x14,4,0);
  nav.structured_callbacks["world_findFreeSpot"]=[](auto &,const auto &){return M::Structured::integer(1);};
  refuses<M::Unsupported>([&]{nav.badger_chase(C);},"free spot refuses malformed scalar result");
  check(nav.calls.back().name=="world_findFreeSpot" && nav_mem.load(C+0x10,4)==0,"malformed free spot cannot fabricate a teleport");
  auto nav_count=nav.writes.size();
  refuses<M::Unsupported>([&]{nav.badger_chase(UINT32_MAX-0x3c7u);},"chase validates Badger extension span");
  check(nav.writes.size()==nav_count,"invalid chase span makes no writes");
  using V=M::Structured;
  auto none=M::shape_result("anim_clear",{});
  check(std::get<V>(none).kind==champions::simulation::rta::creature_movement::ValueKind::None,"void ABI returns None");
  auto mask=M::shape_result("model_buildHideMask",{},UINT64_C(0xfedcba9876543210));
  check(std::get<std::uint64_t>(mask)==UINT64_C(0xfedcba9876543210),"ABI retains unsigned64 mask");
  auto shaped=M::shape_result("world_slideMove",{0u,M::Vec3{},0u,0u},std::uint64_t{17});
  check(std::get<std::uint64_t>(shaped)==17,"null optional output collapses to scalar");
  shaped=M::shape_result("model_getSamplePoint",{0u,0u,M::Vec3{},0u,0u,0u,0u,0u},std::uint64_t{0},std::uint64_t{0},
    std::vector<V>{V::vector({1,2,3}),V::vector({4,5,6})});
  check(std::get<V>(shaped).vec==M::Vec3{1,2,3},"optional output consumed but omitted without changing required tuple");
  auto signed_head=M::shape_result("world_slideMove",{0u,M::Vec3{},0u,1u},V::integer(-9));
  check(std::get<V>(signed_head).items[0].scalar==-9,"compound ABI preserves signed head");
  auto wide_head=M::shape_result("world_slideMove",{0u,M::Vec3{},0u,1u},UINT64_MAX);
  check(std::get<std::uint64_t>(std::get<M::CompoundResult>(wide_head).head)==UINT64_MAX,"compound ABI retains unsigned64 head without signed narrowing");
  auto text=M::shape_result("libc_sprintf",{},std::uint64_t{0},std::uint64_t{0},std::vector<V>{V::buffer(std::string("A\0B",3))});
  check(std::get<M::BufferResult>(text).payload.bytes==std::string("A\0B",3),"text output retains embedded zero byte");
  refuses<M::Unsupported>([&]{M::shape_result("not_an_external",{});},"unknown ABI refuses");
  refuses<M::Unsupported>([&]{M::shape_result("world_slideMove",{});},"missing optional-output argument refuses");
  M::Memory abi_mem;M::Model abi(abi_mem);
  refuses<H::MissingHandler>([&]{abi.call_value("model_getBounds",{});},"generic call defaults to missing-service refusal");
  check(abi.calls.size()==1 && abi.writes.empty(),"generic missing call records exact prefix");
  abi.policy=M::CallbackPolicy::ReferenceZero;
  auto bounds=abi.call_value("model_getBounds",{});
  check(std::get<V>(bounds).kind==champions::simulation::rta::creature_movement::ValueKind::Tuple && std::get<V>(bounds).items.size()==6,"generic zero policy uses full six-output shape");
  auto objects=abi.call_value("world_collectObjectsInBox",{});
  check(std::get<V>(objects).kind==champions::simulation::rta::creature_movement::ValueKind::List,"generic object-array default is a list rather than a tuple");
  auto vector=M::shape_result("model_getAttachPoint",{0u,M::Vec3{},0u,0u,0u,0u});
  check(std::get<V>(vector).kind==champions::simulation::rta::creature_movement::ValueKind::Vector,"single vector output retains tuple identity");
  abi.structured_callbacks["host_service"]=[](auto &model,const auto &args) {
    model.structured_callbacks.erase("host_service");
    return std::get<V>(args[1]);
  };
  auto value=abi.call_value("host_service",{V::integer(-1),V::tuple({V::integer(-1),V::integer(INT64_C(0x100000001))})});
  check(std::get<std::uint32_t>(abi.calls.back().args[0])==UINT32_MAX,"generic top-level integer wraps raw32");
  check(std::get<V>(value).items[0].scalar==-1 && std::get<V>(value).items[1].scalar==INT64_C(0x100000001),"nested tuple integers stay untouched and callback may remove itself");
  abi.structured_callbacks["host_service"]=[](auto &model,const auto &) -> V {
    model.mem.store(C+0x44,4,77);throw M::Error("host failure");
  };
  refuses<M::Error>([&]{abi.call_value("host_service",{});},"generic structured callback failure propagates");
  check(abi_mem.load(C+0x44,4)==77 && abi.calls.back().name=="host_service","generic exception keeps callback memory and call prefix");
  abi.callbacks["mask_service"]=[](auto &,const auto &){return UINT64_C(0xfedcba9876543210);};
  check(std::get<std::uint64_t>(abi.call_value("mask_service",{}))==UINT64_C(0xfedcba9876543210),"generic callback retains raw64 result");
  abi.result_callbacks["full_service"]=[](auto &model,const auto &) -> M::Result {
    model.result_callbacks.erase("full_service");
    return M::shape_result("world_slideMove",{0u,M::Vec3{},0u,1u},UINT64_MAX);
  };
  auto compound=abi.call_value("full_service",{});
  check(std::get<std::uint64_t>(std::get<M::CompoundResult>(compound).head)==UINT64_MAX,"complete callback returns unsigned64 compound and removes itself safely");
  abi.result_callbacks["full_service"]=[](auto &,const auto &) -> M::Result {
    return M::BufferResult{V::integer(-17)};
  };
  check(std::get<M::BufferResult>(abi.call_value("full_service",{})).payload.scalar==-17,"complete callback retains non-byte buffer payload");
  abi.callbacks["record_service"]=[](auto &,const auto &){return 0;};
  abi.call_value("record_service",{M::TaggedTuple{"elem",{V::integer(-1),V::integer(INT64_C(0x100000001))}}});
  const auto &element=std::get<M::TaggedTuple>(abi.calls.back().args[0]);
  check(element.tag=="elem" && element.fields[0].scalar==-1 && element.fields[1].scalar==INT64_C(0x100000001),"element tuple tag and nested integers remain unchanged");
  abi.call_value("record_service",{M::TaggedTuple{"desc",{V::tuple({V::tuple({V::integer(1),V::integer(2),V::integer(3),V::integer(4)}),V{},V::tuple({V::integer(5),V::integer(6),V::integer(7),V::integer(8)}),V{}})}}});
  const auto &descriptor=std::get<M::TaggedTuple>(abi.calls.back().args[0]);
  check(descriptor.tag=="desc" && descriptor.fields[0].items.size()==4 && descriptor.fields[0].items[2].items[0].scalar==5,"full descriptor tuple retains all supplied effect slots");
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
