#pragma once
#include "champions/creature_hp.hpp"
#include "champions/core_math.hpp"
#include <array>
#include <variant>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

// Authored, bounded port of ref_skills_b.py at 356add27. RTA only.
namespace champions::simulation::rta::skills_b {
using Address = creature_hp::Address;
using Memory = creature_hp::Memory;
using Call = creature_hp::Call;
struct Unsupported : std::runtime_error { using std::runtime_error::runtime_error; };
inline constexpr Raw G_RAND_SEED = 0x404464, G_COSMETIC_SEED = 0x404460,
                     G_SEED3 = 0x404468, G_PVP_MODE_A = 0x4f711c;
class Model {
  unsigned depth_ = 0;
  Address sp_ = 0x1e10000;
  Raw node_take();
  void node_push(Address, Raw);
  void node_unlink(Raw);
  void node_free(Raw);
  void partner_drop(Raw);
  Raw iter_skip(Raw, Raw);
  void iter_park(Address, Raw);
  Raw stat_impl(Address, unsigned, unsigned, unsigned, Raw, Raw);
  Raw invoke(const std::string &, std::vector<Raw>);
  Raw next(Address);
  std::optional<Raw> dispatch_skill(Address, Raw, const std::vector<Raw> &);
  std::optional<Raw> dispatch_spell(Address, Raw, const std::vector<Raw> &);
  std::optional<Raw> dispatch_object(Address, Raw, const std::vector<Raw> &);
public:
  using Vec = std::array<Raw, 3>;
  struct Descriptor { std::vector<std::array<Raw,4>> entries; };
  using Argument = std::variant<Raw, Vec, std::string, Mat34, std::monostate, Descriptor>;
  struct SamplePoint { Vec point; Argument second; };
  struct GrowthSample { Vec point; std::vector<Raw> number; Mat3 matrix; };
  using Result = std::variant<Raw, std::vector<Raw>, std::string, SamplePoint, GrowthSample>;
  struct TypedCall { std::string name; std::vector<Argument> args; };
  using TypedCallback = std::function<Result(Model &, const std::vector<Argument> &)>;
  using Callback = std::function<Raw(Model &, const std::vector<Raw> &)>;
  Memory mem;
  std::map<std::string, Callback> callbacks;
  // Force an implemented virtual slot to the explicit callback boundary.
  std::set<std::string> external;
  // Unknown functions are Skill.vN callbacks; no missing-handler zero fallback.
  std::map<Raw, std::string> addr_names;
  std::vector<Call> calls;
  // Complete ordered history; scalar calls retain their original calls ABI.
  std::vector<TypedCall> events;
  std::map<std::string, TypedCallback> typed_callbacks;
  // Explicit table variant of the reference _virtual; the class selects its body inventory.
  enum class VirtualClass { Skill, Spell, Creature };
  Raw virtual_call(VirtualClass, Address, Raw table, unsigned slot, const std::vector<Raw> &);
  Raw table_call(const std::string &, Address, Raw table_offset, unsigned slot, const std::vector<Raw> &);
  Result call(const std::string &, std::vector<Argument>);
  Result typed_invoke(const std::string &, std::vector<Argument>);
  Vec vec(Address) const;
  void wvec(Address, const Vec &);
  // Windows malloc.h defines alloca as a macro. The method name is the port's.
  // The macro is suspended for this declaration only (push/pop), so other
  // translation units that include this header keep it.
#pragma push_macro("alloca")
#undef alloca
  Address alloca(std::size_t);
#pragma pop_macro("alloca")
  Raw spell_call(Address, unsigned, const std::vector<Raw> &);
  Raw vcall(Address, unsigned, const std::vector<Raw> &);
  void creature_update_spells(Address, Address);
  void spell_end(Raw);
  void game_object_ctor(Address, Address, Raw, Raw);
  Raw projectile_ctor(Address, Address, Raw, Raw);
  Address local_vec(const Vec &);
  Vec object_ball(Address);
  static Descriptor descriptor(std::vector<std::array<Raw,4>>);
  Raw coneoffrost_cast(Address, Raw, Raw, Raw=0, Raw=0);
  void coneoffrost_spell_ctor(Address, Raw, Raw, Raw);
  void coneoffrost_run(Address, Address, Raw);
  void dummydamager_ctor(Address, Address, Address, Raw, Raw, Raw, Raw, Raw, Raw, Raw=1);
  void dummydamager_run(Address);
  void dummydamager_on_touch(Address, Raw);
  Raw obj_move_and_collide(Address, Address);
  std::uint64_t draws = 0;
  // Adopt caller-supplied memory and services; histories and scratch start fresh.
  explicit Model(Memory initial = Memory{}, std::map<std::string, Callback> services = {},
                 std::set<std::string> delegated = {});
  Model(const Model &) = delete;
  Model &operator=(const Model &) = delete;
  Raw f32(Address) const;
  std::string cstr(Address, std::int64_t limit=64) const;
  bool is_player(Address) const;
  static Raw half_toward_zero(Raw) noexcept;
  Raw stat_via_slot14(Address skill, Raw stat, Raw level, Raw n);
  using ManaCostMethod = std::function<Raw(Model &, Address, Raw)>;
  static ManaCostMethod mana_cost_method(Raw slot, Raw n);
  Raw skill_call(Address skill, unsigned slot, const std::vector<Raw> &args);
  Raw can_use_base() const noexcept { return 1; }
  Raw cast_base() const noexcept { return 0; }
  Raw mana_cost_base() const noexcept { return ONE; }
  Raw get_stat(Address skill, Raw stat, Raw level);
  Raw get_stat_tier2(Address skill, Raw stat, Raw level, Raw cap);
  Raw get_stat_for(Address skill, Raw stat, Raw level, Raw caster, Raw cap);
  Raw get_stat_for_handle(Address skill, Raw stat, Raw level, Address handle, Raw cap);
  Raw adjust_level(Raw level, Raw = 0) const noexcept { return level; }
  Raw mana_from_stat4(Address skill, Raw level, Raw n);
  Raw mana_cost(Address skill, unsigned skill_id, Raw level);
  void stat_range(Address skill, Raw stat, Raw level, Raw randomise,
                  Address lo, Address hi, Raw spread);
  Raw archery_fraction(Address skill, Raw level);
  Raw lifetap_fraction(Address skill, Raw level);
  Raw lifetap_stat1(Address skill, Raw level);
  Raw strongstance_stat0(Address skill, Raw level);
  Raw naturalresistance_fraction(Address skill, Raw level);
  Raw stunningstrike_stat0_for(Address skill, Raw target, Raw level);
  Raw lifetap_cast(Address, Raw, Raw, Raw=0, Raw=0) const noexcept { return cast_base(); }
  Raw strongstance_cast(Address, Raw, Raw, Raw=0, Raw=0) const noexcept { return cast_base(); }
  void strongstance_trigger(Address skill, Raw player, Raw a2, Raw a3);
  void stunningstrike_effect(Address skill, Raw target);
  Raw draw();
  Raw spell_rand(Address spell);
  Raw object_rand(Address object);
  Raw cosmetic_draw();
  Raw seed3_draw();
  void spell_base_ctor(Address spell, Raw level, Raw owner, Raw skill, Raw table);
  Raw handle_link(Address head, Address back, Raw object);
  Raw list_push(Address head, Raw object);
  Raw handle_remove_node(Raw node);
  void handle_clear(Address head);
  Raw handle_object(Address head) const;
  std::vector<Raw> list_objects(Address head) const;
  void iter_begin(Address iterator, Address head, Raw exclude_mask);
  void iter_next(Address iterator);
  void iter_end(Address iterator, Raw flags);
  void creature_add_spell(Address creature, Raw spell);
  Raw record(Address player) const;
  Raw stat(Address skill, Raw index, Raw level);
  Raw stat_for(Address skill, Raw index, Raw level, Raw caster, Raw cap);
  static Raw unit(Raw draw);
  static Raw signed_unit(Raw draw);
  Vec ball(const std::function<Raw()> &draw);
  Vec spell_ball(Address spell);
  Vec sample_point(Raw creature, Raw model, Raw draw, std::int32_t count);
  Result object_ctor(const std::string &, std::vector<Argument>);
  void light_effect(Raw block, const Vec &pos, Raw f12, Raw f13, Raw f14,
                    Raw s0, Raw s1, Raw s2, Raw s3, Raw a2=0, Raw a3=0,
                    const std::array<Raw,5> &rest={0,0,0,0,0});
  Raw minorhealing_cast(Address skill, Raw level, Raw caster, Raw pos=0, Raw facing=0);
  void minorhealing_spell_ctor(Address spell, Raw level, Raw owner, Raw skill);
  void minorhealing_run(Address spell, Raw pos=0, Raw facing=0);
  Raw holyshield_cast(Address skill, Raw level, Raw caster, Raw pos=0, Raw facing=0);
  void holyshield_spell_ctor(Address spell, Raw level, Raw owner, Raw skill);
  void holyshield_run(Address spell, Raw pos=0, Raw facing=0);
  Raw holyshield_absorb(Address spell);
  Raw healing_cast(Address skill, Raw level, Raw caster, Raw pos=0, Raw facing=0);
  void healing_spell_ctor(Address spell, Raw level, Raw owner, Raw skill);
  Raw heal_share(Address spell, Raw most);
  void healing_run(Address spell, Raw pos=0, Raw facing=0);
  void list_fill(Address head, const std::vector<Raw> &objects);
  void query_into(Address head, const std::string &, std::vector<Argument>);
  Address query_list(const std::string &, std::vector<Argument>);
  Raw convertenemy_can_use(Address skill, Raw caster);
  Raw convertundead_can_use(Address skill, Raw caster);
  Raw convert_cast(unsigned id, Address skill, Raw level, Raw caster);
  void convert_spell_ctor(unsigned id, Address spell, Raw level, Raw owner, Raw skill);
  void convert_run(unsigned id, Address spell);
  void convert_word(Raw target, Raw key);
  Raw convertenemy_cast(Address s, Raw l, Raw c, Raw=0, Raw=0) { return convert_cast(39,s,l,c); }
  Raw convertundead_cast(Address s, Raw l, Raw c, Raw=0, Raw=0) { return convert_cast(40,s,l,c); }
  void convertenemy_spell_ctor(Address s, Raw l, Raw o, Raw k) { convert_spell_ctor(39,s,l,o,k); }
  void convertundead_spell_ctor(Address s, Raw l, Raw o, Raw k) { convert_spell_ctor(40,s,l,o,k); }
  void convertenemy_run(Address s, Raw=0, Raw=0) { convert_run(39,s); }
  void convertundead_run(Address s, Raw=0, Raw=0) { convert_run(40,s); }
  Raw giantgrowth_can_use(Address skill, Raw caster);
  Raw giantgrowth_cast(Address skill, Raw level, Raw caster, Raw pos=0, Raw facing=0);
  void giantgrowth_spell_ctor(Address spell, Raw level, Raw owner, Raw skill);
  void giantgrowth_run(Address spell, Raw pos=0, Raw facing=0);
  void giantgrowth_step_scale(Address spell, Raw owner);
  void giantgrowth_emit(Address spell, Raw owner);
  Raw giantgrowth_owner_scale(Address spell) const;
  Raw ancestralcall_can_use(Address skill, Raw caster);
  Raw ancestralcall_cast(Address skill, Raw level, Raw caster, Raw pos=0, Raw facing=0);
  void ancestralcall_spell_ctor(Address spell, Raw level, Raw owner, Raw skill);
  void ancestralcall_run(Address spell, Raw pos, Raw facing=0);
  void ancestralcall_tint_and_glow(Address spell, Raw object);
  Raw ancestralcall_stat0_fraction(Address spell);
  Raw ancestralcall_stat1_fraction(Address spell);
  Raw ancestralcall_query_for(Address spell, Raw creature, Address out0, Address out1);
  std::array<Raw,2> stat_range(Address skill, Raw index, Raw level, Raw randomise, Raw spread);
  Raw bash_can_use(Address skill, Raw caster);
  Raw bash_cast(Address skill, Raw level, Raw caster, Raw pos=0, Raw facing=0);
  void bash_spell_ctor(Address spell, Raw level, Raw owner, Raw skill);
  void bash_windup_fx(Address spell, Raw unused=3);
  std::array<Raw,2> bash_range(Address spell);
  void bash_run(Address spell, Raw pos=0, Raw facing=0);
  void bash_done(Address spell, Raw head);
  Raw criticalhit_can_use(Address skill, Raw caster);
  Raw criticalhit_cast(Address skill, Raw level, Raw caster, Raw pos=0, Raw facing=0);
  void criticalhit_spell_ctor(Address spell, Raw level, Raw owner, Raw skill);
  void criticalhit_set_tint(Address spell, Raw creature);
  void criticalhit_run(Address spell, Raw pos=0, Raw facing=0);
  Raw criticalhit_bonus_for(Address skill, Raw attacker, Raw own_rank);
  static Raw from_uint(Raw value);
  Raw charge_cast(Address skill, Raw level, Raw caster, Raw pos=0, Raw facing=0);
  Vec charge_trail_point(Raw owner, const std::array<Raw,2> &ahead, Raw index) const;
  void charge_spell_ctor(Address spell, Raw level, Raw owner, Raw skill);
  void charge_run(Address spell, Raw pos=0, Raw facing=0);
  Raw charge_stat3_scaled(Address spell);
  void charge_on_contact(Address spell, Raw target);
  Raw resurrect_can_use(Address skill, Raw caster);
  Raw resurrect_find_target(Address skill, Raw caster);
  Raw resurrect_is_being_raised(Address skill, Raw slot);
  Raw resurrect_cast(Address skill, Raw level, Raw caster, Raw pos=0, Raw facing=0);
  void resurrect_spell_ctor(Address spell, Raw level, Raw target, Raw skill, Raw caster);
  void resurrect_run(Address spell, Raw pos=0, Raw facing=0);
  Raw resurrect_on_owner_death(Address spell);
  Raw cyclone_can_use(Address skill, Raw caster);
  Raw cyclone_cast(Address skill, Raw level, Raw caster, Raw pos=0, Raw facing=0);
  void cyclone_spell_ctor(Address spell, Raw level, Raw owner, Raw skill);
  void cyclone_run(Address spell, Raw pos, Raw facing=0);
  void cyclone_update_fx(Address spell, const Vec &here);
  // Elemental arrow family: explicit RTA IDs 43,44,45,47; 46 is Multi Fire.
  Raw bow_can_use(Raw caster, Raw text_key);
  Raw arrow_can_use(unsigned id, Address skill, Raw caster);
  Raw arrow_cast(unsigned id, Address skill, Raw level, Raw caster, Address pos, Raw facing);
  void arrow_ctor(unsigned id, Address obj, Address pos, Raw facing, Raw owner, Raw level, Raw skill);
  void arrow_ctor2(unsigned id, Address obj, Address pos, Address direction, Raw owner, Raw level, Raw skill);
  void arrow_light(Address obj, const Vec &, Raw radius, Raw falloff, const Vec &colour);
  void arrow_velocity_facing(Address obj, Raw speed);
  void arrow_velocity_dir(Address obj, const Vec &, Raw speed);
  bool arrow_blocked_for_player(Raw other) const;
  void arrow_common(unsigned id, Address obj, Address pos, Raw owner, Raw skill);
  Raw firearrow_stats(Address obj, Raw owner);
  void arrow_finish(Address obj, Raw lump, Raw sound);
  static std::array<Raw,2> monster_spread(Raw amount, Raw draw);
  void arrow_flight(unsigned id, Address obj, Raw terrain_reason=1);
  void arrow_flight(Address obj, const std::function<void(Address,Raw)> &end,
                    const std::function<void(Address)> &particles, Raw terrain_reason=1);
  void flight_particle(Address obj, Raw definition, Raw speed, Raw reach);
  void arrow_particles(unsigned id, Address obj);
  void arrow_run(unsigned id, Address obj);
  void arrow_on_hit(unsigned id, Address obj, Raw other);
  void arrow_end(unsigned id, Address obj, Raw reason=0);
  Raw multifire_can_use(Address skill, Raw caster) { return arrow_can_use(46,skill,caster); }
  Raw multifire_cast(Address skill, Raw level, Raw caster, Address pos, Raw facing);
  Raw shock_cast(Address skill, Raw level, Raw caster, Address pos, Raw facing);
  void shock_ctor(Address obj, Address pos, Raw facing, Raw caster, Raw skill, Raw level);
  Raw shock_in_chain(Address obj, Raw other);
  void shock_build_chain(Address obj, Address origin);
  Raw shock_wants(Address obj, Raw other, Raw target=0);
  void shock_run(Address obj);
  // Frost Storm (58): standalone GameObjects, with delegated engine services.
  static Raw down(Raw value) noexcept;
  static Raw c_rem(Raw dividend, Raw divisor) noexcept;
  Raw froststorm_cast(Address skill, Raw level, Raw caster, Address pos, Raw facing);
  void froststorm_ctor(Address obj, Address pos, Raw frames, Raw damage, Raw caster, Address drift, Raw radius);
  Vec froststorm_point(Address obj, Raw spread);
  void froststorm_run(Address obj);
  void iceshard_particle(Address obj, Raw speed);
  void iceshard_run(Address obj);
  // Ground Pound (38): animation cast and standalone impact effect.
  Raw groundpound_can_use(Address skill, Raw caster);
  Raw groundpound_cast(Address skill, Raw level, Raw caster, Raw pos=0, Raw facing=0);
  void groundpound_effect_ctor(Address obj, Address pos, Raw facing, Raw skill, Raw level, Raw owner);
  Raw groundpound_stat(Address obj, Raw index, Raw cap);
  void groundpound_effect_run(Address obj);
  // Darkness (36): independent GameObject cloud; services remain explicit.
  Raw darkness_cast(Address skill, Raw level, Raw caster, Raw pos=0, Raw facing=0);
  std::array<Raw,4> ambient_bytes() const;
  void darkness_ctor(Address obj, Address pos, Address velocity, Raw skill, Raw level, Raw caster);
  std::vector<Raw> darkness_box(Address obj);
  void darkness_run(Address obj);
  // Harm Touch (42): armed melee-hit spell; caller owns attachment and damage.
  Raw harmtouch_can_use(Address skill, Raw caster);
  Raw harmtouch_cast(Address skill, Raw level, Raw caster, Raw pos=0, Raw facing=0);
  void harmtouch_spell_ctor(Address spell, Raw level, Raw owner, Raw skill);
  void harmtouch_tint(Address spell, Raw left);
  void harmtouch_run(Address spell, Raw pos=0, Raw facing=0);
  void harmtouch_apply_to_hit(Address spell, Raw target, Address low_out, Address high_out);
  // Shadow Step (67): fire ring, target calm, particles and delegated double move.
  Raw shadowstep_cast(Address skill, Raw level, Raw caster, Raw pos=0, Raw facing=0);
  void shadowstep_spell_ctor(Address spell, Raw level, Raw owner, Raw skill);
  void shadowstep_run(Address spell, Raw pos=0, Raw facing=0);
  // Planar Wind (66): front, one-time handle query and delegated hit services.
  Raw planarwind_cast(Address skill, Raw level, Raw caster, Address pos, Raw facing=0);
  void planarwind_spell_ctor(Address spell, Raw level, Raw owner, Raw skill, Address pos);
  void planarwind_strike(Address spell, Raw target, const Vec &step);
  void planarwind_run(Address spell, Raw pos=0, Raw facing=0);
  // Shock of Fire (57): shared collision movement and independent Sparks RNG.
  Raw shockoffire_cast(Address skill, Raw level, Raw caster, Address pos, Raw facing);
  void sparks_ctor(Address obj, Address pos, Address velocity, Raw owner, Raw unused,
                   Raw tag, Raw low, Raw high, Raw splash, Raw radius);
  void sparks_run(Address obj);
  void sparks_on_touch(Address obj, Raw other);
  // Delegated engine bodies require explicit services.
};
} // namespace champions::simulation::rta::skills_b
