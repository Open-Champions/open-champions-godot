#include "champions/detail/checkpoint_control_anim_cache.hpp"
#include "champions/frame_clock.hpp"
#include "control_anim_cache_trace.hpp"
#include <algorithm>
#include <functional>
#include <iostream>
namespace R = champions::simulation::rta;
namespace C = R::session_control;
namespace N = R::native_save;
namespace T = champions::test::control_anim_cache_trace;
unsigned checks = 0;
void check(bool b, const char *why) {
  ++checks;
  if (!b)
    throw std::runtime_error(why);
}
void refuses(std::function<void()> f) {
  bool b = false;
  try {
    f();
  } catch (const std::exception &) {
    b = true;
  }
  check(b, "expected refusal");
}
void equal(N::Scene &a, N::Scene &b) {
  check(T::control(a.snapshot()) == T::control(b.snapshot()),
        "complete continuation mismatch");
}
std::vector<std::string> paced(unsigned f, unsigned fps) {
  N::Scene scene(T::config(f));
  for (auto command : {std::pair<std::int64_t, C::AnimationCommandKind>{
                           1, C::AnimationCommandKind::Start},
                       {4, C::AnimationCommandKind::Restart},
                       {7, C::AnimationCommandKind::Stop},
                       {9, C::AnimationCommandKind::Start}})
    scene.queue_animation(command.second, 1, command.first);
  champions::presentation::rta::FrameClock clock;
  std::vector<std::string> trace;
  unsigned render = 0;
  while (trace.size() < 12) {
    const auto usec =
        fps ? 1000000 / fps
            : std::array<int, 5>{1000, 1900, 400, 2100, 777}[render % 5];
    clock.advance(usec, [&] {
      scene.advance(trace.size() % 3 ? 5 : 16);
      trace.push_back(T::control(scene.snapshot()));
      return trace.size() < 12;
    });
    ++render;
  }
  return trace;
}
void pacing() {
  for (unsigned f = 0; f < 16; ++f) {
    auto baseline = paced(f, 60);
    for (auto fps : {30u, 144u, 240u, 0u})
      check(baseline == paced(f, fps),
            "render schedule changed complete state");
  }
}
void restore_membership() {
  auto cfg = T::config();
  N::Scene scene(cfg);
  for (auto clip : {1u, 2u, 3u}) {
    scene.queue_animation(C::AnimationCommandKind::Start, clip);
    scene.advance(0);
    scene.advance(0);
  }
  scene.queue_animation(C::AnimationCommandKind::Stop, 1);
  scene.advance(0);
  scene.advance(0);
  auto restored = N::Scene::load(scene.save_snapshot(), "rta");
  equal(scene, *restored);
  const auto snap = scene.snapshot();
  // A valid supplied stale owner need not be reciprocal. It is not repaired
  // during direct restoration, even though this command subset does not create
  // it.
  auto runtime_bytes = [](R::animation_runtime::Snapshot r) {
    R::detail::Checkpoint a;
    a.fields(r.limits, r.decoder);
    auto count = a.count(r.states.size(), 1);
    (void)count;
    for (auto &state : r.states)
      a.fields(state.id, state.value);
    return a.bytes;
  };
  auto write32 = [](N::Bytes &b, std::size_t at, std::uint32_t v) {
    for (unsigned k = 0; k < 4; ++k)
      b[at + k] = std::uint8_t(v >> (8 * k));
  };
  auto read32 = [](const N::Bytes &b, std::size_t at) {
    std::uint32_t v = 0;
    for (unsigned k = 0; k < 4; ++k)
      v |= std::uint32_t(b[at + k]) << (8 * k);
    return v;
  };
  auto checksum = [](const N::Bytes &b, std::size_t at, std::size_t n) {
    std::uint32_t v = ~0u;
    for (std::size_t j = at; j < at + n; ++j) {
      v ^= b[j];
      for (unsigned k = 0; k < 8; ++k)
        v = (v >> 1) ^ ((v & 1) ? 0xedb88320u : 0);
    }
    return ~v;
  };
  auto original = snap.session.animation_cache->runtime;
  auto corrupt_runtime = [&](R::animation_runtime::Snapshot value) {
    auto file = scene.save_snapshot();
    auto old = runtime_bytes(original);
    auto replacement = runtime_bytes(value);
    const std::size_t current = 56 + read32(file, 24),
                      length = read32(file, 32);
    auto found =
        std::search(file.begin() + current, file.begin() + current + length,
                    old.begin(), old.end());
    check(found != file.begin() + current + length,
          "corrupt runtime field location");
    check(old.size() == replacement.size(), "fixed-size corruption fixture");
    std::copy(replacement.begin(), replacement.end(), found);
    write32(file, 36, checksum(file, current, length));
    write32(file, 20, 0);
    write32(file, 20, checksum(file, 0, 56));
    refuses([&] { N::Scene::load(file, "rta"); });
  };
  {
    auto r = original;
    r.decoder.order[0] = r.decoder.order[1];
    corrupt_runtime(r);
  }
  {
    auto r = original;
    r.decoder.clips.front().id = 2;
    corrupt_runtime(r);
  }
  {
    auto r = original;
    r.decoder.tracks.front().id = 2;
    corrupt_runtime(r);
  }
  {
    auto r = original;
    r.states.front().id = 2;
    corrupt_runtime(r);
  }
  {
    auto r = original;
    r.states.front().value.count = 9;
    corrupt_runtime(r);
  }
  {
    auto r = original;
    r.states.front().value.tracks[0] = 2;
    corrupt_runtime(r);
  }
  {
    auto r = original;
    r.decoder.tracks.front().value.slot = 5;
    corrupt_runtime(r);
  }
  {
    auto r = original;
    r.decoder.slots.front().clip = 5;
    corrupt_runtime(r);
  }
  {
    auto r = original;
    r.decoder.slots.front().cursor = UINT64_MAX;
    corrupt_runtime(r);
  }
  {
    auto r = original;
    r.decoder.slots.front().joints[63].pt = INT64_MIN;
    corrupt_runtime(r);
  }
  // Retained command identity must agree with the direct current section even
  // when every affected checksum has been refreshed.
  {
    auto file = scene.save_snapshot();
    const auto journal =
        56 + read32(file, 24) + read32(file, 32) + read32(file, 40);
    const auto size = read32(file, 48);
    const auto first_action =
        journal + 40 + read32(file, journal + 36) + 28 + 1 + 4;
    check(file[first_action] == 4 && file[first_action + 1] == 1,
          "journal fixture location");
    file[first_action + 1] = 2;
    write32(file, journal + 20, 0);
    write32(file, journal + 20, checksum(file, journal, size));
    write32(file, 52, checksum(file, journal, size));
    write32(file, 20, 0);
    write32(file, 20, checksum(file, 0, 56));
    refuses([&] { N::Scene::load(file, "rta"); });
  }

  {
    auto r = original;
    r.decoder.tracks.front().value.clip = 3;
    corrupt_runtime(
        r); // A member ID can still contradict the retained identity.
  }
  {
    auto r = original;
    r.decoder.tracks[1].value.time = R::ZERO;
    corrupt_runtime(r);
  }
  {
    auto r = original;
    const auto slot =
        snap.session.animation_cache->bindings.front().last_decoded->slot;
    r.decoder.slots[slot - 1].joints[0].pos[0] ^= 0x10000u;
    corrupt_runtime(r);
  }
  auto old_bytes = runtime_bytes(original);
  auto supplied = original;
  supplied.decoder.slots.front().owner = 4;
  check(supplied.decoder.tracks[3].value.slot != 1, "supplied stale fixture");
  auto replacement = runtime_bytes(supplied);
  auto file = scene.save_snapshot();
  const std::size_t current = 56 + read32(file, 24), length = read32(file, 32);
  auto found =
      std::search(file.begin() + current, file.begin() + current + length,
                  old_bytes.begin(), old_bytes.end());
  check(found != file.begin() + current + length,
        "runtime checkpoint field location");
  const auto location = std::size_t(found - file.begin());
  std::copy(replacement.begin(), replacement.end(), file.begin() + location);
  auto refresh = [&](N::Bytes &b) {
    write32(b, 36, checksum(b, current, length));
    write32(b, 20, 0);
    write32(b, 20, checksum(b, 0, 56));
  };
  refresh(file);
  auto stale = N::Scene::load(file, "rta");
  check(stale->snapshot()
                .session.animation_cache->runtime.decoder.slots.front()
                .owner == 4,
        "direct restore repaired valid nonreciprocal owner");
  auto again = N::Scene::load(stale->save_snapshot(), "rta");
  equal(*stale, *again);
  for (unsigned k = 0; k < 3; ++k) {
    stale->advance(0);
    again->advance(0);
    equal(*stale, *again);
  }
  auto corrupt = supplied;
  corrupt.decoder.slots.front().owner = 999;
  replacement = runtime_bytes(corrupt);
  std::copy(replacement.begin(), replacement.end(), file.begin() + location);
  refresh(file);
  refuses([&] { N::Scene::load(file, "rta"); });
  R::detail::Checkpoint output;
  output.control_anim_cache = true;
  C::Controller fresh(cfg);
  R::detail::CheckpointAccess::run(fresh, output);
  R::detail::Checkpoint legacy;
  refuses([&] { R::detail::CheckpointAccess::run(fresh, legacy); });
  for (unsigned i = 0; i < 64; ++i) {
    auto bad = output.bytes;
    bad.at(i) ^= 0xff;
    // Each attempt uses an unpublished fresh owner, just like Scene::load.
    C::Controller candidate(cfg);
    R::detail::Checkpoint input(bad);
    input.control_anim_cache = true;
    bool rejected = false;
    try {
      R::detail::CheckpointAccess::run(candidate, input);
      input.finish();
    } catch (const std::exception &) {
      rejected = true;
    }
    check(rejected, "corrupt Controller scalar accepted");
  }
}
int main() {
  try {
    for (unsigned f = 0; f < 16; ++f) {
      N::Scene live(T::config(f));
      live.queue_animation(C::AnimationCommandKind::Start, 1);
      live.advance(21);
      live.advance(21);
      live.advance(0);
      check(live.observation().animation_decoded.has_value(),
            "no decoded joints");
      auto before = T::control(live.snapshot());
      for (unsigned i = 0; i < 5; ++i)
        (void)live.observation();
      check(before == T::control(live.snapshot()), "getter mutated state");
      live.queue_animation(C::AnimationCommandKind::Restart, 2, 8);
      auto replay = N::Scene::load(live.save(), "rta");
      equal(live, *replay);
      auto direct = N::Scene::load(live.save_snapshot(), "rta");
      equal(live, *direct);
      for (unsigned k = 0; k < 10; ++k) {
        live.advance(k % 2 ? 5 : 0);
        replay->advance(k % 2 ? 5 : 0);
        direct->advance(k % 2 ? 5 : 0);
        equal(live, *replay);
        equal(live, *direct);
      }
      live.set_paused(true);
      live.queue_animation(C::AnimationCommandKind::Stop, 2);
      auto paused = N::Scene::load(live.save_snapshot(), "rta");
      equal(live, *paused);
      live.advance(31);
      paused->advance(31);
      equal(live, *paused);
      live.set_paused(false);
      paused->set_paused(false);
      for (unsigned k = 0; k < 3; ++k) {
        live.advance(0);
        paused->advance(0);
        equal(live, *paused);
      }
      auto saved = live.save_snapshot();
      for (auto at : {8u, 20u, 56u, unsigned(saved.size() - 1)}) {
        auto bad = saved;
        bad[at] ^= 1;
        refuses([&] { N::Scene::load(bad, "rta"); });
        check(live.save_snapshot() == saved, "invalid load modified owner");
      }
      auto bad = saved;
      bad.pop_back();
      refuses([&] { N::Scene::load(bad, "rta"); });
      live.reset();
      check(live.snapshot().animation_commands.empty(), "reset queue");
      check(live.snapshot().next_animation_command == 1, "reset IDs");
      auto cfg = T::config(f);
      cfg.animation_cache->key_records = 1;
      N::Scene failure(cfg);
      failure.queue_animation(C::AnimationCommandKind::Start, 1);
      failure.advance(0);
      failure.advance(0);
      check(failure.observation().failed, "key budget did not fail");
      check(!failure.snapshot()
                 .session.animation_cache->operations.back()
                 .completed,
            "failed prefix lost");
      auto prefix = T::control(failure.snapshot());
      failure.advance(31);
      check(T::control(failure.snapshot()) == prefix, "failure retried");
      refuses([&] { failure.save(); });
      refuses([&] { failure.save_snapshot(); });
    }
    {
      auto cfg = T::config();
      cfg.animation_cache->calls_per_tick = 2;
      N::Scene bounded(cfg);
      auto before = T::control(bounded.snapshot());
      refuses(
          [&] { bounded.queue_animation(C::AnimationCommandKind::Start, 1); });
      check(before == T::control(bounded.snapshot()),
            "command work preflight mutated state");
      cfg.animation_cache->calls_per_tick = 3;
      N::Scene one(cfg);
      one.queue_animation(C::AnimationCommandKind::Start, 1);
      before = T::control(one.snapshot());
      refuses([&] { one.queue_animation(C::AnimationCommandKind::Stop, 1); });
      check(before == T::control(one.snapshot()),
            "same-frame work preflight mutated state");
      one.advance(0);
      one.advance(0);
      check(!one.observation().failed, "bounded admitted work failed");
    }
    {
      N::Scene bounded(T::config());
      const auto initial_size = bounded.save().size();
      for (std::size_t i = 0; i < N::MAX_ACTIONS; ++i)
        bounded.release_input();
      const auto bytes = bounded.save();
      check(bytes.size() == initial_size + 10 * N::MAX_ACTIONS,
            "v4 journal records must retain their eight-byte input frame");
      refuses([&] { bounded.release_input(); });
      check(bounded.save() == bytes, "journal budget refusal mutated state");
      auto loaded = N::Scene::load(bounded.save_snapshot(), "rta");
      check(loaded->save() == bytes, "maximum v4 journal direct continuation");
    }
    pacing();
    restore_membership();
    auto cfg = T::config();
    cfg.animation_cache->slots = 1;
    refuses([&] { N::Scene bad(cfg); });
    N::Scene scene(T::config());
    auto before = T::control(scene.snapshot());
    refuses([&] { scene.queue_animation(C::AnimationCommandKind::Start, 4); });
    check(before == T::control(scene.snapshot()), "invalid command mutated");
    std::cout << checks << " control_anim_cache checks PASS\n";
  } catch (const std::exception &e) {
    std::cerr << "after " << checks << ": " << e.what() << '\n';
    return 1;
  }
}
