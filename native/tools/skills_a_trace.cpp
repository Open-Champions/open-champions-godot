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
  for (const auto &c : m.calls) { if (!first) out << ';'; first=false; if (c.name.empty()) out << c.slot; else out << c.name; out << ':' << enc(c.args); }
  if (first) out << '-';
  out << '|' << enc({Raw(m.draws),Raw(m.hidden_draws)});
  return out.str();
}
S::Values execute(S::Model &m, const std::string &op, const S::Values &a) {
  const auto at = [&](std::size_t i) { return a.at(i); };
  if (op=="image_read" || op=="image_cstr") {
    S::ImageExe image([&](S::Address at,std::size_t n) { return Raw(m.mem.load(at,n)); });
    const auto bytes=op=="image_read" ? image.read(at(0),s32(at(1))) : image.cstr(at(0));
    S::Values result;
    for (const auto byte:bytes) result.push_back(static_cast<unsigned char>(byte));
    return result;
  }
  if (op=="shared_draws") {
    S::SharedDraws draws(m); S::Values result;
    if (at(0)>65536) throw S::Unsupported("draw result limit");
    for (Raw i=0;i<at(0);++i) result.push_back(draws.next());
    return result;
  }
  if (op=="descriptor") {
    std::vector<S::Values> entries;
    for (std::size_t i=0;i<a.size();) {
      const auto count=at(i++);
      if (count>a.size()-i) throw S::Unsupported("descriptor trace input");
      entries.emplace_back(a.begin()+i,a.begin()+i+count); i+=count;
    }
    S::Values result;
    for (const auto &slot:S::descriptor(entries)) {
      if (slot) result.insert(result.end(),slot->begin(),slot->end());
      else result.push_back(UINT32_MAX);
    }
    return result;
  }
  if (op=="summon_can_use") return {m.summon_can_use(at(0),at(1))};
  if (op=="summon_cast") return {m.summon_cast(at(0),at(1),at(2),{at(3),at(4),at(5)},at(6))};
  if (op=="_summon_sample") { const auto p=m.summon_sample(at(0),at(1)); return {p[0],p[1],p[2]}; }
  if (op=="skelight_ctor") { m.skelight_ctor(at(0),{at(1),at(2),at(3)},at(4),at(5),at(6)); return {}; }
  if (op=="summon_spell_ctor") { m.summon_spell_ctor(at(0),at(1),at(2),at(3),at(4)); return {}; }
  if (op=="summon_spell_run") { m.summon_spell_run(at(0)); return {}; }
  if (op=="frame") return {m.frame()};
  if (op=="is_player") return {Raw(m.is_player(at(0)))};
  if (op=="mana_cost") return {m.mana_cost(at(0),at(1))};
  if (op=="can_use_slot") return {m.can_use_slot(at(0),at(1))};
  if (op=="first_skill_call") return m.first_skill_call(at(0),at(1),S::Values(a.begin()+2,a.end()));
  if (op=="carries_getter") {
    const auto getter=S::carries_getter([&](S::Address at) { return Raw(m.mem.load(at,4)); },at(0));
    return getter ? S::Values{*getter} : S::Values{};
  }
  if (op=="lcg") return {S::lcg(at(0))};
  if (op=="unit") return {S::unit(at(0))};
  if (op=="frac") return {S::frac(at(0))};
  if (op=="round_half") return {S::round_half(at(0))};
  if (op=="spread_max") return {S::spread_max(at(0),at(1),a.size()>2 ? at(2) : 0x3e4ccccc)};
  if (op=="rem") return {S::rem(at(0),at(1))};
  if (op=="quot") return {S::quot(at(0),at(1))};
  if (op=="half_toward_zero") return {S::half_toward_zero(at(0))};
  if (op=="cosmetic_draw") return {m.cosmetic_draw()};
  if (op=="_cosmetic_ball") { const auto v=m.cosmetic_ball(); return {v[0],v[1],v[2]}; }
  if (op=="riposte_counter_attack") { m.riposte_counter_attack(at(0),at(1),at(2),at(3)); return {}; }
  if (op=="item_tables") { const auto &t=m.item_tables(); return {Raw(t.defs.size()),Raw(t.presets.size()),Raw(t.prefixes.size()),Raw(t.suffixes.size()),Raw(t.formats.size())}; }
  if (op=="item_parse") { m.item_parse(at(0),at(1)); return {}; }
  if (op=="item_resource_index") return {m.item_resource_index(at(0))};
  if (op=="hammer_can_use") return {m.hammer_can_use(at(0),at(1))};
  if (op=="hammer_cast") return {m.hammer_cast(at(0),at(1),at(2),{at(3),at(4),at(5)},at(6))};
  if (op=="hammer_ctor") { m.hammer_ctor(at(0),{at(1),at(2),at(3)},at(4),at(5),at(6),at(7)); return {}; }
  if (op=="hammer_spell_ctor") { m.hammer_spell_ctor(at(0),at(1),at(2),at(3),at(4)); return {}; }
  if (op=="hammer_spell_run") { m.hammer_spell_run(at(0)); return {}; }
  if (op=="_hammer_pull") { m.hammer_pull(at(0),{at(1),at(2),at(3)},at(4)); return {}; }
  if (op=="_hammer_blow") { m.hammer_blow(at(0),at(1),at(2)); return {}; }
  if (op=="hammer_run") { m.hammer_run(at(0)); return {}; }
  if (op=="seed3_draw") return {m.seed3_draw()};
  if (op=="poisonrain_can_use") return {m.poisonrain_can_use(at(0),at(1))};
  if (op=="poisonrain_cast") return {m.poisonrain_cast(at(0),at(1),at(2),{at(3),at(4),at(5)},at(6))};
  if (op=="poisonrain_ctor") { m.poisonrain_ctor(at(0),at(1),at(2),{at(3),at(4),at(5)},at(6),at(7)); return {}; }
  if (op=="poisonrain_run") { m.poisonrain_run(at(0)); return {}; }
  if (op=="repulse_cast") return {m.repulse_cast(at(0),at(1),at(2),{at(3),at(4),at(5)},at(6))};
  if (op=="damage_radial") { m.damage_radial(at(0),{at(1),at(2),at(3)},at(4),at(5),at(6),at(7),at(8),at(9)); return {}; }
  if (op=="dismissundead_cast") return {m.dismissundead_cast(at(0),at(1),at(2),{at(3),at(4),at(5)},at(6))};
  if (op=="holybolt_ctor") { m.holybolt_ctor(at(0),{at(1),at(2),at(3)},at(4),at(5),at(6),at(7)); return {}; }
  if (op=="holybolt_run") { m.holybolt_run(at(0)); return {}; }
  if (op=="holybolt_hit") { m.holybolt_hit(at(0),at(1)); return {}; }
  if (op=="blessed_can_use") return {m.blessed_can_use(at(0),at(1))};
  if (op=="blessed_cast") return {m.blessed_cast(at(0),at(1),at(2))};
  if (op=="blessed_ctor") { m.blessed_ctor(at(0),at(1),at(2),at(3)); return {}; }
  if (op=="blessed_dtor") { m.blessed_dtor(at(0),at(1)); return {}; }
  if (op=="blessed_stat0") return {m.blessed_stat0(at(0))};
  if (op=="blessed_run") { m.blessed_run(at(0)); return {}; }
  if (op=="wizardbeam_cast") return {m.wizardbeam_cast(at(0),at(1),at(2))};
  if (op=="wizardbeam_ctor") { m.wizardbeam_ctor(at(0),at(1),at(2),at(3)); return {}; }
  if (op=="wizardbeam_run") { m.wizardbeam_run(at(0),{at(1),at(2),at(3)},at(4)); return {}; }
  if (op=="_beam_flash") return m.beam_flash(at(0),at(1));
  if (op=="_beam_chain") { m.beam_chain(at(0),at(1),at(2),{at(3),at(4),at(5)},at(6)); return {}; }
  if (op=="fireweapons_can_use") return {m.fireweapons_can_use(at(0),at(1))};
  if (op=="fireweapons_cast") return {m.fireweapons_cast(at(0),at(1),at(2))};
  if (op=="fireweapons_ctor") { m.fireweapons_ctor(at(0),at(1),at(2),at(3)); return {}; }
  if (op=="fireweapons_run") { m.fireweapons_run(at(0),{at(1),at(2),at(3)},at(4)); return {}; }
  if (op=="coneoffire_can_use") return {m.coneoffire_can_use(at(0),at(1))};
  if (op=="coneoffire_cast") return {m.coneoffire_cast(at(0),at(1),at(2))};
  if (op=="coneoffire_ctor") { m.coneoffire_ctor(at(0),at(1),at(2),at(3)); return {}; }
  if (op=="coneoffire_run") { m.coneoffire_run(at(0),{at(1),at(2),at(3)},at(4)); return {}; }
  if (op=="diseaseshield_can_use") return {m.diseaseshield_can_use(at(0),at(1))};
  if (op=="diseaseshield_cast") return {m.diseaseshield_cast(at(0),at(1),at(2))};
  if (op=="diseaseshield_ctor") { m.diseaseshield_ctor(at(0),at(1),at(2),at(3)); return {}; }
  if (op=="disease_spell_ctor") { m.disease_spell_ctor(at(0),at(1),at(2),at(3),at(4),at(5)); return {}; }
  if (op=="diseaseshield_run") { m.diseaseshield_run(at(0),{at(1),at(2),at(3)},at(4)); return {}; }
  if (op=="disease_spell_run") { m.disease_spell_run(at(0),{at(1),at(2),at(3)},at(4)); return {}; }
  if (op=="blindinglight_can_use") return {m.blindinglight_can_use(at(0),at(1))};
  if (op=="blindinglight_cast") return {m.blindinglight_cast(at(0),at(1),at(2),{at(3),at(4),at(5)},at(6))};
  if (op=="blindinglight_effect_ctor") { m.blindinglight_effect_ctor(at(0),{at(1),at(2),at(3)},at(4),at(5),at(6)); return {}; }
  if (op=="blindinglight_effect_run") { m.blindinglight_effect_run(at(0)); return {}; }
  if (op=="blindinglight_spell_ctor") { m.blindinglight_spell_ctor(at(0),at(1),at(2),at(3),at(4)); return {}; }
  if (op=="blindinglight_spell_run") { m.blindinglight_spell_run(at(0)); return {}; }
  if (op=="diseasebolt_can_use") return {m.diseasebolt_can_use(at(0),at(1))};
  if (op=="diseasebolt_cast") return {m.diseasebolt_cast(at(0),at(1),at(2),{at(3),at(4),at(5)},at(6))};
  if (op=="diseasebolt_ctor") { m.diseasebolt_ctor(at(0),at(1),at(2),{at(3),at(4),at(5)},at(6),at(7)); return {}; }
  if (op=="diseasebolt_run") { m.diseasebolt_run(at(0)); return {}; }
  if (op=="diseasebolt_hit") { m.diseasebolt_hit(at(0),at(1)); return {}; }
  if (op=="holystrike_can_use") return {m.holystrike_can_use(at(0),at(1))};
  if (op=="holystrike_cast") return {m.holystrike_cast(at(0),at(1),at(2),{at(3),at(4),at(5)},at(6))};
  if (op=="holystrike_ctor") { m.holystrike_ctor(at(0),at(1),at(2),{at(3),at(4),at(5)},at(6),at(7)); return {}; }
  if (op=="holystrike_run") { m.holystrike_run(at(0)); return {}; }
  if (op=="holystrike_hit") { m.holystrike_hit(at(0),at(1)); return {}; }
  if (op=="fireflares_can_use") return {m.fireflares_can_use(at(0),at(1))};
  if (op=="fireflares_cast") return {m.fireflares_cast(at(0),at(1),at(2),{at(3),at(4),at(5)},at(6))};
  if (op=="fireflares_ctor") { m.fireflares_ctor(at(0),at(1),at(2),{at(3),at(4),at(5)},at(6)); return {}; }
  if (op=="fireflares_dtor") { m.fireflares_dtor(at(0),at(1)); return {}; }
  if (op=="fireflares_run") { m.fireflares_run(at(0),{at(1),at(2),at(3)},at(4)); return {}; }
  if (op=="fireflare_ctor") { m.fireflare_ctor(at(0),{at(1),at(2),at(3)},at(4),at(5),at(6),at(7)); return {}; }
  if (op=="fireflare_run") { m.fireflare_run(at(0)); return {}; }
  if (op=="spell_dtor_call") return m.spell_call(at(0),1,{at(1)});
  if (op=="shockoffrost_can_use") return {m.shockoffrost_can_use(at(0),at(1))};
  if (op=="shockoffrost_cast") return {m.shockoffrost_cast(at(0),at(1),at(2),{at(3),at(4),at(5)},at(6))};
  if (op=="shockoffrost_ctor") { m.shockoffrost_ctor(at(0),{at(1),at(2),at(3)},at(4),at(5),at(6),at(7)); return {}; }
  if (op=="frostshard_ctor") { m.frostshard_ctor(at(0),{at(1),at(2),at(3)},at(4),at(5),at(6),at(7),at(8)); return {}; }
  if (op=="projectile_ctor") { m.projectile_ctor(at(0),{at(1),at(2),at(3)},at(4),at(5)); return {}; }
  if (op=="move_and_collide") return {m.move_and_collide(at(0),{at(1),at(2),at(3)})};
  if (op=="_frost_debris") { m.frost_debris(at(0),at(1)); return {}; }
  if (op=="_frost_hit") return {Raw(m.frost_hit(at(0),at(1),at(2),at(3),at(4)))};
  if (op=="shockoffrost_run") { m.shockoffrost_run(at(0)); return {}; }
  if (op=="frostshard_run") { m.frostshard_run(at(0)); return {}; }
  if (op=="shockoffrost_hit") { m.shockoffrost_hit(at(0),at(1)); return {}; }
  if (op=="frostshard_hit") { m.frostshard_hit(at(0),at(1)); return {}; }
  if (op=="puff") { m.puff([&] { return m.obj_ball(at(0)); },at(1),{at(2),at(3),at(4)},at(5) ? std::optional<Raw>(at(6)) : std::nullopt,at(7)); return {}; }
  if (op=="diseasetrail_can_use") return {m.diseasetrail_can_use(at(0),at(1))};
  if (op=="diseasetrail_cast") return {m.diseasetrail_cast(at(0),at(1),at(2),{at(3),at(4),at(5)},at(6))};
  if (op=="diseasetrail_spell_ctor") { m.diseasetrail_spell_ctor(at(0),{at(1),at(2),at(3)},at(4),at(5),at(6)); return {}; }
  if (op=="diseasetrail_spell_run") { m.diseasetrail_spell_run(at(0),{at(1),at(2),at(3)},at(4)); return {}; }
  if (op=="diseasetrail_ctor") { m.diseasetrail_ctor(at(0),at(1),{at(2),at(3),at(4)},{at(5),at(6),at(7)},at(8),at(9)); return {}; }
  if (op=="diseasetrail_run") { m.diseasetrail_run(at(0)); return {}; }
  if (op=="_trail_puffs") {
    m.trail_puffs([&] { return at(1) ? m.spell_rand(at(0)) : m.obj_rand(at(0)); },
      [&] { return at(1) ? m.spell_ball(at(0)) : m.obj_ball(at(0)); },{at(2),at(3),at(4)},{at(5),at(6),at(7)}); return {};
  }
  if (op=="flash2") { m.flash2(at(0),at(1),at(2),at(3),at(4)); return {}; }
  if (op=="box_around") return m.box_around({at(0),at(1),at(2)},at(3),at(4));
  if (op=="obj_ball") { const auto b=m.obj_ball(at(0)); return {b.begin(),b.end()}; }
  if (op=="delete_self") { m.delete_self(at(0)); return {}; }
  if (op=="gameobject_ctor") { m.gameobject_ctor(at(0),{at(1),at(2),at(3)},at(4),at(5)); return {}; }
  if (op=="named_call") return m.call("Creature.v16",a);
  if (op=="generic_table") return m.table_call(at(0)==0 ? "Spell" : "Creature",at(1),at(2),at(3),S::Values(a.begin()+4,a.end()));
  if (op=="model_init") return {Raw(m.draws),Raw(m.hidden_draws),Raw(m.calls.size()),Raw(m.writes.size())};
  if (op=="object_call") return m.vcall(at(0),at(1),S::Values(a.begin()+2,a.end()));
  if (op=="firestorm_can_use") return {m.firestorm_can_use(at(0),at(1))};
  if (op=="firestorm_cast") return {m.firestorm_cast(at(0),at(1),at(2),{at(3),at(4),at(5)},at(6))};
  if (op=="_firestorm_base") { m.firestorm_base(at(0),{at(1),at(2),at(3)},at(4)); return {}; }
  if (op=="firestorm_ctor") { m.firestorm_ctor(at(0),at(1),at(2),{at(3),at(4),at(5)},at(6)); return {}; }
  if (op=="firestorm_ctor2") { m.firestorm_ctor2(at(0),at(1),{at(2),at(3),at(4)},at(5),at(6),at(7),at(8),at(9)); return {}; }
  if (op=="firestorm_init_common") { m.firestorm_init_common(at(0),at(1)); return {}; }
  if (op=="firestorm_run") { m.firestorm_run(at(0)); return {}; }
  if (op=="shieldbash_can_use") return {m.shieldbash_can_use(at(0),at(1))};
  if (op=="shieldbash_cast") return {m.shieldbash_cast(at(0),at(1),at(2))};
  if (op=="shieldbash_ctor") { m.shieldbash_ctor(at(0),at(1),at(2),at(3)); return {}; }
  if (op=="shieldbash_windup_fx") { m.shieldbash_windup_fx(at(0)); return {}; }
  if (op=="shieldbash_run") { m.shieldbash_run(at(0)); return {}; }
  if (op=="shieldbash_done") return {m.shieldbash_done(at(0))};
  if (op=="light_effect") return {m.light_effect(at(0),{at(1),at(2),at(3)},S::Values(a.begin()+4,a.end()))};
  if (op=="player_object") return {m.player_object(at(0))};
  if (op=="root_can_use") return {m.root_can_use(at(0),at(1))};
  if (op=="root_spell_ctor") { m.root_spell_ctor(at(0),at(1),at(2),at(3)); return {}; }
  if (op=="root_spell_run") { m.root_spell_run(at(0)); return {}; }
  if (op=="crit_can_use") return {m.crit_can_use(at(0),at(1))};
  if (op=="crit_cast") return {m.crit_cast(at(0),at(1),at(2))};
  if (op=="crit_spell_ctor") { m.crit_spell_ctor(at(0),at(1),at(2),at(3)); return {}; }
  if (op=="crit_set_tint") { m.crit_set_tint(at(0),at(1)); return {}; }
  if (op=="crit_spell_run") { m.crit_spell_run(at(0)); return {}; }
  if (op=="crit_bonus_for") return {m.crit_bonus_for(at(0),at(1),at(2))};
  if (op=="root_cast") return {m.root_cast(at(0),at(1),at(2),{at(3),at(4),at(5)},at(6))};
  if (op=="lightningweapons_can_use") return {m.lightningweapons_can_use(at(0),at(1))};
  if (op=="lightningweapons_cast") return {m.lightningweapons_cast(at(0),at(1),at(2))};
  if (op=="lightningweapons_run") { m.lightningweapons_run(at(0),{at(1),at(2),at(3)},at(4)); return {}; }
  if (op=="coldweapons_can_use") return {m.coldweapons_can_use(at(0),at(1))};
  if (op=="coldweapons_cast") return {m.coldweapons_cast(at(0),at(1),at(2))};
  if (op=="coldweapons_run") { m.coldweapons_run(at(0),{at(1),at(2),at(3)},at(4)); return {}; }
  if (op=="poisonweapons_can_use") return {m.poisonweapons_can_use(at(0),at(1))};
  if (op=="poisonweapons_cast") return {m.poisonweapons_cast(at(0),at(1),at(2))};
  if (op=="poisonweapons_run") { m.poisonweapons_run(at(0),{at(1),at(2),at(3)},at(4)); return {}; }
  if (op=="undeadshield_can_use") return {m.undeadshield_can_use(at(0),at(1))};
  if (op=="undeadshield_cast") return {m.undeadshield_cast(at(0),at(1),at(2))};
  if (op=="undeadshield_ctor") { m.undeadshield_ctor(at(0),at(1),at(2),at(3)); return {}; }
  if (op=="undeadshield_run") { m.undeadshield_run(at(0),{at(1),at(2),at(3)},at(4)); return {}; }
  if (op=="undeadshield_damage_attacker") { m.undeadshield_damage_attacker(at(0),at(1),at(2),at(3)); return {}; }
  if (op=="secondary_shield_ctor") { m.secondary_shield_ctor(at(0),at(1),at(2),at(3)); return {}; }
  if (op=="secondary_shield_run") { m.secondary_shield_run(at(0)); return {}; }
  if (op=="secondary_shield_damage_attacker") { m.secondary_shield_damage_attacker(at(0),at(1),at(2),at(3)); return {}; }
  if (op=="_shield_glow") { m.shield_glow(at(0),at(1)); return {}; }
  if (op=="_shield_reflect") { m.shield_reflect(at(0),at(1),at(2),at(3),at(4)); return {}; }
  if (op=="spell_ball") { const auto b=m.spell_ball(at(0)); return {b.begin(),b.end()}; }
  if (op=="spell_delete") { m.spell_delete(at(0)); return {}; }
  if (op=="flash_both") { m.flash_both(at(0),at(1),at(2),at(3),at(4)); return {}; }
  if (op=="weaponbuff_can_use") return {m.weaponbuff_can_use(at(0),at(1))};
  if (op=="weaponbuff_cast") return {m.weaponbuff_cast(at(0),at(1),at(2),at(3))};
  if (op=="weaponbuff_ctor") { m.weaponbuff_ctor(at(0),at(1),at(2),at(3),at(4)); return {}; }
  if (op=="weaponbuff_run") { m.weaponbuff_run(at(0),at(1),{at(2),at(3),at(4)},at(5)); return {}; }
  if (op=="spell_call") return m.spell_call(at(0),at(1),S::Values(a.begin()+2,a.end()));
  if (op=="spell_base") { m.spell_base(at(0),at(1),at(2),at(3),at(4)); return {}; }
  if (op=="spell_rand") return {m.spell_rand(at(0))};
  if (op=="spell_type") return {m.spell_type(at(0))};
  if (op=="find_spell_by_type") return {m.find_spell_by_type(at(0),at(1))};
  if (op=="carries") return {m.carries(at(0),at(1))};
  if (op=="add_spell") { m.add_spell(at(0),at(1)); return {}; }
  if (op=="weapons_of") return m.weapons_of(at(0));
  if (op=="enchant") { m.enchant(at(0),at(1),at(2),at(3),at(4)); return {}; }
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
      m.legacy_constructor_vectors = true;
      if (f[3]!="-") for (const auto &p : split(f[3],';')) {
        const auto v=nums(p); if (v.size()!=3) throw std::runtime_error("memory patch");
        m.mem.poke(v[0],v[1],v[2]);
      }
      if (f[4]!="-") for (const auto &s : split(f[4],';')) {
        const auto v=nums(s); if (v.size()!=6) throw std::runtime_error("callback script");
        static const std::map<Raw,std::string> names{{100,"pool_new"},{101,"LightEffect_ctor"},
          {102,"res_find"},{103,"sound_playAtPos"},{104,"world_collectObjectsInBox"},
          {105,"particle_spawn"},{106,"fx_shockModel"},{107,"Creature.v16"},{108,"Creature.v22"},
          {109,"Holder.v4"},{110,"Holder.v5"},{111,"Holder.v6"},{112,"Holder.v8"},
          {113,"Spell.v1"},{114,"Spell.v0"},{115,"handle_link"},{116,"Spell.v2"},{117,"light_link"},{118,"light_relink"},{119,"light_unlink"},
          {120,"particleGroup_create"},{121,"particleGroup_setMatrix"},{122,"particleGroup_begin"},
          {123,"particleGroup_end"},{124,"model_getSamplePointCount"},{125,"model_getSamplePoint"},
          {126,"sound_release"},{127,"damage_apply"},{128,"text_lookup"},{129,"hud_showMessage"},
          {130,"sound_playOnObject"},{131,"world_collectObjectsInArc"},{132,"Creature.v20"},
          {133,"creature_bossRank"},{134,"handle_clear"},{135,"net_objOrGhost"},
          {136,"terrain_heightAtPoint"},{137,"player_getSkillRank"},{138,"anim_startBlendInOut"},{139,"Player_animStep"},
          {140,"Player_move"},{141,"Creature.v26"},{142,"Creature.v30"},{143,"model_getAttachPoint"},
          {144,"world_collectInSphere"},{145,"net_isLocalSlot"},{146,"terrain_height"},{147,"obj_getPropInt"},{148,"obj_runEveryFrame"},{149,"FireStormFlare_ctor"},{150,"Creature.v1"},{151,"obj_updateGridCell"},{152,"water_test"},{153,"libc_sprintf"},{154,"water_disturb"},{155,"PhProp_ctor"},{156,"PhProp_initPhysics"},{157,"fx_spawn"},{158,"terrain_raycast"},{159,"Creature.v6"},{160,"sound_stop"},{161,"light_dtor"},{162,"pool_free"},{163,"trail_create"},{164,"trail_addPoint"},{165,"gfx_setColorOffset"},{166,"GlowingShaft_ctor"},{167,"Creature.v31"},{168,"glow_add"},{169,"Holder.v2"},{170,"trail_setPoints_1918a0"},{171,"trail_clear_191e58"},{172,"curve_sample_1911e8"},{173,"ring_draw_1920e0"},{174,"Creature_getVisualPos"},{175,"voice_setPosition"},{176,"LostSoul_ctor"},{177,"Creature.v2"},{178,"model_transformSamplePoint"},{179,"world_findObjectByName"},{180,"world_findFreeSpot"},{181,"Skelight_ctor"},{182,"Creature_setCharm"},{183,"Creature.v37"},{184,"Creature.v34"},{185,"Holder.v6"}};
        if (v[2]==6) { m.type_info[v[1]]=v[3]; continue; }
        S::Model::Callback cb=[v](S::Model &model,const S::Values &) mutable -> S::Values {
          if (v[2]==1 || v[2]==2 || v[2]==8 || v[2]==12) model.mem.poke(v[3],v[4],v[5]);
          if (v[2]==2) throw std::logic_error("authored callback failure");
          if (v[2]==8) return {v[1]};
          if (v[2]==12) return {v[1],TWO,0x40400000};
          if (v[2]==13) return {v[1],TWO,0x40400000,v[5]};
          if (v[2]==11) return {v[1],v[3],v[5],ONE,ZERO,ZERO};
          if (v[2]==9 || v[2]==10) return {v[1],v[3],v[5]};
          if (v[2]==3) return {v[1],v[5]};
          // The Python script's scalar zero means an empty query, whereas [0]
          // (mode 8) is a nonempty list containing a null address.
          if ((v[0]==104 || v[0]==131 || v[0]==144) && v[1]==0 && (v[2]==0 || v[2]==1)) return {};
          if (v[2]==7) return {};
          return {v[1]};
        };
        // Sequence state is owned separately because Model copies callbacks.
        if (v[2]==4) {
          auto index=std::make_shared<unsigned>(0);
          cb = [v, index](S::Model &,const S::Values &) mutable -> S::Values { return {(*index)++ ? v[5] : v[1]}; };
        }
        if (names.count(v[0])) m.named_callbacks[names.at(v[0])]=cb;
        else m.callbacks[v[0]]=cb;
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
