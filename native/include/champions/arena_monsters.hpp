#pragma once
#include "champions/creature_hp.hpp"
#include <array>
#include <variant>
#include <set>

// RTA only: arena base services, Skelight construction/states and Summon/Tagar spell lifecycles.
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
using Value = std::variant<std::int64_t, Vec3, Sample, Balance, Objects, DamageRange, FreeSpot, std::string>;
struct Call { std::string name; std::vector<Value> args; };
using Event = std::variant<Write, Call>;
struct FilterResult { bool refused; std::int32_t state; };
inline constexpr Address G_SKELIGHT_STATIC = 0x8e4560;
class Model {
  creature_hp::Limits limits_;
  std::size_t depth_ = 0;
  void write(Address, std::size_t, std::uint64_t);
  Value call(const std::string &, std::vector<Value>);
  Raw u32(Address) const;
  Vec3 vec(Address) const;
  Raw model_slot16(Address);
  Raw shared_draw();
  Value tagar_virtual(const std::string &, Address, unsigned, std::vector<Value>);
  void spell_destroy(Address);
  Value virtual_call(Address, unsigned, std::vector<Value>);
public:
  using Callback = std::function<Value(Model &, const std::vector<Value> &)>;
  // Borrowed memory must outlive this model. No address identity is inferred.
  Memory &mem;
  std::map<std::string, Callback> callbacks;
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
  // A filtered state 1 throws Unsupported AFTER filter effects, before any
  // wander/path effects. Monster ticks and other constructors remain absent.
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
