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
void populate(S::Session &s) {
  s.with_world([](O::World &w) {
    auto *o = w.new_object(0x180, {0, 0, 0});
    w.run_every_frame(*o);
  });
}
unsigned task_events(const S::Snapshot &s, const std::string &name) {
  return unsigned(
      std::count_if(s.events.begin(), s.events.end(), [&](const S::Event &e) {
        return e.kind == "task" && e.name == name;
      }));
}
void setup_contract() {
  for (const auto profile : {"norrath", "unknown", "RTA", ""}) {
    auto c = config();
    c.profile = profile;
    rejects([&] { S::Session s(c); }, "unsupported profile refused");
  }
  {
    auto c = config();
    c.network_mode = 0;
    S::Session s(c);
    rejects([&] { s.enable_objects(1, base()); },
            "objects requires network mode");
  }
  {
    S::Session s(config());
    s.register_task("legacy", [](S::Session &) {});
    rejects([&] { s.enable_objects(1, base()); },
            "legacy then objects mixing refused");
  }
  {
    S::Session s(config());
    s.enable_objects(1, base());
    rejects([&] { s.register_task("legacy", [](S::Session &) {}); },
            "objects then legacy mixing refused");
  }
  {
    S::Session s(config());
    s.enable_objects(1, base());
    rejects([&] { s.enable_objects(2, base()); },
            "duplicate mode enable refused");
  }
  {
    S::Session s(config());
    rejects([&] { s.with_world([](O::World &) {}); },
            "world access before enable refused");
  }
  {
    S::Session s(config());
    s.register_task("existing", [](S::Session &) {});
    s.submit(sample());
    rejects([&] { s.enable_objects(1, base()); },
            "enable after submit refused");
  }
  for (auto id : {S::OBJECT_RUN_TASK, S::OBJECT_DRAW_TASK}) {
    S::Session s(config());
    s.enable_objects(1, base());
    rejects(
        [&] {
          s.register_object_task(id, "reserved", 0, 0, [](S::Session &) {});
        },
        "reserved original task identity refused");
  }
  {
    S::Session s(config());
    s.enable_objects(1, base());
    auto snap = s.snapshot();
    check(snap.world.has_value() && snap.world->tasks.tasks.size() == 1,
          "only builtin runObjects task installed");
    check(snap.world->tasks.tasks[0].fn == S::OBJECT_RUN_TASK &&
              snap.world->tasks.tasks[0].priority == 10 &&
              snap.world->tasks.tasks[0].flags == 0,
          "builtin task priority and flags");
    check(snap.tasks.empty() && snap.network_before_frame_callback,
          "object mode has pre-frame hook and no legacy tasks");
    check(!snap.network.before_frame,
          "snapshot cannot invoke live pre-frame callback");
  }
}
void ownership_and_seeds() {
  S::Session s(config());
  s.enable_objects(0x12345678, base());
  auto initial = s.snapshot();
  check(initial.world->shared_seed == 0x12345678 &&
            initial.network.rand_seed == 0x12345678 &&
            initial.seed == 0xabcdef01,
        "explicit shared stream independent of combat seed");
  populate(s);
  R::SharedRandom expected(0x12345678);
  expected.draw();
  auto after = s.snapshot();
  check(after.world->shared_seed == expected.state &&
            after.network.rand_seed == expected.state,
        "constructor shared draw published to wire state");
  check(after.seed == initial.seed, "object construction leaves combat seed");
  auto wire = after.network.rand_seed;
  s.draw();
  after = s.snapshot();
  check(after.network.rand_seed == wire && after.world->shared_seed == wire &&
            after.seed != initial.seed,
        "combat draw leaves shared world/wire stream");
  auto owned = s.snapshot();
  owned.world->objects[0].pos[0] = 99;
  owned.world->name_count = -100;
  owned.world->pool.link.clear();
  check(s.snapshot().world->objects[0].pos[0] == 0 &&
            s.snapshot().world->name_count == 0 &&
            !s.snapshot().world->pool.link.empty(),
        "world snapshot deeply owned");
  const auto before = s.snapshot();
  s.with_world([](O::World &w) { w.objects.begin()->second->tag = 777; });
  const auto changed = s.snapshot();
  check(before.network.rand_seed == changed.network.rand_seed &&
            before.network.sync_acc == changed.network.sync_acc &&
            before.network.positions == changed.network.positions,
        "wire hash inputs unchanged by object tag");
  check(before.world->objects[0].tag != changed.world->objects[0].tag,
        "full snapshot distinguishes same wire hash different world");
}
void ready_boundaries() {
  for (unsigned which = 0; which < 5; ++which) {
    auto c = config();
    S::Session s(c);
    unsigned runs = 0, ends = 0;
    auto b = base();
    b.on_run = {"count", [&](O::GameObject &) { ++runs; }};
    b.end_of_tick = {"end", [&] { ++ends; }};
    s.enable_objects(9, b);
    populate(s);
    if (which == 3)
      s.set_level_pending(true);
    std::vector<unsigned> counts;
    for (unsigned k = 0; k < 4; ++k) {
      auto inbox =
          (which != 1 && ((which == 2 && k == 2) || (which != 2 && k == 0)))
              ? std::vector<N::ByteVector>{ready(c)}
              : std::vector<N::ByteVector>{};
      s.submit(sample(k + 1), 0, 0, 1, which == 4 && k < 2, false, k, inbox);
      counts.push_back(runs);
    }
    constexpr unsigned expected[5][4] = {
        {0, 1, 2, 3}, {0, 0, 0, 0}, {0, 0, 1, 2}, {0, 0, 0, 0}, {0, 0, 1, 2}};
    check(counts == std::vector<unsigned>(expected[which], expected[which] + 4),
          "ready/delayed/stalled/pending/no_sim actual callback count");
    check(runs == ends && task_events(s.snapshot(), "runObjects") == runs,
          "world tasks once per actual execution");
  }
}
std::uint32_t word(const N::Pad &p, unsigned offset) {
  return std::uint32_t(p[offset]) | (std::uint32_t(p[offset + 1]) << 8) |
         (std::uint32_t(p[offset + 2]) << 16) |
         (std::uint32_t(p[offset + 3]) << 24);
}
void mutation_and_failure() {
  {
    auto c = config();
    S::Session s(c);
    unsigned runs = 0, late = 0, draw = 0;
    auto b = base();
    b.on_run = {"run", [&](O::GameObject &) { ++runs; }};
    s.enable_objects(4, b);
    s.register_object_task(100, "prepare", 0, 0, [&](S::Session &host) {
      host.with_world([&](O::World &w) {
        if (w.objects.empty()) {
          auto *o = w.new_object(0x180, {0, 0, 0});
          w.run_every_frame(*o);
          w.tasks.add(200, 20, "late", 0);
        }
      });
    });
    s.register_object_task(200, "late", 20, 0, [&](S::Session &) { ++late; });
    s.with_world([](O::World &w) { w.tasks.remove(200); });
    s.register_object_task(300, "draw", 21, 1, [&](S::Session &) { ++draw; });
    s.submit(sample(), 0, 0, 1, false, false, 0, {ready(c)});
    s.submit(sample());
    check(runs == 1 && late == 1 && draw == 0,
          "pre task creates runnable object and later task same tick; draw "
          "excluded");
    auto snap = s.snapshot();
    check(task_events(snap, "runObjects") == 1 &&
              task_events(snap, "prepare") == 1 &&
              task_events(snap, "late") == 1,
          "no duplicated task dispatch");
  }
  {
    auto c = config();
    S::Session s(c);
    s.enable_objects(3);
    populate(s);
    s.submit(sample(), 0, 0, 1, false, false, 0, {ready(c)});
    rejects<O::ObjectError>(
        [&] { s.submit(sample()); },
        "missing required object callback refuses when invoked");
    check(s.failed() && s.snapshot().world.has_value(),
          "missing handler latches with observable world");
  }
  for (unsigned which = 0; which < 2; ++which) {
    auto c = config();
    S::Session s(c);
    unsigned calls = 0;
    auto b = base();
    if (which == 0)
      b.on_run = {"throw_run", [&](O::GameObject &o) {
                    ++calls;
                    o.tag = 91;
                    throw std::runtime_error("authored run");
                  }};
    else
      b.end_of_tick = {"throw_end", [&] {
                         ++calls;
                         s.with_world([](O::World &w) {
                           w.menu_lockout = 19;
                           w.pads[0].pressed = 77;
                         });
                         throw std::runtime_error("authored end");
                       }};
    s.enable_objects(5, b);
    populate(s);
    s.submit(sample(), 0, 0, 1, false, false, 0, {ready(c)});
    rejects<std::runtime_error>([&] { s.submit(sample()); },
                                "object callback failure propagates");
    auto snap = s.snapshot();
    check(s.failed() && calls == 1 && snap.network.net_frame == 1,
          "callback failure latches unadvanced frame");
    if (which == 0)
      check(snap.world->objects[0].tag == 91,
            "run partial object mutation retained");
    else
      check(snap.world->pads[0].pressed == 77 &&
                word(snap.network.pads[0], 12) == 77,
            "endtick partial pad mutation published");
    rejects<S::Failed>([&] { s.submit(sample()); },
                       "failed callback cannot retry");
    check(calls == 1, "failed callback never repeated");
  }
  for (auto value : {std::int64_t(-1), std::int64_t(0x100000001ULL)}) {
    S::Session s(config());
    s.enable_objects(6, base());
    rejects<S::Error>(
        [&] {
          s.with_world([&](O::World &w) {
            w.pads[0].pressed = value;
            w.frame = 42;
            w.shared.state = 7;
          });
        },
        "out of raw32 range pad mutation refused");
    auto snap = s.snapshot();
    check(snap.world->pads[0].pressed == value &&
              word(snap.network.pads[0], 12) == std::uint32_t(value),
          "wide pad failure retains full World and low32 network values");
    check(snap.world->frame == 42 && snap.network.sim_frame == 42 &&
              snap.network.rand_seed == 7,
          "failure publishes actual frame and shared seed");
    rejects<S::Failed>([&] { s.with_world([](O::World &) {}); },
                       "pad failure no retry");
    check(s.snapshot().world->pads[0].pressed == value,
          "retry does not normalize failed pad");
  }
}
void nested_scope_and_guards() {
  {
    S::Session s(config());
    s.enable_objects(9, base());
    s.with_world([&](O::World &w) {
      w.pads[0].pressed = 123;
      w.shared.state = 456;
      s.with_world([&](O::World &nested) {
        check(&nested == &w && nested.pads[0].pressed == 123 &&
                  nested.shared.state == 456,
              "nested World access never reimports stale pads or seed");
        nested.pads[0].edge2 = 789;
      });
    });
    auto snap = s.snapshot();
    check(word(snap.network.pads[0], 12) == 123 &&
              word(snap.network.pads[0], 16) == 789 &&
              snap.network.rand_seed == 456,
          "outer World scope publishes nested mutations");
  }
  {
    S::Session s(config());
    s.enable_objects(1, base());
    rejects([&] { s.with_world([&](O::World &) { s.submit(sample()); }); },
            "submission inside World scope refused");
  }
  {
    S::Session s(config());
    s.enable_objects(1, base());
    rejects(
        [&] {
          s.with_world([](O::World &w) { w.on_run = [](O::GameObject &) {}; });
        },
        "unregistered callback replacement refused");
  }
  {
    S::Session s(config());
    s.enable_objects(1, base());
    rejects(
        [&] {
          s.with_world([](O::World &w) { w.tasks.add(999, 0, "unknown", 0); });
        },
        "unregistered scheduler handler refused");
  }
  {
    S::Session s(config());
    s.enable_objects(1, base());
    rejects([&] { s.set_sync_state(0, 0, {}, 99); },
            "caller cannot replace object shared seed through sync API");
  }
}
void peer_message_order() {
  auto c = config();
  S::Session s(c);
  s.enable_objects(11, base());
  std::vector<std::array<std::int64_t, 5>> messages;
  s.register_message(
      7, "ordered", [&](S::Session &host, const N::ByteVector &payload) {
        host.with_world([&](O::World &w) {
          auto net = host.network();
          check(w.frame == net.net_frame && net.sim_frame == net.net_frame,
                "frame assigned before peer message");
          messages.push_back({payload.at(0), w.frame, w.pads[0].held,
                              w.pads[1].held, w.pads[0].pressed});
          if (payload.at(0) == 1)
            w.pads[0].pressed = 0x12345678;
        });
      });
  s.queue_message(7, {1});
  N::Machine sender(2, 1, N::Fpu(c.model, c.mul_bias, c.div_nearest));
  sender.peers[1].msg_ring[0] = 7;
  sender.peers[1].msg_ring[1] = 1;
  sender.peers[1].msg_ring[2] = 2;
  for (unsigned f = 1; f <= 16; ++f)
    sender.peers[1].set_record(
        f, N::rec12(f * 17, {129, 65, 254, 9}, {11, 13}, f * 123, f & 1), 0,
        f == 3 ? 3 : 0);
  auto packet =
      sender.build_packet(0, 1, 16, N::PKT_RAW_RELAY, 1, nullptr, true).data;
  unsigned task_seen = 0;
  s.register_object_task(100, "after_messages", 0, 0, [&](S::Session &host) {
    host.with_world([&](O::World &w) {
      if (w.frame == 3) {
        ++task_seen;
        check(messages.size() == 2 && w.pads[0].pressed == 0x12345678,
              "all peer messages precede tasks and earlier peer mutation "
              "survives");
      }
    });
  });
  for (unsigned k = 0; k < 4; ++k)
    s.submit(sample(k + 1), 0, 0, 1, false, false, k,
             k == 0 ? std::vector<N::ByteVector>{packet}
                    : std::vector<N::ByteVector>{});
  check(messages.size() == 2 && messages[0][0] == 1 && messages[1][0] == 2,
        "peer messages dispatched slot order");
  check(messages[0][1] == 3 && messages[1][1] == 3 &&
            messages[0][3] != messages[1][3],
        "first peer message sees old later-peer pad then later peer decode "
        "updates it");
  check(messages[1][4] == 0x12345678 && task_seen == 1,
        "message World publication survives later peer and task import");
}
void held_backlog() {
  auto c = config();
  S::Session s(c);
  unsigned runs = 0;
  auto b = base();
  b.on_run = {"run", [&](O::GameObject &) { ++runs; }};
  s.enable_objects(1, b);
  populate(s);
  s.submit(sample(), 0, 0, 4, true, false, 0, {ready(c)});
  check(runs == 0, "input backlog no_sim does not run world");
  auto result = s.submit(sample(), 0, 0, 1, false, true, 1);
  check(result.held && runs > 0 &&
            task_events(s.snapshot(), "runObjects") == runs,
        "held presentation can execute already ready backlog");
}
void malformed_message_partial_pads() {
  auto c = config();
  S::Session s(c);
  unsigned runs = 0;
  auto b = base();
  b.on_run = {"run", [&](O::GameObject &) { ++runs; }};
  s.enable_objects(17, b);
  populate(s);
  N::Machine remote(2, 1, N::Fpu(c.model, c.mul_bias, c.div_nearest));
  remote.peers[1].msg_ring[0] =
      7; // One declared byte cannot contain a two-byte message header.
  for (unsigned f = 1; f <= 4; ++f)
    remote.peers[1].set_record(
        f, N::rec12(0x123, {129, 65, 254, 9}, {11, 13}, 0, 0), 0,
        f == 1 ? 1 : 0);
  const auto packet =
      remote.build_packet(0, 1, 4, N::PKT_RAW_RELAY, 1, nullptr, true).data;
  s.submit(sample(5), 0, 0, 1, false, false, 0, {packet});
  rejects<N::MalformedPacket>(
      [&] { s.submit(sample(6)); },
      "malformed frame message refuses after pad updates");
  const auto snap = s.snapshot();
  check(snap.failed && runs == 0 && snap.network.net_frame == 1,
        "message parse error prevents world tasks and latches");
  check((snap.world->pads[0].held & 0xffff) == 5 &&
            (snap.world->pads[1].held & 0xffff) == 0x123,
        "out-of-World parser failure imports both current peer pads");
  check(snap.world->pads[0].held == word(snap.network.pads[0], 8) &&
            snap.world->pads[1].held == word(snap.network.pads[1], 8),
        "failure snapshot preserves matching partial network and World pad "
        "state");
  rejects<S::Failed>([&] { s.submit(sample()); }, "malformed message no retry");
}
void opposite_endpoints() {
  struct Projection {
    std::uint32_t shared, combat;
    std::int64_t frame, tag;
    std::array<std::array<std::int64_t, 3>, 4> pads;
    bool operator==(const Projection &o) const {
      return shared == o.shared && combat == o.combat && frame == o.frame &&
             tag == o.tag && pads == o.pads;
    }
  };
  struct Delivery {
    unsigned due, peer;
    N::ByteVector bytes;
  };
  std::array<std::unique_ptr<S::Session>, 2> hosts;
  std::array<std::map<std::int64_t, Projection>, 2> traces;
  std::vector<Delivery> deliveries;
  for (unsigned peer = 0; peer < 2; ++peer) {
    auto c = config();
    c.local = peer;
    hosts[peer] = std::make_unique<S::Session>(c);
    auto b = base();
    b.on_run = {
        "authored_run", [&, peer](O::GameObject &o) {
          hosts[peer]->with_world([&](O::World &w) {
            check((w.pads[0].held & 1) && (w.pads[1].held & 1),
                  "both endpoint inputs available before object callback");
            ++*o.tag;
            w.shared.draw();
          });
        }};
    hosts[peer]->enable_objects(17, b);
    hosts[peer]->with_world([](O::World &w) {
      auto *o = w.new_object(0x180, {0, 0, 0});
      o->tag = 0;
      w.run_every_frame(*o);
    });
    hosts[peer]->register_object_task(
        100, "record", 125, 0, [&, peer](S::Session &x) {
          const auto snap = x.snapshot();
          const auto &w = *snap.world;
          Projection value{
              w.shared_seed, snap.seed, w.frame, *w.objects[0].tag, {}};
          for (unsigned i = 0; i < 4; ++i)
            value.pads[i] = {w.pads[i].held, w.pads[i].pressed,
                             w.pads[i].edge2};
          check(traces[peer].emplace(w.frame, value).second,
                "one post-task per endpoint completed tick");
        });
  }
  for (unsigned wall = 0;
       wall < 160 && (traces[0].size() < 12 || traces[1].size() < 12); ++wall) {
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
      auto result = hosts[peer]->submit(
          sample(1), 0, 0, 1, traces[peer].size() >= 12, false, wall, inbox);
      for (const auto &packet : result.sent) {
        check(packet.peer >= 0 && packet.peer < 2 &&
                  unsigned(packet.peer) != peer,
              "opposite endpoint destination");
        deliveries.push_back({wall + 1 + (peer == 0 ? 1U : 0U),
                              unsigned(packet.peer), packet.data});
      }
    }
  }
  check(traces[0].size() >= 12 && traces[1].size() >= 12,
        "two owned World sessions advance with delayed unequal presentation "
        "cadence");
  std::uint32_t seed =
      17 * 1664525U + 1013904223U; // Constructor consumes one shared draw.
  for (std::int64_t tick = 1; tick <= 12; ++tick) {
    const auto &a = traces[0].at(tick);
    const auto &b = traces[1].at(tick);
    check(a == b, "owned gameplay projection agrees across opposite local "
                  "roles at same tick");
    seed = seed * 1664525U + 1013904223U;
    check(a.shared == seed && a.combat == 0xabcdef01 && a.frame == tick &&
              a.tag == tick,
          "two equal projections match independent LCG and callback-count "
          "anchors");
  }
}
void bounded_world_access() {
  {
    S::Session s(config());
    s.enable_objects(3, base());
    unsigned entered = 0;
    std::function<void()> nested;
    nested = [&] {
      s.with_world([&](O::World &w) {
        ++entered;
        w.menu_lockout = entered;
        nested();
      });
    };
    rejects<S::ResourceLimit>(
        nested,
        "nested World scope budget refuses rather than recursing indefinitely");
    const auto snap = s.snapshot();
    check(entered == 16 && snap.world_depth == 0 && snap.failed &&
              snap.world->menu_lockout == 16,
          "nested exhaustion retains last mutation and balanced depth");
    rejects<S::Failed>(nested, "nested budget failure cannot retry");
  }
  {
    S::Session s(config());
    s.enable_objects(3, base());
    rejects<S::ResourceLimit>(
        [&] { s.submit(sample(), 0, 0, 33); },
        "object adapter retains bounded submit-step policy");
    check(s.failed() && !s.snapshot().started &&
              s.snapshot().world->iterator_serial == 0,
          "submit budget failure never begins scheduler");
  }
}
void arithmetic_and_failure() {
  for (unsigned cfg = 0; cfg < 16; ++cfg) {
    auto c = config(cfg);
    R::Fpu &ambient = R::set_fpu(R::Fpu::Model::Nearest, true, true);
    S::Session s(c);
    s.enable_objects(0, base());
    s.with_world([&](O::World &w) {
      check(ambient.model == c.model && ambient.mul_bias == c.mul_bias &&
                ambient.div_nearest == c.div_nearest,
            "world uses selected FPU");
      w.camera_focus[0] = R::mul(0x3f800001, 0x3f800001);
    });
    check(ambient.model == R::Fpu::Model::Nearest && ambient.mul_bias &&
              ambient.div_nearest,
          "world access restores ambient FPU");
    rejects<std::runtime_error>(
        [&] {
          s.with_world([&](O::World &w) {
            w.menu_lockout = 23;
            throw std::runtime_error("authored");
          });
        },
        "world callback failure propagates");
    check(s.failed() && s.snapshot().world->menu_lockout == 23,
          "world callback partial writes retained and latched");
    check(ambient.model == R::Fpu::Model::Nearest && ambient.mul_bias &&
              ambient.div_nearest,
          "failure restores ambient FPU");
    auto before = s.snapshot();
    rejects<S::Failed>([&] { s.submit(sample()); },
                       "failed world cannot retry submission");
    check(s.snapshot().world->menu_lockout == before.world->menu_lockout &&
              s.snapshot().network.net_frame == before.network.net_frame,
          "retry produces no further mutation");
  }
  R::set_fpu();
}
} // namespace
int main() {
  try {
    setup_contract();
    ownership_and_seeds();
    ready_boundaries();
    mutation_and_failure();
    nested_scope_and_guards();
    peer_message_order();
    held_backlog();
    malformed_message_partial_pads();
    opposite_endpoints();
    bounded_world_access();
    arithmetic_and_failure();
    std::cout << checks << " session object adapter checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "session object check failed: " << e.what() << '\n';
    return 1;
  }
}
