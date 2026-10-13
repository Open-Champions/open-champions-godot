#pragma once
#include "champions/creature_hp.hpp"
#include <set>
#include "champions/core_math.hpp"

namespace champions::simulation::rta::skills_a {
using Address = creature_hp::Address;
using Memory = creature_hp::Memory;
using Values = std::vector<Raw>;
struct Unsupported : std::runtime_error { using std::runtime_error::runtime_error; };
inline constexpr Raw G_RAND_SEED = 0x404464, G_SKILL_TABLE = 0x41b170;
struct Call { unsigned slot; Values args; std::string name; };
// RTA Skill foundation, weapon buffs 12/14/26, shields 15, support 31/32/33, Shield Bash 34 and Fire Storm 7.
// No implicit engine services; callbacks own presentation, queries and damage.
// Memory and history caps are inherited from the standalone raw32 Memory.
class Model {
  std::size_t depth_ = 0;
  void write(Address, std::size_t, Raw);
  void write_vec(Address, const Vec3 &);
  Raw scalar(Raw skill, unsigned slot, Values args);
  Raw named_scalar(const std::string &, const Values &);
  Vec3 vec(Address) const;
  Raw signed_byte(Address) const;
public:
  Memory mem;
  std::vector<creature_hp::Write> writes;
  std::vector<Call> calls;
  std::uint64_t draws = 0, hidden_draws = 0;
  std::set<unsigned> external;
  using Callback = std::function<Values(Model &, const Values &)>;
  std::map<unsigned, Callback> callbacks;
  // Named callbacks flatten vectors into x,y,z lanes in original argument order.
  // Scalars return one lane, void returns empty, box queries return object lanes.
  std::map<std::string, Callback> named_callbacks;
  std::set<std::string> external_named;
  std::map<Raw, Raw> type_info; // caller-supplied getter -> type identity
  Values call(const std::string &, const Values &);
  Values table_call(const std::string &, Raw obj, unsigned offset, unsigned slot, const Values &);
  Values spell_call(Raw spell, unsigned slot, const Values &);
  Raw spell_rand(Raw);
  void spell_base(Raw spell, Raw level, Raw owner, Raw skill, Raw table);
  Raw spell_type(Raw);
  Raw find_spell_by_type(Raw creature, Raw type);
  Raw carries(Raw creature, Raw getter);
  void add_spell(Raw creature, Raw spell);
  Values weapons_of(Raw player) const;
  void enchant(Raw item, Raw kind, Raw value, Raw frames, Raw limit);
  Raw weaponbuff_can_use(unsigned sid, Raw caster);
  Raw weaponbuff_cast(unsigned sid, Raw skill, Raw level, Raw caster);
  void weaponbuff_ctor(unsigned sid, Raw spell, Raw level, Raw owner, Raw skill);
  void weaponbuff_run(unsigned sid, Raw spell, const Vec3 &pos, Raw facing = 0);
  Raw lightningweapons_can_use(Raw, Raw caster) { return weaponbuff_can_use(12, caster); }
  Raw lightningweapons_cast(Raw skill, Raw level, Raw caster) { return weaponbuff_cast(12, skill, level, caster); }
  void lightningweapons_run(Raw spell, const Vec3 &pos, Raw facing = 0) { weaponbuff_run(12, spell, pos, facing); }
  Raw coldweapons_can_use(Raw, Raw caster) { return weaponbuff_can_use(14, caster); }
  Raw coldweapons_cast(Raw skill, Raw level, Raw caster) { return weaponbuff_cast(14, skill, level, caster); }
  void coldweapons_run(Raw spell, const Vec3 &pos, Raw facing = 0) { weaponbuff_run(14, spell, pos, facing); }
  Raw poisonweapons_can_use(Raw, Raw caster) { return weaponbuff_can_use(26, caster); }
  Raw poisonweapons_cast(Raw skill, Raw level, Raw caster) { return weaponbuff_cast(26, skill, level, caster); }
  void poisonweapons_run(Raw spell, const Vec3 &pos, Raw facing = 0) { weaponbuff_run(26, spell, pos, facing); }
  Raw undeadshield_can_use(Raw skill, Raw caster);
  Raw undeadshield_cast(Raw skill, Raw level, Raw caster);
  void undeadshield_ctor(Raw spell, Raw level, Raw owner, Raw skill);
  void shield_glow(Raw spell, unsigned group);
  void undeadshield_run(Raw spell, const Vec3 &pos, Raw facing = 0);
  void undeadshield_damage_attacker(Raw spell, Raw victim, Raw attacker, Raw incoming);
  void shield_reflect(Raw spell, Raw victim, Raw attacker, Raw incoming, Raw percent);
  void secondary_shield_ctor(Raw spell, Raw percent, Raw victim, Raw seconds);
  void secondary_shield_run(Raw spell);
  void secondary_shield_damage_attacker(Raw spell, Raw victim, Raw attacker, Raw incoming);
  Vec3 spell_ball(Raw spell);
  void flash_both(Raw creature, Raw a, Raw b, Raw c, Raw d);
  void spell_delete(Raw spell);
  // Arc query/clear callbacks use STACK_QUERY as the authored stack-list token.
  static constexpr Raw STACK_QUERY = UINT32_MAX;
  Raw player_object(Raw slot) const;
  Raw root_can_use(Raw skill, Raw caster);
  Raw root_cast(Raw skill, Raw level, Raw caster, const Vec3 &pos, Raw facing);
  void root_spell_ctor(Raw spell, Raw level, Raw victim, Raw skill);
  void root_spell_run(Raw spell);
  Raw crit_can_use(Raw skill, Raw caster);
  Raw crit_cast(Raw skill, Raw level, Raw caster);
  void crit_spell_ctor(Raw spell, Raw level, Raw owner, Raw skill);
  void crit_set_tint(Raw spell, Raw creature);
  void crit_spell_run(Raw spell);
  Raw crit_bonus_for(Raw skill, Raw attacker, Raw rank);
  Raw shieldbash_can_use(Raw skill, Raw caster);
  Raw shieldbash_cast(Raw skill, Raw level, Raw caster);
  void shieldbash_ctor(Raw spell, Raw level, Raw owner, Raw skill);
  void shieldbash_windup_fx(Raw spell);
  void shieldbash_run(Raw spell);
  Raw shieldbash_done(Raw spell) const;
  Raw light_effect(Raw block_size, const Vec3 &pos, const Values &numbers);
  Raw obj_rand(Raw obj);
  Vec3 obj_ball(Raw obj);
  void delete_self(Raw obj);
  void gameobject_ctor(Raw obj, const Vec3 &pos, Raw definition, Raw props);
  Raw firestorm_can_use(Raw skill, Raw caster);
  Raw firestorm_cast(Raw skill, Raw level, Raw caster, const Vec3 &pos, Raw facing);
  void firestorm_base(Raw obj, const Vec3 &pos, Raw owner);
  void firestorm_ctor(Raw obj, Raw level, Raw owner, const Vec3 &pos, Raw skill);
  void firestorm_ctor2(Raw obj, Raw owner, const Vec3 &pos, Raw radius, Raw damage, Raw duration, Raw period, Raw scale);
  void firestorm_init_common(Raw obj, Raw owner);
  void firestorm_run(Raw obj);
  Model() = default;
  Model(const Model &) = delete;
  Model &operator=(const Model &) = delete;
  // Callback receives adjusted this followed by low32 arguments. Copying the
  // callable supports replacement during execution. Exceptions preserve state.
  // Missing/unknown derived bodies refuse; no fabricated zero callback result.
  Values skill_call(Raw skill, unsigned slot, const Values &args);
  Raw skill_by_id(Raw id) const;
  Raw skill_can_use_base(Raw skill, Raw caster = 0) const;
  Raw skill_cast_base(Raw skill, Raw level = 0, Raw caster = 0) const;
  Raw skill_mana_base(Raw skill, Raw level = 0) const;
  void skill_init_stats_base(Raw skill);
  Raw skill_stat(Raw skill, Raw stat, Raw level, Raw unused = 0) const;
  Raw skill_stat_tier2(Raw skill, Raw stat, Raw level, Raw cap) const;
  Raw skill_stat_for(Raw skill, Raw stat, Raw level, Raw caster, Raw cap);
  Raw skill_stat_for_handle(Raw skill, Raw stat, Raw level, Raw handle, Raw cap);
  Raw skill_adjust_level(Raw skill, Raw level, Raw n = 0) const;
  Raw skill_mana_from_stat4(Raw skill, Raw level, Raw n);
  Values skill_stat_range(Raw skill, Raw stat, Raw level, Raw randomise, Raw spread);
  Raw passive_cast(Raw skill, Raw level = 0, Raw caster = 0) const;
  Raw dodge_fraction(Raw skill, Raw rank);
  Raw regeneration_stat0(Raw skill, Raw level);
  Raw mana_cost_at(Raw function, Raw skill, Raw level);
  Raw draw();
};
} // namespace champions::simulation::rta::skills_a
