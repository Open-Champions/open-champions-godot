#pragma once
#include "champions/control_anim_cache.hpp"
#include "champions/play_session.hpp"
#include "champions/session.hpp"
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace champions::simulation::rta::detail {
struct Checkpoint;
struct CheckpointAccess;
} // namespace champions::simulation::rta::detail
namespace champions::simulation::rta::session_control {
struct Error : std::runtime_error {
  using std::runtime_error::runtime_error;
};
inline constexpr std::uint32_t POS_X = 1, NEG_X = 2, POS_Y = 4, NEG_Y = 8,
                               DAMAGE = 16, COMMAND_MASK = 31;
inline constexpr std::size_t MAX_READY_TICKS = 2000;
inline constexpr std::size_t MAX_SUBMISSIONS = 8008;
// Explicit bounded scene policy; original player input remains unimplemented.
enum class MovementPolicy : std::uint8_t { Isolated, DryCreature };
inline constexpr std::size_t MAX_CREATURE_READY_TICKS = 512;
inline constexpr std::size_t MAX_RECOVERY_READY_TICKS = 256;
inline constexpr std::uint32_t CREATURE_BASE_TABLE = 0x10000000;
inline constexpr Raw SPEED = 0x42700000, RADIUS = 0x40800000,
                     INITIAL_HP = 0x42c80000, DAMAGE_AMOUNT = 0x40000000;
enum class FacingPolicy : std::uint8_t { Disabled, DirectionalTarget };
inline constexpr const char *FACING_POLICY = "rta-session-facing-v1";
inline constexpr std::size_t MAX_FACING_READY_TICKS = 256;
struct Facing {
  FacingPolicy policy = FacingPolicy::DirectionalTarget;
  session::TurnInitial initial;
  std::int32_t mode = 0;
};
struct Config {
  // Zero selects unchanged complete-history diagnostic mode.
  std::size_t play_window = 0;
  std::string profile = "rta";
  Fpu::Model model = Fpu::Model::Ps2;
  bool mul_bias = false, div_nearest = false;
  std::uint32_t shared_seed = 17;
  std::size_t max_ready_ticks = MAX_READY_TICKS;
  // DryCreature requires an explicit max_ready_ticks <=512. The legacy
  // default remains unchanged, including its complete observations.
  MovementPolicy movement = MovementPolicy::Isolated;
  std::optional<session_recovery::Initial> recovery;
  std::optional<Facing> facing;
  std::optional<AnimationCache> animation_cache;
  // Client start position (raw words) of the controlled actor, for example the
  // level-spawn story start (level_flow::story_start_ground). Requires
  // supplied terrain. Absent keeps the historical origin start and every
  // existing save byte.
  std::optional<world_queries::Vec3> start;
};
struct Observation {
  std::size_t play_window = 0;
  play_session::History history;
  play_session::Retained retained;
  bool animation_cache_enabled = false;
  animation_runtime::TrackId animation_track = 0;
  std::optional<std::int32_t> animation_event;
  std::optional<session::AnimationCacheDecoded> animation_decoded;
  world_queries::Vec3 position{};
  Raw vz = 0, radius = RADIUS, hp = INITIAL_HP;
  bool recovery_enabled = false;
  Raw mana = 0, pending_health = 0, pending_mana = 0;
  Raw max_health = INITIAL_HP, max_mana = 0;
  // Read-only presentation projection of the owned World facing.
  std::uint32_t facing = 0;
  bool facing_enabled = false;
  Raw turn_rate = 0;
  std::uint16_t angular_velocity = 0;
  std::uint8_t turn_slow = 0;
  std::int32_t turn_mode = 0;
  std::optional<std::int16_t> turn_result;
  std::int64_t frame = 0;
  std::size_t submissions = 0, ready_ticks = 0, damage_events = 0;
  bool paused = false, stopped = false, failed = false;
  std::string error;
  MovementPolicy movement = MovementPolicy::Isolated;
  std::size_t ready_tick_limit = MAX_READY_TICKS;
};
// Immutable construction inputs, copied independently of retained histories.
struct Setup {
  Config config;
  world_queries::Snapshot initial_terrain;
  bool supplied_terrain = false;
};
struct Snapshot {
  play_session::History history;
  std::uint64_t next_animation_command = 1;
  std::vector<AnimationCommand> animation_commands;
  animation_runtime::TrackId selected_animation_track = 0;
  Config config;
  // Initial terrain is retained for exact reset; Session contains current data.
  world_queries::Snapshot initial_terrain;
  bool supplied_terrain = false;
  session::Snapshot session;
  std::uint64_t actor_serial = 0;
  std::uint32_t actor_address = 0;
  // Isolated binding only; DryCreature uses hp and Session.creature_movement.
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
  friend struct detail::CheckpointAccess;
  void checkpoint(detail::Checkpoint &);

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
  // Frame zero explicitly targets the next local input frame, including pause.
  std::uint64_t queue_animation(AnimationCommandKind, animation_runtime::ClipId,
                                std::int64_t frame = 0);
  void release_input();
  void stop();
  // Explicit fresh Session with the same configuration and initial terrain.
  void reset();
  Observation observation() const;
  // The session World's shared generator (read only). An opted-in level
  // script draws from the same stream (script_commands, SeedPolicy).
  std::uint32_t shared_seed() const;
  Setup setup() const;
  Snapshot snapshot() const;
  // Explicit partial-history observation for the opted-in continuation mode.
  Snapshot window_snapshot() const;
  world_queries::LevelData terrain_data() const;

private:
  struct State;
  std::unique_ptr<State> state_;
};
} // namespace champions::simulation::rta::session_control
