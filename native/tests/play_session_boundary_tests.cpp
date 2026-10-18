// Scope/blocker regression probe. This does not enable a play-session mode.
#include "champions/detail/checkpoint.hpp"
#include "champions/native_save.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>

namespace R = champions::simulation::rta;
namespace C = R::session_control;
namespace D = R::detail;
namespace O = R::objects;
namespace {
unsigned checks = 0;
void check(bool ok, const char *why) {
  ++checks;
  if (!ok)
    throw std::runtime_error(why);
}
void world_checkpoint_boundary() {
  O::World world(O::CallbackPolicy::ReferenceBase);
  auto *actor = world.new_object(0x400, {0, 0, 0});
  check(actor != nullptr, "synthetic singleton allocation");
  world.run_every_frame(*actor);
  // Real original scheduler increments frame, without altering a counter via
  // a private-state test hook. The callback runs the existing object scheduler.
  const auto tick = [&](std::uint32_t) { world.run_objects(); };
  world.tasks.add(1, 10, "boundary", 0);
  world.tasks_run(2000, tick);
  check(world.frame == 2000, "scheduler reaches frame 2000");
  D::Checkpoint state;
  D::CheckpointAccess::run(world, state);
  state.finish();

  O::World restored(O::CallbackPolicy::ReferenceBase);
  auto *restored_actor = restored.new_object(0x400, {0, 0, 0});
  check(restored_actor != nullptr, "restore singleton allocation");
  restored.run_every_frame(*restored_actor);
  restored.tasks.add(1, 10, "boundary", 0);
  D::Checkpoint input(state.bytes);
  D::CheckpointAccess::run(restored, input);
  input.finish();
  D::Checkpoint canonical;
  D::CheckpointAccess::run(restored, canonical);
  check(canonical.bytes == state.bytes, "frame 2000 canonical restore");
  D::Checkpoint opted;
  opted.play_continuation = true;
  D::CheckpointAccess::run(world, opted);
  check(opted.bytes == state.bytes,
        "opt-in does not change common World bytes");

  world.tasks_run(1, tick);
  const auto before = world.snapshot();
  check(world.frame == 2001, "scheduler continues to frame 2001");
  bool refused = false;
  try {
    D::Checkpoint too_late;
    D::CheckpointAccess::run(world, too_late);
  } catch (const std::runtime_error &error) {
    refused = std::string(error.what()) == "checkpoint World counters";
  }
  check(refused, "legacy World checkpoint refuses frame 2001");
  D::Checkpoint continuation;
  continuation.play_continuation = true;
  D::CheckpointAccess::run(world, continuation);
  D::Checkpoint resumed(continuation.bytes);
  resumed.play_continuation = true;
  D::CheckpointAccess::run(restored, resumed);
  resumed.finish();
  check(restored.frame == 2001, "explicit continuation retains absolute frame");
  // Independently exercise the iterator guard while frame remains legacy-valid.
  // World fields are explicit eight-byte little-endian scalar words. The
  // iterator serial follows the 18 words preceding it in this closed codec.
  auto large_iterator = state.bytes;
  for (unsigned b = 0; b < 8; ++b)
    large_iterator[144 + b] = std::uint8_t(UINT64_C(1000001) >> (8 * b));
  D::Checkpoint large(large_iterator);
  large.play_continuation = true;
  D::CheckpointAccess::run(restored, large);
  large.finish();
  check(restored.snapshot().iterator_serial == 1000001 &&
            restored.frame == 2000,
        "continuation preserves wide iterator identity");
  bool iterator_refused = false;
  try {
    D::Checkpoint legacy;
    D::CheckpointAccess::run(restored, legacy);
  } catch (const std::runtime_error &e) {
    iterator_refused = std::string(e.what()) == "checkpoint World counters";
  }
  check(iterator_refused, "legacy still refuses wide iterator identities");
  const auto after = world.snapshot();
  check(after.frame == before.frame &&
            after.iterator_serial == before.iterator_serial &&
            after.handle_free == before.handle_free &&
            after.shared_seed == before.shared_seed &&
            after.links.size() == before.links.size() &&
            after.objects.front().serial == before.objects.front().serial,
        "failed encoding preserves live counters and identity");
  std::cout << "world_checkpoint: frame=2000 roundtrip=pass; frame=2001 "
               "error=checkpoint World counters; iterator_serial="
            << after.iterator_serial << '\n';
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
void diagnostic_boundary() {
  C::Config config;
  config.movement = C::MovementPolicy::DryCreature;
  config.max_ready_ticks = C::MAX_CREATURE_READY_TICKS;
  R::native_save::Scene scene(config);
  const auto begin = std::chrono::steady_clock::now();
  std::size_t previous = 0;
  for (unsigned i = 0; i <= 512; ++i) {
    // Same bounded movement/health workload at every measured milestone.
    const auto command =
        ((i / 32) % 2 ? C::NEG_X : C::POS_X) | (i % 2 ? 0 : C::DAMAGE);
    const auto observation = scene.advance(command);
    check(!observation.failed, "legacy workload remains healthy");
    if (i == 128 || i == 256 || i == 512) {
      const auto snapshot = scene.snapshot();
      const auto records = history(snapshot);
      check(records > previous, "legacy retains all completed histories");
      previous = records;
      const auto elapsed =
          std::chrono::duration_cast<std::chrono::microseconds>(
              std::chrono::steady_clock::now() - begin)
              .count();
      std::cout << "diagnostic ready_ticks=" << observation.ready_ticks
                << " retained_records=" << records << " memory_bytes="
                << snapshot.session.hp->adapter.model.backing.size()
                << " world_links=" << snapshot.session.world->links.size()
                << " iterator_serial="
                << snapshot.session.world->iterator_serial
                << " elapsed_us=" << elapsed << '\n';
    }
  }
  auto observation = scene.observation();
  check(observation.ready_ticks == 512 && observation.submissions == 513 &&
            observation.stopped && !observation.failed &&
            observation.error == "ready_tick_limit",
        "legacy 512 tick cap remains enforced");
  const auto saved = scene.save();
  scene.advance(C::POS_X);
  check(scene.observation().ready_ticks == 512,
        "extra submission does not reset or advance stopped gameplay");
  auto restored = R::native_save::Scene::load(saved, "rta");
  check(restored->save() == saved, "complete legacy replay byte roundtrip");
  check(history(restored->snapshot()) == previous,
        "complete legacy history retained after load");
  std::cout << "diagnostic replay_bytes=" << saved.size() << '\n';
}
} // namespace
int main() {
  try {
    world_checkpoint_boundary();
    diagnostic_boundary();
    std::cout << "play-session-boundary: " << checks << " checks passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "play-session-boundary: " << error.what() << '\n';
    return 1;
  }
}
