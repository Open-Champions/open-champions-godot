// Authored synthetic scripts/records only. Offsets and literal predicates are
// the public research contracts; every identifier and all other cells are authored.
#include "champions/mission1_scripts.hpp"
#include "champions/mission1_levels.hpp"
#include "champions/arena_monsters.hpp"
#include "champions/quests.hpp"
#include <cmath>
#include <cstring>
#include "champions/script_commands.hpp"
#include <algorithm>
#include <iostream>
#include <memory>
#include <set>

namespace r = champions::simulation::rta;
namespace a = r::amx;
namespace sc = r::script_commands;
namespace ms = r::mission1_scripts;
namespace ml = r::mission1_levels;
namespace {
unsigned checks=0, failures=0;
void check(bool ok, const char *why) { ++checks; if (!ok) { ++failures; std::cerr << "FAIL " << why << '\n'; } }
struct Fixture {
  std::vector<a::Cell> code = std::vector<a::Cell>(0x3000/4, 0x2e);
  a::ImageOptions options;
  std::map<std::string, a::Cell> strings;
  std::map<unsigned,std::string> entries;
  Fixture() { options.data.resize(0x200); }
  void cells(unsigned off, std::initializer_list<a::Cell> values) {
    for (auto v:values) { code.at(off/4)=v; off+=4; }
  }
  a::Cell text(const std::string &s) {
    if (strings.count(s)) return strings.at(s);
    const auto at=a::Cell(options.data.size());
    for (unsigned char ch:s) { options.data.push_back(ch); options.data.insert(options.data.end(),3,0); }
    options.data.insert(options.data.end(),4,0);
    strings[s]=at; return at;
  }
  std::string entry(unsigned at, const std::string &name={}) {
    if (entries.count(at)) return entries.at(at);
    auto s=name.empty()?"fixture-"+std::to_string(at):name;
    entries[at]=s;
    cells(at,{0x2e});
    return s;
  }
  void call(unsigned at, const std::string &name, const std::vector<a::Cell> &args) {
    auto &ns=options.natives;
    auto found=std::find(ns.begin(),ns.end(),name);
    if (found==ns.end()) { ns.push_back(name); found=ns.end()-1; }
    for (unsigned i=0;i<args.size();++i) cells(at-16-8*i,{0x27,args[i]});
    cells(at-8,{0x27,a::Cell(args.size()*4),0x7b,a::Cell(found-ns.begin()),0x2c,a::Cell((args.size()+1)*4)});
  }
  a::ByteVector image(unsigned publics, unsigned natives) {
    unsigned at=0x2600;
    while(entries.size()<publics) { entry(at); at+=16; }
    for (const auto &p:entries) options.publics.emplace_back(p.second,p.first);
    for (std::size_t i=0;options.natives.size()<natives;++i) {
      const auto name=std::string(sc::row(i).name);
      if(std::find(options.natives.begin(),options.natives.end(),name)==options.natives.end()) options.natives.push_back(name);
    }
    return a::build_image(code,options);
  }
};
Fixture strife() {
  Fixture f;
  f.entry(0x638,"start"); f.entry(0x568,"preStart");
  f.call(0x594,"preloadResource",{f.text("authored-preload")});
  const auto counter=f.text("authored-counter");
  f.call(0x5e8,"getv",{counter}); f.cells(0x5f8,{0x0f,0x14c});
  f.cells(0x159c,{0x28,0x14c,0x27,counter,0x27,8});
  f.call(0x15b4,"setv",{counter,0});
  f.cells(0x159c,{0x28,0x14c,0x27,counter,0x27,8});
  const a::Cell stages[]={f.text("authored-stage-a"),f.text("authored-stage-a"),f.text("authored-stage-b"),f.text("authored-stage-c")};
  const unsigned gets[]={0x678,0x760,0x844,0x928};
  for(unsigned i=0;i<4;++i) f.call(gets[i],"getv",{stages[i]});
  f.cells(0x688,{0x54,0x35,0x744}); f.cells(0x770,{0x35,0x828});
  f.cells(0x854,{0x35,0x90c}); f.cells(0x938,{0x35,0x9f0});
  const unsigned sites[4][4]={{0x6b0,0x6dc,0x708,0x734},{0x794,0x7c0,0x7ec,0x818},
                            {0x878,0x8a4,0x8d0,0x8fc},{0x95c,0x988,0x9b4,0x9e0}};
  for(unsigned group=0;group<4;++group) for(unsigned i=0;i<4;++i)
    f.call(sites[group][i],group==i?"showMonster":"hideMonster",{f.text("authored-scout-"+std::to_string(i))});
  f.entry(0x200c); f.entry(0xc94);
  f.call(0x1f44,"getv",{stages[0]}); f.call(0x2040,"setv",{stages[0],1});
  f.call(0x20a4,"callScript",{f.text(f.entries.at(0xc94))});
  const auto quest=f.text("authored-quest");
  f.call(0x20d8,"addQuest",{quest,f.text("authored-body")});
  const unsigned bridges[]={0x1c90,0x1df0,0x1ed4};
  for(unsigned i=0;i<3;++i) f.call(bridges[i],"trigger",{f.text("authored-bridge-"+std::to_string(i)),1});
  f.call(0x1f00,"removeQuest",{quest});
  for(auto at:{0x15ccu,0x156cu,0x17f8u,0x88u,0xccu,0x3dcu,0x420u,0x138u}) f.entry(at);
  f.cells(0x1668,{0x01,0x14c,0x69,2,0x35,0x1690}); f.cells(0x1694,{0x35,0x1770});
  f.call(0x16f4,"setv",{stages[3],1}); f.call(0x1720,"callScript",{f.text(f.entries.at(0x17f8))});
  // Travel sites (research NEWWORLD rows; authored destinations).
  f.entry(0x470); f.entry(0x1a80);
  f.call(0x490,"newWorld",{f.text("authored-story-back")});
  f.call(0x1ae8,"newWorld",{f.text("authored-demo-hub")});
  f.call(0x1b94,"newWorld",{f.text("authored-arena")});
  return f;
}
Fixture drundar() {
  Fixture f;
  f.entry(0x150,"start");
  for(auto at:{0x18cu,0x1c8u,0x204u}) f.call(at,"setMonsterVal",{f.text("authored-boss"),1,2});
  auto flag=f.text("authored-door-done");
  f.call(0x338,"getv",{flag}); f.cells(0x348,{0x35,0x3b8});
  f.call(0x374,"autoPropSetState",{f.text("authored-doors"),1});
  f.call(0x3a8,"autoPropSetState",{f.text("authored-stairs"),1});
  const unsigned gets[]={0x47c,0x4f0,0x564,0x5d8,0x64c,0x6c0,0x734,0x7a8};
  const unsigned effects[]={0x4b0,0x524,0x598,0x60c,0x680,0x6f4,0x768,0x7dc};
  const unsigned skips[]={0x4d4,0x548,0x5bc,0x630,0x6a4,0x718,0x78c,0x800};
  const unsigned keys[]={0x1d38,0x1e08,0x1ed8,0x1fa8,0x2078,0x2148,0x2218,0x22e8};
  const unsigned actions[]={0x1cac,0x1d7c,0x1e4c,0x1f1c,0x1fec,0x20bc,0x218c,0x225c};
  for(unsigned i=0;i<8;++i) {
    const auto key=f.text("authored-switch-"+std::to_string(i));
    const auto effect=f.text("effect-"+std::to_string(i));
    f.entry(0xb30+0x10*i,"effect-"+std::to_string(i));
    f.entry(keys[i]); f.entry(actions[i]);
    f.call(gets[i],"getv",{key}); f.cells(gets[i]+0x10,{0x35,a::Cell(skips[i])});
    f.call(effects[i],"callScript",{effect});
    f.call(keys[i]+0x2c,"getv",{key}); f.cells(keys[i]+0x3c,{0x54,0x30});
    f.cells(actions[i]+0x1c,{0x6d,0});
    f.call(actions[i]+0x48,"setv",{key,1}); f.call(actions[i]+0x74,"callScript",{effect});
  }
  f.entry(0x824); f.entry(0x1c38);
  f.cells(0x868,{0x01,0,0x69,8,0x35,0x900}); f.cells(0x1c54,{0x01,0,0x69,8});
  f.call(0x89c,"setSaveInhibit",{1});
  f.call(0x8dc,"addDeferedCall",{f.text(f.entry(0x934))});
  f.call(0x9e4,"callScript",{f.text(f.entry(0xa3c))});
  // research-strife-drundar@3216e7b0 tools/difftest_strife_drundar.py:399-402.
  f.call(0xa68,"getv",{flag}); f.cells(0xa78,{0x54,0x35,0xb10});
  f.call(0xaa0,"setSaveInhibit",{0}); f.call(0xacc,"cutScene",{f.text("authored-cutscene")});
  f.call(0xb00,"setv",{flag,1});
  f.entry(0x1a00); f.entry(0x1c00);
  f.call(0x1a18,"newWorld",{f.text("authored-demo-hub")});
  f.call(0x1ac4,"newWorld",{f.text("authored-arena")});
  f.call(0x1c20,"newWorld",{f.text("authored-story-back")});
  return f;
}
r::level_factory::Executable executable() {
  return {[](uint32_t va){return "class-"+std::to_string(va)+
      ((va==0x541e98 || va==0x541eb0 || va==0x541ec0)?"-%d":"");},[](uint32_t va){
    const auto ch=int64_t(va)-r::level_factory::CTYPE;
    return uint8_t(ch>='A'&&ch<='Z'?1:ch==' '?8:0);
  }};
}
std::vector<r::level_flow::Record> records(const ms::Contract &c) {
  auto exe=executable();
  std::vector<r::level_flow::Record> rows;
  auto add=[&](uint32_t ctor,std::vector<std::string> props={}) {
    r::level_flow::Record row;
    if(ctor==0x23c44c || ctor==0x2416c4 || ctor==0x23d93c || ctor==0x23ad18) {
      row.cls="authored-fallback";
      props.push_back(exe.string((ctor==0x23c44c || ctor==0x2416c4)?0x5339a0:ctor==0x23d93c?0x533980:0x533990)+"=1");
    }
    else if(!ctor) row.cls="authored-no-object";
    else {
      const auto &tests=r::level_factory::tests();
      const auto p=std::find_if(tests.begin(),tests.end(),[&](const auto &t){return t.size && t.calls[t.call_count-1]==ctor;});
      if(p==tests.end()) throw std::runtime_error("authored constructor row missing");
      row.cls=exe.string(p->name);
    }
    if (ctor==0x2416c4) props.push_back(exe.string(0x541ec8)+"=1");
    row.props=std::move(props); rows.push_back(row);
  };
  if(c.level==ms::Level::Strife) {
    for(auto &name:c.scouts) add(0x29df04,{"name="+name});
    for(unsigned i=0;i<69;++i) {
      std::vector<std::string> props;
      if(i<15) { props={"initscript="+c.entries.at(i<13?0x88:0x3dc),"deathscript="+c.entries.at(i<13?0xcc:0x420)}; }
      add(0x2a0954,props);
    }
    for(unsigned i=0;i<2;++i) add(0x24d440,{"deathscript="+c.entries.at(0x138)});
    for(auto &name:c.bridges) add(0x23c44c,{"name="+name});
  } else {
    for(unsigned i=0;i<71;++i) add(0x26a624);
    for(unsigned i=0;i<14;++i) add(0x29fad0,i==0?std::vector<std::string>{"name=authored-boss","miniboss=1"}:std::vector<std::string>{});
    for(unsigned i=0;i<4;++i) add(0x243594,{"name=authored-doors"});
    add(0x23c44c,{"name=authored-stairs"});
    for(const auto &s:c.switches) add(0x23c44c,{"key="+c.entries.at(s.key_entry),"action1="+c.entries.at(s.action_entry)});
  }
  const unsigned selected=c.level==ms::Level::Strife?267:254;
  const std::map<uint32_t,unsigned> expected=c.level==ms::Level::Strife
    ? std::map<uint32_t,unsigned>{{0x2a0954,69},{0x24d440,2},{0x261dbc,15},{0x2b1e80,70},
        {0x23d93c,73},{0x23ad18,6},{0x23c44c,8},{0x29df04,4},{0x28f15c,6},{0x28cb90,3},
        {0x27c694,1},{0x1ec900,2},{0x1ecdf4,2},{0x1d7778,1},{0x158928,2},
        {0x158bcc,1},{0x158c6c,1},{0x158d0c,1}}
    : std::map<uint32_t,unsigned>{{0x26a624,71},{0x29fad0,14},{0x290c24,29},{0x23d93c,70},
        {0x23ad18,3},{0x23c44c,16},{0x2416c4,8},{0x243594,4},{0x28f15c,12},{0x28cb90,3},{0x27c694,1},
        {0x1ec900,13},{0x1ecdf4,6},{0x158928,1},{0x158bcc,1},{0x158c6c,1},{0x158d0c,1}};
  for(const auto &p:expected) {
    unsigned have=0;
    for(const auto &row:rows) {const auto o=r::level_factory::classify(row.cls,&row.props,exe);if(o.constructor()==p.first)++have;}
    while(have++<p.second) add(p.first);
  }
  if(rows.size()!=selected) throw std::runtime_error("authored fixture census size");
  for(unsigned i=0;i<16;++i) add(0);
  return rows;
}
// Same public record encoder as world_loadRecords' documented authored format.
r::level_flow::Blob objects(const std::vector<r::level_flow::Record> &rows) {
  r::level_flow::Blob out(8,0), strings(1,0);
  auto w16=[&](unsigned at,unsigned v){out.at(at)=uint8_t(v);out.at(at+1)=uint8_t(v>>8);};
  auto w32=[&](unsigned at,unsigned v){for(unsigned i=0;i<4;++i)out.at(at+i)=uint8_t(v>>(8*i));};
  w16(0,unsigned(rows.size()));
  auto string=[&](const std::string &s){const auto at=unsigned(strings.size());strings.insert(strings.end(),s.begin(),s.end());strings.push_back(0);return at;};
  for(const auto &row:rows) {
    const unsigned at=unsigned(out.size());
    out.resize(at+24+4*row.props.size(),0);
    w32(at,string(row.cls));
    for(unsigned i=0;i<row.props.size();++i) w32(at+20+4*i,string(row.props[i]));
    w16(at+4,unsigned(out.size()-at));
  }
  w32(4,unsigned(out.size()));out.insert(out.end(),strings.begin(),strings.end());
  return out;
}

// ---- research-strife-drundar de4aafe docs/mission1-levels.md sections 6-8 and
// the difftest's death/marker/hub/loader tables, with authored names only.
struct CutBuilder {
  struct A { std::string cls; float x=0, y=0, z=0; bool create=false; };
  struct K { float time; int actor, kind; };
  std::uint32_t flags=8; std::vector<A> actors; std::vector<K> keys;
  std::vector<std::uint8_t> bytes() const {
    std::vector<std::uint8_t> out(ml::CUT_HEADER,0);
    auto put32=[&](std::size_t at,std::uint32_t v){for(unsigned i=0;i<4;++i)out.at(at+i)=std::uint8_t(v>>(8*i));};
    auto putf=[&](std::size_t at,float f){std::uint32_t v;std::memcpy(&v,&f,4);put32(at,v);};
    const auto actor_at=out.size(); out.resize(actor_at+actors.size()*ml::CUT_ACTOR,0);
    for(std::size_t k=0;k<actors.size();++k) {
      const auto at=actor_at+k*ml::CUT_ACTOR;
      for(std::size_t i=0;i<actors[k].cls.size()&&i<12;++i) out[at+8+i]=std::uint8_t(actors[k].cls[i]);
      putf(at+0x1c,actors[k].x); putf(at+0x20,actors[k].y); putf(at+0x24,actors[k].z);
      out[at+0x2b]=actors[k].create?1:0;
    }
    const auto key_at=out.size(); out.resize(key_at+keys.size()*ml::CUT_KEY,0);
    for(std::size_t k=0;k<keys.size();++k) {
      const auto at=key_at+k*ml::CUT_KEY; putf(at,keys[k].time);
      out[at+4]=std::uint8_t(keys[k].actor); out[at+5]=std::uint8_t(unsigned(keys[k].actor)>>8);
      out[at+6]=std::uint8_t(keys[k].kind); out[at+7]=std::uint8_t(unsigned(keys[k].kind)>>8);
    }
    put32(0,flags); put32(4,std::uint32_t(key_at)); put32(8,std::uint32_t(keys.size()));
    put32(0xc,std::uint32_t(actor_at)); put32(0x10,std::uint32_t(actors.size()));
    return out;
  }
};
ml::Candidate candidate(const std::string &cls,float x,float y,const std::string &type="9authored-other") {
  ml::Candidate c; c.class_string=cls; c.type_info=type; c.position={x,y,0}; return c;
}
void cutscene_checks() {
  // File layout (doc:338-343).
  CutBuilder b; b.actors={{"authored-door",100,200,5},{"authored-boss",-50,25,0},{"authored-stand",0,0,0,true}};
  b.keys={{1.91f,0,3},{4.0f,1,8},{10.01f,0,4},{12.0f,-1,9}};
  const auto cut=ml::parse_cut(b.bytes());
  check(cut.flags==8 && cut.keys.size()==4 && cut.actors.size()==3,"cut header counts");
  check(cut.actors[0].cls=="authored-doo" && cut.actors[1].position[0]==-50.0f && cut.actors[2].create && !cut.actors[0].create,
        "actor record class, position and create byte");
  check(cut.keys[1].actor==1 && cut.keys[1].kind==8 && cut.keys[3].actor==-1,"key actor and type");
  auto raw=b.bytes();
  bool short_refused=false; try{ml::parse_cut(raw.data(),raw.size()-1);}catch(const ml::CutError &){short_refused=true;}
  check(short_refused,"truncated key table refused");
  raw[8]=0xff; raw[9]=0xff; bool large=false; try{ml::parse_cut(raw);}catch(const ml::CutError &){large=true;}
  check(large,"key count above the host bound refused");
  // Door key order on a bound door: slot 3 with 1, then with 0 (doc:313-317).
  const auto door=ml::slot3_calls(cut,0);
  check(door.ok && door.calls.size()==2 && door.calls[0].argument==1 && door.calls[1].argument==0 &&
        door.calls[0].time<door.calls[1].time,"type-3 then type-4 key gives trigger 1 then 0");
  check(ml::slot3_calls(cut,1).ok && ml::slot3_calls(cut,1).calls.empty(),"no slot-3 keys on a shown actor");
  check(!ml::slot3_calls(cut,3).ok,"actor index outside the table refused");
  CutBuilder late=b; late.keys={{10.01f,0,4},{1.91f,0,3}};
  const auto out_of_order=ml::slot3_calls(ml::parse_cut(late.bytes()),0);
  check(!out_of_order.ok && out_of_order.calls.empty() && out_of_order.reason.find("ascending")!=std::string::npos,
        "slot-3 keys out of time order refused");
  check(ml::shown_actors(cut)==std::vector<std::size_t>{1},"show keys name actor 1 only");
  // Search box (doc:346-348).
  const auto box=ml::actor_box({1000.5f,-3.0f,7.0f});
  check(box.lo[0]==760.5f && box.hi[0]==1240.5f && box.lo[1]==-243.0f && box.hi[1]==237.0f &&
        box.lo[2]==7.0f-100000.0f && box.hi[2]==7.0f+100000.0f,"actor box +-240 x/y, +-100000 z");
  check(ml::compared_type_name("12authored-type")=="authored-type" && ml::compared_type_name("authored3")=="authored3",
        "type name skips only leading digits");
  // Selection (doc:353-357).
  const ml::CutActor actor{"authored-boss",{0,0,0},false};
  std::vector<ml::Candidate> q={candidate("authored-unrelated",1,1),candidate("authored-boss",300,0),
      candidate("authored-boss",100,0),candidate("authored-boss",50,0)};
  q[3].flags=ml::BOUND_FLAG;
  auto bound=ml::bind_actor(actor,q);
  check(bound.status==ml::BindStatus::Bound && bound.index==std::size_t(2) && bound.by_class_string && bound.distance==100.0f,
        "nearest unbound class-string match");
  check(ml::bind_actor(actor,{candidate("authored-boss",600,0)}).status==ml::BindStatus::NoMatch,"600 is not below 600");
  check(ml::bind_actor(actor,{candidate("authored-boss",0,599.5f)}).status==ml::BindStatus::Bound,"599.5 binds");
  check(ml::bind_actor(actor,{candidate("authored-boss",0,10,"")}).status==ml::BindStatus::Bound,"z is the query's filter");
  auto typed=ml::bind_actor(actor,{candidate("authored-x",20,0,"31authored-boss")});
  check(typed.status==ml::BindStatus::Bound && !typed.by_class_string,"type-name match binds");
  auto held=ml::bind_actor(actor,{candidate("authored-boss",300,0),candidate("authored-x",10,0,"5authored-boss")});
  check(held.index==std::size_t(0) && held.by_class_string,"held class-string match skips a nearer type-only match");
  auto later=ml::bind_actor(actor,{candidate("authored-x",10,0,"5authored-boss"),candidate("authored-boss",300,0)});
  check(later.index==std::size_t(0) && !later.by_class_string,"a farther class-string match does not replace a nearer held type match");
  // Capacity: the first 63 results in walk order (doc:369-370).
  std::vector<ml::Candidate> full(63,candidate("authored-unrelated",1,1));
  full.push_back(candidate("authored-boss",1,0));
  auto cut63=ml::bind_actor(actor,full);
  check(cut63.status==ml::BindStatus::NoMatch && cut63.truncated && cut63.kept==63,"64th result is beyond the capacity");
  full.erase(full.begin());
  auto in63=ml::bind_actor(actor,full);
  check(in63.status==ml::BindStatus::Bound && in63.index==std::size_t(62) && !in63.truncated,"63rd result is kept");
  // Player slots and created actors were counted; the rest stays refused.
  auto players=q; players[0].origin=ml::Origin::PlayerSlot; players[1].origin=ml::Origin::CutsceneCreated;
  check(ml::bind_actor(actor,players).status==ml::BindStatus::Bound,"counted origins bind");
  for(auto origin:{ml::Origin::RunTimeSpawn,ml::Origin::Moved,ml::Origin::LoadSpawn}) {
    auto spawned=q; spawned[0].origin=origin;
    const auto refused=ml::bind_actor(actor,spawned);
    check(refused.status==ml::BindStatus::Refused && !refused.index &&
          refused.reason==ml::open_reason(ml::Open::RunTimeObjectsInSearch),"uncounted object in the capacity count refused");
    auto beyond=full; beyond.push_back(candidate("authored-unrelated",1,1)); beyond.back().origin=origin;
    check(ml::bind_actor(actor,beyond).status==ml::BindStatus::Bound,"an uncounted object beyond the kept 63 takes no slot");
  }
  check(ml::bind_actor({"authored-stand",{0,0,0},true},q).status==ml::BindStatus::Created,"created actor is not searched");
  for(auto o:{ml::Open::DoorStateAfterTrigger,ml::Open::ShownStaysHidden,ml::Open::RunTimeObjectsInSearch,
              ml::Open::CutscenePlayback,ml::Open::UnrequestedCutscene,ml::Open::DemoBranch})
    check(std::string(ml::open_reason(o)).find("docs/mission1-levels.md:")!=std::string::npos,"each Unknown cites the research");
}
// Creature_onDeath 0x10beb8 as the client ports it (arena_monsters slot28_on_death),
// on the ten research record shapes (DEATHS_BY_NAME) with quiet 0 and 1.
void death_checks() {
  namespace A=r::arena_monsters;
  using Props=std::map<std::uint64_t,std::string>; // property-name address -> value
  constexpr std::uint64_t SCRIPT_KEY=0x52fbf8, DROP_KEY=0x52fc08, CREATURE=0x10000, TEXT=0x70000;
  const std::vector<Props> shapes={
    {{DROP_KEY,"authored-shard"}}, {}, {},
    {{SCRIPT_KEY,"authored-boss-death"}},
    {{SCRIPT_KEY,"authored-k1"},{DROP_KEY,"authored-potion-a"}},{{SCRIPT_KEY,"authored-k2"},{DROP_KEY,"authored-potion-b"}},
    {{SCRIPT_KEY,"authored-k3"},{DROP_KEY,"authored-potion-a"}},{{SCRIPT_KEY,"authored-k4"},{DROP_KEY,"authored-potion-b"}},
    {{SCRIPT_KEY,"authored-shaman-death"}},{{SCRIPT_KEY,"authored-shaman-death"}}};
  unsigned cases=0;
  for(const auto &props:shapes) for(std::uint64_t quiet:{0u,1u}) {
    A::Memory mem; A::Model m(mem);
    std::map<std::uint64_t,std::uint64_t> text_at;
    std::uint64_t next=TEXT;
    for(const auto &p:props) {
      text_at[p.first]=next;
      for(std::size_t i=0;i<=p.second.size();++i) mem.poke(next+i,1,i<p.second.size()?std::uint8_t(p.second[i]):0);
      next+=0x100;
    }
    auto read=[&](std::uint64_t at){std::string out;for(;;++at){auto ch=char(mem.load(at,1));if(!ch)break;out.push_back(ch);}return out;};
    std::vector<std::string> got;
    m.callbacks["obj_getPropStr"]=[&](A::Model &,const std::vector<A::Value> &args) -> A::Value {
      const auto key=std::uint64_t(std::get<std::int64_t>(args.at(1)));
      return std::int64_t(text_at.count(key)?text_at.at(key):0);
    };
    m.callbacks["script_runByName"]=[&](A::Model &,const std::vector<A::Value> &args) -> A::Value {
      got.push_back("script:"+read(std::uint64_t(std::get<std::int64_t>(args.at(0))))+
                    (mem.load(0x4039f8,4)==CREATURE?":selected":":other"));
      return std::int64_t(0);
    };
    m.callbacks["loot_spawnNamedItem"]=[&](A::Model &,const std::vector<A::Value> &args) -> A::Value {
      got.push_back("drop:"+read(std::uint64_t(std::get<std::int64_t>(args.at(1))))); return std::int64_t(0);
    };
    m.callbacks["creature_stopBossMusic"]=[](A::Model &,const std::vector<A::Value> &) -> A::Value {return std::int64_t(0);};
    m.slot28_on_death(CREATURE,quiet);
    std::vector<std::string> want;
    if(props.count(SCRIPT_KEY)) want.push_back("script:"+props.at(SCRIPT_KEY)+":selected");
    if(!quiet && props.count(DROP_KEY)) want.push_back("drop:"+props.at(DROP_KEY));
    check(got==want && mem.load(0x4039f8,4)==0,"death runs its script with the creature selected, then drops unless quiet");
    ++cases;
  }
  check(cases==20,"twenty death cases");
}
void loader_marker_hub_checks() {
  // Loader names: preStart then start, looked up case-sensitively (doc:189-196).
  for(bool camel:{true,false}) {
    a::ImageOptions options; options.publics={{camel?"preStart":"prestart",0},{"start",12}};
    sc::Host h;
    check(h.load(a::build_image({0x2e,0x59,0x30,0x2e,0x59,0x30},options)) && !h.mission1_contract(),"loader fixture loads");
    const auto stop=h.run_prestart();
    check(camel?stop==sc::Stop::Complete:stop==sc::Stop::Missing,"preStart lookup is case-sensitive");
  }
  // Arrival markers in record order (doc:431-437; MARKERS): rank, last taker, searches.
  namespace lf=r::level_flow;
  auto marker=[](const std::string &from){lf::Record m;m.cls="Player1";if(!from.empty())m.props={"from="+from};return m;};
  struct Case { std::vector<lf::Record> rows; std::string prev; int rank; std::string last; unsigned searches; };
  const std::vector<Case> cases={
    {{marker(""),marker("authored-arena")},"authored-story",1,"",4},
    {{marker(""),marker("authored-arena")},"authored-arena",2,"authored-arena",8},
    {{marker("")},"authored-story",1,"",4},{{marker("")},"authored-arena",1,"",4},
    {{marker("")},"authored-other",1,"",4},{{marker("")},"",1,"",4}};
  for(const auto &c:cases) {
    lf::SpawnState state; unsigned searches=0;
    const auto took=lf::run_markers(state,c.rows,c.prev,"",false,
        [&](const lf::Vec3 &at,auto,auto,bool){++searches;return at;},[]{return std::uint16_t(0);});
    const auto last=took.empty()?std::string("-"):c.rows[took.back()].prop("from").value_or("");
    check(state.rank==c.rank && last==c.last && searches==c.searches,"marker rank, last taker and spawn searches");
  }
  // Hub name (doc:459-467; HUB_NAMES): evil, decision set, mission 1 available or complete.
  namespace q=r::quests;
  for(int evil:{0,1}) for(bool complete:{true,false}) {
    q::Campaign c; c.set_num_players(1); // record 0
    c.set_evil_command(0); if(evil) c.set_evil_command(1); // setEvil(0) acts only on nonzero
    c.set_decision_command();
    c.mission_set_available(1,1); if(complete) c.mission_set_complete(1,-1);
    check(c.hub()==(evil&&complete?q::HUB_EVIL:q::HUB_GOOD),"hub for evil, decision and mission 1 state");
  }
}
} // namespace
int main() {
  try {
    for(bool s:{true,false}) {
      auto f=s?strife():drundar(); auto image=f.image(s?47:36,s?21:20);
      sc::Host host;
      const bool loaded = host.load(image);
      if (!loaded) std::cerr << "contract diagnostic " << host.last_fault() << "\n";
      check(loaded,"authored addressed script loads");
      const auto *contract=host.mission1_contract();
      check(contract!=nullptr,"candidate recognized"); if(!contract) continue;
      auto c=*contract;
      auto rows=records(c); c.validate_objects(rows,executable());
      check(c.objects_validated && c.census.records==(s?283u:270u),"record census and links");
      auto bad=rows; bad.pop_back();
      bool rejected=false;try {c.validate_objects(bad,executable());} catch(const std::exception &){rejected=true;}
      check(rejected&&!c.objects_validated,"wrong record count refused before acceptance");
      host.set_executable(executable());
      const auto blob=objects(rows);
      check(host.adopt_objects(blob.data(),blob.size()),"host adopts validated synthetic records");
      {
        // docs/mission1-levels.md:404-428: two requests per level are admitted,
        // the demoRunning branch is recorded and refused.
        const auto &t=contract->travel;
        check(t.size()==3 && std::count_if(t.begin(),t.end(),[](const ms::Travel &x){return x.demo_branch;})==1,
              "three travel sites, one demo branch");
        for(unsigned i=0;i<t.size();++i) {
          const auto before=host.variables().memory.cstr(0x41b6d0);
          const auto stop=host.mission1_travel(i);
          const auto after=host.variables().memory.cstr(0x41b6d0);
          if(t[i].demo_branch) check(stop==sc::Stop::Refused && after==before &&
              host.last_fault().find("demoRunning")!=std::string::npos,"demo branch travel refused without a request");
          else check(stop==sc::Stop::Complete && after==t[i].destination && !after.empty(),"travel site writes its pending level");
        }
        check(host.mission1_travel(3)==sc::Stop::Refused,"no fourth travel site");
        const auto demo=std::find_if(t.begin(),t.end(),[](const ms::Travel &x){return x.demo_branch;});
        check(contract->sites.count(demo->offset)==0,"demo travel site is not an admitted site");
      }
      if(s) {
        host.variables().quest_set(contract->counter_key,2);
        const auto restore = host.restore_mission1_addressed();
        check(restore==sc::Stop::Complete,"documented scout visibility effects apply through host");
        const auto before_sites=host.mission1_sites_reached();
        check(host.run_start()==sc::Stop::Refused,"untraced start body refuses");
        for(unsigned tick=0;tick<3;++tick) host.on_ready_tick();
        check(host.mission1_sites_reached()==before_sites,"ticks do not resume refused restoration or start");
        check(host.mission1_local_count()==2,"addressed persistent catapult restoration");
        for(unsigned i=0;i<3;++i) check(host.mission1_bridge_request(i)==sc::Stop::Refused,"unported bridge request refused");
      } else {
        host.variables().quest_set(contract->door_flag,1);
        check(host.restore_mission1_addressed()==sc::Stop::Refused,"unported door restoration refuses");
        check(host.mission1_sites_reached().count(0x374)==1 && host.mission1_sites_reached().count(0x3a8)==0,
              "restoration stops before the next effect after refusal");
        host.variables().quest_set(contract->door_flag,0);
        check(host.mission1_count_gate()==sc::Stop::Refused,"eight-switch gate stays closed below eight");
        for(unsigned i=0;i<8;++i) {
          const auto &sw=contract->switches[i];
          check(host.run_mission1_entry(sw.key_entry)==sc::Stop::Complete&&host.last_value()==1,"unpressed switch predicate");
          check(host.run_mission1_entry(sw.action_entry)==sc::Stop::Refused,"action stops at untraced effect body");
          check(host.mission1_local_count()==int(i+1)&&host.variables().quest_get(sw.persistent)==1,"increment and persistent effect survive refusal");
          check(host.run_mission1_entry(sw.key_entry)==sc::Stop::Complete&&host.last_value()==0,"persistent switch key closes");
          check(host.run_mission1_entry(sw.action_entry)==sc::Stop::Refused&&host.mission1_local_count()==int(i+1),"closed key cannot double-count a switch");
        }
        check(host.mission1_count_gate()==sc::Stop::Complete,"exact eight-switch save inhibit and deferred request");
        check(host.variables().memory.word(0x50605c)==1&&host.queue().queued>0,"gate records pending work");
        const auto before=host.queue().queued;
        for(unsigned i=0;i<5;++i) host.on_ready_tick();
        check(host.queue().queued==before,"continuous ticks do not execute untraced deferred work");
        check(host.run_mission1_entry(0xa3c)==sc::Stop::Refused,"zero door value refuses unsupported sequence");
        check(host.variables().memory.word(0x50605c)==1&&host.variables().quest_get(contract->door_flag)==0,"door refusal preserves inhibit and persistent flag");
        const auto save=host.save_campaign();
        sc::Host restored;check(restored.load(image),"revisit script loads");restored.start_single_player();restored.load_campaign(save);
        for(const auto &sw:contract->switches) check(restored.variables().quest_get(sw.persistent)==1,"campaign restores each switch independently");
      }
      const auto key=s?0x1674u:0x874u;
      f.code[key/4]=s?3:9;f.options.publics.clear();
      sc::Host changed;
      check(!changed.load(f.image(s?47:36,s?21:20)),"changed literal predicate refused");
    }
    // Expectations come from original VM observations, not fixture execution:
    // research-strife-drundar@3216e7b0 tools/difftest_strife_drundar.py:421-461.
    for (unsigned inhibit : {0u, 1u})
    for (a::Cell saved : {0, 1, 2, -1, a::Cell(-2147483647-1)}) {
      auto f=drundar(); sc::Host h;
      check(h.load(f.image(36,20)),"predicate fixture loads");
      if (!h.mission1_contract()) continue;
      h.set_executable(executable());
      auto blob=objects(records(*h.mission1_contract()));
      check(h.adopt_objects(blob.data(),blob.size()),"predicate fixture adopts");
      const auto key=h.mission1_contract()->door_flag;
      h.variables().quest_set(key,saved);
      const auto stored=h.variables().quest_get(key); // production signed-byte persistence clamps inputs
      check((stored==0)==(saved==0),"stored value preserves researched branch class");
      h.variables().memory.write(0x50605c,{std::uint8_t(inhibit),0,0,0});
      const auto stop=h.run_mission1_entry(0xa3c);
      const auto &sites=h.mission1_sites_reached();
      check(sites.count(0xa68)==1,"real diagnostic dispatches persistent read");
      check(stop==(saved==0?sc::Stop::Refused:sc::Stop::Complete),"original zero-only branch polarity");
      check(h.variables().memory.word(0x50605c)==inhibit,"door predicate preserves incoming save inhibit");
      check(!sites.count(0xaa0) && !sites.count(0xacc),"no clear or cutscene dispatch on refusal or nonzero skip");
      check(!sites.count(0xb00) && h.variables().quest_get(key)==stored,"no persistent write; saved value unchanged");
      if(saved==0) check(h.last_fault().find("unsupported door sequence")!=std::string::npos,"effect refusal remains explicit");
    }
    // Replacement and removal (authored PROC padding) must fail inspection.
    // Required NOT: research-strife-drundar@3216e7b0 tools/difftest_strife_drundar.py:400,415.
    for (auto operation : {a::Cell(0x55), a::Cell(0x2e)}) {
      auto f=drundar(); f.cells(0xa78,{operation}); sc::Host h;
      check(!h.load(f.image(36,20)),"changed or removed NOT rejected on load");
      check(!h.mission1_contract(),"mismatched predicate never publishes contract");
      check(h.run_mission1_entry(0xa3c)==sc::Stop::Refused && h.mission1_sites_reached().empty(),"rejected predicate cannot enter real diagnostic");
    }
    // An undocumented store inserted between the known action cells must
    // never run. The bounded adapter executes only the addressed increment.
    auto injected=drundar();
    injected.cells(0x1cac+4,{0x0b,99,0x0f,0});
    sc::Host bounded;
    check(bounded.load(injected.image(36,20)),"unknown gap is not part of the admitted body");
    bounded.set_executable(executable());
    if (!bounded.mission1_contract()) return 1;
    auto fixture_rows=records(*bounded.mission1_contract());
    auto blob=objects(fixture_rows);
    check(bounded.adopt_objects(blob.data(),blob.size()),"bounded gap fixture adopts");
    check(bounded.run_mission1_entry(0x1cac)==sc::Stop::Refused && bounded.mission1_local_count()==1,
          "unknown bytecode cannot overwrite addressed count before refusal");
    cutscene_checks();
    death_checks();
    loader_marker_hub_checks();
    // Ordinary scripts retain their existing interpreter path.
    sc::Host generic;
    a::ImageOptions options;options.publics={{"start",0}};
    check(generic.load(a::build_image({0x2e,0x59,0x30},options))&&!generic.mission1_contract(),"unrelated script unchanged");
  } catch(const std::exception &e) {++failures;std::cerr<<"FAIL "<<e.what()<<'\n';}
  std::cout<<"Mission 1 synthetic checks "<<checks<<", failures "<<failures<<'\n';
  return failures?1:0;
}
