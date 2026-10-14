#pragma once
#include "champions/creature_hp.hpp"
#include "champions/creature_movement.hpp"
#include <array>
#include <set>
#include <optional>
#include <variant>

namespace champions::simulation::rta::monsters_1 {
using Memory = creature_hp::Memory;
using Address = creature_hp::Address;
using Write = creature_hp::Write;
using Error = creature_hp::Error;
using CallbackPolicy = creature_hp::CallbackPolicy;
using Vec3 = std::array<Raw, 3>;
using Structured = creature_movement::Value;
struct Descriptor { std::array<Raw,4> effect; };
// General tagged tuples preserve the ABI's ('elem', min, max) and complete
// ('desc', four slots) arguments, without embedding engine records or strings.
struct TaggedTuple { std::string tag; std::vector<Structured> fields; };
using Argument = std::variant<std::uint32_t, Vec3, Descriptor, Structured, TaggedTuple, std::string>;
struct BufferResult { Structured payload; }; // ('buf', supplied value), normally bytes
using HeadResult = std::variant<std::uint64_t, Structured, BufferResult>;
struct CompoundResult { HeadResult head; std::vector<Structured> outputs; };
using Result = std::variant<std::uint64_t, Structured, BufferResult, CompoundResult>;
// EXTERNALS ABI only: output values are supplied by the host, never read from
// guessed stack addresses. Missing outs have the pinned reference's defaults.
Result shape_result(const std::string &, const std::vector<Argument> &,
                    HeadResult ret = std::uint64_t{0}, HeadResult fret = std::uint64_t{0},
                    std::optional<std::vector<Structured>> outs = std::nullopt);
struct Unsupported : Error { using Error::Error; };
struct Call { std::string name; std::vector<Argument> args; };
struct Event { bool is_call = false; Write write{}; Call call{}; };
// Bounded standalone subset, explicitly Return to Arms. Storage is borrowed
// and must outlive this nonmovable model. Missing handlers refuse by default.
class Model {
  std::size_t depth_ = 0;
  bool skelight_domain_ = false;
  bool arena_cast_domain_ = false;
  bool horsemen_domain_ = false;
  bool zek_domain_ = false;
  bool full_domain_ = false;
  std::optional<Result> dispatched_result_;
  void take_damage(Address, Raw attacker, Raw flags, Raw amount);
  bool zek_specials(Address);
  void zek_switch(Address, Raw);
  bool target_charging(Address);
  std::uint64_t allies_near_target(Address, Raw);
  std::uint64_t blocked_by_target(Address);
  Raw cosmetic_draw();
  Vec3 cosmetic_ball();
  Vec3 summon_sample(Address, std::int32_t);
  void init_weapon_holder(Address);
  Argument weapon_descriptor(Address);
  void record_call(const Call &);
  void creature_ctor(Address, Raw, Vec3, Raw, Raw, Raw, Raw);
  void event_params(Address, Raw, std::array<Raw,4>, std::array<Raw,4>);
  void write(Address, std::size_t, std::uint64_t);
  std::uint64_t call(const std::string &, std::vector<Argument>);
  Raw u32(Address) const;
  Vec3 vec(Address) const;
  void wvec(Address, Vec3);
  std::uint32_t random(Address);
  std::pair<bool, std::int32_t> filter(Address, std::uint32_t);
  void clear_path(Address);
  void wander(Address);
  void start(Address, std::uint32_t, std::uint32_t, Raw blend = 0x3dcccccc);
  void swing(Address, std::uint32_t, std::uint32_t);
  void drop_loot(Address, Raw item, Raw, Raw, Raw);
  std::optional<std::uint64_t> virtual_call(Address, unsigned, std::uint32_t);
  void on_death(Address, std::uint32_t);
  void shed(Address);
  void spider_state_body(Address, std::uint32_t);
  void movement(Address, bool settle, Vec3 velocity = {});
  Raw turn_to_point(Address, Vec3);
  Vec3 waypoint(Address, std::int64_t ahead = 0) const;
  Vec3 advance_waypoint(Address, Vec3, Vec3);
  Raw probe_rays(Address, Vec3);
  Vec3 avoidance_target(Address, Vec3);
  Vec3 steer_without_path(Address, Vec3, Vec3);
  Vec3 steer_along_path(Address, Vec3, Vec3);
  void turn_and_step(Address, Vec3);
  Raw move_to(Address, Vec3);
  void move_to_target(Address);
  void turn_to_target(Address);
  // Inherited bodies used by the ordinary run family.
  std::int32_t i16(Address) const;
  std::int32_t i8(Address) const;
  std::uint64_t flags(Address) const;
  Raw handle(Address) const;
  Raw player(Raw) const;
  Structured service(const std::string &, std::vector<Argument>);
  std::optional<std::uint64_t> dispatch(Address, unsigned, std::vector<Argument> = {});
  Structured table_service(const std::string &, Address, Raw, unsigned, std::vector<Argument>, bool skill_dispatch = false);
  Raw shared_draw();
  bool away(Address) const;
  bool near_view(Address, Raw) const;
  bool update_active(Address);
  void creature_run(Address, Raw);
  Raw step_anim(Address, Raw);
  void process_damage(Address, Raw);
  void knockback(Address, Raw, std::array<Raw,4>);
  void hit_reaction(Address);
  Raw gib(Address, Vec3, std::optional<Raw>, Raw);
  void hit_effect(Address, Raw);
  void die(Address, Raw);
  Raw shatter(Address, Raw, Raw);
  Raw remove_body(Address);
  void award_kill(Address);
  void follow_path(Address, Raw);
  void follow_player(Address);
  void chill(Address);
  void boss_meter(Address, std::uint64_t);
  void scavenger(Address);
  void pick_target(Address, Raw);
  Raw is_enemy(Address, Raw);
  Vec3 random_home(Address, Raw);
  void move_to_dest(Address);
  Vec3 avoidance_offset(Address, Raw, Raw);
  void flee(Address, Raw);
  void step(Address, std::int32_t);
  std::optional<std::uint64_t> run_monster(Address, unsigned);
  void spell_run(Address, Raw);
  Raw spell_random(Address);
  Vec3 spell_ball(Address);
  bool tagar_valid(Address, Raw, bool);
  Raw tagar_pick(Address, bool);
  bool tagar_victims(Address);
  void tagar_run(Address);
  Structured movement_virtual(Address, unsigned, std::vector<Structured>);
public:
  using Callback = std::function<std::uint64_t(Model &, const std::vector<Argument> &)>;
  using Balance = std::array<Raw,6>; // found, HP, armour, damage, attack, XP
  std::function<Balance(Model &, Raw)> balance_lookup;
  Memory &mem;
  CallbackPolicy policy;
  std::map<std::string, Callback> callbacks;
  using StructuredCallback = std::function<Structured(Model &, const std::vector<Argument> &)>;
  std::map<std::string, StructuredCallback> structured_callbacks;
  using ResultCallback = std::function<Result(Model &, const std::vector<Argument> &)>;
  std::map<std::string, ResultCallback> result_callbacks;
  // Explicit Creature/Spell virtual overrides use their exact callback name.
  std::set<std::string> external;
  std::vector<Write> writes;
  std::vector<Call> calls;
  std::vector<Event> events;
  std::uint64_t draws = 0; // Shared RNG draws made by inherited lifecycle bodies.
  explicit Model(Memory &, CallbackPolicy = CallbackPolicy::RequireHandlers);
  // Complete callback call domain, including None, tuples, lists and buffers.
  // Only top-level integer arguments wrap to raw32, as in the reference.
  Result call_value(const std::string &, std::vector<Argument>);
  Model(const Model &) = delete;
  Model &operator=(const Model &) = delete;
  // Callback mutation, replacement and exceptions retain partial state; no
  // rollback. Integer callback arguments are raw32; returns retain raw64.
  // Complete Skelight lifecycle and bounded virtual entry. Other arena creature bodies
  // still refuse; this does not claim the complete reference vcall unit.
  void skelight_ctor(Address, Vec3, Raw facing, Raw props, Raw level);
  std::optional<std::uint64_t> skelight_run(Address);
  void skelight_set_state(Address, Raw state);
  void skelight_shatter(Address c = 0, Raw arg = 0);
  std::optional<std::uint64_t> skelight_vcall(Address, unsigned, std::vector<Argument> = {});
  // Complete pinned Creature dispatch, including structured callback returns.
  // Existing bounded entry points retain their original domains.
  Result vcall(Address, unsigned, std::vector<Argument> = {});
  // Complete Zek lifecycle/damage/wave; older entry domains stay bounded.
  void zek_ctor(Address, Raw type_name, Vec3, Raw facing, Raw props);
  std::optional<std::uint64_t> zek_run(Address);
  void zek_set_state(Address, Raw);
  void zek_take_damage(Address, Raw attacker, Raw flags, Raw amount);
  std::int32_t zek_slot35(Address);
  void zek_wave(Address);
  std::optional<std::uint64_t> zek_vcall(Address, unsigned, std::vector<Argument> = {});
  // Complete Horsemen lifecycle; dedicated domain preserves older entry refusals.
  void horsemen_ctor(Address, Raw type_name, Vec3, Raw facing, Raw props, Raw variant, Raw mounted);
  std::optional<std::uint64_t> horsemen_run(Address);
  void horsemen_set_state(Address, Raw);
  void horsemen_cast(Address);
  std::optional<std::uint64_t> horsemen_vcall(Address, unsigned, std::vector<Argument> = {});
  // Complete arena cast dependency family; prior entry domains stay bounded.
  Raw arena_skill_call(Address, unsigned, std::vector<Argument> = {});
  Raw summon_can_use(Address skill, Address caster);
  Raw summon_cast(Address skill, Raw level, Address caster, Vec3, Raw facing);
  Raw tagar_cast(Address skill, Raw level, Address caster, Vec3, Raw facing);
  void spell_base_ctor(Address, Raw level, Raw owner, Raw skill, Raw table);
  void summon_spell_ctor(Address, Raw level, Raw owner, Raw skill, Raw skeleton);
  void tagar_spell_ctor(Address, Raw level, Raw owner, Raw skill);
  Raw anim_flags(Address c, Raw mask) const;
  std::uint64_t see(Address, Raw arc, Raw far, Raw near = 0,
                    std::optional<Raw> facing = std::nullopt);
  void dead_update(Address, Raw events, Raw flag, Raw size, Vec3 push = {});
  Raw badger_pick(Address);
  void scorpion_attack(Address, Raw events);
  void badger_attack(Address, Raw events);
  // Full chase selection, including inherited target-rim/path movement.
  std::optional<std::uint64_t> badger_chase(Address);
  std::optional<std::uint64_t> smallspider_run(Address);
  std::optional<std::uint64_t> gnaw_run(Address);
  std::optional<std::uint64_t> mosquito_run(Address);
  std::optional<std::uint64_t> scorpion_run(Address);
  std::optional<std::uint64_t> badger_run(Address);
  void smallspider_ctor(Address, Raw type_name, Vec3, Raw facing, Raw props);
  void gnaw_ctor(Address, Raw type_name, Vec3, Raw facing, Raw props);
  // Reference order differs from the other constructors.
  void mosquito_ctor(Address, Vec3, Raw facing, Raw props, Raw type_name);
  void scorpion_ctor(Address, Raw type_name, Vec3, Raw facing, Raw props);
  void badger_ctor(Address, Raw type_name, Vec3, Raw facing, Raw props, Raw variant);
  void smallspider_set_state(Address, std::uint32_t state);
  void gnaw_set_state(Address, std::uint32_t state);
  void mosquito_set_state(Address, std::uint32_t state);
  void scorpion_set_state(Address, std::uint32_t state);
  void badger_set_state(Address, std::uint32_t state);
  void smallspider_on_touch(Address, std::uint32_t other);
  void mosquito_lose_part(Address, std::uint32_t part);
  void scorpion_sever_part(Address, std::uint32_t part);
  // Uses caller-supplied signed-character classification bytes at 0x5628e1.
  // A prefix longer than 4096 bytes refuses instead of looping indefinitely.
  std::uint32_t str_prefix_differs(Address text, Address prefix);
};
} // namespace champions::simulation::rta::monsters_1
