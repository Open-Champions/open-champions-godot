#pragma once
#include "champions/creature_hp.hpp"
#include <array>
#include <set>

namespace champions::simulation::rta::creature_movement {
using Address = creature_hp::Address;
using Memory = creature_hp::Memory;
using Write = creature_hp::Write;
using Limits = creature_hp::Limits;
using Error = creature_hp::Error;
using ResourceLimit = creature_hp::ResourceLimit;
using MissingHandler = creature_hp::MissingHandler;
using CallbackPolicy = creature_hp::CallbackPolicy;
using Vec3 = std::array<Raw, 3>;
inline constexpr std::uint32_t G_SIM_FRAME = 0x44fdcc, G_WORLD = 0x44fdc8,
                               S_FX_LUMP = 0x52fc18, S_SPLASH = 0x530058;
inline constexpr std::size_t MAX_CONTACTS = 16, MAX_VALUE_DEPTH = 4,
                             MAX_VALUE_NODES = 64, MAX_BUFFER_BYTES = 4096;
// Vector denotes a three-word Python tuple; Tuple retains general tuple shape
// (slide result or touched objects), List the collected object list, and Buffer
// the ('buf', bytes) token. Integer callback arguments wrap to raw32, while
// integer callback results retain their signed 64-bit supplied value.
enum class ValueKind { None, Integer, Vector, List, Tuple, Buffer };
struct Value {
  ValueKind kind = ValueKind::None;
  std::int64_t scalar = 0;
  Vec3 vec{};
  std::vector<Value> items{};
  std::string bytes{};
  static Value integer(std::int64_t);
  static Value vector(Vec3);
  static Value list(std::vector<Value>);
  static Value tuple(std::vector<Value>);
  static Value buffer(std::string);
};
struct Call {
  std::string name;
  std::vector<Value> args;
};
enum class EventKind { Write, Call };
struct Event {
  EventKind kind = EventKind::Write;
  Write write{};
  Call call{};
};
// Standalone published movement/settle bodies. No scene, live-object identity,
// collision service, shared RNG or original player controls are inferred.
class Model {
  Limits limits_;
  std::unique_ptr<Memory> owned_memory_;
  std::size_t depth_ = 0;
  void write(Address, std::size_t, std::uint64_t);
  Value call(const std::string &, std::vector<Value>);
  Raw u32(Address) const;
  Vec3 vec(Address) const;
  void wvec(Address, Vec3);
  void water_body(Address);
  std::uint32_t move_body(Address, Vec3);
  Value vcall_body(Address, std::uint32_t, std::vector<Value>);

public:
  using Callback = std::function<Value(Model &, const std::vector<Value> &)>;
  Memory &mem;
  std::map<std::string, Callback> callbacks;
  // Only Creature.v6/v10/v11 are supported external base-body overrides.
  std::set<std::string> external;
  std::vector<Write> writes;
  std::vector<Call> calls;
  std::vector<Event> events;
  std::uint64_t draws =
      0; // Shared generator draws: movement never increments it.
  CallbackPolicy policy;
  explicit Model(CallbackPolicy = CallbackPolicy::RequireHandlers, Limits = {});
  // Borrowed storage must outlive this nonmovable Model. Its own memory limits
  // and overlay remain authoritative; model limits bound this model's logs.
  Model(Memory &, CallbackPolicy = CallbackPolicy::RequireHandlers,
        Limits = {});
  bool borrows_memory() const noexcept { return !owned_memory_; }
  Model(const Model &) = delete;
  Model &operator=(const Model &) = delete;
  Model(Model &&) = delete;
  Model &operator=(Model &&) = delete;
  std::uint32_t move_by(Address creature, Vec3 velocity);
  void water(Address creature);
  void settle(Address creature);
  std::uint32_t obj_rand(Address object);
  Value vcall(Address object, std::uint32_t slot, std::vector<Value> args);
  // Callbacks are copied before invocation to permit replacement. Authoritative
  // callback state must be in memory or stable external captures. Exceptions
  // retain ordered partial writes/calls; no rollback or failure latch. Never
  // destroy this Model during a callback. Core FPU remains caller-selected.
};
} // namespace champions::simulation::rta::creature_movement
