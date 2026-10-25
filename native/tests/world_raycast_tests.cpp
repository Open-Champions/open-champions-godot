// Tests for terrain_raycast (0x18a5c0) on authored synthetic levels only.
//
// Two kinds of expectation are used:
//  * values derived by hand from the documented contract (vertical rays,
//    clear rays, flat floors, the wall step, flag behaviour, reach limits);
//  * raw32 results produced by tools/ref_world.py terrain_raycast at research
//    commit 328b072 on the same synthetic input (the reference is the
//    documented contract, so a disagreement means this port or a fixture here
//    is wrong). No game data is used or copied.
// Where the reference differs from the naive geometric answer by a step or
// four, the reason is the documented truncating rotation constant (cos 32767
// is 0.99999994, not 1.0) and truncation (not floor) of the sample index.
#include "champions/world_queries.hpp"
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
namespace R = champions::simulation::rta;
namespace W = R::world_queries;
namespace {
unsigned checks = 0;
void check(bool ok, const std::string &why) {
  ++checks;
  if (!ok)
    throw std::runtime_error(why);
}
// Bit pattern of a literal; no host float arithmetic is ever performed.
R::Raw bits(float v) {
  std::uint32_t r;
  std::memcpy(&r, &v, sizeof r);
  return r;
}
constexpr R::Raw MINUS_ONE = 0xbf800000u;
using Samples = std::vector<std::int16_t>;
Samples field(int w, int h, std::int16_t (*fn)(int)) {
  Samples s;
  for (int j = 0; j < h; ++j)
    for (int i = 0; i < w; ++i)
      s.push_back(fn(i));
  return s;
}
W::HeightField hf(int w, int h, int lo, int hi, Samples s) {
  W::HeightField f;
  f.w = w;
  f.h = h;
  f.lo = lo;
  f.hi = hi;
  f.samples = std::move(s);
  return f;
}
// The grid origin is (0, -1) in 160-unit cells with 14x2 cells; every cell
// lists every element, so only the element rectangles decide collection.
W::LevelData level(std::vector<W::HeightField> fields,
                   std::vector<W::Element> elements) {
  W::LevelData d;
  d.tg_x0 = 0;
  d.tg_y0 = -1;
  d.tg_w = 14;
  d.tg_h = 2;
  d.heightfields = std::move(fields);
  d.elements = std::move(elements);
  std::vector<std::int64_t> all;
  for (std::size_t i = 0; i < d.elements.size(); ++i)
    all.push_back(std::int64_t(i));
  d.grid.assign(28, all);
  return d;
}
W::Element el(std::int64_t i, std::int64_t x0, std::int64_t y0, std::int64_t x1,
              std::int64_t y1, std::int64_t f, std::int64_t flags,
              std::int64_t x, std::int64_t y, std::int64_t cs = 32767,
              std::int64_t sn = 0) {
  return W::make_element(i, x0, y0, x1, y1, f, flags, x, y, 0, cs, sn);
}
std::int16_t flat10(int) { return 160; }
std::int16_t zero(int) { return 0; }
std::int16_t step20(int i) { return i >= 20 ? 800 : 0; }
std::int16_t step30(int i) { return i >= 30 ? 800 : 0; }
std::int16_t step25(int i) { return i >= 25 ? 800 : 0; }
std::int16_t flagged(int i) { return i == 20 ? 1 : i == 30 ? 2 : i == 40 ? 4 : 0; }

W::LevelData named(const std::string &n) {
  if (n == "flat")
    return level({hf(64, 16, 10, 10, field(64, 16, flat10))},
                 {el(0, 0, 0, 256, 64, 0, 0, 0, 0)});
  if (n == "wall")
    return level({hf(64, 8, 0, 50, field(64, 8, step20))},
                 {el(0, 0, 0, 256, 32, 0, 0, 0, 0)});
  if (n == "flags_hi30" || n == "flags_hi0" || n == "flags_hi0_e4")
    return level({hf(64, 8, 0, n == "flags_hi30" ? 30 : 0, field(64, 8, flagged))},
                 {el(0, 0, 0, 256, 32, 0, n == "flags_hi0_e4" ? 4 : 0, 0, 0)});
  if (n == "two")
    return level({hf(64, 8, 0, 0, field(64, 8, zero)),
                  hf(40, 8, 0, 50, field(40, 8, step30))},
                 {el(0, 0, 0, 256, 32, 0, 0, 0, 0),
                  el(1, 600, 0, 760, 32, 1, 0, 600, 0)});
  if (n == "far_floor")
    return level({hf(40, 8, 0, 0, field(40, 8, zero))},
                 {el(0, 600, 0, 760, 32, 0, 0, 600, 0)});
  if (n == "rot90")
    return level({hf(40, 40, 0, 50, field(40, 40, step25))},
                 {el(0, 480, 0, 640, 160, 0, 0, 640, 0, 0, 32767)});
  if (n == "mirror")
    return level({hf(64, 8, 0, 50, field(64, 8, step20))},
                 {el(0, 0, -32, 256, 0, 0, 1, 0, 0)});
  throw std::runtime_error("unknown fixture " + n);
}
struct Case {
  const char *name, *level;
  float max_t;
  float o[3], d[3];
  std::uint32_t flags;
  R::Raw want;
};
// want: ref_world.terrain_raycast at research commit 328b072 (synthetic
// input); the trailing comment is the value as a decimal float.
const Case cases[] = {
    {"vertical down reads floor height under origin", "flat", 500.0f, {50.0f, 20.0f, 100.0f}, {0.0f, 0.0f, -1.0f}, 0, 0x42b40000u}, // 90
    {"vertical up is a miss", "flat", 500.0f, {50.0f, 20.0f, 100.0f}, {0.0f, 0.0f, 1.0f}, 0, 0xbf800000u}, // -1
    {"vertical off the grid reads 0.0", "flat", 500.0f, {-500.0f, 20.0f, 100.0f}, {0.0f, 0.0f, -1.0f}, 0, 0x42c80000u}, // 100
    {"horizontal ray above all terrain", "flat", 200.0f, {2.0f, 20.0f, 100.0f}, {1.0f, 0.0f, 0.0f}, 0, 0xbf800000u}, // -1
    {"origin below floor hits at once (t = 0)", "flat", 200.0f, {2.0f, 20.0f, 5.0f}, {1.0f, 0.0f, 0.0f}, 0, 0x00000000u}, // 0
    {"maxT = 0", "flat", 0.0f, {2.0f, 20.0f, 5.0f}, {1.0f, 0.0f, 0.0f}, 0, 0xbf800000u}, // -1
    {"maxT < 0", "flat", -5.0f, {2.0f, 20.0f, 5.0f}, {1.0f, 0.0f, 0.0f}, 0, 0xbf800000u}, // -1
    {"descending ray onto flat floor", "flat", 200.0f, {2.0f, 20.0f, 50.0f}, {1.0f, 0.0f, -1.0f}, 0, 0x42300000u}, // 44
    {"descending ray, reach too short", "flat", 30.0f, {2.0f, 20.0f, 50.0f}, {1.0f, 0.0f, -1.0f}, 0, 0xbf800000u}, // -1
    {"wall: horizontal ray stops at step", "wall", 200.0f, {2.0f, 10.0f, 20.0f}, {1.0f, 0.0f, 0.0f}, 0, 0x42a00000u}, // 80
    {"wall: reach short of the wall", "wall", 60.0f, {2.0f, 10.0f, 20.0f}, {1.0f, 0.0f, 0.0f}, 0, 0xbf800000u}, // -1
    {"wall: ray above wall top skipped by max height", "wall", 200.0f, {250.0f, 10.0f, 60.0f}, {-1.0f, 0.0f, 0.0f}, 0, 0xbf800000u}, // -1
    {"wall: ray from outside the field clips in", "wall", 400.0f, {-100.0f, 10.0f, 20.0f}, {1.0f, 0.0f, 0.0f}, 0, 0x4337ffffu}, // 184
    {"wall: ray along +y clips in at the near edge", "wall", 200.0f, {100.0f, -20.0f, 20.0f}, {0.0f, 1.0f, 0.0f}, 0, 0x419fffffu}, // 20
    {"wall: ray along -y from beyond the far edge", "wall", 200.0f, {100.0f, 80.0f, 20.0f}, {0.0f, -1.0f, 0.0f}, 0, 0x424ffffeu}, // 52
    {"wall: ray leaving the field never hits", "wall", 400.0f, {-100.0f, 10.0f, 20.0f}, {-1.0f, 0.0f, 0.0f}, 0, 0xbf800000u}, // -1
    {"wall: ray ending in the dead zone outside", "wall", 400.0f, {2.0f, 10.0f, 20.0f}, {-1.0f, 0.0f, 0.0f}, 0, 0xbf800000u}, // -1
    {"wall: oblique ray", "wall", 400.0f, {2.0f, 2.0f, 20.0f}, {1.0f, 0.25f, 0.0f}, 0, 0x42a2fbe3u}, // 81.492
    {"flags ray=0, flagged samples, stored max 30", "flags_hi30", 400.0f, {2.0f, 10.0f, 20.0f}, {1.0f, 0.0f, 0.0f}, 0, 0xbf800000u}, // -1
    {"flags ray=1, flagged samples, stored max 30", "flags_hi30", 400.0f, {2.0f, 10.0f, 20.0f}, {1.0f, 0.0f, 0.0f}, 1, 0x42a00000u}, // 80
    {"flags ray=2, flagged samples, stored max 30", "flags_hi30", 400.0f, {2.0f, 10.0f, 20.0f}, {1.0f, 0.0f, 0.0f}, 2, 0x42f00000u}, // 120
    {"flags ray=3, flagged samples, stored max 30", "flags_hi30", 400.0f, {2.0f, 10.0f, 20.0f}, {1.0f, 0.0f, 0.0f}, 3, 0x42a00000u}, // 80
    {"flags ray=4, flagged samples, stored max 30", "flags_hi30", 400.0f, {2.0f, 10.0f, 20.0f}, {1.0f, 0.0f, 0.0f}, 4, 0x43200000u}, // 160
    {"flags ray=7, flagged samples, stored max 30", "flags_hi30", 400.0f, {2.0f, 10.0f, 20.0f}, {1.0f, 0.0f, 0.0f}, 7, 0x42a00000u}, // 80
    {"flags ray=8, flagged samples, stored max 30", "flags_hi30", 400.0f, {2.0f, 10.0f, 20.0f}, {1.0f, 0.0f, 0.0f}, 8, 0xbf800000u}, // -1
    {"flags ray=0, stored max too low, element flag 0", "flags_hi0", 400.0f, {2.0f, 10.0f, 20.0f}, {1.0f, 0.0f, 0.0f}, 0, 0xbf800000u}, // -1
    {"flags ray=0, stored max too low, element flag 4", "flags_hi0_e4", 400.0f, {2.0f, 10.0f, 20.0f}, {1.0f, 0.0f, 0.0f}, 0, 0xbf800000u}, // -1
    {"flags ray=3, stored max too low, element flag 0", "flags_hi0", 400.0f, {2.0f, 10.0f, 20.0f}, {1.0f, 0.0f, 0.0f}, 3, 0xbf800000u}, // -1
    {"flags ray=3, stored max too low, element flag 4", "flags_hi0_e4", 400.0f, {2.0f, 10.0f, 20.0f}, {1.0f, 0.0f, 0.0f}, 3, 0x42a00000u}, // 80
    {"two: hit in the third 240-unit segment", "two", 1000.0f, {2.0f, 10.0f, 20.0f}, {1.0f, 0.0f, 0.0f}, 0, 0x44347fffu}, // 722
    {"two: reach ends after the second segment", "two", 250.0f, {2.0f, 10.0f, 20.0f}, {1.0f, 0.0f, 0.0f}, 0, 0xbf800000u}, // -1
    {"two: hit lies beyond the reach", "two", 700.0f, {2.0f, 10.0f, 20.0f}, {1.0f, 0.0f, 0.0f}, 0, 0xbf800000u}, // -1
    {"two: hit inside the reach", "two", 740.0f, {2.0f, 10.0f, 20.0f}, {1.0f, 0.0f, 0.0f}, 0, 0x44347fffu}, // 722
    {"two: oblique multi-segment", "two", 1000.0f, {2.0f, 2.0f, 20.0f}, {1.0f, 0.01f, 0.0f}, 0, 0x44347f9au}, // 721.994
    {"slope quirk: height advanced four times too far", "far_floor", 2000.0f, {2.0f, 10.0f, 100.0f}, {1.0f, 0.0f, -0.1f}, 0, 0x44157fffu}, // 598
    {"slope quirk: no quirk in a horizontal ray", "far_floor", 2000.0f, {2.0f, 10.0f, 100.0f}, {1.0f, 0.0f, 0.0f}, 0, 0xbf800000u}, // -1
    {"rotated 90 degrees: ray along +y", "rot90", 500.0f, {560.0f, -100.0f, 20.0f}, {0.0f, 1.0f, 0.0f}, 0, 0x434bffffu}, // 204
    {"rotated 90 degrees: ray along +x misses", "rot90", 500.0f, {400.0f, 80.0f, 20.0f}, {1.0f, 0.0f, 0.0f}, 0, 0xbf800000u}, // -1
    {"mirrored element: ray along +x", "mirror", 400.0f, {2.0f, -10.0f, 20.0f}, {1.0f, 0.0f, 0.0f}, 0, 0x42a00000u}, // 80
    {"mirrored element: ray along -y", "mirror", 400.0f, {100.0f, 30.0f, 20.0f}, {0.0f, -1.0f, 0.0f}, 0, 0x41efffffu}, // 30
};
W::Vec3 vec(const float (&v)[3]) { return {bits(v[0]), bits(v[1]), bits(v[2])}; }

void table() {
  for (const auto &c : cases) {
    W::Level lv(named(c.level));
    const auto got = W::terrain_raycast(lv, bits(c.max_t), vec(c.o), vec(c.d),
                                        c.flags);
    check(got == c.want, std::string("raycast mismatch: ") + c.name);
  }
}
// Values derivable on paper from the contract (Q = 0.25 samples per unit).
void by_hand() {
  W::Level flat(named("flat"));
  // Straight down: origin height minus the sample under it (100 - 10).
  check(W::terrain_raycast(flat, bits(500.f), {bits(50.f), bits(20.f), bits(100.f)},
                           {0, 0, bits(-1.f)}, 0) == bits(90.f),
        "vertical ray returns O.z - floor height");
  // Straight up and horizontal-clear rays are -1.0 exactly.
  check(W::terrain_raycast(flat, bits(500.f), {0, 0, bits(100.f)},
                           {0, 0, bits(1.f)}, 0) == MINUS_ONE,
        "upward vertical ray misses");
  check(W::terrain_raycast(flat, bits(200.f), {bits(2.f), bits(20.f), bits(100.f)},
                           {bits(1.f), 0, 0}, 0) == MINUS_ONE,
        "horizontal ray above everything is -1.0");
  // Origin under the floor: first sample already higher -> t = 0.
  check(W::terrain_raycast(flat, bits(200.f), {bits(2.f), bits(20.f), bits(5.f)},
                           {bits(1.f), 0, 0}, 0) == 0,
        "start under the floor hits at t = 0");
  // maxT <= 0 is -1.0 whatever is below.
  for (float mt : {0.f, -1.f, -1000.f})
    check(W::terrain_raycast(flat, bits(mt), {bits(2.f), bits(20.f), bits(5.f)},
                             {bits(1.f), 0, 0}, 0) == MINUS_ONE,
          "non-positive reach is -1.0");
  // Slope of 1 per unit: z = 50 - 4 t samples; first sample under
  // the 10-high floor is t = 11, which is 44 world units along the ray's
  // horizontal speed 1/4.
  check(W::terrain_raycast(flat, bits(200.f), {bits(2.f), bits(20.f), bits(50.f)},
                           {bits(1.f), 0, bits(-1.f)}, 0) == bits(44.f),
        "sloped ray hits the flat floor at step 11 (44 units)");
  W::Level wall(named("wall"));
  // Wall of sample 20: the hit is 20 samples along, 80 world units.
  check(W::terrain_raycast(wall, bits(200.f), {bits(2.f), bits(10.f), bits(20.f)},
                           {bits(1.f), 0, 0}, 0) == bits(80.f),
        "horizontal ray reaches the wall after 20 samples");
  // Same wall but reach 60 (15 samples) cannot get there.
  check(W::terrain_raycast(wall, bits(60.f), {bits(2.f), bits(10.f), bits(20.f)},
                           {bits(1.f), 0, 0}, 0) == MINUS_ONE,
        "reach shorter than the wall gives -1.0");
  // Clipping in along y from the near edge: 20 units to the edge, 20.0.
  check(W::terrain_raycast(wall, bits(200.f), {bits(100.f), bits(-20.f), bits(20.f)},
                           {0, bits(1.f), 0}, 0) == 0x419fffffu,
        "clip to the near y edge (20 units less one ulp from the truncating "
        "rotation)");
  // Direction scale does not matter: t is a parameter of D.
  check(W::terrain_raycast(wall, bits(100.f), {bits(2.f), bits(10.f), bits(20.f)},
                           {bits(2.f), 0, 0}, 0) == bits(40.f),
        "doubling D halves t");
  // The 240-unit march: the same wall reached through three segments.
  W::Level two(named("two"));
  const auto t = W::terrain_raycast(two, bits(1000.f),
                                    {bits(2.f), bits(10.f), bits(20.f)},
                                    {bits(1.f), 0, 0}, 0);
  check(t == 0x44347fffu, "segmented march returns 722 less one ulp");
  // Slope quirk: E.z advances 4x too far, so a floor that the true ray
  // (still 40 above it at x = 600) never touches is hit as soon as the
  // element is collected: 600 - 2 = 598 world units.
  W::Level far_floor(named("far_floor"));
  check(W::terrain_raycast(far_floor, bits(2000.f),
                           {bits(2.f), bits(10.f), bits(100.f)},
                           {bits(1.f), 0, bits(-0.1f)}, 0) == 0x44157fffu,
        "height advanced four times too far makes the far floor hit at x=600");
  check(W::terrain_raycast(far_floor, bits(2000.f),
                           {bits(2.f), bits(10.f), bits(100.f)},
                           {bits(1.f), 0, 0}, 0) == MINUS_ONE,
        "a horizontal ray is unaffected by the quirk");
  // Flags: ray flag 1 sees the flag-1 sample (step 20), flag 2 the flag-2
  // sample (step 30), flag 4 the flag-4 sample (step 40), flag 8 none.
  const struct { std::uint32_t flags; R::Raw want; } by_flag[] = {
      {0, MINUS_ONE}, {1, bits(80.f)}, {2, bits(120.f)}, {3, bits(80.f)},
      {4, bits(160.f)}, {7, bits(80.f)}, {8, MINUS_ONE}};
  for (const auto &f : by_flag) {
    W::Level lv(named("flags_hi30"));
    check(W::terrain_raycast(lv, bits(400.f), {bits(2.f), bits(10.f), bits(20.f)},
                             {bits(1.f), 0, 0}, f.flags) == f.want,
          "flag masks pick which flagged sample is a 65536-high wall");
  }
  // Only flags & 0xffff counts, so bit 16 alone behaves like flags 0.
  {
    W::Level lv(named("flags_hi30"));
    check(W::terrain_raycast(lv, bits(400.f), {bits(2.f), bits(10.f), bits(20.f)},
                             {bits(1.f), 0, 0}, 0x10000u) == MINUS_ONE,
          "flags above bit 15 do not reach the sample mask");
  }
}
void state_and_limits() {
  W::Level lv(named("wall"));
  const W::Vec3 o{bits(2.f), bits(10.f), bits(20.f)};
  const W::Vec3 d{bits(1.f), 0, 0};
  const auto s0 = lv.data().stamp;
  const auto first = W::terrain_raycast(lv, bits(200.f), o, d, 0);
  check(lv.data().stamp == s0 + 1, "a ray advances the stamp exactly once");
  check(lv.data().elements[0].stamp == s0 + 1, "collected element is stamped");
  check(W::terrain_raycast(lv, bits(200.f), o, d, 0) == first &&
            lv.data().stamp == s0 + 2,
        "repeating a ray gives the same result with a fresh stamp");
  // Vertical rays and non-positive reach: stamp rules.
  const auto s1 = lv.data().stamp;
  (void)W::terrain_raycast(lv, bits(200.f), o, {0, 0, bits(-1.f)}, 0);
  check(lv.data().stamp == s1, "a vertical ray does not touch the stamp");
  (void)W::terrain_raycast(lv, 0, o, d, 0);
  check(lv.data().stamp == s1 + 1,
        "a non-positive reach still advances the stamp (after the vertical test)");
  // The ray never edits terrain.
  check(lv.data().heightfields[0].samples == named("wall").heightfields[0].samples &&
            lv.data().elements[0].z == 0 && lv.data().elements[0].flags == 0,
        "terrain data is untouched");
  // Element flag 2 (ignored) is skipped by collection.
  auto d2 = named("wall");
  d2.elements[0].flags = 2;
  W::Level ignored(d2);
  check(W::terrain_raycast(ignored, bits(200.f), o, d, 0) == MINUS_ONE,
        "an ignored element is never marched");
  // The work budget bounds the march.
  W::Limits tiny;
  tiny.query_work = 10;
  W::Level limited(named("wall"), tiny);
  bool refused = false;
  try {
    (void)W::terrain_raycast(limited, bits(200.f), o, d, 0);
  } catch (const W::ResourceLimit &) {
    refused = true;
  }
  check(refused, "query work budget stops a ray that exceeds it");
}
} // namespace
int main() {
  try {
    R::ScopedFpu scope(R::Fpu(R::Fpu::Model::Ps2, false, false));
    table();
    by_hand();
    state_and_limits();
  } catch (const std::exception &e) {
    std::cerr << "world raycast test failed: " << e.what() << '\n';
    return 1;
  }
  std::cout << "world raycast tests passed (" << checks << " checks)\n";
  return 0;
}
