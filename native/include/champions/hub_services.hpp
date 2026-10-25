#pragma once
// RTA US v2.00 only. research-hub-npcs 31db144, PASS audit fix2:
// Savepoint_interact 0x28dc30, object +0x84 (word) and +0xc4 (byte).
// This is direct use before external card/UI/action services, not proximity,
// auto-save, resurrection or completed persistence. No Player record is written.
#include <cstdint>

namespace champions::simulation::rta::hub_services {
struct Savepoint {
  std::uint32_t activated = 0;
  std::uint8_t effect_pending = 0;
};
enum class Use { Accepted, CardBusy, MenuHeld };
inline Use use_savepoint(Savepoint &point, bool card_busy, bool menu_held) {
  if (card_busy)
    return Use::CardBusy;
  if (menu_held)
    return Use::MenuHeld;
  point.activated = 1;
  point.effect_pending = 1;
  return Use::Accepted;
}
} // namespace champions::simulation::rta::hub_services
