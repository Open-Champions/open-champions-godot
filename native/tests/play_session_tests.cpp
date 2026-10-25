#include "champions/detail/checkpoint.hpp"
#include "champions/native_save.hpp"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <new>
#include <numeric>
#include <stdexcept>
// One-shot allocation failure injection, disabled outside explicit test scopes.
static long fail_after = -1;
void *operator new(std::size_t n) {
  if (fail_after == 0) {
    fail_after = -1;
    throw std::bad_alloc();
  }
  if (fail_after > 0)
    --fail_after;
  if (auto p = std::malloc(n ? n : 1))
    return p;
  throw std::bad_alloc();
}
void operator delete(void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }
namespace R = champions::simulation::rta;
namespace C = R::session_control;
namespace N = R::native_save;
namespace D = R::detail;
namespace {
unsigned checks = 0;
void check(bool ok, const char *why) {
  ++checks;
  if (!ok)
    throw std::runtime_error(why);
}
template <class F> void refuses(F f) {
  bool caught = false;
  try {
    f();
  } catch (const std::exception &) {
    caught = true;
  }
  check(caught, "expected refusal");
}
C::Config config(unsigned mode = 0, unsigned window = 2) {
  C::Config c;
  c.movement = C::MovementPolicy::DryCreature;
  c.max_ready_ticks = 512;
  c.play_window = window;
  c.model = R::Fpu::Model(mode / 4);
  c.mul_bias = mode & 2;
  c.div_nearest = mode & 1;
  return c;
}
void continuation() {
  for (unsigned mode = 0; mode < 16; ++mode) {
    auto c = config(mode);
    auto scene = std::make_unique<N::Scene>(c);
    refuses([&] { scene->save(); });
    refuses([&] { scene->snapshot(); });
    for (unsigned tick = 0; tick < 4; ++tick) {
      const auto o = scene->advance((tick % 2 ? C::NEG_X : C::POS_X) |
                                    (tick % 3 == 0 ? C::DAMAGE : 0));
      check(!o.failed, "play advance failed");
      const auto saved = scene->save_continuation();
      auto next = N::Scene::load(saved, "rta");
      check(next->save_continuation() == saved, "continuation bytes");
      scene->advance(tick % 2 ? 16 : 1);
      next->advance(tick % 2 ? 16 : 1);
      check(next->save_continuation() == scene->save_continuation(),
            "same future state including all window histories");
      scene = std::move(next);
    }
    scene->set_paused(true);
    auto saved = scene->save_continuation();
    scene->advance(31);
    check(scene->save_continuation() == saved,
          "pause does not submit or retire");
    scene = N::Scene::load(saved, "rta");
    scene->set_paused(false);
    scene->release_input();
    scene->advance(0);
    check(!scene->observation().failed, "released resumed play");
    auto bad = saved;
    bad.back() ^= 1;
    refuses([&] { N::Scene::load(bad, "rta"); });
    refuses([&] { N::Scene::load(saved, "norrath"); });
    scene->reset();
    check(scene->observation().ready_ticks == 0 &&
              scene->observation().history.retirements == 0,
          "explicit reset");
  }
  auto c = config();
  c.recovery = R::session_recovery::Initial{};
  refuses([&] { N::Scene s(c); });
  c = config();
  c.play_window = 129;
  refuses([&] { N::Scene s(c); });
}
void failures() {
  N::Scene s(config());
  s.advance(17);
  s.advance(0);
  s.advance(16);
  const auto before = s.observation();
  const auto bytes = s.save_continuation();
  // Fails inside retirement's validation, before any log is cleared.
  fail_after = 0;
  const auto failed = s.advance(0);
  fail_after = -1;
  check(failed.failed && failed.stopped, "allocation failure latches stop");
  check(failed.ready_ticks == before.ready_ticks &&
            failed.history.through_ready == before.history.through_ready &&
            failed.retained.records == before.retained.records,
        "retirement allocation failure retains complete reportable prefix");
  check(s.advance(31).ready_ticks == before.ready_ticks,
        "failed operation never automatically retries");
  refuses([&] { s.save_continuation(); });
  s.reset();
  check(!s.observation().failed, "explicit failure reset");
  for (long point : {0L, 1L, 5L, 20L}) {
    auto original = N::Scene::load(bytes, "rta");
    fail_after = point;
    bool caught = false;
    try {
      auto candidate = N::Scene::load(bytes, "rta");
    } catch (const std::exception &) {
      caught = true;
    }
    fail_after = -1;
    check(caught, "forced load allocation failure");
    check(original->save_continuation() == bytes,
          "load allocation failure leaves running owner unchanged");
  }
  auto refresh = [](N::Bytes &b) {
    std::uint32_t crc = 0xffffffffu;
    for (std::size_t i = 0; i < b.size(); ++i) {
      crc ^= i >= 20 && i < 24 ? 0 : b[i];
      for (unsigned bit = 0; bit < 8; ++bit)
        crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    crc = ~crc;
    for (unsigned i = 0; i < 4; ++i)
      b[20 + i] = std::uint8_t(crc >> (i * 8));
  };
  for (unsigned offset : {8u, 12u, 24u}) {
    auto bad = bytes;
    bad[offset] = 255;
    refresh(bad);
    refuses([&] { N::Scene::load(bad, "rta"); });
  }
  auto word = [](const N::Bytes &b, std::size_t p) {
    std::uint32_t v = 0;
    for (unsigned i = 0; i < 4; ++i)
      v |= std::uint32_t(b[p + i]) << (8 * i);
    return v;
  };
  const auto current = std::size_t(36) + word(bytes, 28);
  for (auto offset : {current, current + 8, current + 16}) {
    auto bad = bytes;
    for (unsigned i = 0; i < 8; ++i)
      bad[offset + i] = 255;
    refresh(bad);
    refuses([&] { N::Scene::load(bad, "rta"); });
  }
  auto truncated = bytes;
  truncated.pop_back();
  refuses([&] { N::Scene::load(truncated, "rta"); });
}
void long_run(unsigned ticks) {
  N::Scene s(config(0, 128));
  const auto begin = std::chrono::steady_clock::now();
  R::play_session::Counts max_records{}, max_capacity{};
  for (unsigned i = 0; i <= ticks; ++i) {
    auto o = s.advance(((i / 32) % 2 ? C::NEG_X : C::POS_X) |
                       (i % 2 ? 0 : C::DAMAGE));
    check(!o.failed && !o.stopped, "play workload stopped");
    for (unsigned j = 0; j < max_records.size(); ++j) {
      max_records[j] = std::max(max_records[j], o.retained.records[j]);
      max_capacity[j] = std::max(max_capacity[j], o.retained.capacity[j]);
    }
    if (i == 128 || i == 256 || i == 512 || i == 2001 || i == ticks) {
      const auto view = s.continuation_snapshot();
      const auto workload_us =
          std::chrono::duration_cast<std::chrono::microseconds>(
              std::chrono::steady_clock::now() - begin)
              .count();
      N::Bytes b;
      if (i == 512 || i == 2001 || i == ticks) {
        b = s.save_continuation();
        auto reloaded = N::Scene::load(b, "rta");
        check(reloaded->save_continuation() == b, "late save/load canonical");
      }

      std::cout << "ready=" << o.ready_ticks
                << " retired_through=" << o.history.through_ready
                << " retained="
                << std::accumulate(o.retained.records.begin(),
                                   o.retained.records.end(), std::uint64_t{})
                << " max_records="
                << std::accumulate(max_records.begin(), max_records.end(),
                                   std::uint64_t{})
                << " max_capacity="
                << std::accumulate(max_capacity.begin(), max_capacity.end(),
                                   std::uint64_t{})
                << " memory_bytes="
                << view.session.hp->adapter.model.backing.size()
                << " world_links=" << view.session.world->links.size()
                << " iterator_serial=" << view.session.world->iterator_serial
                << " workload_elapsed_us=" << workload_us
                << " save_bytes=" << b.size() << " elapsed_us="
                << std::chrono::duration_cast<std::chrono::microseconds>(
                       std::chrono::steady_clock::now() - begin)
                       .count()
                << '\n';
    }
  }
  check(s.observation().ready_ticks == ticks, "absolute ready count");
  if (ticks >= 2001) {
    s.set_paused(true);
    auto resumed = N::Scene::load(s.save_continuation(), "rta");
    resumed->advance(31);
    check(resumed->save_continuation() == s.save_continuation(),
          "late paused load preserves pending state");
    resumed->set_paused(false);
    s.set_paused(false);
    for (auto command : {1u, 17u, 0u, 16u}) {
      resumed->advance(command);
      s.advance(command);
      check(resumed->save_continuation() == s.save_continuation(),
            "late continuation preserves all future state and RNG authority");
    }
  }

  check(std::accumulate(max_records.begin(), max_records.end(),
                        std::uint64_t{}) <= 128 * 96,
        "bounded records");
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc == 2 && std::string(argv[1]) == "--soak")
      long_run(36000);
    else if (argc == 2 && std::string(argv[1]) == "--measure")
      long_run(512);
    else if (argc == 2 && std::string(argv[1]) == "--failures")
      failures();
    else {
      continuation();
      failures();
      long_run(2100);
    }
    std::cout << "play-session: " << checks << " checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
