// Authored adapter invariants, separate from the independently generated
// oracle.
#include "champions/combat_creature.hpp"
#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>
namespace R = champions::simulation::rta;
namespace C = R::combat;
namespace H = R::creature_hp;
namespace B = R::combat_creature;
namespace {
unsigned checks = 0;
constexpr std::uint32_t V = 0x10000, A = 0x20000, NODE = 0x30000;
void check(bool value, const char *message) {
  ++checks;
  if (!value)
    throw std::runtime_error(message);
}
template <class E = B::Error, class F> void rejects(F fn, const char *message) {
  bool caught = false;
  try {
    fn();
  } catch (const E &) {
    caught = true;
  }
  check(caught, message);
}
struct CallbackFailure : std::runtime_error {
  using std::runtime_error::runtime_error;
};
C::Obj creature() {
  C::Obj value;
  value.flags = 0x20;
  value.hp = 0x41200000; // Ten.
  value.max_hp = 0x41a00000;
  value.pos = {0, R::ONE, 0};
  return value;
}
void ownership_and_ranges() {
  static_assert(!std::is_copy_constructible<B::Adapter>::value);
  static_assert(!std::is_move_constructible<B::Adapter>::value);
  B::Adapter b;
  auto &v = b.create(V, creature());
  const auto first = b.binding(v);
  check(first.address == V && first.generation != 0 && first.extent == 0x3b2,
        "stable binding has address, generation and projected extent");
  for (unsigned i = 0; i < 32; ++i)
    b.create(A + i * 0x1000, creature());
  check(b.lookup(V) == &v && b.binding(v).generation == first.generation,
        "registry growth preserves record identity and generation");
  check(b.lookup(V + 1) == nullptr,
        "interior address is not an object identity");
  auto copy = b.bindings();
  copy.front().address = 0;
  check(b.binding(v).address == V, "binding metadata is an owned copy");
  rejects([&] { b.create(V); }, "duplicate address rejected");
  rejects([&] { b.create(V + 0x100); },
          "overlapping projected extent rejected");
  rejects([&] { b.reserve_raw(V + 1, 1, "overlap"); },
          "raw/projected overlap rejected");
  rejects([&] { b.create(0); }, "null address cannot identify an owned record");
  rejects([&] { b.create(0xfffffff0); }, "projected span wrap rejected");
  b.reserve_raw(NODE + 0x40000, 20, "authored node");
  rejects([&] { b.create(NODE + 0x40000); },
          "raw reservation blocks projected overlap");
  auto regions = b.raw_regions();
  regions.front().label = "changed";
  check(b.raw_regions().front().label == "authored node",
        "raw region metadata is copied");
  C::Obj foreign = creature();
  rejects([&] { b.binding(foreign); }, "foreign object has no binding");
  const auto old = b.model().mem.bytes();
  rejects([&] { b.take_damage(foreign, R::ONE, nullptr, 0); },
          "unbound victim refused");
  rejects([&] { b.take_damage(v, R::ONE, &foreign, 0); },
          "unbound attacker refused");
  check(b.model().mem.bytes() == old && b.operation_depth() == 0,
        "identity failure occurs before raw projection and unwinds depth");
}
void projection_and_actual_hits() {
  for (unsigned config = 0; config < 16; ++config) {
    R::ScopedFpu selected(
        R::Fpu(static_cast<R::Fpu::Model>(config / 4), config & 2, config & 1));
    B::Adapter b(0x12345678, 0xdeadbeef, H::CallbackPolicy::ReferenceZero);
    auto &v = b.create(V, creature());
    v.flags = 0x20;
    v.cflags = UINT64_C(0x9000000000000000);
    v.status = {R::ONE, R::TWO, C::THREE, C::FIVE, 0x40e00000};
    b.world().frame = UINT32_MAX;
    b.model().mem.poke(V + 0xc8, 4, R::TWO);
    b.model().mem.poke(V + 0x227, 1, 0xa5);
    b.model().mem.poke(V + 0x22c, 4, 0xabcdef01);
    b.take_damage(v, C::THREE, nullptr, 0x10);
    check(v.hp == 0x40e00000 && b.model().mem.load(V + 0xc8, 4) == C::FIVE,
          "real HP call subtracts three and accumulates pending two to five");
    check(v.flags == 0x20 && v.cflags == UINT64_C(0x9000000000000000),
          "projection retains flags32 and all cflags64 bits");
    check(b.model().mem.load(H::G_SIM_FRAME, 4) == UINT32_MAX &&
              b.model().mem.load(V + 0x304, 4) == UINT32_MAX,
          "world frame projection and HP stamp preserve raw32");
    check(v.status == std::vector<R::Raw>(
                          {R::ONE, R::TWO, C::THREE, C::FIVE, 0x40e00000}),
          "all status entries survive normal HP call");
    check(b.model().mem.load(V + 0x227, 1) == 0xa5 &&
              b.model().mem.load(V + 0x22c, 4) == 0xabcdef01 &&
              v.max_hp == 0x41a00000,
          "adjacent and unprojected raw fields survive projection");
    check(b.world().seed.state == 0x12345678 &&
              b.world().cosmetic.state == 0xdeadbeef &&
              b.world().seed.draws.empty() &&
              b.world().cosmetic.draws.empty() && b.model().draws == 0,
          "direct HP integration consumes neither random domain");
    v.hp = 0x41a00000;
    b.take_damage(v, R::ONE, nullptr, 0);
    check(v.hp == 0x41980000, "each actual callback projects fresh typed HP");
    check(b.operation_depth() == 0 && b.raw_callback_depth() == 0,
          "normal call restores depths");
  }
}
void raw_callbacks_and_partial_publish() {
  B::Adapter b(7, 9);
  auto &v = b.create(V, creature());
  auto &a = b.create(A, creature());
  b.reserve_raw(NODE, 20, "authored handle backing");
  b.set_handle_callback(
      "handle_clear", "clear", [&](B::Adapter &x, const auto &args) {
        check(args == std::vector<std::uint32_t>{V + 0x2e4},
              "handle receives exact raw head address");
        check(x.operation_depth() != 0 && x.raw_callback_depth() != 0,
              "raw callback has active depths");
        x.model().mem.poke(V + 0x228, 4, 0x41a00000);
        x.model().mem.poke(V + 0xc8, 4, C::FIVE);
        x.model().mem.poke(A + 0xc, 4, 0x80);
        x.model().mem.poke(A + 0x4c, 4, NODE);
        x.model().mem.poke(NODE + 0xc, 4, 0xdeadbeef);
      });
  b.set_handle_callback(
      "handle_link", "link", [&](B::Adapter &x, const auto &args) {
        check(args == std::vector<std::uint32_t>(
                          {V + 0x2e4, 0xdeadbeef, 0xdeadbeef}),
              "stale raw target remains raw; registered-record lookup is not "
              "substituted");
        check(v.hp == 0x41a00000 && a.flags == 0x80,
              "raw changes are published before the next callback");
        x.model().mem.poke(V + 0x228, 4, 0x41f00000);
        throw CallbackFailure("authored");
      });
  rejects<CallbackFailure>([&] { b.take_damage(v, C::THREE, &a, 0); },
                           "callback exception is preserved");
  check(v.hp == 0x41f00000 && b.model().mem.load(V + 0xc8, 4) == C::FIVE,
        "throw publishes raw HP while final HP subtraction has not happened");
  check(b.model().writes.size() == 4 && b.model().calls.size() == 2,
        "failed handle call preserves exact model prefix");
  check(b.operation_depth() == 0 && b.raw_callback_depth() == 0,
        "exception unwinds depths");
  b.set_handle_callback("handle_link", "recovered",
                        [](B::Adapter &, const auto &) {});
  b.take_damage(v, R::ONE, nullptr, 0);
  check(v.hp == 0x41e80000, "adapter has no invented failure latch");
}
void source_routes() {
  for (unsigned mode = 0; mode < 5; ++mode) {
    B::Adapter b(0, 0, H::CallbackPolicy::ReferenceZero);
    auto &v = b.create(V, creature());
    auto &a = b.create(A, creature());
    b.reserve_raw(NODE, 20, "node");
    if (mode < 3)
      a.flags |= 0x80;
    b.model().mem.poke(A + 0x4c, 4, mode == 0 ? 0 : NODE);
    b.model().mem.poke(V + 0x360, 4, NODE);
    b.model().mem.poke(NODE + 0xc, 4, mode == 2 || mode == 4 ? 0 : 0xf0000010);
    b.take_damage(v, R::ONE, &a, 0x10000000);
    const auto &calls = b.model().calls;
    if (mode == 2 || mode == 4)
      check(calls.size() == 1, "nonzero node plus zero target suppresses link");
    else
      check(calls.size() == 2 &&
                calls[1].args[1] == (mode == 0 ? A : 0xf0000010),
            "null projectile node falls back to attacker; other routes retain "
            "stale target");
  }
  B::Adapter b(0, 0, H::CallbackPolicy::ReferenceZero);
  auto &v = b.create(V, creature());
  auto &a =
      b.create(A, creature(), B::Projection::None, B::Dispatch::Custom, 0x60);
  a.flags = 0x80;
  a.pos = {C::FIVE, C::FIVE, C::FIVE};
  b.model().mem.poke(A + 0xc, 4, 0);
  b.model().mem.poke(A + 0x10, 4, 0);
  b.model().mem.poke(A + 0x14, 4, 0);
  b.take_damage(v, R::ONE, &a, 0);
  check(b.model().mem.load(A + 0xc, 4) == 0 && a.flags == 0x80 &&
            a.pos[0] == C::FIVE,
        "None projection leaves raw attacker and typed record independently "
        "authoritative");
}
void nested_raw_authority() {
  B::Adapter b(0, 0, H::CallbackPolicy::ReferenceZero);
  auto &v = b.create(V, creature());
  auto &a = b.create(A, creature());
  bool entered = false;
  b.set_before_base("nested", [&](B::Adapter &x, C::Obj &target, R::Raw,
                                  C::Obj *, std::uint32_t) {
    if (&target != &v || entered)
      return;
    entered = true;
    x.model().mem.poke(V + 0x228, 4, 0x41a00000);
    x.model().mem.poke(A + 0x228, 4, 0x41f00000);
    x.take_damage(a, C::FIVE, nullptr, 0);
    check(
        a.hp == 0x41c80000 && v.hp == 0x41a00000,
        "nested HP sees raw-authoritative twenty and thirty before projection");
  });
  b.take_damage(v, C::THREE, nullptr, 0);
  check(v.hp == 0x41880000 && a.hp == 0x41c80000,
        "outer and nested HP both retained");
  check(b.model().writes.size() == 10,
        "two actual HP invocations produce two write sequences");
  B::Adapter throwing(0, 0, H::CallbackPolicy::ReferenceZero);
  auto &t = throwing.create(V, creature());
  throwing.set_after_base(
      "after", [&](B::Adapter &x, C::Obj &, R::Raw, C::Obj *, std::uint32_t) {
        check(t.hp == 0x41100000, "normal base HP published before after hook");
        x.model().mem.poke(V + 0x228, 4, C::FIVE);
        throw CallbackFailure("after");
      });
  rejects<CallbackFailure>([&] { throwing.take_damage(t, R::ONE, nullptr, 0); },
                           "after hook exception retained");
  check(t.hp == C::FIVE && throwing.model().writes.size() == 5,
        "after exception publishes new raw value");
}
void dispatch_and_bounds() {
  B::Adapter b;
  auto &v = b.create(V, creature(), B::Projection::None, B::Dispatch::Custom);
  rejects([&] { b.take_damage(v, R::ONE, nullptr, 0); },
          "custom dispatch requires explicit binding");
  unsigned calls = 0;
  b.bind_custom(v, "authored",
                [&](B::Adapter &x, C::Obj &target, R::Raw amount, C::Obj *,
                    std::uint32_t flags) {
                  ++calls;
                  check(&target == &v && amount == R::TWO && flags == 7,
                        "custom dispatch scalar arguments");
                  target.hp = R::sub(target.hp, amount);
                  check(x.model().writes.empty(),
                        "custom dispatch never silently calls base HP");
                });
  b.take_damage(v, R::TWO, nullptr, 7);
  check(calls == 1 && v.hp == 0x41000000 && b.model().mem.bytes().empty(),
        "custom-only dispatch preserves its own behavior");
  B::Adapter missing;
  auto &m = missing.create(V, creature());
  auto &a = missing.create(A, creature());
  rejects<H::MissingHandler>(
      [&] { missing.take_damage(m, R::ONE, &a, 0); },
      "missing handle callback refuses with model error");
  check(m.hp == 0x41200000 && missing.model().calls.size() == 1 &&
            missing.model().writes.size() == 4,
        "missing handler retains observable pre-HP prefix");
  B::Limits cap;
  cap.entries = 1;
  B::Adapter limited(0, 0, H::CallbackPolicy::ReferenceZero, cap);
  limited.create(V, creature());
  rejects<B::ResourceLimit>([&] { limited.create(A, creature()); },
                            "entry cap refuses without replacing original");
  check(limited.bindings().size() == 1 &&
            limited.binding(*limited.lookup(V)).generation == 1,
        "failed create leaves stable registry");
  cap = B::Limits{};
  cap.depth = 2;
  B::Adapter recursive(0, 0, H::CallbackPolicy::ReferenceZero, cap);
  auto &r = recursive.create(V, creature());
  recursive.set_before_base("recurse", [&](B::Adapter &x, C::Obj &target,
                                           R::Raw, C::Obj *, std::uint32_t) {
    x.take_damage(target, R::ONE, nullptr, 0);
  });
  rejects<B::ResourceLimit>(
      [&] { recursive.take_damage(r, R::ONE, nullptr, 0); },
      "nested invocation cap explicitly refuses");
  check(recursive.operation_depth() == 0 &&
            recursive.raw_callback_depth() == 0 &&
            recursive.model().writes.empty(),
        "recursion refusal unwinds depths without invented HP success");
}

void authority_and_refusals() {
  for (unsigned kind = 0; kind < 5; ++kind) {
    B::Adapter b(0, 0, H::CallbackPolicy::ReferenceZero);
    auto &v = b.create(V, creature());
    if (kind == 0)
      v.flags = UINT64_C(1) << 32;
    if (kind == 1)
      v.pos.pop_back();
    if (kind == 2)
      v.status.push_back(0);
    if (kind == 3)
      b.world().frame = -1;
    if (kind == 4)
      b.world().frame = INT64_C(0x100000000);
    rejects([&] { b.take_damage(v, R::ONE, nullptr, 0); },
            "unrepresentable projected field refused");
    check(b.model().mem.bytes().empty() && b.model().writes.empty() &&
              b.operation_depth() == 0,
          "projection validation has no partial sparse-memory publication");
  }
  B::Adapter b(0, 0, H::CallbackPolicy::ReferenceZero);
  auto &v = b.create(V, creature());
  b.set_before_base(
      "invalid typed write",
      [&](B::Adapter &x, C::Obj &target, R::Raw, C::Obj *, std::uint32_t) {
        x.model().mem.poke(V + 0x228, 4, C::FIVE);
        target.hp = R::ONE;
      });
  rejects(
      [&] { b.take_damage(v, R::ONE, nullptr, 0); },
      "raw callback cannot silently replace authoritative HP with typed write");
  check(v.hp == C::FIVE && b.model().writes.empty() &&
            b.raw_callback_depth() == 0,
        "typed-write refusal republishes raw HP and preserves pre-base prefix");
  B::Adapter setup(0, 0, H::CallbackPolicy::ReferenceZero);
  auto &s = setup.create(V, creature());
  unsigned refused = 0;
  setup.set_before_base("setup guards", [&](B::Adapter &x, C::Obj &, R::Raw,
                                            C::Obj *, std::uint32_t) {
    const auto attempt = [&](auto f) {
      try {
        f();
      } catch (const B::Error &) {
        ++refused;
      }
    };
    attempt([&] { x.create(A); });
    attempt([&] { x.reserve_raw(NODE, 4, "late"); });
    attempt([&] { x.project(); });
    attempt([&] { x.publish(); });
  });
  setup.take_damage(s, R::ONE, nullptr, 0);
  check(
      refused == 4 && setup.bindings().size() == 1 &&
          setup.raw_regions().empty(),
      "live operation refuses registration and explicit setup synchronization");
  B::Limits caps;
  caps.raw_regions = 1;
  B::Adapter regions(0, 0, H::CallbackPolicy::ReferenceZero, caps);
  regions.reserve_raw(NODE, 4, "one");
  rejects<B::ResourceLimit>([&] { regions.reserve_raw(NODE + 4, 4, "two"); },
                            "raw region cap is explicit");
  rejects([&] { regions.create(H::G_SIM_FRAME); },
          "simulation frame storage cannot alias a projected record");
  caps = B::Limits{};
  caps.memory.memory_bytes = 51;
  B::Adapter small(0, 0, H::CallbackPolicy::ReferenceZero, caps);
  auto &sv = small.create(V, creature());
  rejects<B::ResourceLimit>(
      [&] { small.take_damage(sv, R::ONE, nullptr, 0); },
      "complete scalar projection capacity is preflighted");
  check(small.model().mem.bytes().empty() && sv.hp == 0x41200000,
        "capacity refusal leaves typed and raw setup unchanged");
  B::Adapter labels;
  rejects(
      [&] {
        labels.set_handle_callback("unknown", "x", [](auto &, const auto &) {});
      },
      "unknown callback name refused");
  rejects(
      [&] {
        labels.set_handle_callback("handle_clear", "",
                                   [](auto &, const auto &) {});
      },
      "empty callback label refused");
  rejects([&] { labels.set_handle_callback("handle_clear", "x", {}); },
          "missing callback callable refused");
  auto &lv = labels.create(V, creature());
  rejects(
      [&] {
        labels.bind_custom(lv, std::string(129, 'x'),
                           [](auto &, auto &, auto, auto *, auto) {});
      },
      "oversized custom label refused");
  B::Adapter identity;
  C::Obj supplied = creature();
  supplied.owner = &lv;
  supplied.status_source = &lv;
  supplied.last_attacker = &lv;
  auto &owned = identity.create(A, supplied);
  check(owned.owner == &lv && owned.status_source == &lv &&
            owned.last_attacker == &lv,
        "nonprojected pointer fields retain supplied identity; no guessed raw "
        "handles");
}
void selected_fpu_and_callback_replacement() {
  R::ScopedFpu restore(R::Fpu{});
  auto &context = R::set_fpu(R::Fpu::Model::Chop, true, true);
  B::Adapter b(123, 456, H::CallbackPolicy::ReferenceZero);
  auto &v = b.create(V, creature());
  auto &a = b.create(A, creature());
  unsigned first = 0, second = 0;
  b.set_handle_callback(
      "handle_clear", "first", [&](B::Adapter &x, const auto &) {
        ++first;
        x.set_handle_callback("handle_clear", "second",
                              [&](auto &, const auto &) { ++second; });
        R::set_fpu(R::Fpu::Model::Nearest, false, false);
      });
  b.take_damage(v, R::ONE, &a, 0);
  b.take_damage(v, R::ONE, &a, 0);
  check(
      first == 1 && second == 1,
      "copied callback survives replacement and next lookup sees new handler");
  check(context.model == R::Fpu::Model::Nearest && !context.mul_bias &&
            !context.div_nearest,
        "explicit callback FPU mutation remains authoritative, no hidden "
        "context reset");
  check(v.hp == 0x41000000 && b.world().seed.state == 123 &&
            b.world().cosmetic.state == 456,
        "callback replacement preserves HP and both untouched random domains");
  b.set_handle_callback("handle_clear", "throwing context",
                        [&](auto &, const auto &) {
                          R::set_fpu(R::Fpu::Model::Ps2g1, true, false);
                          throw CallbackFailure("fpu");
                        });
  rejects<CallbackFailure>([&] { b.take_damage(v, R::ONE, &a, 0); },
                           "FPU-changing callback failure preserved");
  check(context.model == R::Fpu::Model::Ps2g1 && context.mul_bias &&
            !context.div_nearest && b.operation_depth() == 0,
        "exception preserves deliberate FPU change while unwinding adapter "
        "depth");
}
void damage_composition() {
  for (unsigned config = 0; config < 16; ++config) {
    R::ScopedFpu selected(
        R::Fpu(static_cast<R::Fpu::Model>(config / 4), config & 2, config & 1));
    B::Adapter b(1, 2, H::CallbackPolicy::ReferenceZero);
    auto &v = b.create(V, creature());
    b.world().frame = 32;
    v.dot = R::TWO;
    std::vector<R::Raw> amounts;
    b.set_before_base("observe",
                      [&](B::Adapter &, C::Obj &, R::Raw amount, C::Obj *,
                          std::uint32_t) { amounts.push_back(amount); });
    check(b.damage(v, nullptr, C::THREE, C::THREE, 100000, 0x800000) == 1,
          "real DOT damage succeeds");
    check(amounts == std::vector<R::Raw>({C::FIVE, 0}) && v.hp == C::FIVE &&
              v.dot == 0,
          "DOT emits two actual HP calls, including zero second hit");
    check(b.model().writes.size() == 10 && !b.world().seed.draws.empty() &&
              b.world().cosmetic.draws.empty(),
          "combat random draws retained while HP itself has no draws");
    B::Adapter reflection(1, 2, H::CallbackPolicy::ReferenceZero);
    auto &victim = reflection.create(V, creature());
    auto &attacker = reflection.create(A, creature());
    victim.counter12 = 1;
    attacker.kind = 0x10;
    std::vector<std::uint32_t> targets;
    reflection.set_before_base("observe", [&](B::Adapter &x, C::Obj &target,
                                              R::Raw, C::Obj *, std::uint32_t) {
      targets.push_back(x.binding(target).address);
    });
    reflection.damage(victim, &attacker, R::TWO, R::TWO, 100000, 0);
    check(targets == std::vector<std::uint32_t>({A, V}) &&
              victim.hp == 0x41000000 && attacker.hp == 0x41000000,
          "reflection performs nested attacker hit before victim hit");
    check(attacker.counter11 == 0x11,
          "reflection post-nested mutation retained");
  }
}
} // namespace
int main() {
  try {
    ownership_and_ranges();
    projection_and_actual_hits();
    raw_callbacks_and_partial_publish();
    source_routes();
    nested_raw_authority();
    dispatch_and_bounds();
    authority_and_refusals();
    selected_fpu_and_callback_replacement();
    damage_composition();
    std::cout << checks << " combat-creature checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "combat-creature check " << checks << ": " << e.what() << '\n';
    return 1;
  }
}
