#pragma once
#include "champions/native_save.hpp"
#include "session_recovery_trace.hpp"

#include "session_turn_trace.hpp"
namespace champions::test::facing_trace {
using namespace session_trace;
inline std::string control(R::session_control::Snapshot s) {
  const auto &f = s.config.facing;
  auto facing = f ? sequence({n(unsigned(f->policy)), n(f->initial.rate),
                             n(f->initial.angular_velocity),
                             n(f->initial.slow), n(f->mode)})
                  : "_";
  auto turning = s.session.turning
                     ? session_turn_trace::turning(*s.session.turning)
                     : "_";
  s.session.turning.reset();
  s.config.facing.reset();
  return sequence({recovery_trace::control(std::move(s)), std::move(facing),
                   std::move(turning)});
}
inline R::session_control::Config config(unsigned f, unsigned v,
                                         bool recovery) {
  R::session_control::Config c;
  c.model = R::Fpu::Model(f / 4);
  c.mul_bias = f & 2;
  c.div_nearest = f & 1;
  c.movement = R::session_control::MovementPolicy::DryCreature;
  c.max_ready_ticks = R::session_control::MAX_FACING_READY_TICKS;
  c.facing.emplace();
  c.facing->initial.rate = 0x3f800000;
  c.facing->initial.angular_velocity = v == 6 ? 0xfff0 : 0;
  c.facing->initial.slow = v == 2 ? 1 : v == 3 ? 60 : v == 4 ? 61 : 0;
  c.facing->mode = v == 1 ? 1 : 0;
  if (v == 5)
    c.facing->initial.rate = 0xbf800000;
  if (recovery)
    c.recovery.emplace();
  return c;
}
inline std::vector<std::pair<std::string, unsigned>>
actions(const std::string &name) {
  if (name == "directions") {
    std::vector<std::pair<std::string, unsigned>> out;
    for (unsigned k = 0; k < 16; ++k)
      out.push_back({"advance", k});
    out.push_back({"advance", 0});
    return out;
  }
  if (name == "limit")
    return std::vector<std::pair<std::string, unsigned>>(257, {"advance", 21});
  return {{"advance", 21},   {"checkpoint", 0}, {"advance", 21},
          {"advance", 26},   {"advance", 26},   {"pause", 1},
          {"checkpoint", 1}, {"advance", 31},   {"pause", 0},
          {"advance", 0},    {"release", 0},    {"advance", 9},
          {"advance", 0},    {"stop", 0},       {"advance", 31},
          {"reset", 0},      {"advance", 8},    {"advance", 0}};
}
inline void action(R::native_save::Scene &s, const std::string &a, unsigned v) {
  if (a == "advance")
    s.advance(v);
  else if (a == "pause")
    s.set_paused(v);
  else if (a == "release")
    s.release_input();
  else if (a == "stop")
    s.stop();
  else
    s.reset();
}
} // namespace champions::test::facing_trace
