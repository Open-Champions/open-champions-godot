#pragma once
#include "session_facing_trace.hpp"
namespace champions::test::facing_direct {
namespace R = champions::simulation::rta;
namespace N = R::native_save;
namespace C = R::session_control;
namespace T = champions::test::facing_trace;
struct Fixture {
  R::session::Session session;
  R::session::HpId hp = 0;
  std::uint32_t address = 0;
  std::uint64_t serial = 0;
  Fixture(std::size_t history = 65536, bool recovery = false, unsigned f = 0)
      : session([f] {
          R::session::Config c;
          c.network_mode = 1;
          c.network_seed = 17;
          c.model = R::Fpu::Model(f / 4);
          c.mul_bias = f & 2;
          c.div_nearest = f & 1;
          return c;
        }()) {
    R::session::ObjectBindings b;
    b.policy = R::objects::CallbackPolicy::ReferenceBase;
    session.enable_objects(17, b);
    session.with_world([&](auto &w) {
      auto *o = w.new_object(recovery ? 0x500 : 0x400, {0, 0, 0});
      o->flags |= 0x20;
      o->radius = C::RADIUS;
      o->height = 0x41000000;
      address = o->addr;
      serial = o->serial;
      w.run_every_frame(*o);
    });
    R::session::HpLimits limits;
    limits.memory.history_entries = history;
    session.enable_hp(limits);
    hp = session.bind_hp(
        serial, address,
        {recovery ? R::session_recovery::Initial{}.health : C::INITIAL_HP,
         0x100000,
         {}});
    N::Scene base(T::config(0, 0, false));
    auto terrain = base.setup().initial_terrain;
    session.enable_terrain(terrain.data, terrain.limits);
    session.enable_creature_movement();
    R::session::CreatureInitial initial;
    initial.base_table = C::CREATURE_BASE_TABLE;
    session.bind_creature_mover(hp, initial);
    session.enable_turning({1, 1});
    session.bind_turner(hp, {0x3f800000, 0, 0});
    if (recovery)
      session.enable_recovery(hp, {});
    session.register_object_task(
        0xf4000001, "authored_control", 20, 0, [this, recovery](auto &s) {
          auto a = s.observe_actor(hp);
          s.turn_toward(hp, 0,
                        {R::add(a.position[0], C::SPEED),
                         R::add(a.position[1], C::SPEED), a.position[2]});
          s.move_creature(hp, {C::SPEED, C::SPEED, 0});
          if (recovery)
            s.recover_player(hp);
        });
  }
  void submit(bool no_sim = false) {
    session.submit({5, 0, 128, 128, 128, 128, 0, 0}, 0, 0, 1, no_sim);
  }
};
inline std::string observe(R::session::Snapshot s) {
  auto turning = champions::test::session_turn_trace::turning(*s.turning);
  s.turning.reset();
  return T::sequence(
      {champions::test::recovery_trace::observation(std::move(s)),
       std::move(turning)});
}
inline std::string run(const std::string &name, unsigned f) {
  Fixture x(name == "direct_failure" ? 1 : 65536, false, f);
  if (name == "direct_pending")
    x.session.set_level_pending(true);
  const auto start = observe(x.session.snapshot());
  auto previous = T::parsed(start);
  std::vector<std::string> steps;
  for (unsigned k = 0; k < 4; ++k) {
    if (k == 3 && name == "direct_pending")
      x.session.set_level_pending(false);
    std::string result = "ok";
    try {
      x.submit(name == "direct_no_sim" && k < 3);
    } catch (const std::exception &) {
      result = "error";
    }
    auto current = T::parsed(observe(x.session.snapshot()));
    steps.push_back(
        T::sequence({T::text(result), T::delta(previous, current)}));
    previous = std::move(current);
  }
  return T::sequence({start, T::sequence(steps)});
}
} // namespace champions::test::facing_direct
