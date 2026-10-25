#include "champions/native_save.hpp"
#include "session_creature_trace.hpp"
#include <functional>
#include <iostream>

namespace {
namespace R = champions::simulation::rta;
namespace N = R::native_save;
namespace Control = R::session_control;
namespace W = R::world_queries;
using namespace champions::test::session_trace;
std::size_t checks = 0;
std::uint64_t trace_digest = 14695981039346656037ULL;
void check(bool ok, const char *message) {
  ++checks;
  if (!ok)
    throw std::runtime_error(message);
}
void refuses(const std::function<void()> &fn) {
  bool refused = false;
  try {
    fn();
  } catch (const N::Error &) {
    refused = true;
  }
  check(refused, "invalid save accepted");
}
// Same complete formatter as the existing Python-oracle control trace,
// including all Controller fields, raw HP memory, histories, World, network and
// terrain.
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
std::uint32_t command(unsigned i) {
  static constexpr std::uint32_t cmds[]{17, 17, 0, 20, 4,  8,
                                        18, 2,  0, 9,  16, 0};
  return cmds[i % 12];
}
void equal(const N::Scene &a, const Control::Controller &b) {
  const auto actual = full(a.snapshot());
  check(actual == full(b.snapshot()), "complete continuation trace differs");
  for (unsigned char v : actual) {
    trace_digest ^= v;
    trace_digest *= 1099511628211ULL;
  }
  trace_digest ^= 10;
  trace_digest *= 1099511628211ULL;
}
void trace(Control::MovementPolicy policy, unsigned arithmetic, bool supplied) {
  Control::Config cfg;
  cfg.movement = policy;
  cfg.max_ready_ticks =
      policy == Control::MovementPolicy::DryCreature ? 512 : 2000;
  cfg.model = static_cast<R::Fpu::Model>(arithmetic / 4);
  cfg.mul_bias = arithmetic & 2;
  cfg.div_nearest = arithmetic & 1;
  cfg.shared_seed = 0x12345678;
  std::optional<W::LevelData> d;
  if (supplied)
    d = terrain();
  N::Scene saved(cfg, d);
  Control::Controller uninterrupted(cfg, d);
  equal(saved, uninterrupted);
  // N=12 actual ready ticks, not 12 submissions. Leave a queued damage edge
  // on the final submission, which must fire only after reload's next frame.
  for (unsigned i = 0; i < 13; ++i) {
    saved.advance(command(i));
    uninterrupted.advance(command(i));
  }
  check(saved.observation().ready_ticks == 12,
        "checkpoint not at N ready ticks");
  check(!saved.snapshot().pending_damage_frames.empty(),
        "missing queued edge coverage");
  const auto bytes = saved.save();
  for (auto v : bytes) {
    trace_digest ^= v;
    trace_digest *= 1099511628211ULL;
  }
  auto reloaded = N::Scene::load(bytes, "rta");
  check(reloaded->save() == bytes, "save reload bytes differ");
  equal(*reloaded, uninterrupted);
  // Compare the full retained trace after every continuation operation.
  for (unsigned i = 13; i < 30; ++i) {
    if (i == 16) {
      reloaded->set_paused(true);
      uninterrupted.set_paused(true);
      equal(*reloaded, uninterrupted);
    }
    if (i == 18) {
      reloaded->set_paused(false);
      uninterrupted.set_paused(false);
    }
    if (i == 23) {
      reloaded->release_input();
      uninterrupted.release_input();
      equal(*reloaded, uninterrupted);
    }
    reloaded->advance(command(i));
    uninterrupted.advance(command(i));
    equal(*reloaded, uninterrupted);
  }
  auto second = N::Scene::load(reloaded->save(), "rta");
  equal(*second, uninterrupted);
}
void patch(N::Bytes &b, std::size_t at, std::uint32_t value) {
  for (unsigned i = 0; i < 4; ++i) {
    b[at + i] = std::uint8_t(value);
    value >>= 8;
  }
}
void checksum(N::Bytes &b) {
  // Independent bitwise IEEE CRC32 reference, checked against saved bytes.
  patch(b, 20, 0);
  std::uint32_t crc = ~0u;
  for (auto v : b) {
    crc ^= v;
    for (unsigned i = 0; i < 8; ++i)
      crc = (crc & 1) ? (crc >> 1) ^ 0xedb88320u : crc >> 1;
  }
  patch(b, 20, ~crc);
}
void malformed() {
  N::Scene scene;
  scene.advance(17);
  scene.advance(0);
  auto bytes = scene.save();
  const auto before = full(scene.snapshot());
  auto independent = bytes;
  checksum(independent);
  check(independent == bytes, "independent CRC32 disagrees");
  check(bytes[8] == 1 && bytes[9] == 0 && bytes[12] == 1 && bytes[13] == 0,
        "header endian anchor");
  check(bytes[28] == 17 && bytes[29] == 0 && bytes[32] == 0xd0 &&
            bytes[33] == 7,
        "payload endian anchor");
  // Every truncation point and every individual byte mutation must refuse.
  for (std::size_t n = 0; n < bytes.size(); ++n) {
    auto b = N::Bytes(bytes.begin(), bytes.begin() + n);
    refuses([&] { N::Scene::load(b, "rta"); });
  }
  for (std::size_t i = 0; i < bytes.size(); ++i) {
    auto b = bytes;
    b[i] ^= 1;
    refuses([&] { N::Scene::load(b, "rta"); });
  }
  refuses([&] { N::Scene::load(bytes, "norrath"); });
  refuses([&] { N::Scene::load(bytes, ""); });
  auto edited = [&](std::size_t at, std::uint32_t v) {
    auto b = bytes;
    patch(b, at, v);
    checksum(b);
    refuses([&] { N::Scene::load(b, "rta"); });
  };
  edited(8, 2);
  edited(12, 2);
  edited(32, 0);
  edited(32, 2001);
  edited(36, 65537); // terrain heightfield hard cap
  edited(65, 10001); // action count (no supplied terrain)
  for (auto [at, v] : std::vector<std::pair<std::size_t, std::uint8_t>>{
           {24, 4}, {25, 2}, {26, 2}, {27, 2}, {64, 2}, {69, 4}, {70, 32}}) {
    auto b = bytes;
    b[at] = v;
    checksum(b);
    refuses([&] { N::Scene::load(b, "rta"); });
  }
  auto b = bytes;
  b.push_back(0);
  patch(b, 16, std::uint32_t(b.size()));
  checksum(b);
  refuses([&] { N::Scene::load(b, "rta"); });
  // Re-checksummed but inconsistent continuation witness refuses.
  b = bytes;
  b[73] ^= 1;
  checksum(b);
  refuses([&] { N::Scene::load(b, "rta"); });
  b = bytes;
  patch(b, 65, 0xffffffff);
  checksum(b);
  refuses([&] { N::Scene::load(b, "rta"); });
  check(full(scene.snapshot()) == before, "load refusal mutated live scene");
  refuses([&] { scene.advance(32); });
  check(full(scene.snapshot()) == before, "invalid input mutated live scene");
  b.resize(N::MAX_BYTES + 1);
  refuses([&] { N::Scene::load(b, "rta"); });
}
void supplied_refusals() {
  N::Scene scene({}, terrain());
  const auto bytes = scene.save();
  auto reject = [&](std::size_t at, std::uint32_t value) {
    auto b = bytes;
    patch(b, at, value);
    checksum(b);
    refuses([&] { N::Scene::load(b, "rta"); });
  };
  reject(85, 0xffffffff); // signed grid width outside documented domain
  reject(101, 1048577);   // grid count exceeds hard cap
  reject(105, 4194305);   // cell reference count exceeds aggregate cap
  reject(109, 99);        // out-of-range element reference
  reject(425, 65537);     // element count
  reject(661, 65537);     // heightfield count
  reject(721, 4194305);   // sample count
  reject(721, 1);         // samples do not match dimensions
}
void whole_budget() {
  Control::Config cfg;
  cfg.movement = Control::MovementPolicy::DryCreature;
  cfg.max_ready_ticks = 512;
  N::Scene scene(cfg);
  Control::Controller baseline(cfg);
  for (unsigned i = 0; i < 512; ++i) {
    scene.advance(i % 2 ? 0 : 17);
    baseline.advance(i % 2 ? 0 : 17);
  }
  check(scene.observation().ready_ticks == 511, "long checkpoint ready ticks");
  auto reloaded = N::Scene::load(scene.save(), "rta");
  equal(*reloaded, baseline);
  reloaded->advance(0);
  baseline.advance(0);
  equal(*reloaded, baseline);
  check(reloaded->observation().ready_ticks == 512 &&
            reloaded->observation().stopped,
        "whole budget reload did not stop at cap");
}
void failed_prefix() {
  Control::Config cfg;
  cfg.movement = Control::MovementPolicy::DryCreature;
  cfg.max_ready_ticks = 512;
  W::Limits limits;
  limits.query_work = 0;
  N::Scene scene(cfg, {}, limits);
  scene.advance(1);
  check(scene.advance(1).failed, "missing failed prefix coverage");
  refuses([&] { scene.save(); });
  scene.reset();
  check(!scene.observation().failed, "reset retained failure");
  auto reset = N::Scene::load(scene.save(), "rta");
  check(full(reset->snapshot()) == full(scene.snapshot()),
        "failed reset reload differs");
}
void lifecycle() {
  Control::Config cfg;
  cfg.movement = Control::MovementPolicy::DryCreature;
  cfg.max_ready_ticks = 3;
  N::Scene scene(cfg);
  Control::Controller baseline(cfg);
  scene.advance(17);
  baseline.advance(17);
  scene.set_paused(true);
  baseline.set_paused(true);
  auto paused = N::Scene::load(scene.save(), "rta");
  equal(*paused, baseline);
  paused->set_paused(false);
  baseline.set_paused(false);
  for (unsigned i = 0; i < 3; ++i) {
    paused->advance(16);
    baseline.advance(16);
  }
  check(paused->observation().stopped, "tick limit did not stop");
  auto limited = N::Scene::load(paused->save(), "rta");
  equal(*limited, baseline);
  limited->reset();
  baseline.reset();
  equal(*limited, baseline);
  limited->advance(16);
  baseline.advance(16);
  limited->stop();
  baseline.stop();
  auto stopped = N::Scene::load(limited->save(), "rta");
  equal(*stopped, baseline);
  stopped->reset();
  baseline.reset();
  equal(*stopped, baseline);
  // Cheap no-op operations exercise a strict pre-mutation action bound.
  for (std::size_t i = 0; i < N::MAX_ACTIONS; ++i)
    stopped->release_input();
  const auto bytes = stopped->save();
  refuses([&] { stopped->set_paused(true); });
  check(stopped->save() == bytes, "action cap mutated state");
  auto bounded = N::Scene::load(bytes, "rta");
  check(bounded->save() == bytes, "maximum action stream failed");
  bounded->reset();
  check(bounded->save().size() < bytes.size(), "reset did not compact journal");
}
// Wave 12: the optional client start position (level-spawn story start).
// It survives replay saves, indexed snapshots and play continuations; a scene
// without it keeps the historical bytes and the origin start.
void client_start() {
  Control::Config c;
  c.start = W::Vec3{0x41200000u, 0xc1a00000u, 0}; // (10, -20, 0)
  N::Scene a(c, terrain());
  check(a.observation().position[0] == 0x41200000u &&
            a.observation().position[1] == 0xc1a00000u,
        "the actor starts at the supplied position");
  for (unsigned i = 0; i < 6; ++i)
    a.advance(command(i));
  const auto bytes = a.save();
  auto b = N::Scene::load(bytes, "rta");
  check(b->setup().config.start == c.start, "a replay save keeps the start");
  check(full(b->snapshot()) == full(a.snapshot()), "the reloaded start replays identically");
  check(b->save() == bytes, "the start encoding is canonical");
  auto indexed = N::Scene::load(a.save_indexed_snapshot(), "rta");
  check(full(indexed->snapshot()) == full(a.snapshot()), "an indexed snapshot keeps the start");
  N::Scene origin(Control::Config{}, terrain());
  check(origin.observation().position == W::Vec3{} && !N::Scene::load(origin.save(), "rta")->setup().config.start,
        "no start keeps the origin and the historical encoding");
  auto rejected = [](const std::function<void()> &fn) {
    try {
      fn();
    } catch (const std::exception &) {
      return true;
    }
    return false;
  };
  check(rejected([&] { N::Scene none(c); }), "a start needs supplied terrain");
  Control::Config bad = c;
  bad.start = W::Vec3{0x7f800000u, 0, 0};
  check(rejected([&] { N::Scene infinite(bad, terrain()); }), "a non-finite start is refused");
  Control::Config play;
  play.movement = Control::MovementPolicy::DryCreature;
  play.max_ready_ticks = Control::MAX_CREATURE_READY_TICKS;
  play.play_window = R::play_session::DEFAULT_WINDOW;
  play.start = c.start;
  N::Scene p(play, terrain());
  for (unsigned i = 0; i < 6; ++i)
    p.advance(command(i));
  auto q = N::Scene::load(p.save_continuation(), "rta");
  check(q->setup().config.start == c.start &&
            q->observation().position == p.observation().position,
        "a play continuation keeps the start");
}
} // namespace
int main() {
  try {
    client_start();
    for (auto policy : {Control::MovementPolicy::Isolated,
                        Control::MovementPolicy::DryCreature})
      for (unsigned arithmetic = 0; arithmetic < 16; ++arithmetic)
        for (bool supplied : {false, true})
          trace(policy, arithmetic, supplied);
    malformed();
    supplied_refusals();
    lifecycle();
    failed_prefix();
    whole_budget();
    std::cout << checks
              << " native save checks passed; 64 complete traces: N=12 ready "
                 "ticks, reload, continue; trace FNV1a64="
              << std::hex << trace_digest << "\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
