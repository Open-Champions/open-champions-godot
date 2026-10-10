#include "champions/creature_hp.hpp"
#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
namespace R = champions::simulation::rta;
namespace H = R::creature_hp;
namespace {
unsigned checks = 0;
constexpr std::uint32_t C = 0x1000, A = 0x2000, N = 0x3000, T = 0xdeadbeef;
void check(bool value, const char *message) {
  ++checks;
  if (!value)
    throw std::runtime_error(message);
}
template <class E = H::Error, class F> void rejects(F fn, const char *message) {
  bool caught = false;
  try {
    fn();
  } catch (const E &) {
    caught = true;
  }
  check(caught, message);
}
void setup(H::Model &m) {
  m.mem.poke(C + 0x228, 4, 0x41200000); // Ten hit points.
  m.mem.poke(C + 0xc8, 4, R::TWO);
  m.mem.poke(C + 0x2dc, 4, 1);
  m.mem.poke(C + 0x10, 4, R::ONE);
  m.mem.poke(H::G_SIM_FRAME, 4, 17);
}
void widths_and_sparse_memory() {
  static_assert(!std::is_copy_constructible<H::Model>::value,
                "active model identity is not copied");
  static_assert(!std::is_move_constructible<H::Model>::value,
                "active model identity is not moved");
  H::Memory m;
  check(m.load(0x1234, 8) == 0 && m.bytes().empty(),
        "missing memory reads zero without insertion");
  constexpr std::uint64_t value = 0x8877665544332211ULL;
  for (unsigned width = 0; width <= 8; ++width) {
    const auto mask =
        width == 8 ? UINT64_MAX : (UINT64_C(1) << (8 * width)) - 1;
    const auto address = 0x100 + width * 16;
    m.poke(address, width, value);
    check(m.load(address, width) == (value & mask),
          "little-endian width and masking");
    for (unsigned j = 0; j < width; ++j)
      check(m.load(address + j, 1) == ((value >> (8 * j)) & 255),
            "each sparse byte retains little-endian order");
    m.store(address, width, UINT64_MAX);
    check(m.log().back().address == address && m.log().back().size == width &&
              m.log().back().value == mask,
          "store log masks width including zero and eight");
  }
  const auto count = m.bytes().size();
  m.store(0xffffffff, 0, 123);
  m.poke(0xffffffff, 0, 9);
  check(m.load(0xffffffff, 0) == 0 && m.bytes().size() == count &&
            m.log().back().value == 0,
        "zero width has log but no inserted bytes");
  m.poke(0xffffffff, 1, 0xab);
  check(m.load(0xffffffff, 1) == 0xab, "last address byte accepted");
  const auto before = m.bytes();
  const auto logs = m.log().size();
  for (auto width : {2U, 8U}) {
    rejects<H::ResourceLimit>([&] { m.load(0xffffffff, width); },
                              "cross-boundary load refused");
    rejects<H::ResourceLimit>([&] { m.store(0xffffffff, width, 1); },
                              "cross-boundary store refused");
  }
  rejects<H::ResourceLimit>([&] { m.poke(UINT64_C(0x100000000), 0, 1); },
                            "zero-width out-of-domain address refused");
  rejects<H::ResourceLimit>([&] { m.load(0, 9); }, "wide load refused");
  rejects<H::ResourceLimit>([&] { m.poke(0, 9, 1); }, "wide poke refused");
  check(m.bytes() == before && m.log().size() == logs,
        "span and width failures preserve prior state");
  m.clear_log();
  check(m.log().empty() && m.bytes() == before,
        "clearing store history preserves memory");
  H::Memory partial;
  partial.poke(0x102, 2, 0x2211);
  check(partial.load(0x100, 8) == 0x22110000,
        "partially absent bytes independently read zero");
}
void early_paths() {
  for (unsigned value : {1U, 127U, 128U, 255U}) {
    H::Model m;
    setup(m);
    m.mem.poke(C + 0x231, 1, value);
    const auto before = m.mem.bytes();
    m.take_damage(C, 0xffffffff, 0xffffffff, 0x7fffffff);
    check(m.mem.bytes() == before && m.writes.empty() && m.calls.empty() &&
              m.events.empty(),
          "any nonzero signed inert byte exits before attacker/HP access");
  }
  {
    H::Model m;
    const auto creature = UINT64_C(0xfffffdce);
    m.mem.poke(0xffffffff, 1, 1);
    m.take_damage(creature, 0xffffffff, 0, 0);
    check(m.writes.empty(), "inert last-byte field short circuits inaccessible "
                            "later creature fields");
  }
  {
    H::Model m;
    setup(m);
    m.mem.poke(C + 0xa8, 8, UINT64_C(1) << 48);
    for (auto off : {0x348U, 0x344U, 0x340U, 0x350U, 0x34cU})
      m.mem.poke(C + off, 4, 0xffffffff);
    m.take_damage(C, 0xffffffff, 0xffffffff, 0x40400000);
    constexpr std::array<unsigned, 6> offsets{0x3b0, 0x348, 0x344,
                                              0x340, 0x350, 0x34c};
    check(m.writes.size() == 6 && m.calls.empty(),
          "IgnoreDamage writes only arc and five timers");
    for (unsigned i = 0; i < offsets.size(); ++i)
      check(m.writes[i].address == C + offsets[i] &&
                m.writes[i].size == (i ? 4U : 2U) &&
                m.writes[i].value == (i ? 0U : 0x7fffU),
            "IgnoreDamage exact timer order and widths");
    check(m.mem.load(C + 0x228, 4) == 0x41200000 &&
              m.mem.load(C + 0xc8, 4) == R::TWO &&
              m.mem.load(C + 0x2dc, 4) == 1,
          "IgnoreDamage preserves HP pending and flags");
  }
  for (auto hp : {0U, R::MINUS_ONE, 0x80000000U}) {
    H::Model m;
    setup(m);
    m.mem.poke(C + 0x228, 4, hp);
    m.take_damage(C, 0xffffffff, 0, 0);
    check(m.calls.empty() && m.writes.size() == 5,
          "nonpositive HP prevents even an invalid attacker memory read");
  }
}
void arithmetic_and_order() {
  for (unsigned config = 0; config < 16; ++config) {
    R::set_fpu(static_cast<R::Fpu::Model>(config / 4), config & 2, config & 1);
    H::Model m;
    setup(m);
    m.take_damage(C, 0, 0x20, 0x40400000);
    check(m.mem.load(C + 0x228, 4) == 0x40e00000 &&
              m.mem.load(C + 0xc8, 4) == 0x40a00000,
          "exact integer HP subtraction and pending addition in all FPU modes");
    check(m.mem.load(C + 0x304, 4) == 17 && m.mem.load(C + 0x2dc, 4) == 0x21 &&
              m.mem.load(C + 0x3b0, 2) == 0x7fff,
          "frame flags and arc updates");
    check(m.calls.empty() && m.draws == 0 && m.events.size() == 5 &&
              m.mem.log().size() == 5,
          "null attacker skips calls and consumes no random draw");
    constexpr std::array<unsigned, 5> order{0x3b0, 0x304, 0x2dc, 0x228, 0xc8};
    for (unsigned i = 0; i < order.size(); ++i)
      check(m.events[i].kind == H::EventKind::Write &&
                m.events[i].write.address == C + order[i],
            "ordered observable writes");
  }
  R::set_fpu();
}
void callback_reload_and_identity() {
  H::Model m;
  setup(m);
  m.mem.poke(C + 0x10, 4, 0);
  m.mem.poke(C + 0x14, 4, R::ONE);
  m.callbacks["handle_clear"] = [&](H::Model &x, const auto &args) {
    check(args == std::vector<std::uint32_t>{C + 0x2e4},
          "clear receives exact raw handle address");
    check(x.writes.size() == 4 && x.mem.load(C + 0x228, 4) == 0x41200000 &&
              x.mem.load(C + 0x2e0, 2) == 0x4000,
          "arc/frame/flags/axis-angle written before callback and HP not yet "
          "changed");
    x.mem.poke(A + 0xc, 4, 0x80);
    x.mem.poke(A + 0x4c, 4, N);
    x.mem.poke(N + 0xc, 4, T);
    x.mem.poke(C + 0x228, 4, 0x41a00000);
    x.mem.poke(C + 0xc8, 4, 0x40a00000);
  };
  m.callbacks["handle_link"] = [&](H::Model &x, const auto &args) {
    check(args == std::vector<std::uint32_t>({C + 0x2e4, T, T}),
          "source resolution reloads projectile flags and accepts stale raw "
          "target");
    check(x.mem.load(C + 0x228, 4) == 0x41a00000,
          "link sees clear callback mutations");
    x.mem.poke(C + 0x228, 4, 0x41f00000);
    x.mem.poke(C + 0xc8, 4, 0x40e00000);
  };
  m.take_damage(C, A, 0x10000000, 0x40400000);
  check(m.mem.load(C + 0x228, 4) == 0x41d80000 &&
            m.mem.load(C + 0xc8, 4) == 0x41200000,
        "final arithmetic reloads HP and pending after both callbacks");
  check(m.events.size() == 8 && m.events[4].kind == H::EventKind::Call &&
            m.events[4].call.name == "handle_clear" &&
            m.events[5].kind == H::EventKind::Call &&
            m.events[5].call.name == "handle_link" &&
            m.events[6].write.address == C + 0x228 &&
            m.events[7].write.address == C + 0xc8,
        "combined event trace retains callback/write interleaving");
  check(m.mem.log().size() == 6 && m.writes.size() == 6,
        "callback pokes are not mislabeled model writes");
  for (unsigned mode = 0; mode < 4; ++mode) {
    H::Model x(H::CallbackPolicy::ReferenceZero);
    setup(x);
    x.mem.poke(A + 0xc, 4, mode < 2 ? 0x80 : 0);
    x.mem.poke(A + 0x4c, 4, mode == 1 ? N : 0);
    x.mem.poke(C + 0x360, 4, N);
    x.mem.poke(N + 0xc, 4, mode == 3 ? 0 : T);
    x.take_damage(C, A, 0x10000000, 0);
    if (mode == 3)
      check(x.calls.size() == 1,
            "nonzero source node with null object suppresses link");
    else
      check(x.calls.size() == 2 && x.calls[1].args[1] == (mode == 0 ? A : T),
            "projectile precedence null-node fallback and effect-source paths");
  }
  {
    H::Model x(H::CallbackPolicy::ReferenceZero);
    setup(x);
    x.take_damage(C, C, 0, 0);
    check(x.calls.size() == 2 && x.calls[1].args[1] == C,
          "self-attacker retains identity without object-validity filtering");
  }
}
struct AuthoredFailure : std::runtime_error {
  using std::runtime_error::runtime_error;
};
void callbacks_and_partial_failure() {
  for (unsigned where = 0; where < 2; ++where) {
    H::Model m;
    setup(m);
    m.callbacks["handle_clear"] = [](H::Model &, const auto &) {};
    m.callbacks[where ? "handle_link" : "handle_clear"] = [&](H::Model &x,
                                                              const auto &) {
      x.mem.poke(C + 0xc8, 4, 0x42c80000);
      throw AuthoredFailure("unchanged callback exception");
    };
    rejects<AuthoredFailure>([&] { m.take_damage(C, A, 0x20, 0x40400000); },
                             "callback exception retains its exact type");
    check(m.writes.size() == 4 && m.calls.size() == where + 1 &&
              m.mem.load(C + 0x228, 4) == 0x41200000 &&
              m.mem.load(C + 0xc8, 4) == 0x42c80000,
          "callback failure retains prior model writes and callback poke "
          "without final HP update");
    m.callbacks.clear();
    m.policy = H::CallbackPolicy::ReferenceZero;
    m.take_damage(C, A, 0, 0x40400000);
    check(m.mem.load(C + 0x228, 4) == 0x40e00000,
          "explicit later invocation works; no invented failure latch and "
          "depth unwinds");
  }
  {
    H::Model m;
    setup(m);
    rejects<H::MissingHandler>([&] { m.take_damage(C, A, 0, 0); },
                               "required callback missing refuses");
    check(m.calls.size() == 1 && m.calls[0].name == "handle_clear" &&
              m.writes.size() == 4,
          "missing attempted call is logged before refusal");
  }
  {
    H::Model m;
    setup(m);
    unsigned called = 0;
    m.callbacks["handle_clear"] = [&](H::Model &x, const auto &) {
      x.callbacks.erase("handle_clear");
      x.callbacks["handle_link"] = [&](H::Model &, const auto &) { ++called; };
    };
    m.take_damage(C, A, 0, 0);
    check(called == 1 && !m.callbacks.count("handle_clear"),
          "copied active callback survives self-erasure; later lookup sees "
          "replacement");
  }
  {
    R::set_fpu(R::Fpu::Model::Ps2);
    H::Model m;
    setup(m);
    m.mem.poke(C + 0x228, 4, R::ONE);
    m.callbacks["handle_clear"] = [](H::Model &, const auto &) {
      R::set_fpu(R::Fpu::Model::Nearest);
    };
    m.callbacks["handle_link"] = [](H::Model &, const auto &) {};
    m.take_damage(C, A, 0, 0x33000000);
    check(
        m.mem.load(C + 0x228, 4) == R::ONE,
        "callback-selected nearest context controls subsequent HP arithmetic");
    check(R::sub(R::ONE, 0x33000000) == R::ONE,
          "model does not overwrite callback-selected caller FPU context");
    R::set_fpu();
  }
}
void bounds_and_exhaustion() {
  {
    H::Limits limits;
    limits.memory_bytes = 2;
    H::Memory m(limits);
    m.poke(0, 2, 0x2211);
    rejects<H::ResourceLimit>([&] { m.poke(1, 2, 0x4433); },
                              "sparse capacity prevents partial overwrite");
    check(m.load(0, 2) == 0x2211 && m.bytes().size() == 2,
          "poke budget preflight preserves existing bytes");
    rejects<H::ResourceLimit>([&] { m.store(2, 1, 0xff); },
                              "store byte capacity failure explicit");
    check(m.log().size() == 1 && m.log()[0].address == 2 &&
              m.bytes().size() == 2,
          "store attempt recorded before byte-budget refusal");
  }
  {
    H::Limits limits;
    limits.history_entries = 2;
    H::Memory m(limits);
    m.store(0, 0, 7);
    m.store(1, 0, 9);
    rejects<H::ResourceLimit>([&] { m.store(2, 0, 1); },
                              "zero-byte stores still obey history bound");
    check(m.log().size() == 2 && m.bytes().empty(),
          "history exhaustion has no phantom insertion");
  }
  {
    H::Limits limits;
    limits.history_entries = 2;
    H::Model m(H::CallbackPolicy::ReferenceZero, limits);
    setup(m);
    rejects<H::ResourceLimit>([&] { m.take_damage(C, 0, 0x20, 0); },
                              "model event history exhaustion explicit");
    check(m.writes.size() == 2 && m.events.size() == 2 &&
              m.mem.load(C + 0x2dc, 4) == 1,
          "history failure preserves precise two-write prefix");
  }
  {
    H::Limits limits;
    limits.callback_entries = 0;
    H::Model m(H::CallbackPolicy::ReferenceZero, limits);
    setup(m);
    m.callbacks["unused"] = [](H::Model &, const auto &) {};
    rejects<H::ResourceLimit>([&] { m.take_damage(C, A, 0, 0); },
                              "callback registration budget checked at call");
    check(m.calls.size() == 1 && m.writes.size() == 4,
          "callback cap retains attempted-call prefix");
  }
  {
    H::Limits limits;
    limits.depth = 2;
    H::Model m(H::CallbackPolicy::ReferenceZero, limits);
    setup(m);
    m.callbacks["handle_clear"] = [](H::Model &x, const auto &) {
      x.take_damage(C, A, 0, 0);
    };
    rejects<H::ResourceLimit>([&] { m.take_damage(C, A, 0, 0); },
                              "nested damage bound explicit");
    check(m.calls.size() == 2 && m.writes.size() == 8,
          "nested refusal keeps both observable prefixes");
    m.callbacks.clear();
    m.take_damage(C, 0, 0, 0);
    check(m.writes.size() == 13,
          "nested exception balances depth for later explicit call");
  }
  {
    H::Model m;
    setup(m);
    rejects<H::ResourceLimit>(
        [&] { m.take_damage(C, UINT64_C(0x100000000), 0, 0); },
        "out-of-domain attacker address rejected");
    check(m.writes.empty(), "address-domain refusal precedes body mutation");
  }
  {
    H::Limits invalid;
    invalid.depth = H::MAX_DEPTH + 1;
    rejects<H::ResourceLimit>(
        [&] { H::Model m(H::CallbackPolicy::ReferenceZero, invalid); },
        "limits cannot exceed hard cap");
  }
}
} // namespace
int main() {
  try {
    widths_and_sparse_memory();
    early_paths();
    arithmetic_and_order();
    callback_reload_and_identity();
    callbacks_and_partial_failure();
    bounds_and_exhaustion();
    std::cout << checks << " creature HP checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "creature HP check failed: " << e.what() << '\n';
    return 1;
  }
}
