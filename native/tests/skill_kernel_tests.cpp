// MIT; Copyright (c) 2026 TrustlessMike and contributors.
#include "champions/detail/skill_kernel.hpp"
#include "champions/detail/skill_handle_kernel.hpp"
#include "champions/detail/skill_spell_kernel.hpp"
#include "champions/skills_a.hpp"
#include "champions/skills_b.hpp"
#include "champions/skills_c.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

using namespace champions::simulation::rta;
namespace A = skills_a;
namespace B = skills_b;
namespace C = skills_c;
namespace K = detail::skill_kernel;
namespace {
void require(bool v) { if (!v) throw std::runtime_error("skill kernel/adaptor assertion"); }
template<class E, class F> void error(F f, const std::string &message) {
  try { f(); } catch (const E &e) { require(e.what() == message); return; }
  throw std::runtime_error("missing skill adapter failure");
}
void host_tests() {
  // Read delegates make skipped reads and their failure order observable.
  unsigned reads = 0;
  creature_hp::Memory memory({}, [&](auto, auto) -> std::uint64_t {
    ++reads; throw creature_hp::Error("read probe");
  });
  A::Model a(memory,{}); B::Model b(memory); C::Model c;
  c.mem = memory;
  require(a.skill_stat(UINT32_MAX, 0x80000000, 0) == 0);
  require(a.skill_stat_tier2(UINT32_MAX, 0x80000000, 0, 1) == 0);
  require(b.get_stat(UINT32_MAX, 0, 0) == 0);
  require(b.get_stat_tier2(UINT32_MAX, 0, 0, 1) == 0);
  require(c.execute(C::Operation::Stat, {UINT32_MAX,0x80000000,0})[0] == 0);
  require(c.execute(C::Operation::Tier2, {UINT32_MAX,0x80000000,0,1})[0] == 0);
  error<B::Unsupported>([&]{b.get_stat(0,5,0);}, "only statistics 0..4 are supported");
  require(reads == 0);
  error<creature_hp::Error>([&]{a.skill_stat(0x1000,0,1);}, "read probe");
  require(reads == 1);

  // Below-first and B's delta refusal do not touch base/step.
  reads = 0;
  creature_hp::Memory first_only({}, [&](auto at, auto width) -> std::uint64_t {
    ++reads;
    if (at == 0x100e && width == 2) return 32767;
    throw creature_hp::Error("unexpected later read");
  });
  a.mem=first_only; b.mem=first_only; c.mem=first_only;
  require(a.skill_stat(0x1000,0,1)==0);
  require(b.get_stat(0x1000,0,1)==0);
  require(c.execute(C::Operation::Stat,{0x1000,0,1})[0]==0);
  require(reads==3);
  b.mem=creature_hp::Memory({}, [&](auto at, auto) -> std::uint64_t {
    ++reads; if(at==0x100e) return 65535;
    throw creature_hp::Error("unexpected later read");
  });
  error<B::Unsupported>([&]{b.get_stat(0x1000,0,0x7fffffff);},
                       "level delta exceeds core signed32 conversion domain");
  require(reads==4);

  // Per-port player callback arity and replacement stay in the adapters.
  A::Model ca; B::Model cb; C::Model cc;
  for(auto *mem : {&ca.mem,&cb.mem,&cc.mem}) {
    mem->poke(0x1060,4,0x2000); mem->poke(0x300c,4,0x40);
  }
  ca.callbacks[10]=[](auto &self,const auto &args) -> A::Values {
    require(args==A::Values({0x1000,0,7,3}));
    self.callbacks[10]=[](auto &,const auto &) -> A::Values { return {8}; };
    return {7};
  };
  cb.callbacks["Skill.v10"]=[](auto &self,const auto &args) {
    require(args==std::vector<Raw>({0x1000,0,7}));
    self.callbacks["Skill.v10"]=[](auto &,const auto &) { return Raw(8); };
    return Raw(7);
  };
  cc.callbacks[10]=[](auto &self,const auto &args) {
    require(args==std::vector<Raw>({0x1000,0,7}));
    self.callbacks[10]=[](auto &,const auto &) { return std::vector<Raw>{8}; };
    return std::vector<Raw>{7};
  };
  for(Raw expected : {7u,8u}) {
    require(ca.skill_stat_for(0x1000,0,7,0x3000,3)==expected);
    require(cb.get_stat_for(0x1000,0,7,0x3000,3)==expected);
    require(cc.execute(C::Operation::ForCaster,{0x1000,0,7,0x3000,3})[0]==expected);
  }
  ca.callbacks[10]=[](auto &self,const auto &) -> A::Values {
    self.draw(); throw std::runtime_error("callback after draw");
  };
  cb.callbacks["Skill.v10"]=[](auto &self,const auto &) -> Raw {
    self.draw(); throw std::runtime_error("callback after draw");
  };
  cc.callbacks[10]=[](auto &self,const auto &) -> std::vector<Raw> {
    self.execute(C::Operation::SpellRand,{0x8000});
    throw std::runtime_error("callback after draw");
  };
  error<std::runtime_error>([&]{ca.skill_stat_for(0x1000,0,7,0x3000,3);},"callback after draw");
  error<std::runtime_error>([&]{cb.get_stat_for(0x1000,0,7,0x3000,3);},"callback after draw");
  error<std::runtime_error>([&]{cc.execute(C::Operation::ForCaster,{0x1000,0,7,0x3000,3});},"callback after draw");
  require(ca.draws==1 && cb.draws==1 && cc.draws==0);
  require(ca.mem.log().size()==1 && cb.mem.log().size()==1 && cc.mem.log().size()==1);
  require(ca.writes.size()==1 && cc.writes.size()==1);
  require(ca.mem.load(0x404464,4)==1013904223 && cb.mem.load(0x404464,4)==1013904223);
  require(cc.mem.load(0x8010,4)==1013904223);

  // Preserve partial writes/draws on store-history failure during a range.
  creature_hp::Limits limits; limits.history_entries=2;
  A::Model ra{creature_hp::Memory(limits),{}};
  ra.mem.poke(0x1060,4,0x2000);
  ra.callbacks[10]=[](auto &,const auto &) -> A::Values { return {7}; };
  ra.mem.poke(A::G_RAND_SEED,4,0x12345678);
  error<creature_hp::ResourceLimit>([&]{ra.skill_stat_range(0x1000,0,1,1,0x40400000);},
                                  "memory store history budget exceeded");
  require(ra.draws==2 && ra.mem.log().size()==2 && ra.writes.size()==3);
  C::Model rc; rc.mem=creature_hp::Memory(limits);
  rc.mem.poke(0x1060,4,0x2000);
  rc.callbacks[10]=[](auto &,const auto &) { return std::vector<Raw>{7}; };
  rc.mem.poke(0x404464,4,0x12345678);
  error<creature_hp::ResourceLimit>([&]{rc.execute(C::Operation::Range,{0x1000,0,1,1,0x40400000});},
                                  "memory store history budget exceeded");
  require(rc.draws==2 && rc.mem.log().size()==2 && rc.writes.size()==3);
  B::Model rb{creature_hp::Memory(limits)};
  rb.mem.poke(0x1060,4,0x2000);
  rb.callbacks["Skill.v10"]=[](auto &,const auto &) { return Raw(7); };
  rb.mem.poke(B::G_RAND_SEED,4,0x12345678);
  error<creature_hp::ResourceLimit>([&]{rb.stat_range(0x1000,0,1,1,0x404464,0x5000,0x40400000);},
                                  "memory store history budget exceeded");
  require(rb.draws==2 && rb.mem.log().size()==2); // immediate output store fails before next draw
  require(rb.mem.load(B::G_RAND_SEED,4)==ra.mem.load(A::G_RAND_SEED,4));
}
// Separate transport: no historical kernel input or observation format changes.
void handle_host_tests() {
  namespace H = detail::skill_handle_kernel;
  // Fail every ordered adapter read/write/offset boundary. Retained prefixes
  // are independently specified here, not generated from native observations.
  const std::vector<std::string> expected={"r100", "r200", "w100/4/300", "a200/16", "w216/2/0"};
  for(std::size_t fail=0;fail<=expected.size();++fail) {
    std::vector<std::string> seen;
    auto event=[&](std::string e) {
      if(seen.size()==fail) throw std::runtime_error("handle probe");
      seen.push_back(e);
    };
    auto run=[&] {
      return H::take(100u, [&](unsigned at) {event("r"+std::to_string(at));return at==100?200u:300u;},
          [&](unsigned at,unsigned width,unsigned value) {event("w"+std::to_string(at)+"/"+std::to_string(width)+"/"+std::to_string(value));},
          [&](unsigned at,unsigned off) {event("a"+std::to_string(at)+"/"+std::to_string(off));return at+off;});
    };
    if(fail<expected.size()) error<std::runtime_error>(run,"handle probe");
    else require(run()==200);
    require(seen==std::vector<std::string>(expected.begin(),expected.begin()+fail));
  }
  for(unsigned fail=0;fail<=3;++fail) {
    unsigned calls=0; std::vector<unsigned> stores;
    auto event=[&]{if(calls++==fail) throw std::runtime_error("recycle probe");};
    auto run=[&]{H::recycle(100u,200u,[&](unsigned at){event();require(at==100);return 300u;},
        [&](unsigned at,unsigned width,unsigned value){event();require(width==4);stores.push_back(at);require(value==(at==200?300u:200u));});};
    if(fail<3) error<std::runtime_error>(run,"recycle probe"); else run();
    require(stores.size()==(fail>1?fail-1:0));
  }
  // Empty take has no tag offset or stores.
  require(H::take(0u,[](unsigned){return 0u;},[](auto,auto,auto){require(false);},
                  [](auto,auto){require(false);return 0u;})==0);
  const std::vector<unsigned> order={1,2,3,4,5,6,7,8,9,10,11,12};
  for(unsigned fail=0;fail<=order.size();++fail) {
    std::vector<unsigned> seen;unsigned takes=0,offsets=0,stores=0;
    auto event=[&](unsigned id){if(seen.size()==fail) throw std::runtime_error("pair probe");seen.push_back(id);};
    auto run=[&]{return H::paired_link(100u,101u,99u,
      [&]{event(++takes);return takes==1?200u:300u;},
      [&](unsigned head,unsigned node){event(head==100?3:4);require(node==(head==100?200:300));},
      [&](unsigned at,unsigned width,unsigned value){event(6+2*stores);++stores;require(width==4);
        const unsigned addresses[]={212,208,312,308},values[]={99,300,0,200};
        require(at==addresses[stores-1] && value==values[stores-1]);},
      [&](unsigned at,unsigned off){event(5+2*offsets++);return at+off;});};
    if(fail<order.size()) error<std::runtime_error>(run,"pair probe");else require(run()==200);
    require(seen==std::vector<unsigned>(order.begin(),order.begin()+fail));
  }
  // Lazy tag arithmetic refuses only after publishing the successor.
  B::Model wide_b;C::Model wide_c;
  for(auto *mem:{&wide_b.mem,&wide_c.mem}) {
    mem->poke(0x864ea8,4,0xfffffff8);mem->poke(0xfffffff8,4,0);
  }
  error<B::Unsupported>([&]{wide_b.handle_link(0x5000,0x6000,7);},
                        "address offset exceeds raw32 memory domain");
  error<creature_hp::ResourceLimit>([&]{wide_c.execute(C::Operation::HandleLink,{0x5000,0x6000,7});},
                                   "memory address, width or span outside supported domain");
  require(wide_b.mem.load(0x864ea8,4)==0 && wide_c.mem.load(0x864ea8,4)==0);
  require(wide_b.mem.log().size()==1 && wide_c.mem.log().size()==1 && wide_c.writes.size()==1);
  // Established refusal limits bound cyclic clear/iteration; Python generation
  // only runs cyclic clear with a finite injected store budget.
  B::Model b;C::Model c;
  for(auto *mem:{&b.mem,&c.mem}) {
    mem->poke(0x864ea8,4,0x4000);
    mem->poke(0x5000,4,0x4000);mem->poke(0x4000,4,0x4000);
  }
  error<creature_hp::ResourceLimit>([&]{b.handle_clear(0x5000);},"memory store history budget exceeded");
  error<C::Unsupported>([&]{c.execute(C::Operation::HandleClear,{0x5000});},"write history limit");
  require(b.mem.log().size()==65536 && c.mem.log().size()==65536 && c.writes.size()==65536);
  C::Model items;items.mem.poke(0x5000,4,0x4000);items.mem.poke(0x4000,4,0x4000);
  error<C::Unsupported>([&]{items.execute(C::Operation::HandleItems,{0x5000});},"handle traversal cycle/limit");
}
void spell_host_tests() {
  namespace S = detail::skill_spell_kernel;
  const std::array<Raw,3> seeds={0u,0xffffffffu,0x12345678u};
  const std::array<Raw,3> stepped={0x3c6ef35fu,0x3c558d52u,0x75432777u};
  for(unsigned n=0;n<seeds.size();++n) {
    const std::vector<std::string> order={"read-address","read","write-address","write"};
    for(std::size_t fail=0;fail<=order.size();++fail) {
      std::vector<std::string> seen;Raw state=seeds[n];
      auto event=[&](const std::string &e) {
        if(seen.size()==fail) throw std::runtime_error("spell publication probe");
        seen.push_back(e);
      };
      auto run=[&] {return S::rng_publication(
        [&]{event("read-address");event("read");return state;},
        [&](Raw full){event("write-address");event("write");state=full;});};
      if(fail<order.size()) error<std::runtime_error>(run,"spell publication probe");
      else require(run()==(stepped[n]>>16));
      require(seen==std::vector<std::string>(order.begin(),order.begin()+fail));
      require(state==(fail==order.size()?stepped[n]:seeds[n]));
    }
  }
  // B's checked seed address precedes acquisition; A/C's publication span
  // belongs after acquisition. Inject at every lazy draw/read/address/store.
  for(bool checked : {false,true}) {
    const std::vector<std::string> order=checked ?
      std::vector<std::string>{"seed-address","draw","shared-read","shared-write","seed-write","table-address","table-write"} :
      std::vector<std::string>{"draw","shared-read","shared-write","seed-address","seed-write","table-address","table-write"};
    for(std::size_t fail=0;fail<=order.size();++fail) {
      std::vector<std::string> seen;unsigned draws=0;Raw shared=0,private_seed=0xffffffff,table=0x515ba0;
      auto event=[&](const std::string &e) {
        if(seen.size()==fail) throw std::runtime_error("spell tail probe");
        seen.push_back(e);
      };
      auto run=[&] {
        if(checked) event("seed-address");
        S::constructor_tail([&] {
          event("draw");event("shared-read");event("shared-write");
          shared=0x3c6ef35f;++draws;return Raw(0x3c6e);
        },[&](Raw seed) {
          if(!checked) event("seed-address");
          event("seed-write");private_seed=seed;
        },[&] {event("table-address");event("table-write");table=0xdeadbeef;});
      };
      if(fail<order.size()) error<std::runtime_error>(run,"spell tail probe");else run();
      require(seen==std::vector<std::string>(order.begin(),order.begin()+fail));
      require(draws==(fail>(checked?3u:2u)?1u:0u));
      require(shared==(draws?0x3c6ef35fu:0u));
      require(private_seed==(fail>4?0x3c6eu:0xffffffffu));
      require(table==(fail==order.size()?0xdeadbeefu:0x515ba0u));
    }
  }
}
// Separate input-only spell transport. Each port retains its own observation sink.
void spell_trace(std::istream &in, std::ostream &out) {
  std::string line;
  while(std::getline(in,line)) {
    std::istringstream row(line); unsigned port,budget,room,fail,count;
    if(!(row>>port>>budget>>room>>fail>>count) || port>2 || room>65536)
      throw std::runtime_error("spell fixture header");
    std::vector<std::pair<creature_hp::Address,unsigned>> reads;
    creature_hp::Limits limits;limits.history_entries=budget;
    creature_hp::Memory mem(limits,[&](auto at,auto width) -> std::uint64_t {
      reads.emplace_back(at,width);
      if(fail && reads.size()==fail) throw std::runtime_error("spell read probe");
      return 0;
    });
    for(unsigned i=0;i<count;++i) {
      std::uint64_t at,value;unsigned width;
      if(!(row>>at>>width>>value)) throw std::runtime_error("spell fixture poke");
      mem.poke(at,width,value);
    }
    A::Model a(mem,{});B::Model b(mem);C::Model c;c.mem=mem;
    const auto prefix=65536-room;a.writes.resize(prefix);c.writes.resize(prefix);
    std::string status="ok";std::vector<Raw> result;bool refused=false;
    if(!(row>>count)) throw std::runtime_error("spell fixture commands");
    for(unsigned i=0;i<count;++i) {
      unsigned op,arity;
      if(!(row>>op>>arity) || op>5 || arity>5) throw std::runtime_error("spell fixture operation");
      std::vector<std::uint64_t> args(arity);
      for(auto &v:args) if(!(row>>v) || (port!=1 && v>UINT32_MAX)) throw std::runtime_error("spell fixture argument");
      if(refused) continue;
      try {
        if(port==0) {
          if(op==0) a.spell_base(args.at(0),args.at(1),args.at(2),args.at(3),args.at(4));
          else if(op==1) result.push_back(a.spell_rand(args.at(0)));
          else throw std::runtime_error("spell A fixture operation");
        } else if(port==1) {
          if(op==0) b.spell_base_ctor(args.at(0),args.at(1),args.at(2),args.at(3),args.at(4));
          else if(op==1) result.push_back(b.spell_rand(args.at(0)));
          else if(op==2) result.push_back(b.object_rand(args.at(0)));
          else if(op==3) result.push_back(b.cosmetic_draw());
          else if(op==4) result.push_back(b.seed3_draw());
          else result.push_back(b.draw());
        } else {
          std::vector<Raw> values(args.begin(),args.end());
          if(op>1) throw std::runtime_error("spell C fixture operation");
          auto returned=c.execute(op==0?C::Operation::SpellBase:C::Operation::SpellRand,values);
          result.insert(result.end(),returned.begin(),returned.end());
        }
      } catch(const std::runtime_error &e) {status=e.what();refused=true;}
    }
    std::string trailing;if(row>>trailing) throw std::runtime_error("spell fixture trailing input");
    const auto &memory=port==0?a.mem:port==1?b.mem:c.mem;
    out<<status<<'|';for(std::size_t i=0;i<result.size();++i) {if(i)out<<',';out<<result[i];}
    out<<'|'<<(port==0?a.draws:port==1?b.draws:c.draws)<<'|';
    if(port!=1) {
      const auto &writes=port==0?a.writes:c.writes;
      for(std::size_t i=prefix;i<writes.size();++i) {const auto &w=writes[i];out<<w.address<<','<<w.size<<','<<w.value<<';';}
    }
    out<<'|';for(const auto &w:memory.log())out<<w.address<<','<<w.size<<','<<w.value<<';';
    out<<'|';for(const auto &r:reads)out<<r.first<<','<<r.second<<';';
    out<<'|';for(const auto &byte:memory.bytes())out<<byte.first<<','<<unsigned(byte.second)<<';';out<<'\n';
    require(a.calls.empty() && b.calls.empty() && b.events.empty() && c.calls.empty());
  }
}
void handle_trace(std::istream &in,std::ostream &out) {
  std::string line;
  while(std::getline(in,line)) {
    std::istringstream row(line);unsigned port,budget,room,count;
    if(!(row>>port>>budget>>room>>count) || port>1 || room>65536) throw std::runtime_error("handle fixture header");
    creature_hp::Limits limits;limits.history_entries=budget;
    creature_hp::Memory mem(limits);
    for(unsigned i=0;i<count;++i) {std::uint64_t at,value;unsigned width;row>>at>>width>>value;mem.poke(at,width,value);}
    B::Model b(mem);C::Model c;c.mem=mem;
    const auto prefix=65536-room;c.writes.resize(prefix);
    std::string status="ok";std::vector<Raw> result;bool refused=false;
    row>>count;
    for(unsigned i=0;i<count;++i) {
      unsigned op,arity;row>>op>>arity;std::vector<Raw> args(arity);for(auto &v:args)row>>v;
      if(refused) continue;
      try {
        if(port==0) {
          if(op==0) result.push_back(b.handle_link(args.at(0),args.at(1),args.at(2)));
          else if(op==1) result.push_back(b.handle_remove_node(args.at(0)));
          else if(op==2) {b.handle_clear(args.at(0));result.push_back(0);}
          else if(op==3) {auto values=b.list_objects(args.at(0));result.insert(result.end(),values.begin(),values.end());}
          else throw std::runtime_error("handle fixture operation");
        } else {
          const C::Operation ops[]={C::Operation::HandleLink,C::Operation::HandleRemove,C::Operation::HandleClear,C::Operation::HandleItems};
          auto values=c.execute(ops[op],args);result.insert(result.end(),values.begin(),values.end());
        }
      } catch(const std::runtime_error &e) {status=e.what();refused=true;}
    }
    mem=port?c.mem:b.mem;
    out<<status<<'|';for(std::size_t i=0;i<result.size();++i) {if(i)out<<',';out<<result[i];}
    out<<'|';if(port)for(std::size_t i=prefix;i<c.writes.size();++i) {const auto &w=c.writes[i];out<<w.address<<','<<w.size<<','<<w.value<<';';}
    out<<'|';for(const auto &w:mem.log())out<<w.address<<','<<w.size<<','<<w.value<<';';
    out<<'|'; // These direct operations do not call services/callbacks.
    require(b.calls.empty() && b.events.empty() && c.calls.empty());
    out<<'|';for(const auto &byte:mem.bytes())out<<byte.first<<','<<unsigned(byte.second)<<';';out<<'\n';
  }
}
void range_trace(std::istream &in, std::ostream &out) {
  std::string line;
  while (std::getline(in,line)) {
    std::istringstream row(line);
    unsigned config,port,kind,alias,budget,slot,mode;
    Raw value,random,spread,seed,adjust,object;
    if (!(row>>config>>port>>kind>>value>>random>>spread>>seed>>alias>>budget>>adjust>>object>>slot>>mode))
      throw std::runtime_error("range fixture row");
    set_fpu(static_cast<Fpu::Model>(config/4),config&2,config&1);
    creature_hp::Limits limits; limits.history_entries=budget;
    const auto failure=mode==1 ? creature_hp::Address(object)+0x60 : mode==2 ? 0x2050u : mode==3 ? 0x2054u : UINT64_MAX;
    creature_hp::Memory mem(limits,[&](auto at,auto) -> std::uint64_t {
      if (kind==1 && at==failure) throw std::runtime_error("range read probe");
      return 0;
    });
    for (const auto &entry : std::vector<std::array<std::uint64_t,3>>{
        {creature_hp::Address(object)+0x60,4,0x2000},{0x2050,2,adjust},{0x2054,4,mode==7 ? 0x3c85fcu : 0x9000u},
        {0x404464,4,seed},{0x8010,4,seed},{0x1024,4,value}})
      if (!(kind==1 && entry[0]==failure)) mem.poke(entry[0],entry[1],entry[2]);
    const Raw lo=alias==1 || alias==3 ? 0x404464 : alias==4 ? 0x1024 : 0x5000;
    const Raw hi=alias==2 || alias==3 ? 0x404464 : alias==4 ? 0x1018 : alias==5 ? lo : 0x5004;
    std::vector<Raw> result;
    std::vector<std::vector<Raw>> calls;
    std::vector<creature_hp::Write> writes;
    std::uint64_t draws=0;
    std::string status="ok";
    auto callback=[&](auto &m,const std::vector<Raw> &args,auto private_draw,auto replace) {
      calls.push_back(args);
      if (mode==4) throw std::runtime_error("range callback probe");
      if (mode==5) { private_draw(); throw std::runtime_error("range callback probe"); }
      if (mode==6) replace(m);
      return value;
    };
    auto operate=[&](auto &m,auto range,auto invoke) {
      try {
        if (kind==0) range();
        else if (kind==1) {
          result.push_back(invoke());
          if (mode==6) result.push_back(invoke());
        } else {
          const auto fixed=K::range_scale(value,spread);
          const auto scaled=K::range_random_scale(value,seed>>16,spread);
          result={fixed,scaled,K::range_combine(value,fixed,false),K::range_combine(value,scaled,true),K::adjust_this(object,adjust)};
        }
      } catch(const std::runtime_error &e) { status=e.what(); }
      mem=m.mem;draws=m.draws;
    };
    if (port==0) {
      A::Model m(mem,{});
      if(mode==7) m.external.insert(10);
      m.callbacks[10]=[&](auto &self,const auto &args) -> A::Values {
        return {callback(self,args,[&]{self.spell_rand(0x8000);},[&](auto &target){
          target.callbacks[10]=[&](auto &,const auto &args) -> A::Values {
            calls.push_back(args);return {value+1};
          };
        })};
      };
      operate(m,[&]{result=m.skill_stat_range(object,0,1,random,spread);},
              [&]{return m.skill_call(object,slot,{0,1}).at(0);});
      for(const auto &w:m.writes) writes.push_back({w.address,w.size,w.value});
    } else if (port==1) {
      B::Model m(mem);
      if(mode==7) m.external.insert("Skill.v10");
      m.callbacks["Skill.v10"]=[&](auto &self,const auto &args) -> Raw {
        return callback(self,args,[&]{self.spell_rand(0x8000);},[&](auto &target){
          target.callbacks["Skill.v10"]=[&](auto &,const auto &args) -> Raw {
            calls.push_back(args);return value+1;
          };
        });
      };
      operate(m,[&]{m.stat_range(object,0,1,random,lo,hi,spread);},
              [&]{return m.skill_call(object,slot,{0,1});});
    } else if (port==2) {
      C::Model m;m.mem=mem;
      if(mode==7) m.external.insert(10);
      m.callbacks[10]=[&](auto &self,const auto &args) -> std::vector<Raw> {
        return {callback(self,args,[&]{self.execute(C::Operation::SpellRand,{0x8000});},[&](auto &target){
          target.callbacks[10]=[&](auto &,const auto &args) -> std::vector<Raw> {
            calls.push_back(args);return {value+1};
          };
        })};
      };
      operate(m,[&]{result=m.execute(C::Operation::Range,{object,0,1,random,spread});},
              [&]{return m.skill_call(object,slot,{0,1}).at(0);});
      for(const auto &w:m.writes) writes.push_back({w.address,w.size,w.value});
    } else throw std::runtime_error("range fixture port");
    out<<status<<'|';
    for(std::size_t i=0;i<result.size();++i) out<<(i ? "," : "")<<result[i];
    out<<'|'<<draws<<'|';
    for(const auto &w:writes) out<<w.address<<','<<w.size<<','<<w.value<<';';
    out<<'|';for(const auto &w:mem.log()) out<<w.address<<','<<w.size<<','<<w.value<<';';
    out<<'|';for(const auto &args:calls) {
      for(std::size_t i=0;i<args.size();++i) out<<(i ? "," : "")<<args[i];
      out<<';';
    }
    out<<'|';for(const auto &b:mem.bytes()) out<<b.first<<','<<unsigned(b.second)<<';';
    out<<'\n';
  }
}

void trace(std::istream &in, std::ostream &out) {
  std::string line;
  while(std::getline(in,line)) {
    std::istringstream row(line);
    unsigned config, port, op;
    Raw stat,level,cap,first,base,step,first2,base2,step2,seed;
    if(!(row>>config>>port>>op>>stat>>level>>cap>>first>>base>>step>>first2>>base2>>step2>>seed))
      throw std::runtime_error("kernel fixture row");
    set_fpu(static_cast<Fpu::Model>(config/4),config&2,config&1);
    creature_hp::Memory mem;
    constexpr Raw skill=0x1000;
    for (const auto &entry : std::vector<std::array<Raw,3>>{
        {0xe,2,first},{0x18,2,base},{0x24,4,step},
        {0x38,2,first2},{0x42,2,base2},{0x4c,4,step2}}) {
      const auto at=std::int64_t(skill)+entry[0]+std::int64_t(s32(stat))*entry[1];
      if(at>=0 && at+entry[1]-1<=UINT32_MAX) mem.poke(at,entry[1],entry[2]);
    }
    mem.poke(0x404464,4,seed); mem.poke(0x8010,4,seed);
    if(op==2) { mem.poke(0x1060,4,0x2000); mem.poke(0x2054,4,0x3c85fc); }
    const auto lo=cap==1 || cap==3 ? Raw(0x404464) : cap==4 ? Raw(0x1024) : Raw(0x5000);
    const auto hi=cap==2 || cap==3 ? Raw(0x404464) : cap==4 ? Raw(0x1018) : cap==5 ? lo : Raw(0x5004);
    Raw value=0,shared=0,own=0; std::string status="ok";
    std::size_t draws=0,writes=0;
    auto operate=[&](auto &m, auto statistic, auto shared_random, auto random) {
      try { value=statistic(); shared=shared_random(); own=random(); }
      catch(const std::runtime_error &e) { status=e.what(); }
      mem=m.mem; draws=m.draws;
    };
    if(port==0) {
      A::Model m(mem,{});
      if(op==2) {
        const auto pair=m.skill_stat_range(skill,stat,level,1,step2);
        value=pair[0]; shared=pair[1]; mem=m.mem; draws=m.draws;
      } else operate(m,[&]{return op ? m.skill_stat_tier2(skill,stat,level,cap) : m.skill_stat(skill,stat,level);},
              [&]{return m.draw();}, [&]{return m.spell_rand(0x8000);});
      writes=m.writes.size();
    } else if(port==1) {
      B::Model m(mem);
      if(op==2) {
        m.stat_range(skill,stat,level,1,lo,hi,step2);
        value=Raw(m.mem.load(lo,4)); shared=Raw(m.mem.load(hi,4)); mem=m.mem; draws=m.draws;
      } else operate(m,[&]{return op ? m.get_stat_tier2(skill,stat,level,cap) : m.get_stat(skill,stat,level);},
              [&]{return m.draw();}, [&]{return m.spell_rand(0x8000);});
    } else if(port==2) {
      C::Model m; m.mem=mem;
      if(op==2) {
        const auto pair=m.execute(C::Operation::Range,{skill,stat,level,1,step2});
        value=pair[0]; shared=pair[1]; mem=m.mem; draws=m.draws;
      } else operate(m,[&]{return op ? m.execute(C::Operation::Tier2,{skill,stat,level,cap})[0] :
                                  m.execute(C::Operation::Stat,{skill,stat,level})[0];},
              [&]{m.execute(C::Operation::SpellBase,{0x8000,7,0x9000,skill,0x6000}); return Raw(m.mem.load(0x8010,4));},
              [&]{return m.execute(C::Operation::SpellRand,{0x8000})[0];});
      writes=m.writes.size();
    } else throw std::runtime_error("kernel fixture port");
    // Include kernel raw arithmetic independently of adapter acceptance/rounding.
    const auto d=K::delta(level,s16(first));
    const auto raw=K::statistic_sum(K::statistic_base(Raw(s16(base))),K::statistic_step(step,d));
    out<<status<<'|'<<value<<'|'<<shared<<'|'<<own<<'|'<<draws<<'|'<<writes<<'|'
       <<raw<<'|'<<K::capped_level(level,cap)<<'|'<<K::tier2_fallback(s16(first2))<<'|'
       <<K::lcg_step(seed)<<'|'<<K::lcg_result(K::lcg_step(seed))<<'|';
    for(const auto &w:mem.log()) out<<w.address<<','<<w.size<<','<<w.value<<';';
    out<<'|'; for(const auto &b:mem.bytes()) out<<b.first<<','<<unsigned(b.second)<<';';
    out<<'\n';
  }
}
}
int main(int argc,char **argv) {
  try {
    if(argc==2 && std::string(argv[1])=="--spell-host") {spell_host_tests();std::cout<<"Spell kernel/adaptor checks passed\n";}
    else if(argc==4 && std::string(argv[1])=="--spell-trace") {
      std::ifstream in(argv[2]);std::ofstream out(argv[3]);
      if(!in || !out) throw std::runtime_error("spell trace file");
      spell_trace(in,out);
    }
    else if(argc==2 && std::string(argv[1])=="--handle-host") { handle_host_tests();std::cout<<"Handle kernel/adaptor checks passed\n"; }
    else if(argc==4 && std::string(argv[1])=="--handle-trace") {
      std::ifstream in(argv[2]);std::ofstream out(argv[3]);
      if(!in || !out) throw std::runtime_error("handle trace file");
      handle_trace(in,out);
    }
    else if(argc==4 && std::string(argv[1])=="--range-trace") {
      std::ifstream in(argv[2]); std::ofstream out(argv[3]);
      if(!in || !out) throw std::runtime_error("range trace file");
      range_trace(in,out);
    }
    else if(argc==3) { std::ifstream in(argv[1]); std::ofstream out(argv[2]); if(!in || !out) throw std::runtime_error("kernel trace file"); trace(in,out); }
    else if(argc==1) { host_tests(); std::cout<<"Skill kernel/adaptor host checks passed\n"; }
    else throw std::runtime_error("usage: skill-kernel-tests [input output]");
    return 0;
  } catch(const std::exception &e) { std::cerr<<e.what()<<'\n'; return 1; }
}
