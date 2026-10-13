// Authored lifetime, refusal and failure-prefix checks; original Python parity
// is independently supplied by the animation-runtime trace corpus.
#include "champions/animation_runtime.hpp"
#include <iostream>
namespace R = champions::simulation::rta;
namespace A = R::animation_runtime;
namespace {
unsigned checks = 0, failures = 0;
void check(bool value, const char *why) {
  ++checks;
  if (!value) {
    ++failures;
    std::cerr << "FAIL: " << why << '\n';
  }
}
template <class E, class F> bool refuses(F fn) {
  try {
    fn();
  } catch (const E &) {
    return true;
  } catch (...) {
  }
  return false;
}
std::vector<std::uint8_t> bytes() {
  std::vector<std::uint8_t> out(0x80, 0);
  auto word = [&](std::size_t at, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i)
      out[at + i] = std::uint8_t(value >> (8 * i));
  };
  word(0, 1);
  word(4, 20);
  word(8, 0x80);
  word(12, 0x78);
  word(16, 0x70);
  std::vector<unsigned> bits;
  auto put = [&](std::uint32_t value, unsigned n) {
    for (unsigned i = n; i > 0; --i)
      bits.push_back((value >> (i - 1)) & 1);
  };
  put(7, 4);
  put(64, 8);
  put(0, 8);
  put(0, 8);
  put(13, 4);
  put(4096, 14);
  put(0, 14);
  put(0, 14);
  put(0, 14);
  put(255, 8);
  while (bits.size() % 32)
    bits.push_back(0);
  for (std::size_t i = 0; i < bits.size(); i += 16) {
    std::uint16_t value = 0;
    for (unsigned j = 0; j < 16; ++j)
      value = std::uint16_t((value << 1) | bits[i + j]);
    out.push_back(std::uint8_t(value));
    out.push_back(std::uint8_t(value >> 8));
  }
  return out;
}
void identity_and_clear() {
  A::Runtime r(2);
  auto a = r.add_clip(bytes()), b = r.add_clip(bytes());
  check(a != b, "equal clip bytes retain distinct identity");
  auto s = r.add_state(3);
  auto id = r.anim_push(s, a, 0);
  auto slot = r.animtrack_decode(id);
  auto saved = r.slot(slot);
  r.anim_clear(s);
  check(r.state(s).count == 0 && r.track(id).slot == slot &&
            r.slot(slot).owner == id,
        "clear retains stale cached cell owner");
  check(r.anim_push(s, b, 0) == id, "reuse preserves fixed cell identity");
  check(r.track(id).slot == 0 && r.slot(slot).owner == 0,
        "new track releases stale slot");
  check(r.slot(slot).clip == saved.clip &&
            r.slot(slot).cursor == saved.cursor &&
            r.slot(slot).joints[0].pos == saved.joints[0].pos,
        "release retains decoder payload");
  auto empty = r.add_state(1);
  auto seed = r.cosmetic_seed();
  r.anim_randomize_top_time(empty);
  check(r.cosmetic_seed() == seed, "empty randomize consumes no cosmetic draw");
}
void copy_and_destructor() {
  A::Runtime r(3);
  auto clip = r.add_clip(bytes());
  auto s = r.add_state(3);
  auto id = r.anim_push(s, clip, 0);
  auto slot = r.animtrack_decode(id);
  auto stale = r.state(s).tracks[2];
  r.track(stale).weight = 0xdeadbeefu;
  r.track(stale).clip = clip;
  auto copy = r.anim_state_copy(s);
  auto dst = r.state(copy).tracks;
  check(copy != s && dst[0] != id, "copy creates new state and fixed cells");
  check(r.track(dst[0]).clip == clip && r.track(dst[0]).slot == 0,
        "nonprotected copy shares clip but drops cache");
  check(r.track(dst[2]).weight == 0xdeadbeefu && r.track(dst[2]).clip == clip,
        "copy retains inactive capacity fields");
  check(r.track(id).slot == slot && r.slot(slot).owner == id,
        "copy leaves source cache intact");
  r.anim_clear(s);
  r.anim_release_caches(s);
  check(r.track(id).slot == slot, "release skips inactive cell");
  auto old_cells = r.state(s).tracks;
  r.anim_state_dtor(s);
  check(r.state(s).tracks == old_cells && r.track(id).slot == 0 &&
            r.slot(slot).owner == 0,
        "destructor releases capacity but retains identities");
  check(r.track(stale).weight == 0xdeadbeefu,
        "destructor preserves stale scalar fields");
  check(r.anim_push(s, clip, 0) == id,
        "released state can reuse same owned cell");
}
void owner_transfers() {
  A::Runtime r(3);
  auto a = r.add_clip(bytes()), b = r.add_clip(bytes()),
       c = r.add_clip(bytes());
  auto s = r.add_state(3);
  auto x = r.anim_push(s, a, 0), y = r.anim_push(s, b, 0),
       z = r.anim_push(s, c, A::PERSISTENT);
  r.animtrack_decode(x);
  r.animtrack_decode(y);
  auto slot = r.animtrack_decode(z);
  r.anim_drop_non_persistent(s);
  check(r.state(s).count == 1 && r.track(x).clip == c &&
            r.track(x).slot == slot,
        "compact moves kept values into fixed destination");
  check(r.slot(slot).owner == x && r.track(z).slot == 0,
        "compact repairs moved owner and clears source link");
  check(r.track(z).clip == c, "compact retains vacated scalar payload");
  auto t = r.add_state(3);
  auto i = r.anim_push(t, a, 0), j = r.anim_push(t, b, 0),
       k = r.anim_push(t, c, 0);
  r.animtrack_decode(i);
  r.animtrack_decode(j);
  r.animtrack_decode(k);
  r.anim_stop(R::ZERO, t, b);
  check(r.state(t).count == 2 && r.track(j).clip == c,
        "zero stop shifts full track values");
  check(r.track(i).slot == 0 && r.track(j).slot == 0 && r.track(k).slot == 0,
        "zero stop releases whole active state before shifts");
}
void null_active_cells() {
  A::Runtime r(2);
  auto s = r.add_state(2);
  auto id = r.state(s).tracks[0];
  r.set_state_scalars(s, 0, 1, 0);
  r.anim_release_caches(s);
  check(r.state(s).count == 1 && r.track(id).clip == 0,
        "release admits active null clip without reading it");
  auto copy = r.anim_state_copy(s);
  check(r.state(copy).count == 1 && r.track(r.state(copy).tracks[0]).clip == 0,
        "copy preserves active null clip without dereferencing");
  r.anim_clear(s);
  check(r.state(s).count == 0, "clear admits active null clip");
  r.anim_state_dtor(copy);
  check(r.state(copy).count == 1,
        "destructor release retains null-cell identity and count");
}
void boundaries_and_prefixes() {
  A::Limits l;
  l.state_cells = 2;
  A::Runtime limited(2, l);
  check(refuses<A::ResourceLimit>([&] { limited.add_state(3); }) &&
            limited.snapshot().decoder.tracks.empty(),
        "state cell cap refuses before any allocation");
  check(refuses<A::ValueError>([&] { limited.add_state(0); }),
        "zero capacity refuses");
  check(refuses<A::ValueError>([&] { limited.add_state(128); }),
        "capacity must fit positive signed byte");
  A::Runtime r(2);
  auto clip = r.add_clip(bytes());
  auto s = r.add_state(1);
  auto id = r.anim_push(s, clip, 0);
  r.set_state_scalars(s, 9, 1, 0x80000000u);
  check(refuses<A::Unsupported>([&] { r.anim_clear(s); }) &&
            r.state(s).count == 1 && r.state(s).pose == 9,
        "pose refuses before lifecycle stores");
  r.set_state_scalars(s, 0, 1, 0x80000000u);
  r.track(id).flags = A::ROOT;
  check(refuses<A::Unsupported>([&] { r.anim_clear(s); }) &&
            r.state(s).count == 1,
        "protected global track refuses before mutation");
  r.track(id).flags = 0;
  check(refuses<A::Unsupported>([&] { r.anim_advance(R::ZERO, s, true); }) &&
            r.state(s).count == 1,
        "sound execution refused");
  check(
      refuses<A::Unsupported>([&] { r.anim_advance(R::ZERO, s, false, true); }),
      "root output refused");
  check(refuses<A::Unsupported>(
            [&] { r.anim_advance(R::ZERO, s, false, false, true); }),
        "yaw output refused");
  A::Limits capped;
  capped.max_loop_subtractions = 1;
  A::Runtime wrap(2, capped);
  clip = wrap.add_clip(bytes());
  wrap.clip(clip).length = 1;
  s = wrap.add_state(1);
  id = wrap.anim_push(s, clip, A::LOOP);
  wrap.track(id).time = 0x40800000u;
  wrap.set_state_scalars(s, 0, 1, 0x80000000u);
  check(refuses<A::ResourceLimit>([&] { wrap.anim_advance(R::ZERO, s); }),
        "aggregate loop cap refuses");
  check(wrap.track(id).time == 0x40400000u,
        "loop refusal preserves each completed subtraction store");
  check(wrap.state(s).flags == 0x80000000u,
        "loop refusal precedes final state flag publication");
}
void aggregate_budget_and_prefixes() {
  A::Limits limits;
  limits.max_loop_subtractions = 1;
  A::Runtime r(2, limits);
  auto clip = r.add_clip(bytes());
  r.clip(clip).length = 1;
  auto s = r.add_state(2);
  auto lower = r.anim_push(s, clip, A::LOOP | A::PAUSED);
  auto upper = r.anim_push(s, clip, A::LOOP | A::PAUSED | A::FIXED_STEP);
  r.track(lower).time = r.track(upper).time = R::ONE;
  r.track(upper).rate = R::ONE;
  auto cached = r.animtrack_decode(upper);
  r.set_state_scalars(s, 0, 2, 0x80000000u);
  check(refuses<A::ResourceLimit>([&] { r.anim_advance(R::ZERO, s); }),
        "loop budget is shared across tracks in descending order");
  check(r.track(upper).time == R::ZERO &&
            r.track(upper).weight == R::TWO && r.track(lower).time == R::ONE,
        "later budget failure retains completed upper timing and weight stores");
  check(r.track(upper).slot == cached && r.slot(cached).owner == upper &&
            r.state(s).count == 2 && r.state(s).flags == 0x80000000u,
        "budget failure preserves caches and skips final flag publication");
  check(r.anim_advance(R::ZERO, s) == 0 && r.track(lower).time == R::ZERO,
        "next advance receives a fresh aggregate subtraction budget");

  A::Runtime null_clip(2);
  auto n = null_clip.add_state(1);
  null_clip.set_state_scalars(n, 0, 1, 0);
  null_clip.set_cosmetic_seed(1);
  check(refuses<A::HostFault>([&] { null_clip.anim_randomize_top_time(n); }) &&
            null_clip.cosmetic_seed() == 0x3c88596cu &&
            null_clip.track(null_clip.state(n).tracks[0]).time == R::ZERO,
        "null-clip randomize retains seed store before length failure");
}
void event_boundaries_and_copy() {
  A::Runtime r(2);
  auto clip = r.add_clip(bytes());
  auto s = r.add_state(1);
  auto id = r.anim_push(s, clip, A::PAUSED | A::FIXED_STEP);
  auto copy = r.anim_state_copy(s);
  const auto high = R::mul(R::mul(R::ONE, 0x3f7fffffu), 0x3c888888u);
  r.clip(clip).events =
      {{{R::ZERO, 128}, {high, 1}, {0x3c000000u, 2}, {R::ONE, 4}}};
  check(r.anim_advance(R::ONE, copy) == -126,
        "copy shares mutable events; paused fixed track includes low, excludes high");
  check(r.track(r.state(copy).tracks[0]).time == R::ZERO &&
            r.track(id).time == R::ZERO,
        "paused event delivery leaves both canonical times unchanged");
  r.track(id).flags = A::FIXED_STEP;
  check(r.anim_advance(R::ZERO, s) == 0 && r.track(id).time == 0x3f7fffffu,
        "event interval uses speed even when fixed-step time advances");
}
} // namespace
int main() {
  identity_and_clear();
  copy_and_destructor();
  owner_transfers();
  null_active_cells();
  boundaries_and_prefixes();
  aggregate_budget_and_prefixes();
  event_boundaries_and_copy();
  std::cout << "animation-runtime API: " << checks << " checks, " << failures
            << " failures\n";
  return failures ? 1 : 0;
}
