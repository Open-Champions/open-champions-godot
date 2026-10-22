// Executed prop destructor bodies and anim speed wrapper:
// research-combat-ctors-4 6baf4cd, tools/ref_prop_dtors.py.
#include "champions/combat_ctors.hpp"

namespace champions::simulation::rta::combat_ctors {
namespace {
A::Value iv(std::uint64_t v) { return std::int64_t(v&0xffffffffu); }
}
bool is_prop_dtor(Raw fn) {
  return fn==0x3c2fc8||fn==0x3c4d48||fn==0x3c2d30||
         fn==0x3c2ed8||fn==0x3c31d8;
}
void prop_dtor(A::Model &m,Raw fn,Address prop,Raw flags) {
  if (!is_prop_dtor(fn)) throw A::Unsupported("unknown prop destructor");
  const bool gold=fn==0x3c31d8;
  m.store(prop+0x48,4,gold?0x511a88:0x511d58);
  if (gold) m.external_call("AnimState_dtor",{iv(prop+0x58),iv(2)});
  m.external_call("GameObject_dtor",{iv(prop),iv(0)});
  if (flags&1) m.external_call("pool_free",{iv(prop)});
}
A::Value anim_advance_unit_speed(A::Model &m,Address state,Address sound,
                                 Address root,Address yaw) {
  return m.external_call("anim_advance",{iv(state),iv(sound),iv(root),iv(yaw),iv(ONE)});
}
} // namespace champions::simulation::rta::combat_ctors
