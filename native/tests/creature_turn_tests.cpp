#include "champions/creature_movement.hpp"
#include <iostream>
#include <memory>
namespace R = champions::simulation::rta;
namespace M = R::creature_movement;
namespace H = R::creature_hp;
namespace {
int checks = 0, failures = 0;
void check(bool ok, const char *what) {
  ++checks;
  if (!ok) {
    ++failures;
    std::cerr << "FAIL: " << what << '\n';
  }
}
template <class F> void refused(F f, const char *what) {
  bool caught = false;
  try {
    f();
  } catch (const H::Error &) {
    caught = true;
  }
  check(caught, what);
}
constexpr M::Address C = 0x10000;
void setup(M::Memory &mem, M::Address c = C, R::Raw rate = 0x3f800000,
           std::uint16_t facing = 0xc000, std::uint8_t slow = 0) {
  mem.poke(c + 0x10, 4, 0);
  mem.poke(c + 0x14, 4, 0);
  mem.poke(c + 0x18, 4, 0x7fc12345);
  mem.poke(c + 0x38, 2, facing);
  mem.poke(c + 0x44, 4, 0x12345678);
  mem.poke(c + 0xb0, 4, rate);
  mem.poke(c + 0x110, 2, 0);
  mem.poke(c + 0x2fc, 1, slow);
  mem.poke(0x404464, 4, 0x89abcdef);
  mem.poke(M::G_SIM_FRAME, 4, 77);
}
const M::Vec3 point{0x3f800000, 0, 0xffc12345};
void literal_cases() {
  for (unsigned config = 0; config < 16; ++config) {
    R::set_fpu(static_cast<R::Fpu::Model>(config / 4), (config & 2) != 0,
               (config & 1) != 0);
    M::Model m;
    setup(m.mem);
    m.callbacks["world_collectObjectsInBox"] = [](M::Model &,
                                                  const auto &) -> M::Value {
      throw H::Error("turn must not invoke collision callbacks");
    };
    check(m.turn_toward(C, 0, point) == 16384, "pre-turn signed angle");
    check(m.mem.load(C + 0x110, 2) == 453,
          "spring stores literal angular velocity");
    check(m.mem.load(C + 0x38, 2) == 49605, "literal facing step");
    check(m.writes.size() == 2 && m.writes[0].address == C + 0x110 &&
              m.writes[0].value == 453 && m.writes[1].address == C + 0x38,
          "spring then facing write order");
    check(m.mem.log().size() == 2 && m.events.size() == 2 && m.calls.empty(),
          "complete stores/events and no callbacks");
    check(m.draws == 0 && m.mem.load(C + 0x44, 4) == 0x12345678 &&
              m.mem.load(0x404464, 4) == 0x89abcdef &&
              m.mem.load(M::G_SIM_FRAME, 4) == 77,
          "object/shared RNG and frame untouched");
    check(m.mem.load(C + 0x18, 4) == 0x7fc12345, "z is unused");
    M::Model opposite;
    setup(opposite.mem, C, 0x3f800000, 0);
    check(opposite.turn_toward(C, 1, point) == -32768,
          "opposite-mode signed half-turn");
    check(opposite.mem.load(C + 0x110, 2) == 64630 &&
              opposite.mem.load(C + 0x38, 2) == 64630,
          "negative spring wrapping");
    M::Model stopped;
    setup(stopped.mem, C, 0);
    check(stopped.turn_toward(C, 2, point) == 16384,
          "other mode uses normal target");
    check(stopped.writes.size() == 3 && stopped.writes[0].value == 453 &&
              stopped.writes[1].value == 0 && stopped.writes[2].value == 49152,
          "zero limit retains first spring store then clamp and facing");
    for (auto slow : {30, 60, 61, 255}) {
      M::Model slowed;
      setup(slowed.mem, C, 0x3f800000, 0xc000, static_cast<std::uint8_t>(slow));
      check(slowed.turn_toward(C, -1, point) == 16384,
            "slow retains pre-turn result");
      const auto expected = slow == 30 ? 49378u : slow == 60 ? 49152u : 49605u;
      check(slowed.mem.load(C + 0x110, 2) == 453 &&
                slowed.mem.load(C + 0x38, 2) == expected,
            "slow changes step only; out-of-range slow bypasses scale");
    }
    M::Model negative;
    setup(negative.mem, C, 0xbf800000, 0);
    check(negative.turn_toward(C, 0, point) == 0,
          "negative-rate pre-turn angle");
    check(negative.writes.size() == 3 && negative.writes[0].value == 0 &&
              negative.writes[1].value == 64444 &&
              negative.mem.load(C + 0x38, 2) == 64444,
          "negative rate is not repaired");
  }
}
class ProtectedVelocity : public H::ReadOverlay {
public:
  bool covers(std::uint32_t a) const override {
    return a == C + 0x110 || a == C + 0x111;
  }
  std::uint8_t read_byte(std::uint32_t) const override { return 0; }
};
void host_cases() {
  R::set_fpu(R::Fpu::Model::Ps2, false, false);
  M::Memory mem;
  setup(mem);
  {
    M::Model m(mem);
    check(m.borrows_memory(), "turn borrows exact existing memory");
    m.turn_toward(C, 0, point);
    check(&m.mem == &mem && mem.load(C + 0x38, 2) == 49605,
          "borrowed owner receives facing");
  }
  check(mem.load(C + 0x110, 2) == 453 && mem.log().size() == 2,
        "memory and full store history survive model destruction");
  M::Limits limits;
  limits.history_entries = 1;
  M::Model limited(M::CallbackPolicy::RequireHandlers, limits);
  setup(limited.mem, C, 0);
  refused([&] { limited.turn_toward(C, 0, point); },
          "write budget rejects second store");
  check(limited.writes.size() == 1 && limited.events.size() == 1 &&
            limited.mem.log().size() == 1 &&
            limited.mem.load(C + 0x110, 2) == 453 &&
            limited.mem.load(C + 0x38, 2) == 49152,
        "budget failure keeps first executed store");
  M::Limits shallow;
  shallow.depth = 0;
  M::Model depth(M::CallbackPolicy::RequireHandlers, shallow);
  setup(depth.mem);
  refused([&] { depth.turn_toward(C, 0, point); }, "operation depth guard");
  check(depth.writes.empty() && depth.mem.log().empty(),
        "depth refusal is before stores");
  M::Model address;
  refused([&] { address.turn_toward(UINT64_C(0x100000000), 0, point); },
          "foreign raw address refuses");
  check(address.writes.empty(), "invalid address leaves histories unchanged");
  const auto near = UINT64_C(0xffffffff) - 0x2fb;
  M::Model late;
  late.mem.poke(near + 0xb0, 4, 0x3f800000);
  late.mem.poke(near + 0x38, 2, 0xc000);
  refused([&] { late.turn_toward(near, 0, point); },
          "late slow-byte address overflow refuses");
  check(late.writes.size() == 1 && late.mem.load(near + 0x110, 2) == 453 &&
            late.mem.load(near + 0x38, 2) == 49152,
        "late native address refusal retains prefix");
  M::Memory protected_mem;
  setup(protected_mem);
  auto overlay = std::make_shared<ProtectedVelocity>();
  protected_mem.attach_overlay(overlay);
  M::Model protected_model(protected_mem);
  refused([&] { protected_model.turn_toward(C, 0, point); },
          "protected overlay store refuses");
  check(protected_model.writes.size() == 1 &&
            protected_model.events.size() == 1 &&
            protected_mem.log().size() == 1 &&
            protected_mem.load(C + 0x110, 2) == 0 &&
            protected_mem.load(C + 0x38, 2) == 49152,
        "overlay retains attempted store but no partial backing change");
}
} // namespace
int main() {
  literal_cases();
  host_cases();
  std::cout << "Creature turning API checks: " << checks
            << "; failures: " << failures << '\n';
  return failures ? 1 : 0;
}
