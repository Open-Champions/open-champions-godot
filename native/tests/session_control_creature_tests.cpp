#include "champions/session_control.hpp"
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace R = champions::simulation::rta;
namespace C = R::session_control;
namespace S = R::session;
namespace {
unsigned checks = 0;
void check(bool value, const char *why) {
  ++checks;
  if (!value)
    throw std::runtime_error(why);
}
template <class F> void refuses(F f, const char *why) {
  bool caught = false;
  try {
    f();
  } catch (const std::exception &) {
    caught = true;
  }
  check(caught, why);
}
C::Config config(unsigned c = 0) {
  C::Config out;
  out.model = static_cast<R::Fpu::Model>(c / 4);
  out.mul_bias = bool(c & 2);
  out.div_nearest = bool(c & 1);
  out.max_ready_ticks = C::MAX_CREATURE_READY_TICKS;
  out.movement = C::MovementPolicy::DryCreature;
  return out;
}
std::uint64_t raw(const C::Snapshot &s, std::uint32_t address,
                  unsigned width = 4) {
  const auto &bytes = s.session.hp->adapter.model.backing;
  std::uint64_t value = 0;
  for (unsigned i = 0; i < width; ++i) {
    const auto found = bytes.find(address + i);
    if (found != bytes.end())
      value |= std::uint64_t(found->second) << (8 * i);
  }
  return value;
}
std::size_t history(const C::Snapshot &s) {
  const auto &x = s.session;
  const auto &a = x.hp->adapter;
  const auto &h = a.model;
  const auto &m = *x.creature_movement;
  return x.events.size() + x.draws.size() + x.network.log.size() +
         x.network.outbox.size() + h.stores.size() + h.writes.size() +
         h.calls.size() + h.events.size() + a.world.events.size() +
         a.world.seed.draws.size() + a.world.cosmetic.draws.size() +
         m.writes.size() + m.calls.size() + m.events.size() +
         m.callback_results.size();
}
void authority_and_inputs() {
  for (unsigned mode = 0; mode < 16; ++mode) {
    C::Controller c(config(mode));
    const auto initial = c.snapshot();
    check(initial.config.movement == C::MovementPolicy::DryCreature &&
              initial.mover == 0 && initial.hp != 0 &&
              initial.session.creature_movement &&
              !initial.session.terrain->binding,
          "explicit creature policy has only the HP-backed mover");
    check(initial.session.config.peers == 1 &&
              initial.session.config.local == 0 &&
              initial.session.config.network_mode == 1 &&
              initial.session.world->objects.size() == 1,
          "actual one-peer Session owns a single actor");
    const auto &m = *initial.session.creature_movement;
    check(m.borrowed_memory && m.binding->id == initial.hp &&
              m.initial.base_table == C::CREATURE_BASE_TABLE &&
              raw(initial, initial.actor_address + 0x48) ==
                  C::CREATURE_BASE_TABLE,
          "borrowed HP backing and explicit dispatch table are observed");
    check(initial.session.world->objects[0].flags & 0x20u,
          "actor is explicitly the base creature type");
    check(raw(initial, initial.actor_address + 0xa8, 8) == 0x100000 &&
              !initial.session.hp->adapter.model.backing.count(
                  C::CREATURE_BASE_TABLE),
          "dry flag is installed without zero filling table padding");
    const auto first = c.advance(C::POS_X | C::DAMAGE);
    check(
        first.ready_ticks == 0 && first.position == R::world_queries::Vec3{} &&
            first.damage_events == 0 &&
            c.snapshot().pending_damage_frames == std::vector<std::int64_t>{1},
        "first submitted edge waits for original single-peer readiness");
    const auto second = c.advance(C::NEG_X);
    check(second.ready_ticks == 1 && second.position[0] == 0x3f7fffff &&
              second.position[1] == 0 && second.position[2] == 0 &&
              second.hp == 0x42c40000 && second.damage_events == 1,
          "ready frame uses earlier positive command and original creature "
          "displacement");
    check(second.movement == C::MovementPolicy::DryCreature &&
              second.ready_tick_limit == C::MAX_CREATURE_READY_TICKS,
          "cheap observation reports actual policy and configured limit");
    auto state = c.snapshot();
    check(
        raw(state, state.actor_address + 0x10) == second.position[0] &&
            raw(state, state.actor_address + 0x228) == second.hp &&
            state.session.hp->adapter.objects[0].value.pos[0] ==
                second.position[0],
        "movement then health keeps raw, typed HP and World position aligned");
    check(state.session.hp->adapter.model.stores.size() ==
              state.session.creature_movement->writes.size() +
                  state.session.hp->adapter.model.writes.size(),
          "one shared Memory store log includes both original bodies");
    check(state.session.creature_movement->last_velocity->at(0) == C::SPEED &&
              state.session.creature_movement->calls.size() == 4 &&
              state.session.creature_movement->calls[0].name ==
                  "world_slideMove" &&
              state.session.creature_movement->calls[1].name ==
                  "obj_updateGridCell" &&
              state.session.creature_movement->calls[2].name ==
                  "world_collectObjectsInBox" &&
              state.session.creature_movement->calls[3].name ==
                  "terrain_heightBelow",
          "fixed services preserve original movement order");
    check(state.session.world->shared_seed ==
                  initial.session.world->shared_seed &&
              state.session.world->objects[0].seed ==
                  initial.session.world->objects[0].seed &&
              state.session.seed == initial.session.seed &&
              state.session.draws.empty() &&
              state.session.hp->adapter.world.seed.draws.empty() &&
              state.session.hp->adapter.world.cosmetic.draws.empty() &&
              !state.session.hp->adapter.world.shared_random,
          "movement does not copy or advance any random authority");
    c.advance(0);
    c.advance(C::DAMAGE);
    check(c.advance(0).hp == 0x42c00000,
          "second rising edge preserves98 to96 HP");
    c.reset();
    state = c.snapshot();
    check(state.config.movement == C::MovementPolicy::DryCreature &&
              state.session.hp->adapter.model.backing ==
                  initial.session.hp->adapter.model.backing &&
              state.session.creature_movement->writes.empty() &&
              state.submissions == 0 && state.pending_damage_frames.empty(),
          "reset constructs same policy and fresh backing, never clears a live "
          "history");
    c.advance(C::POS_X | C::NEG_X | C::POS_Y | C::NEG_Y);
    check(c.advance(0).position == R::world_queries::Vec3{},
          "opposed axes cancel with original grounded gravity");
    c.reset();
    c.advance(C::POS_X | C::NEG_Y);
    const auto diagonal = c.advance(0);
    check(diagonal.position[0] == 0x3f7fffff &&
              diagonal.position[1] == 0xbf7fffff,
          "diagonal axes keep independent authored speeds");
  }
}
void lifecycle_and_failures() {
  C::Controller c(config());
  c.advance(C::POS_X | C::DAMAGE);
  c.set_paused(true);
  const auto p = c.snapshot();
  check(p.pending_damage_frames.empty() && p.held_input == 0 &&
            p.minimum_input_frame == 2,
        "pause discards queued edges and records next input cutoff");
  c.advance(31);
  check(c.snapshot().submissions == p.submissions &&
            c.snapshot().session.hp->adapter.model.backing ==
                p.session.hp->adapter.model.backing,
        "paused input does not mutate native simulation");
  c.set_paused(false);
  const auto resumed = c.advance(0);
  check(resumed.position == R::world_queries::Vec3{} &&
            resumed.hp == C::INITIAL_HP && resumed.damage_events == 0,
        "buffered pre-pause frame drains neutrally without queued catchup");
  c.advance(C::DAMAGE);
  c.release_input();
  check(c.advance(0).hp == C::INITIAL_HP,
        "focus release discards a submitted but not-ready damage edge");
  c.stop();
  const auto stopped = c.snapshot();
  check(c.advance(31).stopped &&
            c.snapshot().submissions == stopped.submissions,
        "stop preserves state and refuses implicit retry");
  c.reset();
  const auto before = c.snapshot();
  refuses([&] { c.advance(32); },
          "unknown command bit refuses before mutation");
  check(c.snapshot().submissions == 0 && !c.observation().failed &&
            c.snapshot().session.hp->adapter.model.backing ==
                before.session.hp->adapter.model.backing,
        "invalid input leaves complete backing and lifecycle unchanged");

  R::world_queries::Limits limits;
  limits.query_work = 0;
  C::Controller broken(config(), {}, limits);
  broken.advance(C::POS_X | C::DAMAGE);
  const auto result = broken.advance(0);
  auto failed = broken.snapshot();
  check(result.failed && result.stopped && failed.session.failed &&
            failed.session.failure_operation == "move_creature" &&
            failed.session.creature_movement->phase == "slide",
        "real terrain work refusal retains movement phase and latches Session");
  check(failed.session.creature_movement->writes.size() == 6 &&
            failed.session.creature_movement->calls.size() == 1 &&
            failed.session.creature_movement->callback_results.empty() &&
            failed.session.hp->adapter.model.writes.empty() &&
            failed.damage_events == 0,
        "query failure retains root-consumption prefix before damage edge");
  check(raw(failed, failed.actor_address + 0x10) == result.position[0] &&
            raw(failed, failed.actor_address + 0x228) == C::INITIAL_HP,
        "failed prefix publication preserves World/HP aliases");
  broken.advance(31);
  check(broken.snapshot().session.hp->adapter.model.backing ==
                failed.session.hp->adapter.model.backing &&
            broken.snapshot().session.creature_movement->events.size() ==
                failed.session.creature_movement->events.size(),
        "failed controller cannot replay partial movement automatically");
}
void supplied_terrain_and_context() {
  C::Controller base(config());
  auto data = base.terrain_data();
  data.elements[0].flags = 4;
  for (unsigned y = 0; y < 129; ++y)
    for (unsigned x = 66; x < 129; ++x)
      data.heightfields[0].samples[y * 129 + x] = 1;
  C::Controller wall(config(), data);
  data.heightfields[0].samples.assign(129 * 129, 0);
  for (unsigned i = 0; i < 16; ++i)
    check(!wall.advance(C::POS_X).failed,
          "supplied collision terrain runs through original creature services");
  auto state = wall.snapshot();
  bool hit = false;
  for (const auto &result : state.session.creature_movement->callback_results)
    if (result.value.kind == R::creature_movement::ValueKind::Tuple &&
        result.value.items.size() == 2 && result.value.items[0].scalar != 0 &&
        result.value.items[1].kind == R::creature_movement::ValueKind::Vector)
      hit = true;
  check(hit && state.session.world->objects[0].pos[0] > 0 &&
            state.session.world->objects[0].pos[0] < 0x41000000,
        "actual supplied samples block movement before authored wall atX8");
  wall.reset();
  state = wall.snapshot();
  check(state.config.movement == C::MovementPolicy::DryCreature &&
            state.supplied_terrain &&
            wall.terrain_data().heightfields[0].samples[66] == 1 &&
            state.session.creature_movement->calls.empty(),
        "reset owns original terrain and policy despite caller mutation");
  auto &caller = R::set_fpu(R::Fpu::Model::Nearest, true, true);
  C::Controller own(config(0));
  own.advance(C::POS_X);
  own.advance(0);
  check(caller.model == R::Fpu::Model::Nearest && caller.mul_bias &&
            caller.div_nearest,
        "creature controller restores ambient caller arithmetic after ready "
        "work");
  refuses([&] { own.advance(UINT32_MAX); },
          "bad input remains outside simulation");
  check(caller.model == R::Fpu::Model::Nearest && caller.mul_bias &&
            caller.div_nearest,
        "refusal preserves caller arithmetic selection");
}
void whole_run_and_policy() {
  auto invalid = config();
  invalid.max_ready_ticks = C::MAX_READY_TICKS;
  refuses([&] { C::Controller c(invalid); },
          "legacy tick limit is invalid for creature policy");
  invalid.max_ready_ticks = 0;
  refuses([&] { C::Controller c(invalid); }, "zero creature budget refuses");
  invalid = config();
  invalid.movement = static_cast<C::MovementPolicy>(255);
  refuses([&] { C::Controller c(invalid); }, "unknown movement policy refuses");
  C::Controller legacy;
  check(legacy.observation().movement == C::MovementPolicy::Isolated &&
            legacy.observation().ready_tick_limit == 2000 &&
            !legacy.snapshot().session.creature_movement &&
            legacy.snapshot().session.terrain->binding,
        "legacy defaults retain their existing authority and limit");

  C::Controller c(config());
  std::size_t old_history = 0, old_writes = 0;
  // Alternating axes stay in supplied terrain while every other input is a
  // damage edge. The first submission is warmup, then512 actual ready ticks.
  for (std::size_t i = 0; i <= C::MAX_CREATURE_READY_TICKS; ++i) {
    const auto value = c.advance((i & 1) ? C::NEG_X : C::POS_X | C::DAMAGE);
    check(!value.failed, "full supported run never exhausts history early");
    const auto s = c.snapshot();
    const auto count = history(s);
    check(count >= old_history &&
              s.session.creature_movement->writes.size() >= old_writes,
          "complete histories remain monotonic throughout full run");
    check(count <= 96 * s.ready_ticks && count + 128 < S::MAX_EVENT_HISTORY,
          "conservative whole-run aggregate bound includes both bodies and "
          "tasks");
    old_history = count;
    old_writes = s.session.creature_movement->writes.size();
  }
  const auto end = c.snapshot();
  check(end.ready_ticks == 512 && end.submissions == 513 && end.stopped &&
            !end.failed && end.error == "ready_tick_limit" &&
            end.damage_events == 256 && c.observation().hp == 0xc3ce0000,
        "full budget stops explicitly with all256health edges and original "
        "negative HP");
  check(end.session.creature_movement->writes.size() >= 512 * 5 &&
            end.session.hp->adapter.model.stores.size() ==
                end.session.creature_movement->writes.size() +
                    end.session.hp->adapter.model.writes.size(),
        "full run retains all shared stores without clearing or duplicating "
        "logs");
  c.advance(31);
  check(c.snapshot().submissions == end.submissions &&
            history(c.snapshot()) == old_history,
        "budget stop performs no later simulation work");
}
} // namespace
int main() {
  try {
    authority_and_inputs();
    lifecycle_and_failures();
    supplied_terrain_and_context();
    whole_run_and_policy();
    std::cout << checks << " creature control checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "creature control check " << checks << ": " << e.what()
              << '\n';
    return 1;
  }
}
