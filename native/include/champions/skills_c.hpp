#pragma once
#include "champions/creature_hp.hpp"
#include <set>

// Standalone Return to Arms contract subset, not a Norrath profile.
namespace champions::simulation::rta::skills_c {
using Memory = creature_hp::Memory;
using Address = creature_hp::Address;
using Write = creature_hp::Write;
struct Unsupported : std::runtime_error {
  using std::runtime_error::runtime_error;
};
struct Call {
  unsigned slot;
  std::vector<std::uint32_t> args;
};
// Operation arguments mirror the pinned reference's method arguments, with the
// target address first (skill, spell, object or handle head/node). Return
// integers and floats are unsigned raw32 patterns. HandleItems flattens pairs;
// void runtime operations return an empty vector. Stack-local handles are not
// representable in this raw32 interface and are unsupported.
enum class Operation : unsigned {
  Stat = 0,
  Tier2,
  ForCaster,
  ForHandle,
  Adjust,
  ManaFromStat4,
  Range,
  CanUseDefault,
  CastDefault,
  ManaDefault,
  ManaCost,
  StatisticSpread,
  RageSwing,
  RageDamage,
  Complement,
  Throwing,
  BattleMage,
  PenetratingArrows,
  SpellBase, SpellRand, ObjectRand, SpellUnit, ObjectUnit,
  SpellBall, ObjectBall, VisualPosition, HandleLink, HandleRemove,
  HandleClear, HandleItems
};
class Model {
  std::uint32_t u32(Address) const;
  std::int32_t half(Address) const;
  std::uint32_t tier(Address, unsigned, unsigned, unsigned, std::uint32_t,
                     std::int32_t);
  std::uint32_t draw();
  void write(Address, unsigned, std::uint32_t);
  std::uint32_t own_draw(Address);
  std::uint32_t unit(Address);
  std::vector<std::uint32_t> ball(Address);
  std::uint32_t handle_take();
  void handle_unlink(Address);
  std::uint32_t scalar(Address, unsigned, std::vector<std::uint32_t>);
  std::vector<std::uint32_t> invoke(Address, unsigned,
                                    std::vector<std::uint32_t>);
  std::uint32_t rage(Address, std::uint32_t, Address, std::uint32_t, unsigned,
                     bool);
  std::size_t depth_ = 0;

public:
  Memory mem;
  std::vector<Write> writes;
  std::vector<Call> calls;
  std::uint64_t draws = 0;
  // Force a known slot to the callback, as Python's external set does. Unknown
  // bodies always delegate. Missing handlers refuse after recording the call.
  std::set<unsigned> external;
  using Callback = std::function<std::vector<std::uint32_t>(
      Model &, const std::vector<std::uint32_t> &)>;
  std::map<unsigned, Callback> callbacks;
  // Callbacks may mutate memory, replace handlers, or throw. No rollback.
  // Neither the model nor its memory may be destroyed during a callback.
  std::vector<std::uint32_t> execute(Operation,
                                     const std::vector<std::uint32_t> &);
  std::vector<std::uint32_t> skill_call(Address skill, unsigned slot,
                                        const std::vector<std::uint32_t> &args);
  [[noreturn]] void cast_or_run(std::uint32_t skill_id);
};
} // namespace champions::simulation::rta::skills_c
