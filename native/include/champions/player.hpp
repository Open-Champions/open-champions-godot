#pragma once
#include "champions/core_math.hpp"
#include "champions/creature_hp.hpp"
#include <variant>

namespace champions::simulation::rta::player {
using Memory = creature_hp::Memory;
using Write = creature_hp::Write;
using Address = creature_hp::Address;
using Error = creature_hp::Error;
using MissingHandler = creature_hp::MissingHandler;
using ResourceLimit = creature_hp::ResourceLimit;
struct Unsupported : Error {
  using Error::Error;
};
using Argument = std::variant<std::uint32_t, Vec3>;
// Closed callback set of the supported subset; argument lanes retain raw bits.
enum class Service {
  CreatureRun,
  Knockback,
  LightRelink,
  LightUnlink,
  CamFlag,
  Frozen,
  Voice,
  MemcardBusy,
  Rumble,
  AnimStep,
  Footstep,
  MoveToDest,
  FxSpawn,
  DialogOpen,
  TaskRunning,
  SlotActive,
  ResFind,
  AnimStart,
  AnimBlend
};
struct Call {
  Service service;
  std::vector<Argument> args;
};
struct Event {
  bool is_call = false;
  Write write{};
  Call call{};
};
class Tick {
  std::size_t depth_ = 0;
  std::uint32_t call(Service, std::vector<Argument> = {});
  void write(Address, std::size_t, std::uint64_t);
  std::uint32_t draw();
  std::uint32_t own_draw();
  void entry();

public:
  // A callable is copied before invocation to preserve self-replacement. Keep
  // persistent callback state in memory or stable external captures; mutable
  // value-capture identity is a host boundary, not simulated player state.
  using Callback =
      std::function<std::uint32_t(Tick &, const std::vector<Argument> &)>;
  Memory &mem; // Borrowed synchronous memory; caller retains its lifetime.
  const std::uint32_t P;
  std::map<Service, Callback> callbacks;
  std::vector<Write> writes;
  std::vector<Call> calls;
  std::vector<Event> events;
  std::uint64_t draws = 0;
  Address R0 = 0;
  // Missing services refuse by default. ReferenceZero explicitly opts into
  // ref_player's scalar-zero callback defaults, not real service behavior.
  enum class Policy { RequireHandlers, ReferenceZero };
  const Policy policy;
  explicit Tick(Memory &, std::uint32_t player,
                Policy = Policy::RequireHandlers);
  Tick(const Tick &) = delete;
  Tick &operator=(const Tick &) = delete;
  std::uint32_t u32(Address a) const;
  std::int32_t signed8(Address a) const;
  std::int32_t signed16(Address a) const;
  std::int32_t signed32(Address a) const;
  std::int32_t slot() const;
  std::uint32_t record() const; // This accessor alone wraps like reference R.
  bool is_copy() const;
  // Complete run only for scripted ticks. Ordinary ticks retain entry writes
  // then refuse before +0x4c4, pad selection, switch or tail. No rollback.
  std::string run();
  Address
  choose_pad(); // Returns address; does not assign the reference local pad.
  void idle_fidget(std::uint32_t player, Address pad);
  std::uint32_t can_block(std::uint32_t slot, std::uint32_t state);
  void try_block_move(std::uint32_t player, Address pad);
  std::uint32_t has_anim(std::uint32_t player, std::uint32_t name);
  std::int32_t skill_rank(std::uint32_t slot, std::uint32_t id);
  std::int32_t passive_level(std::uint32_t slot, std::uint64_t mask);
};
const char *service_name(Service);
} // namespace champions::simulation::rta::player
