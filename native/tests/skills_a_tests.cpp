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
    // Named service boundaries preserve writes, callback replacement and recursion.
    S::Model buffs;
    refuses([&] { buffs.weaponbuff_cast(12,0x1000,7,0x3000); });
    require(buffs.calls.size()==1 && buffs.calls.back().name=="pool_new");
    buffs.named_callbacks["pool_new"]=[](S::Model &model,const S::Values &args) -> S::Values {
      require(args==S::Values({0x24}));
      model.named_callbacks["pool_new"]=[](S::Model &,const S::Values &) -> S::Values { return {0}; };
      model.mem.store(0x7000,4,17);
      throw std::logic_error("allocation callback");
    };
    try { buffs.weaponbuff_cast(14,0x1000,7,0x3000); require(false); } catch(const std::logic_error &) {}
    require(buffs.mem.load(0x7000,4)==17);
    require(buffs.weaponbuff_cast(14,0x1000,7,0x3000)==0);
    buffs.named_callbacks["pool_new"]=[](S::Model &model,const S::Values &args) { return model.call("pool_new",args); };
    refuses([&] { buffs.weaponbuff_cast(12,0x1000,7,0x3000); });
    buffs.named_callbacks["pool_new"]=[](S::Model &,const S::Values &) -> S::Values { return {}; };
    refuses([&] { buffs.weaponbuff_cast(12,0x1000,7,0x3000); });
    require(buffs.draws==0);
    refuses([&] { buffs.weaponbuff_cast(13,0x1000,7,0x3000); });
    // Cycle guard is a refusal, not a fabricated not-found result.
    buffs.mem.poke(0x3364,4,0x9000); buffs.mem.poke(0x9000,4,0x9000);
    refuses([&] { buffs.find_spell_by_type(0x3000,17); });
    buffs.mem.poke(0x9000,4,0);
    require(buffs.find_spell_by_type(0x3000,17)==0);
    // Type identity callback receives the original spell, not a virtual adjustment.
    buffs.mem.poke(0x8014,4,0x2000); buffs.mem.poke(0x2000,2,-16);
    buffs.named_callbacks["Spell.v0"]=[](S::Model &,const S::Values &args) -> S::Values {
      require(args==S::Values({0x8000})); return {17};
    };
    require(buffs.spell_type(0x8000)==17);
    buffs.type_info[0]=18;
    require(buffs.spell_type(0x8000)==18);
    // Explicit external override of a ported Spell body keeps signed this adjustment.
    buffs.mem.poke(0x2014,4,0x2e5578); buffs.mem.poke(0x2010,2,-16);
    buffs.external_named.insert("Spell.v2");
    buffs.named_callbacks["Spell.v2"]=[](S::Model &,const S::Values &args) -> S::Values {
      require(args==S::Values({0x7ff0,ONE,TWO,0,99})); return {7};
    };
    require(buffs.spell_call(0x8000,2,{ONE,TWO,0,99})==S::Values({7}));
    // Limits preserve prior state and remain recoverable.
    refuses([&] { buffs.spell_call(0x8000,64,{}); });
    buffs.mem.poke(0x8020,2,0xffff);
    buffs.enchant(0x8000,0x18,17,600,600);
    require(buffs.mem.load(0x8020,2)==0xffff); // unrelated bytes untouched
    // Shield allocation and reflection remain explicit engine service boundaries.
    S::Model shield;
    refuses([&] { shield.undeadshield_cast(0x1000,7,0x3000); });
    require(shield.calls.back().name=="pool_new" && shield.calls.back().args==S::Values({0x64}));
    shield.named_callbacks["pool_new"]=[](S::Model &,const S::Values &) -> S::Values { return {0}; };
    require(shield.undeadshield_cast(0x1000,7,0x3000)==0 && shield.draws==0);
    shield.mem.poke(0x8000,1,0);
    shield.undeadshield_damage_attacker(0x8000,0x3000,0x6000,100);
    require(shield.writes.empty());
    refuses([&] { shield.secondary_shield_damage_attacker(0x8000,0x3000,0x6000,100); });
    require(shield.writes.size()==9 && shield.calls.back().name=="damage_apply");
    shield.named_callbacks["damage_apply"]=[](S::Model &model,const S::Values &args) -> S::Values {
      require(args.size()==13 && args[5]==0x40 && args[6]==UINT32_MAX && args[9]==UINT32_MAX);
      model.named_callbacks["damage_apply"]=[](S::Model &,const S::Values &) -> S::Values { return {}; };
      return {};
    };
    shield.secondary_shield_damage_attacker(0x8000,0x3000,0x6000,100);
    shield.secondary_shield_damage_attacker(0x8000,0x3000,0x6000,100);
    // Empty and malformed sample points refuse after the index draw, retaining its state.
    S::Model glow;
    glow.mem.poke(0x800c,4,0x3000);
    for (const auto &name : {"particleGroup_setMatrix","particleGroup_begin","particleGroup_end"})
      glow.named_callbacks[name]=[](S::Model &,const S::Values &) -> S::Values { return {}; };
    glow.named_callbacks["Creature.v16"]=[](S::Model &,const S::Values &) -> S::Values { return {0x9000}; };
    glow.named_callbacks["model_getSamplePointCount"]=[](S::Model &,const S::Values &) -> S::Values { return {1}; };
    glow.named_callbacks["model_getSamplePoint"]=[](S::Model &,const S::Values &) -> S::Values { return {}; };
    refuses([&] { glow.shield_glow(0x8000,0x20); });
    require(glow.writes.size()==5 && glow.mem.load(0x8010,4)!=0);
    glow.named_callbacks["model_getSamplePointCount"]=[](S::Model &,const S::Values &) -> S::Values { return {0}; };
    glow.shield_glow(0x8000,0x20);
    require(glow.writes.size()==9); // zero samples consume no own draw
    std::cout << "Skill API callbacks, failure prefixes and limits PASS\n";
    return 0;
  } catch(const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
