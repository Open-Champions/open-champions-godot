#pragma once
#include "champions/session_control.hpp"
#include "session_creature_trace.hpp"
namespace champions::test::recovery_trace {
using namespace session_trace;
inline std::string initial(const R::session_recovery::Initial &i) {
  std::vector<std::string> ranks;
  for (const auto &v : i.ranks)
    ranks.push_back(sequence({n(v.first), n(v.second)}));
  return sequence({n(i.slot),
                   n(i.record),
                   n(i.player_class),
                   n(i.health),
                   n(i.max_health),
                   n(i.mana),
                   n(i.max_mana),
                   n(i.pending_health),
                   n(i.health_rate),
                   n(i.pending_mana),
                   n(i.mana_rate),
                   n(i.health_bonus),
                   n(i.mana_bonus),
                   integers(i.passive_masks),
                   integers(i.passive_levels),
                   sequence(ranks),
                   integers(i.equipment),
                   n(unsigned(i.spells)),
                   n(i.blessed_type_token),
                   n(i.copy_at_setup)});
}
inline std::string call(const R::player::Call &c) {
  std::vector<std::string> args;
  for (const auto &a : c.args)
    args.push_back(n(std::get<std::uint32_t>(a)));
  return sequence(
      {text(c.name.empty() ? R::player::service_name(c.service) : c.name),
       sequence(args)});
}
inline std::string recovery(const S::RecoverySnapshot &r) {
  std::vector<std::string> writes, calls, events, callbacks;
  for (const auto &v : r.writes)
    writes.push_back(write(v));
  for (const auto &v : r.calls)
    calls.push_back(call(v));
  for (const auto &v : r.events)
    events.push_back(v.is_call ? sequence({n(1), call(v.call)})
                               : sequence({n(0), write(v.write)}));
  for (const auto &v : r.callbacks)
    callbacks.push_back(sequence({text(v.kind), text(v.name), n(v.present)}));
  const auto &b = r.binding;
  return sequence({sequence({n(b.id), n(b.world_serial), n(b.address),
                             n(b.adapter_generation)}),
                   initial(r.initial), text(r.policy), text(r.phase),
                   text(r.publication_error), text(r.memory_owner), n(r.depth),
                   n(r.calls_this_tick), sequence(writes), sequence(calls),
                   sequence(events), n(r.draws), n(r.R0),
                   n(unsigned(r.callback_policy)), sequence(callbacks),
                   n(r.object_run_completed),
                   optional_number(r.task_priority)});
}
inline std::string observation(S::Snapshot s) {
  // Consume temporary snapshots instead of copying their complete histories.
  auto extension = s.recovery ? recovery(*s.recovery) : "_";
  s.recovery.reset();
  return sequence({creature_trace::observation(s), std::move(extension)});
}
inline std::string control(R::session_control::Snapshot s) {
  return sequence(
      {sequence({text(s.config.profile), n(unsigned(s.config.model)),
                 n(s.config.mul_bias), n(s.config.div_nearest),
                 n(s.config.shared_seed), n(s.config.max_ready_ticks),
                 n(unsigned(s.config.movement)),
                 s.config.recovery ? initial(*s.config.recovery) : "_"}),
       level_snapshot(s.initial_terrain),
       n(s.supplied_terrain),
       observation(std::move(s.session)),
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
} // namespace champions::test::recovery_trace
