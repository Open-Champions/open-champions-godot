// MIT; Copyright (c) 2026 TrustlessMike and contributors.
#include "champions/detail/skill_kernel.hpp"
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
    if(argc==3) { std::ifstream in(argv[1]); std::ofstream out(argv[2]); if(!in || !out) throw std::runtime_error("kernel trace file"); trace(in,out); }
    else if(argc==1) { host_tests(); std::cout<<"Skill kernel/adaptor host checks passed\n"; }
    else throw std::runtime_error("usage: skill-kernel-tests [input output]");
    return 0;
  } catch(const std::exception &e) { std::cerr<<e.what()<<'\n'; return 1; }
}
