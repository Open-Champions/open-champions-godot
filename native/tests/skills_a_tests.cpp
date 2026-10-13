#include "champions/skills_a.hpp"
#include <iostream>
namespace S = champions::simulation::rta::skills_a;
using namespace champions::simulation::rta;
void require(bool v) { if (!v) throw std::runtime_error("Skill API check failed"); }
template<class F> void refuses(F f) {
  bool failed=false; try { f(); } catch(const S::Unsupported &) { failed=true; }
  require(failed);
}
int main() {
  try {
    S::Model m;
    m.mem.poke(0x1060,4,0x2000);
    // Forced callbacks are explicit; no-handler call records failure prefix.
    m.external.insert(10);
    refuses([&] { m.skill_call(0x1000,10,{0,20}); });
    require(m.calls.size()==1 && m.calls[0].args==S::Values({0x1000,0,20}));
    // Callback exception leaves its mutation and unwinds dispatch depth.
    m.callbacks[10]=[](S::Model &model,const S::Values &) -> S::Values {
      model.mem.store(0x7000,4,123); throw std::logic_error("callback");
    };
    try { m.skill_call(0x1000,10,{0,20}); require(false); } catch(const std::logic_error &) {}
    require(m.mem.load(0x7000,4)==123 && m.mem.log().size()==1);
    // Copy-before-call supports replacing the active callback itself.
    m.callbacks[10]=[](S::Model &model,const S::Values &) -> S::Values {
      model.callbacks[10]=[](S::Model &,const S::Values &) -> S::Values { return {8}; };
      return {7};
    };
    require(m.skill_call(0x1000,10,{0,20})==S::Values({7}));
    require(m.skill_call(0x1000,10,{0,20})==S::Values({8}));
    m.callbacks[10]=[](S::Model &model,const S::Values &) { return model.skill_call(0x1000,10,{0,20}); };
    refuses([&] { m.skill_call(0x1000,10,{0,20}); });
    m.callbacks[10]=[](S::Model &,const S::Values &) -> S::Values { return {9}; };
    require(m.skill_call(0x1000,10,{0,20})==S::Values({9}));
    refuses([&] { m.skill_call(0x1000,16,{}); });
    refuses([&] { m.mana_cost_at(0x6000,0x1000,20); });
    // Signed index with early level-zero exit; otherwise invalid addresses refuse.
    require(m.skill_stat(0,0xffffffff,0)==0);
    refuses([&] { m.skill_stat(0,0x80000000,1); });
    m.callbacks[10]=[](S::Model &,const S::Values &) -> S::Values { return {}; };
    refuses([&] { m.skill_stat_range(0x1000,0,1,1,ONE); });
    require(m.draws==0);
    // The range API preserves the selected FPU context, including exceptions.
    set_fpu(Fpu::Model::Nearest,true,true);
    const auto before=from_int(0x01000003);
    try { m.skill_mana_from_stat4(0x1000,20,0); } catch(const S::Unsupported &) {}
    require(from_int(0x01000003)==before);
    set_fpu();
    std::cout << "Skill API callbacks, failure prefixes and limits PASS\n";
    return 0;
  } catch(const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
