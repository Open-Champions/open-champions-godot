#pragma once
#include <cstdint>
#include <stdexcept>

namespace champions::simulation::rta::campaign_fight {
// Authored client commands, not original pad words. The four low bits retain
// the movement mapping; unsupported requests never enter the Player's pad.
inline constexpr std::uint32_t ATTACK = 16, GUARD = 32, GUARD_MOVE = 64,
                               HEAVY = 128, ABILITY1 = 256, ABILITY2 = 512,
                               MOVEMENT = 15,
                               COMMAND_MASK = MOVEMENT | ATTACK | GUARD | GUARD_MOVE |
                                              HEAVY | ABILITY1 | ABILITY2;

struct MoveInput {
  std::uint32_t held = 0, pressed = 0, requests = 0;
};

class MovePad {
public:
  MoveInput update(std::uint32_t command) {
    if (command & ~COMMAND_MASK)
      throw std::invalid_argument("unknown campaign move command");
    const auto requests = command & ~previous_;
    previous_ = command;
    const auto held = command & (ATTACK | GUARD | GUARD_MOVE);
    return {held, requests & held, requests};
  }
private:
  std::uint32_t previous_ = 0;
};
}
