#pragma once
#include "champions/creature_hp.hpp"
#include <array>
#include <variant>
#include <set>

// RTA only: arena base services, Skelight construction/states, Zek state/wave
// lifecycle and Summon/Tagar spell lifecycles.
namespace champions::simulation::rta::arena_monsters {
using Address = creature_hp::Address;
using Memory = creature_hp::Memory;
using Write = creature_hp::Write;
using Error = creature_hp::Error;
using MissingHandler = creature_hp::MissingHandler;
struct Unsupported : Error { using Error::Error; };
using Vec3 = std::array<Raw, 3>;
struct Sample { Vec3 point{}, second{}; };
struct Balance { std::int64_t found = 0; std::array<Raw, 5> factors{}; };
struct Objects { std::vector<Raw> values; };
struct FreeSpot { Vec3 point{}; Raw found = 0; };
struct DamageRange { Raw low = 0, high = 0; };
struct RayHit { Raw fraction = 0, object = 0; };
struct Slide { Raw hit = 0; Vec3 normal{}; };
struct Bounds { std::array<Raw, 6> values{}; };
struct Element { Raw low = 0, high = 0; };
struct Contacts { std::vector<Raw> values; };
// Python callback result shapes, without interpreting engine-owned output data.
// Text and byte buffers, lists and tuples remain distinct; None is not zero.
struct CallbackResult {
  enum class Kind { none, integer, text, bytes, list, tuple };
  Kind kind = Kind::none;
  std::uint64_t magnitude = 0;
  bool negative = false;
  std::string data;
  std::vector<CallbackResult> elements;
  static CallbackResult integer(std::int64_t);
  static CallbackResult unsigned_integer(std::uint64_t);
  static CallbackResult text(std::string);
  static CallbackResult bytes(std::string);
  static CallbackResult list(std::vector<CallbackResult>);
  static CallbackResult tuple(std::vector<CallbackResult>);
  bool truth() const;
};
using Value = std::variant<std::int64_t, Vec3, Sample, Balance, Objects, DamageRange, FreeSpot, std::string, RayHit, Slide, Contacts, Bounds, Element, std::monostate, CallbackResult>;
struct Call { std::string name; std::vector<Value> args; };
using Event = std::variant<Write, Call>;
struct FilterResult { bool refused; std::int32_t state; };
// Lossless Python value view of typed native callback data.
CallbackResult callback_value(const Value &);
// Pure marshaller for all 139 pinned EXTERNALS signatures. This pure operation
// does not install engine services.
CallbackResult shape_result(const std::string &name,
    const std::vector<CallbackResult> &args,
    CallbackResult ret = CallbackResult::integer(0),
    CallbackResult fret = CallbackResult::integer(0),
    const std::vector<CallbackResult> *outs = nullptr);
inline constexpr Address G_SKELIGHT_STATIC = 0x8e4560;
class Model {
  creature_hp::Limits limits_;
  std::size_t depth_ = 0;
  std::size_t tick_depth_ = 0;
  std::size_t contract_depth_ = 0;
  void write(Address, std::size_t, std::uint64_t);
  Value call(const std::string &, std::vector<Value>, bool adapt = true);
  Raw u32(Address) const;
  Vec3 vec(Address) const;
  Raw model_slot16(Address);
  Raw shared_draw();
  Value tagar_virtual(const std::string &, Address, unsigned, std::vector<Value>);
  void spell_destroy(Address);
  Value virtual_call(Address, unsigned, std::vector<Value>);
  void write_vec(Address, const Vec3 &);
  void zek_death_virtual(Address);
  void take_damage(Address, Raw attacker, Raw flags, Raw amount);
  Value horsemen_virtual(Address, unsigned, std::vector<Value>);
  Value horsemen_skill_cast(Address, Raw, Address, const Vec3 &, Raw);
  Value navigation_virtual(Address, unsigned, std::vector<Value>);
  Raw handle_object(Address) const;
  Vec3 random_home_point(Address, std::int32_t);
  Vec3 waypoint(Address, int ahead = 0) const;
  Vec3 advance_waypoint(Address, Vec3, const Vec3 &, Raw, Raw, Raw);
  void request_path(Address, const Vec3 &, Raw, Raw, Raw);
  Raw probe_rays(Address, const Vec3 &);
  Vec3 avoidance_target(Address, const Vec3 &);
  Vec3 avoidance_offset(Address, Raw, Raw);
  Vec3 steer_without_path(Address, const Vec3 &, Vec3);
  Vec3 steer_along_path(Address, const Vec3 &, Vec3);
  void turn_and_step(Address, const Vec3 &);
  void navigation_step(Address, std::int32_t);
  Raw drowsy(Address, Raw);
  void water(Address);
  void flee_from(Address, Raw, Raw);
  Value horsemen_tick(Address);
  Value skelight_tick(Address);
  Value tick_body(Address, unsigned, std::vector<Value>);
  Value tick_virtual(Address, unsigned, std::vector<Value> = {});
  Value tick_table(const std::string &, Address, Raw, unsigned, std::vector<Value>);
  Raw player_object(std::int32_t) const;
  bool near_view(Address, Raw) const;
  void follow_path(Address, const Value &);
  void follow_player(Address);
  void chill(Address);
  void boss_meter(Address, std::uint64_t);
  void scavenger(Address);
  void knockback_speed(Address, Raw, const std::array<Raw, 4> &);
  void hit_reaction(Address);
  Raw gib(Address, const Vec3 &, int, Raw);
  void hit_effect(Address, Raw);
  Raw remove_body(Address);
  Raw shatter_or_gib(Address, Raw, Raw);
  void die(Address, Raw);
  void award_kill(Address);
  void pick_target(Address, Raw);
  std::int64_t enemy(Address, Raw);
public:
  // Explicit Python-contract entry points. Delegated services require handlers
  // unless the caller opts into synthetic reference result observations below.
  // Older APIs continue to require handlers.
  Value reference_call(const std::string &, std::vector<Value> = {});
  Value vcall(Address, unsigned, std::vector<Value> = {});
  Value skill_call(Address, unsigned, std::vector<Value> = {});
  Value table_call(const std::string &, Address, Raw table_offset, unsigned, std::vector<Value> = {});
  Value set_state(Address, Raw);
  using Callback = std::function<Value(Model &, const std::vector<Value> &)>;
  // Borrowed memory must outlive this model. No address identity is inferred.
  Memory &mem;
  std::map<std::string, Callback> callbacks;
  // Synthetic oracle mode only: shape missing callbacks exactly as Python does.
  // These values are observations, with no delegated engine effects.
  bool reference_defaults = false;
  bool external_slot16 = false;
  // Explicitly delegate slot26, including its otherwise modeled base body.
  bool external_slot26 = false;
  // Reference external selections, e.g. Creature.v20 and Skill.v12. A known
  // unported internal body refuses unless its exact virtual name is selected.
  std::set<std::string> external;
  std::vector<Write> writes;
  std::vector<Call> calls;
  std::vector<Event> events;
  std::uint64_t draws = 0; // Shared draws; own draws only write object +0x44.
  // Class bodies ported outside this file (combat_ctors): a virtual call whose
  // table word is a key runs that body, unless the slot is selected external,
  // before any arena or base body. No call-out is recorded for it.
  using ClassBody = std::function<Value(Model &, Address self, const std::vector<Value> &)>;
  std::map<Raw, ClassBody> class_bodies;
  // Recorded call-out and logged store for bodies ported outside this file.
  Value external_call(const std::string &name, std::vector<Value> args);
  void store(Address, std::size_t, std::uint64_t);
  // Creature_takeDamage 0x116638, for a class slot 2 that only jumps to it.
  void base_take_damage(Address c, Raw attacker, Raw flags, Raw amount);
  // Creature_isEnemy 0x119328 (0, 1 or 4), for a class slot 20 that calls it.
  std::int64_t base_is_enemy(Address c, Raw other);
  explicit Model(Memory &, creature_hp::Limits = {});
  // Shared data/holder family. Strings retain bytes without text decoding.
  std::string cstr(Address address, std::size_t limit = 64) const;
  Raw class_model(Address creature) const;
  Raw clip(Address creature, std::uint32_t offset) const;
  Value holder_call(Address holder, unsigned slot, std::vector<Value> args = {});
  Value weapon_descriptor(Address creature);
  Value prop_int(Address creature, Raw name);
  void init_holder(Address creature, Address holder, const Raw *final_table = nullptr);
  Model(const Model &) = delete;
  Model &operator=(const Model &) = delete;
  // Required handlers: animation, handle_clear, sample queries, effects, delete,
  // and Creature.v16 for an unrecognized/explicitly external virtual body.
  // Exceptions retain ordered partial state. Callbacks can write borrowed memory
  // and replace handlers, but must not destroy this model or its memory.
  FilterResult filter_state(Address creature, std::uint32_t requested);
  // Base bodies a ported class setState calls (combat-world Gnaw_setState):
  // GameObject_rand on +0x44, Creature_clearPath, Creature_pickWanderDest.
  Raw own_draw(Address);
  // Shared g_randSeed draw for executed helpers outside arena_monsters.cpp.
  Raw draw_shared() { return shared_draw(); }
  void clear_path(Address);
  void pick_wander_dest(Address);
  void skelight_set_state(Address creature, std::uint32_t requested);
  void skelight_shatter(Address creature = 0, std::uint32_t argument = 0);
  void creature_ctor(Address, Raw type_name, const Vec3 &, Raw facing,
                     Raw class_def, Raw props, Raw hp);
  void set_def_entry_params(Address, Raw entry, const std::array<Raw, 4> &masks,
                            const std::array<Raw, 4> &times);
  std::int64_t turn_to_target(Address);
  std::int64_t can_see_target_in(Address, Raw facing, Raw arc, Raw ray_mode,
                               Raw near, Raw far, Raw lead);
  std::int64_t is_target_charging(Address);
  std::int64_t count_allies_near_target(Address, Raw radius);
  std::int64_t is_blocked_by_target(Address);
  std::int64_t is_not_rooted(Address);
  std::int64_t is_attack_ready(Address);
  std::int64_t is_not_stunned(Address);
  Raw slot16_model(Address);
  std::int64_t slot25_can_see_target(Address, Raw force);
  void slot28_on_death(Address, Raw quiet);
  void slot34_set_home_range(Address, Raw value);
  Raw spell_rand(Address);
  static Raw unit(Raw draw);
  Vec3 spell_ball(Address);
  void spell_base_ctor(Address spell, Raw level, Raw owner, Raw skill, Raw table);
  Raw tagar_cast(Address skill, Raw level, Raw caster, const Vec3 &pos, Raw facing = 0);
  void tagar_spell_ctor(Address spell, Raw level, Raw owner, Raw skill);
  std::int64_t tagar_is_valid(Address spell, Raw object, Raw marker_test);
  Raw tagar_pick_target(Address spell, Raw later);
  std::int64_t tagar_tick_victims(Address spell);
  void tagar_spell_run(Address spell, const Vec3 &pos = {}, Raw facing = 0);
  Raw cosmetic_draw();
  Vec3 cosmetic_ball();
  Vec3 summon_sample(Address caster, std::int32_t count);
  std::int64_t summon_can_use(Address skill, Address caster);
  Raw summon_cast(Address skill, Raw level, Address caster, const Vec3 &pos, Raw facing);
  void summon_spell_ctor(Address spell, Raw level, Raw caster, Raw skill, Raw skeleton);
  void summon_spell_run(Address spell, const Vec3 &pos = {}, Raw facing = 0);
  void skelight_ctor(Address creature, const Vec3 &pos, Raw facing, Raw props, Raw level);
  // Complete Zek construction/state-entry/wave family; no implicit tick dispatch.
  using StateSetter = std::function<void(Address, Raw)>;
  void zek_ctor(Address, Raw type_name, const Vec3 &, Raw facing, Raw props);
  std::int32_t zek_slot35(Address);
  void zek_set_state(Address, Raw requested);
  bool zek_specials(Address, const StateSetter &);
  // Complete AI switch, with a required state setter just as in the reference.
  // Does not execute zek_run's inherited damage/status/animation tick pipeline.
  void zek_switch(Address, Raw events, const StateSetter &);
  bool is_away_from_home(Address) const;
  Raw move_by(Address, const Vec3 &);
  void settle(Address);
  bool move_to(Address, const Vec3 &);
  void move_to_target(Address);
  Raw move_to_dest(Address);
  void flee_from_target(Address, Raw kind);
  void zek_wave(Address);
  // Complete slot-2 damage intake; sparks precede even inert/ignored damage.
  void zek_take_damage(Address, Raw attacker, Raw flags, Raw amount);
  // Horsemen construction, all mounted/on-foot state entries and four casts.
  void horsemen_ctor(Address, Raw type_name, const Vec3 &, Raw facing, Raw props, Raw variant, Raw mounted);
  // The Nightmare mount and its rider link (research docs/spec/horsemen-mount.md,
  // tools/ref_horsemen_mount.py). Off by default so Horsemen_ctor still records
  // a "Nightmare_ctor" call-out; a host that sets it runs the constructor here
  // (the call-out is then not recorded, as in the reference). Pass the name in
  // `external` to delegate again.
  bool nightmare_native = false;
  void nightmare_ctor(Address, Raw type_name, const Vec3 &, Raw facing, Raw props);
  void nightmare_set_state(Address mount, Raw requested);
  void nightmare_run(Address mount);
  void nightmare_draw(Address mount);
  void nightmare_on_hit_terrain(Address mount);
  void nightmare_on_touch_list(Address mount, const std::vector<Raw> &touched);
  Raw nightmare_pick_charge_target(Address mount);
  // Horsemen_mountSeatTransform: the matrix of attach point 10, written at `out`.
  void nightmare_seat_transform(Address mount, Raw out);
  Raw angle_to_target(Address);
  // The object behind a mount's rider handle (or a rider's mount handle), or 0.
  Raw linked_object(Address) const;
  // Runs `body` as a reference contract call (virtual calls go through vcall and
  // missing services shape like the reference when reference_defaults is set).
  void in_reference_mode(const std::function<void()> &body);
  void horsemen_set_state(Address, Raw requested);
  void horsemen_cast(Address);
  // Complete standalone monster ticks; engine services remain required callbacks.
  void horsemen_run(Address);
  void skelight_run(Address);
  void zek_run(Address);
  void process_damage(Address, Raw model);
  Raw creature_run(Address, Raw model);
  bool update_active(Address);
  Raw step_anim(Address, Raw speed);
};
// Exact raw-pattern getScaledTime; signed count uses the core FPU context.
std::int32_t scaled_time(std::int64_t count);
std::int32_t half_toward_zero(Raw value);
std::int32_t third_toward_zero(Raw value);
using ScriptValue = std::variant<std::int64_t, std::string>;
struct ScriptCall { std::string name; std::vector<ScriptValue> args; };
// Script requests are observations for the host to consume, not engine effects.
// Quest variables survive through caller-owned save/load; no engine is inferred.
class ArenaScript {
  std::size_t history_limit_;
  std::size_t frame_depth_ = 0;
  void emit(const std::string &, std::vector<ScriptValue> = {});
  void increment(unsigned);
public:
  std::map<std::string, std::int64_t> q;
  std::map<unsigned, std::int64_t> g;
  std::vector<std::string> front, back;
  std::vector<ScriptCall> log;
  bool paused = false;
  explicit ArenaScript(const std::map<std::string, std::int64_t> &quest = {},
                       std::size_t history_limit = creature_hp::MAX_HISTORY);
  void defer(const std::string &);
  void frame(std::int64_t paused = 0);
  void start();
  void incoming();
  void nosave();
  void setFlag();
  void knight_death(std::int64_t index);
  void knightDefeatedPause();
  void knightDefeatedDelay();
  void knightDefeated();
  void avatar_death();
  void bossDefeatedDelay();
  void endmission();
  void levelChange();
};
} // namespace champions::simulation::rta::arena_monsters
