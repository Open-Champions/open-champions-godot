#include "champions/netplay.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace R = champions::simulation::rta;
namespace N = R::netplay;
namespace W = R::world_queries;

namespace {
unsigned checks = 0;

void check(bool value, const char *why) {
  ++checks;
  if (!value)
    throw std::runtime_error(why);
}

template <class F> void rejects(F fn, const char *why) {
  bool caught = false;
  try {
    fn();
  } catch (const N::Error &) {
    caught = true;
  }
  check(caught, why);
}

R::Raw raw_int(int value) {
  return R::from_int(static_cast<std::uint32_t>(value));
}

// Authored flat pad. Sample 0 is world (x0 * 4). 1024 samples at x0 = -512
// cover about ±2048, inside the 4,194,304 sample cap.
W::Snapshot flat_ground() {
  W::LevelData data;
  constexpr std::int64_t origin = -512, count = 1024;
  data.tg_x0 = data.tg_y0 = -20;
  data.tg_w = data.tg_h = 40;
  data.grid.assign(40 * 40, {0});
  W::HeightField field;
  field.x0 = field.y0 = origin;
  field.w = field.h = count;
  field.samples.assign(static_cast<std::size_t>(count * count), 0);
  data.heightfields.push_back(std::move(field));
  const std::int64_t lo = origin * 4 - 8;
  const std::int64_t hi = (origin + count - 1) * 4 + 8;
  data.elements.push_back(
      W::make_element(0, lo, lo, hi, hi, 0, 0, 0, 0, 0, 32767, 0));
  return {std::move(data), {}};
}

N::Placement fight(int peers, bool creature = true) {
  N::Placement placement;
  const int xs[4] = {0, 48, 0, -48};
  const int ys[4] = {0, 0, 48, 0};
  for (int slot = 0; slot < peers; ++slot) {
    placement.player_present[std::size_t(slot)] = true;
    placement.players[std::size_t(slot)] = {raw_int(xs[slot]), raw_int(ys[slot]),
                                            0};
  }
  placement.creature_present = creature;
  placement.creature = {raw_int(24), raw_int(24), 0};
  return placement;
}

// Centered until frame 16, then ±24 on one stick, reversing every 60 frames.
// Slots 0 and 1 use stick X in opposite directions. Slots 2 and 3 use stick Y.
// Attack is the bound 0x10 bit while the stick is deflected. Pressure stays 0:
// those bytes are not what the combat pad applies as movement.
N::Sample script(int slot, std::uint32_t frame) {
  N::Sample sample;
  sample.camera_flag = true;
  const bool early = frame < 16;
  const bool outward = ((frame / 60) % 2) == 0;
  const std::uint8_t go = early ? 128 : (outward ? 152 : 104);
  const std::uint8_t back = early ? 128 : (outward ? 104 : 152);
  if (slot < 2)
    sample.sticks[0] = slot == 0 ? go : back;
  else
    sample.sticks[1] = slot == 2 ? go : back;
  if (!early)
    sample.buttons = N::ATTACK_BIT;
  return sample;
}

void drive(N::Session &session, std::size_t ticks) {
  unsigned stall = 0;
  while (session.sim_ticks() < ticks || !session.aligned()) {
    const auto before = session.sim_ticks();
    std::vector<N::Sample> samples;
    const auto frame = static_cast<std::uint32_t>(session.render_steps());
    for (int slot = 0; slot < session.peers(); ++slot)
      samples.push_back(script(slot, frame));
    session.step(samples);
    if (session.sim_ticks() == before) {
      if (++stall >= 240)
        throw std::runtime_error("lockstep stalled");
    } else {
      stall = 0;
    }
  }
}

void traces_match(const N::Session &session, const char *why) {
  const auto first = session.trace_bytes(0);
  check(first.size() == session.sim_ticks() * 24, "trace size");
  for (int slot = 1; slot < session.peers(); ++slot)
    check(session.trace_bytes(slot) == first, why);
  check(session.synced(), "lockstep reported out of sync");
}

void loopback() {
  N::LoopbackTransport transport(2);
  rejects([&] { N::LoopbackTransport(1); }, "loopback rejects 1 peer");
  rejects([&] { N::LoopbackTransport(5); }, "loopback rejects 5 peers");
  R::netwire::Sent sent;
  sent.peer = 1;
  sent.data = {0x3f, 0xbd, 0, 0};
  transport.send(0, sent);
  check(transport.take(1).empty(), "loopback hides a packet until deliver");
  check(transport.take(0).empty(), "loopback does not deliver to the sender");
  transport.deliver();
  auto inbox = transport.take(1);
  check(inbox.size() == 1 && inbox[0] == sent.data, "loopback delivers next frame");
  check(transport.take(1).empty(), "loopback take clears the inbox");
}

void long_session(const W::Snapshot &ground, int peers, std::size_t ticks,
                  bool creature) {
  N::Session session(peers, ground, fight(peers, creature));
  const auto midway = ticks / 2;
  drive(session, midway);
  const auto saved = session.save();
  auto loaded = N::Session::load(saved, ground);
  check(loaded.trace_bytes(0) == session.trace_bytes(0), "long save/load");
  drive(session, ticks);
  drive(loaded, ticks);
  traces_match(session, "long peers differ");
  check(session.trace_bytes(0) == loaded.trace_bytes(0), "long continuation");
  check(session.sim_ticks() >= ticks, "long tick count");
  check(session.input_packets() > 0, "long input packets");
  std::cout << "peers=" << peers << " sim_ticks=" << session.sim_ticks()
            << " trace_bytes=" << session.trace_bytes(0).size()
            << " creature=" << (creature ? 1 : 0)
            << " input_packets=" << session.input_packets()
            << " synced=" << (session.synced() ? 1 : 0) << "\n";
}

void short_session(const W::Snapshot &ground, int peers, std::size_t ticks) {
  N::Session a(peers, ground, fight(peers));
  N::Session b(peers, ground, fight(peers));
  drive(a, ticks);
  drive(b, ticks);
  traces_match(a, "peers in one session differ");
  check(a.trace_bytes(0) == b.trace_bytes(0), "two sessions differ");
  check(a.input_packets() > 0, "no input packets");
  check(a.sim_ticks() >= ticks, "short session tick count");
  const auto saved = a.save();
  auto loaded = N::Session::load(saved, ground);
  check(loaded.trace_bytes(0) == a.trace_bytes(0), "save/load trace");
  check(loaded.save() == saved, "save is stable");
  const auto further = ticks + 40;
  drive(a, further);
  drive(loaded, further);
  traces_match(a, "peers diverged after save");
  check(a.trace_bytes(0) == loaded.trace_bytes(0), "continuation diverged");
  check(a.synced() && loaded.synced(), "continuation out of sync");
}

void refusals(const W::Snapshot &ground) {
  rejects([&] { N::Session(1, ground, fight(2)); }, "session rejects 1 peer");
  rejects([&] { N::Session(5, ground, fight(2)); }, "session rejects 5 peers");
  auto placement = fight(2);
  placement.players[0] = {raw_int(100000), 0, 0};
  rejects([&] { N::Session(2, ground, placement); }, "spawn with no floor");
  N::Session session(2, ground, fight(2));
  rejects([&] { session.step({}); }, "sample count");
  drive(session, 4);
  auto bytes = session.save();
  bytes[0] ^= 1;
  rejects([&] { N::Session::load(bytes, ground); }, "save magic");
  bytes = session.save();
  bytes.resize(8);
  rejects([&] { N::Session::load(bytes, ground); }, "truncated save");
  auto other = ground;
  other.data.elements[0].min_x += 1;
  rejects([&] { N::Session::load(session.save(), other); }, "terrain identity");
}
} // namespace

int main(int argc, char **argv) {
  std::size_t long_ticks = 10000;
  if (argc > 1)
    long_ticks = static_cast<std::size_t>(std::stoul(argv[1]));
  R::ScopedFpu fixed{R::Fpu{R::Fpu::Model::Ps2, false, false}};
  try {
    loopback();
    const auto ground = flat_ground();
    refusals(ground);
    short_session(ground, 2, 48);
    short_session(ground, 3, 48);
    short_session(ground, 4, 48);
    std::cout << "short peers=2,3,4 sim_ticks=48 checks=" << checks << "\n";
    // Iterator retention in Full combat: 4 list walks per tick, plus one
    // spell-list iterator per player and per creature, refused at 65536.
    // 2 peers and no creature stay under that cap at 10000. 3 peers refuse
    // after 9362, 4 peers after 8192, so those runs stop short of the cap.
    if (long_ticks >= 10000) {
      long_session(ground, 2, 10000, false);
      long_session(ground, 2, 9000, true);
      long_session(ground, 3, 9000, false);
      long_session(ground, 4, 8000, false);
    } else {
      long_session(ground, 2, long_ticks, false);
    }
    std::cout << "checks=" << checks << "\n";
  } catch (const std::exception &error) {
    std::cerr << "netplay failed: " << error.what() << "\n";
    return 1;
  }
  return 0;
}
