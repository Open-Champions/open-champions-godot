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

// Centered until frame 16, then full deflection (bytes 255 and 0) on one
// stick, reversing every 60 frames. ±24 would sit inside pad_apply's radial
// dead zone (STICK_DEAD 0.32 in 1/128 steps) and never move anyone. Slots 0
// and 1 use stick X in opposite directions, slots 2 and 3 stick Y. The camera
// flag stays clear (follow camera); a set flag makes choose_pad hand the
// player ZERO_PAD. With `attack`, the bound 0x10 bit is pressed for one frame
// every 12 frames from frame 24, the research WalkAndCombo cadence: pad_apply
// reports edges, so a held bit swings once. Pressure stays 0.
N::Sample script(int slot, std::uint32_t frame, bool attack) {
  N::Sample sample;
  sample.camera_flag = false;
  const bool early = frame < 16;
  const bool outward = ((frame / 60) % 2) == 0;
  const std::uint8_t go = early ? 128 : (outward ? 255 : 0);
  const std::uint8_t back = early ? 128 : (outward ? 0 : 255);
  if (slot < 2)
    sample.sticks[0] = slot == 0 ? go : back;
  else
    sample.sticks[1] = slot == 2 ? go : back;
  if (attack && frame >= 24 && frame % 12 == 0)
    sample.buttons = N::ATTACK_BIT;
  return sample;
}

void drive(N::Session &session, std::size_t ticks, bool attack) {
  unsigned stall = 0;
  while (session.sim_ticks() < ticks || !session.aligned()) {
    const auto before = session.sim_ticks();
    std::vector<N::Sample> samples;
    const auto frame = static_cast<std::uint32_t>(session.render_steps());
    for (int slot = 0; slot < session.peers(); ++slot)
      samples.push_back(script(slot, frame, attack));
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
  check(first.size() == session.sim_ticks() * N::TRACE_RECORD, "trace size");
  for (int slot = 1; slot < session.peers(); ++slot)
    check(session.trace_bytes(slot) == first, why);
  check(session.synced(), "lockstep reported out of sync");
}

std::uint32_t trace_word(const std::vector<std::uint8_t> &trace,
                         std::size_t tick, std::size_t word) {
  const auto at = tick * N::TRACE_RECORD + word * 4;
  return std::uint32_t(trace[at]) | (std::uint32_t(trace[at + 1]) << 8) |
         (std::uint32_t(trace[at + 2]) << 16) |
         (std::uint32_t(trace[at + 3]) << 24);
}

// Words 1..4 are player hit points, 5 the creature, 6..17 the positions.
bool trace_changes(const std::vector<std::uint8_t> &trace, std::size_t first,
                   std::size_t last) {
  const auto ticks = trace.size() / N::TRACE_RECORD;
  for (std::size_t tick = 1; tick < ticks; ++tick)
    for (std::size_t word = first; word <= last; ++word)
      if (trace_word(trace, tick, word) != trace_word(trace, 0, word))
        return true;
  return false;
}

// The scripted stick and attack bit have to reach the combat pad: some
// player leaves the spawn, and some player or creature hit-point word moves.
// A walking run must still be moving on its last tick.
void input_reaches_simulation(const N::Session &session, bool hit_points,
                              bool walking = false) {
  const auto trace = session.trace_bytes(0);
  check(trace_changes(trace, 6, 17), "no player left its spawn");
  if (hit_points)
    check(trace_changes(trace, 1, 5), "no hit-point word changed");
  if (walking) {
    const auto last = trace.size() / N::TRACE_RECORD - 1;
    bool moving = false;
    for (std::size_t word = 6; word <= 17; ++word)
      moving = moving ||
               trace_word(trace, last, word) != trace_word(trace, last - 1, word);
    check(moving, "players stopped walking");
  }
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
  drive(session, midway, creature);
  const auto saved = session.save();
  auto loaded = N::Session::load(saved, ground);
  check(loaded.trace_bytes(0) == session.trace_bytes(0), "long save/load");
  drive(session, ticks, creature);
  drive(loaded, ticks, creature);
  traces_match(session, "long peers differ");
  input_reaches_simulation(session, creature, !creature);
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
  drive(a, ticks, true);
  drive(b, ticks, true);
  traces_match(a, "peers in one session differ");
  input_reaches_simulation(a, false);
  check(a.trace_bytes(0) == b.trace_bytes(0), "two sessions differ");
  check(a.input_packets() > 0, "no input packets");
  check(a.sim_ticks() >= ticks, "short session tick count");
  const auto saved = a.save();
  auto loaded = N::Session::load(saved, ground);
  check(loaded.trace_bytes(0) == a.trace_bytes(0), "save/load trace");
  check(loaded.save() == saved, "save is stable");
  const auto further = ticks + 40;
  drive(a, further, true);
  drive(loaded, further, true);
  traces_match(a, "peers diverged after save");
  input_reaches_simulation(a, true);
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
  drive(session, 4, false);
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
    // spell-list iterator per player and per live creature, refused at
    // 65536. 2 peers stay under that cap at 10000 (the creature dies early
    // in the attack run). 3 peers refuse after 9362 and 4 peers after 8192,
    // so those runs stop short of the cap. Walking runs carry no creature: a
    // walking player beside an armed Skelight reaches the unported
    // world_findFreeSpot.
    if (long_ticks >= 10000) {
      long_session(ground, 2, 10000, false);
      long_session(ground, 2, 10000, true);
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
