#include "champions/session.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <type_traits>
namespace R = champions::simulation::rta;
namespace S = R::session;
namespace O = R::objects;
namespace N = R::netwire;
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
S::Config config(unsigned cfg = 0) {
  S::Config c;
  c.network_mode = 1;
  c.peers = 2;
  c.local = 0;
  c.model = static_cast<R::Fpu::Model>(cfg / 4);
  c.mul_bias = cfg & 2;
  c.div_nearest = cfg & 1;
  c.seed = 0xabcdef01;
  c.network_seed = 123;
  return c;
}
S::ObjectBindings base() {
  S::ObjectBindings b;
  b.policy = O::CallbackPolicy::ReferenceBase;
  return b;
}
N::ByteVector sample(unsigned buttons = 1) {
  return {
      std::uint8_t(buttons), std::uint8_t(buttons >> 8), 128, 64, 255, 0, 5, 9};
}
N::ByteVector ready(const S::Config &c) {
  N::Machine sender(2, 1, N::Fpu(c.model, c.mul_bias, c.div_nearest));
  for (unsigned i = 1; i <= 16; ++i)
    sender.peers[1].set_record(
        i, N::rec12(i * 17, {129, 65, 254, 9}, {11, 13}, i * 123, i & 1), 0, 0);
  return sender.build_packet(0, 1, 16, N::PKT_RAW_RELAY, 1, nullptr, true).data;
}
unsigned task_events(const S::Snapshot &s, const std::string &name) {
  return unsigned(
      std::count_if(s.events.begin(), s.events.end(), [&](const S::Event &e) {
        return e.kind == "task" && e.name == name;
      }));
}
// Snapshot accessors are kept below the fixture so the tests inspect copied
// public state, never internal Adapter or World references.
std::uint64_t hp_word(const S::Snapshot &, unsigned, unsigned, unsigned = 4);
R::Raw hp_value(const S::Snapshot &, unsigned = 0);
std::size_t hp_writes(const S::Snapshot &);
bool physical_available(const S::Snapshot &);
struct Fixture {
  S::Session s;
  std::array<std::uint32_t, 3> addresses{};
  std::array<std::uint64_t, 3> serials{};
  std::array<S::HpId, 3> ids{};
  unsigned count = 2, hits = 0;
  explicit Fixture(S::Config c = config(), S::ObjectBindings bindings = base(),
                   unsigned actors = 2)
      : s(c), count(actors) {
    s.enable_objects(17, std::move(bindings));
    s.with_world([&](O::World &w) {
      for (unsigned i = 0; i < count; ++i) {
        auto *o = w.new_object(0x400, {R::from_int(i), 0, 0});
        addresses[i] = o->addr;
        serials[i] = o->serial;
        w.run_every_frame(*o);
      }
    });
    s.enable_hp();
    for (unsigned i = 0; i < count; ++i) {
      S::HpInitial initial;
      initial.hp = 0x42c80000;
      ids[i] = s.bind_hp(serials[i], addresses[i], initial);
    }
  }
  void task(std::int64_t priority = 125) {
    s.register_object_task(100, "authored_hp", priority, 0,
                           [this](S::Session &x) {
                             x.take_damage(ids[0], 0x40000000, ids[1], 0x40);
                             ++hits;
                           });
  }
  void submit(unsigned k, bool inbox = false) {
    s.submit(sample(), 0, 0, 1, false, false, k,
             inbox ? std::vector<N::ByteVector>{ready(config())}
                   : std::vector<N::ByteVector>{});
  }
};
void persistent_ready() {
  for (unsigned cfg = 0; cfg < 16; ++cfg) {
    Fixture f(config(cfg));
    f.task();
    const auto initial = f.s.snapshot();
    f.submit(0, true);
    auto zero = f.s.snapshot();
    check(f.hits == 0 && hp_value(zero) == 0x42c80000 && hp_writes(zero) == 0,
          "presentation without ready task makes no HP progress");
    f.submit(1);
    auto first = f.s.snapshot();
    check(f.hits == 1 && hp_value(first) == 0x42c40000 &&
              hp_word(first, 0, 0xc8) == 0x40000000,
          "first real ready task gives literal100 minus2 and pending2");
    check(physical_available(first),
          "ordinary runObjects leaves raw graph encodable for posttask HP");
    f.submit(2);
    auto second = f.s.snapshot();
    check(f.hits == 2 && hp_value(second) == 0x42c00000 &&
              hp_word(second, 0, 0xc8) == 0x40800000,
          "second hit reads persistent98 and produces96/pending4");
    check(hp_value(first) == 0x42c40000 && hp_value(initial) == 0x42c80000,
          "prior copied HP snapshots remain owned");
    check(second.seed == initial.seed && second.draws.empty() &&
              second.world->shared_seed == initial.world->shared_seed &&
              second.network.rand_seed == initial.world->shared_seed,
          "base HP adds no combat or shared RNG draws");
    check(task_events(second, "authored_hp") == 2 &&
              task_events(second, "runObjects") == 2,
          "one HP operation per actual scheduler callback");
  }
}
void readiness_cases() {
  for (unsigned which = 0; which < 5; ++which) {
    Fixture f;
    f.task();
    if (which == 3)
      f.s.set_level_pending(true);
    std::vector<unsigned> hits;
    for (unsigned k = 0; k < 4; ++k) {
      const bool deliver =
          which != 1 && ((which == 2 && k == 2) || (which != 2 && k == 0));
      f.s.submit(sample(), 0, 0, 1, which == 4 && k < 2, false, k,
                 deliver ? std::vector<N::ByteVector>{ready(config())}
                         : std::vector<N::ByteVector>{});
      hits.push_back(f.hits);
    }
    const unsigned expected[5][4] = {
        {0, 1, 2, 3}, {0, 0, 0, 0}, {0, 0, 1, 2}, {0, 0, 0, 0}, {0, 0, 1, 2}};
    check(hits == std::vector<unsigned>(expected[which], expected[which] + 4),
          "ready/delayed/no-ready/pending/no_sim follow actual task count");
    check(hp_value(f.s.snapshot()) ==
              R::sub(0x42c80000, R::from_int(2 * f.hits)),
          "blocked frames preserve exact accumulated HP");
  }
  {
    Fixture f;
    f.task();
    f.s.submit(sample(), 0, 0, 4, true, false, 0, {ready(config())});
    check(f.hits == 0, "no_sim forms input backlog without HP");
    auto r = f.s.submit(sample(), 0, 0, 1, false, true, 1);
    check(r.held && f.hits > 0,
          "held presentation may process ready HP backlog");
  }
  {
    unsigned runs = 0;
    auto b = base();
    b.on_run = {"run", [&](O::GameObject &) { ++runs; }};
    b.is_frozen = {"frozen", [] { return true; }};
    Fixture f(config(), b);
    f.task();
    f.submit(0, true);
    f.submit(1);
    check(runs == 0 && f.hits == 1 && hp_value(f.s.snapshot()) == 0x42c40000,
          "frozen actor updates do not suppress explicit posttask HP");
  }
}
void setup_and_phase() {
  for (const auto p : {"norrath", "unknown"}) {
    auto c = config();
    c.profile = p;
    rejects([&] { S::Session s(c); }, "title profile refuses unsupported game");
  }
  {
    S::Session s(config());
    rejects([&] { s.enable_hp(); }, "HP requires existing Session-owned World");
  }
  {
    Fixture f;
    rejects([&] { f.s.enable_hp(); }, "duplicate HP installation refuses");
  }
  {
    Fixture f;
    rejects([&] { f.s.take_damage(f.ids[0], R::ONE, f.ids[1], 0); },
            "HP outside actual ready task refuses");
    check(f.s.failed() && hp_writes(f.s.snapshot()) == 0,
          "outside-phase failure latches without HP effects");
  }
  {
    Fixture f;
    rejects([&] { f.s.bind_hp(f.serials[0], f.addresses[0]); },
            "duplicate bound generation refuses");
  }
  {
    Fixture f;
    rejects([&] { f.s.bind_hp(f.serials[0] + 999, f.addresses[0]); },
            "raw address alone does not certify serial");
  }
  {
    S::Session s(config());
    s.enable_objects(1, base());
    std::uint32_t a = 0;
    std::uint64_t serial = 0;
    s.with_world([&](O::World &w) {
      auto *o = w.new_object(0x180, {0, 0, 0});
      a = o->addr;
      serial = o->serial;
    });
    s.enable_hp();
    rejects([&] { s.bind_hp(serial, a); },
            "small generic allocation cannot back Creature fields");
  }
  {
    Fixture f;
    f.task(9);
    f.submit(0, true);
    rejects([&] { f.submit(1); }, "HP before actual runObjects refuses");
    check(hp_writes(f.s.snapshot()) == 0,
          "early-task refusal has no HP writes");
  }
  {
    S::Session *active = nullptr;
    S::HpId victim = 0, attacker = 0;
    auto b = base();
    b.on_run = {"forbidden_hp", [&](O::GameObject &) {
                  active->take_damage(victim, R::ONE, attacker, 0);
                }};
    Fixture f(config(), b);
    active = &f.s;
    victim = f.ids[0];
    attacker = f.ids[1];
    f.task();
    f.submit(0, true);
    rejects([&] { f.submit(1); }, "on_run HP refuses active iterator phase");
    auto snap = f.s.snapshot();
    check(snap.failed && snap.world && snap.world->objects.size() == 2 &&
              hp_writes(snap) == 0,
          "phase failure retains semantic World and HP state");
    check(
        !physical_available(snap),
        "failed on_run graph reports unavailable physical snapshot explicitly");
  }
}
void failures_and_reuse() {
  {
    Fixture f;
    f.task();
    f.s.register_object_task(101, "later_throw", 130, 0, [](S::Session &) {
      throw std::runtime_error("authored later task");
    });
    f.submit(0, true);
    rejects<std::runtime_error>([&] { f.submit(1); },
                                "later task throws after completed HP");
    auto before = f.s.snapshot();
    check(before.failed && hp_value(before) == 0x42c40000 && f.hits == 1,
          "completed HP retained on later-task failure");
    rejects<S::Failed>([&] { f.submit(2); }, "failed Session never retries HP");
    auto after = f.s.snapshot();
    check(hp_writes(before) == hp_writes(after) &&
              hp_value(after) == 0x42c40000 && f.hits == 1,
          "retry refusal adds no HP effects");
  }
  {
    auto b = base();
    b.end_of_tick = {"end_throw",
                     [] { throw std::runtime_error("authored end"); }};
    Fixture f(config(), b);
    f.task();
    f.submit(0, true);
    rejects<std::runtime_error>([&] { f.submit(1); },
                                "end_of_tick throws after scheduler HP task");
    auto snap = f.s.snapshot();
    check(hp_value(snap) == 0x42c40000 && f.hits == 1 &&
              snap.world->pads[0].pressed == 0,
          "end failure retains completed HP and cleared edge prefix");
  }
  {
    Fixture f;
    f.s.register_object_task(99, "exhaust", 15, 0, [](S::Session &x) {
      x.with_world([](O::World &w) {
        const auto free = w.handles.count_free();
        for (std::size_t i = 1; i < free; ++i)
          w.handles.add(w.always_draw, nullptr);
      });
    });
    f.task();
    f.submit(0, true);
    rejects<R::HandleError>(
        [&] { f.submit(1); },
        "actual6096 pool second-take exhaustion propagates");
    auto snap = f.s.snapshot();
    check(snap.failed && hp_value(snap) == 0x42c80000 && hp_writes(snap) > 0 &&
              snap.world->handle_free == 0,
          "failed HP retains first allocation and write prefix, no final "
          "subtract");
  }
  {
    Fixture f(config(), base(), 3);
    f.s.register_object_task(99, "reuse_unrelated", 15, 0, [&](S::Session &x) {
      x.with_world([&](O::World &w) {
        w.destroy(*w.objects.at(f.addresses[2]), 3);
        auto *replacement = w.new_object(0x400, {0, 0, 0});
        check(replacement->addr == f.addresses[2] &&
                  replacement->serial != f.serials[2],
              "authored unrelated bound address genuinely reused");
      });
    });
    f.task();
    f.submit(0, true);
    rejects([&] { f.submit(1); },
            "ANY bound address reuse blocks projection into unrelated victim");
    check(hp_writes(f.s.snapshot()) == 0 &&
              hp_value(f.s.snapshot()) == 0x42c80000,
          "all-binding generation refusal occurs before project/HP writes");
  }
}
std::uint64_t hp_word(const S::Snapshot &s, unsigned record, unsigned offset,
                      unsigned width) {
  const auto &h = s.hp.value();
  const auto address = h.bindings.at(record).address + offset;
  std::uint64_t value = 0;
  for (unsigned i = 0; i < width; ++i) {
    const auto p = h.adapter.model.backing.find(address + i);
    if (p != h.adapter.model.backing.end())
      value |= std::uint64_t(p->second) << (8 * i);
  }
  return value;
}
R::Raw hp_value(const S::Snapshot &s, unsigned record) {
  return s.hp.value().adapter.objects.at(record).value.hp;
}
std::size_t hp_writes(const S::Snapshot &s) {
  return s.hp.value().adapter.model.writes.size();
}
bool physical_available(const S::Snapshot &s) {
  return s.hp && s.hp->physical.has_value();
}
void owned_adapter_snapshot() {
  namespace B = R::combat_creature;
  namespace C = R::combat;
  B::Snapshot kept;
  {
    B::Adapter adapter(17, 19, R::creature_hp::CallbackPolicy::ReferenceZero);
    auto &a = adapter.create(0x10000), &b = adapter.create(0x20000);
    a.tag = U"authored";
    a.hp = 0x42c80000;
    a.owner = &b;
    a.status_source = &b;
    a.last_attacker = &a;
    C::Spell spell;
    spell.kind = U"Authored";
    spell.owner = &b;
    spell.f20 = 0x40000000;
    auto *sp = adapter.world().add_owned_spell(spell);
    a.spells = {sp, nullptr, sp};
    C::ItemDef definition;
    definition.name = U"author sword";
    definition.attack = 7;
    C::Item item;
    item.defn = &definition;
    item.mods[0] = {23, 4, 5, 6};
    C::PlayerRec player;
    player.obj = &a;
    player.bag2 = {&item, nullptr};
    player.equip[0] = &item;
    player.weapon_sets[1] = &item;
    player.second_weapon = &item;
    player.skills = {{1, 2}};
    adapter.world().players.push_back(player);
    adapter.world().owned_spells.push_back(nullptr);
    unsigned invoked = 0;
    adapter.world().stat_fn = [&](auto, auto, auto) {
      ++invoked;
      return 5;
    };
    kept = adapter.snapshot();
    check(invoked == 0 && kept.world.callbacks[0] && kept.world.callbacks[5],
          "snapshot records callbacks without executing or copying closures");
    check(kept.objects.size() == 2 && kept.spells.size() == 1 &&
              kept.items.size() == 1 && kept.definitions.size() == 1,
          "nonempty identity tables deduplicate shared pointers");
    const auto &o = kept.objects[0];
    check(!o.value.owner && !o.value.status_source && !o.value.last_attacker &&
              std::all_of(o.value.spells.begin(), o.value.spells.end(),
                          [](auto *p) { return p == nullptr; }),
          "object snapshot clears every host pointer");
    check(o.owner == 1 && o.status_source == 1 && o.last_attacker == 0 &&
              o.spells.size() == 3 && o.spells[0] == 0 && !o.spells[1] &&
              o.spells[2] == 0,
          "object snapshot preserves null and repeated identity indices");
    const auto &p = kept.world.players[0];
    check(!p.value.obj && !p.value.second_weapon &&
              std::all_of(p.value.bag2.begin(), p.value.bag2.end(),
                          [](auto *q) { return !q; }) &&
              std::all_of(p.value.equip.begin(), p.value.equip.end(),
                          [](auto *q) { return !q; }) &&
              std::all_of(p.value.weapon_sets.begin(),
                          p.value.weapon_sets.end(),
                          [](auto *q) { return !q; }),
          "player snapshot clears all pointer-bearing collections");
    check(p.object == 0 && p.second_weapon == 0 && p.bag2.size() == 2 &&
              p.bag2[0] == 0 && !p.bag2[1] && p.equip[0] == 0 &&
              p.weapon_sets[1] == 0,
          "player table links preserve aliases without host pointers");
    check(!kept.items[0].value.defn && kept.items[0].definition == 0 &&
              !kept.spells[0].value.owner && kept.spells[0].owner == 1,
          "item and spell snapshots use owned indexed identities");
    check(kept.world.owned_spells.size() == 2 &&
              kept.world.owned_spells[0] == 0 && !kept.world.owned_spells[1],
          "owned-spell registry retains null entry and identity");
    a.hp = 0;
    sp->f20 = 0;
    definition.attack = 999;
    item.mods[0][1] = 777;
    adapter.world().players[0].skills.clear();
    check(kept.objects[0].value.hp == 0x42c80000 &&
              kept.spells[0].value.f20 == 0x40000000 &&
              kept.definitions[0].attack == 7 &&
              kept.items[0].value.mods[0][1] == 4 &&
              !kept.world.players[0].value.skills.empty(),
          "snapshot values remain independent of later live mutations");
    B::Snapshot callback_snapshot;
    adapter.set_before_base(
        "observe", [&](B::Adapter &x, C::Obj &, R::Raw, C::Obj *,
                       std::uint32_t) { callback_snapshot = x.snapshot(); });
    adapter.take_damage(a, R::ONE, &b, 0);
    check(callback_snapshot.operation_depth == 1 &&
              callback_snapshot.raw_callback_depth == 1 &&
              callback_snapshot.callback_frames.size() == 1 &&
              callback_snapshot.callback_frames[0].expected.size() == 2,
          "snapshot owns active callback projection frame and depth state");
    check(callback_snapshot.callback_frames[0].expected[0].hp == 0 &&
              adapter.snapshot().callback_frames.empty(),
          "active projection baseline retained after callback guard exits");
    C::Obj foreign;
    a.owner = &foreign;
    rejects<B::Error>(
        [&] { adapter.snapshot(); },
        "foreign object link refuses snapshot instead of losing identity");
    a.owner = nullptr;
    C::Spell foreign_spell;
    foreign_spell.owner = &foreign;
    a.spells = {&foreign_spell};
    rejects<B::Error>(
        [&] { adapter.snapshot(); },
        "foreign spell owner refuses before incomplete snapshot escape");
  }
  check(kept.objects[0].value.tag == U"authored" &&
            kept.definitions[0].name == U"author sword",
        "snapshot remains owned after all live records are destroyed");
}
void opposite_endpoints() {
  struct Projection {
    R::Raw hp, pending;
    std::uint32_t shared, combat;
    std::int64_t frame;
    std::map<std::uint32_t, std::uint8_t> backing;
    bool operator==(const Projection &o) const {
      return hp == o.hp && pending == o.pending && shared == o.shared &&
             combat == o.combat && frame == o.frame && backing == o.backing;
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
    auto c = config();
    c.local = peer;
    hosts[peer] = std::make_unique<Fixture>(c);
    hosts[peer]->task();
    hosts[peer]->s.register_object_task(
        101, "record", 130, 0, [&, peer](S::Session &x) {
          auto snap = x.snapshot();
          Projection p{hp_value(snap),          R::Raw(hp_word(snap, 0, 0xc8)),
                       snap.world->shared_seed, snap.seed,
                       snap.world->frame,       snap.hp->adapter.model.backing};
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
      auto result = hosts[peer]->s.submit(
          sample(), 0, 0, 1, traces[peer].size() >= 12, false, wall, inbox);
      for (const auto &p : result.sent) {
        check(p.peer >= 0 && p.peer < 2 && unsigned(p.peer) != peer,
              "real endpoint packet goes to opposite peer");
        deliveries.push_back(
            {wall + 1 + (peer == 0 ? 1U : 0U), unsigned(p.peer), p.data});
      }
    }
  check(traces[0].size() >= 12 && traces[1].size() >= 12,
        "two persistent HP sessions advance with delayed unequal cadence");
  R::SharedRandom expected(17);
  expected.draw();
  expected.draw();
  for (std::int64_t tick = 1; tick <= 12; ++tick) {
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
void focused_boundaries() {
  auto &ambient = R::set_fpu(R::Fpu::Model::Nearest, true, true);
  Fixture good(config(0));
  good.task();
  good.submit(0, true);
  good.submit(1);
  check(ambient.model == R::Fpu::Model::Nearest && ambient.mul_bias &&
            ambient.div_nearest,
        "HP success restores caller arithmetic context");
  auto first = good.s.snapshot();
  const auto &m = first.hp->adapter.model;
  check(m.writes.size() + m.stores.size() + m.calls.size() + m.events.size() +
                first.hp->adapter.world.events.size() ==
            23,
        "literal fixed-backend positive hit emits23 aggregate history entries");
  Fixture bad(config(0));
  bad.s.register_object_task(123, "throw_after_hp", 125, 0, [&](S::Session &x) {
    x.take_damage(bad.ids[0], R::TWO, bad.ids[1]);
    throw std::runtime_error("authored");
  });
  bad.submit(0, true);
  rejects<std::runtime_error>([&] { bad.submit(1); },
                              "throw after actual HP preserves error");
  check(ambient.model == R::Fpu::Model::Nearest && ambient.mul_bias &&
            ambient.div_nearest,
        "HP throw restores caller arithmetic context");
  Fixture routed;
  routed.s.with_world(
      [&](O::World &w) { w.objects.at(routed.addresses[1])->flags |= 0x80; });
  routed.s.link_hp_head(routed.ids[1], 0x4c, routed.addresses[0]);
  routed.s.register_object_task(120, "owner_route", 125, 0, [&](S::Session &x) {
    const auto during = x.snapshot();
    check(during.hp->object_run_completed && during.hp->task_priority == 125,
          "owned snapshot retains actual post-run HP permission state");
    x.take_damage(routed.ids[0], R::TWO, routed.ids[1], 0x40);
  });
  routed.submit(0, true);
  routed.submit(1);
  auto route = routed.s.snapshot();
  check(route.hp->adapter.model.calls.size() == 2 &&
            route.hp->adapter.model.calls[1].args ==
                std::vector<std::uint32_t>{routed.addresses[0] + 0x2e4,
                                           routed.addresses[0],
                                           routed.addresses[0]},
        "explicit projectile owner head routes actual HP through the existing "
        "World pool");
  check(route.hp->object_run_completed && !route.hp->task_priority &&
            hp_value(route) == 0x42c40000,
        "posttask exits priority guard but retains completed-run diagnostic "
        "and HP");
  Fixture fractional(config(0));
  fractional.s.register_object_task(
      100, "fractional", 125, 0, [&](S::Session &x) {
        x.take_damage(fractional.ids[0], 0x3f000001, fractional.ids[1], 0x40);
      });
  fractional.submit(0, true);
  const std::array<R::Raw, 3> expected_fractional{0x42c6ffff, 0x42c5fffe,
                                                  0x42c4fffd};
  for (unsigned k = 0; k < 3; ++k) {
    fractional.submit(k + 1);
    check(
        hp_value(fractional.s.snapshot()) == expected_fractional[k],
        "fractional PS2 subtraction matches independent literal raw patterns");
  }
  Fixture other;
  other.s.register_object_task(100, "authored_hp", 125, 0, [&](S::Session &x) {
    x.take_damage(other.ids[0], R::ONE, other.ids[1], 0x40);
  });
  other.submit(0, true);
  other.submit(1);
  auto second = other.s.snapshot();
  check(N::sync_hash(
            first.network.rand_seed, first.network.sync_acc,
            {first.network.positions.begin(), first.network.positions.end()}) ==
                N::sync_hash(second.network.rand_seed, second.network.sync_acc,
                             {second.network.positions.begin(),
                              second.network.positions.end()}) &&
            hp_value(first) != hp_value(second) &&
            first.hp->adapter.model.backing != second.hp->adapter.model.backing,
        "same wire hash does not identify complete persistent HP state");
  for (unsigned mode = 0; mode < 3; ++mode) {
    S::Session x(config());
    x.enable_objects(1, base());
    std::uint64_t serial;
    std::uint32_t address;
    x.with_world([&](O::World &w) {
      auto *o = w.new_object(0x400, {0, 0, 0});
      serial = o->serial;
      address = o->addr;
    });
    S::HpLimits limits;
    limits.memory.memory_bytes = mode == 2 ? 51 : 56;
    x.enable_hp(limits);
    S::HpInitial initial;
    initial.cflags = UINT64_MAX;
    initial.hp = R::ONE;
    if (mode == 2) {
      rejects<S::ResourceLimit>(
          [&] { x.bind_hp(serial, address, initial); },
          "bind missing scalar capacity refuses before publishing record");
      auto a = x.snapshot();
      check(a.hp->bindings.empty() && a.hp->adapter.objects.empty() &&
                a.hp->registry.heads.empty(),
            "failed bind capacity leaves all registries empty");
      continue;
    }
    auto id = x.bind_hp(serial, address, initial);
    auto a = x.snapshot();
    check(a.hp->adapter.objects[0].value.cflags == UINT64_MAX &&
              hp_word(a, 0, 0xa8, 8) == UINT64_MAX,
          "high-bit cflags preserved exactly in typed and raw observations");
    if (mode == 0) {
      x.seed_hp_backing(id, {{0xc8, 4, 0x12345678}, {0xca, 2, 0xabcd}});
      auto b = x.snapshot();
      check(b.hp->adapter.model.backing.size() == 56 &&
                hp_word(b, 0, 0xc8) == 0xabcd5678,
            "overlapping writes count unique new bytes and preserve supplied "
            "order");
    } else {
      rejects<S::ResourceLimit>(
          [&] { x.seed_hp_backing(id, {{0xc8, 4, 1}, {0x231, 1, 1}}); },
          "combined missing-byte overflow refuses all setup writes");
      auto b = x.snapshot();
      check(b.hp->adapter.model.backing == a.hp->adapter.model.backing &&
                b.hp->adapter.model.stores.empty(),
            "capacity failure has no accepted prefix or invented store log");
    }
  }
  Fixture capped;
  for (unsigned i = 0; i < 31700; ++i)
    capped.s.draw();
  unsigned completed = 0;
  capped.s.register_object_task(
      123, "near_history", 125, 0, [&](S::Session &x) {
        for (unsigned i = 0; i < 64; ++i) {
          x.take_damage(capped.ids[0], R::ONE, capped.ids[1]);
          ++completed;
        }
      });
  capped.submit(0, true);
  rejects<S::ResourceLimit>(
      [&] { capped.submit(1); },
      "aggregate near-cap HP task refuses within history reserve");
  auto cap = capped.s.snapshot();
  check(completed > 0 && completed < 64 && cap.failed &&
            cap.hp->calls_this_tick == completed,
        "near-cap failure retains completed calls and usable full snapshot");
  auto writes = hp_writes(cap);
  rejects([&] { capped.submit(2); }, "near-cap latch forbids retry");
  check(hp_writes(capped.s.snapshot()) == writes,
        "near-cap retry creates no HP writes");
}

} // namespace
int main() {
  try {
    persistent_ready();
    readiness_cases();
    setup_and_phase();
    failures_and_reuse();
    owned_adapter_snapshot();
    opposite_endpoints();
    focused_boundaries();
    std::cout << checks << " Session HP checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Session HP check " << checks << ": " << e.what() << '\n';
    return 1;
  }
}
