#pragma once
#include "champions/core_arithmetic.hpp"
#include <cstddef>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace champions::simulation::rta::creature_hp {
// Standalone RTA slot 2. No object-address validity or HP/Session mapping is
// inferred. Arithmetic uses the caller's current core FPU configuration.
using Address = std::uint64_t;
inline constexpr std::uint32_t G_SIM_FRAME = 0x44fdcc;
inline constexpr std::size_t MAX_MEMORY_BYTES = 1024 * 1024,
                             MAX_HISTORY = 65536, MAX_CALLBACKS = 64,
                             MAX_CALLBACK_NAME = 128, MAX_DEPTH = 64;
struct Error : std::runtime_error {
  using std::runtime_error::runtime_error;
};
struct ResourceLimit : Error {
  using Error::Error;
};
struct MissingHandler : Error {
  using Error::Error;
};
struct Limits {
  std::size_t memory_bytes = MAX_MEMORY_BYTES, history_entries = MAX_HISTORY,
              callback_entries = MAX_CALLBACKS, depth = MAX_DEPTH;
};
struct Write {
  std::uint32_t address = 0;
  std::size_t size = 0;
  std::uint64_t value = 0;
};
class Memory {
  Limits limits_;
  std::map<std::uint32_t, std::uint8_t> bytes_;
  std::vector<Write> log_;

public:
  explicit Memory(Limits limits = {});
  // Widths 0..8 and addresses/spans within unsigned 32-bit space only. Missing
  // bytes read as zero without insertion. No external backing-memory overlay.
  std::uint64_t load(Address, std::size_t) const;
  void poke(Address, std::size_t, std::uint64_t);
  void store(Address, std::size_t, std::uint64_t);
  const std::map<std::uint32_t, std::uint8_t> &bytes() const noexcept;
  const std::vector<Write> &log() const noexcept;
  void clear_log() noexcept;
};
struct Call {
  std::string name;
  std::vector<std::uint32_t> args;
};
enum class EventKind { Write, Call };
struct Event {
  EventKind kind = EventKind::Write;
  Write write;
  Call call;
};
enum class CallbackPolicy { RequireHandlers, ReferenceZero };
class Model {
  Limits limits_;
  std::size_t depth_ = 0;
  void write(Address, std::size_t, std::uint64_t);
  void call(const std::string &, std::vector<std::uint32_t>);

public:
  using Callback =
      std::function<void(Model &, const std::vector<std::uint32_t> &)>;
  Memory mem;
  // Callback effects, self-replacement and exceptions are preserved. Copy the
  // callable before invocation; mutable capture identity is a host boundary.
  // Keep authoritative callback state in memory or stable external reference
  // captures. Never destroy this Model while a callback is executing.
  std::map<std::string, Callback> callbacks;
  std::vector<Write> writes;
  std::vector<Call> calls;
  std::vector<Event> events;
  std::uint64_t draws = 0; // This entry point never draws randomness.
  CallbackPolicy policy;
  explicit Model(CallbackPolicy policy = CallbackPolicy::RequireHandlers,
                 Limits limits = {});
  Model(const Model &) = delete;
  Model &operator=(const Model &) = delete;
  Model(Model &&) = delete;
  Model &operator=(Model &&) = delete;
  // Offsets are checked without wrapping, matching the model's address
  // expressions within this explicit address domain. Callback arguments use
  // their low 32 bits. No rollback, failure latch or implicit retry is added.
  void take_damage(Address creature, Address attacker, std::uint32_t flags,
                   Raw amount);
};
} // namespace champions::simulation::rta::creature_hp
