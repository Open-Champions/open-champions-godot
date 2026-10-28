// Authored native expectations from original-execution probes, never a model
// comparing itself: research-death-reload 0e3afdb8 docs/death-reload.md:75-89,
// tools/difftest_death_reload.py:132-365. Fixtures contain synthetic values only.
#include "champions/ps2_class_restore.hpp"
#include "champions/creature_anim/runtime.hpp"
#include <iostream>
#include <stdexcept>
using namespace champions::simulation::rta;
using namespace champions::simulation::rta::ps2_save;
namespace {
unsigned checks = 0;
void check(bool value) { ++checks; if (!value) throw std::runtime_error("class restore check " + std::to_string(checks)); }
void put(Bytes &b, std::uint64_t v, unsigned n) { for (unsigned i=0;i<n;++i) b.push_back(std::uint8_t(v>>(8*i))); }
template<class F> void refuses(F f, const std::string &reason) {
  bool caught=false;
  try { f(); } catch (const Unknown &e) { caught=std::string(e.what()).find(reason)!=std::string::npos; }
  check(caught);
}
constexpr std::uint32_t C=0x10000;
void trigger() {
  for (auto flag : {0,1,255}) {
    builtins::Memory m;
    Bytes b; put(b,0x12345678,4); put(b,flag,1);
    check(restore_class_fields(m,C,"Trigger",b,{})==5);
    check(m.word(C+0x64)==0x12345678 && m.u8(C+0x60)==flag);
  }
}
void creatures() {
  constexpr std::uint64_t masks[]={0x1000000ull,0x100000000ull,0x400000ull,0x800000ull,0x2000000000000ull};
  for (bool dead : {false,true}) for (unsigned bits : {0,1,2,4,8,16,32,64,127}) for (bool match : {false,true}) {
    builtins::Memory m;
    m.word(C+0x22c,0x43200000); // constructed max HP 160
    m.word(C+0xc,0x20); m.byte(C+0x231,0xfe);
    // Nonserialized flag bits must survive; serialized bits must be replaced.
    m.w64(C+0xa8,~std::uint64_t(0));
    Bytes b; put(b,7,4); put(b,dead?0xbe800000:0x3f000000,4);
    if(dead) put(b,11,4);
    put(b,bits,4); if(bits&64) put(b,5,4);
    std::vector<std::string> events;
    ClassLoadServices s;
    s.variant=[&](auto a){check(a==C && m.word(C+0xe8)==7);events.push_back("variant");};
    s.stop=[&](auto a){check(a==C);events.push_back("stop");};
    s.dead_clip=[&](auto a,auto h)->std::optional<std::uint32_t>{check(a==C&&h==11);events.push_back("resolve");if(match)return 0x5678;return {};};
    s.reset_animation=[&](auto a){check(a==C+0x114);events.push_back("reset");};
    s.start_animation=[&](auto a,auto clip,auto flags){check(a==C+0x114&&clip==0x5678&&flags==4);events.push_back("start");};
    s.end_animation=[&](auto a){check(a==C+0x114);events.push_back("end");};
    s.motion=[&](auto a,auto value){check(a==C&&value==1);events.push_back("motion");};
    s.eligible_creature=[](auto){return true;};
    s.unlink=[&](auto a){check(a==C+0x234);events.push_back("unlink");};
    check(restore_class_fields(m,C,"Creature",b,s)==b.size());
    check(m.word(C+0x228)==(dead?0xc2200000:0x42a00000)); // -40 / 80
    auto flags=~std::uint64_t(0);
    for(unsigned i=0;i<5;++i) flags=(flags&~masks[i])|((bits&(1u<<i))?masks[i]:0);
    check(m.u64(C+0xa8)==flags);
    check(m.u8(C+0x231)==((bits&32)?0xff:0xfe));
    check(m.u16(C+0x312)==((bits&64)?5:0));
    std::vector<std::string> want{"variant"};
    if(dead){check(m.u16(C+0x232)==20&&m.word(C+0xc)==0x2029);want.insert(want.end(),{"stop","resolve"});
      if(match){want.insert(want.end(),{"reset","start","end","motion"});check(m.u16(C+0x11a)&0x80);}}
    if(bits&64)want.push_back("unlink");
    check(events==want);
  }
  builtins::Memory m; Bytes b; put(b,7,4);put(b,0,4);put(b,11,4);put(b,0,4);
  refuses([&]{restore_class_fields(m,C,"Creature",b,{});},"variant");
  ClassLoadServices s; s.variant=[](auto){};
  refuses([&]{restore_class_fields(m,C,"Creature",b,s);},"stop/list");
  check(m.u16(C+0x232)==20); // known prefix, no invented corpse continuation
}
void chests() {
  for(bool ac:{false,true})for(unsigned opened:{0,1})for(unsigned gold:{0,1,32767})for(unsigned variant:{0,1}){
    builtins::Memory m; m.word(C+0xc,0x100800);m.word(C+0x24,0x20000);m.word(0x2001c,0x1234);m.word(C+0xf4,0x1234);
    Bytes b;put(b,0xaabbccdd,4);put(b,gold,2);put(b,2,1);put(b,variant,1);put(b,opened,4);
    std::vector<unsigned> calls;
    ClassLoadServices s;
    s.start_animation=[&](auto a,auto clip,auto flags){check(a==C+(ac?0x98:0x5c)&&clip==0x1234&&flags==0x204);calls.push_back(1);};
    s.end_animation=[&](auto a){check(a==C+(ac?0x98:0x5c));calls.push_back(2);};
    s.class_definition=[&](auto v){check(v==variant);return 0x5678+v;};
    s.set_class=[&](auto a,auto d){check(a==C&&d==0x5678+variant);calls.push_back(3);};
    check(restore_class_fields(m,C,ac?"AutoChest":"Chest",b,s)==12);
    check(m.word(C+(ac?0xc8:0x54))==0xaabbccdd&&m.u16(C+(ac?0xdc:0x4c))==gold);
    check(m.u8(C+(ac?0xe4:0x58))==2&&m.u8(C+(ac?0xe5:0x59))==variant);
    check(m.word(C+(ac?0xd8:0x84))==opened);
    check(m.word(C+0xc)==((opened&&!gold)?0x800:0x100800));
    std::vector<unsigned>want;if(opened)want={1,2};if(!ac)want.push_back(3);check(calls==want);
  }
  builtins::Memory m; Bytes b(12);b[8]=1;
  refuses([&]{restore_class_fields(m,C,"Chest",b,{});},"clip start");
}
// Execute the existing native animation services, matching the original's
// constructed-cache control, not a rendered/world reconstruction claim.
// 0e3afdb8 tools/difftest_death_reload.py:340-365;
// docs/death-reload.md:77-78; tools/ref_anim.py:685,919.
void chest_animation_runtime() {
  for (bool ac : {false, true}) for (unsigned opened : {0, 1}) {
    creature_anim::Runtime runtime;
    creature_anim::Clip clip; clip.length = 73;
    const auto clip_id = runtime.add_clip(clip);
    const auto state = runtime.add_state(1);
    builtins::Memory m;
    m.word(C+0xc,0x100000); m.word(C+0x24,0x20000);
    m.word(0x2001c,std::uint32_t(clip_id)); m.word(C+0xf4,std::uint32_t(clip_id));
    Bytes b;put(b,7,4);put(b,0,2);put(b,0,1);put(b,0,1);put(b,opened,4);
    ClassLoadServices services;
    services.start_animation = [&](auto track, auto selected, auto flags) {
      check(track == C+(ac?0x98:0x5c));
      runtime.anim_push(state, selected, flags);
    };
    services.end_animation = [&](auto track) {
      check(track == C+(ac?0x98:0x5c));
      runtime.anim_jump_top_to_end(state);
    };
    services.class_definition = [](auto variant) { check(variant==0); return 0x20000u; };
    services.set_class = [&](auto object, auto definition) {
      check(object==C && definition==m.word(C+0x24)); // already-current cache boundary
    };
    check(restore_class_fields(m,C,ac?"AutoChest":"Chest",b,services)==12);
    check(runtime.state(state).count==int(opened));
    check(m.word(C+0xc)==(opened?0:0x100000));
    if (opened) {
      const auto &track=runtime.track(runtime.state(state).tracks[0]);
      check(track.clip==clip_id && track.flags==0x204);
      check(track.time==Fpu{}.from_int(73));
    }
  }
}
void doors() {
  for(const auto *cls:{"DoorSwing","DoorSecret","RollDoor"})for(unsigned opened:{0,1}){
    world_queries::LevelData data;data.pieces={0};data.heightfields.push_back({});
    data.elements.push_back(world_queries::make_element(0,0,0,4,4,0,0x12,0,0,123,32767,0));
    world_queries::Level terrain(data);
    level_flow::Piece piece;piece.tz=16;
    level_flow::Pieces world({piece},terrain);
    builtins::Memory m;m.word(C+0xc,0x100800);m.w16(C+0x4c,0);
    Bytes b;const std::string name=cls;
    if(name=="DoorSwing"){put(b,opened*32768,2);put(b,0,2);put(b,0x3fa00000,4);}
    else if(name=="DoorSecret")put(b,opened,1);
    else{put(b,opened,4);put(b,0,4);put(b,0x40200000,4);put(b,99,4);}
    unsigned updates=0;ClassLoadServices s;s.pieces=&world;s.update=[&](auto a){check(a==C);++updates;};
    check(restore_class_fields(m,C,name,b,s)==b.size());
    if(name=="DoorSwing"){
      check(m.u16(C+0x50)==opened*32768&&m.word(C+0x54)==0x3fa00000);
      check(terrain.data().elements[0].flags==0x12&&updates==opened);
      check(m.word(C+0xc)==(opened?0x801:0x100800));
      check(world.poses()[0].chunks[0].tz==16);
    }else if(name=="DoorSecret"){
      check(m.u8(C+0x58)==opened&&updates==0);
      check(m.word(C+0xc)==(opened?0x800:0x100800));
      check(opened?world.poses()[0].chunks[0].tz==-1184:world.poses()[0].chunks.empty());
    }else{
      check(m.u16(C+0x5a)==opened&&m.u16(C+0x4e)==123&&m.word(C+0x54)==99);
      check(terrain.data().elements[0].flags==0x10&&updates==opened);
      check(world.poses()[0].chunks.empty());
    }
  }
  builtins::Memory m;refuses([&]{restore_class_fields(m,C,"RollDoor",Bytes(16),{});},"world pieces");
}
void gates() {
  for(unsigned active:{0,1}){
    builtins::Memory m;ClassLoadServices s;std::vector<unsigned>calls;
    s.light=[&](auto a){check(a==C+0x100);calls.push_back(1);};
    s.gate_asset=[&](auto a,auto key){check(a==C+0x90&&key==0x547ee0);calls.push_back(2);return 0x3456;};
    s.start_animation=[&](auto a,auto clip,auto flags){check(a==C+0xd8&&clip==0x3456&&flags==3);calls.push_back(3);};
    s.update=[&](auto a){check(a==C);calls.push_back(4);};
    check(restore_class_fields(m,C,"JumpGate",Bytes{2,std::uint8_t(active)},s)==2);
    check(m.u8(C+0x4d)==2&&m.u8(C+0x4e)==active);
    check(calls==(active?std::vector<unsigned>{1,2,3,4}:std::vector<unsigned>{4}));
  }
  builtins::Memory m;refuses([&]{restore_class_fields(m,C,"JumpGate",Bytes{2,1},{});},"light effects");
  refuses([&]{restore_class_fields(m,C,"JumpGate",Bytes{2,0},{});},"update queue");
  refuses([&]{restore_class_fields(m,C,"OtherPortal",{},{});},"undocumented class");
}
void props() {
  for(bool item:{false,true})for(bool bit:{false,true})for(unsigned mask:{8,10}){
    builtins::Memory m;m.word(C+0xc,0x3800);m.w16(C+0x38,0xc000);
    m.word(C+0x10,0x40800000);m.word(C+0x14,0x40a00000);m.word(C+0x18,0x40c00000);
    Bytes b;
    if(item){b.resize(0x48);b[0]=7;if(bit)b[12]=0x80;}
    put(b,11,4);put(b,22,4);put(b,33,4);
    if(mask==8)put(b,3,1);else{put(b,0xffffffff,4);put(b,mask,8);put(b,0,1);}
    put(b,0,3);put(b,10000,2);put(b,1,2);put(b,0xfffe,2);put(b,3,2);
    ClassLoadServices s;unsigned assets=0;
    s.string=[](auto h){if(h==11)return std::string("synthetic_asset");if(h==22)return std::string("synthetic_model");check(h==33);return std::string("synthetic_texture");};
    s.asset=[&](const auto &a,const auto &name){check(a=="synthetic_asset");check(name==(!assets?"synthetic_model":"synthetic_texture"));return ++assets==1?0x1234:0x5678;};
    check(restore_class_fields(m,C,item?"ItemProp":"Prop",b,s)==b.size());
    check(m.word(C+0x4c)==0x1234&&m.word(C+0x50)==0x5678&&m.u64(C+0xe8)==mask);
    check(m.word(C+0x110)==ONE&&m.word(C+0x120)==ONE&&m.word(C+0x130)==ONE);
    check(m.word(C+0x134)==(item?0x40800000:ONE));
    check(m.word(C+0x138)==(item?0x40a00000:0xc0000000));
    check(m.word(C+0x13c)==(item?0x40c00000:0x40400000));
    if(item){check(m.word(C+0x148)==0x3f7650+7*0x40);check(m.word(C+0xc)==(bit?0x100000:0x103000));}
  }
  builtins::Memory m; Bytes b(24); b[16]=0x10; b[17]=0x27;
  refuses([&]{restore_class_fields(m,C,"Prop",b,{});},"string resolution");
}
// Refusal assertions cover host admission and exact callback boundaries, not
// whole-world completion. Pin: docs/death-reload.md:58-62,76,82-83,91-97.
void boundaries() {
  auto unchanged = [](const std::string &cls, const Bytes &b, const std::string &reason) {
    builtins::Memory m;
    m.word(C+0xc, 0x12345678); m.word(C+0xe8, 99);
    const auto before = m.read(C, 0x400);
    unsigned calls = 0; ClassLoadServices s;
    s.variant = [&](auto){++calls;};
    s.string = [&](auto){++calls;return std::string("synthetic");};
    s.asset = [&](const auto &, const auto &){++calls;return 1;};
    refuses([&]{restore_class_fields(m,C,cls,b,s);}, reason);
    check(m.read(C,0x400)==before && calls==0);
  };
  Bytes creature; put(creature,7,4); put(creature,0x3f000000,4); put(creature,128,4);
  unchanged("Creature",creature,"packed flags");
  creature[4]=0; creature[5]=0; creature[6]=0x80; creature[7]=0x7f;
  unchanged("Creature",creature,"nonfinite HP");
  unchanged("OtherPortal",{},"undocumented class");
  Bytes prop(24); prop[16]=0x10; prop[17]=0x27;
  auto bad = prop; bad[13]=1;
  unchanged("Prop",bad,"quaternion");
  bad=prop; bad[12]=64;
  unchanged("Prop",bad,"mask bit");
  Bytes item(0x48); item.insert(item.end(),prop.begin(),prop.end());
  unchanged("ItemProp",item,"facing");
  // Every truncated prefix of a supported fixed payload refuses before writes.
  for (const auto &entry : std::vector<std::pair<std::string,Bytes>>{
      {"Trigger",Bytes(5)}, {"Chest",Bytes(12)}, {"JumpGate",Bytes(2)}, {"Prop",prop}}) {
    for (std::size_t n=0;n<entry.second.size();++n) {
      builtins::Memory m; m.word(C+0xc,0x12345678);
      const auto before=m.read(C,0x400); bool caught=false;
      try { restore_class_fields(m,C,entry.first,Bytes(entry.second.begin(),entry.second.begin()+n),{}); }
      catch (const Error &) { caught=true; }
      check(caught && m.read(C,0x400)==before);
    }
  }
  // A supplied service can explicitly refuse its still-unknown world effect.
  // Its supported prefix is retained, but no later callback or field executes.
  builtins::Memory m; m.word(C+0x228,0x12345678); m.word(C+0xa8,0xdeadbeef);
  Bytes dead; put(dead,7,4);put(dead,0,4);put(dead,11,4);put(dead,127,4);put(dead,5,4);
  ClassLoadServices s; s.variant=[](auto){};
  s.stop=[](auto){throw Unknown("Creature world stop/list integration Unknown");};
  refuses([&]{restore_class_fields(m,C,"Creature",dead,s);},"world stop/list");
  check(m.word(C+0xa8)==0xdeadbeef && m.u16(C+0x312)==0);
  unsigned later=0;
  s.light=[](auto){throw Unknown("JumpGate portal light world effects Unknown");};
  s.gate_asset=[&](auto,auto){++later;return 1;};
  s.update=[&](auto){++later;};
  refuses([&]{restore_class_fields(m,C,"JumpGate",Bytes{2,1},s);},"portal light");
  check(later==0 && m.u8(C+0x4d)==2 && m.u8(C+0x4e)==1);

  world_queries::LevelData data; data.pieces={0}; data.heightfields.push_back({});
  data.elements.push_back(world_queries::make_element(0,0,0,4,4,0,0x12,0,0,123,32767,0));
  world_queries::Level terrain(data); level_flow::Pieces world({level_flow::Piece{}},terrain);
  s.pieces=&world; m.w16(C+0x4c,0); m.word(C+0x50,0x87654321);
  Bytes door;put(door,1,2);put(door,0,2);put(door,0,4);
  const auto before=m.read(C,0x400);
  refuses([&]{restore_class_fields(m,C,"DoorSwing",door,s);},"arbitrary-angle");
  check(m.read(C,0x400)==before && world.poses()[0].chunks.empty());
  check(terrain.data().elements[0].flags==0x12 && later==0);
}

}
int main(){try{trigger();creatures();chests();chest_animation_runtime();doors();gates();props();boundaries();}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}std::cout<<"PS2 class restore: "<<checks<<" checks passed; recording services are bounded, not full-world proof\n";}
