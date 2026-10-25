#pragma once
#include "champions/combat_runtime.hpp"
namespace champions::simulation::rta::combat_runtime {
// Opt-in ready-frame adapter. The later Session integration must make this
// World its sole actor authority; never run it alongside Session's legacy
// object host.
class SessionAdapter {
  World &world_;
  Raw next_;
  bool failed_ = false;
  void ready() const;

public:
  SessionAdapter(World &world, Raw first_ready_frame = 0)
      : world_(world), next_(first_ready_frame) {}
  void ready_tick(Raw frame);
  void hit_test_event(Address actor_handle, Raw mask);
  bool failed() const { return failed_; }
  const objects::GameObject *factory_object(Address handle) const {
    return world_.factory_object(handle);
  }

  // ---- Full composition input (round 2) -----------------------------------
  // The lockstep pad of a player slot for the next ready tick: stick, then
  // the held and newly pressed button masks. The tick clears the pressed and
  // +0x10 edge words afterwards, as tasks_run does.
  struct PadInput {
    Raw stick_x = 0, stick_y = 0, held = 0, pressed = 0;
  };
  void pad_input(unsigned slot, const PadInput &);
  // Button masks are the caller's controller mapping; none is invented.
  void bind_button(unsigned slot, World::Button which, Raw mask);
  // Convenience: press (and optionally hold) the bound attack or block mask.
  void attack(unsigned slot, bool held = true);
  void block(unsigned slot, bool held = true);
  // A projectile made by a call site (bow release, monster attack): the
  // executed constructors run now; the projectile first runs next tick.
  // Returns a retained handle to it (reads 0 once it destroys itself).
  Address spawn_missile(Address owner_handle, const Missile &);
  Address spawn_thrown(Address owner_handle, const Thrown &);
  // Holy Strike (16), Shock of Fire (57) or Wave of Flame (95), given with
  // World::give_skill: made in the caster's next run.
  void cast(Address caster_handle, int skill_id, int level = 1,
            std::optional<Vec3> point = std::nullopt,
            std::optional<Raw> facing = std::nullopt);
};
} // namespace champions::simulation::rta::combat_runtime
