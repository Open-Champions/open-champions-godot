// Executed Minion_run prefix and held Soul placement: research-combat-5
// f33d1a3, docs/spec/minion-place.md, tools/ref_minion.py.
#include "champions/combat_ctors.hpp"

namespace champions::simulation::rta::combat_ctors {
namespace {
A::Value iv(std::uint64_t v) {return std::int64_t(v&0xffffffffu);}
Raw number(const A::Value &v) {
  if (auto p=std::get_if<std::int64_t>(&v)) return Raw(*p);
  if (auto p=std::get_if<A::CallbackResult>(&v))
    if (p->kind==A::CallbackResult::Kind::integer)
      return Raw(p->negative?-std::int64_t(p->magnitude):std::int64_t(p->magnitude));
  throw A::Error("Minion integer callback required");
}
}
bool minion_run_prefix(A::Model &m, Address c) {
  auto u=[&](Address a,unsigned n=4){return Raw(m.mem.load(a,n));};
  auto call=[&](const char *name,std::vector<A::Value> args){
    return m.external_call(name,std::move(args));
  };
  const auto definition=u(c+0x24),model=u(definition+0xc);
  call("Creature_processDamage",{iv(c),iv(model)});
  if (number(call("Creature_run",{iv(c),iv(model)}))) return false;
  call("Creature_updateActive",{iv(c)});
  call("Creature_stepAnim",{iv(c),iv(ONE)});
  call("Creature_applyKnockback",{iv(c),iv(0x3e4ccccc)});
  const auto table=u(c+0x48);
  call("Minion.slot23",{iv(c+Raw(s16(u(table+0xb8,2)))),iv(0)});
  const auto node=u(c+0x3c8);
  if (node) {
    const auto held=u(node+0xc);
    const Vec3 position{u(c+0x10),u(c+0x14),u(c+0x18)};
    auto result=call("model_getAttachPoint",{iv(model),position,
        std::int64_t(s16(u(c+0x38,2))),iv(c+0x114),iv(0),iv(u(c+0xdc))});
    auto sample=std::get_if<A::Sample>(&result);
    if (!sample) throw A::Error("Minion attach point and direction required");
    for (unsigned k=0;k<3;++k) m.store(held+0x10+4*k,4,sample->point[k]);
    m.store(held+0x38,2,u(c+0x38,2));
    call("obj_updateGridCell",{iv(held)});
  }
  const auto state=s16(u(c+0x232,2));
  if (state>=0&&state<0x18)
    throw A::Unsupported("Minion_run state dispatch is not executed");
  return true;
}
} // namespace champions::simulation::rta::combat_ctors
