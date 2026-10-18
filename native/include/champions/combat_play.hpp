#pragma once
#include "champions/session_combat_runtime_adapter.hpp"
#include "champions/session_control.hpp"
#include "champions/play_animation.hpp"

namespace champions::simulation::rta::combat_play {
// Authored, opt-in synthetic composition. Not original encounter/input policy.
inline constexpr const char *POLICY = "rta-combat-fixture-v1";
inline constexpr const char *ANIMATED_POLICY = "rta-animated-combat-fixture-v1";
struct Actor {
  unsigned slot;
  combat_runtime::Address handle;
  bool present, dead;
  Vec3 position;
  Raw hp, state;
};
struct VisualEvent {
  bool death = false;
  unsigned slot = 0;
  Vec3 position{};
};
class Session {
  struct State;
  std::unique_ptr<State> s_;
public:
  explicit Session(const world_queries::Snapshot &, bool animated = false);
  bool animated() const;
  const play_animation::Frame &animation_frame(unsigned slot) const;
  ~Session();
  session_control::Observation advance(std::uint32_t command);
  session_control::Observation observation() const;
  std::vector<Actor> actors() const;
  // Owned observations from this ready tick; no FX policy or RNG in simulation.
  const std::vector<VisualEvent> &visual_events() const;
  // Explicit animation event fixture, not an attack-selection/action switch.
  void hit_event(unsigned slot, Raw mask);
  void release_input();
  void set_paused(bool);
  void stop();
  Raw shared_seed() const;
};
}
