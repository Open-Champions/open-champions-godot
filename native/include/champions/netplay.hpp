#pragma once

// Lockstep transport for 2–4 peers over the ported RTA wire model
// (champions/netwire.hpp, ref_netwire.py). Each peer owns one Full combat
// world. The worlds run the offline co-op simulation (network byte 0x402791
// clear, offline screen clamp against the research's authored planes,
// single-player speed base), not the original net-game simulation. Packets
// move through LoopbackTransport. Internet datagrams, the lobby and
// rendezvous are not implemented; see docs/NETPLAY.md.
#include "champions/combat_runtime.hpp"
#include "champions/netwire.hpp"
#include "champions/world_queries.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

namespace champions::simulation::rta::netplay {

inline constexpr const char *POLICY = "rta-netplay-lockstep-v1";
// Button bit bound as Attack. combat_play binds this same mask, and
// netcode.md lists 0x10 in the lockstep button word.
inline constexpr std::uint16_t ATTACK_BIT = 0x10;

struct Error : std::runtime_error {
  using std::runtime_error::runtime_error;
};

// One local pad sample for one rendered frame. Eight payload bytes match
// the pad sampler net_main consumes; yaw and the camera flag are the
// separate arguments of that call.
struct Sample {
  std::uint16_t buttons = 0;
  std::array<std::uint8_t, 4> sticks{128, 128, 128, 128};
  std::array<std::uint8_t, 2> pressure{0, 0};
  std::int16_t yaw = 0;
  bool camera_flag = false;
};

// Authored spawn positions on the supplied terrain. Slots 0..peers-1 are
// the players. The creature is optional. z is the caller's floor height.
struct Placement {
  std::array<std::array<Raw, 3>, 4> players{};
  std::array<bool, 4> player_present{};
  std::array<Raw, 3> creature{};
  bool creature_present = false;
};

// One simulated tick of the shared values the lockstep compares: the combat
// world's shared seed, each player record's hit points, the creature, and
// each player actor's position (object +16, +20, +24; zero when absent).
struct TracePoint {
  std::uint32_t seed = 0;
  std::array<std::uint32_t, 4> player_hp{};
  std::uint32_t creature_hp = 0;
  std::array<std::array<std::uint32_t, 3>, 4> player_position{};
  bool operator==(const TracePoint &o) const {
    return seed == o.seed && player_hp == o.player_hp &&
           creature_hp == o.creature_hp && player_position == o.player_position;
  }
};
// Bytes per tick in trace_bytes(): 18 little-endian words.
inline constexpr std::size_t TRACE_RECORD = 72;

// In-process datagrams. send() is invisible to take() until deliver(), which
// is the next rendered frame. A socket transport would use these same three
// operations; none is implemented here.
class LoopbackTransport {
public:
  explicit LoopbackTransport(int peers);
  void send(int from, const netwire::Sent &packet);
  std::vector<netwire::ByteVector> take(int slot);
  void deliver();
  int peers() const { return peers_; }

private:
  int peers_;
  std::vector<std::vector<netwire::ByteVector>> inbox_, pending_;
};

class Session {
public:
  Session(int peers, const world_queries::Snapshot &terrain,
          const Placement &placement);
  ~Session();
  Session(const Session &) = delete;
  Session &operator=(const Session &) = delete;
  Session(Session &&) noexcept;
  Session &operator=(Session &&) noexcept;

  int peers() const;
  // Minimum simulated ticks across peers. Send rotation lets one machine
  // tick while another is still waiting on a packet.
  std::size_t sim_ticks() const;
  bool aligned() const;
  std::size_t render_steps() const;
  bool synced() const;
  std::uint64_t input_packets() const;
  // Little-endian seed, four player hit-point words, the creature word and
  // four player positions, one TRACE_RECORD per simulated tick.
  std::vector<std::uint8_t> trace_bytes(int peer) const;
  // world_findFreeSpot searches this peer's world has made (diagnostic; the
  // creature's Creature_moveTo stuck recovery). Recounted by load()'s replay.
  std::uint64_t free_spot_searches(int peer) const;

  // One rendered frame. Returns how many simulation ticks every peer ran.
  int step(const std::vector<Sample> &samples);

  // Authored continuation. Terrain bytes are not stored; load() requires the
  // same snapshot identity and replays the journal.
  std::vector<std::uint8_t> save() const;
  static Session load(const std::vector<std::uint8_t> &bytes,
                      const world_queries::Snapshot &terrain);

private:
  struct State;
  std::unique_ptr<State> state_;
};

} // namespace champions::simulation::rta::netplay
