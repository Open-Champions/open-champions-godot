#include "champions/animation_clock.hpp"
#include <iostream>

using namespace champions::simulation::rta;
namespace A = champions::simulation::rta::animation_clock;
namespace {
int checks = 0, failures = 0;
void check(bool ok, const char *name) {
  ++checks;
  if (!ok) {
    ++failures;
    std::cerr << "FAIL: " << name << '\n';
  }
}
template <class E, class F> bool refuses(F f) {
  try {
    f();
  } catch (const E &) {
    return true;
  } catch (...) {
  }
  return false;
}
bool same(const A::AnimState &a, const A::AnimState &b) {
  if (a.pose != b.pose || a.count != b.count || a.capacity != b.capacity ||
      a.flags != b.flags || a.tracks.size() != b.tracks.size())
    return false;
  for (std::size_t i = 0; i < a.tracks.size(); ++i) {
    const auto &x = a.tracks[i], &y = b.tracks[i];
    if (x.clip != y.clip || x.slot != y.slot || x.weight != y.weight ||
        x.rate != y.rate || x.life != y.life || x.life_rate != y.life_rate ||
        x.time != y.time || x.flags != y.flags || x.sound_idx != y.sound_idx)
      return false;
  }
  return true;
}
} // namespace
int main() {
  set_fpu(Fpu::Model::Chop);
  for (int capacity : {1, 127}) {
    A::AnimState s(capacity);
    check(s.tracks.size() == std::size_t(capacity),
          "capacity endpoints own every cell");
  }
  for (int capacity : {0, -1, 128})
    check(refuses<A::ValueError>([&] { A::AnimState s(capacity); }),
          "invalid capacity refuses");
  auto a = std::make_shared<A::Clip>(), b = std::make_shared<A::Clip>();
  a->length = b->length = 4;
  A::AnimState s(3);
  A::anim_push(s, a, A::LOOP);
  A::anim_push(s, b, A::LOOP);
  A::anim_push(s, a, A::LOOP);
  A::anim_stop(ZERO, s, s.tracks[0].clip);
  check(s.count == 1 && s.tracks[0].clip == b,
        "stop retains aliased target while cells shift");
  check(s.tracks[1].clip == a && s.tracks[2].clip == a,
        "removal retains stale cells");
  auto before = s;
  s.tracks[2].slot = 7;
  auto blocked = s;
  check(refuses<A::Unsupported>([&] { A::anim_clear(s); }) && same(s, blocked),
        "inactive cache token refuses without mutation");
  s = before;
  check(refuses<A::Unsupported>([&] { A::anim_push(s, a, A::ROOT); }) &&
            same(s, before),
        "root service refusal is atomic");
  check(refuses<A::Unsupported>([&] { A::anim_push(s, a, A::YAW); }) &&
            same(s, before),
        "yaw service refusal is atomic");
  A::AnimSystem system;
  check(
      refuses<A::Unsupported>([&] { A::anim_advance(system, ONE, s, true); }) &&
          same(s, before),
      "sound execution refuses before state change");
  s.pose = 9;
  blocked = s;
  check(refuses<A::Unsupported>([&] { A::anim_release_caches(s); }) &&
            same(s, blocked),
        "pose cache refusal is atomic");
  A::AnimState loop(1);
  A::anim_push(loop, a, A::LOOP);
  loop.tracks[0].time = 0x41000000u; // 8 ticks
  loop.flags = A::TALK;
  system.limits.max_loop_subtractions = 0;
  check(
      refuses<A::ResourceLimit>([&] { A::anim_advance(system, ZERO, loop); }) &&
          loop.tracks[0].time == 0x41000000u && loop.flags == A::TALK,
      "zero budget preserves the original pre-wrap store and old flags");
  system.limits.max_loop_subtractions = 1;
  check(
      refuses<A::ResourceLimit>([&] { A::anim_advance(system, ZERO, loop); }) &&
          loop.tracks[0].time == 0x40800000u && loop.flags == A::TALK,
      "cap retains one completed original subtraction store");
  system.limits.max_loop_subtractions = 2;
  check(A::anim_advance(system, ZERO, loop) == 0 && loop.tracks[0].time == ZERO,
        "enough budget completes the original loop");
  A::AnimState empty;
  system.cosmetic_seed = 0xdeadbeefu;
  A::anim_randomize_top_time(system, empty);
  check(system.cosmetic_seed == 0xdeadbeefu,
        "empty state consumes no cosmetic draw");
  std::weak_ptr<A::Clip> weak;
  {
    A::AnimState owner(1);
    auto clip = std::make_shared<A::Clip>();
    weak = clip;
    A::anim_push(owner, clip, 0);
    clip.reset();
    A::anim_clear(owner);
    check(!weak.expired(), "inactive cell retains clip lifetime");
  }
  check(weak.expired(), "state destruction releases final clip owner");
  std::cout << "Animation clock API checks: " << checks
            << "; failures: " << failures << '\n';
  return failures ? 1 : 0;
}
