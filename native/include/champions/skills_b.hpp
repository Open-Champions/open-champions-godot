#pragma once
#include "champions/creature_hp.hpp"
#include "champions/core_math.hpp"
#include <array>
#include <variant>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

// Authored, bounded subset of ref_skills_b.py at 356add27. RTA only.
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
public:
  using Vec = std::array<Raw, 3>;
  using Argument = std::variant<Raw, Vec, std::string, Mat34>;
  struct SamplePoint { Vec point; Argument second; };
  using Result = std::variant<Raw, std::vector<Raw>, std::string, SamplePoint>;
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
  Result typed_invoke(const std::string &, std::vector<Argument>);
  Vec vec(Address) const;
  void wvec(Address, const Vec &);
  Address alloca(std::size_t);
  Raw spell_call(Address, unsigned, const std::vector<Raw> &);
  Raw vcall(Address, unsigned, const std::vector<Raw> &);
  void creature_update_spells(Address, Address);
  void spell_end(Raw);
  void game_object_ctor(Address, Address, Raw, Raw);
  Raw projectile_ctor(Address, Address, Raw, Raw);
  Raw obj_move_and_collide(Address, Address);
  std::uint64_t draws = 0;
  Model() = default;
  Model(const Model &) = delete;
  Model &operator=(const Model &) = delete;
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
  // Unported registered virtual bodies refuse unless explicitly external.
};
} // namespace champions::simulation::rta::skills_b
