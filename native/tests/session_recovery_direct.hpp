#pragma once
#include "session_recovery_recipes.hpp"
#include "session_recovery_trace.hpp"
namespace champions::test::recovery_direct {
namespace R = champions::simulation::rta;
namespace T = recovery_trace;
inline std::string run(const std::string &name, unsigned f) {
  auto config = recovery_recipes::config(f, 0);
  R::session::Config cfg;
  cfg.model = config.model;
  cfg.mul_bias = config.mul_bias;
  cfg.div_nearest = config.div_nearest;
  cfg.network_mode = 1;
  cfg.network_seed = 17;
  R::session::Session s(cfg);
  R::session::ObjectBindings callbacks;
  callbacks.policy = R::objects::CallbackPolicy::ReferenceBase;
  s.enable_objects(17, callbacks);
  std::uint64_t serial = 0;
  std::uint32_t address = 0;
  s.with_world([&](auto &w) {
    auto *a = w.new_object(0x500, {0, 0, 0});
    a->flags |= 0x20;
    a->radius = R::session_control::RADIUS;
    a->height = 0x41000000;
    serial = a->serial;
    address = a->addr;
    w.run_every_frame(*a);
  });
  R::session::HpLimits limits;
  if (name == "direct_health_failure")
    limits.memory.history_entries = 1;
  else if (name == "direct_mana_failure")
    limits.memory.history_entries = 3;
  else if (name != "direct_pending" && name != "direct_no_sim")
    throw std::runtime_error("unknown direct recipe");
  s.enable_hp(limits);
  const auto hp = s.bind_hp(serial, address, {config.recovery->health, 0, {}});
  s.enable_recovery(hp, *config.recovery);
  s.register_object_task(0xf4000001, "authored_control", 20, 0,
                         [hp](auto &running) { running.recover_player(hp); });
  if (name == "direct_pending")
    s.set_level_pending(true);
  const auto initial = T::observation(s.snapshot());
  auto before = T::parsed(initial);
  std::vector<std::string> steps;
  for (unsigned k = 0;
       k < (name == "direct_pending" || name == "direct_no_sim" ? 4u : 3u);
       ++k) {
    if (name == "direct_pending" && k == 3)
      s.set_level_pending(false);
    std::string result = "ok";
    try {
      s.submit({0, 0, 128, 128, 128, 128, 0, 0}, 0, 0, 1,
               name == "direct_no_sim" && k < 3);
    } catch (const std::exception &) {
      result = "error";
    }
    auto current = T::parsed(T::observation(s.snapshot()));
    steps.push_back(T::sequence({T::text(result), T::delta(before, current)}));
    before = std::move(current);
  }
  return T::sequence({initial, T::sequence(steps)});
}
} // namespace champions::test::recovery_direct
