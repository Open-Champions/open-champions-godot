#pragma once
#include "champions/creature_movement.hpp"
#include <array>
#include <optional>
#include <vector>

namespace champions::simulation::rta::creature_water {
using Address = creature_hp::Address;
using Memory = creature_hp::Memory;
using Write = creature_hp::Write;
using Vec3 = std::array<Raw, 3>;

struct Unsupported : creature_hp::Error {
  using Error::Error;
};

// Supplied relocated tree, chunk table, payloads and runtime grids. The owner
// keeps Memory and this service alive while an installed callback can run.
// The service records ordered writes separately from creature bookkeeping.
class Service {
  Memory &memory_;
  std::vector<Write> writes_;
  std::uint64_t read(Address, std::size_t) const;
  void write(Address, std::size_t, std::uint64_t);

public:
  explicit Service(Memory &memory) : memory_(memory) {}
  Memory &memory() const noexcept { return memory_; }
  const std::vector<Write> &writes() const noexcept { return writes_; }
  void clear_writes() noexcept { writes_.clear(); }
  std::vector<std::uint32_t> query(Address world, Vec3 lo, Vec3 hi,
                                   std::size_t capacity,
                                   std::optional<Address> out = std::nullopt);
  std::uint32_t water_test(Raw radius, Address world, Vec3 position,
                           Address height_out = 0);
  std::uint32_t water_disturb(Raw strength, Raw radius, Address world,
                              Vec3 position);
};

// Explicit opt-in for the existing creature caller. No Session policy or
// static WORLD-to-runtime-grid conversion is inferred. The service must share
// the creature Model's Memory and outlive the installed callbacks.
void install_creature_callbacks(creature_movement::Model &creature,
                                Service &service);
} // namespace champions::simulation::rta::creature_water
