#include "champions/skills_a.hpp"
#include <iostream>
namespace S = champions::simulation::rta::skills_a;
using namespace champions::simulation::rta;
void require_at(bool v, int line) { if (!v) throw std::runtime_error("Skill API check failed at line " + std::to_string(line)); }
#define require(v) require_at((v), __LINE__)
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
    // Root target limits count failed allocations; stack list cleanup is ordered.
    S::Model support;
    support.mem.poke(0x1060,4,0x2000);
    support.external.insert(14); support.external.insert(10);
    support.callbacks[14]=[](S::Model &,const S::Values &a) -> S::Values { return {a.at(1)}; };
    support.callbacks[10]=[](S::Model &,const S::Values &) -> S::Values { return {1}; };
    support.named_callbacks["res_find"]=[](S::Model &,const S::Values &) -> S::Values { return {17}; };
    support.named_callbacks["sound_playOnObject"]=[](S::Model &,const S::Values &) -> S::Values { return {}; };
    support.named_callbacks["world_collectObjectsInArc"]=[](S::Model &,const S::Values &a) -> S::Values {
      require(a==S::Values({ONE,TWO,0,0xffffffff,S::Model::STACK_QUERY,0,0x43780000,0x42f00000,0}));
      return {0x6000,0x7000};
    };
    support.named_callbacks["creature_bossRank"]=[](S::Model &,const S::Values &) -> S::Values { return {0}; };
    unsigned allocations=0, clears=0;
    support.named_callbacks["pool_new"]=[&](S::Model &,const S::Values &a) -> S::Values {
      require(a==S::Values({0x34})); ++allocations; return {0};
    };
    support.named_callbacks["handle_clear"]=[&](S::Model &,const S::Values &a) -> S::Values {
      require(a==S::Values({S::Model::STACK_QUERY})); ++clears; return {};
    };
    require(support.root_cast(0x1000,7,0,{ONE,TWO,0},0xffff)==0);
    require(allocations==1 && clears==2 && support.draws==0);
    // Refuse oversized queries before traversal, then recover with the same model.
    support.named_callbacks["world_collectObjectsInArc"]=[](S::Model &,const S::Values &) -> S::Values { return S::Values(65537,0); };
    refuses([&] { support.root_cast(0x1000,7,0,{ONE,TWO,0},0); });
    support.named_callbacks["world_collectObjectsInArc"]=[](S::Model &,const S::Values &) -> S::Values { return {}; };
    require(support.root_cast(0x1000,7,0,{ONE,TWO,0},0)==0 && clears==4);
    // Root uses modeled signed remainder with zero count, and requires a point.
    S::Model root;
    root.mem.poke(0x801c,4,1); root.mem.poke(0x800c,4,0x3000);
    root.named_callbacks["Creature.v16"]=[](S::Model &,const S::Values &) -> S::Values { return {17}; };
    root.named_callbacks["model_getSamplePointCount"]=[](S::Model &,const S::Values &) -> S::Values { return {0}; };
    root.named_callbacks["model_getSamplePoint"]=[](S::Model &,const S::Values &a) -> S::Values {
      require(a.at(6)==0x3c6e && a.at(7)==1); return {};
    };
    refuses([&] { root.root_spell_run(0x8000); });
    require(root.mem.load(0x8010,4)==0x3c6ef35f && root.draws==0 && root.writes.size()==2);
    // A successful nonplayer root stores only low16 duration*60 and enters phase one.
    S::Model held;
    held.mem.poke(0x8008,4,0x1000); held.mem.poke(0x800c,4,0x3000);
    held.mem.poke(0x8018,4,0x3f04dcca); held.mem.poke(0x8020,4,0x10003);
    held.external.insert(10);
    held.callbacks[10]=[](S::Model &,const S::Values &) -> S::Values { return {101}; };
    for (const auto &name : {"particleGroup_begin","particleGroup_setMatrix","particleGroup_end","particle_spawn"})
      held.named_callbacks[name]=[](S::Model &,const S::Values &) -> S::Values { return {}; };
    held.root_spell_run(0x8000);
    require(held.mem.load(0x3314,2)==180 && held.mem.load(0x801c,4)==1 && held.draws==1);
    // Critical Hit presence is based on class identity, shared across aliases.
    S::Model active;
    active.type_info[0x3cf000]=0x3333;
    require(active.crit_can_use(0x1000,0x3000)==1);
    active.mem.poke(0x3364,4,0xf000); active.mem.poke(0xf00c,4,0x8000);
    active.mem.poke(0x8014,4,0x519c28); active.mem.poke(0x519c2c,4,0x3cf000);
    require(active.crit_can_use(0x1000,0x3000)==0);
    // Critical bonus still adjusts a zero level, allowing an explicit callback to change it.
    S::Model critical;
    critical.mem.poke(0x1060,4,0x2000); critical.external={10,14};
    critical.callbacks[14]=[](S::Model &,const S::Values &a) -> S::Values { require(a==S::Values({0x1000,0,20})); return {7}; };
    critical.callbacks[10]=[](S::Model &,const S::Values &a) -> S::Values { require(a==S::Values({0x1000,0,7})); return {25}; };
    require(critical.crit_bonus_for(0x1000,0x3000,0)==mul(from_int(25),0x3c23d70a));
    critical.mem.poke(0x8018,4,0);
    refuses([&] { critical.crit_spell_run(0x8000); });
    require(critical.calls.back().name=="Spell.v1");
    // Shield Bash: missing animation service preserves the preceding shared writes.
    S::Model bash;
    refuses([&] { bash.shieldbash_ctor(0x8000,257,0x3000,0x1000); });
    require(bash.mem.load(0x8000,1)==1 && bash.mem.load(0x358c,4)==6 && bash.draws==1);
    require(bash.calls.back().name=="res_find");
    bash.named_callbacks["res_find"]=[](S::Model &,const S::Values &) -> S::Values { return {0xa000}; };
    bash.named_callbacks["anim_startBlendInOut"]=[](S::Model &,const S::Values &a) -> S::Values {
      require(a==S::Values({0x3114,0xa000,0x812,0x3dcccccc,0x3dcccccc})); return {};
    };
    bash.named_callbacks["world_collectObjectsInArc"]=[](S::Model &,const S::Values &) -> S::Values { return S::Values(65537,0); };
    refuses([&] { bash.shieldbash_ctor(0x8000,7,0x3000,0x1000); });
    require(bash.mem.load(0xa020,1)==2 && bash.mem.load(0xa01c,4)==0x3f181040);
    bash.mem.poke(0x600c,4,0x1000000);
    bash.named_callbacks["world_collectObjectsInArc"]=[](S::Model &,const S::Values &) -> S::Values { return {0x6000,0x6100}; };
    bash.named_callbacks["handle_clear"]=[](S::Model &,const S::Values &a) -> S::Values { require(a==S::Values({S::Model::STACK_QUERY})); return {}; };
    bash.shieldbash_ctor(0x8000,7,0x3000,0x1000);
    require(bash.mem.load(0x8024,4)==0x6000 && bash.shieldbash_done(0x8000)==0);
    bash.named_callbacks["net_objOrGhost"]=[](S::Model &,const S::Values &) -> S::Values { return {0x3000}; };
    bash.named_callbacks["model_getSamplePointCount"]=[](S::Model &,const S::Values &) -> S::Values { return {0}; };
    bash.named_callbacks["model_getSamplePoint"]=[](S::Model &,const S::Values &a) -> S::Values {
      require(a.size()==10 && a[6]!=0 && a[7]==1 && a[8]==0 && a[9]==ONE); return {};
    };
    const auto own=bash.mem.load(0x8010,4);
    refuses([&] { bash.shieldbash_windup_fx(0x8000); });
    require(bash.mem.load(0x8010,4)!=own);
    bash.named_callbacks["pool_new"]=[](S::Model &,const S::Values &) -> S::Values { return {0x9000}; };
    bash.named_callbacks["LightEffect_ctor"]=[](S::Model &,const S::Values &) -> S::Values { throw std::logic_error("effect failure"); };
    try { bash.light_effect(0xbc,{ONE,TWO,0},{}); require(false); } catch (const std::logic_error &) {}
    require(bash.hidden_draws==0);
    bash.named_callbacks["LightEffect_ctor"]=[](S::Model &,const S::Values &) -> S::Values { return {}; };
    require(bash.light_effect(0xbc,{ONE,TWO,0},{})==0x9000 && bash.hidden_draws==1);
    // Polling dispatch preserves signed16 this adjustment; forced dispatch stays external.
    bash.mem.poke(0x51ad90,2,1); bash.mem.poke(0x51ad94,4,0x2fb67c); bash.mem.poke(0x8019,4,123);
    require(bash.spell_call(0x8000,4,{})==S::Values({123}));
    bash.external_named.insert("Spell.v4");
    refuses([&] { bash.spell_call(0x8000,4,{}); });
    // Fire Storm keeps owner services explicit, and failure preserves its own draws.
    S::Model storm;
    storm.mem.poke(0x8048,4,0x518c98);
    storm.mem.poke(0x518cbc,4,0x2d470c);
    storm.mem.poke(0x44fdcc,4,60);
    storm.mem.poke(0x8090,4,240); storm.mem.poke(0x8094,4,180);
    storm.mem.poke(0x804c,4,0xf000); storm.mem.poke(0xf00c,4,0x3000);
    storm.mem.poke(0x809c,4,24); storm.mem.poke(0x80a8,4,15);
    refuses([&] { storm.firestorm_run(0x8000); });
    require(storm.calls.back().name=="terrain_height" && storm.draws==0 && storm.hidden_draws==0);
    require(storm.mem.load(0x8044,4)!=0 && storm.writes.size()>4);
    // Explicit external dispatch bypasses the native object body.
    storm.external_named.insert("Creature.v4");
    storm.mem.poke(0x518cb8,2,0xffff);
    storm.named_callbacks["Creature.v4"]=[](S::Model &,const S::Values &a) -> S::Values {
      require(a==S::Values({0x7fff})); return {17};
    };
    require(storm.table_call("Creature",0x8000,0x48,4,{})==S::Values({17}));
    // An oversized area query refuses before enemy traversal; a later call recovers.
    storm.mem.poke(0x80a8,4,17);
    storm.named_callbacks["world_collectObjectsInBox"]=[](S::Model &,const S::Values &) -> S::Values { return S::Values(65537,0); };
    refuses([&] { storm.firestorm_run(0x8000); });
    storm.named_callbacks["world_collectObjectsInBox"]=[](S::Model &,const S::Values &) -> S::Values { return {}; };
    storm.firestorm_run(0x8000);
    // The flare callback's hidden draw is counted only after successful return.
    storm.mem.poke(0x80a8,4,15);
    storm.named_callbacks["terrain_height"]=[](S::Model &,const S::Values &) -> S::Values { return {ZERO}; };
    storm.named_callbacks["pool_new"]=[](S::Model &,const S::Values &) -> S::Values { return {0x9000}; };
    storm.named_callbacks["FireStormFlare_ctor"]=[](S::Model &,const S::Values &) -> S::Values { throw std::logic_error("flare failure"); };
    try { storm.firestorm_run(0x8000); require(false); } catch (const std::logic_error &) {}
    require(storm.hidden_draws==0);
    storm.named_callbacks["FireStormFlare_ctor"]=[](S::Model &,const S::Values &a) -> S::Values { require(a.size()==9); return {}; };
    storm.firestorm_run(0x8000); require(storm.hidden_draws==1);
    // Empty owner does not synthesize a lifetime manager; null delete is a no-op.
    storm.mem.poke(0x804c,4,0);
    refuses([&] { storm.firestorm_run(0x8000); });
    require(storm.calls.back().name=="Creature.v1");
    const auto calls_before=storm.calls.size(); storm.delete_self(0);
    require(storm.calls.size()==calls_before);
    // Generic particle callbacks retain order, replacement, and failure prefixes.
    S::Model trail;
    unsigned random_calls=0, ball_calls=0, particles=0;
    const std::function<Raw()> random=[&] { ++random_calls; return 32768u; };
    const std::function<Vec3()> ball=[&] { ++ball_calls; return Vec3{ZERO,ZERO,ZERO}; };
    trail.named_callbacks["particle_spawn"]=[&](S::Model &model,const S::Values &a) -> S::Values {
      ++particles; require(random_calls==1 && ball_calls==2 && a[0]==0x8e5190);
      require(a[1]==ONE && a[2]==TWO && a[3]==ZERO);
      model.named_callbacks["particle_spawn"]=[&](S::Model &,const S::Values &v) -> S::Values {
        ++particles; require(ball_calls==particles*2 && random_calls==particles);
        require(v[0]==(particles==2 ? 0x8e51e0u : 0x8e5230u)); return {};
      };
      return {};
    };
    trail.trail_puffs(random,ball,{ZERO,ZERO,ZERO},{TWO,0x40800000,ZERO});
    require(particles==4 && random_calls==4 && ball_calls==8 && trail.draws==0);
    trail.named_callbacks.erase("particle_spawn");
    refuses([&] { trail.trail_puffs(random,ball,{},{ }); });
    require(random_calls==5 && ball_calls==10);
    // The new query helper bounds results without fabricating an empty query.
    refuses([&] { trail.box_around({ONE,TWO,ZERO},ONE,64); });
    trail.named_callbacks["world_collectObjectsInBox"]=[](S::Model &,const S::Values &) -> S::Values { return S::Values(65537,0); };
    refuses([&] { trail.box_around({},ONE,64); });
    trail.named_callbacks["world_collectObjectsInBox"]=[](S::Model &,const S::Values &) -> S::Values { return {7,7,0}; };
    require(trail.box_around({},ONE,64)==S::Values({7,7,0}));
    // Trail virtual dispatch may be forced external, preserving adjusted this.
    trail.mem.poke(0xa048,4,0x5184f0); trail.mem.poke(0x518510,2,0xffff);
    trail.mem.poke(0x518514,4,0x2cb65c); trail.external_named.insert("Creature.v4");
    trail.named_callbacks["Creature.v4"]=[](S::Model &,const S::Values &a) -> S::Values { require(a==S::Values({0x9fff})); return {13}; };
    require(trail.table_call("Creature",0xa000,0x48,4,{})==S::Values({13}));
    trail.mem.poke(0x8014,4,0x518608); trail.mem.poke(0x518618,2,1);
    trail.mem.poke(0x51861c,4,0x2cc248); trail.external_named.insert("Spell.v2");
    trail.named_callbacks["Spell.v2"]=[](S::Model &,const S::Values &a) -> S::Values { require(a==S::Values({0x8001,ONE,TWO,0})); return {19}; };
    require(trail.spell_call(0x8000,2,{ONE,TWO,0})==S::Values({19}));
    // Projectile services must be supplied, including terrain on an empty query.
    S::Model frost;
    frost.mem.poke(0x801c,4,ONE); frost.mem.poke(0x8020,4,ONE);
    refuses([&] { frost.move_and_collide(0x8000,{ONE,0,0}); });
    require(frost.calls.back().name=="obj_updateGridCell" && frost.writes.size()==3);
    frost.named_callbacks["obj_updateGridCell"]=[](S::Model &,const S::Values &) -> S::Values { return {}; };
    frost.named_callbacks["terrain_height"]=[](S::Model &,const S::Values &) -> S::Values { return {ZERO}; };
    frost.named_callbacks["world_collectObjectsInBox"]=[](S::Model &,const S::Values &) -> S::Values { return S::Values(65537,0); };
    refuses([&] { frost.move_and_collide(0x8000,{}); });
    frost.named_callbacks["world_collectObjectsInBox"]=[](S::Model &,const S::Values &) -> S::Values { return {}; };
    require(frost.move_and_collide(0x8000,{})==0);
    // Re-read geometry after the other object's callback; later duplicates see mutation.
    frost.mem.poke(0x8010,4,0); frost.mem.poke(0x8014,4,0); frost.mem.poke(0x8018,4,0);
    frost.mem.poke(0x601c,4,ONE); frost.mem.poke(0x6020,4,ONE);
    frost.mem.poke(0x6048,4,0x7000); frost.mem.poke(0x7030,2,0xffff);
    frost.named_callbacks["world_collectObjectsInBox"]=[](S::Model &,const S::Values &) -> S::Values { return {0x6000,0x6000}; };
    unsigned contacts=0;
    frost.named_callbacks["Creature.v6"]=[&](S::Model &model,const S::Values &a) -> S::Values {
      ++contacts;
      if (contacts==1) {
        require(a==S::Values({0x5fff,0x8000}));
        model.mem.poke(0x8010,4,0x42c80000); // Second overlap is now far away.
      } else require(a==S::Values({0x8000,0x6000}));
      return {};
    };
    require(frost.move_and_collide(0x8000,{})==2 && contacts==2);
    // Native/forced-external hit dispatch and signed adjustment.
    frost.mem.poke(0x8048,4,0x51a8c8); frost.mem.poke(0x51a8f8,2,0xffff);
    frost.mem.poke(0x51a8fc,4,0x2f71ac); frost.mem.poke(0x806f,4,1);
    require(frost.table_call("Creature",0x8000,0x48,6,{0x6000}).empty());
    frost.external_named.insert("Creature.v6");
    frost.named_callbacks["Creature.v6"]=[](S::Model &,const S::Values &a) -> S::Values { require(a==S::Values({0x7fff,0x6000})); return {19}; };
    require(frost.table_call("Creature",0x8000,0x48,6,{0x6000})==S::Values({19}));
    // Generic puff consumes one ball without spread, two even with zero spread.
    unsigned balls=0;
    const std::function<Vec3()> puff_ball=[&] { ++balls; return Vec3{ONE,TWO,ZERO}; };
    frost.named_callbacks["particle_spawn"]=[](S::Model &,const S::Values &a) -> S::Values {
      require(a==S::Values({7,ONE,TWO,ZERO,ONE,TWO,ZERO,ZERO})); return {};
    };
    frost.puff(puff_ball,7,{ONE,TWO,0},std::nullopt,ONE); require(balls==1);
    frost.puff(puff_ball,7,{ONE,TWO,0},ZERO,ONE); require(balls==3);
    frost.named_callbacks.erase("particle_spawn");
    refuses([&] { frost.puff(puff_ball,7,{},ONE,ONE); }); require(balls==5);
    // Debris counts its hidden draw only after the external constructor returns.
    frost.mem.poke(0x8044,4,0xffffffff);
    frost.named_callbacks["pool_new"]=[](S::Model &,const S::Values &) -> S::Values { return {0x9000}; };
    frost.named_callbacks["PhProp_ctor"]=[](S::Model &,const S::Values &) -> S::Values { throw std::logic_error("debris failure"); };
    // Choose an authored seed whose first draw requests at least one prop.
    Raw debris_seed=0;
    while (((debris_seed*0x19660du+0x3c6ef35fu)>>16)<16384) ++debris_seed;
    frost.mem.poke(0x8044,4,debris_seed);
    try { frost.frost_debris(0x8000,0x809c); require(false); } catch (const std::logic_error &) {}
    require(frost.hidden_draws==0 && frost.mem.load(0x8044,4)!=debris_seed);
    frost.mem.poke(0x8044,4,debris_seed);
    unsigned debris_parts=0;
    frost.named_callbacks["PhProp_ctor"]=[&](S::Model &,const S::Values &a) -> S::Values { require(a.size()==11 && a[7]==(1u<<debris_parts++)); return {}; };
    frost.named_callbacks["PhProp_initPhysics"]=[](S::Model &,const S::Values &) -> S::Values { return {}; };
    frost.frost_debris(0x8000,0x809c); require(frost.hidden_draws>=1);
    // Fire Flares destructor snapshots the following node before deletion callbacks.
    // Empty objects are skipped and callbacks may replace themselves synchronously.
    S::Model flares;
    flares.mem.poke(0x8018,4,0xf000); flares.mem.poke(0xf000,4,0xf100);
    flares.mem.poke(0xf00c,4,0x9000); flares.mem.poke(0xf100,4,0xf200);
    flares.mem.poke(0xf10c,4,0); flares.mem.poke(0xf20c,4,0xa000);
    unsigned deleted=0;
    flares.named_callbacks["Creature.v1"]=[&](S::Model &model,const S::Values &a) -> S::Values {
      require(a==S::Values({0x9000,3})); ++deleted;
      model.mem.poke(0xf000,4,0); // Already-snapshotted next survives this mutation.
      model.named_callbacks["Creature.v1"]=[&](S::Model &,const S::Values &b) -> S::Values {
        require(b==S::Values({0xa000,3})); ++deleted; return {};
      }; return {};
    };
    for (const auto *name:{"sound_stop","light_unlink","light_dtor","handle_clear","pool_free"})
      flares.named_callbacks[name]=[](S::Model &,const S::Values &) -> S::Values { return {}; };
    flares.fireflares_dtor(0x8000,3);
    require(deleted==2 && flares.mem.load(0x8014,4)==0x515ba0 && flares.mem.load(0x8074,4)==UINT32_MAX);
    require(flares.calls.back().name=="pool_free" && flares.calls.back().args==S::Values({0x8000}));
    flares.mem.poke(0x8018,4,0); flares.named_callbacks.erase("light_dtor");
    refuses([&] { flares.fireflares_dtor(0x8000,0); });
    require(flares.calls.back().name=="light_dtor" && flares.mem.load(0x8014,4)==0x5194f8);
    // Cycles refuse without rolling back the preceding light state or callbacks.
    flares.mem.poke(0x8018,4,0xf000); flares.mem.poke(0xf000,4,0xf000); flares.mem.poke(0xf00c,4,0);
    refuses([&] { flares.fireflares_dtor(0x8000,0); });
    flares.mem.poke(0x8018,4,0);
    flares.named_callbacks["light_dtor"]=[](S::Model &,const S::Values &) -> S::Values { return {}; };
    flares.fireflares_dtor(0x8000,0); // Recovery after traversal refusal.
    // Signed this adjustment and explicit forced external destruction/run paths.
    flares.mem.poke(0x8014,4,0x5194f8); flares.mem.poke(0x519500,2,0xffff);
    flares.mem.poke(0x519504,4,0x2dd0c8); flares.external_named.insert("Spell.v1");
    flares.named_callbacks["Spell.v1"]=[](S::Model &,const S::Values &a) -> S::Values { require(a==S::Values({0x7fff,3})); return {17}; };
    require(flares.spell_call(0x8000,1,{3})==S::Values({17}));
    flares.mem.poke(0x9048,4,0x5193e0); flares.mem.poke(0x519400,2,1);
    flares.mem.poke(0x519404,4,0x2dbf10); flares.external_named.insert("Creature.v4");
    flares.named_callbacks["Creature.v4"]=[](S::Model &,const S::Values &a) -> S::Values { require(a==S::Values({0x9001})); return {23}; };
    require(flares.table_call("Creature",0x9000,0x48,4,{})==S::Values({23}));
    // Burst's two particles share point/velocity; there are no draws between them.
    flares.mem.poke(0x9058,4,1); flares.mem.poke(0x9054,4,0x1000);
    flares.mem.poke(0x9050,4,7); flares.mem.poke(0x905e,2,0); flares.mem.poke(0x901c,4,0x41c00000);
    flares.external.insert(10);
    flares.callbacks[10]=[](S::Model &,const S::Values &a) -> S::Values { require(a==S::Values({0x1000,1,7})); return {38}; };
    std::uint64_t seed_at_particle=0; S::Values first_particle;
    flares.named_callbacks["particle_spawn"]=[&](S::Model &model,const S::Values &a) -> S::Values {
      first_particle=a; require(a[0]==0x8e59b0); seed_at_particle=model.mem.load(0x9044,4);
      model.named_callbacks["particle_spawn"]=[&](S::Model &m,const S::Values &b) -> S::Values {
        require(b[0]==0x8e5a00 && S::Values(b.begin()+1,b.end())==S::Values(first_particle.begin()+1,first_particle.end()));
        require(m.mem.load(0x9044,4)==seed_at_particle); throw std::logic_error("burst failure");
      }; return {};
    };
    try { flares.fireflare_run(0x9000); require(false); } catch (const std::logic_error &) {}
    require(flares.mem.load(0x905e,2)==7 && flares.calls.back().name=="particle_spawn");
    // Native flare queries retain the existing explicit result bound.
    flares.mem.poke(0x9058,4,0); flares.mem.poke(0x44fdcc,4,10);
    flares.named_callbacks["particle_spawn"]=[](S::Model &,const S::Values &) -> S::Values { return {}; };
    flares.named_callbacks["world_collectObjectsInBox"]=[](S::Model &,const S::Values &) -> S::Values { return S::Values(65537,0); };
    refuses([&] { flares.fireflare_run(0x9000); });
    flares.named_callbacks["world_collectObjectsInBox"]=[](S::Model &,const S::Values &) -> S::Values { return {}; };
    flares.fireflare_run(0x9000);
    // Homing projectile arc services are explicit. Refusal retains constructor
    // writes and seed, and an oversized candidate list never invokes isEnemy.
    S::Model homing;
    homing.named_callbacks["obj_getPropInt"]=[](S::Model &,const S::Values &) -> S::Values { return {0}; };
    homing.named_callbacks["handle_link"]=[](S::Model &,const S::Values &) -> S::Values { return {}; };
    homing.named_callbacks["obj_runEveryFrame"]=[](S::Model &,const S::Values &) -> S::Values { return {}; };
    homing.named_callbacks["res_find"]=[](S::Model &,const S::Values &) -> S::Values { return {17}; };
    homing.named_callbacks["sound_playOnObject"]=[](S::Model &,const S::Values &) -> S::Values { return {}; };
    refuses([&] { homing.diseasebolt_ctor(0x8000,32767,65537,{ONE,TWO,ZERO},0x3000,0x1000); });
    require(homing.calls.back().name=="world_collectObjectsInArc" && homing.draws==1);
    require(homing.mem.load(0x8060,2)==1 && homing.mem.load(0x8070,4)==300);
    homing.named_callbacks["world_collectObjectsInArc"]=[](S::Model &,const S::Values &) -> S::Values { return S::Values(65537,0); };
    refuses([&] { homing.diseasebolt_ctor(0x8000,0,7,{},0x3000,0x1000); });
    require(homing.calls.back().name=="world_collectObjectsInArc");
    // Callback replacement preserves candidate order and links only the first enemy.
    homing.named_callbacks["world_collectObjectsInArc"]=[](S::Model &,const S::Values &a) -> S::Values {
      require(a==S::Values({ZERO,ZERO,ZERO,ZERO,S::Model::STACK_QUERY,0,0x438bfffc,0x42700000,ZERO}));
      return {0x6000,0x6100,0x6200};
    };
    unsigned enemies=0,links=0,clears_homing=0;
    homing.named_callbacks["Creature.v20"]=[&](S::Model &model,const S::Values &a) -> S::Values {
      require(a==S::Values({0x3000,0x6000})); ++enemies;
      model.named_callbacks["Creature.v20"]=[&](S::Model &,const S::Values &v) -> S::Values {
        require(v==S::Values({0x3000,0x6100})); ++enemies; return {1};
      }; return {0};
    };
    homing.named_callbacks["handle_link"]=[&](S::Model &model,const S::Values &a) -> S::Values {
      if (a[0]==0x8058) { ++links; require(a==S::Values({0x8058,0x6100,0x6100})); model.mem.poke(0x8058,4,0xf100); }
      return {};
    };
    homing.named_callbacks["handle_clear"]=[&](S::Model &,const S::Values &a) -> S::Values {
      require(a==S::Values({S::Model::STACK_QUERY})); ++clears_homing; return {};
    };
    homing.diseasebolt_ctor(0x8000,0,7,{},0x3000,0x1000);
    require(enemies==2 && links==1 && clears_homing==1 && homing.mem.load(0x8058,4)==0xf100);
    // Holy Strike's arc owns a list at +0x58; its return value is ignored and it
    // never fabricates target nodes or clears the constructor's owned list.
    homing.mem.poke(0x300c,4,0x20);
    homing.named_callbacks["trail_create"]=[](S::Model &,const S::Values &a) -> S::Values {
      require(a==S::Values({1,0xfa,0,0xfa,200,100,0x80,0x3e4ccccc})); return {29};
    };
    homing.named_callbacks["trail_addPoint"]=[](S::Model &,const S::Values &a) -> S::Values {
      require(a==S::Values({29,ONE,TWO,ZERO})); return {};
    };
    homing.named_callbacks["world_collectObjectsInArc"]=[](S::Model &model,const S::Values &a) -> S::Values {
      require(a==S::Values({ONE,TWO,ZERO,0xffff8000,0x8058,0x3000,0x43b40000,0x42960000,ZERO}));
      model.mem.poke(0x8058,4,0xf200); return {0xdead};
    };
    homing.holystrike_ctor(0x8000,0x8000,7,{ONE,TWO,ZERO},0x3000,0x1000);
    require(homing.mem.load(0x8058,4)==0xf200 && homing.mem.load(0x807c,4)==29 && clears_homing==1);
    // No-owner Disease hit sets has-hit BEFORE damage; failure preserves that
    // prefix. Holy only sets it after a successful damage callback.
    S::Model hit;
    hit.mem.poke(0x805c,4,0x1000); hit.mem.poke(0x8060,2,7); hit.external.insert(13);
    hit.callbacks[13]=[](S::Model &,const S::Values &a) -> S::Values {
      require(a==S::Values({0x1000,0,7,0x804c,0})); return {65537};
    };
    refuses([&] { hit.diseasebolt_hit(0x8000,0x6000); });
    require(hit.mem.load(0x8078,4)==1 && hit.mem.load(0x8044,4)==0x3c6ef35f);
    require(hit.calls.back().name=="damage_apply" && hit.calls.back().args[6]==0x26 && hit.calls.back().args[7]==1);
    S::Model holy_hit;
    holy_hit.mem.poke(0x8068,4,0x1000); holy_hit.mem.poke(0x805c,2,7); holy_hit.external.insert(10);
    holy_hit.callbacks[10]=[](S::Model &,const S::Values &) -> S::Values { return {10}; };
    refuses([&] { holy_hit.holystrike_hit(0x8000,0x6000); });
    require(holy_hit.mem.load(0x8078,4)==0 && holy_hit.mem.load(0x6040,2)==0);
    unsigned hits=0;
    holy_hit.named_callbacks["damage_apply"]=[&](S::Model &model,const S::Values &a) -> S::Values {
      require(a.size()==13 && a[5]==0x80000 && a[6]==UINT32_MAX); ++hits;
      // Reenter on a different projectile, through a replacement callback.
      model.named_callbacks["damage_apply"]=[&](S::Model &,const S::Values &) -> S::Values { ++hits; return {}; };
      model.mem.poke(0x9068,4,0x1000); model.mem.poke(0x905c,2,7);
      model.holystrike_hit(0x9000,0x6100); return {};
    };
    holy_hit.holystrike_hit(0x8000,0x6000);
    require(hits==2 && holy_hit.mem.load(0x8078,4)==1 && holy_hit.mem.load(0x9078,4)==1);
    require(holy_hit.mem.load(0x6090,2)==0xff && holy_hit.mem.load(0x6098,2)==0xff);
    // All new object/Skill bodies preserve explicit external overrides and this.
    for (Raw body:{0x2caec8u,0x2ecae8u}) {
      holy_hit.mem.poke(0x8048,4,0x7000); holy_hit.mem.poke(0x7030,2,0xffff); holy_hit.mem.poke(0x7034,4,body);
      holy_hit.mem.poke(0x8077,4,1);
      require(holy_hit.table_call("Creature",0x8000,0x48,6,{0x6000}).empty());
      holy_hit.external_named.insert("Creature.v6");
      holy_hit.named_callbacks["Creature.v6"]=[](S::Model &,const S::Values &a) -> S::Values { require(a==S::Values({0x7fff,0x6000})); return {19}; };
      require(holy_hit.table_call("Creature",0x8000,0x48,6,{0x6000})==S::Values({19}));
      holy_hit.external_named.erase("Creature.v6");
    }
    // Blinding Light examines only the first arc entry, including friend/zero.
    // It still flashes a null caster and clears the stack list on normal return.
    S::Model blind;
    refuses([&] { blind.blindinglight_cast(0x1000,7,0,{ONE,TWO,ZERO},0x8000); });
    require(blind.calls.back().name=="gfx_setColorOffset");
    require(blind.mem.load(0x90,2)==0x56 && blind.mem.load(0x98,2)==0x56);
    blind.named_callbacks["gfx_setColorOffset"]=[](S::Model &,const S::Values &a) -> S::Values {
      require(a==S::Values({128,128,128,0})); return {};
    };
    blind.named_callbacks["world_collectObjectsInArc"]=[](S::Model &,const S::Values &a) -> S::Values {
      require(a==S::Values({ONE,TWO,ZERO,0xffff8000,S::Model::STACK_QUERY,0,0x43b40000,0x42b40000,ZERO}));
      return {0x6000,0x6100};
    };
    blind.named_callbacks["res_find"]=[](S::Model &,const S::Values &) -> S::Values { return {71}; };
    blind.named_callbacks["sound_playOnObject"]=[](S::Model &,const S::Values &) -> S::Values { return {}; };
    unsigned blind_enemies=0,blind_clears=0;
    blind.named_callbacks["Creature.v20"]=[&](S::Model &,const S::Values &a) -> S::Values {
      require(a==S::Values({0x3000,0x6000})); ++blind_enemies; return {0};
    };
    blind.named_callbacks["handle_clear"]=[&](S::Model &,const S::Values &a) -> S::Values {
      require(a==S::Values({S::Model::STACK_QUERY})); ++blind_clears; return {};
    };
    require(blind.blindinglight_cast(0x1000,7,0x3000,{ONE,TWO,ZERO},0x8000)==0);
    require(blind_enemies==1 && blind_clears==1 && blind.draws==0);
    require(blind.blindinglight_cast(0x1000,7,0,{ONE,TWO,ZERO},0x8000)==0);
    require(blind_enemies==1 && blind_clears==2);
    // A successful chance with allocation failure still sets the no-target timer.
    // Replaced enemy callbacks retain ordered duplicate box results.
    blind.external.insert(10);
    blind.callbacks[10]=[](S::Model &,const S::Values &a) -> S::Values {
      return {a[1]==0 ? 101u : a[1]==2 ? 3u : 2u};
    };
    blind.named_callbacks["handle_link"]=[](S::Model &,const S::Values &) -> S::Values { return {}; };
    blind.named_callbacks["obj_getPropInt"]=[](S::Model &,const S::Values &) -> S::Values { return {0}; };
    blind.named_callbacks["world_collectObjectsInBox"]=[](S::Model &,const S::Values &) -> S::Values {
      return {0x6000,0x6000};
    };
    blind.named_callbacks["Creature.v20"]=[](S::Model &model,const S::Values &) -> S::Values {
      model.named_callbacks["Creature.v20"]=[](S::Model &,const S::Values &) -> S::Values { return {1}; };
      return {1};
    };
    blind.named_callbacks["creature_bossRank"]=[](S::Model &,const S::Values &) -> S::Values { return {0}; };
    blind.named_callbacks["pool_new"]=[](S::Model &,const S::Values &) -> S::Values { return {0}; };
    unsigned blind_timers=0;
    blind.named_callbacks["Creature.v31"]=[&](S::Model &,const S::Values &a) -> S::Values {
      require(a==S::Values({0x6000,180})); ++blind_timers; return {};
    };
    blind.named_callbacks["obj_runEveryFrame"]=[](S::Model &,const S::Values &) -> S::Values { return {}; };
    blind.blindinglight_effect_ctor(0x8000,{ONE,TWO,ZERO},7,0x3000,0x1000);
    require(blind_timers==2 && blind.draws==1 && blind.hidden_draws==0);
    // Particle failure preserves exactly the first four own draws. Replacement
    // uses the next particle callback, with no shared draw consumed.
    blind.mem.poke(0x804c,4,90); blind.mem.poke(0x44fdcc,4,30); blind.mem.poke(0x8044,4,1);
    refuses([&] { blind.blindinglight_effect_run(0x8000); });
    Raw blind_seed=1;
    for (unsigned i=0;i<4;++i) blind_seed=blind_seed*0x19660du+0x3c6ef35fu;
    require(blind.mem.load(0x8044,4)==blind_seed && blind.calls.back().name=="particle_spawn");
    unsigned blind_particles=0;
    blind.named_callbacks["particle_spawn"]=[&](S::Model &model,const S::Values &) -> S::Values {
      ++blind_particles;
      model.named_callbacks["particle_spawn"]=[&](S::Model &,const S::Values &) -> S::Values { ++blind_particles; return {}; };
      model.mem.poke(0x8050,4,0); return {};
    };
    blind.blindinglight_effect_run(0x8000);
    for (unsigned i=0;i<48;++i) blind_seed=blind_seed*0x19660du+0x3c6ef35fu;
    require(blind_particles==12 && blind.mem.load(0x8044,4)==blind_seed && blind.draws==1);
    // Periodic damage rereads victim and raw caster after a stat callback.
    S::Model blind_damage;
    blind_damage.external.insert(10);
    blind_damage.mem.poke(0x44fdcc,4,30); blind_damage.mem.poke(0x9000,1,255);
    blind_damage.mem.poke(0x9008,4,0x1000); blind_damage.mem.poke(0x900c,4,0x6000);
    blind_damage.mem.poke(0x9018,4,210); blind_damage.mem.poke(0x901c,4,0x3000);
    blind_damage.mem.poke(0x6228,4,ONE);
    blind_damage.callbacks[10]=[](S::Model &model,const S::Values &a) -> S::Values {
      require(a==S::Values({0x1000,1,0xffffffff}));
      model.mem.poke(0x900c,4,0x6100); model.mem.poke(0x901c,4,0xdead); return {Raw(-3)};
    };
    refuses([&] { blind_damage.blindinglight_spell_run(0x9000); });
    require(blind_damage.calls.back().args[0]==0x6100 && blind_damage.calls.back().args[1]==0xdead);
    require(blind_damage.calls.back().args[11]==0xbf800000 && blind_damage.mem.load(0x9010,4)==0x3c6ef35f);
    // The spell expiry wins over damage and can be forced through an external body.
    blind_damage.mem.poke(0x9014,4,0x51a6f8); blind_damage.mem.poke(0x51a70c,4,0x2f4538);
    blind_damage.mem.poke(0x9018,4,30);
    refuses([&] { blind_damage.spell_call(0x9000,2,{}); });
    require(blind_damage.calls.back().name=="Spell.v1");
    blind_damage.external_named.insert("Spell.v2");
    blind_damage.mem.poke(0x51a708,2,0xffff);
    blind_damage.named_callbacks["Spell.v2"]=[](S::Model &,const S::Values &a) -> S::Values {
      require(a==S::Values({0x8fff})); return {17};
    };
    require(blind_damage.spell_call(0x9000,2,{})==S::Values({17}));
    // Mandatory scalar results and object/Skill external dispatch remain explicit.
    blind.named_callbacks["Creature.v20"]=[](S::Model &,const S::Values &) -> S::Values { return {}; };
    refuses([&] { blind.blindinglight_effect_ctor(0x8000,{},7,0x3000,0x1000); });
    require(blind.calls.back().name=="Creature.v20");
    blind.mem.poke(0x8048,4,0x51a5e0); blind.mem.poke(0x51a600,2,0xffff);
    blind.mem.poke(0x51a604,4,0x2f42e4); blind.external_named.insert("Creature.v4");
    blind.named_callbacks["Creature.v4"]=[](S::Model &,const S::Values &a) -> S::Values {
      require(a==S::Values({0x7fff})); return {23};
    };
    require(blind.table_call("Creature",0x8000,0x48,4,{})==S::Values({23}));
    blind.mem.poke(0x1060,4,0x2000); blind.mem.poke(0x2008,2,0xffff);
    blind.mem.poke(0x200c,4,0x2f4b10); blind.external.insert(1);
    blind.callbacks[1]=[](S::Model &,const S::Values &a) -> S::Values {
      require(a==S::Values({0xfff,0x3000})); return {29};
    };
    require(blind.skill_call(0x1000,1,{0x3000})==S::Values({29}));
    // Damage callback replacement permits reentry on another live spell.
    blind_damage.mem.poke(0x9018,4,210); blind_damage.mem.poke(0x900c,4,0x6000);
    blind_damage.callbacks[10]=[](S::Model &,const S::Values &) -> S::Values { return {35}; };
    unsigned blind_hits=0;
    blind_damage.named_callbacks["damage_apply"]=[&](S::Model &model,const S::Values &) -> S::Values {
      ++blind_hits;
      model.named_callbacks["damage_apply"]=[&](S::Model &,const S::Values &) -> S::Values { ++blind_hits; return {}; };
      model.mem.poke(0x9100,1,7); model.mem.poke(0x9108,4,0x1000);
      model.mem.poke(0x910c,4,0x6000); model.mem.poke(0x9118,4,210);
      model.blindinglight_spell_run(0x9100); return {};
    };
    blind_damage.blindinglight_spell_run(0x9000);
    require(blind_hits==2 && blind_damage.mem.load(0x9110,4)==0x3c6ef35f);
    // Disease status: missing presentation keeps the first swarm update, then
    // replacement/reentry permits expiry of a distinct spell during the next run.
    S::Model disease;
    disease.mem.poke(0x44fdcc,4,30); disease.mem.poke(0x91a0,4,300);
    disease.mem.poke(0x919c,4,0x6000); disease.mem.poke(0x601c,4,ONE);
    refuses([&] { disease.disease_spell_run(0x9000,{ONE,TWO,ZERO}); });
    require(disease.calls.back().name=="glow_add" && disease.writes.size()>6);
    unsigned glows=0,deletes=0;
    disease.named_callbacks["Spell.v1"]=[&](S::Model &,const S::Values &a) -> S::Values {
      require(a==S::Values({0xa000,3})); ++deletes; return {};
    };
    disease.named_callbacks["glow_add"]=[&](S::Model &model,const S::Values &a) -> S::Values {
      require(a.size()==12 && a[6]==200); ++glows;
      model.named_callbacks["glow_add"]=[&](S::Model &,const S::Values &) -> S::Values { ++glows; return {}; };
      model.disease_spell_run(0xa000,{}); return {};
    };
    disease.disease_spell_run(0x9000,{ONE,TWO,ZERO});
    require(glows==16 && deletes==1 && disease.mem.load(0x6098,2)==255);
    // Periodic damage uses full stored level and rereads its handle after the
    // statistic callback, rather than keeping the original source/victim.
    disease.mem.poke(0x9018,4,0xf000); disease.mem.poke(0xf00c,4,0x3000);
    disease.mem.poke(0x91a8,4,65537); disease.mem.poke(0x91ac,4,0x1000);
    disease.external.insert(12);
    disease.callbacks[12]=[](S::Model &model,const S::Values &a) -> S::Values {
      require(a==S::Values({0x1000,0,65537,0x3000,0}));
      model.mem.poke(0x9018,4,0); model.mem.poke(0x919c,4,0x6100); return {Raw(-2)};
    };
    unsigned disease_hits=0;
    disease.named_callbacks["damage_apply"]=[&](S::Model &,const S::Values &a) -> S::Values {
      require(a.size()==13 && a[0]==0x6100 && a[1]==0 && a[5]==0xc040 && a[11]==ONE);
      ++disease_hits; return {};
    };
    disease.disease_spell_run(0x9000,{}); require(disease_hits==1);
    // Unknown identities refuse; zero allocations never invent attachments.
    refuses([&] { disease.diseaseshield_can_use(0x1000,0x3000); });
    disease.named_callbacks["pool_new"]=[](S::Model &,const S::Values &) -> S::Values { return {0}; };
    require(disease.diseaseshield_cast(0x1000,7,0)==0);
    require(disease.mem.load(0x98,2)==0xff9c);
    // Oversized query refusal and recovery after the complete swarm prefix.
    disease.mem.poke(0x800c,4,0x3000); disease.mem.poke(0x81a4,4,300);
    disease.named_callbacks["world_collectObjectsInBox"]=[](S::Model &,const S::Values &) -> S::Values { return S::Values(65537,0); };
    refuses([&] { disease.diseaseshield_run(0x8000,{}); });
    disease.named_callbacks["world_collectObjectsInBox"]=[](S::Model &,const S::Values &) -> S::Values { return {}; };
    disease.diseaseshield_run(0x8000,{});
    // Forced external spell dispatch retains signed this adjustment and lanes.
    disease.mem.poke(0x8014,4,0x5186c0); disease.mem.poke(0x5186d0,2,0xffff);
    disease.mem.poke(0x5186d4,4,0x2cd318); disease.external_named.insert("Spell.v2");
    disease.named_callbacks["Spell.v2"]=[](S::Model &,const S::Values &a) -> S::Values {
      require(a==S::Values({0x7fff,ONE,TWO,ZERO,9})); return {17};
    };
    require(disease.spell_call(0x8000,2,{ONE,TWO,ZERO,9})==S::Values({17}));
    {
    // Fire lifecycles require services; no draw before failed allocation.
    S::Model fire;
    refuses([&] { fire.fireweapons_cast(0x1000,7,0x3000); });
    refuses([&] { fire.coneoffire_cast(0x1000,7,0x3000); });
    require(fire.draws==0 && fire.calls.back().args==S::Values({0x58}));
    fire.named_callbacks["pool_new"]=[](S::Model &,const S::Values &) -> S::Values { return {0}; };
    require(fire.fireweapons_cast(0x1000,7,0x3000)==0 && fire.coneoffire_cast(0x1000,7,0x3000)==0);
    refuses([&] { fire.fireweapons_can_use(0x1000,0x3000); });
    // Constructor failure retains Spell seed and light writes, without hiding draws.
    fire.named_callbacks["light_link"]=[](S::Model &model,const S::Values &a) -> S::Values {
      require(a==S::Values({0x8018})); model.mem.poke(0x7000,4,99);
      throw std::logic_error("light service");
    };
    try { fire.coneoffire_ctor(0x8000,255,0x3000,0x1000); require(false); } catch(const std::logic_error &) {}
    require(fire.draws==1 && fire.hidden_draws==0 && fire.mem.load(0x7000,4)==99);
    require(fire.mem.load(0x8014,4)==0x51a470 && fire.mem.load(0x8040,4)==div(ONE,ZERO));
    fire.mem.poke(0x1060,4,0x2000); fire.mem.poke(0x44fdcc,4,30);
    fire.external.insert(12);
    fire.callbacks[12]=[](S::Model &,const S::Values &a) -> S::Values {
      require(a[2]==0xffffffff); return {a[1]==1 ? 5u : 4u};
    };
    unsigned particles=0,hits=0,clears=0;
    fire.named_callbacks["particle_spawn"]=[&](S::Model &model,const S::Values &a) -> S::Values {
      require(a.size()==8); ++particles;
      require(model.coneoffire_can_use(0,0)==1);
      model.named_callbacks["particle_spawn"]=[&](S::Model &,const S::Values &) -> S::Values { ++particles; return {}; };
      return {};
    };
    fire.named_callbacks["world_collectObjectsInArc"]=[](S::Model &,const S::Values &a) -> S::Values {
      require(a.size()==9 && a[4]==S::Model::STACK_QUERY && a[6]==0x42c00000);
      return {0x6000,0x6000};
    };
    fire.named_callbacks["Creature.v20"]=[](S::Model &,const S::Values &) -> S::Values { return {1}; };
    fire.named_callbacks["damage_apply"]=[&](S::Model &model,const S::Values &a) -> S::Values {
      // Preserve the reference's reversed range: low 1, high 0.
      require(a.size()==16 && a[0]==0x6000 && a[6]==0x15 && a[7]==1 && a[8]==0);
      ++hits; model.mem.poke(0x7004,4,hits); return {};
    };
    fire.named_callbacks["res_find"]=[](S::Model &,const S::Values &) -> S::Values { return {71}; };
    fire.named_callbacks["sound_playAtPos"]=[](S::Model &,const S::Values &) -> S::Values { return {73}; };
    fire.named_callbacks["handle_clear"]=[&](S::Model &,const S::Values &a) -> S::Values {
      require(a==S::Values({S::Model::STACK_QUERY})); ++clears; return {};
    };
    fire.coneoffire_run(0x8000,{ONE,TWO,ZERO},32768);
    require(particles==28 && hits==2 && clears==1 && fire.draws==1);
    // Query bound refuses after the complete particle prefix and recovers.
    fire.named_callbacks["world_collectObjectsInArc"]=[](S::Model &,const S::Values &) -> S::Values { return S::Values(65537,0); };
    refuses([&] { fire.coneoffire_run(0x8000,{}); });
    require(particles==56 && clears==1);
    fire.named_callbacks["world_collectObjectsInArc"]=[](S::Model &,const S::Values &) -> S::Values { return {}; };
    fire.coneoffire_run(0x8000,{}); require(particles==84 && clears==2);
    // Explicit override of either ported run retains virtual this adjustment.
    fire.external_named.insert("Spell.v2"); fire.mem.poke(0x51a480,2,0xffff); fire.mem.poke(0x51a484,4,0x2f02fc);
    fire.named_callbacks["Spell.v2"]=[](S::Model &,const S::Values &a) -> S::Values {
      require(a==S::Values({0x7fff,ONE,TWO,ZERO,17})); return {9};
    };
    require(fire.spell_call(0x8000,2,{ONE,TWO,ZERO,17})==S::Values({9}));
    }
    {
      S::Model beam;
      beam.mem.poke(0x1060,4,0x2000); beam.mem.poke(0x2054,4,0x3c85fc);
      for (unsigned stat=0;stat<4;++stat) {
        beam.mem.poke(0x100e + 2*stat,2,1);
        beam.mem.poke(0x1018+2*stat,2,stat==0 ? 135 : stat==1 ? 8 : stat==2 ? 2 : 4);
      }
      refuses([&] { beam.wizardbeam_cast(0x1000,255,0x3000); });
      beam.named_callbacks["pool_new"]=[](S::Model &,const S::Values &a)->S::Values { require(a==S::Values({0x380}));return {0}; };
      require(beam.wizardbeam_cast(0x1000,255,0x3000)==0 && beam.draws==0);
      // Constructor callback replacement and reentry preserve active callable.
      unsigned created=0;
      beam.named_callbacks["trail_create"]=[&](S::Model &model,const S::Values &a)->S::Values {
        require(a.size()==8 && a[0]==1 && a[1]==200);
        model.named_callbacks["trail_create"]=[&](S::Model &,const S::Values &)->S::Values { ++created;return {900+created}; };
        require(model.beam_flash(0x8000,0).size()==2);
        ++created;return {900+created};
      };
      beam.named_callbacks["res_find"]=[](S::Model &,const S::Values &)->S::Values { return {5}; };
      beam.named_callbacks["sound_playAtPos"]=[](S::Model &,const S::Values &)->S::Values { return {}; };
      beam.wizardbeam_ctor(0x8000,255,0x3000,0x1000);
      require(beam.draws==1 && beam.hidden_draws==0 && created==9);
      require(beam.mem.load(0x8000,1)==255 && beam.mem.load(0x80b8,4)==0);
      require(beam.mem.load(0x8010,4)!=beam.mem.load(S::G_RAND_SEED,4));
      // A malformed scalar response refuses at the precise constructor prefix.
      beam.named_callbacks["trail_create"]=[](S::Model &,const S::Values &)->S::Values { return {}; };
      const auto prior=beam.draws;
      refuses([&] { beam.wizardbeam_ctor(0x9000,128,0x3000,0x1000); });
      require(beam.draws==prior+1 && beam.mem.load(0x9014,4)==0x51a5b0);
      // No-target callbacks are mandatory; excessive query/count domains refuse.
      beam.named_callbacks["world_collectObjectsInBox"]=[](S::Model &,const S::Values &)->S::Values { return {}; };
      refuses([&] { beam.wizardbeam_run(0x8000,{}); });
      beam.named_callbacks["trail_clear_191e58"]=[](S::Model &,const S::Values &)->S::Values { return {}; };
      beam.named_callbacks["trail_setPoints_1918a0"]=[](S::Model &,const S::Values &a)->S::Values {
        require(a.size()==6 && a[1]==S::Model::STACK_QUERY && a[2]==6); return {};
      };
      beam.mem.poke(0x44fdcc,4,0); beam.wizardbeam_run(0x8000,{});
      beam.named_callbacks["world_collectObjectsInBox"]=[](S::Model &,const S::Values &)->S::Values { return S::Values(65537,0); };
      refuses([&] { beam.wizardbeam_run(0x8000,{}); });
      beam.named_callbacks["world_collectObjectsInBox"]=[](S::Model &,const S::Values &)->S::Values { return {}; };
      beam.mem.poke(0x80b8,4,65537); refuses([&] { beam.wizardbeam_run(0x8000,{}); });
      beam.mem.poke(0x80b8,4,0);
      beam.named_callbacks["curve_sample_1911e8"]=[](S::Model &,const S::Values &)->S::Values { return {}; };
      beam.named_callbacks["ring_draw_1920e0"]=[](S::Model &,const S::Values &)->S::Values { return {}; };
      beam.wizardbeam_run(0x8000,{}); // all guards recover
      refuses([&] { beam.beam_chain(0x8028,0x8070,65537,{ONE,0,0},0x8348); });
      // Forced virtual override applies signed16 this adjustment before callback.
      beam.mem.poke(0x51a5c0,2,0xffff);beam.mem.poke(0x51a5c4,4,0x2f1a44);
      beam.external_named.insert("Spell.v2");
      beam.named_callbacks["Spell.v2"]=[](S::Model &,const S::Values &a)->S::Values {
        require(a==S::Values({0x7fff,ONE,TWO,0,65535}));return {};
      };
      beam.spell_call(0x8000,2,{ONE,TWO,0,65535});
    }
    std::cout << "Skill API callbacks, failure prefixes and limits PASS\n";
    return 0;
  } catch(const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
