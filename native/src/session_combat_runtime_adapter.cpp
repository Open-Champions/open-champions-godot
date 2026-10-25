#include "champions/session_combat_runtime_adapter.hpp"
namespace champions::simulation::rta::combat_runtime {
namespace {
constexpr Address G_BTN_ATTACK = 0x41b980, G_BTN_BLOCK = 0x41b9d0;
} // namespace
void SessionAdapter::ready() const {
  if (failed_)
    throw Error("combat adapter latched after failure");
}
void SessionAdapter::ready_tick(Raw frame) {
  ready();
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
  ready();
  auto actor = world_.target(handle);
  if (!actor)
    throw Error("hit event handle expired");
  world_.animation_events(actor, mask);
}
void SessionAdapter::pad_input(unsigned slot, const PadInput &in) {
  ready();
  world_.stick(slot, in.stick_x, in.stick_y);
  world_.pad(slot, in.held, in.pressed);
}
void SessionAdapter::bind_button(unsigned slot, World::Button which, Raw mask) {
  ready();
  world_.bind_button(slot, which, mask);
}
namespace {
void press(World &w, unsigned slot, Address binding, bool held) {
  if (slot >= 4)
    throw Error("pad slot out of range");
  auto mask = Raw(w.read(binding + 4 * slot));
  if (!mask)
    throw Error("bind the button mask first");
  auto pad = PADS + slot * PAD_SIZE;
  auto now_held = Raw(w.read(pad + 8)), now_pressed = Raw(w.read(pad + 0xc));
  w.pad(slot, held ? now_held | mask : now_held & ~mask, now_pressed | mask);
}
} // namespace
void SessionAdapter::attack(unsigned slot, bool held) {
  ready();
  press(world_, slot, G_BTN_ATTACK, held);
}
void SessionAdapter::block(unsigned slot, bool held) {
  ready();
  press(world_, slot, G_BTN_BLOCK, held);
}
Address SessionAdapter::spawn_missile(Address owner_handle, const Missile &m) {
  ready();
  auto owner = owner_handle ? world_.target(owner_handle) : 0;
  if (owner_handle && !owner)
    throw Error("projectile owner handle expired");
  return world_.retain(world_.spawn_missile(owner, m));
}
Address SessionAdapter::spawn_thrown(Address owner_handle, const Thrown &t) {
  ready();
  auto owner = owner_handle ? world_.target(owner_handle) : 0;
  if (owner_handle && !owner)
    throw Error("projectile owner handle expired");
  return world_.retain(world_.spawn_thrown(owner, t));
}
void SessionAdapter::cast(Address caster_handle, int skill_id, int level,
                          std::optional<Vec3> point, std::optional<Raw> facing) {
  ready();
  auto caster = world_.target(caster_handle);
  if (!caster)
    throw Error("caster handle expired");
  world_.queue_cast(caster, skill_id, level, point, facing);
}
} // namespace champions::simulation::rta::combat_runtime
