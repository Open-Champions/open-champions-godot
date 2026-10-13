#include "champions/frame_clock.hpp"
#include "champions/session_control.hpp"
#include "session_creature_trace.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace champions::test::session_trace;
namespace Control = R::session_control;
using Clock = champions::presentation::rta::FrameClock;
void check(bool yes, const char *why) {
  if (!yes)
    throw std::runtime_error(why);
}
std::string control_snapshot(const Control::Snapshot &s) {
  return sequence(
      {sequence({text(s.config.profile),
                 n(static_cast<unsigned>(s.config.model)), n(s.config.mul_bias),
                 n(s.config.div_nearest), n(s.config.shared_seed),
                 n(s.config.max_ready_ticks), n(unsigned(s.config.movement))}),
       level_snapshot(s.initial_terrain),
       n(s.supplied_terrain),
       champions::test::creature_trace::observation(s.session),
       n(s.actor_serial),
       n(s.actor_address),
       n(s.mover),
       n(s.hp),
       n(s.held_input),
       integers(s.pending_damage_frames),
       n(s.minimum_input_frame),
       n(s.submissions),
       n(s.ready_ticks),
       n(s.damage_events),
       n(s.paused),
       n(s.stopped),
       n(s.failed),
       n(s.busy),
       text(s.phase),
       text(s.error)});
}
// Every Controller and complete Session/World/network/terrain/HP/creature
// field, including retained histories, is serialized after EVERY submission.
std::vector<std::string> trace(unsigned hz, unsigned mode) {
  Control::Config config;
  config.movement = Control::MovementPolicy::DryCreature;
  config.max_ready_ticks = 512;
  config.model = static_cast<R::Fpu::Model>(mode / 4);
  config.mul_bias = bool(mode & 2);
  config.div_nearest = bool(mode & 1);
  Control::Controller control(config);
  Clock clock;
  std::vector<std::string> states{control_snapshot(control.snapshot())};
  std::uint64_t now = 0, frame = 0;
  unsigned tick = 0;
  while (now < 1000000) {
    // hz=0 is an irregular uncapped render workload, including a long stall.
    auto next = hz ? (++frame * Clock::SECOND / hz)
                   : now + (frame++ % 4 == 0 ? 73123 : 317);
    next = std::min<std::uint64_t>(next, 1000000);
    clock.advance(next - now, [&] {
      const auto command = (tick < 15   ? Control::POS_X
                            : tick < 30 ? Control::POS_Y
                                        : Control::NEG_X) |
                           (tick % 7 == 0 ? Control::DAMAGE : 0);
      auto o = control.advance(command);
      check(!o.failed, "script must remain in supported domain");
      const auto full = control.snapshot();
      const auto &actor = full.session.world->objects.front();
      check(o.position == actor.pos && o.facing == actor.angle &&
                o.hp == full.session.hp->adapter.objects.front().value.hp &&
                o.frame == full.session.world->frame,
            "small read-only projection matches complete snapshot");
      states.push_back(control_snapshot(full));
      ++tick;
      return true;
    });
    now = next;
  }
  check(tick == 60 && control.observation().ready_ticks == 59,
        "60 submissions/second; first input waits for a ready tick");
  check(clock.alpha() == 0, "exact second has no fractional remainder");
  return states;
}
int main() {
  try {
    for (unsigned mode = 0; mode < 16; ++mode) {
      const auto baseline = trace(60, mode);
      for (auto hz : {30u, 144u, 240u, 0u})
        check(trace(hz, mode) == baseline,
              "complete per-tick traces differ by render rate");
    }
    Clock clock;
    unsigned ticks = 0;
    clock.advance(8333, [&] {
      ++ticks;
      return true;
    });
    check(ticks == 0 && clock.alpha() > 0.49, "fraction retained");
    clock.reset(); // pause, reset, teleport, load discard presentation debt
    clock.advance(8333, [&] {
      ++ticks;
      return true;
    });
    check(ticks == 0, "pause/reset must discard fractional debt");
    clock.advance(1000000, [&] {
      ++ticks;
      return false;
    });
    check(ticks == 1 && clock.alpha() == 0, "stop cancels remaining catch-up");
    bool refused = false;
    try {
      clock.advance(-1, [] { return true; });
    } catch (const std::invalid_argument &) {
      refused = true;
    }
    check(refused, "negative elapsed time refuses before mutation");
    std::cout << "Frame clock PASS: complete traces at 30/60/144/240/uncapped, "
                 "16 arithmetic modes\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
