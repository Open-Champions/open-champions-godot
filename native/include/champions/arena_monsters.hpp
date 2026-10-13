#pragma once
#include "champions/creature_hp.hpp"
#include <array>
#include <variant>

// RTA only: arena creature base services and bounded Skelight state transitions.
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
using Value = std::variant<std::int64_t, Vec3, Sample, Balance>;
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
  Raw clip(Address, std::uint32_t) const;
  Raw model_slot16(Address);
  Value virtual_call(Address, unsigned, std::vector<Value>);
public:
  using Callback = std::function<Value(Model &, const std::vector<Value> &)>;
  // Borrowed memory must outlive this model. No address identity is inferred.
  Memory &mem;
  std::map<std::string, Callback> callbacks;
  bool external_slot16 = false;
  // Explicitly delegate slot26, including its otherwise modeled base body.
  bool external_slot26 = false;
  std::vector<Write> writes;
  std::vector<Call> calls;
  std::vector<Event> events;
  std::uint64_t draws = 0; // Shared draws; own draws only write object +0x44.
  explicit Model(Memory &, creature_hp::Limits = {});
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
  // A filtered state 1 throws Unsupported AFTER filter effects, before any
  // wander/path effects. Monster constructors, run, other classes and spells are absent.
};
} // namespace champions::simulation::rta::arena_monsters
