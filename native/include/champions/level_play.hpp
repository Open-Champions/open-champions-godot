#pragma once
#include "champions/session_control.hpp"
#include <memory>
#include <string>
namespace champions::simulation::rta::level_play {
// Closed, authored two-level fixture policy, not a general engine service host.
inline constexpr const char *POLICY = "rta-level-fixture-v1";
struct Flow {
  std::string level;
  unsigned generation, travel_ticks, restored;
  bool gate_contact, trigger_inside, menu_open;
  Vec3 marker, resolved_spawn;
  std::size_t saved_levels, restored_bytes;
};
class Session {
  struct State;
  std::unique_ptr<State> s_;
public:
  Session();
  ~Session();
  session_control::Observation advance(std::uint32_t);
  session_control::Observation observation() const;
  Flow flow() const;
  world_queries::Snapshot terrain() const;
  void open_gate();
  void confirm_gate(const std::string &);
  void request_level(const std::string &);
  void release_input();
  void set_paused(bool);
  void stop();
  // The fixture World's shared random authority (the level flow's shared draws).
  Raw shared_seed() const;
};
}
