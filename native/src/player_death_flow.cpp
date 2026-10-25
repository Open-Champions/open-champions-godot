#include "champions/player_death_flow.hpp"
#include "champions/combat.hpp"
#include <utility>

namespace champions::simulation::rta::player_death_flow {
namespace {
constexpr Address WIPE = WIPE_COUNTDOWN, TIMERS = DEAD_COUNTDOWNS;
template <typename Callback> const Callback &required(const Callback &callback, const char *name) {
  if (!callback) throw combat_runtime::Unsupported(std::string("death flow: ") + name);
  return callback;
}
int signed_half(std::uint64_t value) {
  return value & 0x8000 ? int(value & 0xffff) - 0x10000 : int(value & 0xffff);
}
}
Flow::Flow(Memory memory, Services services)
    : memory_(std::move(memory)), services_(std::move(services)) {
  required(memory_.read, "memory reader missing");
  required(memory_.write, "memory writer missing");
}
unsigned Flow::players() const {
  const auto count = memory_.read(combat_runtime::NUM_PLAYERS, 4);
  if (count > 4) throw combat_runtime::Unsupported("death flow: player count outside 0..4");
  return unsigned(count);
}
void Flow::level_enter(unsigned count) {
  if (count > 4) throw combat_runtime::Unsupported("death flow: player count outside 0..4");
  memory_.write(WIPE, 2, INITIAL_COUNTDOWN);
  for (unsigned slot = 0; slot < count; ++slot) memory_.write(TIMERS + 2 * slot, 2, INITIAL_COUNTDOWN);
  running_ = false;
  force_network_ = false;
  phase_ = 0;
}
void Flow::schedule(bool force_network) {
  if (running_) throw combat_runtime::Unsupported("death flow: duplicate task schedule");
  required(services_.clear_fade, "fade clear service missing")();
  phase_ = 0;
  force_network_ = force_network;
  running_ = true;
}
void Flow::check_all_dead() {
  const auto count = players();
  const auto alive = memory_.read(0x4f8420, 4);
  const bool cutscene = memory_.read(0x44fcd0, 1) != 0;
  const bool pvp_b = memory_.read(0x4f7120, 4) != 0;
  if (!cutscene && memory_.read(0x4f85b0, 4) && !pvp_b && !alive && count) {
    if (!required(services_.frozen, "freeze service missing")()) {
      const bool allowed = !memory_.read(0x402791, 1) ||
          !required(services_.fewer_than_two_active, "network active-peer service missing")();
      if (allowed) {
        memory_.write(0x41b4fc, 4, 60);
        const auto timer = signed_half(memory_.read(WIPE, 2));
        if (timer > 0) {
          memory_.write(WIPE, 2, timer - 1);
          if (timer == 1) {
            required(services_.close_menus, "close-menu service missing")();
            schedule();
          }
        }
      }
    }
  }
  if (!cutscene && !memory_.read(0x41b4f4, 4) && count && alive != count) {
    for (unsigned slot = 0; slot < count; ++slot) {
      const auto record = combat_runtime::PLAYERS + combat_runtime::RECORD_SIZE * slot;
      const auto hp = Raw(memory_.read(record + 0xac, 4));
      const auto timer = signed_half(memory_.read(TIMERS + 2 * slot, 2));
      if (combat::fle(hp, 0) && timer > 0) {
        memory_.write(TIMERS + 2 * slot, 2, timer - 1);
        if (timer == 1 && pvp_b)
          required(services_.arena_respawn, "arena_respawnPlayer missing")(slot);
      }
    }
  }
}
void Flow::task_step() {
  if (!running_) return;
  switch (phase_) {
  case 0:
    if (memory_.read(0x4f712c, 4)) phase_ = 10;
    else if (required(services_.step_fade, "hud_stepFade missing")(4) == 0x80) phase_ = 1;
    break;
  case 1:
    memory_.write(0x44fd6c, 4, 1);
    if (force_network_) {
      memory_.write(0x402791, 1, 1);
      phase_ = 2;
    } else if (required(services_.find_act, "save_findAct tables missing")() == -1) {
      required(services_.request_current_level, "current-level restart loader missing")();
      for (unsigned slot = 0, count = players(); slot < count; ++slot) {
        const auto record = combat_runtime::PLAYERS + combat_runtime::RECORD_SIZE * slot;
        memory_.write(record + 0xac, 4, memory_.read(record + 0xb0, 4));
      }
      running_ = false;
    } else {
      required(services_.reload_checkpoint,
          "save_reload unavailable: valid in-memory checkpoint and complete saved player records required; invalid-buffer behavior Unknown")();
      phase_ = 2;
    }
    break;
  case 2:
    if (!required(services_.pending_level, "pending-level service missing")() &&
        required(services_.qualified_names_match, "qualified-level match service missing")()) running_ = false;
    break;
  case 10:
    if (memory_.read(0x4f8420, 4)) running_ = false;
    break;
  default:
    throw combat_runtime::Unsupported("death flow: unknown task phase");
  }
}
} // namespace champions::simulation::rta::player_death_flow
