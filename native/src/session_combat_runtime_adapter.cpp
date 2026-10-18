#include "champions/session_combat_runtime_adapter.hpp"
namespace champions::simulation::rta::combat_runtime {
void SessionAdapter::ready_tick(Raw frame) {
  if (failed_)
    throw Error("combat adapter latched after failure");
  if (frame != next_ || world_.read(FRAME) != frame)
    throw Error("combat adapter requires exactly the next ready frame");
  try {
    world_.tick();
    ++next_;
  } catch (...) {
    failed_ = true;
    throw;
  }
}
void SessionAdapter::hit_test_event(Address handle, Raw mask) {
  if (failed_)
    throw Error("combat adapter latched after failure");
  auto actor = world_.target(handle);
  if (!actor)
    throw Error("hit event handle expired");
  world_.animation_events(actor, mask);
}
} // namespace champions::simulation::rta::combat_runtime
