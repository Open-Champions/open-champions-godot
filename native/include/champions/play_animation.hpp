#pragma once
#include "champions/creature_anim/dispatch.hpp"
namespace champions::simulation::rta::play_animation {
// Authored geometry/resources and already-filtered actions. This is not Agony AI.
struct Frame {
  std::vector<std::array<std::int64_t, 3>> vertices;
  std::vector<creature_anim::DispatchEvent> events;
  Raw flags = 0;
  std::uint32_t tick = 0;
};
class Actors {
  creature_anim::Runtime runtime_;
  std::array<creature_anim::StateId, 3> states_{};
  std::array<Frame, 3> frames_{};
  creature_anim::Mesh mesh_;
  std::function<void(unsigned, const creature_anim::DispatchEvent &)> footstep_;
  void pose(unsigned);
public:
  Actors();
  void advance(unsigned slot, std::uint32_t tick);
  const Frame &frame(unsigned slot) const;
  // Optional consumers can receive typed 0x4/0x8
  // footsteps here. The scene uses owned ready-tick packets with explicit clip
  // roles; callbacks must not advance simulation or draw RNG.
  void set_footstep_hook(std::function<void(unsigned, const creature_anim::DispatchEvent &)> hook);
};
}
