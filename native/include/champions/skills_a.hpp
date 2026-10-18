#pragma once
#include "champions/creature_hp.hpp"
#include <set>
#include "champions/items.hpp"
#include "champions/core_math.hpp"

namespace champions::simulation::rta::skills_a {
using Address = creature_hp::Address;
using Memory = creature_hp::Memory;
using Values = std::vector<Raw>;
struct Unsupported : std::runtime_error { using std::runtime_error::runtime_error; };
inline constexpr Raw G_RAND_SEED = 0x404464, G_SKILL_TABLE = 0x41b170;
inline constexpr Raw G_COSMETIC_SEED = 0x404460;
Raw lcg(Raw seed);
Raw unit(Raw draw);
Raw frac(Raw draw);
Raw round_half(Raw value);
Raw spread_max(Raw base, Raw draw, Raw factor = 0x3e4ccccc);
Raw rem(Raw a, Raw b);
Raw quot(Raw a, Raw b);
Raw half_toward_zero(Raw value);
std::optional<Raw> carries_getter(const std::function<Raw(Address)> &read, Raw function);
// Canonical damage slots; present entries retain their length and low16 lanes.
using Descriptor = std::array<std::optional<Values>, 4>;
Descriptor descriptor(const std::vector<Values> &entries);
// Callback-backed synchronous image reader. No file access or cached bytes.
class ImageExe {
public:
  using Load = std::function<Raw(Address, std::size_t)>;
  Load load;
  explicit ImageExe(Load load);
  std::string read(Address addr, std::int32_t size) const;
  std::string cstr(Address addr) const;
};
class Model;
class SharedDraws {
public:
  Model *model;
  explicit SharedDraws(Model &model);
  Raw next();
};
// Memory-backed lazy methods of the imported item table; cached static fields
// use the existing item module, while computed reads retain the supplied image.
struct ImageItemTables : items::Tables {
  const Memory *image = nullptr;
  std::optional<std::string> prefix_text(std::size_t, std::int32_t) const;
  std::optional<std::string> suffix_text(std::size_t, std::int32_t) const;
  Raw weapon_mult(std::int32_t) const;
  std::array<Raw,3> quality_row(std::int32_t) const;
};
struct Call { unsigned slot; Values args; std::string name; };
// RTA Skill foundation, weapon buffs 12/14/26, shields 15, support 31/32/33,
// Shield Bash 34, Fire Storm 7, Disease Trail 6, Shock of Frost 23 and Fire Flares 8.
// Homing projectiles Disease Bolt 4 and Holy Strike 16; Blinding Light 20;
// Disease Shield 5, Disease Effect 29, Fire Weapons 13 and Cone of Fire 18.
// No implicit engine services; callbacks own presentation, queries and damage.
// Memory and history caps are inherited from the standalone raw32 Memory.
class Model {
  std::size_t depth_ = 0;
  Values spell_dispatch(Raw, unsigned, unsigned, const Values &);
  Values skill_dispatch(Raw, unsigned, const Values &, bool recognise_can_use);
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
  // Only the pinned 195-unit trace corpus uses the superseded research prefix.
  bool legacy_constructor_vectors = false;
  std::set<unsigned> external;
  using Callback = std::function<Values(Model &, const Values &)>;
  std::map<unsigned, Callback> callbacks;
  // Named callbacks flatten vectors into x,y,z lanes in original argument order.
  // Scalars return one lane, void returns empty, box queries return object lanes.
  std::map<std::string, Callback> named_callbacks;
  std::set<std::string> external_named;
  std::map<Raw, Raw> type_info; // caller-supplied getter -> type identity
  // Owned initial memory and callback maps; construction performs no calls/writes.
  Model(Memory initial, std::map<std::string, Callback> services,
        std::set<std::string> forced = {});
  Values vcall(Raw obj, unsigned slot, const Values &);
  Values call(const std::string &, const Values &);
  Values table_call(const std::string &, Raw obj, unsigned offset, unsigned slot, const Values &);
  Values spell_call(Raw spell, unsigned slot, const Values &);
  Raw spell_rand(Raw);
  void spell_base(Raw spell, Raw level, Raw owner, Raw skill, Raw table);
  Raw spell_type(Raw);
  Raw find_spell_by_type(Raw creature, Raw type);
  Raw carries(Raw creature, Raw getter);
  // Summon Skeleton (11). The skeleton and terrain services remain callbacks.
  Raw summon_can_use(Raw skill, Raw caster);
  Raw summon_cast(Raw skill, Raw level, Raw caster, const Vec3 &pos, Raw facing);
  Vec3 summon_sample(Raw caster, Raw count);
  void skelight_ctor(Raw obj, const Vec3 &pos, Raw facing, Raw props, Raw level);
  void summon_spell_ctor(Raw spell, Raw level, Raw caster, Raw skill, Raw skeleton);
  void summon_spell_run(Raw spell);
  // Riposte (9/82): presentation draws remain separate from shared damage RNG.
  Raw cosmetic_draw();
  Vec3 cosmetic_ball();
  void riposte_counter_attack(Raw skill, Raw defender, Raw attacker, Raw level);
  void add_spell(Raw creature, Raw spell);
  Values weapons_of(Raw player) const;
  // Hammer of Wrath (17), supplied static item tables; states 1/2 refuse as in the pin.
  std::optional<ImageItemTables> item_tables_cache;
  const ImageItemTables &item_tables();
  void item_parse(Raw text, Raw record);
  Raw item_resource_index(Raw record) const;
  Raw hammer_can_use(Raw skill, Raw caster);
  Raw hammer_cast(Raw skill, Raw level, Raw caster, const Vec3 &pos, Raw facing);
  void hammer_ctor(Raw obj, const Vec3 &pos, Raw facing, Raw owner, Raw level, Raw skill);
  void hammer_spell_ctor(Raw spell, Raw level, Raw owner, Raw skill, Raw hammer);
  void hammer_spell_run(Raw spell);
  void hammer_pull(Raw obj, const Vec3 &anchor, Raw slack);
  void hammer_blow(Raw obj, Raw victim, Raw rating);
  void hammer_run(Raw obj);
  Raw seed3_draw();
  // Poison Rain (24): monster area attack, presentation/damage are callbacks.
  Raw poisonrain_can_use(Raw skill, Raw caster = 0) const;
  Raw poisonrain_cast(Raw skill, Raw level, Raw caster, const Vec3 &pos, Raw facing);
  void poisonrain_ctor(Raw obj, Raw level, Raw owner, const Vec3 &pos, Raw skill, Raw sound);
  void poisonrain_run(Raw obj);
  // Offensive undead family (21/22). LostSoul and damage consumers are callbacks.
  Raw repulse_cast(Raw skill, Raw level, Raw caster, const Vec3 &pos, Raw facing = 0);
  void damage_radial(Raw attacker, const Vec3 &pos, Raw dmg, Raw flags, Raw minimum, Raw skip, Raw inner, Raw outer);
  Raw dismissundead_cast(Raw skill, Raw level, Raw caster, const Vec3 &pos, Raw facing);
  void holybolt_ctor(Raw obj, const Vec3 &pos, Raw facing, Raw level, Raw owner, Raw skill);
  void holybolt_run(Raw obj);
  void holybolt_hit(Raw obj, Raw other);
  // Blessed (10): full spell lifecycle and statistic consumer interface.
  Raw blessed_can_use(Raw skill, Raw caster);
  Raw blessed_cast(Raw skill, Raw level, Raw caster);
  void blessed_ctor(Raw spell, Raw level, Raw owner, Raw skill);
  void blessed_dtor(Raw spell, Raw flags);
  Raw blessed_stat0(Raw spell);
  void blessed_run(Raw spell);
  // Wizard Beam (1). Trail point arrays stay presentation-owned, as in the pin.
  Raw wizardbeam_cast(Raw skill, Raw level, Raw caster);
  void wizardbeam_ctor(Raw spell, Raw level, Raw owner, Raw skill);
  Values beam_flash(Raw spell, Raw target);
  void beam_chain(Raw base, Raw velocity, Raw count, const Vec3 &direction, Raw length_at);
  void wizardbeam_run(Raw spell, const Vec3 &pos, Raw facing = 0);
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
  Raw fireweapons_can_use(Raw skill, Raw caster);
  Raw fireweapons_cast(Raw skill, Raw level, Raw caster);
  void fireweapons_ctor(Raw spell, Raw level, Raw owner, Raw skill);
  void fireweapons_run(Raw spell, const Vec3 &pos, Raw facing = 0);
  Raw coneoffire_can_use(Raw skill, Raw caster = 0) const;
  Raw coneoffire_cast(Raw skill, Raw level, Raw caster);
  void coneoffire_ctor(Raw spell, Raw level, Raw owner, Raw skill);
  void coneoffire_run(Raw spell, const Vec3 &pos, Raw facing = 0);
  Raw diseaseshield_can_use(Raw skill, Raw caster);
  Raw diseaseshield_cast(Raw skill, Raw level, Raw caster);
  void diseaseshield_ctor(Raw spell, Raw level, Raw owner, Raw skill);
  void diseaseshield_run(Raw spell, const Vec3 &pos, Raw facing = 0);
  void disease_spell_ctor(Raw spell, Raw seconds, Raw level, Raw victim, Raw attacker, Raw skill);
  void disease_spell_run(Raw spell, const Vec3 &pos, Raw facing = 0);
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
  // Disease Trail (6): full modeled lifecycle; engine services remain callbacks.
  Raw diseasetrail_can_use(Raw skill, Raw caster);
  Raw diseasetrail_cast(Raw skill, Raw level, Raw caster, const Vec3 &pos, Raw facing = 0);
  void diseasetrail_spell_ctor(Raw spell, const Vec3 &pos, Raw level, Raw owner, Raw skill);
  void diseasetrail_spell_run(Raw spell, const Vec3 &pos, Raw facing = 0);
  void diseasetrail_ctor(Raw obj, Raw level, const Vec3 &start, const Vec3 &end, Raw owner, Raw skill);
  void diseasetrail_run(Raw obj);
  void trail_puffs(const std::function<Raw()> &draw, const std::function<Vec3()> &ball,
                   const Vec3 &near, const Vec3 &far);
  void flash2(Raw creature, Raw a, Raw b, Raw c, Raw d);
  Values box_around(const Vec3 &centre, Raw half, Raw capacity);
  void projectile_ctor(Raw obj, const Vec3 &pos, Raw owner, Raw definition = 0);
  Raw move_and_collide(Raw obj, const Vec3 &velocity);
  void puff(const std::function<Vec3()> &ball, Raw definition, const Vec3 &centre,
            const std::optional<Raw> &spread, Raw speed);
  Raw shockoffrost_can_use(Raw skill, Raw caster = 0) const;
  Raw shockoffrost_cast(Raw skill, Raw level, Raw caster, const Vec3 &pos, Raw facing);
  void shockoffrost_ctor(Raw obj, const Vec3 &pos, Raw facing, Raw level, Raw owner, Raw skill);
  void frost_debris(Raw obj, Raw velocity_at);
  void shockoffrost_run(Raw obj);
  bool frost_hit(Raw obj, Raw other, Raw stat, Raw level, Raw hit_off);
  void shockoffrost_hit(Raw obj, Raw other);
  void frostshard_ctor(Raw obj, const Vec3 &pos, Raw facing, Raw level, Raw owner, Raw skill, Raw exclude);
  void frostshard_run(Raw obj);
  void frostshard_hit(Raw obj, Raw other);
  // Fire Flares (8): caster orbit lifecycle and orbit/burst object bodies.
  Raw fireflares_can_use(Raw skill, Raw caster);
  Raw fireflares_cast(Raw skill, Raw level, Raw caster, const Vec3 &pos, Raw facing = 0);
  void fireflares_ctor(Raw spell, Raw level, Raw owner, const Vec3 &pos, Raw skill);
  void fireflares_dtor(Raw spell, Raw flags);
  void fireflares_run(Raw spell, const Vec3 &pos, Raw facing = 0);
  void fireflare_ctor(Raw obj, const Vec3 &pos, Raw level, Raw owner, Raw skill, Raw angle);
  void fireflare_run(Raw obj);
  // Homing attack projectiles (4/16), including target selection and expiry.
  Raw diseasebolt_can_use(Raw skill, Raw caster = 0) const;
  Raw diseasebolt_cast(Raw skill, Raw level, Raw caster, const Vec3 &pos, Raw facing);
  void diseasebolt_ctor(Raw obj, Raw facing, Raw level, const Vec3 &pos, Raw owner, Raw skill);
  void diseasebolt_run(Raw obj);
  void diseasebolt_hit(Raw obj, Raw other);
  Raw holystrike_can_use(Raw skill, Raw caster = 0) const;
  Raw holystrike_cast(Raw skill, Raw level, Raw caster, const Vec3 &pos, Raw facing);
  void holystrike_ctor(Raw obj, Raw facing, Raw level, const Vec3 &pos, Raw owner, Raw skill);
  void holystrike_run(Raw obj);
  void holystrike_hit(Raw obj, Raw other);
  // Blinding Light (20): area effect and victim status lifecycle.
  Raw blindinglight_can_use(Raw skill, Raw caster = 0) const;
  Raw blindinglight_cast(Raw skill, Raw level, Raw caster, const Vec3 &pos, Raw facing);
  void blindinglight_effect_ctor(Raw obj, const Vec3 &pos, Raw level, Raw caster, Raw skill);
  void blindinglight_effect_run(Raw obj);
  void blindinglight_spell_ctor(Raw spell, Raw level, Raw caster, Raw victim, Raw skill);
  void blindinglight_spell_run(Raw spell);
  Model() = default;
  Model(const Model &) = delete;
  Model &operator=(const Model &) = delete;
  // Callback receives adjusted this followed by low32 arguments. Copying the
  // callable supports replacement during execution. Exceptions preserve state.
  // Missing/unknown derived bodies refuse; no fabricated zero callback result.
  Values skill_call(Raw skill, unsigned slot, const Values &args);
  // Original dispatch layer, before the later instruction-shape override.
  Values first_skill_call(Raw skill, unsigned slot, const Values &args);
  Raw can_use_slot(Raw skill, Raw caster);
  Raw mana_cost(Raw skill, Raw level);
  Raw frame() const;
  bool is_player(Raw obj) const;
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
