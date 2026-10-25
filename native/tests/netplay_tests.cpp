#include "champions/netplay.hpp"
#include "../tools/netplay_level_lmp.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

// Largest single allocation, for the bounded-load check.
static std::size_t largest_allocation = 0;
void *operator new(std::size_t size) {
  if (size > largest_allocation)
    largest_allocation = size;
  if (void *p = std::malloc(size ? size : 1))
    return p;
  throw std::bad_alloc();
}
void operator delete(void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }

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

// Authored flat pad past x = 100,000: 256 x 256 samples from sample
// (25000, -128), world x 100,000 .. 101,020 and y -512 .. 508.
W::Snapshot far_ground() {
  W::LevelData data;
  constexpr std::int64_t x0 = 25000, y0 = -128, count = 256;
  data.tg_x0 = 624;
  data.tg_y0 = -5;
  data.tg_w = 10;
  data.tg_h = 10;
  data.grid.assign(10 * 10, {0});
  W::HeightField field;
  field.x0 = x0;
  field.y0 = y0;
  field.w = field.h = count;
  field.samples.assign(static_cast<std::size_t>(count * count), 0);
  data.heightfields.push_back(std::move(field));
  data.elements.push_back(W::make_element(0, x0 * 4 - 8, y0 * 4 - 8,
                                          (x0 + count - 1) * 4 + 8,
                                          (y0 + count - 1) * 4 + 8, 0, 0, 0,
                                          0, 0, 32767, 0));
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
// player ZERO_PAD. Pressure stays 0. The bound 0x10 bit is pressed for one
// frame (pad_apply reports edges, so a held bit swings once):
// - Walk: never.
// - Mash: every 12 frames from frame 24 for the whole run, the research
//   WalkAndCombo cadence. Each press past the swing window chains the next
//   swing, so Player_run's state 1 never ends and the player never walks
//   again (research t3/research-combat-walk difftest_combat_walk walk.mash.12).
// - Swing: one press (frame 24), then the stick alone. State 1 ends 40 ticks
//   after the swing starts with the stick past 0.8 (walk.once), and the
//   player walks on. The Skelight survives the two swings.
// - Combo: four presses (frames 24, 36, 48, 60; 36 and 60 fall inside the
//   swing window, 48 chains a second swing), then the stick alone. State 1
//   ends 40 ticks after the last swing start (walk.mash.stop).
enum class Input { Walk, Mash, Swing, Combo };
std::uint32_t last_press(Input input) {
  return input == Input::Mash ? 0xffffffffu : input == Input::Combo ? 60 : 24;
}
N::Sample script(int slot, std::uint32_t frame, Input input) {
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
  const bool press = input != Input::Walk && frame >= 24 &&
                     frame % 12 == 0 && frame <= last_press(input);
  if (press)
    sample.buttons = N::ATTACK_BIT;
  return sample;
}

void drive(N::Session &session, std::size_t ticks, Input input) {
  unsigned stall = 0;
  while (session.sim_ticks() < ticks || !session.aligned()) {
    const auto before = session.sim_ticks();
    std::vector<N::Sample> samples;
    const auto frame = static_cast<std::uint32_t>(session.render_steps());
    for (int slot = 0; slot < session.peers(); ++slot)
      samples.push_back(script(slot, frame, input));
    session.step(samples);
    if (session.sim_ticks() == before) {
      if (++stall >= 240)
        throw std::runtime_error("lockstep stalled");
    } else {
      stall = 0;
    }
  }
}

// FNV-1a 64 of a trace, printed so arm64 and x86_64 logs can be compared.
std::uint64_t fnv(const std::vector<std::uint8_t> &bytes) {
  std::uint64_t h = 0xcbf29ce484222325ull;
  for (auto b : bytes)
    h = (h ^ b) * 0x100000001b3ull;
  return h;
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

// The screen clamp's planes are the research fixture's authored PLANES
// (test_ref_combat_full.py: everyone above z = -1000 is inside), not a
// box. Two walking players beyond x = 100,000 must still be free to move
// further out along x.
void research_planes() {
  const auto ground = far_ground();
  N::Placement placement;
  const int xs[2] = {100400, 100600};
  for (int slot = 0; slot < 2; ++slot) {
    placement.player_present[std::size_t(slot)] = true;
    placement.players[std::size_t(slot)] = {raw_int(xs[slot]), 0, 0};
  }
  N::Session session(2, ground, placement);
  drive(session, 150, Input::Walk);
  traces_match(session, "far peers differ");
  const auto trace = session.trace_bytes(0);
  bool beyond = false;
  for (std::size_t tick = 0; tick < session.sim_ticks(); ++tick)
    for (int slot = 0; slot < 2; ++slot) {
      const auto x = trace_word(trace, tick, 6 + 3 * std::size_t(slot));
      beyond = beyond || R::lt(raw_int(xs[slot] + 1), x);
    }
  check(beyond, "screen clamp held a player inside an invented box");
}

// Authored LMP: one record naming "a.world" (4 payload bytes) and a count
// word whose low 16 bits are 1. FORMATS.md: the LMP count is u32.
std::vector<std::uint8_t> authored_lmp(std::uint32_t count) {
  std::vector<std::uint8_t> lmp(4 + 12, 0);
  auto put = [&](std::size_t at, std::uint32_t value) {
    for (unsigned k = 0; k < 4; ++k)
      lmp[at + k] = std::uint8_t(value >> (8 * k));
  };
  put(0, count);
  const std::uint32_t name_at = 16, data_at = 24;
  put(4, name_at);
  put(8, data_at);
  put(12, 4);
  for (char c : std::string("a.world"))
    lmp.push_back(std::uint8_t(c));
  lmp.push_back(0);
  lmp.insert(lmp.end(), {1, 2, 3, 4});
  return lmp;
}

void lmp_count_is_u32() {
  namespace L = champions::netplay_level;
  std::vector<std::uint8_t> world, objects;
  auto good = authored_lmp(1);
  bool found = false;
  L::LmpRefusals refusals;
  found = L::lmp_world(good.data(), good.size(), world, objects, refusals);
  check(found && world == std::vector<std::uint8_t>{1, 2, 3, 4} &&
            refusals.count == 0,
        "authored LMP world");
  auto wide = authored_lmp(0x00010001u);
  found = L::lmp_world(wide.data(), wide.size(), world, objects, refusals);
  check(!found && refusals.count == 1, "LMP count read as u16");
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

// Words 6..17: whether some player position word changes from tick - 1.
bool moved_at(const std::vector<std::uint8_t> &trace, std::size_t tick) {
  for (std::size_t word = 6; word <= 17; ++word)
    if (trace_word(trace, tick, word) != trace_word(trace, tick - 1, word))
      return true;
  return false;
}

void long_session(const W::Snapshot &ground, int peers, std::size_t ticks,
                  bool creature, Input input) {
  N::Session session(peers, ground, fight(peers, creature));
  const auto midway = ticks / 2;
  drive(session, midway, input);
  const auto saved = session.save();
  auto loaded = N::Session::load(saved, ground);
  check(loaded.trace_bytes(0) == session.trace_bytes(0), "long save/load");
  drive(session, ticks, input);
  drive(loaded, ticks, input);
  traces_match(session, "long peers differ");
  const bool walking = input != Input::Mash;
  input_reaches_simulation(session, creature, walking);
  check(session.trace_bytes(0) == loaded.trace_bytes(0), "long continuation");
  check(session.sim_ticks() >= ticks, "long tick count");
  check(session.input_packets() > 0, "long input packets");
  for (int slot = 1; slot < peers; ++slot)
    check(session.free_spot_searches(slot) == session.free_spot_searches(0),
          "peers made different free-spot searches");
  check(loaded.free_spot_searches(0) == session.free_spot_searches(0),
        "the replay made different free-spot searches");
  if (creature && walking)
    check(session.free_spot_searches(0) > 0,
          "a walking player beside the Skelight never reached world_findFreeSpot");
  if (input == Input::Swing || input == Input::Combo) {
    // The players walk again once the last swing has ended (its press plus
    // 40 ticks, well inside 200), on all but a few later ticks.
    const auto trace = session.trace_bytes(0);
    std::size_t still = 0;
    for (std::size_t tick = 200; tick < session.sim_ticks(); ++tick)
      still += moved_at(trace, tick) ? 0 : 1;
    check(still * 20 < session.sim_ticks() - 200,
          "players stopped walking after their attacks");
  }
  std::cout << "peers=" << peers << " sim_ticks=" << session.sim_ticks()
            << " trace_bytes=" << session.trace_bytes(0).size()
            << " creature=" << (creature ? 1 : 0) << " input="
            << (input == Input::Walk   ? "walk"
                : input == Input::Mash ? "mash"
                : input == Input::Swing ? "swing-then-walk"
                                        : "combo-then-walk")
            << " input_packets=" << session.input_packets()
            << " free_spot_searches=" << session.free_spot_searches(0)
            << " synced=" << (session.synced() ? 1 : 0) << " fnv=" << std::hex
            << fnv(session.trace_bytes(0)) << std::dec << "\n";
}

// Attack, then walk: the cause of "never walks again" was the input, not
// Player_run. With presses every 12 frames the players stand still from
// their first swing to the end of the run (the swings chain); after one
// swing, or a two-swing combo, they walk on once the last swing ends.
void attack_then_walk(const W::Snapshot &ground) {
  auto run = [&](Input input) {
    N::Session session(2, ground, fight(2, false));
    drive(session, 400, input);
    traces_match(session, "attack-then-walk peers differ");
    return session.trace_bytes(0);
  };
  auto moved_after = [](const std::vector<std::uint8_t> &trace) {
    for (std::size_t tick = 140; tick < 400; ++tick)
      if (moved_at(trace, tick))
        return true;
    return false;
  };
  check(!moved_after(run(Input::Mash)), "a press every 12 frames let a player walk");
  for (Input input : {Input::Swing, Input::Combo}) {
    const auto trace = run(input);
    check(moved_after(trace) && moved_at(trace, 399),
          "a player who attacked did not walk again");
  }
}

void short_session(const W::Snapshot &ground, int peers, std::size_t ticks) {
  N::Session a(peers, ground, fight(peers));
  N::Session b(peers, ground, fight(peers));
  drive(a, ticks, Input::Mash);
  drive(b, ticks, Input::Mash);
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
  drive(a, further, Input::Mash);
  drive(loaded, further, Input::Mash);
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
  drive(session, 4, Input::Walk);
  auto bytes = session.save();
  bytes[0] ^= 1;
  rejects([&] { N::Session::load(bytes, ground); }, "save magic");
  bytes = session.save();
  bytes.resize(8);
  rejects([&] { N::Session::load(bytes, ground); }, "truncated save");
  auto other = ground;
  other.data.elements[0].min_x += 1;
  rejects([&] { N::Session::load(session.save(), other); }, "terrain identity");
  // A header that claims 2^20 render steps and then ends after the
  // placement block must be refused before the journal is allocated.
  bytes = session.save();
  constexpr std::size_t journal_at = 8 + 4 * 4 + 11 * 4 + 4 * 13 + 13;
  bytes.resize(journal_at);
  for (unsigned k = 0; k < 4; ++k) {
    bytes[16 + k] = std::uint8_t((1u << 20) >> (8 * k));
    bytes[20 + k] = 0;
  }
  largest_allocation = 0;
  rejects([&] { N::Session::load(bytes, ground); }, "journal past the save");
  check(largest_allocation < (1u << 20), "load allocated before its bound");
}
} // namespace

int main(int argc, char **argv) {
  std::size_t long_ticks = 10000;
  if (argc > 1)
    long_ticks = static_cast<std::size_t>(std::stoul(argv[1]));
  R::ScopedFpu fixed{R::Fpu{R::Fpu::Model::Ps2, false, false}};
  try {
    loopback();
    lmp_count_is_u32();
    research_planes();
    const auto ground = flat_ground();
    refusals(ground);
    short_session(ground, 2, 48);
    short_session(ground, 3, 48);
    short_session(ground, 4, 48);
    std::cout << "short peers=2,3,4 sim_ticks=48 checks=" << checks << "\n";
    attack_then_walk(ground);
    // Iterator retention in Full combat: 4 list walks per tick, plus one
    // spell-list iterator per player and per live creature, refused at
    // 65536. So 2 peers walking stay under that cap at 10000, and the
    // mashing creature run too because its creature dies early. 3 peers
    // refuse after 9362 and 4 peers after 8192 with no creature. With a
    // live creature: 2 peers after 9362, 3 after 8192, 4 after 7281. Every
    // run stops short of its cap. The walking runs with a creature pass
    // through world_findFreeSpot (Creature_moveTo's stuck recovery).
    if (long_ticks >= 10000) {
      long_session(ground, 2, 10000, false, Input::Walk);
      long_session(ground, 2, 10000, true, Input::Mash);
      long_session(ground, 3, 9000, false, Input::Walk);
      long_session(ground, 4, 8000, false, Input::Walk);
      long_session(ground, 2, 9000, true, Input::Swing);
      long_session(ground, 3, 8000, true, Input::Walk);
      long_session(ground, 4, 7000, true, Input::Walk);
    } else {
      long_session(ground, 2, long_ticks, false, Input::Walk);
      long_session(ground, 2, long_ticks, true, Input::Swing);
    }
    std::cout << "checks=" << checks << "\n";
  } catch (const std::exception &error) {
    std::cerr << "netplay failed: " << error.what() << "\n";
    return 1;
  }
  return 0;
}
