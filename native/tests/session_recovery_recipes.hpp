#pragma once
#include "champions/session_control.hpp"
namespace champions::test::recovery_recipes {
namespace R = champions::simulation::rta;
inline R::session_control::Config config(unsigned f, unsigned variant) {
  R::session_control::Config c;
  c.model = R::Fpu::Model(f / 4);
  c.mul_bias = f & 2;
  c.div_nearest = f & 1;
  c.movement = R::session_control::MovementPolicy::DryCreature;
  c.max_ready_ticks = 256;
  c.recovery.emplace();
  auto &i = *c.recovery;
  i.passive_masks[0] = 0x10;
  i.passive_masks[1] = 0x200;
  switch (variant) {
  case 0:
    break;
  case 1:
    i.player_class = 0;
    break;
  case 2:
    i.health = 0;
    break;
  case 3:
    i.health = 0x80000000;
    break;
  case 4:
    i.health = 0xbf800000;
    break;
  case 5:
    i.health = i.max_health;
    i.mana = i.max_mana;
    break;
  case 6:
    i.pending_health = 1;
    i.health_rate = 0x42c80000;
    i.pending_mana = 1;
    i.mana_rate = 0x42480000;
    break;
  case 7:
    i.health_bonus = -128;
    i.mana_bonus = -1;
    i.passive_levels[0] = -1;
    i.passive_levels[1] = -128;
    break;
  case 8:
    i.health_bonus = 127;
    i.mana_bonus = 127;
    i.passive_levels[0] = 127;
    i.passive_levels[1] = 127;
    break;
  case 9:
    i.health_rate = 0xbf800000;
    i.mana_rate = 0x80000000;
    i.pending_health = 0x80000000;
    i.pending_mana = 0xbf800000;
    break;
  case 10:
    i.health = 0x42c7ffff;
    i.mana = 0x4247ffff;
    i.health_rate = 0;
    i.mana_rate = 0;
    break;
  case 11:
    i.copy_at_setup = true;
    break;
  case 12:
    i.health_rate = 0xbf800000;
    i.mana_rate = 0xbf000000;
    break;
  case 13:
    i.max_health = 0;
    break;
  default:
    throw std::runtime_error("unknown recovery recipe");
  }
  return c;
}
using Action = std::pair<std::string, unsigned>;
inline std::vector<Action> actions(const std::string &name) {
  if (name == "ready")
    return {{"advance", 0}, {"advance", 0}, {"advance", 0}, {"advance", 0}};
  if (name == "ordering")
    return {{"advance", 17},
            {"advance", 17},
            {"advance", 0},
            {"advance", 16},
            {"advance", 0}};
  if (name == "pause")
    return {{"advance", 17}, {"pause", 1},    {"advance", 31}, {"pause", 0},
            {"advance", 0},  {"advance", 16}, {"advance", 0}};
  if (name == "release")
    return {{"advance", 17},
            {"release", 0},
            {"advance", 0},
            {"advance", 16},
            {"advance", 0}};
  if (name == "reset")
    return {{"advance", 17},
            {"advance", 0},
            {"reset", 0},
            {"advance", 0},
            {"advance", 0}};
  if (name == "stop")
    return {{"advance", 0},
            {"advance", 0},
            {"stop", 0},
            {"advance", 16},
            {"reset", 0}};
  throw std::runtime_error("unknown recovery schedule");
}
} // namespace champions::test::recovery_recipes
