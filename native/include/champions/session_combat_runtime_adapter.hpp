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

public:
  SessionAdapter(World &world, Raw first_ready_frame = 0)
      : world_(world), next_(first_ready_frame) {}
  void ready_tick(Raw frame);
  void hit_test_event(Address actor_handle, Raw mask);
  bool failed() const { return failed_; }
};
} // namespace champions::simulation::rta::combat_runtime
