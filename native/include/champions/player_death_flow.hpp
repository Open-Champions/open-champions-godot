#pragma once
#include "champions/combat_runtime.hpp"
#include <functional>
#include <string>

namespace champions::simulation::rta::player_death_flow {
// Handwritten from research-death-flow 8be79eb, player-death-flow.md and the
// audited ref_death_flow.py. Services are evidence boundaries, never defaults.
using Address = combat_runtime::Address;
inline constexpr Address WIPE_COUNTDOWN = 0x4f7194, DEAD_COUNTDOWNS = 0x4f84a8;
inline constexpr unsigned INITIAL_COUNTDOWN = 240;
struct Memory {
  std::function<std::uint64_t(Address, unsigned)> read;
  std::function<void(Address, unsigned, std::uint64_t)> write;
};
struct Services {
  std::function<bool()> frozen, fewer_than_two_active;
  std::function<void()> close_menus, clear_fade;
  std::function<Raw(Raw)> step_fade;
  std::function<int()> find_act;
  std::function<void()> reload_checkpoint, request_current_level;
  std::function<bool()> pending_level, qualified_names_match;
  std::function<void(unsigned)> arena_respawn;
};
class Flow {
public:
  Flow(Memory, Services);
  void level_enter(unsigned players);
  void check_all_dead();
  void schedule(bool force_network = false);
  void task_step();
  bool running() const { return running_; }
  unsigned phase() const { return phase_; }
  static constexpr unsigned TASK_PRIORITY = 20;
private:
  unsigned players() const;
  Memory memory_;
  Services services_;
  bool running_ = false, force_network_ = false;
  unsigned phase_ = 0;
};
} // namespace champions::simulation::rta::player_death_flow
