#include "champions/session.hpp"
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace R = champions::simulation::rta;
namespace S = R::session;
namespace A = R::amx;
namespace N = R::netwire;
namespace {
unsigned checks = 0;
void check(bool condition, const char *message) {
  ++checks;
  if (!condition)
    throw std::runtime_error(message);
}
template <class E = S::Error, class F>
void rejects(F operation, const char *message) {
  bool caught = false;
  try {
    operation();
  } catch (const E &) {
    caught = true;
  }
  check(caught, message);
}
N::ByteVector input(std::uint16_t buttons = 0) {
  return {std::uint8_t(buttons),
          std::uint8_t(buttons >> 8),
          128,
          128,
          128,
          128,
          0,
          0};
}
R::items::Definition definition() {
  R::items::Definition d;
  d.addr = R::items::A_DEFS;
  d.name = "authored-blade";
  d.level = 1;
  d.weight = R::ONE;
  d.count = 1;
  d.damage_min = 3;
  d.damage_max = 9;
  d.price = 17;
  d.armor = 5;
  d.attack = 7;
  d.damage_bits = 1;
  return d;
}
void setup(S::Session &s) {
  s.add_definition("definition", definition());
  S::ItemRecord record;
  record.count = 1;
  s.add_item("item", "definition", record);
}
A::ByteVector program(const std::vector<A::AsmLine> &lines,
                      const std::vector<std::string> &natives = {}) {
  A::ImageOptions options;
  options.data.resize(32);
  options.stack_cells = 64;
  options.publics = {{"main", 0}};
  options.natives = natives;
  return A::build_image(A::assemble(lines).cells, options);
}
A::AsmInstruction op(const std::string &name,
                     std::vector<A::AsmValue> args = {}) {
  return {name, std::move(args), {}, {}};
}
void profile_and_setup() {
  static_assert(!std::is_copy_constructible<S::Session>::value,
                "session owns stable identities");
  static_assert(!std::is_move_constructible<S::Session>::value,
                "callbacks retain session identity");
  for (const auto profile : {"norrath", "unknown", "", "RTA"}) {
    S::Config c;
    c.profile = profile;
    rejects([&] { S::Session s(c); }, "unsupported title/profile accepted");
  }
  for (const auto peers : {0, 5}) {
    S::Config c;
    c.peers = peers;
    rejects([&] { S::Session s(c); }, "invalid peer count accepted");
  }
  {
    S::Config c;
    c.local = 1;
    rejects([&] { S::Session s(c); }, "invalid local slot accepted");
  }
  S::Session s;
  auto d = definition();
  s.add_definition("definition", d);
  d.name = "mutated";
  d.attack = 999;
  S::ItemRecord item;
  item.count = 1;
  item.mods[0] = R::items::Modifier{0x15, {3, 7, 0}};
  s.add_item("item", "definition", item);
  item.mods[0]->values[0] = 999;
  auto before = s.snapshot();
  check(before.definitions.at("definition").name == "authored-blade" &&
            before.definitions.at("definition").attack == 7,
        "definition retains owned input");
  check(before.items.at("item").record.mods[0]->values[0] == 3,
        "item retains owned modifier input");
  before.definitions.at("definition").name = "snapshot mutation";
  before.items.at("item").record.mods[0]->values[0] = 42;
  before.network.peers[0].ring.fill(255);
  check(s.snapshot().definitions.at("definition").name == "authored-blade" &&
            s.snapshot().items.at("item").record.mods[0]->values[0] == 3 &&
            s.network().peers[0].ring[0] == 0,
        "snapshots do not alias live storage");
  check(!before.network.run_tasks && !before.network.on_kill &&
            !before.network.stack_bytes,
        "snapshots retain no executable callbacks");
  rejects([&] { s.add_definition("definition", definition()); },
          "duplicate definition accepted");
}
void ticks_and_errors() {
  S::Session s;
  setup(s);
  unsigned tasks = 0;
  s.register_task("authored_count", [&](S::Session &current) {
    ++tasks;
    current.draw();
  });
  s.submit(input());
  auto first = s.submit(input());
  check(first.steps == 1 && tasks == 1 && s.network().net_frame == 2,
        "ready input invokes task once");
  check(s.snapshot().draws.size() == 1, "task changes owned random state");
  s.set_level_pending(true);
  const auto frame = s.network().net_frame;
  s.submit(input());
  check(tasks == 1 && s.network().net_frame == frame,
        "level pending suppresses task and frame advancement");
  s.set_level_pending(false);
  s.submit(input(), 0, 0, 1, true);
  check(tasks == 1, "no_sim suppresses tasks");
  S::Config held_config;
  held_config.network_mode = 1;
  S::Session held(held_config);
  unsigned held_tasks = 0;
  held.register_task("count", [&](S::Session &) { ++held_tasks; });
  held.submit(input());
  held.submit(input());
  auto hold = held.submit(input(), 0, 0, 1, false, true);
  check(hold.held && held_tasks == 1,
        "hold after initial wire warmup cannot create another task");
  S::Config remote;
  remote.peers = 2;
  remote.network_mode = 1;
  S::Session waiting(remote);
  unsigned waiting_tasks = 0;
  waiting.register_task("count", [&](S::Session &) { ++waiting_tasks; });
  auto none = waiting.submit(input());
  check(none.steps == 0 && waiting_tasks == 0 &&
            waiting.network().net_frame == 1,
        "missing remote input cannot advance simulation");
  S::Session broken;
  setup(broken);
  broken.register_task("mutate_then_throw", [](S::Session &x) {
    x.set_modifier("item", 0x15, 9);
    x.draw();
    throw std::runtime_error("authored callback failure");
  });
  broken.submit(input());
  rejects<std::exception>([&] { broken.submit(input()); },
                          "callback failure swallowed");
  auto failed = broken.snapshot();
  check(failed.failed && failed.network.net_frame == 1 &&
            failed.draws.size() == 1 &&
            failed.items.at("item").record.mods[0]->values[0] == 9,
        "failure latches with prethrow mutations and no frame advancement");
  rejects<S::Failed>([&] { broken.submit(input()); },
                     "failed session retried task");
  auto after = broken.snapshot();
  check(after.draws == failed.draws &&
            after.events.size() == failed.events.size() &&
            after.failure_operation == failed.failure_operation,
        "retry changes latched state");
  rejects<S::Failed>([&] { broken.draw(); },
                     "failed session permits random mutation");
  S::Session recursive;
  recursive.register_task("reenter", [](S::Session &x) { x.submit(input()); });
  recursive.submit(input());
  rejects<std::exception>([&] { recursive.submit(input()); },
                          "reentrant submit accepted");
  check(recursive.failed() && recursive.network().net_frame == 1,
        "reentrant submission fails session");
}
void script_composition() {
  S::Session s;
  setup(s);
  unsigned native_calls = 0;
  A::NativeMap bindings;
  bindings["authored_set"] =
      [&](A::Machine &,
          const std::vector<A::Cell> &args) -> std::optional<A::Address> {
    ++native_calls;
    check(args == std::vector<A::Cell>({0x15, 3}),
          "AMX argument order differs");
    return s.set_modifier("item", args[0], args[1], 7, 0);
  };
  bindings["authored_evaluate"] =
      [&](A::Machine &,
          const std::vector<A::Cell> &args) -> std::optional<A::Address> {
    ++native_calls;
    check(args.empty(), "zero-argument native gained parameters");
    const auto value = s.evaluate_item("item");
    check(value.weight == R::ONE && value.damage.first == 6 &&
              value.damage.second == 16,
          "authored item values unexpectedly changed");
    return 0x12345678;
  };
  auto image = program(
      {op("PROC"), op("PUSH.C", {A::Cell(3)}), op("PUSH.C", {A::Cell(0x15)}),
       op("PUSH.C", {A::Cell(8)}), op("SYSREQ.C", {A::Cell(0)}),
       op("STACK", {A::Cell(12)}), op("PUSH.C", {A::Cell(0)}),
       op("SYSREQ.C", {A::Cell(1)}), op("STACK", {A::Cell(4)}),
       op("STOR.pri", {A::Cell(0)}), op("RETN")},
      {"authored_set", "authored_evaluate"});
  s.add_script("probe", image, bindings);
  std::fill(image.begin(), image.end(), 0);
  bindings.clear();
  s.register_task("authored_script",
                  [](S::Session &x) { x.execute("probe", 0); });
  s.submit(input(1));
  s.submit(input(1));
  const auto snap = s.snapshot();
  const auto &vm = snap.scripts.at("probe");
  check(native_calls == 2 &&
            snap.items.at("item").record.mods[0]->values[0] == 3,
        "ready tick failed to execute AMX item/combat operations");
  const auto offset = static_cast<std::size_t>(vm.data_offset);
  check(vm.stored.image.at(offset) == 0x78 &&
            vm.stored.image.at(offset + 1) == 0x56 &&
            vm.stored.image.at(offset + 2) == 0x34 &&
            vm.stored.image.at(offset + 3) == 0x12,
        "AMX result missing from owned data memory");
  check(vm.profile && !vm.profile->empty() && vm.budget &&
            *vm.budget < S::SCRIPT_BUDGET,
        "script executed without bounded instruction evidence");
  auto altered = s.snapshot();
  altered.scripts.at("probe").stored.image[offset] = 0;
  altered.scripts.at("probe").profile->clear();
  altered.scripts.at("probe").decoded->clear();
  check(s.snapshot().scripts.at("probe").stored.image[offset] == 0x78 &&
            !s.snapshot().scripts.at("probe").profile->empty() &&
            !s.snapshot().scripts.at("probe").decoded->empty(),
        "script snapshot aliases owned bytes or maps");
  rejects([&] { s.register_task("late", [](S::Session &) {}); },
          "late task registration accepted");
  S::Session missing;
  auto foreign = program({op("PROC"), op("RETN")}, {"unmapped_engine"});
  rejects([&] { missing.add_script("probe", foreign); },
          "missing engine native silently accepted");
  S::Session vm_error;
  vm_error.add_script("bad", program({op("PROC"), op("HALT", {A::Cell(11)})}));
  vm_error.register_task("bad", [](S::Session &x) { x.execute("bad", 0); });
  vm_error.submit(input());
  rejects([&] { vm_error.submit(input()); },
          "nonzero VM error treated as success");
  check(vm_error.failed() && vm_error.network().net_frame == 1,
        "VM error fails before frame advance");
  S::Session loop;
  loop.add_script("loop", program({op("PROC"), A::Label{"loop:"},
                                   op("JUMP", {std::string("loop")})}));
  loop.register_task("loop", [](S::Session &x) { x.execute("loop", 0); });
  loop.submit(input());
  rejects<A::Limit>([&] { loop.submit(input()); },
                    "infinite VM loop escaped instruction budget");
  check(loop.failed() && loop.snapshot().scripts.at("loop").budget == 0,
        "instruction limit did not retain exhausted budget");
}
void failed_mutable_script_snapshot() {
  for (bool truncate : {false, true}) {
    S::Session s;
    A::ByteVector changed_image;
    A::Address changed_offset = 0, changed_count = 0;
    unsigned calls = 0;
    A::NativeMap natives;
    natives["authored_corrupt"] =
        [&](A::Machine &m,
            const std::vector<A::Cell> &) -> std::optional<A::Address> {
      ++calls;
      if (truncate)
        m.image.resize(12);
      else {
        m.natives_off = std::numeric_limits<A::Address>::max();
        m.num_natives = 64;
        m.image[0] ^= 0xff;
      }
      changed_image = m.image;
      changed_offset = m.natives_off;
      changed_count = m.num_natives;
      throw std::runtime_error("authored native metadata failure");
    };
    s.add_script("probe",
                 program({op("PROC"), op("PUSH.C", {A::Cell(0)}),
                          op("SYSREQ.C", {A::Cell(0)}),
                          op("STACK", {A::Cell(4)}), op("RETN")},
                         {"authored_corrupt"}),
                 natives);
    s.register_task("authored_script",
                    [](S::Session &x) { x.execute("probe", 0); });
    s.submit(input());
    rejects<std::runtime_error>(
        [&] { s.submit(input()); },
        "mutating native callback failure was swallowed");
    const auto retained = s.snapshot();
    const auto &vm = retained.scripts.at("probe");
    check(retained.failed &&
              retained.failure_operation == "native:probe:authored_corrupt" &&
              vm.natives_off == changed_offset &&
              vm.num_natives == changed_count &&
              vm.stored.image == changed_image,
          "failed snapshot reparses or loses malformed script partial state");
    check(vm.declared_native_names ==
              std::vector<std::string>{"authored_corrupt"},
          "failed script snapshot loses initial native declarations");
    rejects<S::Failed>([&] { s.submit(input()); },
                       "malformed script state allows retry");
    const auto after = s.snapshot();
    check(calls == 1 &&
              after.scripts.at("probe").stored.image == changed_image &&
              after.events.size() == retained.events.size(),
          "refused retry alters malformed retained script state");
  }
}
void registration_bounds() {
  S::Session tasks;
  for (std::size_t i = 0; i < S::MAX_TASKS; ++i)
    tasks.register_task("task" + std::to_string(i), [](S::Session &) {});
  rejects<S::ResourceLimit>(
      [&] { tasks.register_task("excess", [](S::Session &) {}); },
      "task limit missing");
  check(tasks.snapshot().tasks.size() == S::MAX_TASKS,
        "task rejection changed existing registry");
  S::Session names;
  rejects(
      [&] {
        names.add_definition(std::string(S::MAX_NAME + 1, 'x'), definition());
      },
      "unbounded name accepted");
  S::Session sample;
  sample.register_task("none", [](S::Session &) {});
  rejects<S::ResourceLimit>([&] { sample.submit(N::ByteVector(13)); },
                            "oversized sample accepted");
  check(!sample.snapshot().started && sample.failed(),
        "oversized sample mutated running state");
  S::Session inbox;
  inbox.register_task("none", [](S::Session &) {});
  rejects<S::ResourceLimit>(
      [&] {
        inbox.submit(input(), 0, 0, 1, false, false, 0,
                     std::vector<N::ByteVector>(257));
      },
      "inbox count limit missing");
  S::Session absent;
  rejects([&] { absent.submit(input()); }, "empty task registry succeeded");
}
void history_exhaustion() {
  S::Session s;
  for (std::size_t i = 0; i < S::MAX_EVENT_HISTORY / 2; ++i)
    s.draw();
  const auto before = s.snapshot();
  rejects<S::ResourceLimit>([&] { s.draw(); },
                            "history budget was silently exceeded");
  const auto after = s.snapshot();
  check(after.failed && after.draws == before.draws &&
            after.events.size() == before.events.size(),
        "history refusal changed seed or made retained state unavailable");
  rejects<S::Failed>([&] { s.draw(); }, "history failure retries mutation");
}
void random_and_message_boundaries() {
  S::Config cfg;
  cfg.seed = 17;
  cfg.network_seed = 1234;
  S::Session s(cfg);
  const auto initial = s.network();
  const auto result = s.draw();
  check(result != 0 && s.snapshot().seed != 17 && s.network().rand_seed == 1234,
        "combat draw implicitly aliases network seed");
  s.set_sync_state(1, 2, initial.positions);
  check(s.network().rand_seed == 1234,
        "sync update publishes seed without request");
  s.set_sync_state(1, 2, initial.positions, s.snapshot().seed);
  check(std::uint32_t(s.network().rand_seed) == s.snapshot().seed,
        "explicit seed publication lost");
  auto view = s.network();
  check(!view.run_tasks && !view.on_kill && view.handlers.empty(),
        "network view exposes live callbacks");
  S::Session messages;
  setup(messages);
  std::int64_t last_task_frame = 0;
  messages.register_task(
      "later", [&](S::Session &x) { last_task_frame = x.network().net_frame; });
  messages.register_message(7, "authored_message",
                            [](S::Session &x, const N::ByteVector &payload) {
                              x.set_modifier("item", 0x0d, payload.at(0));
                              throw std::runtime_error("message stop");
                            });
  messages.queue_message(7, {25});
  bool message_failed = false;
  for (unsigned attempt = 0; attempt < 4 && !message_failed; ++attempt) {
    try {
      messages.submit(input());
    } catch (const std::runtime_error &) {
      message_failed = true;
    }
  }
  check(message_failed, "message failure swallowed");
  auto failed = messages.snapshot();
  check(failed.failed &&
            failed.failure_operation == "message:authored_message" &&
            last_task_frame < failed.network.net_frame &&
            failed.items.at("item").record.mods[0]->values[0] == 25,
        "message failure lost order or partial state");
  rejects<S::Failed>([&] { messages.submit(input()); },
                     "message failure permits retry");
  for (const auto kind : {-1, 256}) {
    S::Session bad_kind;
    rejects(
        [&] {
          bad_kind.register_message(kind, "authored",
                                    [](S::Session &, const N::ByteVector &) {});
        },
        "message kind outside byte domain accepted");
  }
  S::Session payload;
  payload.register_message(7, "authored",
                           [](S::Session &, const N::ByteVector &) {});
  payload.queue_message(7, N::ByteVector(255, 7));
  const auto queued = payload.network().peers[0].pack();
  rejects<S::ResourceLimit>(
      [&] { payload.queue_message(7, N::ByteVector(256, 7)); },
      "message length silently truncated at256");
  check(payload.network().peers[0].pack() == queued,
        "invalid message length changed queued bytes");
  S::Session unknown;
  rejects([&] { unknown.queue_message(999, {}); },
          "unknown message mapping invented");
  for (const auto steps :
       {std::int64_t(-1), std::int64_t(S::MAX_SUBMIT_STEPS) + 1}) {
    S::Session limited;
    limited.register_task("none", [](S::Session &) {});
    rejects<S::ResourceLimit>([&] { limited.submit(input(), 0, 0, steps); },
                              "submit step bound missing");
    check(!limited.snapshot().started && limited.failed(),
          "invalid step count changes frame state");
  }
}
// Two owned sessions exchange their real transport output. Presentation calls
// intentionally differ; only game-owned state at the same completed tick is
// compared. This is an authored command map, not an original engine binding.
void two_session_exchange() {
  struct Projection {
    std::uint32_t seed;
    std::vector<std::uint32_t> draws;
    std::int16_t modifier;
    A::ByteVector image;
    bool operator==(const Projection &v) const {
      return seed == v.seed && draws == v.draws && modifier == v.modifier &&
             image == v.image;
    }
  };
  struct Delivery {
    unsigned due;
    unsigned peer;
    N::ByteVector bytes;
  };
  std::array<std::unique_ptr<S::Session>, 2> sessions;
  std::array<std::map<std::int64_t, Projection>, 2> traces;
  std::vector<Delivery> deliveries;
  unsigned wall = 0;
  const auto image = program(
      {op("PROC"), op("PUSH.C", {A::Cell(0)}), op("SYSREQ.C", {A::Cell(0)}),
       op("STACK", {A::Cell(4)}), op("STOR.pri", {A::Cell(0)}), op("RETN")},
      {"authored_step"});
  for (unsigned peer = 0; peer < 2; ++peer) {
    S::Config c;
    c.peers = 2;
    c.local = peer;
    c.network_mode = 1;
    c.seed = 17;
    sessions[peer] = std::make_unique<S::Session>(c);
    auto &s = *sessions[peer];
    setup(s);
    A::NativeMap natives;
    natives["authored_step"] =
        [&,
         peer](A::Machine &,
               const std::vector<A::Cell> &args) -> std::optional<A::Address> {
      check(args.empty(), "two-session authored native arguments changed");
      auto &x = *sessions[peer];
      const auto net = x.network();
      unsigned active = 0;
      for (unsigned slot = 0; slot < 2; ++slot) {
        const auto &pad = net.pads[slot];
        const std::uint32_t value =
            std::uint32_t(pad[0x3c]) | (std::uint32_t(pad[0x3d]) << 8) |
            (std::uint32_t(pad[0x3e]) << 16) | (std::uint32_t(pad[0x3f]) << 24);
        if (value == R::ONE)
          ++active;
      }
      check(active == 2, "ready task observed missing authored input mapping");
      x.set_modifier("item", 0x15, static_cast<std::int32_t>(active * 3));
      x.evaluate_item("item");
      return x.draw();
    };
    s.add_script("probe", image, natives);
    s.register_task("authored_script", [&, peer](S::Session &x) {
      x.execute("probe", 0);
      const auto net = x.network();
      x.set_sync_state(net.sim_frame + 1, net.sync_acc + 17, net.positions);
      const auto snap = x.snapshot();
      traces[peer].emplace(
          net.net_frame,
          Projection{snap.seed, snap.draws,
                     snap.items.at("item").record.mods[0]->values[0],
                     snap.scripts.at("probe").stored.image});
    });
  }
  for (wall = 0; wall < 160 && (traces[0].size() < 12 || traces[1].size() < 12);
       ++wall) {
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
      const auto result = sessions[peer]->submit(
          input(1), 0, 0, 1, traces[peer].size() >= 12, false, wall, inbox);
      for (const auto &datagram : result.sent) {
        check(datagram.peer >= 0 && datagram.peer < 2 &&
                  unsigned(datagram.peer) != peer,
              "datagram destination does not identify remote session");
        deliveries.push_back({wall + 1 + (peer == 0 ? 1U : 0U),
                              unsigned(datagram.peer), datagram.data});
      }
    }
  }
  check(traces[0].size() >= 12 && traces[1].size() >= 12,
        "delayed two-session exchange failed to progress");
  std::uint32_t seed = 17;
  std::vector<std::uint32_t> draws;
  for (std::int64_t tick = 1; tick <= 12; ++tick) {
    const auto &a = traces[0].at(tick), &b = traces[1].at(tick);
    check(a == b, "two owned sessions diverged at the same completed tick");
    seed = seed * 1664525U + 1013904223U;
    draws.push_back(seed >> 16);
    check(a.seed == seed && a.draws == draws && a.modifier == 6,
          "two equal sessions differ from independent RNG/action expectation");
  }
  check(sessions[0]->network().rand_seed == 0 &&
            sessions[1]->network().rand_seed == 0,
        "two-session combat draws changed independent wire hash seeds");
}
void arithmetic_and_snapshot() {
  for (unsigned config = 0; config < 16; ++config) {
    S::Config c;
    c.model = static_cast<R::Fpu::Model>(config / 4);
    c.mul_bias = config & 2;
    c.div_nearest = config & 1;
    auto &ambient = R::set_fpu(R::Fpu::Model::Nearest, true, true);
    S::Session s(c);
    setup(s);
    s.evaluate_item("item");
    s.roll_damage(0x40400000, 0x41100000);
    check(ambient.model == R::Fpu::Model::Nearest && ambient.mul_bias &&
              ambient.div_nearest,
          "successful composed operations leak caller FPU");
    s.register_task("throw", [](S::Session &x) {
      x.evaluate_item("item");
      throw std::runtime_error("stop");
    });
    s.submit(input());
    rejects<std::exception>([&] { s.submit(input()); },
                            "throwing arithmetic task succeeds");
    check(ambient.model == R::Fpu::Model::Nearest && ambient.mul_bias &&
              ambient.div_nearest,
          "throwing composed operations leak caller FPU");
  }
  S::Session a, b;
  setup(a);
  setup(b);
  a.set_modifier("item", 0x15, 1);
  b.set_modifier("item", 0x15, 2);
  const auto sa = a.snapshot(), sb = b.snapshot();
  const auto hash = [](const S::Snapshot &x) {
    return N::sync_hash(
        x.network.rand_seed, x.network.sync_acc,
        std::vector<std::optional<N::Position>>(x.network.positions.begin(),
                                                x.network.positions.end()));
  };
  check(hash(sa) == hash(sb), "authored comparison must have equal wire hash");
  check(sa.items.at("item").record.mods[0]->values[0] !=
            sb.items.at("item").record.mods[0]->values[0],
        "full snapshot loses combat item divergence excluded from wire hash");
}
} // namespace
int main() {
  try {
    profile_and_setup();
    ticks_and_errors();
    script_composition();
    failed_mutable_script_snapshot();
    registration_bounds();
    history_exhaustion();
    random_and_message_boundaries();
    two_session_exchange();
    arithmetic_and_snapshot();
    std::cout << checks << " session integration checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
