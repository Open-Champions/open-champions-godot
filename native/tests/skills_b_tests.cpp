// Input-only runner. Expected state never enters native dispatch.
#include "champions/skills_b.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
using namespace champions::simulation::rta;
namespace SB = champions::simulation::rta::skills_b;
namespace {
std::vector<std::string> split(const std::string &s, char delim) {
  std::vector<std::string> out;
  std::size_t start = 0;
  for (;;) {
    auto end = s.find(delim, start);
    out.push_back(s.substr(start, end == s.npos ? end : end-start));
    if (end == s.npos) return out;
    start = end+1;
  }
}
Raw number(const std::string &s) {
  if (s.empty() || (s.size()>1 && s[0]=='0')) throw std::runtime_error("noncanonical input");
  for (char c : s) if (c < '0' || c > '9') throw std::runtime_error("invalid input number");
  auto n = std::stoull(s);
  if (n > UINT32_MAX) throw std::runtime_error("input beyond raw32");
  return static_cast<Raw>(n);
}
struct CallbackFailure {};
Raw command(SB::Model &m, const std::string &op, const std::vector<Raw> &a) {
  const auto at = [&](unsigned i) { return a.at(i); };
  if (op=="link") return m.handle_link(at(0),at(1),at(2));
  if (op=="push") return m.list_push(at(0),at(1));
  if (op=="remove") return m.handle_remove_node(at(0));
  if (op=="clear") { m.handle_clear(at(0)); return 0; }
  if (op=="begin") { m.iter_begin(at(0),at(1),at(2)); return 0; }
  if (op=="next") { m.iter_next(at(0)); return 0; }
  if (op=="end") { m.iter_end(at(0),at(1)); return 0; }
  if (op=="attach") { m.creature_add_spell(at(0),at(1)); return 0; }
  if (op=="headobj") return m.handle_object(at(0));
  if (op=="peek") return static_cast<Raw>(m.mem.load(at(0),4));
  if (op=="poke") { m.mem.poke(at(0),at(1),at(2)); return 0; }
  if (op=="stat") return m.get_stat(at(0),at(1),at(2));
  if (op=="tier") return m.get_stat_tier2(at(0),at(1),at(2),at(3));
  if (op=="for") return m.get_stat_for(at(0),at(1),at(2),at(3),at(4));
  if (op=="handle") return m.get_stat_for_handle(at(0),at(1),at(2),at(3),at(4));
  if (op=="manacost") return m.mana_cost(at(0),at(1),at(2));
  if (op=="mana") return m.mana_from_stat4(at(0),at(1),at(2));
  if (op=="range") { m.stat_range(at(0),at(1),at(2),at(3),at(4),at(5),at(6)); return 0; }
  if (op=="archery") return m.archery_fraction(at(0),at(1));
  if (op=="lifetap") return m.lifetap_fraction(at(0),at(1));
  if (op=="life1") return m.lifetap_stat1(at(0),at(1));
  if (op=="stance") return m.strongstance_stat0(at(0),at(1));
  if (op=="resist") return m.naturalresistance_fraction(at(0),at(1));
  if (op=="stun") return m.stunningstrike_stat0_for(at(0),at(1),at(2));
  if (op=="ctor") { m.spell_base_ctor(at(0),at(1),at(2),at(3),at(4)); return 0; }
  if (op=="spellrand") return m.spell_rand(at(0));
  if (op=="objectrand") return m.object_rand(at(0));
  if (op=="shared") return m.draw();
  if (op=="cosmetic") return m.cosmetic_draw();
  if (op=="third") return m.seed3_draw();
  if (op=="adjust") return m.adjust_level(at(1),at(2));
  if (op=="can") return m.can_use_base();
  if (op=="cast") return m.cast_base();
  if (op=="cost") return m.mana_cost_base();
  if (op=="virtual") return m.skill_call(at(0),at(1),{a.begin()+2,a.end()});
  throw std::runtime_error("unknown command");
}
std::string execute(const std::string &line) {
  auto fields = split(line,'\t');
  if (fields.size()!=4) throw std::runtime_error("expected four input fields");
  auto config = number(fields[0]);
  if (config>15) throw std::runtime_error("invalid FPU context");
  set_fpu(static_cast<Fpu::Model>(config/4),bool(config&2),bool(config&1));
  SB::Model m;
  if (!fields[1].empty()) for (const auto &r : split(fields[1],';')) {
    auto a = split(r,',');
    if (a.size()!=3) throw std::runtime_error("invalid memory record");
    m.mem.poke(number(a[0]),number(a[1]),number(a[2]));
  }
  if (!fields[2].empty()) for (const auto &r : split(fields[2],';')) {
    auto a = split(r,',');
    if (a.size()!=4) throw std::runtime_error("invalid callback recipe");
    auto ret=number(a[1]), mutation=number(a[2]), throws=number(a[3]);
    m.callbacks[a[0]] = [=](SB::Model &self,const std::vector<Raw> &) {
      if (mutation) self.mem.poke(mutation,4,ret);
      if (throws==1) throw CallbackFailure{};
      if (throws==2) throw SB::Unsupported("explicit missing handler");
      return ret;
    };
  }
  std::vector<Raw> results;
  std::string status="ok";
  try {
    for (const auto &r : split(fields[3],';')) {
      auto a=split(r,','); std::vector<Raw> args;
      for (std::size_t i=1;i<a.size();++i) args.push_back(number(a[i]));
      results.push_back(command(m,a[0],args));
    }
  } catch (const CallbackFailure &) { status="callback"; }
    catch (const SB::Unsupported &) { status="unsupported"; }
  std::ostringstream out;
  out << status << '|';
  for (std::size_t i=0;i<results.size();++i) { if(i) out<<','; out<<results[i]; }
  out << '|' << m.draws << '|';
  bool first=true;
  for (auto [a,b] : m.mem.bytes()) { if(!first) out<<';'; first=false; out<<a<<','<<unsigned(b); }
  out << '|'; first=true;
  for (auto w : m.mem.log()) { if(!first) out<<';'; first=false; out<<w.address<<','<<w.size<<','<<w.value; }
  out << '|'; first=true;
  for (const auto &call : m.calls) {
    if(!first) out<<';'; first=false; out<<call.name;
    for (auto a : call.args) out<<','<<a;
  }
  return out.str();
}
void host_checks() {
  SB::Model m;
  auto require = [](bool value) { if (!value) throw std::runtime_error("host check failed"); };
  auto refuses = [&](auto fn) { bool failed=false; try { fn(); } catch(const SB::Unsupported &) { failed=true; } require(failed); };
  refuses([&]{m.get_stat(0,5,1);});
  refuses([&]{m.get_stat(UINT64_MAX,0,1);});
  refuses([&]{m.spell_rand(UINT64_MAX);});
  m.mem.poke(0xe,2,65535);
  refuses([&]{m.get_stat(0,0,0x7fffffff);});
  m.mem.poke(0x60,4,0x1000);
  refuses([&]{m.skill_call(0,10,{0,1});});
  require(m.calls.size()==1 && m.calls[0].name=="Skill.v10");
  refuses([&]{m.skill_call(0,2,{1,0,0,0});});
  // Cyclic virtual dispatch refuses, and the guard restores depth on failure.
  m.mem.poke(0x1054,4,0x3c87f0);
  m.mem.poke(0x1064,4,0x3c87f0);
  refuses([&]{m.skill_call(0,12,{0,1,0,0});}); // arity prevents bad body/signature dispatch
  m.mem.poke(0x1054,4,0x1234);
  m.callbacks["Skill.v10"] = [](SB::Model &self,const std::vector<Raw> &) -> Raw {
    return self.skill_call(0,10,{0,1});
  };
  refuses([&]{m.skill_call(0,10,{0,1});});
  m.callbacks["Skill.v10"] = [](SB::Model &,const std::vector<Raw> &) { return 7; };
  require(m.skill_call(0,10,{0,1})==7);
  m.mem.poke(0x1054,4,0x3c85fc);
  m.external.insert("Skill.v10");
  require(m.skill_call(0,10,{0,1})==7);
  m.addr_names[0x3c85fc]="renamed";
  m.callbacks["renamed"] = [](SB::Model &,const std::vector<Raw> &) { return 9; };
  require(m.skill_call(0,10,{0,1})==9);
  // Missing bossRank is a refusal, with the reached call recorded.
  m.mem.poke(0x200c,4,0x20);
  refuses([&]{m.stunningstrike_stat0_for(0,0x2000,1);});
  // Traversal bounds cover read-only walks too, without changing valid lists.
  m.mem.poke(0x3000,4,0x4000);
  m.mem.poke(0x4000,4,0x4000);
  m.mem.poke(0x400c,4,0);
  refuses([&]{m.list_objects(0x3000);});
  refuses([&]{m.iter_begin(0x5000,0x3000,0);});
  require(m.mem.load(0x5014,4)==0); // pre-failure mask store is retained
  refuses([&]{m.iter_begin(UINT64_MAX,0,0);});
  refuses([&]{m.creature_add_spell(UINT32_MAX,1);});
  m.creature_add_spell(UINT64_MAX,0); // reference null-spell short circuit

  SB::Model lists;
  lists.mem.poke(0x864ea8,4,0x7000);
  for (Raw i=0;i<12;++i) lists.mem.poke(0x7000+i*0x20,4,i==11 ? 0 : 0x7020+i*0x20);
  lists.creature_add_spell(0x1000,0x2000);
  lists.creature_add_spell(0x1000,0x2100);
  require(lists.list_objects(0x1364)==std::vector<Raw>({0x2100,0x2000}));
  lists.iter_begin(0x6000,0x1364,0);
  lists.handle_clear(0x2104); // currently visited spell removes itself
  lists.creature_add_spell(0x1000,0x2200); // new head waits for next walk
  lists.iter_next(0x6000);
  auto current=lists.mem.load(0x6004,4);
  require(current && lists.mem.load(current+0xc,4)==0x2000);
  lists.iter_next(0x6000);
  require(lists.mem.load(0x6004,4)==0);
  lists.iter_end(0x6000,2);
  require(lists.list_objects(0x1364)==std::vector<Raw>({0x2200,0x2000}));
  lists.handle_clear(0x1364);
  require(lists.list_objects(0x1364).empty());
  require(lists.mem.load(0x2004,4)==0 && lists.mem.load(0x2204,4)==0);
}
}
int main(int argc,char **argv) {
  try {
    if (argc==1) { host_checks(); std::cout<<"skills-b host checks passed\n"; return 0; }
    if (argc!=3) throw std::runtime_error("usage: skills-b-tests INPUT OUTPUT");
    std::ifstream input(argv[1]); std::ofstream output(argv[2]);
    if (!input || !output) throw std::runtime_error("cannot open trace files");
    std::string line; std::size_t count=0;
    while (std::getline(input,line)) { output<<execute(line)<<'\n'; ++count; }
    if (!input.eof() || !output || !count) throw std::runtime_error("incomplete trace");
    std::cout<<count<<" input-driven traces\n";
  } catch(const std::exception &e) { std::cerr<<e.what()<<'\n'; return 1; }
}
