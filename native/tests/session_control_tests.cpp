#include "champions/session_control.hpp"
#include "session_snapshot_trace.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>
namespace R = champions::simulation::rta;
namespace C = R::session_control;
namespace X = champions::test::session_trace;
namespace {
unsigned checks = 0;
void check(bool value, const char *why) {
  ++checks;
  if (!value)
    throw std::runtime_error(why);
}
template <class F> void refuses(F f, const char *why) {
  bool yes = false;
  try {
    f();
  } catch (const std::exception &) {
    yes = true;
  }
  check(yes, why);
}
C::Config config(unsigned c = 0) {
  C::Config v;
  v.model = static_cast<R::Fpu::Model>(c / 4);
  v.mul_bias = c & 2;
  v.div_nearest = c & 1;
  return v;
}
void motion_and_health() {
  for (unsigned c = 0; c < 16; ++c) {
    C::Controller v(config(c));
    const auto initial = v.snapshot();
    check(initial.session.config.peers == 1 &&
              initial.session.config.local == 0 &&
              initial.session.config.network_mode == 1,
          "controller owns a real single-peer Session");
    check(initial.session.world && initial.session.world->objects.size() == 1 &&
              initial.session.hp && initial.session.terrain,
          "default scene owns one actor, persistent HP and actual terrain");
    auto first = v.advance(C::POS_X | C::DAMAGE);
    check(first.ready_ticks == 0 && first.submissions == 1 &&
              first.position == R::world_queries::Vec3{} &&
              first.hp == 0x42c80000,
          "startup samples controls without fabricated readiness or immediate "
          "damage");
    check(v.snapshot().pending_damage_frames == std::vector<std::int64_t>{1},
          "startup rising damage edge retains submitted input frame");
    auto second = v.advance(C::POS_X | C::DAMAGE);
    check(
        second.ready_ticks == 1 && second.position[0] == 0x3f7fffff &&
            second.position[1] == 0 && second.position[2] == 0,
        "actual ready movement gives independently anchored raw displacement");
    check(second.hp == 0x42c40000 && second.damage_events == 1 &&
              v.snapshot().pending_damage_frames.empty(),
          "first exact input edge produces100 to98 once");
    auto third = v.advance(0);
    check(third.hp == 0x42c40000 && third.damage_events == 1,
          "held damage does not repeat on second buffered frame");
    v.advance(C::DAMAGE);
    auto fifth = v.advance(0);
    check(fifth.hp == 0x42c00000 && fifth.damage_events == 2,
          "released and pressed again produces persistent98 to96");
    const auto end = v.snapshot();
    check(end.session.world->shared_seed ==
                  initial.session.world->shared_seed &&
              end.session.seed == initial.session.seed &&
              end.session.draws.empty(),
          "movement and base HP consume neither shared nor dormant Session "
          "random stream");
    check(end.session.hp->adapter.world.seed.draws.empty(),
          "HP adapter fallback random ledger stays empty");
    check(initial.session.world->objects[0].pos == R::world_queries::Vec3{} &&
              initial.session.hp->adapter.objects[0].value.hp == 0x42c80000,
          "older complete snapshots remain owned after movement and damage");
    v.reset();
    check(X::snapshot(v.snapshot().session) == X::snapshot(initial.session),
          "reset constructs identical fresh authoritative state");
    check(v.observation().submissions == 0 &&
              v.observation().damage_events == 0 && !v.observation().failed,
          "reset clears controller epoch counters");
  }
  C::Controller cancel;
  cancel.advance(15);
  auto c = cancel.advance(0);
  check(c.position == R::world_queries::Vec3{}, "opposite axes cancel exactly");
  C::Controller diagonal;
  diagonal.advance(C::POS_X | C::NEG_Y);
  auto d = diagonal.advance(0);
  check(d.position[0] == 0x3f7fffff && d.position[1] == 0xbf7fffff,
        "diagonal axes preserve full authored speed without normalization");
}
void pause_and_release() {
  C::Controller v;
  v.advance(C::POS_X | C::DAMAGE);
  v.set_paused(true);
  const auto paused = v.snapshot();
  check(paused.held_input == 0 && paused.pending_damage_frames.empty() &&
            paused.minimum_input_frame == 2,
        "pause clears startup edge and sets exact buffered-input cutoff");
  for (unsigned i = 0; i < 4; ++i)
    v.advance(31);
  check(X::snapshot(v.snapshot().session) == X::snapshot(paused.session) &&
            v.observation().submissions == 1,
        "paused calls neither submit nor advance Session");
  v.set_paused(false);
  auto neutral = v.advance(0);
  check(neutral.ready_ticks == 1 &&
            neutral.position == R::world_queries::Vec3{} &&
            neutral.hp == 0x42c80000,
        "resume consumes stale queued frame neutrally rather than executing "
        "old controls");
  v.advance(C::POS_Y | C::DAMAGE);
  v.release_input();
  v.advance(0);
  check(v.observation().position == R::world_queries::Vec3{} &&
            v.observation().damage_events == 0,
        "focus release drops pending movement and damage together");
  v.advance(C::NEG_X | C::DAMAGE);
  auto accepted = v.advance(0);
  check(accepted.position[0] == 0xbf7fffff && accepted.hp == 0x42c40000,
        "fresh input after focus release remains usable");
  v.stop();
  const auto stopped = v.snapshot();
  v.advance(31);
  check(v.observation().stopped &&
            X::snapshot(v.snapshot().session) == X::snapshot(stopped.session),
        "explicit stop prevents further submissions");
  v.reset();
  check(!v.observation().stopped && v.observation().hp == C::INITIAL_HP,
        "explicit reset exits stop with fresh health");
}
void refusals_and_caps() {
  for (const auto *profile : {"norrath", "unknown", ""}) {
    auto c = config();
    c.profile = profile;
    refuses([&] { C::Controller x(c); }, "unvalidated game profiles refuse");
  }
  auto bad = config();
  bad.max_ready_ticks = C::MAX_READY_TICKS + 1;
  refuses([&] { C::Controller x(bad); },
          "configured history horizon cannot exceed supported cap");
  C::Controller v;
  const auto before = v.snapshot();
  refuses([&] { v.advance(32); },
          "unknown command bit refuses before mutation");
  check(X::snapshot(v.snapshot().session) == X::snapshot(before.session) &&
            v.snapshot().held_input == 0 && v.observation().submissions == 0,
        "invalid command preserves prior controller and Session state");
  auto cfg = config();
  cfg.max_ready_ticks = 2;
  C::Controller limited(cfg);
  limited.advance(C::POS_X);
  limited.advance(C::POS_X);
  limited.advance(C::POS_X);
  const auto prior = limited.observation();
  auto failed = limited.advance(C::POS_X);
  check(prior.ready_ticks == 2 && !failed.failed && failed.stopped &&
            failed.error == "ready_tick_limit",
        "finite ready horizon stops normally with an explicit reason");
  check(failed.position == prior.position && failed.hp == prior.hp,
        "budget refusal does not execute an extra move or hit");
  const auto failure = X::snapshot(limited.snapshot().session);
  limited.advance(31);
  check(X::snapshot(limited.snapshot().session) == failure,
        "failed controller never retries its partial Session");
  limited.reset();
  check(!limited.observation().failed && limited.observation().ready_ticks == 0,
        "reset creates a new bounded run after exhaustion");
  auto zero_work = R::world_queries::Limits{};
  zero_work.query_work = 0;
  C::Controller operational({}, v.terrain_data(), zero_work);
  operational.advance(1);
  auto broken = operational.advance(1);
  check(broken.failed && broken.stopped &&
            operational.snapshot().phase == "failed",
        "real query budget failure latches controller failure");
  auto retained = X::snapshot(operational.snapshot().session);
  operational.advance(0);
  check(X::snapshot(operational.snapshot().session) == retained,
        "operational failure retains full partial state without retry");
  auto terrain = v.terrain_data();
  terrain.grid[0] = {9999};
  refuses([&] { C::Controller invalid({}, terrain); },
          "malformed topology refuses independent candidate construction");
  check(X::snapshot(v.snapshot().session) == X::snapshot(before.session),
        "failed candidate does not modify existing controller");
}
void ownership_and_context() {
  static_assert(!std::is_copy_constructible<C::Controller>::value &&
                    !std::is_move_constructible<C::Controller>::value,
                "controller callbacks require stable ownership");
  C::Controller seed;
  auto level = seed.terrain_data();
  auto old = level;
  C::Controller supplied({}, level);
  level.heightfields[0].samples[0] = 32000;
  level.grid.clear();
  check(supplied.snapshot().initial_terrain.data.grid == old.grid &&
            supplied.terrain_data().heightfields[0].samples[0] == 0,
        "supplied terrain and reset source are deep-owned");
  supplied.advance(C::POS_X);
  supplied.advance(0);
  supplied.reset();
  check(supplied.snapshot().supplied_terrain &&
            supplied.terrain_data().grid == old.grid,
        "reset retains exact supplied terrain rather than replacing with "
        "default");
  auto &caller = R::set_fpu(R::Fpu::Model::Nearest, true, true);
  C::Controller own(config(0));
  own.advance(1);
  own.advance(0);
  check(caller.model == R::Fpu::Model::Nearest && caller.mul_bias &&
            caller.div_nearest,
        "native control operations restore caller FPU context");
  refuses([&] { own.advance(UINT32_MAX); },
          "invalid-mask exception is contained by caller");
  check(caller.model == R::Fpu::Model::Nearest && caller.mul_bias &&
            caller.div_nearest,
        "refusal also preserves caller arithmetic context");
}
} // namespace
int main() {
  try {
    motion_and_health();
    pause_and_release();
    refusals_and_caps();
    ownership_and_context();
    std::cout << checks << " session control checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "session control check " << checks << ": " << e.what() << '\n';
    return 1;
  }
}
