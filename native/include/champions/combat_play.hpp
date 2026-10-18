#pragma once
#include "champions/session_combat_runtime_adapter.hpp"
#include "champions/session_control.hpp"

namespace champions::simulation::rta::combat_play {
// Authored, opt-in synthetic composition. Not original encounter/input policy.
inline constexpr const char *POLICY = "rta-combat-fixture-v1";
struct Actor {
  unsigned slot;
  combat_runtime::Address handle;
  bool present, dead;
  Vec3 position;
  Raw hp, state;
};
class Session {
  struct State;
  std::unique_ptr<State> s_;
public:
  explicit Session(const world_queries::Snapshot &);
  ~Session();
  session_control::Observation advance(std::uint32_t command);
  session_control::Observation observation() const;
  std::vector<Actor> actors() const;
  // Explicit animation event fixture, not an attack-selection/action switch.
  void hit_event(unsigned slot, Raw mask);
  void release_input();
  void set_paused(bool);
  void stop();
  Raw shared_seed() const;
};
}
