// Authored Session ownership/phase/failure regressions, separate from original
// turning arithmetic and the independent Python complete-state corpus.
#include "champions/session.hpp"
#include "session_animation_trace.hpp"
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
namespace R = champions::simulation::rta;
namespace S = R::session;
namespace O = R::objects;
namespace H = R::creature_hp;
namespace {
unsigned checks = 0;
void check(bool ok, const char *why) {
  ++checks;
  if (!ok)
    throw std::runtime_error(why);
}
template <class E = std::exception, class F>
void rejects(F &&f, const char *why) {
  bool seen = false;
  try {
    f();
  } catch (const E &) {
    seen = true;
  }
  check(seen, why);
}
S::Config config(unsigned bits = 0) {
  S::Config c;
  c.network_mode = 1;
  c.peers = 1;
  c.local = 0;
  c.model = static_cast<R::Fpu::Model>(bits / 4);
  c.mul_bias = (bits & 2) != 0;
  c.div_nearest = (bits & 1) != 0;
  c.seed = 0xabcdef01;
  return c;
}
R::netwire::ByteVector sample() { return {0, 0, 128, 128, 128, 128, 0, 0}; }
std::uint64_t load(const S::Snapshot &s, std::uint32_t a, unsigned n) {
  std::uint64_t v = 0;
  for (unsigned i = 0; i < n; ++i) {
    auto it = s.hp->adapter.model.backing.find(a + i);
    if (it != s.hp->adapter.model.backing.end())
      v |= std::uint64_t(it->second) << (8 * i);
  }
  return v;
}
struct Fixture {
  S::Session session;
  std::vector<O::GameObject *> actors;
  std::vector<S::HpId> hp;
  unsigned runs = 0;
  Fixture(unsigned bits = 0, unsigned count = 2, S::HpLimits limits = {},
          bool frozen = false)
      : session(config(bits)) {
    S::ObjectBindings bindings;
    bindings.policy = O::CallbackPolicy::ReferenceBase;
    bindings.on_run = {"count_run", [this](auto &) { ++runs; }};
    if (frozen)
      bindings.is_frozen = {"frozen", [] { return true; }};
    session.enable_objects(17, std::move(bindings));
    session.with_world([&](auto &w) {
      for (unsigned i = 0; i < count; ++i) {
        auto *o = w.new_object(0x400, {0, 0, R::ONE});
        o->flags |= 0x20;
        o->angle = 0xc000;
        w.run_every_frame(*o);
        actors.push_back(o);
      }
    });
    session.enable_hp(limits);
    for (auto *o : actors)
      hp.push_back(session.bind_hp(o->serial, o->addr, {0x42c80000, 0, {}}));
  }
  void enable(S::TurnLimits limits = {}, R::Raw rate = R::ONE,
              std::uint8_t slow = 0) {
    session.enable_turning(limits);
    for (auto id : hp)
      check(session.bind_turner(id, {rate, 0, slow}) == id,
            "turn keeps explicit HP identity");
  }
  void task(S::Session::Task fn, std::int64_t priority = 20,
            std::uint32_t id = 0xf7000001) {
    session.register_object_task(id, "authored_turn", priority, 0,
                                 std::move(fn));
  }
  void prime() {
    check(session.submit(sample()).steps == 0,
          "first submission is not a ready tick");
  }
  void tick() { session.submit(sample()); }
};
void arithmetic_and_authority() {
  S::Snapshot retained;
  for (unsigned bits = 0; bits < 16; ++bits) {
    Fixture f(bits);
    f.enable();
    rejects(
        [&] {
          champions::test::session_animation_trace::observation(
              f.session.snapshot());
        },
        "animation observer refuses turning omission");
    auto a = f.actors[0]->addr, b = f.actors[1]->addr;
    const auto initial = f.session.snapshot();
    unsigned calls = 0;
    f.task([&](auto &s) {
      s.with_world(
          [&](auto &) { f.actors[0]->pos = {R::ONE, 0x40000000, 0x7fc12345}; });
      auto result =
          s.turn_toward(f.hp[0], 0, {0x40000000, 0x40000000, 0xffc12345});
      ++calls;
      if (calls == 1)
        check(result == 16384, "literal original pre-turn angle");
    });
    auto &ambient = R::set_fpu(R::Fpu::Model::Nearest, true, true);
    f.prime();
    check(f.session.snapshot().turning->operations.empty(),
          "no-ready input cannot turn");
    f.tick();
    auto after = f.session.snapshot();
    const auto &t = *after.turning;
    check(f.actors[0]->angle == 49605 && load(after, a + 0x110, 2) == 453,
          "literal original spring and facing");
    check(t.operations.size() == 1 && t.operations[0].completed &&
              t.operations[0].result == 16384,
          "accepted operation owns output");
    check(t.bindings[0].turned_this_tick &&
              t.bindings[0].calls_this_tick == 1 &&
              t.bindings[1].calls_this_tick == 0,
          "per-actor accounting");
    check(t.writes.size() == 2 && t.events.size() == 2 && t.calls.empty() &&
              t.draws == 0,
          "turn histories contain only original stores");
    check(t.writes[0].address == a + 0x110 && t.writes[1].address == a + 0x38,
          "ordered spring/facing");
    check(after.hp->adapter.model.stores.size() ==
              initial.hp->adapter.model.stores.size() + 2,
          "one shared store history");
    check(t.borrowed_memory && t.memory_owner == "hp.adapter.model.mem" &&
              t.callbacks.empty() && t.external.empty(),
          "exact borrowed callback-free ownership");
    check(load(after, a + 0x10, 4) == R::ONE &&
              load(after, a + 0x14, 4) == 0x40000000,
          "raw XY imports selected World");
    const auto &obj = after.hp->adapter.objects.at(0);
    check(obj.value.pos[0] == R::ONE && obj.value.pos[1] == 0x40000000 &&
              obj.value.pos[2] == R::ONE,
          "typed XY coherent, typed Z retained");
    check(load(after, a + 0x18, 4) == R::ONE &&
              f.actors[0]->pos[2] == 0x7fc12345,
          "raw and World Z remain independently unchanged");
    check(load(after, H::G_SIM_FRAME, 4) == load(initial, H::G_SIM_FRAME, 4),
          "turn leaves raw global frame");
    check(load(after, b + 0x38, 2) == load(initial, b + 0x38, 2) &&
              f.actors[1]->angle == 0xc000,
          "unselected actor untouched");
    check(t.phase == "idle" && t.depth == 0 && t.publication_error.empty(),
          "successful phase unwinds");
    check(ambient.model == R::Fpu::Model::Nearest && ambient.mul_bias &&
              ambient.div_nearest,
          "ambient FPU restored");
    retained = after;
    f.tick();
    auto second = f.session.snapshot();
    check(second.turning->operations.size() == 2 &&
              second.turning->bindings[0].calls_this_tick == 1,
          "next ready tick resets only counters");
    check(retained.turning->operations.size() == 1 &&
              retained.turning->bindings[0].last_result == 16384,
          "retained snapshot stays copied");
  }
}
void scheduling_and_limits() {
  {
    Fixture f;
    f.enable();
    f.task([&](auto &s) {
      for (auto id : f.hp)
        s.turn_toward(id, 0, {R::ONE, 0, 0});
    });
    f.prime();
    f.tick();
    auto s = f.session.snapshot();
    check(s.turning->operations.size() == 2 && s.turning->calls_this_tick == 2,
          "two actors require no singleton/terrain");
  }
  {
    Fixture f;
    f.enable();
    f.task([&](auto &s) {
      s.turn_toward(f.hp[0], 0, {R::ONE, 0, 0});
      s.turn_toward(f.hp[0], 0, {R::ONE, 0, 0});
    });
    f.prime();
    rejects([&] { f.tick(); }, "duplicate turn refuses");
    auto s = f.session.snapshot();
    check(s.failed && s.turning->operations.size() == 1 &&
              s.turning->operations[0].completed,
          "duplicate preserves completed prefix");
  }
  {
    Fixture f;
    f.enable({64, 1});
    f.task([&](auto &s) {
      s.turn_toward(f.hp[0], 0, {R::ONE, 0, 0});
      s.turn_toward(f.hp[1], 0, {R::ONE, 0, 0});
    });
    f.prime();
    rejects([&] { f.tick(); }, "aggregate budget refuses");
    auto s = f.session.snapshot();
    check(s.turning->bindings[1].calls_this_tick == 0 &&
              s.turning->operations.size() == 1,
          "second actor preflight has no accepted operation");
  }
  {
    Fixture f(0, 1, {}, true);
    f.enable();
    f.task([&](auto &s) { s.turn_toward(f.hp[0], 0, {R::ONE, 0, 0}); });
    f.prime();
    f.tick();
    check(f.runs == 0 && f.session.snapshot().turning->operations.size() == 1,
          "frozen object run does not suppress explicit later task");
  }
  for (auto priority : {9, 10}) {
    Fixture f(0, 1);
    f.enable();
    f.task([&](auto &s) { s.turn_toward(f.hp[0], 0, {R::ONE, 0, 0}); },
           priority);
    f.prime();
    rejects([&] { f.tick(); }, "turn requires post-object priority");
    check(f.session.snapshot().turning->operations.empty(),
          "wrong priority accepts no turn");
  }
}
void refusal_and_prefixes() {
  {
    Fixture f;
    rejects([&] { f.session.bind_turner(f.hp[0], {R::ONE, 0, 0}); },
            "turn enable required");
  }
  {
    Fixture f;
    f.enable();
    auto old = f.session.snapshot();
    rejects([&] { f.session.turn_toward(f.hp[0], 0, {R::ONE, 0, 0}); },
            "outside ready phase");
    auto s = f.session.snapshot();
    check(s.turning->operations.empty() &&
              s.hp->adapter.model.backing == old.hp->adapter.model.backing,
          "phase refusal preserves memory");
  }
  {
    Fixture f;
    f.enable();
    auto old = f.session.snapshot();
    rejects([&] { f.session.bind_turner(f.hp[0], {R::ONE, 0, 0}); },
            "duplicate binding");
    check(f.session.snapshot().turning->bindings.size() == 2 &&
              f.session.snapshot().hp->adapter.model.backing ==
                  old.hp->adapter.model.backing,
          "duplicate binding preserves setup");
  }
  {
    Fixture f;
    f.session.enable_turning({1, 64});
    f.session.bind_turner(f.hp[0], {R::ONE, 0, 0});
    auto old = f.session.snapshot();
    rejects([&] { f.session.bind_turner(f.hp[1], {R::ONE, 0, 0}); },
            "binding capacity");
    check(f.session.snapshot().hp->adapter.model.backing ==
              old.hp->adapter.model.backing,
          "capacity precedes writes");
  }
  {
    Fixture f;
    f.enable();
    auto old = f.session.snapshot();
    rejects([&] { f.session.seed_hp_backing(f.hp[0], {{0x38, 2, 7}}); },
            "facing authority cannot be shadowed");
    check(f.session.snapshot().hp->adapter.model.backing ==
              old.hp->adapter.model.backing,
          "protected setup keeps backing");
  }
  {
    Fixture f(0, 1);
    f.session.with_world([&](auto &) { f.actors[0]->angle = 65536; });
    f.session.enable_turning();
    rejects([&] { f.session.bind_turner(f.hp[0], {R::ONE, 0, 0}); },
            "wide facing unsupported");
    check(f.session.snapshot().turning->bindings.empty(),
          "wide facing binds nothing");
  }
  {
    Fixture f(0, 1);
    f.enable();
    f.prime();
    rejects([&] { f.session.enable_turning(); },
            "setup freezes on first submission");
  }
  {
    Fixture f;
    rejects([&] { f.session.enable_turning({0, 64}); }, "zero capacity");
  }
  {
    Fixture f;
    rejects([&] { f.session.enable_turning({65, 64}); },
            "hard binding capacity");
  }
  {
    Fixture f;
    rejects([&] { f.session.enable_turning({64, 65}); }, "hard call capacity");
  }
  for (unsigned capacity : {60u, 61u}) {
    S::HpLimits h;
    h.memory.memory_bytes = capacity;
    Fixture f(0, 1, h);
    f.session.enable_turning();
    const auto before = f.session.snapshot();
    check(before.hp->adapter.model.backing.size() == 52,
          "measured HP byte count");
    if (capacity == 60) {
      rejects([&] { f.session.bind_turner(f.hp[0], {R::ONE, 0, 0}); },
              "unique byte binding cap");
      auto after = f.session.snapshot();
      check(after.hp->adapter.model.backing ==
                    before.hp->adapter.model.backing &&
                after.turning->bindings.empty() &&
                after.hp->adapter.model.stores.empty(),
            "binding capacity refusal leaves shared state unchanged");
    } else {
      f.session.bind_turner(f.hp[0], {R::ONE, 0, 0});
      check(f.session.snapshot().hp->adapter.model.backing.size() == 61,
            "exact unique-byte binding capacity accepts");
    }
  }
  {
    S::HpLimits h;
    h.memory.history_entries = 5;
    Fixture f(0, 1, h);
    f.enable();
    f.task([&](auto &s) {
      s.take_damage(f.hp[0], R::ONE, {}, 0);
      s.turn_toward(f.hp[0], 0, {R::ONE, 0, 0});
    });
    f.prime();
    rejects([&] { f.tick(); }, "shared HP memory store cap");
    auto state = f.session.snapshot();
    const auto &turn = *state.turning;
    check(state.hp->adapter.model.stores.size() == 5 &&
              turn.writes.size() == 1 && turn.events.size() == 1 &&
              turn.operations.size() == 1 && !turn.operations[0].completed &&
              !turn.operations[0].result,
          "shared store refusal retains exact attempted prefix once");
    check(load(state, f.actors[0]->addr + 0x110, 2) == 0 &&
              f.actors[0]->angle == 0xc000 && turn.depth == 0 && state.failed,
          "lower-layer refusal retains facing and unwinds");
  }
  {
    S::HpLimits h;
    h.memory.history_entries = 1;
    Fixture f(0, 1, h);
    f.enable({}, 0);
    f.task([&](auto &s) { s.turn_toward(f.hp[0], 0, {R::ONE, 0, 0}); });
    f.prime();
    rejects([&] { f.tick(); }, "original write-budget boundary");
    auto s = f.session.snapshot();
    auto &t = *s.turning;
    check(t.writes.size() == 1 && t.events.size() == 1 &&
              t.operations.size() == 1 && !t.operations[0].completed &&
              !t.operations[0].result,
          "incomplete body prefix recorded");
    check(load(s, f.actors[0]->addr + 0x110, 2) == 453 &&
              f.actors[0]->angle == 0xc000,
          "escaped spring, retained facing");
    check(t.depth == 0 && s.failed, "failure unwinds and latches");
    auto old = t.operations.size();
    rejects([&] { f.tick(); }, "failed Session cannot retry");
    check(f.session.snapshot().turning->operations.size() == old,
          "no retry operations");
  }
  {
    Fixture f(0, 1);
    f.enable();
    f.task([&](auto &s) {
      s.turn_toward(f.hp[0], 0, {R::ONE, 0, 0});
      throw std::runtime_error("authored later failure");
    });
    f.prime();
    rejects([&] { f.tick(); }, "later consumer failure");
    check(f.session.snapshot().turning->operations[0].completed &&
              f.actors[0]->angle == 49605,
          "later failure retains completed turn");
  }
}
} // namespace
int main() {
  try {
    arithmetic_and_authority();
    scheduling_and_limits();
    refusal_and_prefixes();
    std::cout << checks << " Session turn checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Session turn test failure: " << e.what() << "\n";
    return 1;
  }
}
