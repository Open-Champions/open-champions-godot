#pragma once
#include "champions/session.hpp"
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace champions::simulation::rta::session_control {
struct Error : std::runtime_error {
  using std::runtime_error::runtime_error;
};
inline constexpr std::uint32_t POS_X = 1, NEG_X = 2, POS_Y = 4, NEG_Y = 8,
                               DAMAGE = 16, COMMAND_MASK = 31;
inline constexpr std::size_t MAX_READY_TICKS = 2000;
inline constexpr std::size_t MAX_SUBMISSIONS = 8008;
inline constexpr Raw SPEED = 0x42700000, RADIUS = 0x40800000,
                     INITIAL_HP = 0x42c80000, DAMAGE_AMOUNT = 0x40000000;
struct Config {
  std::string profile = "rta";
  Fpu::Model model = Fpu::Model::Ps2;
  bool mul_bias = false, div_nearest = false;
  std::uint32_t shared_seed = 17;
  std::size_t max_ready_ticks = MAX_READY_TICKS;
};
struct Observation {
  world_queries::Vec3 position{};
  Raw vz = 0, radius = RADIUS, hp = INITIAL_HP;
  std::int64_t frame = 0;
  std::size_t submissions = 0, ready_ticks = 0, damage_events = 0;
  bool paused = false, stopped = false, failed = false;
  std::string error;
};
struct Snapshot {
  Config config;
  // Initial terrain is retained for exact reset; Session contains current data.
  world_queries::Snapshot initial_terrain;
  bool supplied_terrain = false;
  session::Snapshot session;
  std::uint64_t actor_serial = 0;
  std::uint32_t actor_address = 0;
  session::MoverId mover = 0;
  session::HpId hp = 0;
  std::uint32_t held_input = 0;
  std::vector<std::int64_t> pending_damage_frames;
  std::int64_t minimum_input_frame = 1;
  std::size_t submissions = 0, ready_ticks = 0, damage_events = 0;
  bool paused = false, stopped = false, failed = false, busy = false;
  std::string phase = "idle", error;
};
// Authored diagnostic controls, not original Player_move/input behavior.
// One existing Session/World/pool, one actor, one local peer, no fabricated
// input packets. Every advance submits at most one frame; no wall clock or host
// float.
class Controller {
public:
  explicit Controller(Config config = {},
                      std::optional<world_queries::LevelData> terrain = {},
                      world_queries::Limits limits = {});
  ~Controller();
  Controller(const Controller &) = delete;
  Controller &operator=(const Controller &) = delete;
  Controller(Controller &&) = delete;
  Controller &operator=(Controller &&) = delete;
  // Commands are wire button bits, with neutral analog sticks. Motion uses the
  // imported ready frame's World Pad.held: opposing directions cancel; diagonal
  // axes each retain SPEED (no normalization). Damage rising edges are queued
  // by input frame and applied once with null attacker after movement.
  // Invalid masks refuse before mutation. Operational failures latch stopped,
  // preserve partial Session state and return a failed observation.
  Observation advance(std::uint32_t command_mask);
  // Pause never submits or advances simulation. Release discards pending damage
  // and buffered commands older than the next local input frame. Resume drains
  // any old buffered ready frame neutrally, not as accumulated input catchup.
  void set_paused(bool);
  void release_input();
  void stop();
  // Explicit fresh Session with the same configuration and initial terrain.
  void reset();
  Observation observation() const;
  Snapshot snapshot() const;
  world_queries::LevelData terrain_data() const;

private:
  struct State;
  std::unique_ptr<State> state_;
};
} // namespace champions::simulation::rta::session_control
