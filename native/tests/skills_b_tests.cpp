// Input-only runner. Expected state never enters native dispatch.
#include "champions/skills_b.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
// Model::alloca is the port's method name; a platform alloca macro is
// dropped in this translation unit only, as skills_b.cpp does.
#ifdef alloca
#undef alloca
#endif
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
  if (op=="f32") return m.f32(at(0));
  if (op=="player") return m.is_player(at(0));
  if (op=="half") return m.half_toward_zero(at(0));
  if (op=="stat14") return m.stat_via_slot14(at(0),at(1),at(2),at(3));
  if (op=="factory") return m.mana_cost_method(at(0),at(1))(m,at(2),at(3));
  if (op=="cstrlen") return static_cast<Raw>(m.cstr(at(0),s32(at(1))).size());
  if (op=="cstrbyte") {
    auto text=m.cstr(at(0),s32(at(1)));
    return at(2)<text.size() ? static_cast<unsigned char>(text[at(2)]) : Raw(256);
  }
  static const std::array<std::string,4> classes={"Skill","Spell","Creature","Other"};
  if (op=="route") return m.virtual_call(static_cast<SB::Model::VirtualClass>(at(0)),at(1),at(2),at(3),{a.begin()+4,a.end()});
  if (op=="table") return m.table_call(classes.at(at(0)),at(1),at(2),at(3),{a.begin()+4,a.end()});
  if (op=="extskill") { m.external.insert("Skill.v"+std::to_string(at(0))); return 0; }
  if (op=="emptyname") { m.addr_names[at(0)]=""; return 0; }
  if (op=="call") {
    static const std::array<std::string,5> names={"handle_link","handle_clear","handle_removeNode","synthetic","typed"};
    std::vector<SB::Model::Argument> args(a.begin()+1,a.end());
    return std::get<Raw>(m.call(names.at(at(0)),std::move(args)));
  }
  if (op=="lifecast") return m.lifetap_cast(at(0),at(1),at(2),at(3),at(4));
  if (op=="stancecast") return m.strongstance_cast(at(0),at(1),at(2),at(3),at(4));
  if (op=="stancetrigger") { m.strongstance_trigger(at(0),at(1),at(2),at(3)); return 0; }
  if (op=="stuneffect") { m.stunningstrike_effect(at(0),at(1)); return 0; }
  if (op=="shadowcast") return m.shadowstep_cast(at(0),at(1),at(2),at(3),at(4));
  if (op=="shadowctor") { m.shadowstep_spell_ctor(at(0),at(1),at(2),at(3)); return 0; }
  if (op=="shadowrun") { m.shadowstep_run(at(0),at(1),at(2)); return 0; }
  if (op=="windcast") return m.planarwind_cast(at(0),at(1),at(2),at(3),at(4));
  if (op=="windctor") { m.planarwind_spell_ctor(at(0),at(1),at(2),at(3),at(4)); return 0; }
  if (op=="windrun") { m.planarwind_run(at(0)); return 0; }
  if (op=="windstrike") { m.planarwind_strike(at(0),at(1),{at(2),at(3),at(4)}); return 0; }
  if (op=="sparkcast") return m.shockoffire_cast(at(0),at(1),at(2),at(3),at(4));
  if (op=="sparkctor") { m.sparks_ctor(at(0),at(1),at(2),at(3),at(4),at(5),at(6),at(7),at(8),at(9)); return 0; }
  if (op=="sparkrun") { m.sparks_run(at(0)); return 0; }
  if (op=="sparktouch") { m.sparks_on_touch(at(0),at(1)); return 0; }
  if (op=="darkcast") return m.darkness_cast(at(0),at(1),at(2),at(3),at(4));
  if (op=="darkctor") { m.darkness_ctor(at(0),at(1),at(2),at(3),at(4),at(5)); return 0; }
  if (op=="darkrun") { m.darkness_run(at(0)); return 0; }
  if (op=="ambient") return m.ambient_bytes().at(at(0));
  if (op=="darkbox") return static_cast<Raw>(m.darkness_box(at(0)).size());
  if (op=="harmcan") return m.harmtouch_can_use(at(0),at(1));
  if (op=="harmcast") return m.harmtouch_cast(at(0),at(1),at(2),at(3),at(4));
  if (op=="harmctor") { m.harmtouch_spell_ctor(at(0),at(1),at(2),at(3)); return 0; }
  if (op=="harmtint") { m.harmtouch_tint(at(0),at(1)); return 0; }
  if (op=="harmrun") { m.harmtouch_run(at(0),at(1),at(2)); return 0; }
  if (op=="harmhit") { m.harmtouch_apply_to_hit(at(0),at(1),at(2),at(3)); return 0; }
  if (op=="poundcan") return m.groundpound_can_use(at(0),at(1));
  if (op=="poundcast") return m.groundpound_cast(at(0),at(1),at(2),at(3),at(4));
  if (op=="poundctor") { m.groundpound_effect_ctor(at(0),at(1),at(2),at(3),at(4),at(5)); return 0; }
  if (op=="poundstat") return m.groundpound_stat(at(0),at(1),at(2));
  if (op=="poundrun") { m.groundpound_effect_run(at(0)); return 0; }
  if (op=="down") return m.down(at(0));
  if (op=="crem") return m.c_rem(at(0),at(1));
  if (op=="frostcast") return m.froststorm_cast(at(0),at(1),at(2),at(3),at(4));
  if (op=="frostctor") { m.froststorm_ctor(at(0),at(1),at(2),at(3),at(4),at(5),at(6)); return 0; }
  if (op=="frostrun") { m.froststorm_run(at(0)); return 0; }
  if (op=="frostpoint") return m.froststorm_point(at(0),at(1)).at(at(2));
  if (op=="shardparticle") { m.iceshard_particle(at(0),at(1)); return 0; }
  if (op=="shardrun") { m.iceshard_run(at(0)); return 0; }
  if (op=="shockcast") return m.shock_cast(at(0),at(1),at(2),at(3),at(4));
  if (op=="shockctor") { m.shock_ctor(at(0),at(1),at(2),at(3),at(4),at(5)); return 0; }
  if (op=="shockchain") { m.shock_build_chain(at(0),at(1)); return 0; }
  if (op=="shockin") return m.shock_in_chain(at(0),at(1));
  if (op=="shockwants") return m.shock_wants(at(0),at(1),at(2));
  if (op=="shockrun") { m.shock_run(at(0)); return 0; }
  if (op=="bowcan") return m.bow_can_use(at(0),at(1));
  if (op=="arrowcan") return m.arrow_can_use(at(0),at(1),at(2));
  if (op=="arrowcast") return m.arrow_cast(at(0),at(1),at(2),at(3),at(4),at(5));
  if (op=="arrowctor") { m.arrow_ctor(at(0),at(1),at(2),at(3),at(4),at(5),at(6)); return 0; }
  if (op=="arrowctor2") { m.arrow_ctor2(at(0),at(1),at(2),at(3),at(4),at(5),at(6)); return 0; }
  if (op=="arrowrun") { m.arrow_run(at(0),at(1)); return 0; }
  if (op=="arrowhit") { m.arrow_on_hit(at(0),at(1),at(2)); return 0; }
  if (op=="arrowend") { m.arrow_end(at(0),at(1),at(2)); return 0; }
  if (op=="multican") return m.multifire_can_use(at(0),at(1));
  if (op=="multicast") return m.multifire_cast(at(0),at(1),at(2),at(3),at(4));
  if (op=="arrowlight") { m.arrow_light(at(0),m.vec(at(1)),at(2),at(3),{at(4),at(5),at(6)}); return 0; }
  if (op=="arrowvf") { m.arrow_velocity_facing(at(0),at(1)); return 0; }
  if (op=="arrowvd") { m.arrow_velocity_dir(at(0),m.vec(at(1)),at(2)); return 0; }
  if (op=="arrowblocked") return m.arrow_blocked_for_player(at(0));
  if (op=="arrowcommon") { m.arrow_common(at(0),at(1),at(2),at(3),at(4)); return 0; }
  if (op=="firestats") return m.firearrow_stats(at(0),at(1));
  if (op=="arrowfinish") { m.arrow_finish(at(0),at(1),at(2)); return 0; }
  if (op=="monsterspread") return m.monster_spread(at(0),at(1)).at(at(2));
  if (op=="flightparticle") { m.flight_particle(at(0),at(1),at(2),at(3)); return 0; }
  if (op=="arrowparticles") { m.arrow_particles(at(0),at(1)); return 0; }
  if (op=="arrowflight") { m.arrow_flight(at(0),at(1),at(2)); return 0; }
  if (op=="conecast") return m.coneoffrost_cast(at(0),at(1),at(2),at(3),at(4));
  if (op=="conector") { m.coneoffrost_spell_ctor(at(0),at(1),at(2),at(3)); return 0; }
  if (op=="conerun") { m.coneoffrost_run(at(0),at(1),at(2)); return 0; }
  if (op=="dummyctor") { m.dummydamager_ctor(at(0),at(1),at(2),at(3),at(4),at(5),at(6),at(7),at(8),at(9)); return 0; }
  if (op=="dummyrun") { m.dummydamager_run(at(0)); return 0; }
  if (op=="dummytouch") { m.dummydamager_on_touch(at(0),at(1)); return 0; }
  if (op=="localvec") return static_cast<Raw>(m.local_vec(m.vec(at(0))));
  if (op=="objball") return m.object_ball(at(0)).at(at(1));
  if (op=="cyclonecan") return m.cyclone_can_use(at(0),at(1));
  if (op=="cyclonecast") return m.cyclone_cast(at(0),at(1),at(2),at(3),at(4));
  if (op=="cyclonector") { m.cyclone_spell_ctor(at(0),at(1),at(2),at(3)); return 0; }
  if (op=="cyclonerun") { m.cyclone_run(at(0),at(1),at(2)); return 0; }
  if (op=="cyclonefx") { m.cyclone_update_fx(at(0),m.vec(at(1))); return 0; }
  if (op=="revivecan") return m.resurrect_can_use(at(0),at(1));
  if (op=="revivetarget") return m.resurrect_find_target(at(0),at(1));
  if (op=="reviveraised") return m.resurrect_is_being_raised(at(0),at(1));
  if (op=="revivecast") return m.resurrect_cast(at(0),at(1),at(2));
  if (op=="revivector") { m.resurrect_spell_ctor(at(0),at(1),at(2),at(3),at(4)); return 0; }
  if (op=="reviverun") { m.resurrect_run(at(0)); return 0; }
  if (op=="revivedeath") return m.resurrect_on_owner_death(at(0));
  if (op=="bashcan") return m.bash_can_use(at(0),at(1));
  if (op=="bashcast") return m.bash_cast(at(0),at(1),at(2));
  if (op=="bashctor") { m.bash_spell_ctor(at(0),at(1),at(2),at(3)); return 0; }
  if (op=="bashwind") { m.bash_windup_fx(at(0)); return 0; }
  if (op=="bashrun") { m.bash_run(at(0)); return 0; }
  if (op=="bashdone") { m.bash_done(at(0),at(1)); return 0; }
  if (op=="critcan") return m.criticalhit_can_use(at(0),at(1));
  if (op=="critcast") return m.criticalhit_cast(at(0),at(1),at(2));
  if (op=="critctor") { m.criticalhit_spell_ctor(at(0),at(1),at(2),at(3)); return 0; }
  if (op=="crittint") { m.criticalhit_set_tint(at(0),at(1)); return 0; }
  if (op=="critrun") { m.criticalhit_run(at(0)); return 0; }
  if (op=="critbonus") return m.criticalhit_bonus_for(at(0),at(1),at(2));
  if (op=="chargecast") return m.charge_cast(at(0),at(1),at(2));
  if (op=="chargector") { m.charge_spell_ctor(at(0),at(1),at(2),at(3)); return 0; }
  if (op=="chargerun") { m.charge_run(at(0)); return 0; }
  if (op=="chargestat") return m.charge_stat3_scaled(at(0));
  if (op=="chargecontact") { m.charge_on_contact(at(0),at(1)); return 0; }
  if (op=="bashrange") return m.bash_range(at(0)).at(at(1));
  if (op=="rangehelper") return m.stat_range(at(0),at(1),at(2),at(3),at(4)).at(at(5));
  if (op=="trailpoint") return m.charge_trail_point(at(0),{at(1),at(2)},at(3)).at(at(4));
  if (op=="uintfloat") return m.from_uint(at(0));
  if (op=="ancestralcan") return m.ancestralcall_can_use(at(0),at(1));
  if (op=="ancestralcast") return m.ancestralcall_cast(at(0),at(1),at(2));
  if (op=="ancestralctor") { m.ancestralcall_spell_ctor(at(0),at(1),at(2),at(3)); return 0; }
  if (op=="ancestralrun") { m.ancestralcall_run(at(0),at(1)); return 0; }
  if (op=="ancestraltint") { m.ancestralcall_tint_and_glow(at(0),at(1)); return 0; }
  if (op=="ancestralstat0") return m.ancestralcall_stat0_fraction(at(0));
  if (op=="ancestralstat1") return m.ancestralcall_stat1_fraction(at(0));
  if (op=="ancestralquery") return m.ancestralcall_query_for(at(0),at(1),at(2),at(3));
  if (op=="growthcan") return m.giantgrowth_can_use(at(0),at(1));
  if (op=="growthcast") return m.giantgrowth_cast(at(0),at(1),at(2));
  if (op=="growthctor") { m.giantgrowth_spell_ctor(at(0),at(1),at(2),at(3)); return 0; }
  if (op=="growthrun") { m.giantgrowth_run(at(0)); return 0; }
  if (op=="growthstep") { m.giantgrowth_step_scale(at(0),at(1)); return 0; }
  if (op=="growthemit") { m.giantgrowth_emit(at(0),at(1)); return 0; }
  if (op=="growthscale") return m.giantgrowth_owner_scale(at(0));
  if (op=="enemycan") return m.convertenemy_can_use(at(0),at(1));
  if (op=="undeadcan") return m.convertundead_can_use(at(0),at(1));
  if (op=="enemycast") return m.convertenemy_cast(at(0),at(1),at(2));
  if (op=="undeadcast") return m.convertundead_cast(at(0),at(1),at(2));
  if (op=="enemyctor") { m.convertenemy_spell_ctor(at(0),at(1),at(2),at(3)); return 0; }
  if (op=="undeadctor") { m.convertundead_spell_ctor(at(0),at(1),at(2),at(3)); return 0; }
  if (op=="enemyrun") { m.convertenemy_run(at(0)); return 0; }
  if (op=="undeadrun") { m.convertundead_run(at(0)); return 0; }
  if (op=="convertcast") return m.convert_cast(at(0),at(1),at(2),at(3));
  if (op=="convertctor") { m.convert_spell_ctor(at(0),at(1),at(2),at(3),at(4)); return 0; }
  if (op=="convertrun") { m.convert_run(at(0),at(1)); return 0; }
  if (op=="convertword") { m.convert_word(at(0),at(1)); return 0; }
  if (op=="fill") { m.list_fill(at(0),{a.begin()+1,a.end()}); return 0; }
  if (op=="queryinto") { m.query_into(at(0),"synthetic_query",{}); return 0; }
  if (op=="querylist") return static_cast<Raw>(m.query_list("synthetic_query",{}));
  if (op=="minorcast") return m.minorhealing_cast(at(0),at(1),at(2));
  if (op=="minorctor") { m.minorhealing_spell_ctor(at(0),at(1),at(2),at(3)); return 0; }
  if (op=="minorrun") { m.minorhealing_run(at(0)); return 0; }
  if (op=="shieldcast") return m.holyshield_cast(at(0),at(1),at(2));
  if (op=="shieldctor") { m.holyshield_spell_ctor(at(0),at(1),at(2),at(3)); return 0; }
  if (op=="shieldrun") { m.holyshield_run(at(0)); return 0; }
  if (op=="absorb") return m.holyshield_absorb(at(0));
  if (op=="healcast") return m.healing_cast(at(0),at(1),at(2));
  if (op=="healctor") { m.healing_spell_ctor(at(0),at(1),at(2),at(3)); return 0; }
  if (op=="healrun") { m.healing_run(at(0)); return 0; }
  if (op=="healshare") return m.heal_share(at(0),at(1));
  if (op=="record") return m.record(at(0));
  if (op=="unit") return m.unit(at(0));
  if (op=="signedunit") return m.signed_unit(at(0));
  if (op=="helperstat") return m.stat(at(0),at(1),at(2));
  if (op=="helperfor") return m.stat_for(at(0),at(1),at(2),at(3),at(4));
  if (op=="update") { m.creature_update_spells(at(0),at(1)); return 0; }
  if (op=="finish") { m.spell_end(at(0)); return 0; }
  if (op=="gamector") { m.game_object_ctor(at(0),at(1),at(2),at(3)); return 0; }
  if (op=="projectile") return m.projectile_ctor(at(0),at(1),at(2),at(3));
  if (op=="move") return m.obj_move_and_collide(at(0),at(1));
  if (op=="objects") return static_cast<Raw>(m.list_objects(at(0)).size());
  if (op=="alloc") return static_cast<Raw>(m.alloca(at(0)));
  if (op=="spellvirtual") return m.spell_call(at(0),at(1),{a.begin()+2,a.end()});
  if (op=="objvirtual") return m.vcall(at(0),at(1),{a.begin()+2,a.end()});
  if (op=="external") { m.external.insert(std::string(at(0) ? "Creature" : "Spell")+".v"+std::to_string(at(1))); return 0; }
  if (op=="namebody") { m.addr_names[at(0)]="named"; return 0; }
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
  SB::Memory initial;
  if (!fields[1].empty()) for (const auto &r : split(fields[1],';')) {
    auto a = split(r,',');
    if (a.size()!=3) throw std::runtime_error("invalid memory record");
    initial.poke(number(a[0]),number(a[1]),number(a[2]));
  }
  SB::Model m(std::move(initial));
  if (!fields[2].empty()) for (const auto &r : split(fields[2],';')) {
    auto a = split(r,',');
    if (a.size()!=4 && a.size()!=6) throw std::runtime_error("invalid callback recipe");
    auto mutation=number(a[2]), throws=number(a[3]);
    Raw action=a.size()==6 ? number(a[4]) : 0, operand=a.size()==6 ? number(a[5]) : 0;
    auto token=a[1];
    auto cursor=std::make_shared<std::size_t>(0);
    auto fn=[=](SB::Model &self,const std::vector<SB::Model::Argument> &args) -> SB::Model::Result {
      SB::Model::Result value;
      if (token.substr(0,2)=="g:") {
        auto p=split(token.substr(2),':');
        if (p.size()!=13) throw std::runtime_error("invalid growth sample recipe");
        Mat3 matrix; for (unsigned i=0;i<9;++i) matrix[i]=number(p[i+4]);
        value=SB::Model::GrowthSample{{number(p[0]),number(p[1]),number(p[2])},{number(p[3])},matrix};
      } else if (token.substr(0,2)=="p:") {
        auto point=split(token.substr(2),':');
        if (point.size()!=4) throw std::runtime_error("invalid point callback recipe");
        value=SB::Model::SamplePoint{{number(point[0]),number(point[1]),number(point[2])},number(point[3])};
      } else if (token.substr(0,2)=="s:") value=token.substr(2);
      else if (token.substr(0,2)=="l:") {
        std::vector<Raw> list;
        if (token.size()>2) for (const auto &n:split(token.substr(2),':')) list.push_back(number(n));
        value=std::move(list);
      } else if (token.substr(0,2)=="q:") {
        auto seq=split(token.substr(2),':');
        value=number(seq[std::min((*cursor)++,seq.size()-1)]);
      } else value=number(token);
      if (mutation) self.mem.poke(mutation,4,std::get<Raw>(value));
      if (throws==1) throw CallbackFailure{};
      if (throws==2) throw SB::Unsupported("explicit missing handler");
      Raw object=args.empty() || !std::holds_alternative<Raw>(args[0]) ? 0 : std::get<Raw>(args[0]);
      if (action==1) self.spell_end(object);
      if (action==2) self.handle_clear(SB::Address(object)+4);
      if (action==3) self.creature_add_spell(0x30000,operand);
      if (action==4) self.mem.store(operand,4,std::get<Raw>(value));
      if (action==5) {
        Raw seed=static_cast<Raw>(self.mem.load(SB::G_RAND_SEED,4))*1664525u+1013904223u;
        self.mem.store(SB::G_RAND_SEED,4,seed);
        self.mem.store(SB::Address(object)+0x44,4,seed>>16);
      }
      if (action==6) self.handle_clear(operand);
      return value;
    };
    m.typed_callbacks[a[0]]=fn;
    m.callbacks[a[0]]=[fn](SB::Model &self,const std::vector<Raw> &args) {
      return std::get<Raw>(fn(self,{args.begin(),args.end()}));
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
  for (const auto &call : m.events) {
    if(!first) out<<';'; first=false; out<<call.name; if (call.args.empty()) out<<',';
    for (const auto &a : call.args) {
      out<<',';
      if (const auto *v=std::get_if<Raw>(&a)) out<<*v;
      else if (const auto *v=std::get_if<SB::Model::Vec>(&a)) out<<"v:"<<(*v)[0]<<':'<<(*v)[1]<<':'<<(*v)[2];
      else if (const auto *v=std::get_if<Mat34>(&a)) {
        out<<"v:"; for (unsigned i=0;i<12;++i) { if(i) out<<':'; out<<(*v)[i]; }
      }
      else if (std::holds_alternative<std::monostate>(a)) out<<"n";
      else if (const auto *d=std::get_if<SB::Model::Descriptor>(&a)) {
        out<<"d"; for (const auto &e:d->entries) for (auto v:e) out<<':'<<v;
      }
      else out<<"s:"<<std::get<std::string>(a);
    }
  }
  return out.str();
}
void host_checks() {
  SB::Model m;
  auto check_require = [](bool value, int line) { if (!value) throw std::runtime_error("host check failed at line "+std::to_string(line)); };
#define require(...) check_require((__VA_ARGS__),__LINE__)
  {
    SB::Model *active=nullptr;unsigned shared_reads=0;
    SB::Memory memory({},[&](auto at,auto width) -> std::uint64_t {
      if(at==SB::G_RAND_SEED && width==4) {
        ++shared_reads;require(active && active->draws==0 && active->mem.log().size()==5);
        require(active->mem.load(0x8014,4)==0x515ba0);
        active->mem.poke(0x8010,4,0xffffffff);return 0;
      }
      return 0;
    });
    memory.poke(0x8000,4,0xa1b2c3d4);
    SB::Model spell(memory);active=&spell;
    spell.spell_base_ctor(0x8000,0x1234,7,9,0xdeadbeef);
    require(shared_reads==1 && spell.draws==1 && spell.mem.log().size()==8);
    require(spell.mem.load(0x8000,4)==0xa1b2c334 && spell.mem.load(0x8010,4)==0x3c6e);
    const std::array<unsigned,8> addresses={0x8000,0x8014,0x8008,0x8004,0x800c,SB::G_RAND_SEED,0x8010,0x8014};
    for(unsigned i=0;i<addresses.size();++i) require(spell.mem.log()[i].address==addresses[i]);
    require(spell.mem.log()[1].value==0x515ba0 && spell.mem.log()[7].value==0xdeadbeef);
    require(spell.spell_rand(0x8000)==0x3b43 && spell.mem.load(0x8010,4)==0x3b43d8f5);
    require(spell.draws==1 && spell.calls.empty() && spell.events.empty());
  }
  // Misaligned admitted head aliases expose B's cached first pointer.
  {
    SB::Model handles;
    handles.mem.poke(0x864ea8,4,0x4000);handles.mem.poke(0x4000,4,0x4100);
    handles.mem.poke(0x4100,4,0);handles.mem.poke(0x4010,4,0xa1b2ffff);
    require(handles.handle_link(0x4001,0x6000,7)==0x4000);
    require(handles.mem.load(0x45,4)==0x4000);
    require(handles.mem.load(0x4010,4)==0xa1b20000);
    require(handles.calls.empty() && handles.events.empty());
  }

  auto refuses = [&](auto fn) { bool failed=false; try { fn(); } catch(const SB::Unsupported &) { failed=true; } require(failed); };
  {
    unsigned reads=0;
    SB::Model bounded(SB::Memory({},[&](auto at,auto width) -> std::uint64_t {
      require(at==0x1060 && width==4); ++reads; return 0x2000;
    }));
    refuses([&]{ bounded.skill_call(0x1000,16,{}); });
    require(reads==1); // B fetches the table before rejecting the slot.
    refuses([&]{ bounded.skill_call(UINT32_MAX,10,{}); });
    require(reads==1); // Its wrapper offset rejects before fetching memory.
    bounded.callbacks["Skill.v10"]=[&](auto &,const auto &args) -> Raw {
      require(args==std::vector<Raw>({0xffff8000,1})); return 9;
    };
    bounded.mem.poke(0x2050,2,0x8000); bounded.mem.poke(0x2054,4,0x9000);
    // The direct virtual API historically admits a wide Address, publishing low32.
    require(bounded.virtual_call(SB::Model::VirtualClass::Skill,UINT64_C(0x100000000),0x2000,10,{1})==9);
  }
  SB::Memory initial;
  initial.poke(0x1060,4,0x2000);
  initial.poke(0x2054,4,0x3c85fc);
  initial.store(0x3000,4,0x7fc12345);
  std::map<std::string,SB::Model::Callback> services;
  services["Skill.v10"]=[](auto &self,const auto &args) {
    self.mem.store(0x4000,4,args.at(1)); return Raw(73);
  };
  SB::Model setup(std::move(initial),std::move(services),{"Skill.v10"});
  require(setup.draws==0 && setup.calls.empty() && setup.events.empty() && setup.addr_names.empty());
  require(setup.f32(0x3000)==0x7fc12345 && setup.mem.log().size()==1);
  require(setup.alloca(0)==0x1e10000 && setup.alloca(1)==0x1e0fff0);
  require(setup.skill_call(0x1000,10,{2,9})==73 && setup.mem.load(0x4000,4)==2);
  require(setup.calls.size()==1 && setup.events.size()==1);
  SB::Model independent;
  require(independent.alloca(16)==0x1e0fff0 && independent.callbacks.empty() && independent.external.empty());
  require(independent.mem.bytes().empty() && independent.mem.log().empty() && independent.draws==0);
  for (Raw value : {0u,1u,2u,3u,0x7fffffffu,0x80000000u,0x80000001u,0xfffffffdu,0xfffffffeu,0xffffffffu})
    require(SB::Model::half_toward_zero(value)==static_cast<Raw>(s32(value)/2));
  setup.mem.poke(0xc,4,0x40);
  require(setup.is_player(0));
  setup.mem.poke(0xc,4,0xffffffbf);
  require(!setup.is_player(0));
  for (unsigned i=0;i<256;++i) setup.mem.poke(0x5000+i,1,i+1);
  require(setup.cstr(0x5000).size()==64 && setup.cstr(0x5000,256).size()==255);
  for (unsigned i=0;i<255;++i) require(static_cast<unsigned char>(setup.cstr(0x5000,256)[i])==i+1);
  require(setup.cstr(UINT64_MAX,0).empty() && setup.cstr(UINT64_MAX,-1).empty());
  require(setup.cstr(0x9000).empty());
  refuses([&]{setup.cstr(0x5000,65537);});
  bool raw_address_refused=false;
  try { setup.f32(UINT32_MAX); } catch (const champions::simulation::rta::creature_hp::Error &) { raw_address_refused=true; }
  require(raw_address_refused);
  setup.mem.poke(UINT32_MAX,1,255);
  require(setup.cstr(UINT32_MAX,1)==std::string(1,static_cast<char>(255)));
  raw_address_refused=false;
  try { setup.cstr(UINT32_MAX,2); } catch (const SB::Unsupported &) { raw_address_refused=true; }
  require(raw_address_refused);
  auto factory15=SB::Model::mana_cost_method(15,0xdeadbeef);
  auto factory_other=SB::Model::mana_cost_method(0xffffffff,7);
  setup.callbacks["Skill.v15"]=[](auto &,const auto &args) { return args.at(2); };
  setup.callbacks["Skill.v14"]=[](auto &self,const auto &args) { self.mem.store(0x4000,4,args.at(2)); return ONE; };
  require(factory15(setup,0x1000,8)==0xdeadbeef);
  require(factory_other(setup,0x1000,9)==ZERO && setup.mem.load(0x4000,4)==7);
  require(factory15(setup,0x1000,10)==0xdeadbeef);
  refuses([&]{factory15(independent,0,1);});
  refuses([&]{m.get_stat(0,5,1);});
  refuses([&]{m.get_stat(UINT64_MAX,0,1);});
  refuses([&]{m.spell_rand(UINT64_MAX);});
  m.mem.poke(0xe,2,65535);
  refuses([&]{m.get_stat(0,0,0x7fffffff);});
  m.mem.poke(0x60,4,0x1000);
  refuses([&]{m.skill_call(0,10,{0,1});});
  require(m.calls.size()==1 && m.calls[0].name=="Skill.v10");
  refuses([&]{m.skill_call(0,2,{1,0,0,0});});
  // Typed cast callbacks snapshot the vector before callback mutations.
  SB::Model routing;
  routing.mem.poke(0x1060,4,0x2000);
  routing.mem.poke(0x2010,2,65520);
  routing.mem.poke(0x2014,4,0x1234);
  routing.mem.poke(0x3000,4,0x7fc12345);
  routing.mem.poke(0x3004,4,0x80000000);
  routing.mem.poke(0x3008,4,ONE);
  routing.typed_callbacks["Skill.v2"]=[](SB::Model &self,const std::vector<SB::Model::Argument> &args) -> SB::Model::Result {
    if (std::get<Raw>(args.at(0))!=0xff0 || std::get<SB::Model::Vec>(args.at(3))!=SB::Model::Vec{0x7fc12345,0x80000000,ONE})
      throw std::runtime_error("cast callback ABI");
    self.mem.store(0x3000,4,7);
    self.typed_callbacks.erase("Skill.v2");
    return Raw(99);
  };
  require(routing.skill_call(0x1000,2,{1,2,0x3000,4})==99);
  require(routing.events.size()==1 && routing.calls.empty());
  routing.typed_callbacks["Skill.v2"]=[](SB::Model &,const std::vector<SB::Model::Argument> &) -> SB::Model::Result { return std::string("bad"); };
  refuses([&]{routing.skill_call(0x1000,2,{1,2,0x3000,4});});
  routing.typed_callbacks["typed"]=[](SB::Model &,const std::vector<SB::Model::Argument> &) -> SB::Model::Result { return std::string("value"); };
  require(std::get<std::string>(routing.call("typed",{SB::Model::Vec{1,2,3}}))=="value");
  require(std::get<std::string>(routing.call("typed",{Raw(1)}))=="value");
  refuses([&]{routing.call("handle_link",{SB::Model::Vec{1,2,3}});});
  refuses([&]{routing.call("handle_clear",{});});
  refuses([&]{routing.call("missing",{Raw(0x1e00000)});});
  refuses([&]{routing.table_call("Other",0,0,65,{});});
  refuses([&]{routing.virtual_call(SB::Model::VirtualClass::Skill,0,0,16,{});});
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

  // New machinery retains partial prefixes and never supplies missing effects.
  SB::Model shared;
  bool missing=false;
  try { shared.game_object_ctor(0x1000,0,0,0); }
  catch (const SB::Unsupported &) { missing=true; }
  require(missing && shared.draws==1 && shared.mem.log().empty());
  shared.mem.poke(0x1014,4,0x2000);
  shared.mem.poke(0x2014,4,0x311e3c); // Shadow Step dispatch now reaches its explicit clamp service.
  auto count=shared.events.size();
  missing=false;
  try { shared.spell_call(0x1000,2,{0,0}); }
  catch (const SB::Unsupported &) { missing=true; }
  require(missing && shared.events.size()==count+1 && shared.events.back().name=="Player_clampToScreen");
  shared.mem.poke(0x2014,4,0x1234);
  shared.typed_callbacks["Spell.v2"]=[](SB::Model &self,const std::vector<SB::Model::Argument> &) -> SB::Model::Result {
    return self.spell_call(0x1000,2,{0,0});
  };
  missing=false;
  try { shared.spell_call(0x1000,2,{0,0}); }
  catch (const SB::Unsupported &) { missing=true; }
  require(missing);
  shared.typed_callbacks["Spell.v2"]=[](SB::Model &self,const std::vector<SB::Model::Argument> &args) -> SB::Model::Result {
    if (!std::holds_alternative<SB::Model::Vec>(args.at(1))) throw std::runtime_error("vector ABI lost");
    self.typed_callbacks.erase("Spell.v2"); // copied callable survives self-removal
    return Raw(9);
  };
  require(shared.spell_call(0x1000,2,{0,0})==9);
  require(shared.alloca(1)==0x1e0fff0);
  missing=false;
  try { shared.alloca(0x10000); } catch (const SB::Unsupported &) { missing=true; }
  require(missing);
  shared.wvec(0x3000,{1,2,3});
  require(shared.vec(0x3000)==SB::Model::Vec({1,2,3}));
  auto &log=shared.mem.log();
  require(log.size()==2 && log[0].size==8 && log[0].value==(std::uint64_t(2)<<32|1) && log[1].size==4);

  // Healing services have strict typed results and retain failure prefixes.
  SB::Model healing;
  auto check_refusal=[&](auto fn) {
    bool refused=false; try { fn(); } catch (const SB::Unsupported &) { refused=true; }
    require(refused);
  };
  check_refusal([&]{healing.sample_point(0,0,1,0);});
  require(healing.events.empty());
  healing.typed_callbacks["model_getSamplePoint"]=[](SB::Model &,const std::vector<SB::Model::Argument> &) -> SB::Model::Result { return Raw(0); };
  check_refusal([&]{healing.sample_point(0,0,1,7);});
  require(healing.events.size()==1);
  // The reference discards the second model output; the research callback
  // supplies a vector. Both scalar and vector metadata are accepted unchanged.
  healing.typed_callbacks["model_getSamplePoint"]=[](SB::Model &,const std::vector<SB::Model::Argument> &) -> SB::Model::Result {
    return SB::Model::SamplePoint{{1,2,3},SB::Model::Vec{4,5,6}};
  };
  require(healing.sample_point(0,0,1,7)==SB::Model::Vec({1,2,3}));
  check_refusal([&]{healing.object_ctor("GlowingShaft_ctor",{Raw(1),SB::Model::Vec{0,0,0}});});
  require(healing.draws==1);
  check_refusal([&]{healing.light_effect(1,{0,0,0},0,0,0,0,0,0,0);});
  require(healing.draws==2);
  // A perpetual rejection stream is bounded without replacing its random draws.
  std::size_t rejected=0;
  check_refusal([&]{healing.ball([&]{++rejected; return Raw(0);});});
  require(rejected==3*creature_hp::MAX_HISTORY);
  // Signed record slots wrap at the formula boundary.
  healing.mem.poke(0x13c8,4,0xffffffff);
  require(healing.record(0x1000)==0x4d3ec0);
  healing.mem.poke(0x1060,4,0x2000);
  healing.mem.poke(0x2064,4,0x1234); // slot 12 delegates
  healing.callbacks["Skill.v12"]=[](SB::Model &,const std::vector<Raw> &) { return Raw(0x7fffffff); };
  healing.mem.poke(0x3008,4,0x1000);
  check_refusal([&]{healing.healing_run(0x3000);});
  healing.callbacks["Skill.v12"]=[](SB::Model &,const std::vector<Raw> &) { return Raw(0); };
  healing.mem.poke(0x300c,4,0x4000);
  healing.mem.poke(0x400c,4,0x40);
  healing.mem.poke(0x4f8420,4,65537);
  check_refusal([&]{healing.healing_run(0x3000);});

  // Conversion boundaries refuse before guessing query or profile behavior.
  SB::Model conversion;
  auto conversion_refuses=[&](auto fn) {
    bool refused=false; try { fn(); } catch (const SB::Unsupported &) { refused=true; }
    require(refused);
  };
  conversion_refuses([&]{conversion.convert_cast(38,0,0,0);});
  require(conversion.events.empty() && conversion.mem.log().empty());
  conversion.typed_callbacks["query"]=[](SB::Model &,const std::vector<SB::Model::Argument> &) -> SB::Model::Result { return std::string("wrong"); };
  conversion_refuses([&]{conversion.query_list("query",{});});
  require(conversion.events.size()==1 && conversion.mem.load(0x1e0fff0,4)==0);
  conversion.typed_callbacks["query"]=[](SB::Model &,const std::vector<SB::Model::Argument> &) -> SB::Model::Result {
    return std::vector<Raw>(creature_hp::MAX_HISTORY+1,0);
  };
  conversion_refuses([&]{conversion.query_into(0x1000,"query",{});});
  conversion_refuses([&]{conversion.list_fill(0x1000,std::vector<Raw>(creature_hp::MAX_HISTORY+1,0));});
  require(conversion.mem.log().empty());
  conversion.typed_callbacks["query"]=[](SB::Model &,const std::vector<SB::Model::Argument> &) -> SB::Model::Result { return Raw(0); };
  conversion.query_into(0x1000,"query",{});
  require(conversion.mem.load(0x1000,4)==0);

  // Delegated attach points must contain exactly three patterns. Refusal retains
  // the clock increment and callback history that precede the failed service.
  SB::Model aura;
  aura.mem.poke(0x1018,4,0x2000); aura.mem.poke(0x211a,2,0x20);
  aura.callbacks["Player_animStep"]=[](SB::Model &,const std::vector<Raw> &) { return Raw(0); };
  aura.typed_callbacks["model_getAttachPoint"]=[](SB::Model &,const std::vector<SB::Model::Argument> &) -> SB::Model::Result {
    return std::vector<Raw>{ONE,ONE};
  };
  check_refusal([&]{aura.ancestralcall_run(0x1000,0x3000);});
  require(aura.mem.load(0x101c,4)==0x3c888888 && aura.events.size()==2);
  // A format operation with no substitution argument preserves Python None.
  SB::Model format;
  bool saw_none=false;
  format.typed_callbacks["libc_sprintf"]=[&](SB::Model &,const std::vector<SB::Model::Argument> &a) -> SB::Model::Result {
    saw_none=a.size()==2 && std::holds_alternative<std::monostate>(a[1]);
    return SB::Model::SamplePoint{{0,0,0},Raw(0)};
  };
  format.callbacks["res_find"]=[](SB::Model &,const std::vector<Raw> &) { return Raw(0); };
  format.callbacks["sound_playOnObject"]=[](SB::Model &,const std::vector<Raw> &) { return Raw(0); };
  check_refusal([&]{format.ancestralcall_spell_ctor(0x1000,5,0x2000,0x3000);});
  require(saw_none && format.draws==1 && format.mem.load(0x1018,4)==0x2000);
  SB::Model growth;
  growth.callbacks["light_link"]=[](SB::Model &,const std::vector<Raw> &) { return Raw(0); };
  growth.callbacks["particleGroup_create"]=[](SB::Model &,const std::vector<Raw> &) { return Raw(0); };
  growth.callbacks["Skill.v10"]=[](SB::Model &,const std::vector<Raw> &) { return Raw(1); };
  growth.callbacks["model_getSamplePointCount"]=[](SB::Model &,const std::vector<Raw> &) { return Raw(0); };
  check_refusal([&]{growth.giantgrowth_spell_ctor(0x1000,5,0x2000,0x3000);});
  require(growth.draws==1 && growth.mem.load(0x1028,4)==0);
  require(growth.mem.load(0x1010,4)!=0); // two private draws precede zero-count refusal
  growth.callbacks["particleGroup_begin"]=[](SB::Model &,const std::vector<Raw> &) { return Raw(0); };
  growth.typed_callbacks["particleGroup_setMatrix"]=[](SB::Model &,const std::vector<SB::Model::Argument> &) -> SB::Model::Result { return Raw(0); };
  growth.typed_callbacks["model_getSamplePoint"]=[](SB::Model &,const std::vector<SB::Model::Argument> &) -> SB::Model::Result { return Raw(0); };
  check_refusal([&]{growth.giantgrowth_emit(0x1000,0x2000);});
  growth.typed_callbacks["model_getSamplePoint"]=[](SB::Model &,const std::vector<SB::Model::Argument> &) -> SB::Model::Result {
    return SB::Model::GrowthSample{{0,0,0},{},mat3_set_identity()};
  };
  check_refusal([&]{growth.giantgrowth_emit(0x1000,0x2000);});

  // Melee callback boundaries preserve all writes/draws before refusal.
  SB::Model melee;
  melee.mem.poke(0x100c,4,0x2000);
  melee.callbacks["Player_animStep"]=[](SB::Model &,const std::vector<Raw> &) { return Raw(2); };
  melee.callbacks["Player_move"]=[](SB::Model &,const std::vector<Raw> &) { return Raw(0); };
  melee.callbacks["Skill.v10"]=[](SB::Model &,const std::vector<Raw> &) { return Raw(1); };
  melee.typed_callbacks["model_getAttachPoint"]=[](SB::Model &,const std::vector<SB::Model::Argument> &) -> SB::Model::Result { return std::vector<Raw>{1,2}; };
  check_refusal([&]{melee.bash_run(0x1000);});
  require(melee.mem.load(0x101c,4)==1 && melee.mem.load(0x1018,4)==0);
  melee.callbacks["net_objOrGhost"]=[](SB::Model &,const std::vector<Raw> &) { return Raw(0x2000); };
  melee.callbacks["model_getSamplePointCount"]=[](SB::Model &,const std::vector<Raw> &) { return Raw(0); };
  auto before_seed=melee.mem.load(0x1010,4);
  check_refusal([&]{melee.bash_windup_fx(0x1000);});
  require(melee.mem.load(0x1010,4)!=before_seed && melee.draws==0);
  melee.typed_callbacks["trail_addPoint"]=[](SB::Model &,const std::vector<SB::Model::Argument> &) -> SB::Model::Result { return Raw(0); };
  melee.callbacks["Creature.v16"]=[](SB::Model &,const std::vector<Raw> &) { return Raw(0); };
  melee.mem.poke(0x1044,4,melee.mem.load(creature_hp::G_SIM_FRAME,4));
  before_seed=melee.mem.load(0x1010,4);
  check_refusal([&]{melee.charge_run(0x1000);});
  require(melee.mem.load(0x1010,4)!=before_seed && melee.mem.load(0x402797,1)==0);
  // Vector-taking Creature slot 26 retains signed adjustment and named dispatch.
  melee.mem.poke(0x1018,4,1); melee.mem.poke(0x1024,4,0x3000);
  melee.mem.poke(0x2048,4,0x4000); melee.mem.poke(0x40d0,2,0xfff0); melee.mem.poke(0x40d4,4,0x777700);
  melee.mem.poke(0x3010,4,ONE); melee.mem.poke(0x211a,2,0x10);
  melee.callbacks["net_isLocalSlot"]=[](SB::Model &,const std::vector<Raw> &) { return Raw(0); };
  melee.addr_names[0x777700]="turn";
  bool turned=false;
  melee.typed_callbacks["turn"]=[&](SB::Model &self,const std::vector<SB::Model::Argument> &args) -> SB::Model::Result {
    require(std::get<Raw>(args.at(0))==0x1ff0 && std::get<Raw>(args.at(1))==0);
    require(std::get<SB::Model::Vec>(args.at(2))==SB::Model::Vec({ONE,0,0}));
    turned=true; self.typed_callbacks.erase("turn"); return Raw(0);
  };
  melee.bash_run(0x1000); require(turned);
  SB::Model malformed_box;
  for (const auto &name:{"res_find","sound_playOnObject","camera_shake","damage_apply"})
    malformed_box.callbacks[name]=[](SB::Model &,const std::vector<Raw> &) { return Raw(0); };
  malformed_box.callbacks["Skill.v10"]=[](SB::Model &,const std::vector<Raw> &) { return Raw(1); };
  malformed_box.typed_callbacks["world_collectObjectsInBox"]=[](SB::Model &,const std::vector<SB::Model::Argument> &) -> SB::Model::Result { return std::string("invalid"); };
  check_refusal([&]{malformed_box.charge_on_contact(0x1000,0x3000);});
  require(malformed_box.mem.load(0x1010,4)!=0 && malformed_box.mem.load(0x1048,4)==0);

  // Resurrection refuses unavailable services after its documented prefixes.
  SB::Model revive;
  check_refusal([&]{revive.resurrect_spell_ctor(0x1000,255,0x2000,0x3000,0x4000);});
  require(revive.draws==1 && revive.mem.load(0x1000,1)==255);
  require(revive.mem.load(0x1014,4)==0x51be00 && revive.mem.load(0x105c,4)==ONE);
  require(revive.events.size()==1 && revive.events.back().name=="light_link");
  SB::Model search;
  search.mem.poke(0x4d8ae0,4,65537);
  check_refusal([&]{search.resurrect_find_target(0,0);});
  require(search.events.empty() && search.mem.log().empty());
  search.mem.poke(0x4d8ae0,4,UINT32_MAX);
  require(search.resurrect_find_target(0,0)==UINT32_MAX);
  SB::Model raising;
  raising.mem.poke(0x4f8420,4,1);
  check_refusal([&]{raising.resurrect_is_being_raised(0,0);});
  require(raising.events.size()==1 && raising.events[0].name=="SpellResurrect_typeinfo");
  SB::Model streams;
  streams.mem.poke(0x4d8ae0,4,2); streams.mem.poke(0x1018,4,0x2000);
  streams.mem.poke(0x1068,4,0x3000); streams.mem.poke(0x2010,4,ONE);
  streams.mem.poke(0x1060,4,ONE);
  unsigned emissions=0;
  streams.typed_callbacks["fx_emitFromModelVerts"]=[&](SB::Model &,const std::vector<SB::Model::Argument> &args) -> SB::Model::Result {
    require(args.size()==6 && std::get<SB::Model::Vec>(args[3])==SB::Model::Vec({ONE,0,0}));
    if (emissions++==0) require(std::get<Raw>(args[4])==0);
    else require(std::get<SB::Model::Vec>(args[4])==SB::Model::Vec({0xc2280000,0x80000000,0x80000000}));
    return Raw(0);
  };
  streams.callbacks["Player_animStep"]=[](SB::Model &,const std::vector<Raw> &) -> Raw { throw CallbackFailure{}; };
  bool step_failed=false;
  try { streams.resurrect_run(0x1000); } catch (const CallbackFailure &) { step_failed=true; }
  require(step_failed && emissions==2 && streams.mem.load(0x402797,1)==0);
  require(streams.mem.load(0x1060,4)==ONE && streams.mem.load(0x2090,2)==0x62);

  // Cyclone keeps movement vectors by value, refuses malformed attachment
  // results, and bounds its live (rather than snapshotted) candidate walk.
  SB::Model cyclone;
  cyclone.mem.poke(0x100c,4,0x2000);
  cyclone.mem.poke(0x211a,2,0x30); cyclone.mem.poke(0x258c,4,6);
  cyclone.mem.poke(0x1068,4,ONE);
  cyclone.callbacks["Player_animStep"]=[](SB::Model &,const std::vector<Raw> &) { return Raw(0); };
  cyclone.callbacks["Skill.v10"]=[](SB::Model &,const std::vector<Raw> &) { return Raw(40); };
  cyclone.callbacks["net_getCamYaw"]=[](SB::Model &,const std::vector<Raw> &) { return Raw(0x80000000); };
  bool saw_push=false;
  cyclone.typed_callbacks["Player_move"]=[&](SB::Model &self,const std::vector<SB::Model::Argument> &args) -> SB::Model::Result {
    require(std::get<Raw>(args.at(0))==0x2000 && std::get<Raw>(args.at(1))==5);
    auto push=std::get<SB::Model::Vec>(args.at(3)); require(push[2]==ZERO);
    saw_push=true; self.mem.poke(0x2010,4,ONE);
    self.typed_callbacks.erase("Player_move"); return Raw(0);
  };
  cyclone.callbacks["light_relink"]=[](SB::Model &,const std::vector<Raw> &) { return Raw(0); };
  unsigned helix=0;
  cyclone.typed_callbacks["particle_spawn"]=[&](SB::Model &,const std::vector<SB::Model::Argument> &args) -> SB::Model::Result {
    require(std::get<Raw>(args[0])==0x8e5500);
    require(std::holds_alternative<SB::Model::Vec>(args[1]) && std::holds_alternative<SB::Model::Vec>(args[2]));
    ++helix; return Raw(0);
  };
  cyclone.typed_callbacks["model_getAttachPoint"]=[](SB::Model &,const std::vector<SB::Model::Argument> &) -> SB::Model::Result {
    return std::vector<Raw>{ONE,ZERO};
  };
  check_refusal([&]{cyclone.cyclone_run(0x1000,0x3000);});
  require(saw_push && helix==72 && cyclone.mem.load(0x106c,4)==0x3f7686d7);
  cyclone.typed_callbacks["model_getAttachPoint"]=[](SB::Model &,const std::vector<SB::Model::Argument> &) -> SB::Model::Result {
    return std::vector<Raw>{ONE,ZERO,ZERO};
  };
  cyclone.mem.poke(0x105c,4,1); cyclone.mem.poke(0x1018,4,0x4000);
  cyclone.mem.poke(0x4000,4,0x4000); // null object cycle, no callbacks to advance it
  check_refusal([&]{cyclone.cyclone_run(0x1000,0x3000);});
  require(helix==144 && cyclone.mem.load(0x402797,1)==0);
  SB::Model cyclone_missing;
  check_refusal([&]{cyclone_missing.cyclone_spell_ctor(0x1000,9,0x2000,0x3000);});
  require(cyclone_missing.draws==1 && cyclone_missing.events.back().name=="light_link");

  // Damage descriptors retain their four 16-bit lanes and callbacks by value.
  auto desc=SB::Model::descriptor({{0x10016,0xffffffff,65536,0x12345}});
  require(desc.entries==std::vector<std::array<Raw,4>>({{0x16,65535,0,0x2345}}));
  require(SB::Model::descriptor({}).entries.empty());
  SB::Model cone_missing;
  check_refusal([&]{cone_missing.coneoffrost_spell_ctor(0x1000,255,0x2000,0x3000);});
  require(cone_missing.draws==1 && cone_missing.mem.load(0x1014,4)==0x519ed8);
  require(cone_missing.events.back().name=="light_link" && cone_missing.mem.load(0x1054,4)==0);
  SB::Model dummy;
  dummy.mem.poke(0x1060,4,0x18); dummy.mem.poke(0x1068,4,from_int(-1));
  check_refusal([&]{dummy.dummydamager_on_touch(0x1000,0x2000);});
  require(dummy.mem.load(0x1068,4)==0 && dummy.events.back().name=="damage_apply");
  bool damage_seen=false;
  dummy.mem.poke(0x1068,4,from_int(65537));
  dummy.typed_callbacks["damage_apply"]=[&](SB::Model &self,const std::vector<SB::Model::Argument> &a) -> SB::Model::Result {
    require(a.size()==10 && std::get<Raw>(a[1])==0 && std::get<Raw>(a[8])==0);
    require(std::get<SB::Model::Descriptor>(a[6]).entries==std::vector<std::array<Raw,4>>({{0x16,1,1,0}}));
    damage_seen=true; self.typed_callbacks.erase("damage_apply"); return Raw(0);
  };
  dummy.dummydamager_on_touch(0x1000,0x2000); require(damage_seen);
  dummy.mem.poke(0x106c,2,1);
  check_refusal([&]{dummy.dummydamager_run(0x1000);});
  require(dummy.mem.load(0x106c,2)==0 && dummy.events.back().name=="Creature.v1");
  SB::Model scratch;
  auto point=scratch.local_vec({1,2,3});
  require(point==0x1e0fff0 && scratch.vec(point)==SB::Model::Vec({1,2,3}) && scratch.mem.log().empty());
  check_refusal([&]{scratch.local_vec({1,2,3}); scratch.alloca(0x10000);});

  // Bow IDs fail before doing work; Multi Fire never invents a projectile.
  SB::Model arrows;
  check_refusal([&]{arrows.arrow_cast(46,0,0,0,0,0);});
  check_refusal([&]{arrows.arrow_run(42,0);});
  require(arrows.events.empty() && arrows.mem.log().empty());
  arrows.mem.poke(0x104c,4,0x3000); arrows.mem.poke(0x300c,4,0x4000);
  arrows.mem.poke(0x400c,4,0x40); arrows.mem.poke(0x200c,4,0x1000000);
  arrows.mem.poke(0x106c,4,9); arrows.mem.poke(0x10ac,4,0x5000);
  arrows.mem.poke(0x5060,4,0x6000); arrows.mem.poke(0x606c,4,0x1234);
  arrows.callbacks["Skill.v13"]=[](SB::Model &,const std::vector<Raw> &) { return Raw(7); };
  arrows.arrow_on_hit(43,0x1000,0x2000);
  require(arrows.events.size()==1 && arrows.mem.load(0x1044,4)!=0); // rejected hit still draws
  check_refusal([&]{arrows.arrow_end(43,0x1000,0);});
  require(arrows.mem.load(0x1060,4)==0 && arrows.events.back().name=="obj_delete");
  arrows.callbacks["obj_delete"]=[](SB::Model &self,const std::vector<Raw> &) { self.callbacks.erase("obj_delete"); return Raw(0); };
  arrows.arrow_end(43,0x1000,0); auto once=arrows.events.size(); arrows.arrow_end(43,0x1000,1);
  require(arrows.events.size()==once && arrows.mem.load(0x1060,4)==1);
  auto spread=SB::Model::monster_spread(0x7fffffff,65535);
  require(spread[0]==from_int(0x7fffffff) && lt(spread[1],ZERO)); // conversion wraps at signed32
  // The shared flight helper accepts caller-owned end/particle functions.
  SB::Model flight;
  flight.callbacks["obj_updateGridCell"]=[](SB::Model &,const std::vector<Raw> &){return Raw(0);};
  flight.typed_callbacks["terrain_height"]=[](SB::Model &,const std::vector<SB::Model::Argument> &) -> SB::Model::Result { return ZERO; };
  flight.typed_callbacks["world_collectObjectsInBox"]=[](SB::Model &,const std::vector<SB::Model::Argument> &) -> SB::Model::Result { return std::vector<Raw>{}; };
  bool emitted=false,expired=false;
  flight.arrow_flight(0x1000,{},[&](SB::Address a){require(a==0x1000);emitted=true;});
  require(emitted && flight.mem.load(0x106c,4)==1);
  flight.mem.poke(0x106c,4,600);
  flight.arrow_flight(0x1000,[&](SB::Address a,Raw reason){require(a==0x1000 && reason==0);expired=true;},{});
  require(expired && flight.mem.load(0x106c,4)==601);
  flight.mem.poke(0x106c,4,0);
  check_refusal([&]{flight.arrow_flight(0x1000,std::function<void(SB::Address,Raw)>{},std::function<void(SB::Address)>{});});
  require(flight.mem.load(0x106c,4)==1); // refusal retains the movement prefix
  // Multi Fire retains both owned vectors and all seventeen delegated fields.
  SB::Model fan;
  fan.mem.poke(0x1060,4,0x2000);
  fan.callbacks["Skill.v14"]=[](SB::Model &,const std::vector<Raw> &a){return a[1];};
  fan.callbacks["Skill.v12"]=[](SB::Model &,const std::vector<Raw> &a){return a[1]==2 ? Raw(1):Raw(7);};
  fan.callbacks["pool_new"]=[](SB::Model &,const std::vector<Raw> &a){return a[0]==0xbc ? Raw(0):Raw(0x3000);};
  bool missile=false;
  fan.typed_callbacks["MissileWeapon_ctor"]=[&](SB::Model &self,const std::vector<SB::Model::Argument> &a) -> SB::Model::Result {
    require(a.size()==17 && std::holds_alternative<SB::Model::Vec>(a[4]) && std::holds_alternative<SB::Model::Vec>(a[5]));
    require(std::get<Raw>(a[14])==10000 && std::get<Raw>(a[16])==0);
    missile=true; self.typed_callbacks.erase("MissileWeapon_ctor"); return Raw(0);
  };
  check_refusal([&]{fan.multifire_cast(0x1000,2,0x4000,0x5000,0);}); // final missing res_find
  require(missile && fan.draws==2 && fan.events.back().name=="res_find");

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
  // Shock retains reached stores on missing services and owns typed beam vectors.
  SB::Model shock;
  shock.mem.poke(0x60010,4,ONE); shock.mem.poke(0x60120,4,ONE);
  shock.callbacks["sound_setVolume"]=[](auto &,const auto &){return Raw(0);};
  missing=false;
  try {shock.shock_run(0x60000);} catch(const SB::Unsupported &){missing=true;}
  require(missing && shock.mem.load(0x60010,4)==add(ONE,ONE));
  require(shock.events.size()==2 && shock.events.back().name=="obj_updateGridCell");
  SB::Model beam;
  beam.mem.poke(0x60189,1,1); beam.mem.poke(0x60054,4,0x90000);
  beam.mem.poke(0x90000,4,0x90018); beam.mem.poke(0x9000c,4,0x71000);
  beam.mem.poke(0x90024,4,0x72000); beam.mem.poke(0x6012c,4,UINT32_MAX);
  beam.callbacks["sound_setVolume"]=[](auto &,const auto &){return Raw(0);};
  beam.callbacks["obj_updateGridCell"]=[](auto &,const auto &){return Raw(0);};
  beam.typed_callbacks["beam_create"]=[](auto &self,const auto &args)->SB::Model::Result {
    auto a=std::get<SB::Model::Vec>(args.at(0)),b=std::get<SB::Model::Vec>(args.at(1));
    if (a[2]!=0x42100000 || b[2]!=0x42100000 || std::get<Raw>(args.at(2))!=0x104 || args.size()!=7)
      throw std::runtime_error("Shock beam ABI");
    self.typed_callbacks.erase("beam_create"); return Raw(0x99);
  };
  missing=false; try {beam.shock_run(0x60000);} catch(const SB::Unsupported &){missing=true;}
  require(missing && beam.mem.load(0x6012c,4)==0x99 && !beam.typed_callbacks.count("beam_create"));
  // A cyclic candidate list refuses before recursive chain selection.
  SB::Model cycle; cycle.mem.poke(0x6018a,1,4); cycle.mem.poke(0x60050,4,0x90000);
  cycle.mem.poke(0x90000,4,0x90000);
  missing=false; try {cycle.shock_build_chain(0x60000,0x50000);} catch(const SB::Unsupported &){missing=true;}
  require(missing); cycle.mem.poke(0x90000,4,0);
  cycle.shock_build_chain(0x60000,0x50000); // failed walk restored the recursion guard.
  SB::Model deep;
  deep.mem.poke(0x864ea8,4,0xa0000);
  for (Raw i=0;i<256;++i) deep.mem.poke(0xa0000+24*i,4,i==255?0:0xa0000+24*(i+1));
  std::vector<Raw> candidates;
  for (Raw i=0;i<70;++i) candidates.push_back(0x71000+i*0x200);
  deep.list_fill(0x60050,candidates); deep.mem.poke(0x6018a,1,127); deep.mem.poke(0x6018c,4,ONE);
  missing=false; try {deep.shock_build_chain(0x60000,0x50000);} catch(const SB::Unsupported &){missing=true;}
  require(missing && deep.mem.load(0x6018b,1)==64);
  deep.mem.poke(0x60050,4,0); deep.shock_build_chain(0x60000,0x50000);
  // Frost Storm service refusals retain movement, owned vector and descriptor ABI.
  SB::Model frost;
  frost.mem.poke(0x44fdcc,4,30); frost.mem.poke(0x60010,4,ONE);
  frost.mem.poke(0x60060,4,ONE); frost.mem.poke(0x60058,4,0x3f733333);
  check_refusal([&]{frost.froststorm_run(0x60000);});
  require(frost.mem.load(0x60010,4)==add(ONE,ONE) && frost.events.back().name=="light_relink");
  frost.callbacks["light_relink"]=[](auto &,const auto &){return Raw(0);};
  frost.typed_callbacks["world_collectObjectsInBox"]=[](auto &,const auto &)->SB::Model::Result {return std::string("bad list");};
  check_refusal([&]{frost.froststorm_run(0x60000);});
  frost.mem.poke(0x6005c,4,101); frost.mem.poke(0x6006c,4,0x90000); frost.mem.poke(0x9000c,4,0x30000);
  frost.typed_callbacks["world_collectObjectsInBox"]=[](auto &,const auto &a)->SB::Model::Result {
    if (a.size()!=3 || std::get<Raw>(a[2])!=64 || !std::holds_alternative<SB::Model::Vec>(a[0]) || !std::holds_alternative<SB::Model::Vec>(a[1]))
      throw std::runtime_error("Frost Storm box ABI");
    return std::vector<Raw>{0x71000,0x71000};
  };
  frost.callbacks["Creature.v20"]=[](auto &self,const auto &)->Raw {self.mem.poke(0x6006c,4,0);return 1;};
  unsigned ticks=0;
  frost.typed_callbacks["damage_apply"]=[&](auto &self,const auto &a)->SB::Model::Result {
    require(a.size()==10 && std::get<Raw>(a[1])==0);
    require(std::get<Raw>(a[5])==0 && std::get<Raw>(a[8])==0x424a0000 && std::get<Raw>(a[9])==0x424a0000);
    auto d=std::get<SB::Model::Descriptor>(a[6]);
    require(d.entries==std::vector<std::array<Raw,4>>{{0x16,50,50,0}});
    ++ticks; if (ticks==2) self.typed_callbacks.erase("damage_apply"); return Raw(0);
  };
  frost.callbacks["Creature.v1"]=[](auto &,const auto &){return Raw(0);};
  frost.froststorm_run(0x60000); require(ticks==2 && !frost.typed_callbacks.count("damage_apply"));
  SB::Model shard_missing; shard_missing.mem.poke(0x44fdcc,4,1); shard_missing.mem.poke(0x80060,4,120);
  shard_missing.mem.poke(0x80050,4,ONE); shard_missing.mem.poke(0x80044,4,1);
  check_refusal([&]{shard_missing.iceshard_run(0x80000);});
  require(shard_missing.mem.load(0x8005c,4)==add(0,0xbeced8f4) && shard_missing.events.back().name=="particle_spawn");
  require(SB::Model::c_rem(0x80000000,0xffffffff)==0 && SB::Model::c_rem(0x80000000,0)==0x80000000);
  require(SB::Model::down(0x80000000)==0 && SB::Model::down(0x7fc00001)==0xffc00001);
  // Infinite particle counts refuse at the existing host traversal bound.
  SB::Model huge; huge.mem.poke(0x8004c,4,1); huge.mem.poke(0x80050,4,0x7f800000); huge.mem.poke(0x80044,4,1);
  huge.typed_callbacks["particle_spawn"]=[](auto &self,const auto &)->SB::Model::Result {self.mem.clear_log();self.events.clear();return Raw(0);};
  check_refusal([&]{huge.iceshard_run(0x80000);});

  // Ground Pound: missing services preserve prefixes and callback-owned values.
  SB::Model pound_cast;
  pound_cast.mem.poke(0x3000c,4,0x40);
  check_refusal([&]{pound_cast.groundpound_cast(0x10000,1,0x30000);});
  require(pound_cast.mem.load(0x3058c,4)==10 && pound_cast.events.back().name=="res_find");
  pound_cast.callbacks["res_find"]=[](auto &,const auto &){return Raw(0x81000);};
  check_refusal([&]{pound_cast.groundpound_cast(0x10000,1,0x30000);});
  require(pound_cast.mem.load(0x81020,1)==2 && pound_cast.mem.load(0x8102c,4)==0x3fbd2e2f);
  SB::Model pound_ctor;
  check_refusal([&]{pound_ctor.groundpound_effect_ctor(0x60000,0x50000,0,0x10000,20,0);});
  require(pound_ctor.draws==1 && pound_ctor.mem.log().empty());
  bool checked_water=false;
  pound_ctor.typed_callbacks["GameObject_ctor"]=[&](auto &,const auto &a)->SB::Model::Result {
    require(std::get<SB::Model::Vec>(a[1])==SB::Model::Vec({0,0,0}));
    return Raw(0);
  };
  for (auto name:{"light_link","res_find","sound_playOnObject","obj_runEveryFrame","light_unlink","light_relink"})
    pound_ctor.callbacks[name]=[](auto &,const auto &){return Raw(0);};
  pound_ctor.typed_callbacks["terrain_height"]=[](auto &,const auto &)->SB::Model::Result {return Raw(0x43100000);};
  pound_ctor.typed_callbacks["water_test"]=[&](auto &,const auto &a)->SB::Model::Result {
    require(std::get<std::string>(a[2])=="stack" && std::get<Raw>(a[3])==0x41000000);
    checked_water=true; return Raw(128);
  };
  pound_ctor.groundpound_effect_ctor(0x60000,0x50000,0,0x10000,255,0);
  require(checked_water && pound_ctor.mem.load(0x60050,1)==0 && pound_ctor.mem.load(0x6004f,1)==128);
  require(pound_ctor.mem.load(0x6004d,1)==255 && pound_ctor.mem.load(0x6008c,4)==0x41f00000);
  // Invalid query results refuse after effects but before final light/counter stores.
  SB::Model pound;
  pound.mem.poke(0x60058,4,0x10000); pound.mem.poke(0x60054,4,0x42d80000);
  pound.callbacks["Skill.v10"]=[](auto &,const auto &){return Raw(2);};
  pound.callbacks["Skill.v12"]=[](auto &,const auto &){return Raw(2);};
  for (auto name:{"pool_new","camera_shake"})
    pound.callbacks[name]=[](auto &,const auto &){return Raw(0);};
  for (auto name:{"particle_spawn","decal_add"})
    pound.typed_callbacks[name]=[](auto &,const auto &)->SB::Model::Result {return Raw(0);};
  pound.typed_callbacks["world_collectObjectsInBox"]=[](auto &,const auto &)->SB::Model::Result {return std::string("bad");};
  check_refusal([&]{pound.groundpound_effect_run(0x60000);});
  require(pound.events.back().name=="world_collectObjectsInBox" && pound.mem.load(0x6004c,1)==0);
  pound.typed_callbacks["world_collectObjectsInBox"]=[](auto &,const auto &)->SB::Model::Result {
    return std::vector<Raw>(creature_hp::MAX_HISTORY+1,0);
  };
  check_refusal([&]{pound.groundpound_effect_run(0x60000);});
  // Slot-20 mutation drops the owner before damage; the reference still reads address 0xc.
  pound.mem.poke(0x60098,4,0x41000); pound.mem.poke(0x4100c,4,0x30000); pound.mem.poke(0xc,4,0x77);
  pound.typed_callbacks["world_collectObjectsInBox"]=[](auto &,const auto &)->SB::Model::Result {return std::vector<Raw>{0x71000,0x71000};};
  pound.callbacks["Creature.v20"]=[](auto &self,const auto &){self.mem.poke(0x60098,4,0);return Raw(1);};
  unsigned pound_hits=0;
  pound.callbacks["damage_apply"]=[&](auto &self,const auto &a){
    require(a[1]==0x77 && a[2]==100000 && a[5]==0x80100 && a[8]==from_int(2));
    ++pound_hits; self.callbacks.erase("damage_apply"); return Raw(0);
  };
  pound.groundpound_effect_run(0x60000); require(pound_hits==1 && pound.mem.load(0x6004c,1)==1);
  // Deletion callback is followed by the light updates and increment, including byte wrap.
  pound.mem.poke(0x6004c,1,30);
  pound.callbacks["obj_delete"]=[](auto &self,const auto &){self.mem.poke(0x6004c,1,255);return Raw(0);};
  pound.groundpound_effect_run(0x60000); require(pound.mem.load(0x6004c,1)==0);

  // Harm Touch preserves undefined-sampling prefixes and typed service failures.
  SB::Model harm;
  harm.mem.poke(0x6014,4,0x8000); harm.mem.poke(0x6018,4,ONE);
  harm.mem.poke(0x600c,4,0x3000); harm.mem.poke(0x6008,4,0x1000);
  harm.mem.poke(0x6010,4,1); harm.mem.poke(0x700c,4,0x20);
  harm.mem.poke(0x3010,4,0); harm.mem.poke(0x3014,4,0);
  harm.callbacks["Creature.v16"]=[](auto &,const auto &){return Raw(0x9000);};
  harm.callbacks["model_getSamplePointCount"]=[](auto &,const auto &){return Raw(0);};
  auto harm_seed=Raw(1)*1664525u+1013904223u;
  refuses([&]{harm.harmtouch_apply_to_hit(0x6000,0x7000,0xa000,0xa004);});
  require(harm.mem.load(0x6010,4)==harm_seed && harm.events.size()==2);
  require(harm.events.back().name=="model_getSamplePointCount");
  require(harm.mem.load(0x3096,2)==113 && harm.mem.load(0x3092,2)==128);
  require(harm.mem.load(0xa000,4)==0 && harm.draws==0);
  harm.callbacks["model_getSamplePointCount"]=[](auto &,const auto &){return Raw(7);};
  harm.typed_callbacks["model_getSamplePoint"]=[](auto &,const auto &)->SB::Model::Result {return Raw(0);};
  refuses([&]{harm.harmtouch_apply_to_hit(0x6000,0x7000,0xa000,0xa004);});
  require(harm.events.back().name=="model_getSamplePoint");
  harm.typed_callbacks["model_getSamplePoint"]=[](auto &,const auto &)->SB::Model::Result {
    return SB::Model::SamplePoint{{ONE,ONE,ZERO},Raw(0)};
  };
  unsigned harm_particles=0;
  harm.typed_callbacks["particle_spawn"]=[&](auto &self,const auto &a)->SB::Model::Result {
    require(a.size()==4 && std::holds_alternative<SB::Model::Vec>(a[1]) && std::holds_alternative<SB::Model::Vec>(a[2]));
    ++harm_particles; self.typed_callbacks.erase("particle_spawn"); return Raw(0);
  };
  refuses([&]{harm.harmtouch_apply_to_hit(0x6000,0x7000,0xa000,0xa004);});
  require(harm_particles==1 && harm.events.back().name=="particle_spawn");
  // A noncreature hit refreshes tint but leaves the range and spell armed.
  harm.mem.poke(0x700c,4,0); harm.mem.poke(0xa000,4,17); harm.mem.poke(0xa004,4,29);
  auto harm_events=harm.events.size(); auto harm_log=harm.mem.log().size();
  harm.harmtouch_apply_to_hit(0x6000,0x7000,0xa000,0xa004);
  require(harm.events.size()==harm_events && harm.mem.log().size()==harm_log+3);
  require(harm.mem.load(0xa000,4)==17 && harm.mem.load(0xa004,4)==29);
  // Expiry keeps its timer write even if destruction is unavailable.
  harm.mem.poke(0x6018,4,0);
  refuses([&]{harm.harmtouch_run(0x6000);});
  require(harm.mem.load(0x6018,4)==sub(ZERO,0x3c888888) && harm.events.back().name=="Spell.v1");

  // Darkness validates typed services and keeps every reached failure prefix.
  SB::Model dark;
  dark.mem.poke(0x6009c,4,1); dark.mem.poke(0x6004c,4,ONE);
  refuses([&]{dark.darkness_run(0x60000);});
  require(dark.mem.load(0x60010,4)==ONE && dark.events.back().name=="terrain_height");
  dark.typed_callbacks["terrain_height"]=[&](auto &,const auto &a)->SB::Model::Result {
    require(std::get<SB::Model::Vec>(a.at(1))[0]==TWO && std::get<Raw>(a.at(2))==0x41c00000);
    return std::string("invalid height");
  };
  refuses([&]{dark.darkness_run(0x60000);});
  require(dark.mem.load(0x60010,4)==TWO);
  dark.typed_callbacks["world_collectObjectsInBox"]=[](auto &,const auto &)->SB::Model::Result {return std::string("invalid list");};
  refuses([&]{dark.darkness_box(0x60000);});
  dark.typed_callbacks["world_collectObjectsInBox"]=[](auto &,const auto &)->SB::Model::Result {
    return std::vector<Raw>(creature_hp::MAX_HISTORY+1,0);
  };
  refuses([&]{dark.darkness_box(0x60000);});
  dark.typed_callbacks["world_collectObjectsInBox"]=[](auto &,const auto &)->SB::Model::Result {return Raw(0);};
  require(dark.darkness_box(0x60000).empty());

  SB::Model dark_end;
  dark_end.mem.poke(0x60094,4,0); dark_end.mem.poke(0x600a0,1,1);
  dark_end.callbacks["light_relink"]=[](auto &,const auto &){return Raw(0);};
  unsigned particles=0;
  dark_end.typed_callbacks["particle_spawn"]=[&](auto &,const auto &)->SB::Model::Result {++particles;return Raw(0);};
  refuses([&]{dark_end.darkness_run(0x60000);});
  require(particles==8 && dark_end.mem.load(0x60094,4)==UINT32_MAX && dark_end.events.back().name=="obj_delete");
  dark_end.mem.poke(0x60094,4,0);
  dark_end.callbacks["obj_delete"]=[](SB::Model &self,const auto &){
    self.callbacks.erase("obj_delete"); return Raw(0);
  };
  refuses([&]{dark_end.darkness_run(0x60000);});
  require(particles==16 && dark_end.events.back().name=="light_unlink");
  require(!dark_end.callbacks.count("obj_delete"));

  // No-target votes remain caller services; mutation removes the caster between votes.
  SB::Model dark_owner;
  dark_owner.mem.poke(0x600a8,4,0x41000); dark_owner.mem.poke(0x41004,4,0x600a8);
  dark_owner.mem.poke(0x4100c,4,0x30000); dark_owner.mem.poke(0x600ac,4,0x10000);
  dark_owner.typed_callbacks["world_collectObjectsInBox"]=[](auto &,const auto &)->SB::Model::Result {return std::vector<Raw>{0x71000};};
  std::vector<Raw> owners;
  dark_owner.callbacks["Creature.v20"]=[&](SB::Model &self,const auto &a){
    owners.push_back(a.at(0)); self.handle_clear(0x600a8); return Raw(1);
  };
  dark_owner.callbacks["Skill.v10"]=[](auto &,const auto &){return Raw(1);};
  refuses([&]{dark_owner.darkness_run(0x60000);}); // range slot 8 needs unsupported stack outputs
  require(owners==std::vector<Raw>({0x30000,0}) && dark_owner.mem.load(0x600a0,1)==1);
  require(dark_owner.mem.load(0x60094,4)==60 && dark_owner.events.back().name=="Creature.v20");

  // Sparks retains movement and private RNG prefixes when typed services refuse.
  SB::Model sparks;
  sparks.mem.poke(0x60054,4,1); sparks.mem.poke(0x60058,4,ONE);
  sparks.mem.poke(0x6001c,4,0x41400000);
  sparks.callbacks["obj_updateGridCell"]=[](auto &,const auto &){return Raw(0);};
  sparks.typed_callbacks["terrain_height"]=[](auto &,const auto &)->SB::Model::Result {
    return std::string("invalid height");
  };
  refuses([&]{sparks.sparks_run(0x60000);});
  require(sparks.mem.load(0x60010,4)==0x3c888888 && sparks.events.back().name=="terrain_height");
  sparks.typed_callbacks["terrain_height"]=[](auto &,const auto &)->SB::Model::Result {return Raw(0);};
  sparks.typed_callbacks["world_collectObjectsInBox"]=[](auto &,const auto &)->SB::Model::Result {return Raw(0);};
  refuses([&]{sparks.sparks_run(0x60000);}); // shared movement requires a list, even when empty
  sparks.typed_callbacks["world_collectObjectsInBox"]=[](auto &,const auto &)->SB::Model::Result {
    return std::vector<Raw>(creature_hp::MAX_HISTORY+1,0);
  };
  refuses([&]{sparks.sparks_run(0x60000);});
  sparks.typed_callbacks["world_collectObjectsInBox"]=[](auto &,const auto &)->SB::Model::Result {return std::vector<Raw>{};};
  refuses([&]{sparks.sparks_run(0x60000);});
  require(sparks.mem.load(0x60054,4)==1 && sparks.events.back().name=="voice_setPosition");
  sparks.mem.poke(0x60054,4,0);
  auto spark_seed=sparks.mem.load(0x60044,4);
  refuses([&]{sparks.sparks_run(0x60000);});
  require(sparks.mem.load(0x60044,4)!=spark_seed && sparks.events.back().name=="particle_spawn");
  unsigned spark_particles=0;
  sparks.typed_callbacks["particle_spawn"]=[&](auto &self,const auto &a)->SB::Model::Result {
    require(std::get<Raw>(a.at(0))==0x8e4100 && std::holds_alternative<SB::Model::Vec>(a.at(2)));
    ++spark_particles; self.typed_callbacks.erase("particle_spawn"); return Raw(0);
  };
  refuses([&]{sparks.sparks_run(0x60000);});
  require(spark_particles==1 && sparks.events.back().name=="particle_spawn");
  sparks.typed_callbacks["particle_spawn"]=[&](auto &,const auto &)->SB::Model::Result {++spark_particles;return Raw(0);};
  refuses([&]{sparks.sparks_run(0x60000);});
  require(spark_particles==51 && sparks.events.back().name=="Creature.v1");

  // Direct damage captures the owner before services but reads damage after them.
  SB::Model spark_hit;
  spark_hit.mem.poke(0x60054,4,700); spark_hit.mem.poke(0x6004c,4,0x41000);
  spark_hit.mem.poke(0x41004,4,0x6004c); spark_hit.mem.poke(0x4100c,4,0x30000);
  spark_hit.mem.poke(0x600ac,4,0xffffffff); spark_hit.mem.poke(0x600a8,4,0x12345678);
  spark_hit.callbacks["Creature.v20"]=[](auto &self,const auto &){self.handle_clear(0x6004c);return Raw(1);};
  spark_hit.callbacks["pool_new"]=[](auto &,const auto &){return Raw(0);};
  spark_hit.callbacks["res_find"]=[](auto &,const auto &){return Raw(0x77);};
  spark_hit.callbacks["sound_playOnObject"]=[](auto &,const auto &){return Raw(0);};
  refuses([&]{spark_hit.sparks_on_touch(0x60000,0x71000);});
  require(spark_hit.mem.load(0x60054,4)==700 && spark_hit.mem.load(0x6004c,4)==0);
  const auto &damage_args=spark_hit.events.back().args;
  require(spark_hit.events.back().name=="damage_apply" && std::get<Raw>(damage_args.at(1))==0x30000);
  require(std::get<SB::Model::Descriptor>(damage_args.at(6)).entries==
    std::vector<std::array<Raw,4>>({{0x1f,100,0xffff,0x5678}}));
  auto hit_events=spark_hit.events.size();
  spark_hit.sparks_on_touch(0x60000,0x72000);
  require(spark_hit.events.size()==hit_events); // owner is now gone
  spark_hit.mem.poke(0x6004c,4,0x41000); spark_hit.mem.poke(0x4100c,4,0x30000);
  spark_hit.typed_callbacks["damage_apply"]=[](auto &self,const auto &)->SB::Model::Result {
    self.mem.store(0x60054,4,999); self.typed_callbacks.erase("damage_apply"); return Raw(0);
  };
  spark_hit.sparks_on_touch(0x60000,0x71000);
  require(spark_hit.mem.load(0x60054,4)==0 && spark_hit.draws==0);

  // Constructor keeps caller input snapshots, delegated draw and pre-link writes.
  SB::Model spark_ctor;
  spark_ctor.mem.poke(0x600a0,4,123); spark_ctor.mem.poke(0x600a4,4,456);
  spark_ctor.mem.poke(0x50000,4,ONE); spark_ctor.mem.poke(0x5000c,4,TWO);
  spark_ctor.typed_callbacks["GameObject_ctor"]=[](auto &self,const auto &)->SB::Model::Result {
    self.mem.poke(0x50000,4,0); self.mem.poke(0x5000c,4,0); return Raw(0);
  };
  refuses([&]{spark_ctor.sparks_ctor(0x60000,0x50000,0x5000c,0,9,10,11,12,13,14);});
  require(spark_ctor.draws==1 && spark_ctor.mem.load(0x60094,4)==ONE);
  require(spark_ctor.mem.load(0x60058,4)==0 && spark_ctor.mem.load(0x600a0,4)==123 &&
    spark_ctor.mem.load(0x600a4,4)==456 && spark_ctor.events.back().name=="light_link");

  // Planar Wind refuses malformed services after retaining the reached prefix.
  SB::Model wind;
  wind.mem.poke(0x60024,4,ONE); wind.mem.poke(0x60034,4,0x44700000);
  wind.mem.poke(0x60020,4,0x41400000); wind.mem.poke(0x60068,4,53);
  refuses([&]{wind.planarwind_run(0x60000);});
  require(wind.mem.load(0x60018,4)==mul(0x44700000,0x3c888888) &&
          wind.mem.load(0x60068,4)==53 && wind.events.back().name=="light_relink");
  wind.callbacks["light_relink"]=[](auto &,const auto &){return Raw(0);};
  wind.typed_callbacks["terrain_height"]=[](auto &,const auto &)->SB::Model::Result {
    return std::vector<Raw>{};
  };
  refuses([&]{wind.planarwind_run(0x60000);});
  require(wind.mem.load(0x60068,4)==53 && wind.events.back().name=="terrain_height");
  wind.typed_callbacks["terrain_height"]=[](auto &,const auto &)->SB::Model::Result {return ZERO;};
  refuses([&]{wind.planarwind_run(0x60000);});
  require(wind.mem.load(0x60068,4)==54 && wind.events.back().name=="Spell.v1");
  int endings=0;
  wind.callbacks["Spell.v1"]=[&](auto &,const auto &a){
    require(a==std::vector<Raw>({0x60000,3})); ++endings; return Raw(0);
  };
  wind.planarwind_run(0x60000);
  require(endings==1 && wind.mem.load(0x60068,4)==55);

  SB::Model wind_hit;
  wind_hit.mem.poke(0x60008,4,0x10000); wind_hit.mem.poke(0x60060,4,101);
  wind_hit.callbacks["Skill.v12"]=[](auto &,const auto &){return Raw(100);};
  wind_hit.typed_callbacks["damage_apply"]=[](auto &,const auto &)->SB::Model::Result {return Raw(0);};
  wind_hit.callbacks["Creature_setEffectTimer"]=[](auto &,const auto &){return Raw(0);};
  wind_hit.callbacks["res_find"]=[](auto &,const auto &){return Raw(1);};
  wind_hit.typed_callbacks["sound_playAtPos"]=[](auto &,const auto &)->SB::Model::Result {return Raw(0);};
  wind_hit.callbacks["Creature.v16"]=[](auto &,const auto &){return Raw(0x90000);};
  wind_hit.callbacks["model_getSamplePointCount"]=[](auto &,const auto &){return Raw(0);};
  refuses([&]{wind_hit.planarwind_strike(0x60000,0x71000,{ONE,ZERO,ZERO});});
  require(wind_hit.draws==1 && wind_hit.mem.load(0x60010,4)!=0 &&
          wind_hit.events.back().name=="model_getSamplePointCount");
  wind_hit.callbacks["model_getSamplePointCount"]=[](auto &,const auto &){return Raw(1);};
  wind_hit.typed_callbacks["model_getSamplePoint"]=[](auto &,const auto &)->SB::Model::Result {return Raw(0);};
  refuses([&]{wind_hit.planarwind_strike(0x60000,0x71000,{ONE,ZERO,ZERO});});
  require(wind_hit.events.back().name=="model_getSamplePoint");
  wind_hit.typed_callbacks["model_getSamplePoint"]=[](auto &,const auto &)->SB::Model::Result {
    return SB::Model::SamplePoint{{ONE,ZERO,ZERO},Raw(0)};
  };
  unsigned hit_particles=0;
  wind_hit.typed_callbacks["particle_spawn"]=[&](auto &self,const auto &)->SB::Model::Result {
    ++hit_particles; self.typed_callbacks.erase("particle_spawn"); return Raw(0);
  };
  refuses([&]{wind_hit.planarwind_strike(0x60000,0x71000,{ONE,ZERO,ZERO});});
  require(hit_particles==1 && wind_hit.events.back().name=="particle_spawn");


  // Shadow Step: malformed and unavailable services retain exactly the reached prefix.
  SB::Model shadow;
  shadow.mem.poke(0x6000c,4,0x30000); shadow.mem.poke(0x6002c,4,0x30000);
  shadow.mem.poke(0x60020,4,0x43b90000); shadow.mem.poke(0x60030,2,0x8123);
  refuses([&]{shadow.shadowstep_run(0x60000);});
  require(shadow.mem.log().empty() && shadow.events.back().name=="Player_clampToScreen");
  shadow.typed_callbacks["Player_clampToScreen"]=[](auto &,const auto &)->SB::Model::Result {return std::vector<Raw>{ONE,ZERO};};
  refuses([&]{shadow.shadowstep_run(0x60000);});
  require(shadow.mem.log().empty());
  shadow.typed_callbacks["Player_clampToScreen"]=[](auto &,const auto &)->SB::Model::Result {return std::vector<Raw>{ONE,ZERO,ZERO};};
  refuses([&]{shadow.shadowstep_run(0x60000);});
  require(shadow.mem.load(0x300dc,4)==0xbf800000 && shadow.mem.load(0x60018,4)==0 &&
          shadow.events.back().name=="obj_moveWithCollision");
  unsigned moves=0;
  shadow.typed_callbacks["obj_moveWithCollision"]=[&](auto &self,const auto &args)->SB::Model::Result {
    require(std::get<SB::Model::Vec>(args.at(1))==SB::Model::Vec({HALF,ZERO,ZERO}));
    require(std::get<Raw>(args.at(0))==(moves?0x31000u:0x30000u));
    if (!moves++) self.mem.store(0x6002c,4,0x31000);
    return Raw(0);
  };
  shadow.mem.poke(0x310a8,8,0x8765432112345678ull);
  shadow.shadowstep_run(0x60000);
  require(moves==2 && shadow.mem.load(0x402797,1)==1 &&
          shadow.mem.load(0x310a8,8)==0x8765432112345678ull);
  // Captured movement vector persists; live mover is read again after each callback.
  shadow.typed_callbacks["obj_moveWithCollision"]=[](auto &self,const auto &)->SB::Model::Result {
    self.typed_callbacks.erase("obj_moveWithCollision"); return Raw(0);
  };
  refuses([&]{shadow.shadowstep_run(0x60000);});
  shadow.typed_callbacks["obj_moveWithCollision"]=[](auto &,const auto &)->SB::Model::Result {return Raw(0);};
  shadow.mem.poke(0x60018,4,ONE);
  refuses([&]{shadow.shadowstep_run(0x60000);});
  require(shadow.mem.load(0x30038,2)==0x8123 && shadow.events.back().name=="Spell.v1");
  SB::Model shadow_ctor;
  shadow_ctor.mem.poke(0x10060,4,0x20000);
  shadow_ctor.callbacks["res_find"]=[](auto &,const auto &){return Raw(1);};
  shadow_ctor.typed_callbacks["sound_playAtPos"]=[](auto &,const auto &)->SB::Model::Result {return Raw(0);};
  shadow_ctor.callbacks["Skill.v10"]=[](auto &,const auto &){return Raw(0);};
  shadow_ctor.typed_callbacks["world_collectObjectsInBox"]=[](auto &,const auto &)->SB::Model::Result {return std::vector<Raw>(creature_hp::MAX_HISTORY+1,0);};
  refuses([&]{shadow_ctor.shadowstep_spell_ctor(0x60000,6,0x30000,0x10000);});
  require(shadow_ctor.draws==1 && shadow_ctor.mem.load(0x3058c,4)==14 &&
          shadow_ctor.mem.load(0x3000c,4)==5 && shadow_ctor.events.back().name=="world_collectObjectsInBox");
  shadow_ctor.typed_callbacks["world_collectObjectsInBox"]=[](auto &,const auto &)->SB::Model::Result {return Raw(0);};
  shadow_ctor.callbacks["Creature.v16"]=[](auto &,const auto &){return Raw(0);};
  shadow_ctor.callbacks["model_getSamplePointCount"]=[](auto &,const auto &){return Raw(0);};
  refuses([&]{shadow_ctor.shadowstep_spell_ctor(0x60000,6,0x30000,0x10000);});
  require(shadow_ctor.draws==2 && shadow_ctor.events.back().name=="model_getSamplePointCount");
  SB::Model passives;
  passives.mem.poke(0x30544,4,0x90000);
  passives.callbacks["model_getSamplePointCount"]=[](auto &,const auto &){return Raw(0);};
  passives.callbacks["pool_new"]=[](auto &,const auto &){return Raw(0);};
  refuses([&]{passives.strongstance_trigger(0x10000,0x30000,0,0);});
  require(passives.mem.load(SB::G_SEED3,4)==1013904223 && passives.draws==0 &&
          passives.events.back().name=="pool_new");
  passives.callbacks["model_getSamplePointCount"]=[](auto &,const auto &){return Raw(1);};
  passives.typed_callbacks["model_getSamplePoint"]=[](auto &,const auto &)->SB::Model::Result {return Raw(0);};
  refuses([&]{passives.strongstance_trigger(0x10000,0x30000,0,0);});
  require(passives.events.back().name=="model_getSamplePoint");
  passives.typed_callbacks["model_getSamplePoint"]=[](auto &,const auto &)->SB::Model::Result {
    return SB::Model::SamplePoint{{ONE,HALF,ZERO},Raw(0)};
  };
  unsigned passive_particles=0;
  passives.typed_callbacks["particle_spawn"]=[&](auto &,const auto &)->SB::Model::Result {++passive_particles; return Raw(0);};
  passives.callbacks["pool_new"]=[](auto &,const auto &args){return args[0]==0x3d8 ? Raw(0x60000) : Raw(0);};
  refuses([&]{passives.strongstance_trigger(0x10000,0x30000,0xdeadbeef,0xffffffff);});
  require(passive_particles==25 && passives.draws==1 && passives.events.back().name=="StrongStancePhantom_ctor");
  passives.callbacks["StrongStancePhantom_ctor"]=[](auto &self,const auto &args) {
    self.callbacks.erase("StrongStancePhantom_ctor");
    if (args!=std::vector<Raw>{0x60000,0x30000,0xdeadbeef,0xffffffff,0}) throw std::runtime_error("phantom arguments");
    return Raw(0);
  };
  passives.strongstance_trigger(0x10000,0x30000,0xdeadbeef,0xffffffff);
  require(passive_particles==50 && passives.draws==2);
  passives.callbacks["pool_new"]=[](auto &,const auto &args)->Raw {
    if (args[0]==0x3d8) throw SB::Unsupported("second allocation failed");
    return 0;
  };
  refuses([&]{passives.strongstance_trigger(0x10000,0x30000,0,0);});
  require(passive_particles==75 && passives.draws==2 && passives.events.back().name=="pool_new");
  SB::Model stun_effect;
  stun_effect.stunningstrike_effect(0,0);
  stun_effect.stunningstrike_effect(0,0x30000);
  require(stun_effect.events.empty() && stun_effect.draws==0);
  stun_effect.mem.poke(0x3000c,4,0x20);
  refuses([&]{stun_effect.stunningstrike_effect(0,0x30000);});
  require(stun_effect.draws==0 && stun_effect.events.back().name=="res_find");
  stun_effect.callbacks["res_find"]=[](auto &,const auto &){return Raw(1);};
  stun_effect.typed_callbacks["sound_playAtPos"]=[](auto &,const auto &)->SB::Model::Result {return Raw(0);};
  stun_effect.callbacks["pool_new"]=[](auto &,const auto &){return Raw(0x60000);};
  refuses([&]{stun_effect.stunningstrike_effect(0,0x30000);});
  require(stun_effect.draws==1 && stun_effect.events.back().name=="LightEffect_ctor");
  stun_effect.typed_callbacks["LightEffect_ctor"]=[](auto &,const auto &)->SB::Model::Result {return Raw(0);};
  refuses([&]{stun_effect.stunningstrike_effect(0,0x30000);});
  require(stun_effect.draws==3 && stun_effect.events.back().name=="Creature.v30");
  unsigned stunned=0;
  stun_effect.callbacks["Creature.v30"]=[&](auto &self,const auto &args) {
    self.callbacks.erase("Creature.v30");
    require(args.size()==3 && args[0]==0x30000 && args[1]>=90 && args[1]<=149 && args[2]==1);
    ++stunned; return Raw(0);
  };
  stun_effect.stunningstrike_effect(0,0x30000);
  require(stunned==1 && stun_effect.draws==5);
#undef require
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
