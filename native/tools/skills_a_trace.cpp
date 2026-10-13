#include "champions/skills_a.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
using namespace champions::simulation::rta;
namespace S = champions::simulation::rta::skills_a;
namespace {
std::vector<std::string> split(const std::string &s, char c) {
  std::vector<std::string> out; std::istringstream in(s); std::string x;
  while (std::getline(in, x, c)) out.push_back(x);
  return out;
}
Raw number(const std::string &s) {
  if (s.empty() || s.size() > 8 || s.find_first_not_of("0123456789abcdef") != std::string::npos)
    throw std::runtime_error("invalid hex input");
  return Raw(std::stoul(s, nullptr, 16));
}
S::Values nums(const std::string &s) {
  S::Values out; if (s == "-") return out;
  for (const auto &v : split(s, ',')) out.push_back(number(v));
  return out;
}
std::string enc(const S::Values &v) {
  if (v.empty()) return "-";
  std::ostringstream out; out << std::hex << std::setfill('0');
  for (std::size_t i=0;i<v.size();++i) { if (i) out << ','; out << std::setw(8) << v[i]; }
  return out.str();
}
std::string state(const S::Model &m) {
  std::ostringstream out; bool first=true;
  for (const auto &b : m.mem.bytes()) { if (!first) out << ';'; first=false; out << enc({b.first,b.second}); }
  if (first) out << '-';
  const auto writes = [&](const auto &log) {
    out << '|'; bool begin=true;
    for (const auto &w : log) { if (!begin) out << ';'; begin=false; out << enc({w.address,Raw(w.size),Raw(w.value)}); }
    if (begin) out << '-';
  };
  writes(m.writes); writes(m.mem.log()); out << '|'; first=true;
  for (const auto &c : m.calls) { if (!first) out << ';'; first=false; out << c.slot << ':' << enc(c.args); }
  if (first) out << '-';
  out << '|' << enc({Raw(m.draws),Raw(m.hidden_draws)});
  return out.str();
}
S::Values execute(S::Model &m, const std::string &op, const S::Values &a) {
  const auto at = [&](std::size_t i) { return a.at(i); };
  if (op=="call") return m.skill_call(at(0),at(1),S::Values(a.begin()+2,a.end()));
  if (op=="passive_cast") return {m.passive_cast(at(0))};
  if (op=="dodge_fraction") return {m.dodge_fraction(at(0),at(1))};
  if (op=="regeneration_stat0") return {m.regeneration_stat0(at(0),at(1))};
  if (op=="skill_stat") return {m.skill_stat(at(0),at(1),at(2))};
  if (op=="skill_stat_tier2") return {m.skill_stat_tier2(at(0),at(1),at(2),at(3))};
  if (op=="skill_stat_for") return {m.skill_stat_for(at(0),at(1),at(2),at(3),at(4))};
  if (op=="skill_stat_for_handle") return {m.skill_stat_for_handle(at(0),at(1),at(2),at(3),at(4))};
  if (op=="skill_mana_from_stat4") return {m.skill_mana_from_stat4(at(0),at(1),at(2))};
  if (op=="skill_stat_range") return m.skill_stat_range(at(0),at(1),at(2),at(3),at(4));
  if (op=="skill_init_stats_base") { m.skill_init_stats_base(at(0)); return {}; }
  if (op=="skill_can_use_base") return {m.skill_can_use_base(at(0))};
  if (op=="skill_cast_base") return {m.skill_cast_base(at(0))};
  if (op=="skill_mana_base") return {m.skill_mana_base(at(0))};
  if (op=="skill_adjust_level") return {m.skill_adjust_level(at(0),at(1),at(2))};
  if (op=="skill_by_id") return {m.skill_by_id(at(0))};
  throw std::runtime_error("unknown operation");
}
}
int main(int argc, char **argv) {
  try {
    if (argc != 3) throw std::runtime_error("usage: skills-a-trace inputs.tsv actual.tsv");
    std::ifstream in(argv[1]); std::ofstream out(argv[2]);
    if (!in || !out) throw std::runtime_error("trace file open failed");
    std::string line; std::size_t count=0;
    while (std::getline(in,line)) {
      if (line.empty() || line[0]=='#') continue;
      if (line.size()>1024*1024) throw std::runtime_error("line limit");
      const auto f=split(line,'\t'); if (f.size()!=6) throw std::runtime_error("expected six INPUT fields");
      const auto config=std::stoul(f[1]); if (config>15) throw std::runtime_error("FPU config");
      set_fpu(Fpu::Model(config/4), bool(config&2), bool(config&1));
      S::Model m;
      if (f[3]!="-") for (const auto &p : split(f[3],';')) {
        const auto v=nums(p); if (v.size()!=3) throw std::runtime_error("memory patch");
        m.mem.poke(v[0],v[1],v[2]);
      }
      if (f[4]!="-") for (const auto &s : split(f[4],';')) {
        const auto v=nums(s); if (v.size()!=6) throw std::runtime_error("callback script");
        m.callbacks[v[0]]=[v](S::Model &model,const S::Values &) -> S::Values {
          if (v[2]) model.mem.poke(v[3],v[4],v[5]);
          if (v[2]==2) throw std::logic_error("authored callback failure");
          return {v[1]};
        };
      }
      for (auto slot : nums(f[5])) m.external.insert(slot);
      std::string result;
      try { result=enc(execute(m,f[0],nums(f[2]))); }
      catch (const S::Unsupported &) { result="unsupported"; }
      catch (const std::logic_error &) { result="callback-error"; }
      out << result << '\t' << state(m) << '\n'; ++count;
    }
    if (!in.eof() || !out) throw std::runtime_error("trace IO failed");
    std::cout << count << " input-driven state records\n";
    return 0;
  } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
