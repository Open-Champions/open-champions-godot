#include "champions/session.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
namespace R = champions::simulation::rta;
namespace S = R::session;
namespace O = R::objects;
namespace W = R::world_queries;
namespace N = R::netwire;
namespace {
unsigned checks = 0;
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
R::Raw I(int v) { return R::from_int(std::uint32_t(v)); }
S::Config config(unsigned bits = 0, unsigned local = 0) {
  S::Config c;
  c.network_mode = 1;
  c.peers = 2;
  c.local = local;
  c.model = static_cast<R::Fpu::Model>(bits / 4);
  c.mul_bias = bits & 2;
  c.div_nearest = bits & 1;
  c.seed = 0xabcdef01;
  return c;
}
W::LevelData terrain() {
  W::LevelData d;
  d.tg_x0 = -1;
  d.tg_y0 = -1;
  d.tg_w = d.tg_h = 2;
  d.grid = {{0}, {0}, {0}, {0}};
  W::HeightField h;
  h.x0 = -100;
  h.y0 = -2;
  h.w = 201;
  h.h = 5;
  h.samples.resize(1005);
  d.heightfields.push_back(h);
  d.elements.push_back(
      W::make_element(7, -400, -8, 400, 8, 0, 0, 0, 0, 0, 32767, 0));
  return d;
}
N::ByteVector sample() { return {1, 0, 128, 64, 255, 0, 5, 9}; }
N::ByteVector ready(const S::Config &c) {
  N::Machine sender(2, 1, N::Fpu(c.model, c.mul_bias, c.div_nearest));
  for (unsigned i = 1; i <= 16; ++i)
    sender.peers[1].set_record(i, N::rec12(1, {128, 64, 255, 0}, {5, 9}, 0, 0),
                               0, 0);
  return sender.build_packet(0, 1, 16, N::PKT_RAW_RELAY, 1, nullptr, true).data;
}
struct Fixture {
  S::Session s;
  S::Config cfg;
  O::GameObject *actor = nullptr;
  S::MoverId mover = 0;
  S::HpId hp = 0;
  unsigned runs = 0, moves = 0;
  explicit Fixture(unsigned bits = 0, bool health = false,
                   W::Limits limits = {}, S::ObjectBindings bindings = {},
                   unsigned local = 0)
      : s(config(bits, local)), cfg(config(bits, local)) {
    bindings.policy = O::CallbackPolicy::ReferenceBase;
    if (!bindings.on_run.function)
      bindings.on_run = {"count_run", [this](auto &) { ++runs; }};
    s.enable_objects(17, std::move(bindings));
    s.with_world([&](O::World &w) {
      actor = w.new_object(0x400, {0, 0, 0});
      actor->radius = I(4);
      w.run_every_frame(*actor);
    });
    if (health) {
      s.enable_hp();
      hp = s.bind_hp(actor->serial, actor->addr, {I(100), 0, {}});
    }
    s.enable_terrain(terrain(), limits);
    mover = s.bind_isolated_mover(actor->serial, actor->addr);
  }
  void task(std::function<void(S::Session &)> fn, int priority = 20) {
    s.register_object_task(0xf3000001 + unsigned(priority), "authored_terrain",
                           priority, 0, std::move(fn));
  }
  void move_task(W::Vec3 velocity = {0, 0, 0}) {
    task([this, velocity](auto &x) {
      x.move_isolated(mover, velocity);
      ++moves;
    });
  }
  void prime() {
    auto r = s.submit(sample(), 0, 0, 1, false, false, 0, {ready(cfg)});
    check(r.steps == 0, "first input does not invent a ready tick");
  }
  N::NetMainResult tick() { return s.submit(sample()); }
};
const O::ObjectSnapshot &actor(const S::Snapshot &s) {
  return s.world->objects.front();
}
void basic() {
  for (unsigned c = 0; c < 16; ++c) {
    Fixture f(c);
    f.move_task({I(60), 0, 0});
    auto before = f.s.snapshot();
    f.prime();
    f.tick();
    auto after = f.s.snapshot();
    R::ScopedFpu fp(R::Fpu(static_cast<R::Fpu::Model>(c / 4), c & 2, c & 1));
    check(actor(after).pos[0] == R::mul(I(60), 0x3c888888) &&
              actor(after).pos[2] == 0 && actor(after).vz == 0,
          "ready movement advances configured raw step and lands");
    check(after.terrain->last_result && after.terrain->last_result->bits == 4 &&
              after.terrain->calls_this_tick == 1 && f.moves == 1,
          "one ready task records actual isolated move result");
    check(after.world->shared_seed == before.world->shared_seed &&
              after.seed == before.seed && after.draws == before.draws,
          "terrain does not consume either random stream");
    check(before.terrain->level.data.stamp == 0 && actor(before).pos[0] == 0,
          "earlier snapshot owns old terrain and actor scalars");
    check(after.terrain->phase == "idle" && after.terrain->depth == 0 &&
              !after.terrain->task_priority,
          "completed movement clears transient operation guards");
  }
}
void setup_and_isolation() {
  for (auto name : {"norrath", "unknown"}) {
    auto c = config();
    c.profile = name;
    rejects([&] { S::Session bad(c); }, "unsupported title profile refuses");
  }
  {
    S::Session s(config());
    rejects([&] { s.enable_terrain(terrain()); },
            "terrain requires existing owned World");
  }
  {
    Fixture f;
    rejects([&] { f.s.enable_terrain(terrain()); },
            "terrain setup is single ownership");
  }
  {
    Fixture f;
    rejects([&] { f.s.bind_isolated_mover(f.actor->serial, f.actor->addr); },
            "mover binding cannot be replaced");
  }
  for (unsigned mode : {0u, 1u, 3u}) {
    Fixture f;
    f.s.with_world([&](O::World &w) {
      auto *other = w.new_object(0x400, {I(1000), 0, 0});
      if (mode == 1)
        w.move_off_grid(*other);
      if (mode == 3)
        w.objects.erase(other->addr);
    });
    f.move_task();
    f.prime();
    rejects([&] { f.tick(); }, "any second live retained actor refuses "
                               "isolation, even hidden or pending deletion");
    auto s = f.s.snapshot();
    check(s.failed && s.terrain->level.data.stamp == 0 &&
              actor(s).pos == W::Vec3{},
          "isolation refusal precedes query and scalar mutation");
  }
  {
    Fixture f;
    f.task([&](auto &s) {
      s.with_world([](auto &w) {
        auto *o = w.new_object(0x400, {0, 0, 0});
        w.delete_object(*o);
      });
      s.move_isolated(f.mover, {});
    });
    f.prime();
    rejects([&] { f.tick(); },
            "same-tick pending-delete actor still violates isolation");
  }
  {
    Fixture f;
    f.task([&](auto &s) {
      s.with_world([](auto &w) { w.new_object(0x400, {0, 0, 0}); });
      s.move_isolated(f.mover, {});
    });
    f.prime();
    rejects([&] { f.tick(); },
            "same-tick stationary creation invalidates isolation");
  }
  {
    Fixture f;
    f.task([&](auto &s) {
      s.with_world([&](auto &w) {
        auto addr = f.actor->addr;
        w.destroy(*f.actor, 3);
        auto *next = w.new_object(0x400, {0, 0, 0});
        check(next->addr == addr, "authored reuse actually reuses address");
      });
      s.move_isolated(f.mover, {});
    });
    f.prime();
    rejects([&] { f.tick(); }, "old mover generation refuses reused address");
  }
}
void phases_and_failures() {
  {
    Fixture f;
    rejects([&] { f.s.move_isolated(f.mover, {}); },
            "movement outside ready task refuses");
  }
  {
    Fixture f;
    f.task([&](auto &s) { s.move_isolated(f.mover, {}); }, 5);
    f.prime();
    rejects([&] { f.tick(); }, "pre-runObjects movement refuses");
  }
  {
    Fixture f;
    f.task([&](auto &s) {
      s.with_world([&](auto &) { s.move_isolated(f.mover, {}); });
    });
    f.prime();
    rejects([&] { f.tick(); }, "nested with_world movement refuses");
  }
  {
    Fixture f;
    f.task([&](auto &s) {
      s.move_isolated(f.mover, {I(60), 0, 0});
      s.move_isolated(f.mover, {});
    });
    f.prime();
    rejects([&] { f.tick(); }, "second move in ready tick refuses");
    auto before = f.s.snapshot();
    check(before.terrain->calls_this_tick == 1 && actor(before).pos[0] != 0,
          "double move retains first completed result");
    rejects<S::Failed>([&] { f.tick(); }, "failed movement is not retried");
    auto after = f.s.snapshot();
    check(after.terrain->level.data.stamp == before.terrain->level.data.stamp &&
              actor(after).pos == actor(before).pos,
          "retry changes no terrain or position");
  }
  {
    W::Limits cap;
    cap.query_work = 1;
    Fixture f(0, false, cap);
    f.move_task({I(60), 0, 0});
    f.prime();
    rejects<W::ResourceLimit>([&] { f.tick(); },
                              "query cap refuses with exact partial stamps");
    auto s = f.s.snapshot();
    check(s.terrain->level.data.stamp == 1 &&
              s.terrain->level.data.elements[0].stamp == 0 &&
              actor(s).pos == W::Vec3{} && !s.terrain->last_result &&
              s.terrain->phase == "terrain_query",
          "query failure commits no movement result");
  }
  {
    Fixture f(0, true);
    f.task([&](auto &s) {
      s.with_world([&](auto &w) { w.handles.clear(f.actor->heads[2]); });
      s.move_isolated(f.mover, {I(60), 0, 0});
    });
    f.prime();
    rejects<O::ObjectError>(
        [&] { f.tick(); },
        "missing grid pair refuses after successful terrain computation");
    auto s = f.s.snapshot();
    check(actor(s).pos[0] != 0 && s.terrain->last_result &&
              s.terrain->phase == "grid_update" && s.failed,
          "grid failure preserves moved scalars and result");
    check(s.hp->adapter.objects[0].value.pos[0] == actor(s).pos[0],
          "grid failure still publishes moved position into HP alias");
  }
  {
    Fixture f;
    f.move_task({I(60), 0, 0});
    f.s.register_object_task(0xf3000080, "later_failure", 80, 0, [](auto &) {
      throw std::runtime_error("authored failure");
    });
    f.prime();
    rejects<std::runtime_error>([&] { f.tick(); },
                                "later task failure propagates");
    check(actor(f.s.snapshot()).pos[0] != 0,
          "later task failure retains completed movement");
  }
}
void health_and_pool() {
  Fixture f(0, true);
  f.task([&](auto &s) {
    s.move_isolated(f.mover, {I(60), 0, 0});
    s.take_damage(f.hp, I(2));
  });
  f.prime();
  f.tick();
  auto a = f.s.snapshot();
  f.tick();
  auto b = f.s.snapshot();
  check(a.hp->adapter.objects[0].value.hp == I(98) &&
            b.hp->adapter.objects[0].value.hp == I(96),
        "two ready moves retain persistent sole-actor HP");
  check(actor(b).pos[0] != actor(a).pos[0] &&
            b.hp->adapter.objects[0].value.pos[0] == actor(b).pos[0],
        "HP import/publication does not restore stale moved position");
  Fixture full;
  std::uint64_t node = 0;
  full.s.with_world([&](auto &w) {
    node = static_cast<R::Node *>(full.actor->heads[2].next->partner)->index;
    auto count = w.handles.count_free();
    for (std::size_t i = 0; i < count; ++i)
      w.handles.add(w.always_draw, nullptr);
  });
  full.move_task({I(60), 0, 0});
  full.prime();
  full.tick();
  full.s.with_world([&](auto &w) {
    check(
        w.handles.count_free() == 0 &&
            static_cast<R::Node *>(full.actor->heads[2].next->partner)->index ==
                node,
        "full pool movement reuses original grid node without allocation");
  });
}
void scheduling_and_peers() {
  {
    Fixture f;
    f.move_task();
    for (unsigned i = 0; i < 3; ++i)
      f.s.submit(sample());
    check(f.moves == 0 && f.s.snapshot().terrain->level.data.stamp == 0,
          "missing remote ready inputs do not move terrain actor");
  }
  {
    Fixture f;
    f.move_task();
    f.s.set_level_pending(true);
    f.prime();
    f.tick();
    check(f.moves == 0, "pending level suppresses terrain tasks");
    f.s.set_level_pending(false);
    f.tick();
    check(f.moves == 1, "terrain resumes only when ready task is allowed");
  }
  {
    Fixture f;
    f.move_task();
    f.prime();
    f.s.submit(sample(), 0, 0, 1, true);
    check(f.moves == 0, "no_sim suppresses ready terrain tasks");
  }
  {
    S::ObjectBindings b;
    b.is_frozen = {"frozen", [] { return true; }};
    Fixture f(0, false, {}, b);
    f.move_task();
    f.prime();
    f.tick();
    check(f.runs == 0 && f.moves == 1,
          "frozen runObjects still permits explicit post-run terrain task");
  }
  {
    Fixture *owner = nullptr;
    S::ObjectBindings b;
    b.on_run = {"bad_run",
                [&](auto &) { owner->s.move_isolated(owner->mover, {}); }};
    Fixture f(0, false, {}, b);
    owner = &f;
    f.move_task();
    f.prime();
    rejects([&] { f.tick(); }, "on_run cannot call post-run terrain operation");
    check(f.s.snapshot().terrain->level.data.stamp == 0,
          "on_run refusal does not start terrain query");
  }
  {
    S::ObjectBindings b;
    b.end_of_tick = {"end_failure",
                     [] { throw std::runtime_error("authored end failure"); }};
    Fixture f(0, false, {}, b);
    f.move_task({I(60), 0, 0});
    f.prime();
    rejects<std::runtime_error>(
        [&] { f.tick(); }, "end-of-tick failure propagates after movement");
    check(f.s.snapshot().terrain->last_result.has_value(),
          "end failure retains completed terrain result");
  }
  std::array<std::unique_ptr<Fixture>, 2> peers{
      std::make_unique<Fixture>(0, false, W::Limits{}, S::ObjectBindings{}, 0),
      std::make_unique<Fixture>(0, false, W::Limits{}, S::ObjectBindings{}, 1)};
  for (auto &p : peers)
    p->move_task({I(60), 0, 0});
  std::array<std::vector<N::ByteVector>, 2> queues;
  std::array<std::map<std::int64_t, std::vector<std::uint64_t>>, 2> states;
  const std::array<unsigned, 12> order{0, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1};
  for (unsigned turn = 0; turn < order.size(); ++turn) {
    auto which = order[turn];
    auto &p = *peers[which];
    std::vector<N::ByteVector> inbox;
    if (turn >= 3) {
      inbox = queues[which];
      queues[which].clear();
    }
    auto old = p.moves;
    auto r = p.s.submit(sample(), 0, 0, 1, false, false, turn, inbox);
    for (const auto &packet : r.sent)
      queues.at(std::size_t(packet.peer)).push_back(packet.data);
    if (p.moves > old) {
      auto snap = p.s.snapshot();
      const auto &o = actor(snap);
      states[which][snap.network.net_frame] = {o.pos[0],
                                               o.pos[1],
                                               o.pos[2],
                                               o.vz,
                                               snap.terrain->level.data.stamp,
                                               snap.world->shared_seed,
                                               snap.world->handle_free};
    }
  }
  unsigned matched = 0;
  for (const auto &[frame, value] : states[0])
    if (states[1].count(frame)) {
      check(value == states[1].at(frame),
            "opposite real endpoints agree on completed movement, terrain, "
            "pool and shared state");
      ++matched;
    }
  check(matched >= 2,
        "opposite endpoint evidence contains multiple common completed ticks");
}
void setup_copy_and_foreign_guards() {
  {
    S::Session s(config());
    S::ObjectBindings b;
    b.policy = O::CallbackPolicy::ReferenceBase;
    s.enable_objects(17, b);
    std::uint64_t serial = 0;
    std::uint32_t address = 0;
    s.with_world([&](auto &w) {
      auto *o = w.new_object(0x400, {0, 0, 0});
      serial = o->serial;
      address = o->addr;
    });
    auto source = terrain();
    W::Limits limits;
    limits.query_work = 91;
    s.enable_terrain(source, limits);
    source.heightfields[0].samples[0] = 123;
    source.grid.clear();
    source.elements[0].z = 99;
    limits.query_work = 0;
    s.bind_isolated_mover(serial, address);
    auto snap = s.snapshot();
    check(snap.terrain->level.limits.query_work == 91 &&
              snap.terrain->level.data.grid.size() == 4 &&
              snap.terrain->level.data.elements[0].z == 0 &&
              snap.terrain->level.data.heightfields[0].samples[0] == 0,
          "Session owns original Level data and limits independently of caller "
          "mutation");
  }
  for (unsigned mode = 0; mode < 3; ++mode) {
    Fixture f;
    O::GameObject foreign(f.actor->addr, f.actor->serial);
    f.task([&](auto &s) {
      s.with_world([&](auto &w) {
        if (mode == 0)
          w.objects[f.actor->addr] = nullptr;
        else if (mode == 1)
          w.objects[f.actor->addr] = &foreign;
        else
          w.pool.free(f.actor->addr);
      });
      s.move_isolated(f.mover, {I(60), 0, 0});
    });
    f.prime();
    if (mode == 1) {
      rejects<O::ObjectError>([&] { f.tick(); },
                              "foreign live registry pointer is rejected "
                              "through owned identity before query");
      check(f.s.failed() && f.actor->pos == W::Vec3{},
            "foreign pointer refusal leaves actor unmoved and failure latched");
    } else {
      rejects([&] { f.tick(); },
              "null registry or freed allocated block refuses selected mover");
      const auto snap = f.s.snapshot();
      check(snap.terrain->level.data.stamp == 0 && actor(snap).pos == W::Vec3{},
            "identity and allocation refusal precedes terrain mutation");
    }
  }
  for (bool bind : {false, true}) {
    Fixture f;
    f.prime();
    rejects(
        [&] {
          if (bind)
            f.s.bind_isolated_mover(f.actor->serial, f.actor->addr);
          else
            f.s.enable_terrain(terrain());
        },
        "terrain setup APIs refuse after first submission");
  }
  {
    Fixture f(0, true);
    f.task([&](auto &s) {
      s.take_damage(f.hp, I(2));
      s.move_isolated(f.mover, {I(60), 0, 0});
      s.take_damage(f.hp, I(2));
    });
    f.prime();
    f.tick();
    const auto snap = f.s.snapshot();
    const auto &backing = snap.hp->adapter.model.backing;
    std::uint32_t rawx = 0;
    for (unsigned i = 0; i < 4; ++i)
      rawx |= std::uint32_t(backing.at(f.actor->addr + 0x10 + i)) << (8 * i);
    check(snap.hp->adapter.objects[0].value.hp == I(96) &&
              rawx == actor(snap).pos[0] && rawx != 0,
          "HP then move then HP preserves moved raw and typed position with "
          "persistent health");
  }
}
void ownership_and_fpu() {
  {
    Fixture f;
    auto copy = f.s.snapshot();
    copy.terrain->level.data.heightfields[0].samples[0] = 123;
    copy.terrain->binding->address = 0;
    auto actual = f.s.snapshot();
    check(actual.terrain->level.data.heightfields[0].samples[0] == 0 &&
              actual.terrain->binding->address == f.actor->addr,
          "terrain snapshot owns topology and identity values");
  }
  for (bool fail : {false, true}) {
    R::Fpu ambient(R::Fpu::Model::Nearest, true, true);
    auto &current =
        R::set_fpu(ambient.model, ambient.mul_bias, ambient.div_nearest);
    Fixture f;
    f.task([&](auto &s) {
      s.move_isolated(f.mover, {});
      if (fail)
        throw std::runtime_error("after movement");
    });
    f.prime();
    try {
      f.tick();
    } catch (const std::runtime_error &) {
      if (!fail)
        throw;
    }
    check(current.model == ambient.model &&
              current.mul_bias == ambient.mul_bias &&
              current.div_nearest == ambient.div_nearest,
          "Session restores caller FPU on success and failure");
  }
}
} // namespace
int main() {
  try {
    basic();
    setup_and_isolation();
    phases_and_failures();
    health_and_pool();
    scheduling_and_peers();
    setup_copy_and_foreign_guards();
    ownership_and_fpu();
    std::cout << checks << " session terrain checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "session terrain check " << checks << ": " << e.what() << '\n';
    return 1;
  }
}
