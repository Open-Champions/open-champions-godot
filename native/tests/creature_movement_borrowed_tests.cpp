#include "champions/creature_movement.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>
#include <type_traits>

namespace R = champions::simulation::rta;
namespace H = R::creature_hp;
namespace M = R::creature_movement;
namespace {
unsigned checks = 0;
constexpr std::uint32_t C = 0x1000;
void check(bool value, const char *why) {
  ++checks;
  if (!value)
    throw std::runtime_error(why);
}
template <class E, class F> void rejects(F fn, const char *why) {
  bool caught = false;
  try {
    fn();
  } catch (const E &) {
    caught = true;
  }
  check(caught, why);
}
void setup(H::Memory &memory) {
  memory.poke(C + 0x1c, 4, R::ONE);
  memory.poke(C + 0x20, 4, 0x41000000);
  memory.poke(C + 0xa8, 8, 0x100000);
  memory.poke(C + 0x228, 4, 0x42c80000);
  memory.poke(C + 0x2dc, 4, 1);
}
struct Overlay final : H::ReadOverlay {
  std::uint32_t address = C + 0x231;
  std::uint8_t value = 8;
  bool covers(std::uint32_t a) const override { return a == address; }
  std::uint8_t read_byte(std::uint32_t a) const override {
    if (a != address)
      throw H::Error("unexpected overlay address");
    return value;
  }
};
void owned_default() {
  static_assert(!std::is_copy_constructible<M::Model>::value,
                "movement model cannot duplicate borrowed identity");
  static_assert(!std::is_move_constructible<M::Model>::value,
                "movement model cannot move active references");
  M::Model a(M::CallbackPolicy::ReferenceZero),
      b(M::CallbackPolicy::ReferenceZero);
  check(&a.mem != &b.mem && !a.borrows_memory() && !b.borrows_memory(),
        "default constructors still own distinct memories");
  setup(a.mem);
  setup(b.mem);
  a.mem.poke(C + 0x18, 4, R::ONE);
  check(b.mem.load(C + 0x18, 4) == 0,
        "standalone setup never aliases another default model");
  check(b.move_by(C, {}) == 4 && a.mem.log().empty(),
        "default standalone movement retains its isolated store history");
}
void hp_storage_identity() {
  H::Model hp(H::CallbackPolicy::ReferenceZero);
  setup(hp.mem);
  const auto original = hp.mem.bytes();
  {
    M::Model movement(hp.mem, M::CallbackPolicy::ReferenceZero);
    check(movement.borrows_memory() && &movement.mem == &hp.mem,
          "borrowed movement exposes the actual HP memory object");
    hp.take_damage(C, 0, 0, R::TWO);
    check(movement.mem.load(C + 0x228, 4) == 0x42c40000,
          "HP write is visible immediately without copying or resynchronizing");
    const auto hp_writes = hp.writes.size();
    const auto store_count = hp.mem.log().size();
    check(movement.move_by(C, {}) == 4,
          "borrowed movement executes original landing");
    check(hp.mem.log().size() == store_count + movement.writes.size() &&
              hp.writes.size() == hp_writes,
          "shared stores are single history while operation histories stay "
          "separate");
    check(movement.mem.load(C + 0xc8, 4) == R::TWO &&
              movement.mem.load(C + 0x228, 4) == 0x42c40000,
          "movement preserves pending damage and persistent HP in the same "
          "bytes");
    movement.mem.poke(C + 0x228, 4, 0x42c00000);
    check(hp.mem.load(C + 0x228, 4) == 0x42c00000,
          "borrower write is immediately owner-visible in reverse direction");
    hp.take_damage(C, 0, 0, R::TWO);
    check(movement.mem.load(C + 0x228, 4) == 0x42bc0000,
          "later HP operation reads borrower-updated health");
    check(original.at(C + 0x228 + 2) == 0xc8,
          "previous copied sparse observation remains independent");
  }
  check(hp.mem.load(C + 0x228, 4) == 0x42bc0000 && !hp.mem.log().empty(),
        "destroying borrower preserves the live owner's state and histories");
  hp.mem.poke(C + 0x228, 4, R::ONE);
  check(hp.mem.load(C + 0x228, 4) == R::ONE,
        "owner remains usable after borrower ends; no expired-owner access "
        "tested");
}
void overlay_and_failures() {
  H::Memory memory;
  setup(memory);
  auto overlay = std::make_shared<Overlay>();
  memory.attach_overlay(overlay);
  {
    M::Model movement(memory, M::CallbackPolicy::ReferenceZero);
    check(movement.mem.overlay() == overlay && movement.move_by(C, {}) == 0 &&
              movement.writes.empty() && memory.log().empty(),
          "borrower observes owner's read-through hidden byte immediately");
    overlay->value = 0;
    check(movement.move_by(C, {}) == 4,
          "changing live overlay data affects the next original operation");
    memory.clear_log();
    movement.writes.clear();
    movement.events.clear();
    movement.calls.clear();
    overlay->address = C + 0x38;
    overlay->value = 7;
    const auto before = memory.bytes();
    rejects<H::Error>(
        [&] { movement.move_by(C, {}); },
        "managed yaw write is rejected by shared read-only overlay");
    check(memory.bytes() == before && memory.load(C + 0x38, 2) == 7 &&
              memory.log().size() == 1 && movement.writes.size() == 1 &&
              movement.events.size() == 1 && movement.calls.empty(),
          "overlay refusal retains exactly the attempted model and shared "
          "store write");
    check(memory.log()[0].address == C + 0x38 && movement.writes[0].value == 7,
          "attempted write uses effective read value without overwriting "
          "backing");
  }
  check(memory.overlay() == overlay,
        "borrower destruction neither detaches nor destroys owner's overlay");
  memory.detach_overlay(overlay);
  check(!memory.overlay(),
        "owner controls overlay lifetime after borrower ends");
}
void independent_limits() {
  {
    H::Limits owner_limit;
    owner_limit.memory_bytes = 3;
    H::Memory owner(owner_limit);
    M::Model movement(owner, M::CallbackPolicy::ReferenceZero);
    rejects<H::ResourceLimit>(
        [&] { movement.obj_rand(C); },
        "borrowed storage keeps its owner's capacity limit");
    check(owner.bytes().empty() && owner.log().size() == 1 &&
              movement.writes.size() == 1,
          "owner capacity error retains attempted write without partial bytes");
  }
  {
    H::Memory owner;
    setup(owner);
    M::Limits model_limit;
    model_limit.memory_bytes =
        0; // Borrowed memory's own limit is authoritative.
    model_limit.history_entries = 1;
    M::Model movement(owner, M::CallbackPolicy::ReferenceZero, model_limit);
    rejects<H::ResourceLimit>(
        [&] { movement.move_by(C, {}); },
        "borrower's separate operation history is bounded");
    check(owner.log().size() == 1 && movement.writes.size() == 1 &&
              owner.load(C + 0x38, 2) == 0,
          "model history limit retains first real shared write");
    owner.poke(C + 0x30, 4, R::ONE);
    check(owner.load(C + 0x30, 4) == R::ONE,
          "borrower's zero memory setting does not replace owner limits");
  }
  {
    H::Limits limit;
    limit.history_entries = 0;
    H::Memory owner(limit);
    M::Model movement(owner, M::CallbackPolicy::ReferenceZero);
    rejects<H::ResourceLimit>(
        [&] { movement.obj_rand(C); },
        "shared store history cap still belongs to owner");
    check(movement.writes.size() == 1 && owner.log().empty() &&
              owner.bytes().empty(),
          "store cap stops backing mutation after the model attempted-write "
          "event");
  }
}
} // namespace

int main() {
  try {
    R::ScopedFpu context(R::Fpu{});
    owned_default();
    hp_storage_identity();
    overlay_and_failures();
    independent_limits();
    std::cout << "borrowed creature movement: " << checks << " checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "borrowed creature movement failed after " << checks << ": "
              << e.what() << '\n';
    return 1;
  }
}
