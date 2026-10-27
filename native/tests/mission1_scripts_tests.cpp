// Authored synthetic scripts/records only. Offsets and literal predicates are
// the public 0d42d46 contract; every identifier and all other cells are authored.
#include "champions/mission1_scripts.hpp"
#include "champions/script_commands.hpp"
#include <algorithm>
#include <iostream>
#include <memory>
#include <set>

namespace r = champions::simulation::rta;
namespace a = r::amx;
namespace sc = r::script_commands;
namespace ms = r::mission1_scripts;
namespace {
unsigned checks=0, failures=0;
void check(bool ok, const char *why) { ++checks; if (!ok) { ++failures; std::cerr << "FAIL " << why << '\n'; } }
struct Fixture {
  std::vector<a::Cell> code = std::vector<a::Cell>(0x3000/4, 0x7c);
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
    auto s=name.empty()?"authored-entry-"+std::to_string(at):name;
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
    const auto effect=f.text("authored-untraced-effect-"+std::to_string(i));
    f.entry(0xb30+0x10*i,"authored-untraced-effect-"+std::to_string(i));
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
  f.call(0xa68,"getv",{flag}); f.cells(0xa78,{0x54,0x35,0xb10});
  f.call(0xaa0,"setSaveInhibit",{0}); f.call(0xacc,"cutScene",{f.text("authored-cutscene")});
  f.call(0xb00,"setv",{flag,1});
  return f;
}
r::level_factory::Executable executable() {
  return {[](uint32_t va){return "authored-class-"+std::to_string(va);},[](uint32_t va){
    const auto ch=int64_t(va)-r::level_factory::CTYPE;
    return uint8_t(ch>='A'&&ch<='Z'?1:ch==' '?8:0);
  }};
}
std::vector<r::level_flow::Record> records(const ms::Contract &c) {
  auto exe=executable();
  std::vector<r::level_flow::Record> rows;
  auto add=[&](uint32_t ctor,std::vector<std::string> props={}) {
    r::level_flow::Record row;
    if(ctor==0x23c44c || ctor==0x23d93c || ctor==0x23ad18) {
      row.cls="authored-fallback";
      props.push_back(exe.string(ctor==0x23c44c?0x5339a0:ctor==0x23d93c?0x533980:0x533990)+"=1");
    }
    else if(!ctor) row.cls="authored-no-object";
    else {
      const auto &tests=r::level_factory::tests();
      const auto p=std::find_if(tests.begin(),tests.end(),[&](const auto &t){return t.size && t.calls[t.call_count-1]==ctor;});
      if(p==tests.end()) throw std::runtime_error("authored constructor row missing");
      row.cls=exe.string(p->name);
    }
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
        {0x23ad18,3},{0x23c44c,24},{0x243594,4},{0x28f15c,12},{0x28cb90,3},{0x27c694,1},
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
} // namespace
int main() {
  try {
    for(bool s:{true,false}) {
      auto f=s?strife():drundar(); auto image=f.image(s?47:36,s?21:20);
      sc::Host host;
      check(host.load(image),"authored addressed script loads");
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
      if(s) {
        host.variables().quest_set(contract->counter_key,2);
        host.restore_mission1_addressed();
        check(host.mission1_local_count()==2,"addressed persistent catapult restoration");
        for(unsigned i=0;i<3;++i) check(host.mission1_bridge_request(i)==sc::Stop::Refused,"unported bridge request refused");
      } else {
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
        check(host.run_mission1_entry(0xa3c)==sc::Stop::Refused,"door stops at unknown cutscene");
        check(host.variables().memory.word(0x50605c)==0&&host.variables().quest_get(contract->door_flag)==0,"door clears inhibit but does not invent completion after cutscene refusal");
        const auto save=host.save_campaign();
        sc::Host restored;check(restored.load(image),"revisit script loads");restored.start_single_player();restored.load_campaign(save);
        for(const auto &sw:contract->switches) check(restored.variables().quest_get(sw.persistent)==1,"campaign restores each switch independently");
      }
      const auto key=s?0x1674u:0x874u;
      f.code[key/4]=s?3:9;f.options.publics.clear();
      sc::Host changed;
      check(!changed.load(f.image(s?47:36,s?21:20)),"changed literal predicate refused");
    }
    // Ordinary scripts retain their existing interpreter path.
    sc::Host generic;
    a::ImageOptions options;options.publics={{"start",0}};
    check(generic.load(a::build_image({0x2e,0x59,0x30},options))&&!generic.mission1_contract(),"unrelated script unchanged");
  } catch(const std::exception &e) {++failures;std::cerr<<"FAIL "<<e.what()<<'\n';}
  std::cout<<"Mission 1 synthetic checks "<<checks<<", failures "<<failures<<'\n';
  return failures?1:0;
}
