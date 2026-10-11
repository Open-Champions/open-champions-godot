#include "champions/session.hpp"
#include <algorithm>
#include <array>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace R = champions::simulation::rta;
namespace S = R::session;
namespace O = R::objects;
namespace W = R::world_queries;
namespace M = R::creature_movement;
namespace N = R::netwire;
namespace {
unsigned checks = 0;
constexpr std::uint32_t TABLE = 0x100000;
void check(bool value, const char *why) {
  ++checks;
  if (!value)
    throw std::runtime_error(why);
}
template <class E = S::Error, class F> void rejects(F fn, const char *why) {
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
  c.mul_bias = bits & 2;
  c.div_nearest = bits & 1;
  c.seed = 0xabcdef01;
  return c;
}
W::LevelData floor_data(bool wall = false) {
  W::LevelData d;
  d.tg_x0 = d.tg_y0 = -2;
  d.tg_w = d.tg_h = 4;
  d.grid.assign(16, {0});
  W::HeightField h;
  h.x0 = h.y0 = -64;
  h.w = h.h = 129;
  h.samples.assign(129 * 129, 0);
  if (wall) {
    // Authored flag wall starts at world x=8; same owned terrain supplies both
    // sweep and ground. Element flag4 keeps low-height flag samples in the
    // original sweep candidate set. No fabricated hit answer.
    for (unsigned y = 0; y < 129; ++y)
      for (unsigned x = 66; x < 129; ++x)
        h.samples[y * 129 + x] = 1;
  }
  d.heightfields.push_back(std::move(h));
  d.elements.push_back(W::make_element(0, -256, -256, 256, 256, 0, wall ? 4 : 0,
                                       0, 0, 0, 32767, 0));
  return d;
}
N::ByteVector sample() { return {0, 0, 128, 128, 128, 128, 0, 0}; }
struct Fixture {
  S::Session session;
  O::GameObject *object = nullptr;
  S::HpId hp = 0;
  unsigned runs = 0;
  explicit Fixture(unsigned bits = 0, S::CreatureInitial initial = {},
                   bool bind = true, W::Limits level_limits = {},
                   S::HpLimits hp_limits = {}, bool wall = false,
                   std::uint64_t cflags = 0x100000,
                   S::ObjectBindings bindings = {})
      : session(config(bits)) {
    bindings.policy = O::CallbackPolicy::ReferenceBase;
    if (!bindings.on_run.function)
      bindings.on_run = {"count_run", [this](auto &) { ++runs; }};
    session.enable_objects(17, std::move(bindings));
    session.with_world([&](O::World &world) {
      object = world.new_object(0x400, {});
      object->flags |= 0x20;
      object->radius = 0x40800000;
      object->height = 0x41000000;
      world.run_every_frame(*object);
    });
    session.enable_hp(hp_limits);
    hp =
        session.bind_hp(object->serial, object->addr, {0x42c80000, cflags, {}});
    session.enable_terrain(floor_data(wall), level_limits);
    session.enable_creature_movement();
    if (!initial.base_table)
      initial.base_table = TABLE;
    if (bind)
      session.bind_creature_mover(hp, initial);
  }
  void task(std::function<void(S::Session &)> fn, std::int64_t priority = 20,
            std::uint32_t id = 0xf4000001) {
    session.register_object_task(id, "authored_creature", priority, 0,
                                 std::move(fn));
  }
  void move(W::Vec3 velocity = {}) {
    task([this, velocity](S::Session &s) { s.move_creature(hp, velocity); });
  }
  void prime() {
    check(session.submit(sample()).steps == 0,
          "actual one-peer machine has an initial non-ready submission");
  }
  void tick() { session.submit(sample()); }
};
const O::ObjectSnapshot &actor(const S::Snapshot &s) {
  return s.world->objects.front();
}
std::uint64_t word(const S::Snapshot &s, std::uint32_t address,
                   unsigned width = 4) {
  const auto &bytes = s.hp->adapter.model.backing;
  std::uint64_t value = 0;
  for (unsigned i = 0; i < width; ++i) {
    const auto at = bytes.find(address + i);
    if (at != bytes.end())
      value |= std::uint64_t(at->second) << (8 * i);
  }
  return value;
}
std::vector<std::string> calls(const S::Snapshot &s) {
  std::vector<std::string> out;
  for (const auto &c : s.creature_movement->calls)
    out.push_back(c.name);
  return out;
}
void coherent(const S::Snapshot &s) {
  const auto &a = actor(s);
  const auto &h = s.hp->adapter.objects.front().value;
  for (unsigned i = 0; i < 3; ++i)
    check(a.pos[i] == h.pos[i] && word(s, a.addr + 0x10 + i * 4) == a.pos[i],
          "World, typed HP and raw HP share current position");
  check(word(s, a.addr + 0x30) == a.vz &&
            word(s, a.addr + 0x38, 2) == a.angle &&
            word(s, a.addr + 0x1c) == a.radius &&
            word(s, a.addr + 0x20) == a.height,
        "movement aliases retain velocity, facing and dimensions");
}
void arithmetic_and_owned_state() {
  for (unsigned cfg = 0; cfg < 16; ++cfg) {
    Fixture f(cfg);
    f.move({0x42700000, 0, 0});
    const auto initial = f.session.snapshot();
    f.prime();
    check(f.session.snapshot().creature_movement->writes.empty(),
          "no ready frame means no movement writes");
    f.tick();
    auto first = f.session.snapshot();
    R::Fpu fp(static_cast<R::Fpu::Model>(cfg / 4), cfg & 2, cfg & 1);
    check(actor(first).pos[0] == fp.mul(0x42700000, 0x3c888888) &&
              actor(first).pos[2] == 0 && actor(first).vz == 0,
          "original creature step uses selected arithmetic and lands once");
    check(first.creature_movement->last_bits ==
                  std::optional<std::uint32_t>(4) &&
              first.creature_movement->calls_this_tick == 1 && f.runs == 1,
          "one accepted movement follows actual runObjects");
    check(first.creature_movement->borrowed_memory &&
              first.creature_movement->memory_owner == "hp.adapter.model.mem" &&
              first.creature_movement->callback_policy ==
                  M::CallbackPolicy::RequireHandlers,
          "snapshot identifies real borrowed HP storage and strict services");
    check(calls(first) == std::vector<std::string>{"world_slideMove",
                                                   "obj_updateGridCell",
                                                   "world_collectObjectsInBox",
                                                   "terrain_heightBelow"},
          "ready creature uses actual slide, grid, collection, ground order");
    const auto &slide = first.creature_movement->callback_results.front().value;
    check(slide.kind == M::ValueKind::Tuple && slide.items.size() == 2 &&
              slide.items[0].scalar == 0 &&
              slide.items[1].kind == M::ValueKind::None,
          "real clear terrain preserves no-hit None normal");
    check(first.world->shared_seed == initial.world->shared_seed &&
              actor(first).seed == actor(initial).seed &&
              first.seed == initial.seed && first.draws.empty() &&
              first.hp->adapter.world.seed.draws.empty() &&
              first.hp->adapter.world.cosmetic.draws.empty(),
          "dry movement draws no object, shared, legacy combat or cosmetic "
          "randomness");
    coherent(first);
    f.tick();
    auto second = f.session.snapshot();
    check(actor(second).pos[0] != actor(first).pos[0] &&
              actor(initial).pos[0] == 0 &&
              first.creature_movement->calls_this_tick == 1,
          "successive ticks advance persistent state and old snapshots remain "
          "owned");
    first.hp->adapter.objects[0].value.cflags = 0;
    check(
        f.session.snapshot().hp->adapter.objects[0].value.cflags == 0x100000,
        "mutating a copied cflags snapshot never enables live water behavior");
  }
}
void roots_timer_inert_and_gravity() {
  for (unsigned which = 0; which < 4; ++which) {
    S::CreatureInitial initial;
    initial.root = {R::ONE, R::MINUS_ONE};
    initial.root_yaw = 5;
    initial.root_timer = which == 1 ? 0xffff : 0;
    initial.inert = which == 2 ? 8 : 0;
    Fixture f(0, initial);
    f.session.with_world([&](auto &) {
      f.object->angle = 0xfffe;
      if (which == 3)
        f.object->pos[2] = 0x42c80000;
    });
    f.move({});
    f.prime();
    f.tick();
    const auto s = f.session.snapshot();
    const auto a = f.object->addr;
    if (which == 2) {
      check(
          s.creature_movement->writes.empty() &&
              s.creature_movement->calls.empty() &&
              s.creature_movement->last_bits == std::optional<std::uint32_t>(0),
          "hidden actor accepts the call but original body leaves no effects");
      check(actor(s).angle == 0xfffe && word(s, a + 0xbc) == R::ONE &&
                word(s, a + 0xc4, 2) == 5,
            "hidden movement does not consume root delta or yaw");
    } else {
      check(actor(s).angle == 3 && word(s, a + 0xbc, 8) == 0 &&
                word(s, a + 0xc4, 2) == 0,
            "root yaw wraps and root translation is consumed once");
      check((actor(s).pos[0] == 0) == (which == 1),
            "signed nonzero root timer suppresses movement but not yaw "
            "consumption");
      if (which == 3) {
        R::Fpu fp;
        check(actor(s).vz == 0xc10fffff &&
                  actor(s).pos[2] ==
                      fp.add(0x42c80000, fp.mul(0xc10fffff, 0x3c888888)),
              "airborne actor gets exactly one original gravity step");
      }
    }
    coherent(s);
  }
}
void callback_threshold_and_grid_failures() {
  for (unsigned mode = 0; mode < 3; ++mode) {
    Fixture f;
    f.task([&](auto &s) {
      s.with_world([&](auto &w) { w.handles.clear(f.object->heads[2]); });
      s.move_creature(f.hp, mode == 2
                                ? W::Vec3{0x42700000, 0, 0}
                                : W::Vec3{0, 0, mode == 1 ? 0x42700000U : 0U});
    });
    f.prime();
    if (mode == 2) {
      rejects<O::ObjectError>(
          [&] { f.tick(); },
          "missing grid link fails at original slide callback update");
      auto s = f.session.snapshot();
      check(s.failed && actor(s).pos[0] != 0 &&
                calls(s) == std::vector<std::string>{"world_slideMove",
                                                     "obj_updateGridCell"} &&
                !s.creature_movement->last_bits &&
                s.creature_movement->depth == 0,
            "grid failure preserves moved XY and exact prefix without collect "
            "or gravity");
      check(word(s, f.object->addr + 0x30) == 0 &&
                s.creature_movement->phase != "idle",
            "grid failure records primary phase before gravity write");
      coherent(s);
    } else {
      f.tick();
      auto s = f.session.snapshot();
      check(!s.failed &&
                calls(s) ==
                    std::vector<std::string>{"world_collectObjectsInBox",
                                             "terrain_heightBelow"},
            "zero or vertical-only delta never adds a final grid update");
      check(actor(s).pos == W::Vec3{},
            "vertical supplied velocity is not independently applied without "
            "XY slide");
    }
  }
  {
    Fixture f;
    std::size_t index = 0;
    f.session.with_world([&](auto &w) {
      index = static_cast<R::Node *>(f.object->heads[2].next->partner)->index;
      const auto available = w.handles.count_free();
      for (std::size_t n = 0; n < available; ++n)
        w.handles.add(w.always_draw, nullptr);
    });
    f.move({0x42700000, 0, 0});
    f.prime();
    f.tick();
    f.session.with_world([&](auto &w) {
      check(
          w.handles.count_free() == 0 &&
              static_cast<R::Node *>(f.object->heads[2].next->partner)->index ==
                  index,
          "full pool creature movement reuses the existing grid pair");
    });
  }
  {
    Fixture f(0, {}, true, {}, {}, true);
    f.move({0x447a0000, 0, 0}); // 1000 units/second crosses the authored wall.
    f.prime();
    f.tick();
    const auto s = f.session.snapshot();
    check(s.creature_movement->last_bits &&
              (*s.creature_movement->last_bits & 1),
          "actual supplied terrain produces a blocked slide");
    const auto &result = s.creature_movement->callback_results.front().value;
    check(result.items[0].scalar != 0 &&
              result.items[1].kind == M::ValueKind::Vector,
          "actual terrain hit supplies a real vector normal");
    const auto actual_calls = calls(s);
    check(std::find(actual_calls.begin(), actual_calls.end(), "Creature.v10") ==
              actual_calls.end(),
          "base slot10 runs directly, without a fake external callback");
  }
}
void setup_and_domain_refusals() {
  {
    S::Session s(config());
    rejects([&] { s.enable_creature_movement(); },
            "movement needs actual owned World, HP and terrain");
    check(!s.snapshot().creature_movement,
          "failed enable does not publish a partial movement model");
  }
  for (unsigned mode = 0; mode < 7; ++mode) {
    Fixture f(0, {}, false);
    S::CreatureInitial initial;
    initial.base_table = mode == 0   ? 0
                         : mode == 1 ? f.object->addr
                         : mode == 2 ? 0x44fdcc
                         : mode == 3 ? 0xfffffff0
                                     : TABLE;
    if (mode == 4)
      f.object->flags |= 0x40;
    if (mode == 5)
      f.object->angle = 65536;
    const auto before = f.session.snapshot();
    rejects(
        [&] { f.session.bind_creature_mover(mode == 6 ? 999 : f.hp, initial); },
        "bad table, derived actor, angle or unknown binding refuses setup");
    const auto after = f.session.snapshot();
    check(
        !after.creature_movement->binding &&
            after.hp->adapter.model.backing ==
                before.hp->adapter.model.backing &&
            actor(after).vtable == actor(before).vtable,
        "invalid bind preserves complete backing and original dispatch table");
  }
  {
    Fixture f(0, {}, false, {}, {}, false, 0);
    S::CreatureInitial initial;
    initial.base_table = TABLE;
    rejects([&] { f.session.bind_creature_mover(f.hp, initial); },
            "water-enabled actor is explicitly outside dry Session gate");
  }
  {
    Fixture f;
    rejects([&] { f.session.seed_hp_backing(f.hp, {{0xa8, 8, 0}}); },
            "raw setup cannot bypass protected HP cflags authority");
    check(f.session.snapshot().hp->adapter.objects.front().value.cflags ==
              0x100000,
          "rejected water mutation leaves dry flag intact");
  }
  {
    Fixture f;
    rejects(
        [&] {
          f.session.bind_isolated_mover(f.object->serial, f.object->addr);
        },
        "old authored isolated mover cannot coexist with creature binding");
  }
  {
    Fixture f;
    f.move();
    f.prime();
    rejects([&] { f.session.seed_hp_backing(f.hp, {{0xbc, 4, R::ONE}}); },
            "raw setup freezes on first submission even before a ready frame");
  }
  {
    Fixture f;
    const auto before = f.session.snapshot();
    rejects([&] { f.session.seed_hp_backing(f.hp, {{0x48, 4, TABLE + 128}}); },
            "raw setup cannot overwrite World-owned dispatch pointer");
    const auto after = f.session.snapshot();
    check(after.hp->adapter.model.backing == before.hp->adapter.model.backing &&
              actor(after).vtable == actor(before).vtable,
          "dispatch setup refusal preserves backing and World authority");
  }
  {
    Fixture f;
    f.session.with_world([&](auto &) { f.object->vtable = TABLE + 128; });
    f.move();
    f.prime();
    rejects([&] { f.tick(); },
            "changed World dispatch pointer is revalidated before movement");
    const auto s = f.session.snapshot();
    check(s.creature_movement->calls_this_tick == 0 &&
              s.creature_movement->writes.empty(),
          "table preflight rejects before accepting or executing movement");
  }
  {
    Fixture f;
    f.session.seed_hp_backing(f.hp, {{0xbc, 4, R::ONE}, {0xc4, 2, 7}});
    f.move();
    f.prime();
    f.tick();
    const auto s = f.session.snapshot();
    check(actor(s).pos[0] == R::mul(0x42700000, 0x3c888888) &&
              actor(s).angle == 7 && word(s, f.object->addr + 0xbc) == 0,
          "permitted authored root backing is consumed by the original body");
  }
}
void singleton_and_mutable_authority() {
  for (unsigned mode = 0; mode < 7; ++mode) {
    Fixture f;
    f.task([&](auto &s) {
      s.with_world([&](auto &w) {
        if (mode < 2) {
          auto *other = w.new_object(0x400, {});
          if (mode == 1) {
            other->flags |= 8;
            w.handles.clear(other->heads[2]);
          }
        } else if (mode == 2 || mode == 3) {
          w.destroy(*f.object);
          if (mode == 3)
            w.new_object(0x400, {});
        } else if (mode == 4)
          f.object->angle = 65536;
        else if (mode == 5)
          w.frame = -1;
        else
          f.object->flags |= 0x80;
      });
      s.move_creature(f.hp, {});
    });
    f.prime();
    rejects(
        [&] { f.tick(); },
        "singleton/generation/frame/facing/base domain rechecked at each move");
    const auto snapshot = f.session.snapshot();
    check(snapshot.failed && snapshot.creature_movement->calls_this_tick == 0 &&
              snapshot.creature_movement->writes.empty(),
          "unsupported live mutation refuses before original movement writes");
  }
  {
    Fixture f;
    f.task([&](auto &s) {
      s.with_world([&](auto &) {
        f.object->pos = {R::TWO, 0, 0};
        f.object->angle = 11;
        f.object->radius = R::ONE;
        f.object->height = 0x41000000;
      });
      s.take_damage(f.hp, R::TWO);
      s.move_creature(f.hp, {0x42700000, 0, 0});
      s.take_damage(f.hp, R::TWO);
    });
    f.prime();
    f.tick();
    const auto s = f.session.snapshot();
    check(s.hp->adapter.objects.front().value.hp == 0x42c00000 &&
              actor(s).pos[0] ==
                  R::add(R::TWO, R::mul(0x42700000, 0x3c888888)) &&
              actor(s).angle == 11 && actor(s).radius == R::ONE,
          "HP before and after movement preserves authoritative World edits "
          "and cumulative health");
    coherent(s);
  }
  {
    Fixture f;
    f.session.enable_base_combat();
    f.session.configure_base_combat(f.hp);
    f.task([&](auto &s) {
      s.apply_base_damage(f.hp, f.hp, R::TWO, R::TWO, 32767);
      s.move_creature(f.hp, {0x42700000, 0, 0});
    });
    f.prime();
    f.tick();
    const auto s = f.session.snapshot();
    check(s.hp->adapter.objects.front().value.hp == 0x42c40000 &&
              !s.hp->adapter.world.shared_random->draws.empty() &&
              s.creature_movement->draws == 0,
          "supported self base combat and dry movement share HP without adding "
          "movement RNG draws");
    coherent(s);
  }
}
void scheduling_and_observation() {
  {
    Fixture f;
    rejects([&] { f.session.move_creature(f.hp, {}); },
            "movement outside actual ready scheduler is refused");
  }
  for (unsigned mode = 0; mode < 3; ++mode) {
    Fixture f;
    if (mode == 0)
      f.task([&](auto &s) { s.move_creature(f.hp, {}); }, 9);
    else if (mode == 1)
      f.task([&](auto &s) {
        s.with_world([&](auto &) { s.move_creature(f.hp, {}); });
      });
    else
      f.task([&](auto &s) { s.move_creature(999, {}); });
    f.prime();
    rejects([&] { f.tick(); },
            "priority, nested World depth and ID are explicit movement gates");
    check(f.session.snapshot().creature_movement->writes.empty(),
          "scheduler refusal has no borrowed movement writes");
  }
  {
    S::Session *active = nullptr;
    S::HpId id = 0;
    S::ObjectBindings b;
    b.on_run = {"forbidden_on_run",
                [&](auto &) { active->move_creature(id, {}); }};
    Fixture f(0, {}, true, {}, {}, false, 0x100000, b);
    active = &f.session;
    id = f.hp;
    f.prime();
    rejects([&] { f.tick(); },
            "on_run cannot perform HP movement inside active object iterators");
    const auto s = f.session.snapshot();
    check(!s.creature_movement->object_run_completed &&
              s.creature_movement->calls_this_tick == 0,
          "failed on_run never claims scheduler quiescence");
  }
  {
    Fixture f;
    std::optional<S::Snapshot> before, after;
    f.task([&](auto &s) {
      before = s.snapshot();
      s.move_creature(f.hp, {});
      after = s.snapshot();
    });
    f.prime();
    f.tick();
    check(before->submitting &&
              before->creature_movement->object_run_completed &&
              before->creature_movement->task_priority ==
                  std::optional<std::int64_t>(20) &&
              before->creature_movement->calls_this_tick == 0,
          "actual task observes published post-run guard before movement");
    check(after->creature_movement->calls_this_tick == 1 &&
              after->creature_movement->phase == "idle" &&
              after->creature_movement->depth == 0 &&
              !f.session.snapshot().creature_movement->task_priority,
          "task and quiescent snapshots distinguish completed operation and "
          "scheduler phase");
  }
  {
    Fixture f;
    f.task([&](auto &s) {
      s.move_creature(f.hp, {0x42700000, 0, 0});
      s.move_creature(f.hp, {});
    });
    f.prime();
    rejects<S::ResourceLimit>([&] { f.tick(); },
                              "second movement in one tick refuses");
    const auto failed = f.session.snapshot();
    check(failed.creature_movement->calls_this_tick == 1 &&
              failed.creature_movement->last_bits ==
                  std::optional<std::uint32_t>(4) &&
              actor(failed).pos[0] != 0 && failed.failed,
          "once-per-tick failure retains first movement and latches Session");
    rejects([&] { f.tick(); },
            "failed Session cannot retry movement through another submit");
    check(f.session.snapshot().creature_movement->writes.size() ==
              failed.creature_movement->writes.size(),
          "latched retry adds no movement writes");
  }
  {
    Fixture f;
    f.move();
    f.session.submit(sample(), 0, 0, 1, true);
    f.session.set_level_pending(true);
    f.session.submit(sample());
    check(f.session.snapshot().creature_movement->writes.empty(),
          "no-simulation and pending submissions do not run creature movement");
  }
  {
    S::ObjectBindings b;
    b.is_frozen = {"frozen", [] { return true; }};
    Fixture f(0, {}, true, {}, {}, false, 0x100000, b);
    f.move();
    f.prime();
    f.tick();
    check(f.runs == 0 &&
              f.session.snapshot().creature_movement->last_bits.has_value(),
          "frozen object run does not silently suppress later authored "
          "scheduler task");
  }
}
void resource_and_failure_prefixes() {
  {
    W::Limits limit;
    limit.query_work = 0;
    Fixture f(0, {}, true, limit);
    f.move({0x42700000, 0, 0});
    f.prime();
    rejects<W::ResourceLimit>(
        [&] { f.tick(); },
        "actual terrain budget failure propagates from fixed slide service");
    const auto s = f.session.snapshot();
    check(s.failed && s.creature_movement->calls_this_tick == 1 &&
              !s.creature_movement->last_bits &&
              s.creature_movement->depth == 0 &&
              s.creature_movement->phase == "slide" &&
              calls(s) == std::vector<std::string>{"world_slideMove"} &&
              s.creature_movement->callback_results.empty() &&
              actor(s).pos == W::Vec3{},
          "query refusal retains accepted count, exact callback prefix and "
          "unmoved position");
    coherent(s);
  }
  {
    S::HpLimits limits;
    limits.memory.history_entries = 2;
    Fixture f(0, {}, true, {}, limits);
    f.move();
    f.prime();
    rejects<M::ResourceLimit>(
        [&] { f.tick(); },
        "borrowed operation history cap terminates original body");
    const auto s = f.session.snapshot();
    check(s.creature_movement->writes.size() == 2 &&
              s.creature_movement->calls.empty() &&
              s.creature_movement->depth == 0 &&
              !s.creature_movement->last_bits,
          "history failure retains only the permitted original write prefix");
    coherent(s);
  }
  {
    S::HpLimits limits;
    limits.memory.memory_bytes = 100;
    Fixture f(0, {}, false, {}, limits);
    const auto before = f.session.snapshot();
    S::CreatureInitial initial;
    initial.base_table = TABLE;
    rejects([&] { f.session.bind_creature_mover(f.hp, initial); },
            "field/table capacity is checked before publishing binding");
    const auto after = f.session.snapshot();
    check(!after.creature_movement->binding &&
              after.hp->adapter.model.backing ==
                  before.hp->adapter.model.backing &&
              actor(after).vtable == actor(before).vtable,
          "capacity refusal leaves raw fields, table identity and binding "
          "unpublished");
  }
  {
    Fixture f;
    f.move({0x42700000, 0, 0});
    f.task([](auto &) { throw std::runtime_error("authored later task"); }, 30,
           0xf4000002);
    f.prime();
    rejects<std::runtime_error>(
        [&] { f.tick(); }, "later task failure propagates after real movement");
    const auto s = f.session.snapshot();
    check(
        s.failed && s.creature_movement->phase == "idle" &&
            s.creature_movement->last_bits.has_value() &&
            actor(s).pos[0] != 0 && s.creature_movement->depth == 0,
        "later failure preserves completed movement and its owned observation");
    coherent(s);
  }
}
} // namespace
int main() {
  try {
    R::ScopedFpu fixed(R::Fpu{});
    arithmetic_and_owned_state();
    roots_timer_inert_and_gravity();
    callback_threshold_and_grid_failures();
    setup_and_domain_refusals();
    singleton_and_mutable_authority();
    scheduling_and_observation();
    resource_and_failure_prefixes();
    std::cout << "session creature: " << checks << " checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "session creature failed after " << checks << ": " << e.what()
              << '\n';
    return 1;
  }
}
