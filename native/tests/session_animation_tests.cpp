// Authored Session-boundary regressions. These test the declared scheduler and
// ownership policy; they do not assert original game animation ordering.
#include "champions/session.hpp"
#include <array>
#include <functional>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace R = champions::simulation::rta;
namespace S = R::session;
namespace O = R::objects;
namespace A = R::animation_clock;
namespace {
unsigned checks = 0;
void check(bool ok, const char *why) {
  ++checks;
  if (!ok)
    throw std::runtime_error(why);
}
template <class E = S::Error, class F> void rejects(F &&fn, const char *why) {
  bool caught = false;
  try {
    fn();
  } catch (const E &) {
    caught = true;
  }
  check(caught, why);
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
A::Clip clip(std::int32_t length = 100) {
  A::Clip c;
  c.length = length;
  return c;
}
struct Fixture {
  S::Session session;
  std::vector<O::GameObject *> objects;
  std::vector<S::HpId> hp;
  unsigned runs = 0;
  explicit Fixture(unsigned bits = 0, unsigned actors = 1,
                   S::ObjectBindings bindings = {})
      : session(config(bits)) {
    bindings.policy = O::CallbackPolicy::ReferenceBase;
    if (!bindings.on_run.function)
      bindings.on_run = {"count_run", [this](auto &) { ++runs; }};
    session.enable_objects(17, std::move(bindings));
    session.with_world([&](auto &w) {
      for (unsigned i = 0; i < actors; ++i) {
        auto *o = w.new_object(0x400, {});
        o->flags |= 0x20;
        w.run_every_frame(*o);
        objects.push_back(o);
      }
    });
    session.enable_hp();
    for (auto *o : objects)
      hp.push_back(session.bind_hp(o->serial, o->addr, {0x42c80000, 0, {}}));
  }
  void task(S::Session::Task fn, std::int64_t priority = 20,
            std::uint32_t id = 0xf5000001) {
    session.register_object_task(id, "authored_animation", priority, 0,
                                 std::move(fn));
  }
  void prime() {
    check(session.submit(sample()).steps == 0,
          "one-peer initial submission has no ready tick");
  }
  void tick() { session.submit(sample()); }
};
const S::AnimationBindingSnapshot &binding(const S::Snapshot &s,
                                           std::size_t i = 0) {
  return s.animation->bindings.at(i);
}
void enable(Fixture &f, S::AnimationLimits limits = {},
            std::int32_t capacity = 3) {
  f.session.enable_animation(limits);
  for (auto id : f.hp)
    check(f.session.bind_animation(id, capacity) == id,
          "animation preserves explicit HP identity");
}
void ready_arithmetic_and_ownership() {
  S::Snapshot retained;
  for (unsigned cfg = 0; cfg < 16; ++cfg) {
    Fixture f(cfg);
    enable(f);
    auto authored = clip();
    authored.joint_mask = 0xffffffffffffffffULL;
    authored.events[0] = {0, 128};
    const auto id = f.session.add_animation_clip(authored);
    authored.length = 1;
    authored.events[0].mask = 0;
    unsigned calls = 0;
    f.task([&](S::Session &s) {
      if (calls++ == 0)
        check(s.start_animation(f.hp[0], id) == std::optional<std::int32_t>(0),
              "start returns owned cell index");
      const auto result = s.advance_animation(f.hp[0], 0x3f000001);
      check(result == (calls == 1 ? -128 : 0),
            "event byte is signed and crossing is not repeated");
    });
    const auto initial = f.session.snapshot();
    auto &ambient = R::set_fpu(R::Fpu::Model::Nearest, !(cfg & 2), !(cfg & 1));
    f.prime();
    auto primed = f.session.snapshot();
    check(primed.animation->operations.empty() && binding(primed).count == 0,
          "non-ready submission cannot start or advance a clock");
    f.tick();
    auto first = f.session.snapshot();
    R::Fpu fp(static_cast<R::Fpu::Model>(cfg / 4), (cfg & 2) != 0,
              (cfg & 1) != 0);
    const auto step = fp.mul(0x3f000001, A::K_STEP);
    check(binding(first).tracks[0].time == step &&
              binding(first).last_event == -128,
          "selected raw arithmetic and signed event are observable");
    check(
        first.animation->clips[0].value.length == 100 &&
            first.animation->clips[0].value.joint_mask == 0xffffffffffffffffULL,
        "clip registry copies caller metadata and preserves unsigned64 masks");
    check(first.animation->calls_this_tick == 2 &&
              binding(first).calls_this_tick == 2 &&
              binding(first).advanced_this_tick,
          "accepted lifecycle and advance counters are distinct from ready "
          "ticks");
    check(first.animation->phase == "idle" && first.animation->depth == 0 &&
              first.animation->object_run_completed &&
              !first.animation->task_priority,
          "quiescent snapshot retains completion but no active task priority");
    check(first.animation->system.sim_frame ==
              static_cast<std::uint32_t>(first.world->frame),
          "system frame comes from the actual ready World frame");
    check(first.animation->system.cosmetic_seed == 0 &&
              first.seed == initial.seed && first.draws.empty() &&
              first.world->shared_seed == initial.world->shared_seed &&
              first.network.rand_seed == initial.network.rand_seed,
          "clock operations draw from neither object/network nor "
          "combat/cosmetic streams");
    check(first.hp->adapter.objects[0].value.hp == 0x42c80000,
          "event result alone does not invent an HP consumer");
    check(ambient.model == R::Fpu::Model::Nearest &&
              ambient.mul_bias == !(cfg & 2) &&
              ambient.div_nearest == !(cfg & 1),
          "selected Session FPU restores caller context");
    retained = first;
    first.animation->clips[0].value.events[0].mask = 17;
    first.animation->bindings[0].tracks[0].time = 0;
    f.tick();
    const auto second = f.session.snapshot();
    check(binding(second).tracks[0].time == fp.add(step, step) &&
              binding(second).last_event == 0,
          "second actual ready frame advances persistent owned clock once");
    check(second.animation->calls_this_tick == 1 &&
              binding(second).calls_this_tick == 1 &&
              second.animation->clips[0].value.events[0].mask == 128,
          "per-tick counters reset and copied snapshot mutation is isolated");
    check(binding(retained).tracks[0].time == step &&
              retained.animation->operations.size() == 2,
          "prior snapshots never alias later runtime mutation");
  }
  check(retained.animation->clips[0].value.length == 100 &&
            binding(retained).tracks[0].clip == 1,
        "value snapshot remains owned after Session destruction");
}
void aliases_and_stale_cells() {
  Fixture f(0, 2);
  enable(f);
  auto a = clip();
  const auto one = f.session.add_animation_clip(a),
             two = f.session.add_animation_clip(a);
  std::optional<S::Snapshot> before_stop;
  f.task([&](auto &s) {
    s.start_animation(f.hp[0], one, 0, 0x10020);
    s.start_animation(f.hp[0], one, 0, 0x10020);
    s.start_animation(f.hp[1], two);
    s.set_animation_event(one, 0, {0, 255});
    check(s.advance_animation(f.hp[0]) == -1,
          "shared clip event mutation reaches every referencing track");
    check(s.advance_animation(f.hp[1]) == 0,
          "equal-content independent clip retains separate identity");
    before_stop = s.snapshot();
    s.stop_animation(f.hp[0], one);
  });
  f.prime();
  f.tick();
  const auto final = f.session.snapshot();
  check(binding(*before_stop).tracks[0].flags == 0x20 &&
            binding(*before_stop).tracks[1].flags == 0x20,
        "raw32 input flags undergo only original start normalization");
  check(before_stop->animation->operations[0].flags == 0x10020,
        "operation observation retains supplied raw32 flags");
  check(binding(final).count == 0 && binding(final).tracks.size() == 3 &&
            binding(final).tracks[0].clip == one &&
            binding(final).tracks[1].clip == one,
        "stop retains all stale capacity cells and their clip identities");
  check(binding(final, 1).count == 1 && binding(final, 1).tracks[0].clip == two,
        "other binding remains independent after alias removal");
  check(before_stop->animation->task_priority == 20 &&
            before_stop->animation->object_run_completed &&
            before_stop->animation->depth == 0,
        "supported scheduler observation has exact priority and completed "
        "runObjects guard");
}
void setup_refusals() {
  {
    S::Session s(config());
    rejects([&] { s.enable_animation(); },
            "animation requires an existing World and HP authority");
    check(!s.snapshot().animation,
          "failed enable does not publish partial module");
  }
  for (unsigned field = 0; field < 5; ++field) {
    Fixture f;
    S::AnimationLimits limits;
    if (field == 0)
      limits.clips = S::MAX_ANIMATION_CLIPS + 1;
    if (field == 1)
      limits.bindings = S::MAX_ANIMATION_BINDINGS + 1;
    if (field == 2)
      limits.cells = S::MAX_ANIMATION_CELLS + 1;
    if (field == 3)
      limits.calls_per_tick = S::MAX_ANIMATION_CALLS + 1;
    if (field == 4)
      limits.clock.max_loop_subtractions =
          S::MAX_ANIMATION_LOOP_SUBTRACTIONS + 1;
    rejects([&] { f.session.enable_animation(limits); },
            "oversized module limit refuses before publication");
    check(!f.session.snapshot().animation,
          "invalid limits leave module absent");
  }
  for (int cap : {0, 128, -1}) {
    Fixture f;
    f.session.enable_animation();
    rejects<A::ValueError>([&] { f.session.bind_animation(f.hp[0], cap); },
                           "invalid track capacity refuses");
    check(f.session.snapshot().animation->bindings.empty() &&
              f.session.snapshot().animation->total_cells == 0,
          "invalid capacity has no binding/cell prefix");
  }
  {
    Fixture f;
    f.session.enable_animation();
    rejects([&] { f.session.bind_animation(999); },
            "unknown HP identity is not guessed");
    check(f.session.snapshot().animation->bindings.empty(),
          "unknown binding publishes no record");
  }
  {
    Fixture f;
    enable(f);
    rejects([&] { f.session.bind_animation(f.hp[0]); },
            "duplicate animation binding refuses");
    check(f.session.snapshot().animation->bindings.size() == 1,
          "duplicate binding preserves original record");
  }
  {
    Fixture f;
    f.session.enable_animation();
    auto c = clip();
    c.has_sound_table = true;
    const auto id = f.session.add_animation_clip(c);
    check(id == 1 &&
              f.session.snapshot().animation->clips[0].value.has_sound_table,
          "sound-table presence is preserved as metadata without inventing "
          "execution");
  }
  {
    Fixture f(0, 2);
    S::AnimationLimits cap;
    cap.cells = 3;
    f.session.enable_animation(cap);
    f.session.bind_animation(f.hp[0], 3);
    rejects([&] { f.session.bind_animation(f.hp[1], 1); },
            "aggregate capacity is checked before second binding");
    const auto s = f.session.snapshot();
    check(s.animation->bindings.size() == 1 && s.animation->total_cells == 3,
          "failed aggregate reservation preserves prior cells");
  }
  for (unsigned op = 0; op < 3; ++op) {
    Fixture f;
    enable(f);
    f.task([](auto &) {});
    f.prime();
    rejects(
        [&] {
          if (op == 0)
            f.session.add_animation_clip(clip());
          else if (op == 1)
            f.session.bind_animation(f.hp[0]);
          else
            f.session.enable_animation();
        },
        "setup freezes on first submission even before ready work");
    check(f.session.snapshot().animation->operations.empty(),
          "setup freeze refusal invents no runtime operation");
  }
}
void runtime_gates_and_failure_latch() {
  {
    Fixture f;
    enable(f);
    const auto c = f.session.add_animation_clip(clip());
    rejects([&] { f.session.start_animation(f.hp[0], c); },
            "animation cannot start outside an actual ready scheduler task");
    const auto s = f.session.snapshot();
    check(s.failed && s.animation->operations.empty() &&
              s.animation->calls_this_tick == 0,
          "outside-phase refusal latches without animation prefix");
  }
  for (unsigned mode = 0; mode < 3; ++mode) {
    Fixture f;
    enable(f);
    const auto c = f.session.add_animation_clip(clip());
    f.task(
        [&](auto &s) {
          if (mode == 2)
            s.with_world([&](auto &) { s.start_animation(f.hp[0], c); });
          else
            s.start_animation(f.hp[0], c);
        },
        mode == 0   ? 5
        : mode == 1 ? 10
                    : 20);
    f.prime();
    rejects(
        [&] { f.tick(); },
        "only quiescent post-runObjects priority above10 may invoke animation");
    check(f.session.snapshot().animation->operations.empty(),
          "phase preflight precedes operation publication");
  }
  {
    Fixture *active = nullptr;
    S::ObjectBindings b;
    b.on_run = {"invalid_animation_in_run", [&](auto &) {
                  active->session.advance_animation(active->hp[0]);
                }};
    Fixture f(0, 1, b);
    active = &f;
    enable(f);
    f.task([](auto &) {});
    f.prime();
    rejects([&] { f.tick(); },
            "object callback does not receive invented animation ordering");
    const auto s = f.session.snapshot();
    check(!s.animation->object_run_completed && s.animation->operations.empty(),
          "failed runObjects retains false completion and no animation writes");
  }
  for (unsigned mode = 0; mode < 7; ++mode) {
    Fixture f;
    enable(f);
    const auto c = f.session.add_animation_clip(clip());
    f.task([&](auto &s) {
      if (mode < 2)
        s.start_animation(f.hp[0], c, 0, mode == 0 ? A::ROOT : A::YAW);
      else if (mode == 2)
        s.start_animation(f.hp[0], 999);
      else if (mode == 3)
        s.advance_animation(999);
      else if (mode == 4)
        s.set_animation_event(c, 4, {0, 1});
      else if (mode == 5)
        s.set_animation_event(0, 0, {0, 1});
      else
        s.set_animation_event(999, 0, {0, 1});
    });
    f.prime();
    if (mode < 2)
      rejects<A::Unsupported>(
          [&] { f.tick(); },
          "ROOT/YAW service requests refuse before acceptance");
    else
      rejects([&] { f.tick(); }, "unknown identity or malformed event refuses");
    const auto failed = f.session.snapshot();
    check(
        failed.failed && failed.animation->operations.empty() &&
            failed.animation->calls_this_tick == 0,
        "all service preflights precede clock mutation and accepted counters");
    rejects([&] { f.tick(); }, "failed Session rejects subsequent submissions");
    check(f.session.snapshot().animation->operations.empty(),
          "latched retry adds no animation operation");
  }
  {
    Fixture f;
    enable(f);
    f.task([&](auto &s) {
      check(!s.start_animation(f.hp[0], 0),
            "null clip start returns original null result");
      s.stop_animation(f.hp[0], 0);
    });
    f.prime();
    f.tick();
    const auto s = f.session.snapshot();
    check(binding(s).count == 0 && s.animation->operations.size() == 2 &&
              s.animation->operations[0].completed &&
              s.animation->operations[1].completed,
          "null clip lifecycle no-ops remain accepted observable calls");
  }
  {
    Fixture f;
    S::AnimationLimits cap;
    cap.calls_per_tick = 1;
    enable(f, cap);
    const auto c = f.session.add_animation_clip(clip());
    f.task([&](auto &s) {
      s.start_animation(f.hp[0], c);
      s.set_animation_event(c, 0, {0, 1});
    });
    f.prime();
    rejects<S::ResourceLimit>(
        [&] { f.tick(); },
        "all runtime methods share one aggregate call budget");
    const auto s = f.session.snapshot();
    check(binding(s).count == 1 && s.animation->operations.size() == 1 &&
              s.animation->calls_this_tick == 1 &&
              s.animation->clips[0].value.events[0].mask == 0,
          "budget refusal retains accepted first operation and untouched "
          "rejected event");
  }
  {
    Fixture f;
    enable(f);
    const auto c = f.session.add_animation_clip(clip());
    f.task([&](auto &s) {
      s.start_animation(f.hp[0], c);
      s.advance_animation(f.hp[0]);
      s.advance_animation(f.hp[0]);
    });
    f.prime();
    rejects<S::ResourceLimit>(
        [&] { f.tick(); },
        "binding advances at most once per actual ready frame");
    const auto s = f.session.snapshot();
    check(binding(s).tracks[0].time == A::K_STEP &&
              binding(s).last_event == 0 && s.animation->operations.size() == 2,
          "second advance refusal retains first clock result without retry");
  }
}
void ready_and_freeze_distinctions() {
  {
    Fixture f;
    enable(f);
    const auto c = f.session.add_animation_clip(clip());
    f.task([&](auto &s) {
      s.start_animation(f.hp[0], c);
      s.advance_animation(f.hp[0]);
    });
    f.session.submit(sample(), 0, 0, 1, true);
    f.session.set_level_pending(true);
    f.session.submit(sample());
    check(f.session.snapshot().animation->operations.empty(),
          "no_sim and pending frames never run authored animation task");
  }
  {
    S::ObjectBindings b;
    b.is_frozen = {"frozen", [] { return true; }};
    Fixture f(0, 1, b);
    enable(f);
    const auto c = f.session.add_animation_clip(clip());
    f.task([&](auto &s) {
      s.start_animation(f.hp[0], c);
      s.advance_animation(f.hp[0]);
    });
    f.prime();
    f.tick();
    const auto s = f.session.snapshot();
    check(f.runs == 0 && binding(s).tracks[0].time == A::K_STEP,
          "frozen object-run callback does not silently freeze later authored "
          "scheduler task");
  }
  {
    Fixture f;
    enable(f);
    auto metadata = clip();
    metadata.events[0] = {0, 128};
    const auto c = f.session.add_animation_clip(metadata);
    f.task([&](auto &s) {
      s.start_animation(f.hp[0], c, 0, A::PAUSED);
      check(s.advance_animation(f.hp[0]) == -128,
            "paused track still exposes original event-window result");
    });
    f.prime();
    f.tick();
    const auto s = f.session.snapshot();
    check(f.runs == 1 && binding(s).tracks[0].time == 0,
          "track PAUSED flag is distinct from World frozen policy");
  }
}
void identity_revalidation() {
  for (unsigned mode = 0; mode < 4; ++mode) {
    Fixture f(0, 2);
    enable(f);
    const auto c = f.session.add_animation_clip(clip());
    f.task([&](auto &s) {
      s.with_world([&](auto &w) {
        if (mode == 3)
          w.frame = -1;
        else {
          auto *old = f.objects[mode == 2 ? 1 : 0];
          const auto address = old->addr;
          w.destroy(*old);
          if (mode != 0) {
            auto *replacement = w.new_object(0x400, {});
            check(replacement->addr == address &&
                      replacement->serial != old->serial,
                  "reuse test creates distinct live identity at the same "
                  "address");
          }
        }
      });
      s.start_animation(f.hp[0], c);
    });
    f.prime();
    rejects([&] { f.tick(); }, "selected retirement, any reused HP address and "
                               "invalid frame refuse before projection");
    const auto s = f.session.snapshot();
    check(s.animation->operations.empty() && binding(s).count == 0,
          "identity and frame guards retain untouched animation state");
  }
}
void aggregate_history_reserve() {
  Fixture f;
  enable(f);
  // Real public draws fill retained history before submission; no private
  // counters or state mutation are used to manufacture the cap boundary.
  for (unsigned i = 0; i < 31720; ++i)
    f.session.draw();
  unsigned completed = 0;
  f.task([&](auto &s) {
    for (unsigned i = 0; i < 64; ++i) {
      s.start_animation(f.hp[0], 0);
      ++completed;
    }
  });
  f.prime();
  rejects<S::ResourceLimit>(
      [&] { f.tick(); },
      "animation respects aggregate Session history reserve");
  const auto state = f.session.snapshot();
  check(completed > 0 && completed < 64 &&
            state.animation->operations.size() == completed &&
            state.animation->calls_this_tick == completed,
        "history exhaustion preserves completed calls and a readable full "
        "snapshot");
  check(state.animation->operations.back().completed &&
            state.animation->phase == "idle",
        "history refusal occurs before accepting the next operation");
  rejects([&] { f.tick(); }, "history failure forbids replay");
  check(f.session.snapshot().animation->operations.size() == completed,
        "latched history retry appends no operations");
}
void original_prefix_and_explicit_consumer() {
  {
    Fixture f;
    S::AnimationLimits limit;
    limit.clock.max_loop_subtractions = 1;
    enable(f, limit);
    const auto c = f.session.add_animation_clip(clip(1));
    f.task([&](auto &s) {
      s.start_animation(f.hp[0], c, 0, A::LOOP);
      s.advance_animation(f.hp[0], 0x40800000);
    });
    f.prime();
    auto &ambient = R::set_fpu(R::Fpu::Model::Nearest, true, true);
    rejects<A::ResourceLimit>([&] { f.tick(); },
                              "original LOOP exhaustion propagates after its "
                              "allowed subtraction prefix");
    const auto s = f.session.snapshot();
    R::Fpu fp;
    check(binding(s).tracks[0].time ==
                  fp.sub(fp.mul(0x40800000, A::K_STEP), R::ONE) &&
              binding(s).flags == A::LOOP,
          "failed loop retains completed time subtraction and old state flags");
    check(s.animation->phase == "advance_animation" &&
              s.animation->depth == 0 && binding(s).advanced_this_tick &&
              !binding(s).last_event,
          "failed accepted advance remains distinguishable from completion");
    check(s.animation->operations.size() == 2 &&
              s.animation->operations[0].completed &&
              !s.animation->operations[1].completed,
          "operation log preserves accepted incomplete prefix");
    check(ambient.model == R::Fpu::Model::Nearest && ambient.mul_bias &&
              ambient.div_nearest,
          "exception restores the caller arithmetic context");
    rejects([&] { f.tick(); }, "LOOP failure latches and cannot be retried");
    check(f.session.snapshot().animation->operations.size() == 2,
          "retry does not consume additional clock work");
  }
  {
    Fixture f(0, 2);
    enable(f);
    auto metadata = clip();
    metadata.events[0] = {0, 1};
    const auto c = f.session.add_animation_clip(metadata);
    f.task([&](auto &s) {
      s.with_world([](auto &w) {
        const auto count = w.handles.count_free();
        for (std::size_t i = 1; i < count; ++i)
          w.handles.add(w.always_draw, nullptr);
      });
      s.start_animation(f.hp[0], c);
      if (s.advance_animation(f.hp[0]) & 1)
        s.take_damage(f.hp[0], R::ONE, f.hp[1]);
    });
    f.prime();
    rejects<R::HandleError>([&] { f.tick(); },
                            "explicit HP consumer uses actual pool and "
                            "preserves second-allocation failure");
    const auto s = f.session.snapshot();
    check(binding(s).last_event == 1 &&
              binding(s).tracks[0].time == A::K_STEP &&
              s.animation->operations.back().completed,
          "completed clock and event survive failure in an explicit later "
          "consumer");
    check(!s.hp->adapter.model.writes.empty() &&
              s.hp->adapter.objects[0].value.hp == 0x42c80000 &&
              s.world->handle_free == 0,
          "actual HP write and first allocation prefix survives without "
          "invented final subtraction");
    check(s.failed && s.failure_operation == "take_damage",
          "first failing consumer names the Session latch");
    rejects([&] { f.tick(); },
            "failed event consumer cannot repeat same ready frame");
    check(f.session.snapshot().hp->adapter.model.writes.size() ==
              s.hp->adapter.model.writes.size(),
          "latched replay adds no HP writes");
  }
}
} // namespace
int main() {
  try {
    ready_arithmetic_and_ownership();
    aliases_and_stale_cells();
    setup_refusals();
    runtime_gates_and_failure_latch();
    ready_and_freeze_distinctions();
    identity_revalidation();
    aggregate_history_reserve();
    original_prefix_and_explicit_consumer();
    std::cout << "Session animation API checks: " << checks
              << "; failures: 0\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Session animation API failed after " << checks
              << " checks: " << e.what() << '\n';
    return 1;
  }
}
