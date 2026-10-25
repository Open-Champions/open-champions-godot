#include "champions/frame_clock.hpp"
#include "session_facing_direct.hpp"
#include <functional>
#include <iostream>
namespace R = champions::simulation::rta;
namespace N = R::native_save;
namespace C = R::session_control;
namespace T = champions::test::facing_trace;
unsigned checks = 0;
std::uint64_t digest = UINT64_C(14695981039346656037);
void observe(const std::string &s) {
  for (unsigned char c : s) {
    digest ^= c;
    digest *= UINT64_C(1099511628211);
  }
}
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
  auto x = T::control(a.snapshot());
  check(x == T::control(b.snapshot()), "complete continuation mismatch");
  observe(x);
}
using champions::test::facing_direct::Fixture;
void direct_checks() {
  Fixture f;
  f.session.set_level_pending(true);
  for (unsigned k = 0; k < 3; ++k)
    f.submit();
  check(f.session.snapshot().turning->operations.empty(), "pending turned");
  f.session.set_level_pending(false);
  for (unsigned k = 0; k < 3; ++k)
    f.submit(true);
  check(f.session.snapshot().turning->operations.empty(), "no_sim turned");
  f.submit();
  check(!f.session.snapshot().turning->operations.empty(),
        "eligible did not turn");
  for (unsigned limit : {1u, 2u, 3u}) {
    Fixture x(limit, true);
    x.submit();
    refuses([&] { x.submit(); });
    auto before = x.session.snapshot();
    observe(champions::test::facing_direct::observe(before));
    check(before.failed && before.turning->writes.size() >= 1,
          "failure prefix lost");
    if (limit == 1)
      check(!before.turning->operations.back().completed,
            "turn failure reported success");
    else
      check(before.turning->operations.back().completed,
            "completed turn lost on later failure");
    refuses([&] { x.submit(); });
    check(x.session.snapshot().turning->writes.size() ==
              before.turning->writes.size(),
          "failed operation retried");
  }
  Fixture reused;
  reused.session.with_world([&](auto &w) {
    w.destroy(*w.objects.at(reused.address), 3);
    auto *o = w.new_object(0x500, {0, 0, 0});
    check(o->addr == reused.address, "reuse setup");
  });
  refuses([&] {
    reused.submit();
    reused.submit();
  });
  check(reused.session.failed(), "reused generation accepted");
}

std::uint32_t word(const N::Bytes &b, std::size_t at) {
  std::uint32_t v = 0;
  for (unsigned k = 0; k < 4; ++k)
    v |= std::uint32_t(b.at(at + k)) << (8 * k);
  return v;
}
void patch(N::Bytes &b, std::size_t at, std::uint32_t v) {
  for (unsigned k = 0; k < 4; ++k)
    b.at(at + k) = std::uint8_t(v >> (8 * k));
}
std::uint32_t crc(const N::Bytes &b, std::size_t at, std::size_t n) {
  std::uint32_t c = ~0u;
  for (std::size_t j = at; j < at + n; ++j) {
    c ^= b.at(j);
    for (unsigned k = 0; k < 8; ++k)
      c = (c >> 1) ^ ((c & 1) ? 0xedb88320u : 0);
  }
  return ~c;
}
void checksum(N::Bytes &b, std::size_t n) {
  patch(b, 20, 0);
  patch(b, 20, crc(b, 0, n));
}
void corrupt_checks() {
  N::Scene live(T::config(0, 0, true));
  live.advance(5);
  live.advance(5);
  auto original = live.save_snapshot();
  auto end = std::size_t(56) + word(original, 24) + word(original, 32) +
             word(original, 40);
  for (unsigned offset : {50u, 9u}) {
    auto bad = original;
    bad.at(end - offset) ^= 2;
    const auto start = std::size_t(56) + word(bad, 24) + word(bad, 32);
    patch(bad, 44, crc(bad, start, word(bad, 40)));
    checksum(bad, 56);
    refuses([&] { N::Scene::load(bad, "rta"); });
    check(live.save_snapshot() == original,
          "corrupt history changed live scene");
  }
  auto bad = live.save();
  bad.at(40) = 0;
  checksum(bad, bad.size());
  refuses([&] { N::Scene::load(bad, "rta"); });
  check(live.save_snapshot() == original,
        "unsupported descriptor changed live scene");
}
int main() {
  try {
    direct_checks();
    corrupt_checks();
    for (unsigned f = 0; f < 16; ++f)
      for (bool recovery : {false, true}) {
        auto c = T::config(f, 6, recovery);
        N::Scene a(c);
        check(a.snapshot().session.turning->operations.empty(), "setup turned");
        a.advance(4);
        check(a.snapshot().session.turning->operations.empty(),
              "nonready turned");
        a.advance(2);
        check(a.observation().angular_velocity != 0, "spring velocity missing");
        a.advance(2);
        a.advance(16);
        a.set_paused(true);
        for (bool direct : {false, true}) {
          auto bytes = direct ? a.save_snapshot() : a.save();
          check(bytes[8] == (direct ? 5 : 3), "version");
          auto b = N::Scene::load(bytes, "rta");
          equal(a, *b);
          a.set_paused(false);
          b->set_paused(false);
          for (unsigned mask : {8u, 0u, 5u, 10u, 0u}) {
            a.advance(mask);
            b->advance(mask);
            equal(a, *b);
          }
          a.set_paused(true);
          bytes[8] = 99;
          refuses([&] { N::Scene::load(bytes, "rta"); });
        }
        a.reset();
        check(a.observation().angular_velocity == 0xfff0,
              "reset initial velocity");
        a.advance(4);
        a.release_input();
        a.advance(0);
        check(a.snapshot().session.turning->operations.empty(),
              "release turned stale input");
        a.stop();
        auto before = T::control(a.snapshot());
        a.advance(4);
        check(before == T::control(a.snapshot()), "stop mutated");
      }
    for (unsigned f = 0; f < 16; ++f)
      for (bool recovery : {false, true}) {
        N::Scene a(T::config(f, 0, recovery));
        for (unsigned k = 0; k < 257; ++k)
          check(!a.advance(21).failed, "budget failure");
        check(a.observation().ready_ticks == 256 && a.observation().stopped,
              "budget cap");
        auto b = N::Scene::load(a.save_snapshot(), "rta");
        equal(a, *b);
      }
    auto c = T::config(0, 0, false);
    c.movement = C::MovementPolicy::Isolated;
    refuses([&] { N::Scene a(c); });
    c = T::config(0, 0, false);
    c.max_ready_ticks = 257;
    refuses([&] { N::Scene a(c); });
    c = T::config(0, 0, false);
    c.facing->mode = 2;
    refuses([&] { N::Scene a(c); });
    c = T::config(0, 0, false);
    c.facing->policy = C::FacingPolicy::Disabled;
    refuses([&] { N::Scene a(c); });
    std::string reference;
    for (unsigned f = 0; f < 16; ++f) {
      reference.clear();
      std::vector<std::string> tick_states;
      for (unsigned fps : {30u, 60u, 144u, 240u, 0u}) {
        N::Scene a(T::config(f, 0, true));
        champions::presentation::rta::FrameClock clock;
        unsigned ticks = 0;
        std::uint64_t previous = 0;
        for (unsigned frame = 1; ticks < 30; ++frame) {
          auto now = fps ? std::uint64_t(frame) * 1000000 / fps
                         : previous + (frame % 3 == 0   ? 0
                                       : frame % 3 == 1 ? 2700
                                                        : 19400);
          clock.advance(now - previous, [&] {
            a.advance(ticks++ < 15 ? 5 : 10);
            const auto state = T::control(a.snapshot());
            if (fps == 30)
              tick_states.push_back(state);
            else
              check(state == tick_states.at(ticks - 1),
                    "render pace changed tick-indexed complete state");
            observe(state);
            return ticks < 30;
          });
          previous = now;
        }
        auto s = T::control(a.snapshot());
        observe(s);
        if (reference.empty())
          reference = s;
        check(s == reference, "render pace changed complete state");
      }
    }
    std::cout << checks << " facing checks PASS; complete FNV1a64=" << std::hex
              << digest << "\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
