// Authored safety and lifetime regressions; original numerical parity is
// separate.
#include "animation_decoder_trace.hpp"
#include <iostream>
#include <limits>
namespace R = champions::simulation::rta;
namespace A = R::animation_decoder;
namespace T = champions::test::animation_decoder_trace;
namespace {
unsigned checks = 0, failures = 0;
void check(bool ok, const char *why) {
  ++checks;
  if (!ok) {
    ++failures;
    std::cerr << "FAIL: " << why << '\n';
  }
}
template <class E, class F> bool refuses(F &&fn) {
  try {
    fn();
  } catch (const E &) {
    return true;
  } catch (...) {
  }
  return false;
}
std::string observed(const A::Context &c) { return T::snapshot(c.snapshot()); }
std::vector<std::uint8_t> bytes() {
  std::vector<std::uint8_t> out(0x80, 0);
  auto word = [&](std::size_t at, std::uint32_t v) {
    for (unsigned i = 0; i < 4; ++i)
      out[at + i] = std::uint8_t(v >> (8 * i));
  };
  word(0, 1);
  word(4, 20);
  word(8, 0x80);
  word(12, 0x78);
  word(16, 0x70);
  std::vector<unsigned> bits;
  auto put = [&](std::uint32_t v, unsigned n) {
    for (unsigned i = n; i > 0; --i)
      bits.push_back((v >> (i - 1)) & 1);
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
    std::uint16_t v = 0;
    for (unsigned j = 0; j < 16; ++j)
      v = std::uint16_t((v << 1) | bits[i + j]);
    out.push_back(std::uint8_t(v));
    out.push_back(std::uint8_t(v >> 8));
  }
  return out;
}
void admission() {
  check(refuses<A::ValueError>([] { A::Context c(1); }),
        "one-slot pool refused");
  A::Limits l;
  l.slots = 2;
  check(refuses<A::ResourceLimit>([&] { A::Context c(3, l); }),
        "slot limit before allocation");
  l.input_bytes = 1;
  check(refuses<A::ResourceLimit>([&] { A::parse_clip(bytes(), {}, l); }),
        "clip byte budget");
  l = {};
  l.owned_bytes = 1;
  check(refuses<A::ResourceLimit>([&] { A::parse_clip(bytes(), {}, l); }),
        "aggregate clip storage budget");
  check(refuses<A::ValueError>([] { A::parse_clip({}); }),
        "short header refuses");
  auto bad = bytes();
  bad[0] = 65;
  check(refuses<A::HostFault>([&] { A::parse_clip(bad); }),
        "invalid parsed count original fault");
  A::Context c(2);
  auto before = observed(c);
  check(refuses<A::ValueError>(
            [&] { c.add_clip(std::vector<std::uint8_t>{}); }) &&
            observed(c) == before,
        "failed clip publication atomic");
  l = {};
  l.clips = 1;
  A::Context single(2, l);
  single.add_clip(bytes());
  before = observed(single);
  check(refuses<A::ResourceLimit>([&] { single.add_clip(bytes()); }) &&
            observed(single) == before,
        "clip identity budget atomic");
  l = {};
  l.tracks = 1;
  A::Context tracks(2, l);
  auto clip = tracks.add_clip(bytes());
  tracks.add_track();
  before = observed(tracks);
  check(refuses<A::ResourceLimit>([&] { tracks.decode_clip(clip, R::ZERO); }) &&
            observed(tracks) == before,
        "transient identity budget atomic");
  check(refuses<A::Unsupported>([&] { c.clip(0); }),
        "null foreign clip access");
  check(refuses<A::Unsupported>([&] { c.track(1); }), "foreign track access");
  before = observed(c);
  check(refuses<A::Unsupported>([&] { c.touch(3); }) && observed(c) == before,
        "foreign slot access");
}
void graph_and_prefix() {
  A::Context c(2);
  auto a = c.add_clip(bytes());
  A::Track t;
  t.clip = a;
  auto id = c.add_track(t);
  c.order()[0] = c.order()[1];
  auto before = observed(c);
  check(refuses<A::Unsupported>([&] { c.animtrack_decode(id); }) &&
            observed(c) == before,
        "invalid permutation refused before writes");
  c.order() = {2, 1};
  c.track(id).clip = 77;
  before = observed(c);
  check(refuses<A::Unsupported>([&] { c.alloc(); }) && observed(c) == before,
        "foreign graph identity refuses atomically");
  c.track(id).clip = a;
  c.slot(1).index = 99;
  before = observed(c);
  check(refuses<A::Unsupported>([&] { c.touch(2); }) && observed(c) == before,
        "foreign model slot index refuses");
  A::Limits l;
  l.key_records = 0;
  A::Context bounded(2, l);
  a = bounded.add_clip(bytes());
  t = {};
  t.clip = a;
  id = bounded.add_track(t);
  check(refuses<A::ResourceLimit>([&] { bounded.animtrack_decode(id); }),
        "zero work refuses at next delta read");
  auto s = bounded.snapshot();
  auto selected = bounded.track(id).slot;
  check(selected && bounded.slot(selected).owner == id &&
            bounded.slot(selected).tick == 0 &&
            bounded.slot(selected).clip == a,
        "work failure retains allocation and initialization");
  check(bounded.slot(selected).joints[0].pos[0] == R::ONE,
        "work failure retains original position store");
  check(bounded.slot(selected).cursor > 0,
        "work failure retains published initialization cursor");
  before = observed(bounded);
  check(refuses<A::IndexError>(
            [&] { bounded.joint_pos(selected, 64, R::ZERO); }) &&
            observed(bounded) == before,
        "joint output bound atomic");
  check(bounded.joint_pos(selected, -64, R::ZERO) ==
            bounded.joint_pos(selected, 0, R::ZERO),
        "negative index contract");
  A::Context empty(2);
  A::Track no_clip;
  id = empty.add_track(no_clip);
  check(refuses<A::Unsupported>([&] { empty.animtrack_decode(id); }) &&
            empty.track(id).slot &&
            empty.slot(empty.track(id).slot).owner == id,
        "null restart refusal retains executed allocator prefix");
}
void lifetime_and_fpu(unsigned config) {
  R::set_fpu(static_cast<R::Fpu::Model>(config / 4), (config & 2) != 0,
             (config & 1) != 0);
  A::Context c(2);
  c.sim_frame = 123;
  c.cosmetic_seed = 0xdeadbeef;
  auto a = c.add_clip(bytes(), "authored");
  auto b = c.add_clip(bytes(), "authored");
  check(a != b, "equal clips retain distinct identities");
  auto *retained = &c.clip(a);
  auto snap = c.snapshot();
  c.clip(a).events[0] = {R::ONE, 128};
  check(snap.clips[0].value.events[0].mask == 0 &&
            c.clip(b).events[0].mask == 0,
        "snapshots deep copy and distinct metadata");
  auto result = c.decode_clip(a, R::ZERO);
  auto state = c.snapshot();
  check(result.size() == 1 && result[0].position[0] == R::ONE &&
            result[0].quaternion[3] == R::ONE,
        "independent literal raw sample");
  check(state.tracks.size() == 1 &&
            state.slots[state.tracks[0].value.slot - 1].owner ==
                state.tracks[0].id,
        "temporary track has stable retained owner");
  auto *track = &c.track(state.tracks[0].id);
  for (unsigned i = 0; i < 64; ++i) {
    c.add_clip(bytes());
    c.add_track();
  }
  check(&c.clip(a) == retained && &c.track(state.tracks[0].id) == track,
        "registry growth preserves live references");
  auto retained_snapshot = c.snapshot();
  c.slot(track->slot).joints[63].pos[2] = 0xabcdef01;
  check(retained_snapshot.slots[track->slot - 1].joints[63].pos[2] == 0,
        "snapshots retain every stale joint by value");
  check(c.sim_frame == 123 && c.cosmetic_seed == 0xdeadbeef,
        "decoder leaves frame and cosmetic state unchanged");
  A::Context protected_pool(2);
  A::Track p;
  p.flags = A::ROOT;
  auto p1 = protected_pool.add_track(p);
  auto p2 = protected_pool.add_track(p);
  protected_pool.slot(2).owner = p1;
  protected_pool.slot(1).owner = p2;
  auto order = protected_pool.order();
  check(refuses<A::HostFault>([&] { protected_pool.alloc(); }),
        "protected pool original failure");
  check(protected_pool.order() == std::vector<A::SlotId>({order[1], order[0]}),
        "protected failure retains N+1 rotations");
  check(A::decode_clip(A::parse_clip(bytes()), R::ZERO).size() == 1,
        "fresh context convenience lifetime");
}
} // namespace
int main() {
  try {
    admission();
    graph_and_prefix();
    for (unsigned i = 0; i < 16; ++i)
      lifetime_and_fpu(i);
  } catch (const std::exception &e) {
    ++failures;
    std::cerr << "Unexpected: " << e.what() << '\n';
  }
  std::cout << "Animation decoder API checks: " << checks
            << "; failures: " << failures << '\n';
  return failures ? 1 : 0;
}
