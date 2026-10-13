#include "champions/native_save.hpp"
#include "session_creature_trace.hpp"
#include <chrono>
#include <functional>
#include <iostream>
namespace {
namespace R = champions::simulation::rta;
namespace N = R::native_save;
namespace Control = R::session_control;
namespace W = R::world_queries;
using namespace champions::test::session_trace;
std::size_t checks = 0;
std::uint64_t digest = 14695981039346656037ULL;
void check(bool ok, const char *why) {
  ++checks;
  if (!ok)
    throw std::runtime_error(why);
}
void refuses(const std::function<void()> &fn) {
  bool rejected = false;
  try {
    fn();
  } catch (const N::Error &) {
    rejected = true;
  }
  check(rejected, "invalid snapshot accepted");
}
std::string full(const Control::Snapshot &s) {
  return sequence(
      {sequence({text(s.config.profile), n(unsigned(s.config.model)),
                 n(s.config.mul_bias), n(s.config.div_nearest),
                 n(s.config.shared_seed), n(s.config.max_ready_ticks),
                 n(unsigned(s.config.movement))}),
       level_snapshot(s.initial_terrain),
       n(s.supplied_terrain),
       champions::test::creature_trace::observation(s.session),
       n(s.actor_serial),
       n(s.actor_address),
       n(s.mover),
       n(s.hp),
       n(s.held_input),
       integers(s.pending_damage_frames),
       n(s.minimum_input_frame),
       n(s.submissions),
       n(s.ready_ticks),
       n(s.damage_events),
       n(s.paused),
       n(s.stopped),
       n(s.failed),
       n(s.busy),
       text(s.phase),
       text(s.error)});
}
template <class T> std::string setup_text(const T &s) {
  return sequence({text(s.config.profile), n(unsigned(s.config.model)),
                   n(s.config.mul_bias), n(s.config.div_nearest),
                   n(s.config.shared_seed), n(s.config.max_ready_ticks),
                   n(unsigned(s.config.movement)),
                   level_snapshot(s.initial_terrain), n(s.supplied_terrain)});
}
void projection(const N::Scene &live, const Control::Snapshot &s) {
  const auto value = live.observation();
  const auto &actor = s.session.world->objects.front();
  check(value.position == actor.pos && value.vz == actor.vz &&
            value.radius == actor.radius &&
            value.hp == s.session.hp->adapter.objects.front().value.hp &&
            value.frame == s.session.world->frame &&
            value.submissions == s.submissions &&
            value.ready_ticks == s.ready_ticks &&
            value.damage_events == s.damage_events &&
            value.paused == s.paused && value.stopped == s.stopped &&
            value.failed == (s.failed || s.session.failed) &&
            value.error == s.error && value.movement == s.config.movement &&
            value.ready_tick_limit == s.config.max_ready_ticks,
        "bounded observation differs from complete snapshot projection");
  check(setup_text(live.setup()) == setup_text(s),
        "bounded setup differs from complete snapshot projection");
}
W::LevelData terrain() {
  W::LevelData d;
  d.tg_x0 = d.tg_y0 = -2;
  d.tg_w = d.tg_h = 4;
  d.grid.assign(16, {0, 1});
  W::HeightField h;
  h.x0 = h.y0 = -64;
  h.w = h.h = 129;
  h.lo = -3;
  h.hi = 3;
  h.offset = -123;
  h.samples.assign(129 * 129, -48);
  // Collision ridge and a flag4 element, authored synthetic data.
  for (unsigned y = 0; y < 129; ++y)
    h.samples[y * 129 + 65] = 48;
  d.heightfields.push_back(h);
  d.elements.push_back(
      W::make_element(-5, -256, -256, 256, 256, 0, 0, 0, 0, 0, 32767, 0));
  d.elements.push_back(
      W::make_element(23, -256, -256, 256, 256, 0, 4, 0, 0, 0, 32767, 0));
  d.elements[1].stamp = 9;
  d.stamp = 17;
  d.pieces = {0, 0, 1};
  d.pg_x0 = -1;
  d.pg_y0 = 2;
  d.pg_w = 2;
  d.pg_h = 1;
  d.path_cells = {0x12345678, 0xffffffff};
  return d;
}

void equal(const N::Scene &a, const N::Scene &b) {
  const auto state = a.snapshot();
  projection(a, state);
  auto trace = full(state);
  const auto expected = full(b.snapshot());
  if (trace != expected) {
    const auto mismatch = std::mismatch(trace.begin(), trace.end(),
                                        expected.begin(), expected.end());
    const auto offset = std::size_t(mismatch.first - trace.begin());
    std::cerr << "first difference at " << offset
              << " actual=" << trace.substr(offset > 80 ? offset - 80 : 0, 200)
              << " expected="
              << expected.substr(offset > 80 ? offset - 80 : 0, 200) << "\n";
  }
  check(trace == expected, "complete snapshot/replay trace differs");
  for (unsigned char c : trace) {
    digest ^= c;
    digest *= 1099511628211ULL;
  }
}
void trace(Control::MovementPolicy policy, unsigned arithmetic, bool supplied) {
  Control::Config cfg;
  cfg.movement = policy;
  cfg.max_ready_ticks =
      policy == Control::MovementPolicy::DryCreature ? 512 : 2000;
  cfg.model = R::Fpu::Model(arithmetic / 4);
  cfg.mul_bias = arithmetic & 2;
  cfg.div_nearest = arithmetic & 1;
  std::optional<W::LevelData> data;
  if (supplied)
    data = terrain();
  N::Scene live(cfg, data);
  for (unsigned i = 0; i < 13; ++i)
    live.advance(i % 3 ? 0 : 17);
  check(!live.snapshot().pending_damage_frames.empty(),
        "pending damage coverage");
  const auto replay = live.save(), snapshot = live.save_snapshot();
  check(snapshot[4] == 'N' && snapshot[8] == 1, "snapshot format anchor");
  for (auto byte : snapshot) {
    digest ^= byte;
    digest *= 1099511628211ULL;
  }
  auto direct = N::Scene::load(snapshot, "rta"),
       restored = N::Scene::load(replay, "rta");
  check(direct->save() == replay, "snapshot changed replay bytes");
  check(direct->save_snapshot() == snapshot, "snapshot resave differs");
  equal(*direct, *restored);
  for (unsigned i = 13; i < 35; ++i) {
    if (i == 16 || i == 18) {
      direct->set_paused(i == 16);
      restored->set_paused(i == 16);
      direct = N::Scene::load(direct->save_snapshot(), "rta");
      equal(*direct, *restored);
    }
    if (i == 23) {
      direct->release_input();
      restored->release_input();
      equal(*direct, *restored);
    }
    const auto command = i % 3 ? 4 : 17;
    direct->advance(command);
    restored->advance(command);
    equal(*direct, *restored);
    if (i == 26)
      direct = N::Scene::load(direct->save_snapshot(), "rta");
  }
  direct->stop();
  restored->stop();
  direct = N::Scene::load(direct->save_snapshot(), "rta");
  equal(*direct, *restored);
  direct->reset();
  restored->reset();
  equal(*direct, *restored);
}
void patch(N::Bytes &b, std::size_t at, std::uint32_t value) {
  for (unsigned i = 0; i < 4; ++i)
    b[at + i] = std::uint8_t(value >> (8 * i));
}
void checksum(N::Bytes &b) {
  patch(b, 20, 0);
  std::uint32_t c = ~0u;
  for (auto x : b) {
    c ^= x;
    for (unsigned i = 0; i < 8; ++i)
      c = (c & 1) ? (c >> 1) ^ 0xedb88320u : c >> 1;
  }
  patch(b, 20, ~c);
}
void malformed() {
  N::Scene live;
  live.advance(17);
  live.advance(0);
  const auto bytes = live.save_snapshot(), before = live.save();
  for (auto n :
       {std::size_t(0), std::size_t(8), std::size_t(23), bytes.size() - 1}) {
    N::Bytes b(bytes.begin(), bytes.begin() + n);
    refuses([&] { N::Scene::load(b, "rta"); });
  }
  // Checksum-free malformed structural cases reach the decoder.
  for (auto at : {8u, 12u, 24u}) {
    auto b = bytes;
    patch(b, at, 0xffffffff);
    checksum(b);
    refuses([&] { N::Scene::load(b, "rta"); });
  }
  auto b = bytes;
  b.push_back(0);
  patch(b, 16, std::uint32_t(b.size()));
  checksum(b);
  refuses([&] { N::Scene::load(b, "rta"); });
  refuses([&] { N::Scene::load(bytes, "norrath"); });
  // First direct-state field is held_input (u64), followed by edge count.
  std::uint32_t replay_size = 0;
  for (unsigned i = 0; i < 4; ++i)
    replay_size |= std::uint32_t(bytes[24 + i]) << (8 * i);
  const auto state = 32 + replay_size;
  for (auto [offset, value] :
       std::vector<std::pair<std::size_t, std::uint32_t>>{
           {state, 32}, {state + 8, 0xffffffff}, {state + 12, 0xffffffff}}) {
    b = bytes;
    patch(b, offset, value);
    checksum(b);
    refuses([&] { N::Scene::load(b, "rta"); });
  }
  b = bytes;
  b.back() ^= 1;
  refuses([&] { N::Scene::load(b, "rta"); });
  // Reach graph validation with a correctly checksummed, otherwise canonical
  // file. Find the explicit run-list record using the owned World observation.
  const auto world = *live.snapshot().session.world;
  const auto root = std::find_if(
      world.links.begin(), world.links.end(),
      [](const auto &link) { return link.id == R::objects::G_RUN_ACTIVE; });
  check(root != world.links.end(), "missing run-list graph fixture");
  N::Bytes record;
  for (auto word : std::array<std::uint64_t, 8>{
           root->id, root->next, root->prev, root->partner, root->object,
           std::uint64_t(root->kind), root->tag, root->object_raw})
    for (unsigned i = 0; i < 8; ++i)
      record.push_back(std::uint8_t(word >> (8 * i)));
  const auto found = std::search(bytes.begin() + state, bytes.end(),
                                 record.begin(), record.end());
  check(found != bytes.end(), "missing serialized graph fixture");
  const auto root_at = std::size_t(found - bytes.begin());
  b = bytes;
  patch(b, root_at + 8, R::objects::G_RUN_ACTIVE);
  checksum(b);
  refuses([&] { N::Scene::load(b, "rta"); }); // next points to a head / cycle
  b = bytes;
  patch(b, root_at + 8, 0xffffffff);
  patch(b, root_at + 12, 0xffffffff);
  checksum(b);
  refuses([&] { N::Scene::load(b, "rta"); }); // foreign graph identity
  b = bytes;
  patch(b, root_at + 40,
        0xffffffff); // enum outside its signed underlying range
  checksum(b);
  refuses([&] { N::Scene::load(b, "rta"); });
  check(live.save() == before, "load refusal changed live owner");
}
void access_cost() {
  using Clock = std::chrono::steady_clock;
  for (unsigned n : {12u, 511u}) {
    Control::Config cfg;
    cfg.movement = Control::MovementPolicy::DryCreature;
    cfg.max_ready_ticks = 512;
    N::Scene live(cfg);
    for (unsigned i = 0; i <= n; ++i)
      live.advance(i % 2 ? 0 : 17);
    const auto before = live.save();
    const auto expected = setup_text(live.snapshot());
    const auto observation = live.observation();
    auto detached = live.setup();
    detached.config.shared_seed = 999;
    detached.initial_terrain.data.heightfields.front().samples.front() = 123;
    check(setup_text(live.setup()) == expected,
          "setup copy aliases live construction state");
    auto start = Clock::now();
    for (unsigned i = 0; i < 10; ++i) {
      const auto state = live.snapshot();
      check(state.ready_ticks == n, "access-cost fixture ready ticks");
    }
    const auto snapshot_us =
        std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() -
                                                              start)
            .count();
    start = Clock::now();
    for (unsigned i = 0; i < 10; ++i)
      check(live.observation().ready_ticks == n,
            "access-cost observation ticks");
    const auto observation_us =
        std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() -
                                                              start)
            .count();
    start = Clock::now();
    for (unsigned i = 0; i < 10; ++i) {
      const auto setup = live.setup();
      check(
          setup.config.movement == cfg.movement &&
              setup.initial_terrain.data.heightfields.front().samples.front() ==
                  0,
          "access-cost setup fields");
    }
    const auto setup_us = std::chrono::duration_cast<std::chrono::microseconds>(
                              Clock::now() - start)
                              .count();
    check(live.save() == before, "read-only access changed replay bytes");
    projection(live, live.snapshot());
    live.set_paused(true);
    projection(live, live.snapshot());
    live.release_input();
    projection(live, live.snapshot());
    live.set_paused(false);
    live.advance(0);
    projection(live, live.snapshot());
    live.stop();
    projection(live, live.snapshot());
    live.reset();
    projection(live, live.snapshot());
    check(setup_text(live.setup()) == expected && observation.ready_ticks == n,
          "setup changed across control operations/reset");
    std::cout << "N=" << n << " repeats=10 snapshot_us=" << snapshot_us
              << " observation_us=" << observation_us
              << " setup_us=" << setup_us << "\n";
  }
}
void timing(const std::vector<unsigned> &frames = {12u, 511u}) {
  using Clock = std::chrono::steady_clock;
  for (unsigned n : frames) {
    Control::Config cfg;
    cfg.movement = Control::MovementPolicy::DryCreature;
    cfg.max_ready_ticks = 512;
    N::Scene live(cfg);
    for (unsigned i = 0; i <= n; ++i)
      live.advance(i % 2 ? 0 : 17);
    const auto replay = live.save(), snapshot = live.save_snapshot();
    auto start = Clock::now();
    auto a = N::Scene::load(replay, "rta");
    const auto replay_us =
        std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() -
                                                              start)
            .count();
    start = Clock::now();
    auto b = N::Scene::load(snapshot, "rta");
    const auto snapshot_us =
        std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() -
                                                              start)
            .count();
    equal(*a, *b);
    a->advance(0);
    b->advance(0);
    equal(*a, *b);
    std::cout << "N=" << n << " replay_us=" << replay_us
              << " snapshot_us=" << snapshot_us
              << " replay_bytes=" << replay.size()
              << " snapshot_bytes=" << snapshot.size() << "\n";
  }
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc == 2 && std::string(argv[1]) == "--access-cost") {
      access_cost();
      return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--malformed") {
      malformed();
      std::cout << checks << " malformed snapshot checks passed\n";
      return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--boundary") {
      timing({63u, 64u});
      return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--long") {
      timing();
      return 0;
    }
    for (auto p : {Control::MovementPolicy::Isolated,
                   Control::MovementPolicy::DryCreature})
      for (unsigned a = 0; a < 16; ++a)
        for (bool supplied : {false, true})
          trace(p, a, supplied);
    malformed();
    timing();
    std::cout << checks
              << " direct snapshot checks passed; exact trace FNV1a64="
              << std::hex << digest << "\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << "\n";
    return 1;
  }
}
