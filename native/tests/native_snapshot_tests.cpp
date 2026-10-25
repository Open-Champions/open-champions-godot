#include "champions/detail/checkpoint_fields.hpp"
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
void partition(Control::MovementPolicy policy, unsigned arithmetic,
               bool supplied, unsigned ticks = 12) {
  using Codec = R::detail::Checkpoint;
  using Access = R::detail::CheckpointAccess;
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
  for (unsigned i = 0; i <= ticks; ++i)
    live.advance(i % 3 ? 0 : 17);
  const auto replay = live.save(), snapshot = live.save_snapshot();
  auto replay_owner = N::Scene::load(replay, "rta");
  // Authored snapshot-v1 envelope; the unchanged complete direct block is
  // the independent reference for the internal two-stream representation.
  auto u32 = [&](std::size_t at) {
    std::uint32_t out = 0;
    for (unsigned i = 0; i < 4; ++i)
      out |= std::uint32_t(snapshot.at(at + i)) << (8 * i);
    return out;
  };
  const auto offset = std::size_t(28) + u32(24);
  const auto size = u32(offset);
  check(offset + 4 + size == snapshot.size(), "partition reference envelope");
  N::Bytes state(snapshot.begin() + offset + 4, snapshot.end());
  Control::Controller source(cfg, data), restored(cfg, data);
  Codec input(state);
  Access::run(source, input);
  input.finish();
  check(full(source.snapshot()) == full(live.snapshot()),
        "partition reference differs from facade");
  Codec split(Codec::Storage::Split);
  Access::run(source, split);
  check(split.bytes.size() + split.history_bytes.size() == state.size(),
        "partition omitted or duplicated checkpoint bytes");
  Codec read(split.bytes, split.history_bytes);
  Access::run(restored, read);
  read.finish();
  check(full(restored.snapshot()) == full(source.snapshot()),
        "partition complete trace differs");
  check(full(restored.snapshot()) == full(replay_owner->snapshot()),
        "partition differs from replay reconstruction");
  Codec joined;
  Access::run(restored, joined);
  check(joined.bytes == state, "partition changed snapshot-v1 direct bytes");
  Codec canonical(Codec::Storage::Split);
  Access::run(restored, canonical);
  check(canonical.bytes == split.bytes &&
            canonical.history_bytes == split.history_bytes,
        "partition resave differs");
  auto hash = [&](const auto &bytes) {
    for (unsigned char byte : bytes) {
      digest ^= byte;
      digest *= 1099511628211ULL;
    }
  };
  hash(split.bytes);
  hash(split.history_bytes);
  hash(full(restored.snapshot()));
  check(live.save() == replay && live.save_snapshot() == snapshot,
        "partition mutated reference formats");
  // Pending network input, health edges and history indices must continue
  // with exactly the same full observations, not just the scalar witness.
  for (auto command : {17u, 0u, 4u}) {
    restored.advance(command);
    replay_owner->advance(command);
    check(full(restored.snapshot()) == full(replay_owner->snapshot()),
          "partition ready continuation differs");
  }
  restored.set_paused(true);
  replay_owner->set_paused(true);
  restored.release_input();
  replay_owner->release_input();
  restored.set_paused(false);
  replay_owner->set_paused(false);
  restored.advance(0);
  replay_owner->advance(0);
  check(full(restored.snapshot()) == full(replay_owner->snapshot()),
        "partition pause/release continuation differs");
  restored.stop();
  replay_owner->stop();
  check(full(restored.snapshot()) == full(replay_owner->snapshot()),
        "partition stop differs");
  restored.reset();
  replay_owner->reset();
  check(full(restored.snapshot()) == full(replay_owner->snapshot()),
        "partition reset differs");
  if (!arithmetic && !supplied)
    std::cout << "partition policy=" << unsigned(policy) << " N=" << ticks
              << " current_bytes=" << split.bytes.size()
              << " history_bytes=" << split.history_bytes.size() << "\n";

  // Both streams must be complete and canonical. Refusal destroys only a
  // fresh candidate; the original Scene and its replay journal remain intact.
  auto reject = [&](N::Bytes current, N::Bytes history) {
    bool rejected = false;
    try {
      Control::Controller candidate(cfg, data);
      Codec bad(std::move(current), std::move(history));
      Access::run(candidate, bad);
      bad.finish();
    } catch (const std::runtime_error &) {
      rejected = true;
    }
    check(rejected, "malformed partition accepted");
  };
  auto truncated = split.bytes;
  truncated.pop_back();
  reject(truncated, split.history_bytes);
  truncated = split.history_bytes;
  truncated.pop_back();
  reject(split.bytes, truncated);
  auto trailing = split.bytes;
  trailing.push_back(0);
  reject(trailing, split.history_bytes);
  trailing = split.history_bytes;
  trailing.push_back(0);
  reject(split.bytes, trailing);
  reject(split.bytes, {});
  reject(split.history_bytes, split.bytes);
  check(live.save() == replay && live.save_snapshot() == snapshot,
        "partition refusal changed original scene");
}
struct CountingSource final : N::Source {
  N::Bytes bytes;
  mutable std::vector<std::pair<std::size_t, std::size_t>> reads;
  explicit CountingSource(N::Bytes b) : bytes(std::move(b)) {}
  std::size_t size() const override { return bytes.size(); }
  N::Bytes read(std::size_t at, std::size_t length) const override {
    if (at > bytes.size() || length > bytes.size() - at)
      throw N::Error("test source range");
    reads.emplace_back(at, length);
    return {bytes.begin() + at, bytes.begin() + at + length};
  }
};
std::uint32_t word32(const N::Bytes &bytes, std::size_t at) {
  std::uint32_t out = 0;
  for (unsigned i = 0; i < 4; ++i)
    out |= std::uint32_t(bytes.at(at + i)) << (8 * i);
  return out;
}
std::size_t indexed(Control::MovementPolicy policy, unsigned arithmetic,
                    bool supplied, unsigned ticks = 12) {
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
  for (unsigned i = 0; i <= ticks; ++i)
    live.advance(i % 3 ? 0 : 17);
  const auto replay = live.save(), old = live.save_snapshot();
  const auto bytes = live.save_indexed_snapshot();
  check(word32(bytes, 8) == 3, "indexed version anchor");
  const auto historical_at = 56 + word32(bytes, 24) + word32(bytes, 32);
  auto source = std::make_shared<CountingSource>(bytes);
  const auto start = std::chrono::steady_clock::now();
  auto direct = N::Scene::load_source(source, "rta");
  const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                           std::chrono::steady_clock::now() - start)
                           .count();
  check(direct->history_pending(), "indexed load eagerly restored histories");
  check(source->reads.size() == 4, "indexed load read count");
  std::size_t read_bytes = 0;
  for (const auto &range : source->reads) {
    check(range.first + range.second <= historical_at,
          "indexed load read historical records or journal");
    read_bytes += range.second;
  }
  projection(*direct, live.snapshot());
  check(direct->history_pending() && source->reads.size() == 4,
        "small observation/setup materialized historical records");
  auto restored = N::Scene::load(replay, "rta");
  // A complete observation explicitly materializes; every field and retained
  // history must match the unchanged independent replay reconstruction.
  equal(*direct, *restored);
  check(!direct->history_pending(), "complete snapshot left deferred records");
  check(direct->save() == replay && direct->save_snapshot() == old &&
            direct->save_indexed_snapshot() == bytes,
        "indexed materialization changed existing bytes");
  direct = N::Scene::load_source(source, "rta");
  const auto reads_before_play = source->reads.size();
  for (auto mask : {17u, 0u, 4u, 16u, 0u, 8u, 0u}) {
    direct->advance(mask);
    restored->advance(mask);
    projection(*direct, restored->snapshot());
    check(direct->history_pending() &&
              source->reads.size() == reads_before_play,
          "resumed play read retained history");
  }
  equal(*direct, *restored);
  check(direct->save() == restored->save() &&
            direct->save_snapshot() == restored->save_snapshot(),
        "suffix merge changed replay or snapshot-v1 bytes");
  direct = N::Scene::load(direct->save_indexed_snapshot(), "rta");
  direct->set_paused(true);
  restored->set_paused(true);
  direct->release_input();
  restored->release_input();
  direct->set_paused(false);
  restored->set_paused(false);
  direct->advance(0);
  restored->advance(0);
  equal(*direct, *restored);
  direct->stop();
  restored->stop();
  equal(*direct, *restored);
  auto reset = N::Scene::load_source(source, "rta");
  const auto reads_before_reset = source->reads.size();
  reset->reset();
  check(!reset->history_pending() && source->reads.size() == reads_before_reset,
        "reset unnecessarily read discarded histories");
  restored->reset();
  equal(*reset, *restored);
  if (!arithmetic && !supplied)
    std::cout << "indexed policy=" << unsigned(policy) << " N=" << ticks
              << " read_bytes=" << read_bytes << " load_us=" << elapsed
              << " total_bytes=" << bytes.size() << "\n";
  // Current authority remains usable if only historical storage corrupts.
  // Complete observations and all saves must refuse, preserving current play.
  // Reset can explicitly discard the unread history.
  source = std::make_shared<CountingSource>(bytes);
  direct = N::Scene::load_source(source, "rta");
  source->bytes.at(historical_at + 4) ^= 1;
  direct->advance(0);
  const auto before = direct->observation();
  refuses([&] { direct->snapshot(); });
  refuses([&] { direct->save(); });
  refuses([&] { direct->save_snapshot(); });
  refuses([&] { direct->save_indexed_snapshot(); });
  check(direct->history_pending() &&
            direct->observation().submissions == before.submissions &&
            direct->observation().position == before.position,
        "deferred corruption mutated current authority");
  direct->reset();
  equal(*direct, *reset);
  auto broken = bytes;
  broken.at(56) ^= 1;
  refuses([&] { N::Scene::load(broken, "rta"); });
  broken = bytes;
  broken.at(56 + word32(bytes, 24)) ^= 1;
  refuses([&] { N::Scene::load(broken, "rta"); });
  refuses([&] { N::Scene::load_source(source, "norrath"); });
  // v2 remains readable with its eager-before-mutation behavior. Derive it
  // from authored bytes by removing only v3's four-byte action count.
  auto legacy = bytes;
  const auto current_at = 56 + word32(bytes, 24);
  legacy.erase(legacy.begin() + current_at, legacy.begin() + current_at + 4);
  patch(legacy, 8, 2);
  patch(legacy, 16, std::uint32_t(legacy.size()));
  patch(legacy, 32, word32(bytes, 32) - 4);
  const auto section_crc = [](const N::Bytes &b, std::size_t at,
                              std::size_t size) {
    std::uint32_t c = ~0u;
    for (std::size_t i = at; i < at + size; ++i) {
      c ^= b[i];
      for (unsigned k = 0; k < 8; ++k)
        c = (c >> 1) ^ ((c & 1) ? 0xedb88320u : 0);
    }
    return ~c;
  };
  patch(legacy, 36, section_crc(legacy, current_at, word32(legacy, 32)));
  N::Bytes header(legacy.begin(), legacy.begin() + 56);
  checksum(header);
  patch(legacy, 20, word32(header, 20));
  auto legacy_owner = N::Scene::load(legacy, "rta");
  auto original = N::Scene::load(replay, "rta");
  equal(*legacy_owner, *original);
  legacy_owner = N::Scene::load(legacy, "rta");
  legacy_owner->advance(0);
  original->advance(0);
  equal(*legacy_owner, *original);
  // Exhausted journals refuse before any mutation or deferred read.
  N::Scene capped(cfg, data);
  for (std::size_t i = 0; i < N::MAX_ACTIONS; ++i)
    capped.set_paused(true);
  auto capped_source =
      std::make_shared<CountingSource>(capped.save_indexed_snapshot());
  auto capped_owner = N::Scene::load_source(capped_source, "rta");
  refuses([&] { capped_owner->advance(0); });
  refuses([&] { capped_owner->set_paused(false); });
  refuses([&] { capped_owner->release_input(); });
  refuses([&] { capped_owner->stop(); });
  check(capped_source->reads.size() == 4 && capped_owner->observation().paused,
        "action cap lost retained prefix or read histories");
  // Source ownership persists independently of the caller's handle.
  source = std::make_shared<CountingSource>(bytes);
  std::weak_ptr<CountingSource> weak = source;
  direct = N::Scene::load_source(source, "rta");
  source.reset();
  check(!weak.expired(), "deferred source ownership escaped Scene");
  direct->snapshot();
  check(weak.expired(), "materialized Scene retained file source");
  return read_bytes;
}
// Per-service caps must count unread records independently, not just their
// aggregate. These synthetic current-only restores exercise each reserve.
void resume_limits() {
  using Codec = R::detail::Checkpoint;
  using Access = R::detail::CheckpointAccess;
  R::creature_hp::Limits limits;
  limits.history_entries = 1;
  R::creature_hp::Memory memory(limits);
  Codec memory_input(N::Bytes(4), std::vector<std::uint32_t>{1});
  Access::run(memory, memory_input);
  memory_input.finish();
  bool refused = false;
  try {
    memory.store(0, 1, 0);
  } catch (const R::creature_hp::ResourceLimit &) {
    refused = true;
  }
  check(refused && memory.bytes().empty() && memory.log().empty(),
        "deferred memory prefix lost its store reserve");
  for (unsigned log = 0; log < 3; ++log) {
    std::vector<std::uint32_t> counts(3);
    limits.history_entries = 64;
    counts[log] = 64;
    R::creature_hp::Model hp(R::creature_hp::CallbackPolicy::ReferenceZero,
                             limits);
    Codec input(N::Bytes{}, counts);
    input.history(hp.writes, hp.calls, hp.events);
    Access::history(hp, input);
    input.finish();
    refused = false;
    hp.mem.poke(0x228, 4, Control::INITIAL_HP);
    try {
      hp.take_damage(0, 0x1000, 0, 0);
    } catch (const R::creature_hp::ResourceLimit &) {
      refused = true;
    }
    check(refused, "deferred HP prefix lost its per-log reserve");
    R::creature_movement::Model movement(
        R::creature_movement::CallbackPolicy::ReferenceZero, limits);
    Codec move_input(N::Bytes{}, counts);
    move_input.history(movement.writes, movement.calls, movement.events);
    Access::history(movement, move_input);
    move_input.finish();
    refused = false;
    try {
      if (log == 1)
        movement.vcall(0, 6, {R::creature_movement::Value::integer(0)});
      else
        movement.turn_toward(0, 0, {0, 0, 0});
    } catch (const R::creature_movement::ResourceLimit &) {
      refused = true;
    }
    check(refused, "deferred movement prefix lost its per-log reserve");
  }
  // Append bounds and stream ownership are validated before publication.
  Codec suffix(Codec::Storage::Split);
  std::vector<std::uint32_t> one{7};
  suffix.history(one);
  std::vector<std::uint32_t> full_log(65536);
  Codec merged(suffix.bytes, suffix.history_bytes, true);
  refused = false;
  try {
    merged.history(full_log);
  } catch (const std::runtime_error &) {
    refused = true;
  }
  check(refused && full_log.size() == 65536 && merged.bytes == suffix.bytes,
        "append reserve or exception stream ownership failed");
}
struct NestedHistory {
  bool value = false;
};
void checkpoint_fields(R::detail::Checkpoint &a, NestedHistory &v) {
  a.history(v.value);
}
void partition_limits() {
  using Codec = R::detail::Checkpoint;
  bool rejected = false;
  try {
    Codec oversized(N::Bytes(Codec::budget), N::Bytes(1));
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(rejected, "partition combined read budget not enforced");
  Codec write(Codec::Storage::Split);
  write.bytes.resize(Codec::budget - 8);
  std::uint64_t word = 0;
  write.history(word);
  rejected = false;
  try {
    write.history(word);
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(rejected && write.bytes.size() == Codec::budget - 8 &&
            write.history_bytes.size() == 8,
        "partition combined write budget or exception stream ownership");
  Codec bad(N::Bytes(8), N::Bytes{2});
  bool boolean = false;
  rejected = false;
  try {
    bad.history(boolean);
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  bad.value(word);
  check(rejected && word == 0 && bad.bytes.size() == 8 &&
            bad.history_bytes == N::Bytes{2},
        "partition exception failed to restore active stream");
  Codec empty(N::Bytes{}, N::Bytes{});
  empty.finish();
  check(empty.bytes.empty() && empty.history_bytes.empty(),
        "empty partition changed storage");
  Codec nested(Codec::Storage::Split);
  NestedHistory v;
  rejected = false;
  try {
    nested.history(v);
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  nested.value(word);
  nested.history(boolean);
  check(rejected && nested.bytes.size() == 8 &&
            nested.history_bytes == N::Bytes{0},
        "nested history refusal did not restore stream ownership");
  Codec inline_codec;
  inline_codec.history(v);
  check(inline_codec.bytes == N::Bytes{0} && inline_codec.history_bytes.empty(),
        "history annotations changed inline byte order");
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc == 2 && std::string(argv[1]) == "--resume") {
      indexed(Control::MovementPolicy::DryCreature, 0, false);
      indexed(Control::MovementPolicy::Isolated, 0, false);
      resume_limits();
      std::cout << checks << " resume checks passed\n";
      return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--indexed") {
      resume_limits();
      for (auto p : {Control::MovementPolicy::Isolated,
                     Control::MovementPolicy::DryCreature})
        for (unsigned a = 0; a < 16; ++a)
          for (bool supplied : {false, true})
            indexed(p, a, supplied);
      const auto short_work =
          indexed(Control::MovementPolicy::DryCreature, 0, false, 12);
      for (unsigned n : {0u, 63u, 64u, 511u, 512u}) {
        const auto work =
            indexed(Control::MovementPolicy::DryCreature, 0, false, n);
        check(work <= short_work + 1024 && short_work <= work + 1024,
              "initial load work grew with retained play history");
      }
      std::cout << checks
                << " indexed snapshot checks passed; exact trace FNV1a64="
                << std::hex << digest << "\n";
      return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--partition") {
      for (auto p : {Control::MovementPolicy::Isolated,
                     Control::MovementPolicy::DryCreature})
        for (unsigned a = 0; a < 16; ++a)
          for (bool supplied : {false, true})
            partition(p, a, supplied);
      // Boot (one submission, zero ready ticks), the history/cache boundary,
      // and the final stopped checkpoint must all retain exact histories.
      for (unsigned n : {0u, 63u, 64u, 511u, 512u})
        partition(Control::MovementPolicy::DryCreature, 0, false, n);
      partition_limits();
      std::cout << checks
                << " partition checks passed; exact partition FNV1a64="
                << std::hex << digest << "\n";
      return 0;
    }
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
