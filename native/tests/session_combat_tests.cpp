#include "champions/session.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
namespace R = champions::simulation::rta;
namespace S = R::session;
namespace O = R::objects;
namespace N = R::netwire;
namespace C = R::combat;
namespace {
unsigned checks = 0;
void check(bool v, const char *message) {
  ++checks;
  if (!v)
    throw std::runtime_error(message);
}
template <class E = S::Error, class F> void rejects(F f, const char *message) {
  bool caught = false;
  try {
    f();
  } catch (const E &) {
    caught = true;
  }
  check(caught, message);
}
S::Config config(unsigned cfg = 0, unsigned local = 0) {
  S::Config c;
  c.network_mode = 1;
  c.peers = 2;
  c.local = local;
  c.model = static_cast<R::Fpu::Model>(cfg / 4);
  c.mul_bias = cfg & 2;
  c.div_nearest = cfg & 1;
  c.seed = 0xabcdef01;
  return c;
}
S::ObjectBindings base() {
  S::ObjectBindings b;
  b.policy = O::CallbackPolicy::ReferenceBase;
  return b;
}
N::ByteVector sample() { return {1, 0, 128, 64, 255, 0, 5, 9}; }
N::ByteVector ready(const S::Config &c) {
  N::Machine sender(2, 1, N::Fpu(c.model, c.mul_bias, c.div_nearest));
  for (unsigned i = 1; i <= 16; ++i)
    sender.peers[1].set_record(
        i, N::rec12(17 * i, {129, 65, 254, 9}, {11, 13}, 123 * i, i & 1), 0, 0);
  return sender.build_packet(0, 1, 16, N::PKT_RAW_RELAY, 1, nullptr, true).data;
}
R::Raw hp(const S::Snapshot &s, unsigned i = 0) {
  return s.hp->adapter.objects.at(i).value.hp;
}
std::uint64_t word(const S::Snapshot &s, unsigned offset, unsigned i = 0) {
  auto a = s.hp->bindings.at(i).address + offset;
  std::uint64_t v = 0;
  for (unsigned k = 0; k < 4; ++k) {
    auto p = s.hp->adapter.model.backing.find(a + k);
    if (p != s.hp->adapter.model.backing.end())
      v |= std::uint64_t(p->second) << (8 * k);
  }
  return v;
}
const C::SharedRandomSnapshot &authority(const S::Snapshot &s) {
  return s.hp->adapter.world.shared_random.value();
}
std::vector<std::uint32_t> draw_values(const S::Snapshot &s) {
  std::vector<std::uint32_t> out;
  for (const auto &d : authority(s).draws)
    out.push_back(d.value);
  return out;
}
struct Fixture {
  S::Config settings;
  S::Session session;
  std::array<std::uint32_t, 3> addresses{};
  std::array<std::uint64_t, 3> serials{};
  std::array<S::HpId, 3> ids{};
  unsigned hits = 0;
  std::vector<int> results;
  Fixture(S::Config c = config(), std::uint32_t seed = 1,
          std::int16_t armor = 0, S::ObjectBindings callbacks = base(),
          unsigned count = 2, S::HpLimits limits = {}, std::uint64_t cflags = 0,
          unsigned configured = 3)
      : settings(c), session(c) {
    session.enable_objects(seed, std::move(callbacks));
    session.with_world([&](O::World &w) {
      for (unsigned i = 0; i < count; ++i) {
        auto *o = w.new_object(0x400, {R::from_int(i), 0, 0});
        o->flags |= 0x20;
        addresses[i] = o->addr;
        serials[i] = o->serial;
        w.run_every_frame(*o);
      }
    });
    session.enable_hp(limits);
    for (unsigned i = 0; i < count; ++i) {
      S::HpInitial initial;
      initial.hp = 0x42c80000;
      if (!i)
        initial.cflags = cflags;
      ids[i] = session.bind_hp(serials[i], addresses[i], initial);
    }
    session.enable_base_combat();
    for (unsigned i = 0; i < std::min(count, configured); ++i)
      session.configure_base_combat(
          ids[i], S::BaseCombatInitial{std::int16_t(i ? 0 : armor)});
  }
  void task(R::Raw low = R::TWO, R::Raw high = R::TWO,
            std::int32_t attack = 100000, std::uint32_t flags = 0x40) {
    session.register_object_task(
        100, "authored_combat", 125, 0,
        [this, low, high, attack, flags](S::Session &x) {
          results.push_back(
              x.apply_base_damage(ids[0], ids[1], low, high, attack, flags));
          ++hits;
        });
  }
  void submit(unsigned k, bool inbox = false) {
    session.submit(sample(), 0, 0, 1, false, false, k,
                   inbox ? std::vector<N::ByteVector>{ready(settings)}
                         : std::vector<N::ByteVector>{});
  }
};
void literal_authority() {
  for (unsigned cfg = 0; cfg < 16; ++cfg) {
    Fixture f(config(cfg));
    auto initial = f.session.snapshot();
    check(initial.world->shared_seed == 0x5e8885db &&
              authority(initial).state == 0x5e8885db &&
              authority(initial).draws.empty(),
          "authority reads actual two-constructor World state without copying "
          "seed");
    std::vector<std::uint32_t> outside;
    f.session.register_object_task(99, "before", 20, 0, [&](S::Session &x) {
      x.with_world([&](O::World &w) { outside.push_back(w.shared.draw()); });
    });
    f.task();
    f.session.register_object_task(101, "after", 130, 0, [&](S::Session &x) {
      x.with_world([&](O::World &w) { outside.push_back(w.shared.draw()); });
    });
    f.submit(0, true);
    check(f.hits == 0 && authority(f.session.snapshot()).draws.empty(),
          "unready presentation does not draw or damage");
    f.submit(1);
    auto first = f.session.snapshot();
    check(outside == std::vector<std::uint32_t>{33046, 41893} &&
              draw_values(first) == std::vector<std::uint32_t>{46195, 3312,
                                                               24216, 50774,
                                                               36450, 1080},
          "literal object/combat/object interleaving uses one live stream");
    const std::array<std::uint32_t, 6> states{
        0xb4733ac5, 0x0cf06d60, 0x5e98c13f, 0xc656dd92, 0x8e625fc9, 0x0438e694};
    for (unsigned i = 0; i < states.size(); ++i)
      check(authority(first).draws[i].state == states[i],
            "each combat draw retains its actual resulting authority state");
    check(first.world->shared_seed == 0xa3a5a0e3 &&
              authority(first).state == 0xa3a5a0e3 &&
              first.network.rand_seed == 0xa3a5a0e3,
          "postdraw authority and published network seed agree");
    check(hp(first) == 0x42c40000 && word(first, 0xc8) == R::TWO &&
              f.results == std::vector<int>{1},
          "actual combat applies literal100 to98 and pending2");
    check(first.seed == 0xabcdef01 && first.draws.empty() &&
              first.hp->adapter.world.seed.state == 0 &&
              first.hp->adapter.world.seed.draws.empty() &&
              first.hp->adapter.world.cosmetic.draws.empty(),
          "legacy and dormant fallback/cosmetic streams remain distinct");
    f.submit(2);
    check(hp(f.session.snapshot()) == 0x42c00000 &&
              word(f.session.snapshot(), 0xc8) == 0x40800000 &&
              hp(first) == 0x42c40000,
          "second combat reads persistent98 while old snapshot stays owned");
  }
}
void draw_branches() {
  for (unsigned cfg = 0; cfg < 16; ++cfg) {
    Fixture range(config(cfg));
    range.task(R::from_int(3), R::from_int(9));
    range.submit(0, true);
    range.submit(1);
    auto r = range.session.snapshot();
    check(hp(r) == 0x42b60000 && draw_values(r).size() == 7 &&
              draw_values(r).front() == 33046 &&
              authority(r).state == 0x0438e694,
          "inclusive range roll33046mod7 gives9 and seven shared draws");
    Fixture equal(config(cfg));
    equal.task(R::from_int(3), R::from_int(3));
    equal.submit(0, true);
    equal.submit(1);
    check(hp(equal.session.snapshot()) == 0x42c20000 &&
              draw_values(equal.session.snapshot()).size() == 6,
          "equal range skips only base roll draw");
    Fixture zero(config(cfg));
    zero.task(0, 0);
    zero.submit(0, true);
    zero.submit(1);
    check(hp(zero.session.snapshot()) == 0x42c80000 &&
              draw_values(zero.session.snapshot()).size() == 6 &&
              !zero.session.snapshot().hp->adapter.model.writes.empty(),
          "zero amount retains actual draws and HP metadata writes");
    Fixture miss(config(cfg), 1, 1);
    miss.task(R::TWO, R::TWO, 0);
    miss.submit(0, true);
    miss.submit(1);
    auto m = miss.session.snapshot();
    check(miss.results == std::vector<int>{0} &&
              draw_values(m) == std::vector<std::uint32_t>{33046, 46195} &&
              hp(m) == 0x42c80000 && word(m, 0x2dc) == 0x42000,
          "guaranteed miss draws twice and invokes real zero HP path with miss "
          "flags");
    Fixture range_miss(config(cfg), 1, 1);
    range_miss.task(R::TWO, R::from_int(8), 0);
    range_miss.submit(0, true);
    range_miss.submit(1);
    check(draw_values(range_miss.session.snapshot()).size() == 3 &&
              hp(range_miss.session.snapshot()) == 0x42c80000,
          "ranged miss consumes roll plus two hit draws");
    Fixture second_hit(config(cfg), 1, 4);
    second_hit.session.with_world([](O::World &w) { w.shared.draw(); });
    second_hit.task(R::from_int(8), R::from_int(8), 1);
    second_hit.submit(0, true);
    second_hit.submit(1);
    auto h = second_hit.session.snapshot();
    check(second_hit.results == std::vector<int>{1} &&
              draw_values(h) == std::vector<std::uint32_t>{46195, 3312} &&
              hp(h) == 0x42c00000,
          "initial miss/final hit consumes two draws and applies attenuated4");
    Fixture second_miss(config(cfg), 0xdeadbeef, 4);
    second_miss.task(R::from_int(8), R::from_int(8), 1);
    second_miss.submit(0, true);
    second_miss.submit(1);
    auto sm = second_miss.session.snapshot();
    check(second_miss.results == std::vector<int>{0} &&
              draw_values(sm) == std::vector<std::uint32_t>{2441, 14499, 25459,
                                                            55354, 14330,
                                                            64549} &&
              hp(sm) == 0x42c80000,
          "initial hit/final miss preserves independent six-draw branch");
  }
}
void failure_prefix() {
  Fixture pool;
  pool.session.register_object_task(99, "drain", 20, 0, [](S::Session &x) {
    x.with_world([](O::World &w) {
      const auto count = w.handles.count_free();
      for (std::size_t i = 1; i < count; ++i)
        w.handles.add(w.always_draw, nullptr);
    });
  });
  pool.task();
  pool.submit(0, true);
  rejects<R::HandleError>([&] { pool.submit(1); },
                          "second handle take fails after actual combat draws");
  auto failed = pool.session.snapshot();
  check(failed.failed &&
            draw_values(failed) == std::vector<std::uint32_t>{33046, 46195,
                                                              3312, 24216,
                                                              50774, 36450} &&
            failed.world->shared_seed == 0x8e625fc9 &&
            failed.network.rand_seed == 0x8e625fc9,
        "failed HP publishes advanced live shared authority without rollback");
  check(
      hp(failed) == 0x42c80000 && failed.world->handle_free == 0 &&
          !failed.hp->adapter.model.writes.empty(),
      "pool failure retains write/allocation prefix but no final HP subtract");
  rejects([&] { pool.submit(2); }, "failure latches retries");
  check(draw_values(pool.session.snapshot()) == draw_values(failed),
        "refused retry draws nothing");
  Fixture post;
  post.task();
  post.session.register_object_task(101, "throw", 130, 0, [](S::Session &) {
    throw std::runtime_error("authored");
  });
  post.submit(0, true);
  rejects<std::runtime_error>([&] { post.submit(1); },
                              "later task failure propagates");
  check(hp(post.session.snapshot()) == 0x42c40000 &&
            draw_values(post.session.snapshot()).size() == 6,
        "later failure retains successful combat and shared draws");
  Fixture cap;
  for (unsigned i = 0; i < 31700; ++i)
    cap.session.draw();
  unsigned done = 0;
  cap.session.register_object_task(100, "cap", 125, 0, [&](S::Session &x) {
    for (unsigned i = 0; i < 64; ++i) {
      x.apply_base_damage(cap.ids[0], cap.ids[1], R::ONE, R::ONE, 100000);
      ++done;
    }
  });
  cap.submit(0, true);
  rejects<S::ResourceLimit>(
      [&] { cap.submit(1); },
      "aggregate combat reserve refuses before unbounded histories");
  auto c = cap.session.snapshot();
  check(done > 0 && done < 64 && c.failed &&
            authority(c).draws.size() == done * 6,
        "near history cap remains snapshotable with complete accepted draw "
        "prefix");
}
void readiness_and_domain() {
  for (unsigned mode = 0; mode < 4; ++mode) {
    Fixture f;
    f.task();
    if (mode == 2)
      f.session.set_level_pending(true);
    for (unsigned k = 0; k < 3; ++k)
      f.session.submit(sample(), 0, 0, 1, mode == 3, false, k,
                       mode && k == 0
                           ? std::vector<N::ByteVector>{ready(f.settings)}
                           : std::vector<N::ByteVector>{});
    check(f.hits == (mode == 1 ? 2u : 0u) &&
              authority(f.session.snapshot()).draws.size() ==
                  (mode == 1 ? 12u : 0u),
          "ready versus missing/pending/no_sim counts real tasks and draws");
  }
  for (auto flag : {0x40u, 0x80u, 0x10000000u}) {
    Fixture f;
    f.task();
    f.session.with_world(
        [&](O::World &w) { w.objects.at(f.addresses[0])->flags |= flag; });
    f.submit(0, true);
    rejects([&] { f.submit(1); },
            "live unsupported victim flags refuse despite valid setup");
    check(authority(f.session.snapshot()).draws.empty() &&
              f.session.snapshot().hp->adapter.model.writes.empty(),
          "domain refusal precedes random/HP side effects");
  }
  for (auto flags : {1u, 0x800000u, 0x10000000u}) {
    Fixture f;
    f.task(R::TWO, R::TWO, 100000, flags);
    f.submit(0, true);
    rejects([&] { f.submit(1); }, "unsupported caller flag refuses instead of "
                                  "invoking deferred services");
    check(authority(f.session.snapshot()).draws.empty(),
          "unsupported flags consume no shared draw");
  }
  Fixture outer;
  rejects(
      [&] {
        outer.session.apply_base_damage(outer.ids[0], outer.ids[1], R::ONE,
                                        R::ONE, 1);
      },
      "combat refuses outside actual ready task");
  Fixture reused(config(), 1, 0, base(), 3);
  reused.session.register_object_task(99, "reuse", 20, 0, [&](S::Session &x) {
    x.with_world([&](O::World &w) {
      w.destroy(*w.objects.at(reused.addresses[2]), 3);
      auto *o = w.new_object(0x400, {0, 0, 0});
      check(o->addr == reused.addresses[2] && o->serial != reused.serials[2],
            "actual unrelated bound generation reused");
    });
  });
  reused.task();
  reused.submit(0, true);
  rejects([&] { reused.submit(1); },
          "any bound address reuse refuses before combat");
  check(authority(reused.session.snapshot()).draws.empty(),
        "reuse refusal never projects or draws combat");
}
void frozen_and_setup() {
  unsigned actor_runs = 0;
  auto hooks = base();
  hooks.is_frozen = {"frozen", [] { return true; }};
  hooks.on_run = {"observe", [&](O::GameObject &) { ++actor_runs; }};
  Fixture f(config(), 1, 0, hooks);
  f.task();
  f.submit(0, true);
  f.submit(1);
  f.submit(2);
  check(actor_runs == 0 && f.hits == 2 &&
            hp(f.session.snapshot()) == 0x42c00000 &&
            authority(f.session.snapshot()).draws.size() == 12,
        "frozen actors do not suppress explicit ready post-run combat task");
  Fixture late;
  late.submit(0, true);
  rejects([&] { late.session.configure_base_combat(late.ids[0]); },
          "combat configuration is setup-only once presentation starts");
  S::Session absent(config());
  absent.enable_objects(1, base());
  rejects([&] { absent.enable_base_combat(); },
          "combat requires installed existing HP/backend");
}
void legacy_and_fpu() {
  auto &ambient = R::set_fpu(R::Fpu::Model::Nearest, true, true);
  Fixture f(config(0));
  auto before = f.session.snapshot();
  f.session.draw();
  f.session.roll_damage(R::ONE, R::TWO);
  auto legacy = f.session.snapshot();
  check(legacy.world->shared_seed == before.world->shared_seed &&
            authority(legacy).draws.empty() && legacy.draws.size() == 2,
        "legacy Session operations remain on separate authored seed");
  f.task();
  f.submit(0, true);
  f.submit(1);
  check(ambient.model == R::Fpu::Model::Nearest && ambient.mul_bias &&
            ambient.div_nearest,
        "normal combat restores ambient arithmetic configuration");
  auto s = f.session.snapshot();
  check(s.seed == legacy.seed && s.draws == legacy.draws &&
            s.hp->adapter.world.cosmetic.draws.empty(),
        "shared combat does not advance legacy or cosmetic streams");
  Fixture bad(config(0));
  bad.task();
  bad.session.register_object_task(101, "failure", 130, 0, [](S::Session &) {
    throw std::runtime_error("authored");
  });
  bad.submit(0, true);
  rejects<std::runtime_error>([&] { bad.submit(1); },
                              "throw after combat propagates");
  check(ambient.model == R::Fpu::Model::Nearest && ambient.mul_bias &&
            ambient.div_nearest,
        "throw restores caller FPU");
}

void boundaries() {
  C::World standalone(1, 9);
  check(standalone.draw_shared() == 15496 &&
            standalone.seed.state == 0x3c88596c &&
            !standalone.shared_random_snapshot(),
        "unbound World preserves original standalone seed draw");
  R::SharedRandom source(1);
  standalone.bind_shared_random(source, "authored.shared");
  source.draw();
  check(standalone.draw_shared() == 24200 && source.state == 0x5e8885db,
        "borrowed authority observes interleaved external draw without state "
        "copy");
  auto old = standalone.shared_random_snapshot().value();
  source.draw();
  check(old.state == 0x5e8885db && old.draws.size() == 1 &&
            old.draws[0].value == 24200 && old.draws[0].state == 0x5e8885db &&
            standalone.shared_random_snapshot()->state == 0x8116017e &&
            standalone.seed.state == 0x3c88596c &&
            standalone.seed.draws.size() == 1 &&
            standalone.cosmetic.draws.empty(),
        "authority snapshot owns attributed pairs; fallback/cosmetic remain "
        "isolated");
  rejects<C::Error>([&] { standalone.bind_shared_random(source, "again"); },
                    "authority cannot silently rebind");
  Fixture duplicate;
  rejects([&] { duplicate.session.enable_base_combat(); },
          "mode cannot be enabled twice");
  Fixture config_twice;
  rejects(
      [&] { config_twice.session.configure_base_combat(config_twice.ids[0]); },
      "record configuration cannot silently change");
  Fixture missing(config(), 1, 0, base(), 2, {}, 0, 1);
  missing.task();
  missing.submit(0, true);
  rejects([&] { missing.submit(1); },
          "both identities require explicit configuration");
  check(authority(missing.session.snapshot()).draws.empty(),
        "missing configuration refuses before draw");
  Fixture frame;
  frame.task();
  frame.session.register_object_task(99, "frame", 20, 0, [](S::Session &x) {
    x.with_world([](O::World &w) { w.frame = INT64_C(0x100000000); });
  });
  frame.submit(0, true);
  rejects([&] { frame.submit(1); },
          "unrepresentable frame refuses before any combat draw");
  check(authority(frame.session.snapshot()).draws.empty() &&
            frame.session.snapshot().hp->adapter.model.writes.empty(),
        "bad frame retains no new combat or HP side effects");
  S::HpLimits limits;
  limits.calls_per_tick = 1;
  Fixture mixed(config(), 1, 0, base(), 2, limits);
  mixed.session.register_object_task(100, "mixed", 125, 0, [&](S::Session &x) {
    x.take_damage(mixed.ids[0], R::ONE, mixed.ids[1], 0x40);
    x.apply_base_damage(mixed.ids[0], mixed.ids[1], R::TWO, R::TWO, 100000);
  });
  mixed.submit(0, true);
  rejects<S::ResourceLimit>([&] { mixed.submit(1); },
                            "direct HP and combat share one ready-call cap");
  check(hp(mixed.session.snapshot()) == 0x42c60000 &&
            authority(mixed.session.snapshot()).draws.empty() &&
            mixed.session.snapshot().hp->calls_this_tick == 1,
        "shared cap retains first direct HP but prevents combat draw");
  for (auto flags : {C::CF_INVULNERABLE, UINT64_C(1) << 48}) {
    Fixture inert(config(), 1, 0, base(), 2, {}, flags);
    inert.task();
    inert.submit(0, true);
    inert.submit(1);
    check(hp(inert.session.snapshot()) == 0x42c80000 &&
              authority(inert.session.snapshot()).draws.size() ==
                  (flags == C::CF_INVULNERABLE ? 0u : 6u),
          "accepted invulnerable early return and IgnoreDamage six draws "
          "retain HP");
  }
  Fixture maximum;
  maximum.task(R::from_int(3), R::from_int(9));
  maximum.submit(0, true);
  auto before = maximum.session.snapshot();
  maximum.submit(1);
  auto after = maximum.session.snapshot();
  const auto history = [](const S::Snapshot &s) {
    const auto &m = s.hp->adapter.model;
    return m.writes.size() + m.stores.size() + m.calls.size() +
           m.events.size() + s.hp->adapter.world.events.size() +
           authority(s).draws.size();
  };
  check(history(after) - history(before) == 31,
        "maximum accepted ranged hit adds literal31 aggregate history entries");
  Fixture *active = nullptr;
  auto hooks = base();
  hooks.on_run = {"combat_in_run", [&](O::GameObject &) {
                    active->session.apply_base_damage(
                        active->ids[0], active->ids[1], R::ONE, R::ONE, 1);
                  }};
  Fixture gate(config(), 1, 0, hooks);
  active = &gate;
  gate.submit(0, true);
  rejects([&] { gate.submit(1); },
          "on_run combat refuses active iterator/early phase");
  auto failed = gate.session.snapshot();
  check(failed.failed && failed.world && failed.hp && !failed.hp->physical &&
            authority(failed).draws.empty(),
        "early callback refusal still retains semantic World and unavailable "
        "physical view");
}

void opposite_endpoints() {
  struct Projection {
    R::Raw hp, pending;
    std::uint32_t shared, combat;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> draws;
    std::uint32_t fallback, cosmetic;
    std::vector<std::uint32_t> fallback_draws, cosmetic_draws;
    std::int64_t frame;
    std::map<std::uint32_t, std::uint8_t> backing;
    bool operator==(const Projection &o) const {
      return hp == o.hp && pending == o.pending && shared == o.shared &&
             combat == o.combat && draws == o.draws && fallback == o.fallback &&
             cosmetic == o.cosmetic && fallback_draws == o.fallback_draws &&
             cosmetic_draws == o.cosmetic_draws && frame == o.frame &&
             backing == o.backing;
    }
  };
  struct Delivery {
    unsigned due, peer;
    N::ByteVector bytes;
  };
  std::array<std::unique_ptr<Fixture>, 2> hosts;
  std::array<std::map<std::int64_t, Projection>, 2> traces;
  std::vector<Delivery> deliveries;
  for (unsigned peer = 0; peer < 2; ++peer) {
    auto c = config(0, peer);
    hosts[peer] = std::make_unique<Fixture>(c);
    hosts[peer]->task();
    hosts[peer]->session.register_object_task(
        101, "record", 130, 0, [&, peer](S::Session &x) {
          auto snap = x.snapshot();
          std::vector<std::pair<std::uint32_t, std::uint32_t>> ledger;
          for (const auto &d : authority(snap).draws)
            ledger.emplace_back(d.value, d.state);
          Projection p{hp(snap),
                       R::Raw(word(snap, 0xc8)),
                       snap.world->shared_seed,
                       snap.seed,
                       ledger,
                       snap.hp->adapter.world.seed.state,
                       snap.hp->adapter.world.cosmetic.state,
                       snap.hp->adapter.world.seed.draws,
                       snap.hp->adapter.world.cosmetic.draws,
                       snap.world->frame,
                       snap.hp->adapter.model.backing};
          check(traces[peer].emplace(p.frame, p).second,
                "each endpoint records one HP state per completed tick");
        });
  }
  for (unsigned wall = 0;
       wall < 160 && (traces[0].size() < 12 || traces[1].size() < 12); ++wall)
    for (unsigned peer = 0; peer < 2; ++peer) {
      if (peer == 1 && wall % 2)
        continue;
      std::vector<N::ByteVector> inbox;
      for (auto it = deliveries.begin(); it != deliveries.end();) {
        if (it->peer == peer && it->due <= wall) {
          inbox.push_back(it->bytes);
          it = deliveries.erase(it);
        } else
          ++it;
      }
      auto result = hosts[peer]->session.submit(
          sample(), 0, 0, 1, traces[peer].size() >= 12, false, wall, inbox);
      for (const auto &p : result.sent) {
        check(p.peer >= 0 && p.peer < 2 && unsigned(p.peer) != peer,
              "real endpoint packet goes to opposite peer");
        deliveries.push_back(
            {wall + 1 + (peer == 0 ? 1U : 0U), unsigned(p.peer), p.data});
      }
    }
  check(traces[0].size() >= 12 && traces[1].size() >= 12,
        "two persistent combat sessions advance with delayed unequal cadence");
  R::SharedRandom expected(1);
  expected.draw();
  expected.draw();
  for (std::int64_t tick = 1; tick <= 12; ++tick) {
    for (unsigned i = 0; i < 6; ++i)
      expected.draw();
    const auto &a = traces[0].at(tick);
    const auto &b = traces[1].at(tick);
    check(a == b, "opposite local roles agree on complete HP backing at same "
                  "completed tick");
    check(a.hp == R::from_int(100 - 2 * tick) &&
              a.pending == R::from_int(2 * tick) &&
              a.shared == expected.state && a.combat == 0xabcdef01,
          "peer agreement also satisfies independent HP and RNG anchors");
  }
}
} // namespace
int main() {
  try {
    literal_authority();
    draw_branches();
    failure_prefix();
    readiness_and_domain();
    frozen_and_setup();
    legacy_and_fpu();
    boundaries();
    opposite_endpoints();
    std::cout << checks << " Session combat checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Session combat check " << checks << ": " << e.what() << '\n';
    return 1;
  }
}
