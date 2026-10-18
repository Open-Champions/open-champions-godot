// Tests of the fx recipes port (native/src/fx/recipes.cpp):
//  1. every assertion of the research unit tests tools/test_ref_fx_recipes.py, ported;
//  2. explicit refusals;
//  3. replay of native/tests/vectors/fx_recipes.tsv (written by
//     tools/generate_fx_recipes_vectors.py from the pinned reference on synthetic inputs):
//     every row's call stream, generator states and mutated state must match.
// Usage: champions-fx-recipes-tests <vectors directory>
#include "champions/fx_recipes.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {
namespace rx = champions::rta::fx::recipes;
namespace sim = champions::simulation::rta;
using rx::Call;
using rx::Calls;
using rx::Kind;
using rx::Raw;
using rx::Vec3;
using sim::Lcg;
using sim::Mat34;

int g_checks = 0, g_failures = 0;
#define CHECK(cond)                                                                        \
  do {                                                                                     \
    ++g_checks;                                                                            \
    if (!(cond)) {                                                                         \
      ++g_failures;                                                                        \
      std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                 \
    }                                                                                      \
  } while (0)

// Test-only conversions between host floats and patterns (inputs and readings only).
Raw f32(float v) {
  Raw r;
  std::memcpy(&r, &v, 4);
  return r;
}
double F(Raw r) {
  float v;
  std::memcpy(&v, &r, 4);
  return v;
}
const Raw ONE = sim::ONE;
const Vec3 POS{f32(100.5f), f32(-200.25f), f32(30.0f)};

// Copies: callers often pass a temporary run's calls.
std::vector<Call> spawns(const Calls &calls) {
  std::vector<Call> out;
  for (const auto &c : calls)
    if (c.kind == Kind::Spawn)
      out.push_back(c);
  return out;
}
std::size_t count_kind(const Calls &calls, Kind kind) {
  std::size_t n = 0;
  for (const auto &c : calls)
    n += c.kind == kind;
  return n;
}

// ---- the unit tests' FakeModel --------------------------------------------------------------
struct FakeModel : rx::SampleModel, rx::WeaponModel {
  std::int32_t n;
  Raw size;
  explicit FakeModel(std::int32_t count = 5, float s = 4000.0f) : n(count), size(f32(s)) {}
  std::int32_t sample_count() override { return n; }
  rx::SamplePoint sample_point(Raw, std::uint32_t, const Vec3 &, std::int32_t,
                               std::int32_t index) override {
    return {{f32(10.0f * static_cast<float>(index)), f32(1.0f), f32(2.0f)}, size};
  }
  rx::SamplePoint transform_sample_point(const Mat34 &m, std::int32_t index) override {
    return {sim::mat34_transform_point(m, {f32(static_cast<float>(index)), 0, 0}), size};
  }
  std::array<Vec3, 2> effect_segment() override {
    return {Vec3{0, f32(4.5f), 0}, Vec3{0, f32(38.0f), 0}};
  }
  std::uint32_t attach_base() override { return 2; }
};

// =======================================================================================
// 1. Port of tools/test_ref_fx_recipes.py
// =======================================================================================
struct SpawnRun {
  Calls calls;
  Lcg cos, shr;
};
SpawnRun run_id(std::int32_t id, std::uint32_t seed = 12345, bool near = true, int gore = 2,
                rx::MadeFlags *made = nullptr) {
  SpawnRun r{{}, Lcg(seed), Lcg(seed ^ 0x5555)};
  r.calls = rx::fx_spawn(id, POS, r.cos, r.shr, near, gore, made);
  return r;
}

void test_fx_spawn() {
  // test_counts
  const std::pair<int, std::size_t> counts[] = {
      {0, 300},   {1, 300},   {2, 150},  {3, 150},  {4, 150},  {6, 150},   {7, 3},
      {8, 300},   {9, 300},   {10, 105}, {0xB, 40}, {0xC, 35}, {0xD, 1},   {0xE, 1},
      {0xF, 1},   {0x10, 1},  {0x11, 1}, {0x12, 1}, {0x14, 3}, {0x15, 10}, {0x16, 2}};
  for (const auto &c : counts)
    CHECK(spawns(run_id(c.first).calls).size() == c.second);
  // test_camera_gate_and_id5
  {
    auto r = run_id(2, 12345, false);
    CHECK(r.calls.size() == 1 && r.calls[0].kind == Kind::Near && r.calls[0].v[0] == POS);
    CHECK(r.cos.state == 12345);
    r = run_id(5, 12345, false);
    CHECK(count_kind(r.calls, Kind::Near) == 0);
    const auto fires = count_kind(r.calls, Kind::Fire);
    CHECK(fires == 3 || fires == 4);
    CHECK(r.cos.state == 12345);
    CHECK(r.shr.state != (12345u ^ 0x5555u));
    for (const auto &c : r.calls)
      if (c.kind == Kind::Fire) {
        const double dx = F(c.v[0][0]) - F(POS[0]), dz = F(c.v[0][2]) - F(POS[2]);
        CHECK(-6 <= dx && dx <= 5);
        CHECK(0 <= dz && dz <= 17);
      }
  }
  // test_gore_zero_blocks_red_blood_only
  CHECK(spawns(run_id(0, 12345, true, 0).calls).empty());
  CHECK(spawns(run_id(1, 12345, true, 0).calls).empty());
  CHECK(spawns(run_id(9, 12345, true, 0).calls).size() == 300);
  // test_first_use_constructs_all_definitions_once
  {
    rx::MadeFlags made;
    auto r = run_id(0x11, 12345, true, 2, &made);
    std::vector<std::int64_t> ctors;
    for (const auto &c : r.calls)
      if (c.kind == Kind::Ctor)
        ctors.push_back(c.i[0]);
    CHECK(ctors == std::vector<std::int64_t>(rx::FX_SPAWN_DEFS.begin(), rx::FX_SPAWN_DEFS.end()));
    r = run_id(0x11, 12345, true, 2, &made);
    CHECK(count_kind(r.calls, Kind::Ctor) == 0);
  }
  // test_backdate_is_zero_and_static_ids_have_no_velocity
  for (int id = 0; id < 0x17; ++id)
    for (const Call &c : spawns(run_id(id).calls))
      CHECK(c.f[0] == 0);
  for (int id : {0xD, 0xE, 0xF, 0x10, 0x11, 0x12}) {
    const auto s = spawns(run_id(id).calls);
    CHECK(s.size() == 1 && s[0].v[1] == (Vec3{0, 0, 0}));
  }
  // test_id6_scales_all_axes_by_16
  for (const Call &c : spawns(run_id(6).calls)) {
    double sum = 0;
    for (Raw v : c.v[1])
      sum += F(v) * F(v);
    CHECK(sum <= 16.0 * 16.0 * 1.0001);
  }
  // test_upward_ids
  for (const Call &c : spawns(run_id(0x16).calls))
    CHECK(F(c.v[1][2]) >= 0.0);
  for (std::uint32_t seed = 0; seed < 50; ++seed)
    for (const Call &c : spawns(run_id(7, seed).calls))
      CHECK(F(c.v[1][2]) > 6.05 - 4.75 * 0.77 - 1e-3);
  // test_id15_vertical_terms_vanish_except_at_minus_one
  for (std::uint32_t seed = 0; seed < 30; ++seed)
    for (const Call &c : spawns(run_id(0x15, seed).calls)) {
      const double vz = F(c.v[1][2]), dz = F(c.v[0][2]) - F(POS[2]);
      CHECK(vz == 0.0 || vz == 12.0);
      CHECK(dz == 0.0 || dz == 6.0 || dz == 36.0);
    }
  // test_id13_second_particle_one_in_ten
  {
    std::size_t extra = 0;
    for (std::uint32_t seed = 0; seed < 400; ++seed)
      extra += spawns(run_id(0x13, seed).calls).size() - 1;
    CHECK(10 <= extra && extra <= 80);
  }
  // test_point_within_15
  for (std::uint32_t seed = 0; seed < 100; ++seed) {
    const auto s = spawns(run_id(0xD, seed).calls);
    CHECK(s.size() == 1);
    const double dx = F(s[0].v[0][0]) - F(POS[0]), dy = F(s[0].v[0][1]) - F(POS[1]);
    const double dz = F(s[0].v[0][2]) - F(POS[2]);
    CHECK(dx * dx + dy * dy <= 15.0 * 15.0 * 1.001);
    CHECK(-7.5 <= dz && dz <= 7.5);
  }
}

void test_deaths() {
  // test_disintegrate_volume_rule: 4000^3 = 6.4e10 < 8.09e10, the remainder carries over
  {
    FakeModel model(10, 4000.0f);
    Lcg rng(1);
    const auto calls = rx::disintegrate_model(model, POS, 0, ONE, 0, false, 0, rng);
    const auto s = spawns(calls);
    CHECK(7 <= s.size() && s.size() <= 10);
    CHECK(calls[0].kind == Kind::Hide);
    for (const Call &c : s) {
      CHECK(0 <= F(c.f[0]) && F(c.f[0]) < 0.4);
      CHECK(c.i[0] == 0x870638);
    }
  }
  // test_shatter_uses_only_the_shared_seed
  {
    FakeModel model(12);
    Lcg shared(99);
    const auto calls = rx::shatter_to_loose_ice(model, POS, 0, f32(4.0f), ONE, 0, 0, shared);
    std::vector<const Call *> chunks;
    const Call *first_sample = nullptr;
    for (const auto &c : calls) {
      if (c.kind == Kind::PhProp)
        chunks.push_back(&c);
      if (c.kind == Kind::Sample && !first_sample)
        first_sample = &c;
    }
    CHECK(chunks.size() == 4);
    CHECK(first_sample && first_sample->i[2] == 3); // step 12 // 4
    for (const Call *c : chunks)
      CHECK(c->i[0] == 2 || c->i[0] == 3 || c->i[0] == 4);
    Lcg draws(99);
    for (int k = 0; k < 5; ++k)
      draws.draw();
    CHECK(shared.state == draws.state);
  }
}

std::uint64_t test_mask(std::uint32_t, std::uint32_t variant) {
  return std::uint64_t(variant) * UINT64_C(0x0101010101);
}
rx::Creature creature(std::int32_t kind = 0) {
  rx::Creature c;
  for (std::uint32_t k = 0; k < 8; ++k)
    c.cls[k] = k;
  c.kind = kind;
  return c;
}

void test_lose_part() {
  // test_ant_chunk_shows_the_hidden_meshes
  {
    auto c = creature();
    const auto calls = rx::lose_body_part(rx::PartClass::Ant, c, 3, test_mask);
    CHECK(c.variant == 0x10);
    CHECK(count_kind(calls, Kind::Gut) == 1);
    for (const auto &g : calls)
      if (g.kind == Kind::Gut) {
        CHECK(g.mask64 == (test_mask(3, 0) & ~test_mask(3, 0x10)));
        CHECK(g.i[6] == 0xE && g.i[7] == 2);
      }
    CHECK(count_kind(calls, Kind::Lift) == 1);
  }
  // test_firebeetle_constant_masks_and_blackwidow
  {
    auto c = creature();
    const auto calls = rx::lose_body_part(rx::PartClass::FireBeetle, c, 5, test_mask);
    for (const auto &g : calls)
      if (g.kind == Kind::Gut)
        CHECK(g.mask64 == 0xC00);
    CHECK(!calls.empty() && calls.back().kind == Kind::Mask);
    auto w = creature(1);
    CHECK(rx::lose_body_part(rx::PartClass::BlackWidow, w, 2, test_mask).empty());
    auto b = creature();
    const auto bc = rx::lose_body_part(rx::PartClass::BlackWidow, b, 9, test_mask);
    CHECK(count_kind(bc, Kind::Gut) == 0);
    CHECK(b.variant == 0x200);
  }
  // test_orc_head_mask
  CHECK(rx::orc_head_mask(0) == 0x20406);
  CHECK(rx::orc_head_mask(1) == 0x60406);
  CHECK(rx::orc_head_mask(0x104) == (0x80000000u | (0x20406u ^ 6u) | 0x300000u));
  CHECK(rx::orc_head_mask(0x800) == (UINT64_C(0x400000000) | 0x20406u)); // beyond 32 bits
}

void test_model_recipes() {
  // test_emit_from_verts
  {
    FakeModel model;
    Lcg rng(5);
    const auto calls =
        rx::emit_from_model_verts(model, POS, 0, 0, 7, 0x1234, nullptr, nullptr, ONE, rng);
    CHECK(spawns(calls).size() == 7);
  }
  // test_sparkle_counts: size^2 = 1, one particle per point
  {
    FakeModel model(5, 1.0f);
    Lcg rng(5);
    const auto calls = rx::model_sparkle(&model, POS, 0, 0, ONE, 0, rng);
    std::size_t a = 0, b = 0;
    for (const Call &c : spawns(calls)) {
      a += c.i[0] == 0x870728;
      b += c.i[0] == 0x870778;
    }
    CHECK(a == 17); // round(16.59)
    CHECK(b == 4);  // round(3.76)
    Lcg rng2(5);
    const auto gated = rx::model_sparkle(&model, POS, 0, 0, ONE, 0, rng2, false);
    CHECK(gated.size() == 1 && gated[0].kind == Kind::Near && gated[0].v[0] == POS);
  }
  // test_arcs_create_then_update
  {
    rx::ArcState state;
    FakeModel model;
    auto answer = [](const Vec3 &, const Vec3 &) { return 77u; };
    Lcg rng(3);
    auto calls = rx::model_arcs(&model, POS, 0, 0, f32(2.0f), ONE, 0, state, 0, rng, true,
                                answer);
    CHECK(count_kind(calls, Kind::BeamCreate) == 3);
    for (const auto &a : state.arcs)
      CHECK(a.beam == 77);
    std::size_t updates = 0;
    for (std::uint32_t seed = 0; seed < 20; ++seed) {
      Lcg r(seed);
      calls = rx::model_arcs(&model, POS, 0, 0, f32(2.0f), ONE, 0, state, 0, r, true, answer);
      for (const auto &c : calls)
        if (c.kind == Kind::BeamUpdate) {
          ++updates;
          CHECK(c.i[1] == 94); // alpha clamped to 1
        }
    }
    CHECK(updates > 20);
  }
  // test_glow_ramp_clamps
  {
    rx::GlowRamp ramp;
    FakeModel model;
    const std::uint32_t flags = 0x20;
    const std::array<std::uint32_t, 4> ta{100, 100, 100, 100}, tb{200, 200, 200, 200};
    for (int k = 0; k < 200; ++k)
      rx::model_glow(&model, &flags, ramp, POS, 0, 0, ONE, 0, ONE, ta, tb, 0, 0, 0);
    for (Raw v : ramp.b)
      CHECK(F(v) == 200.0);
    for (int k = 0; k < 200; ++k)
      rx::model_glow(&model, &flags, ramp, POS, 0, 0, ONE, 0, ONE, ta, tb, 2, 0, 0);
    for (Raw v : ramp.b)
      CHECK(F(v) == 0.0);
  }
}

struct StatusRun {
  Calls calls;
  rx::WeaponFxState state;
  rx::WeaponItem item;
};
StatusRun run_fx(std::vector<std::array<std::uint32_t, 2>> entries, bool swinging = false,
                 float amount = 1.0f, const rx::WeaponContext &ctx = {}) {
  StatusRun r;
  for (std::size_t k = 0; k < entries.size(); ++k)
    r.item.entries[k] = entries[k];
  r.state.matrix = sim::mat34_set_pos_rot_z({0, 0, 0}, 0x1000);
  FakeModel model;
  Lcg rng(7);
  r.calls = rx::model_status_effects(f32(amount), ONE, r.item, &model, r.state, POS, 0, nullptr,
                                     0, swinging, 0, nullptr, rng, ctx);
  return r;
}

void test_weapon_status() {
  // test_level_flag_and_dialog
  {
    rx::WeaponContext ctx;
    ctx.level_flags = 0x10000000;
    CHECK(run_fx({{0x18, 1}}, false, 1.0f, ctx).calls.empty());
    rx::WeaponContext dialog;
    dialog.dialog_open = true;
    const auto calls = run_fx({{0x18, 1}}, false, 1.0f, dialog).calls;
    CHECK(calls.size() == 1 && calls[0].kind == Kind::DialogOpen);
  }
  // test_lightning_minimum_and_beam
  {
    rx::WeaponContext ctx;
    ctx.answers.beam_create = 9;
    const auto r = run_fx({{0x18, 1}}, false, 0.01f, ctx);
    std::size_t n = 0;
    for (const Call &c : spawns(r.calls))
      n += c.i[0] == 0x870958;
    CHECK(n >= 8);
    CHECK(r.state.beam == 9);
  }
  // test_entry_value_only_gates
  {
    const auto a = run_fx({{0x16, 1}}).calls;
    const auto b = run_fx({{0x16, 100}}).calls;
    CHECK(rx::stream_text(a) == rx::stream_text(b));
    CHECK(spawns(run_fx({{0x16, 0}}).calls).empty());
  }
  // test_poison_ignores_motion
  {
    const auto calls = run_fx({{0x17, 1}}).calls;
    std::size_t n = 0;
    for (const Call &c : spawns(calls))
      n += c.i[0] == 0x870CC8;
    CHECK(n == static_cast<std::size_t>(26 * 0.155997));
    CHECK(!spawns(calls).empty() && spawns(calls)[0].f[0] == 0); // backdate starts at 0
  }
  // test_item_colours_fade
  {
    rx::WeaponItem item;
    item.glow = {5, -5, 1, 0};
    item.colour2 = {3, 0, -1, 0};
    rx::WeaponFxState state;
    FakeModel model;
    Lcg rng(7);
    rx::model_status_effects(ONE, ONE, item, &model, state, POS, 0, nullptr, 0, false, 0,
                             nullptr, rng);
    CHECK((item.glow == std::array<std::int32_t, 4>{3, -3, 0, 0}));
    CHECK((item.colour2 == std::array<std::int32_t, 4>{1, 0, 0, 0}));
  }
}

// =======================================================================================
// 2. Refusals
// =======================================================================================
struct ShrinkingModel : FakeModel {
  int queries = 0;
  ShrinkingModel() : FakeModel(12) {}
  std::int32_t sample_count() override { return queries++ == 0 ? 12 : 0; }
};

template <class Fn> bool refuses(Fn fn) {
  try {
    fn();
  } catch (const rx::Refusal &) {
    return true;
  }
  return false;
}

void test_refusals() {
  {
    FakeModel model(12);
    Lcg shared(99);
    CHECK(refuses([&] { rx::shatter_to_loose_ice(model, POS, 0, f32(0.5f), ONE, 0, 0, shared); }));
    CHECK(shared.state == 99);
    ShrinkingModel shrinking;
    CHECK(refuses([&] {
      rx::shatter_to_loose_ice(shrinking, POS, 0, f32(4.0f), ONE, 0, 0, shared);
    }));
    CHECK(shared.state == 99);
  }
  {
    auto c = creature();
    c.variant = 0x1234;
    CHECK(refuses([&] { rx::lose_body_part(rx::PartClass::Orc, c, 6, test_mask); }));
    CHECK(refuses([&] { rx::lose_body_part(rx::PartClass::Orc, c, 7, test_mask); }));
    CHECK(c.variant == 0x1234);
  }
  {
    FakeModel empty(0);
    Lcg rng(5);
    CHECK(refuses([&] {
      rx::emit_from_model_verts(empty, POS, 0, 0, 3, 0x1234, nullptr, nullptr, ONE, rng);
    }));
    CHECK(rng.state == 5);
    CHECK(rx::emit_from_model_verts(empty, POS, 0, 0, 0, 0x1234, nullptr, nullptr, ONE, rng)
              .size() == 2);
  }
  {
    rx::WeaponItem item;
    item.entries[0] = {0x18, 1};
    item.glow = {4, 0, 0, 0};
    rx::WeaponFxState state;
    state.counter = 7;
    FakeModel empty(0);
    Lcg rng(7);
    CHECK(refuses([&] {
      rx::model_status_effects(ONE, ONE, item, &empty, state, POS, 0, nullptr, 0, true, 0,
                               nullptr, rng);
    }));
    CHECK(rng.state == 7 && state.counter == 7 && item.glow[0] == 4);
  }
}

// =======================================================================================
// 3. Vector replay
// =======================================================================================
std::uint64_t fnv64(const std::string &s) {
  std::uint64_t v = UINT64_C(0xCBF29CE484222325);
  for (unsigned char b : s)
    v = (v ^ b) * UINT64_C(0x100000001B3);
  return v;
}

std::vector<std::string> split(const std::string &s, char sep) {
  std::vector<std::string> out;
  std::string::size_type start = 0;
  for (;;) {
    const auto at = s.find(sep, start);
    out.push_back(s.substr(start, at == std::string::npos ? std::string::npos : at - start));
    if (at == std::string::npos)
      return out;
    start = at + 1;
  }
}
std::int64_t num(const std::string &s) {
  if (s.empty())
    throw std::runtime_error("empty number");
  if (s[0] == '-')
    return -static_cast<std::int64_t>(std::stoull(s.substr(1), nullptr, 16));
  return static_cast<std::int64_t>(std::stoull(s, nullptr, 16));
}
std::uint64_t unum(const std::string &s) { return std::stoull(s, nullptr, 16); }
Raw raw(const std::string &s) { return static_cast<Raw>(num(s)); }
std::vector<std::int64_t> nums(const std::string &s) {
  std::vector<std::int64_t> out;
  for (const auto &t : split(s, ','))
    out.push_back(num(t));
  return out;
}
Vec3 vec(const std::string &s) {
  const auto v = nums(s);
  if (v.size() != 3)
    throw std::runtime_error("vector of " + std::to_string(v.size()));
  return {Raw(v[0]), Raw(v[1]), Raw(v[2])};
}
Mat34 mat(const std::string &s) {
  const auto v = nums(s);
  if (v.size() != 12)
    throw std::runtime_error("matrix of " + std::to_string(v.size()));
  Mat34 m{};
  for (int k = 0; k < 12; ++k)
    m[k] = Raw(v[k]);
  return m;
}
std::string hex(std::int64_t v) {
  char b[24];
  if (v < 0)
    std::snprintf(b, sizeof b, "-%llx", static_cast<unsigned long long>(-v));
  else
    std::snprintf(b, sizeof b, "%llx", static_cast<unsigned long long>(v));
  return b;
}
template <class Seq> std::string hexl(const Seq &values) {
  std::string out;
  for (const auto &v : values) {
    if (!out.empty())
      out += ',';
    out += hex(static_cast<std::int64_t>(v));
  }
  return out;
}

// ---- synthetic tables (tools/generate_fx_recipes_vectors.py) --------------------------------
constexpr Raw P_2M10 = 0x3A800000, P_2M4 = 0x3D800000, P_EIGHTH = 0x3E000000,
              MINUS_50 = 0xC2480000;
std::int64_t py_mod(std::int64_t a, std::int64_t n) {
  std::int64_t r = a % n;
  if (r != 0 && ((r < 0) != (n < 0)))
    r += n;
  return r;
}
struct Mix {
  std::uint64_t s;
  std::uint64_t next() {
    s += UINT64_C(0x9E3779B97F4A7C15);
    std::uint64_t z = s;
    z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
    return z ^ (z >> 31);
  }
  Raw fixed(std::int64_t lo, std::int64_t hi, Raw scale) {
    const auto k = lo + static_cast<std::int64_t>(next() % static_cast<std::uint64_t>(hi - lo + 1));
    return sim::mul(sim::from_int(static_cast<std::uint32_t>(k)), scale);
  }
};

struct TableModel : rx::SampleModel {
  std::int32_t n = 0;
  std::vector<Vec3> points;
  std::vector<Raw> sizes;
  // "<n>,<seed>,<big>"
  explicit TableModel(const std::string &field) {
    const auto f = split(field, ',');
    n = static_cast<std::int32_t>(num(f[0]));
    Mix mix{unum(f[1])};
    const bool big = num(f[2]) != 0;
    const std::int32_t count = std::max(n, 1);
    for (std::int32_t i = 0; i < count; ++i) {
      Vec3 p{};
      for (auto &v : p)
        v = mix.fixed(-3072000, 3072000, P_2M10);
      points.push_back(p);
    }
    const std::int64_t top = big ? 45000 : 10000;
    for (std::int32_t i = 0; i < count; ++i) {
      const auto kind = mix.next() % 4;
      sizes.push_back(kind == 0   ? mix.fixed(0, top * 16, P_2M4)
                      : kind == 1 ? mix.fixed(0, 12800, P_2M4)
                      : kind == 2 ? 0
                                  : MINUS_50);
    }
  }
  std::int32_t sample_count() override { return n; }
  rx::SamplePoint sample_point(Raw scale, std::uint32_t, const Vec3 &pos, std::int32_t,
                               std::int32_t index) override {
    const auto i = static_cast<std::size_t>(py_mod(index, std::int64_t(points.size())));
    Vec3 p{};
    for (int k = 0; k < 3; ++k)
      p[k] = sim::add(points[i][k], sim::mul(scale, pos[k]));
    return {p, sizes[i]};
  }
};

struct TableWeapon : rx::WeaponModel {
  std::int32_t n = 0;
  std::uint32_t base = 0;
  std::vector<Vec3> points;
  std::vector<Raw> sizes;
  std::array<Vec3, 2> segment{};
  // "<n>,<seed>,<attach base>"
  explicit TableWeapon(const std::string &field) {
    const auto f = split(field, ',');
    n = static_cast<std::int32_t>(num(f[0]));
    Mix mix{unum(f[1])};
    base = static_cast<std::uint32_t>(num(f[2]));
    for (std::int32_t i = 0; i < n; ++i) {
      Vec3 p{};
      for (auto &v : p)
        v = mix.fixed(-20480, 20480, P_2M10);
      points.push_back(p);
    }
    for (std::int32_t i = 0; i < n; ++i)
      sizes.push_back(mix.fixed(0, 48000, P_2M4));
    for (auto &v : segment[0])
      v = mix.fixed(-5120, 5120, P_2M10);
    for (auto &v : segment[1])
      v = mix.fixed(20480, 61440, P_2M10);
  }
  std::int32_t sample_count() override { return n; }
  rx::SamplePoint transform_sample_point(const Mat34 &m, std::int32_t index) override {
    const auto i = static_cast<std::size_t>(py_mod(index, n));
    return {sim::mat34_transform_point(m, points[i]), sizes[i]};
  }
  std::array<Vec3, 2> effect_segment() override { return segment; }
  std::uint32_t attach_base() override { return base; }
};

struct TableHolder : rx::HolderModel {
  Mat34 base{};
  explicit TableHolder(std::uint64_t seed) {
    Mix mix{seed};
    for (int k = 0; k < 9; ++k)
      base[k] = mix.fixed(-1229, 1229, P_2M10);
    for (int k = 9; k < 12; ++k)
      base[k] = mix.fixed(-61440, 61440, P_2M10);
  }
  Mat34 attach_matrix(Raw, std::uint32_t, std::int32_t index) override {
    Mat34 m{};
    for (int k = 0; k < 12; ++k)
      m[k] = sim::add(base[k], sim::mul(sim::from_int(static_cast<std::uint32_t>(index & 7)),
                                        P_EIGHTH));
    return m;
  }
};

std::uint64_t hash_show_mask(std::uint32_t model, std::uint32_t variant) {
  const std::uint64_t x = (std::uint64_t(model) * 0x2545F491u) ^
                          (std::uint64_t(variant) * UINT64_C(0x9E3779B97F4A7C15));
  return x ^ (x >> 29);
}

// ---- replay ------------------------------------------------------------------------------------
struct Tally {
  std::size_t rows = 0, calls = 0, refusals = 0, failures = 0;
};
std::map<std::string, Tally> g_tally;
std::size_t g_examples = 0;

struct Outcome {
  bool refused = false;
  Calls calls;
  std::vector<std::string> state; // the final state fields, as the generator writes them
};

// Compares one row: the expected fields start at `at` (calls, digest, stream, state...).
void compare(const std::string &prop, const std::vector<std::string> &f, std::size_t at,
             const Outcome &got, std::size_t line) {
  Tally &t = g_tally[prop];
  ++t.rows;
  std::string problem;
  const bool want_refusal = f[at] == "REFUSE";
  if (want_refusal != got.refused) {
    problem = want_refusal ? "expected a refusal" : "refused unexpectedly";
  } else if (!want_refusal) {
    const std::string text = rx::stream_text(got.calls);
    t.calls += got.calls.size();
    char digest[24];
    std::snprintf(digest, sizeof digest, "%016llx",
                  static_cast<unsigned long long>(fnv64(text)));
    if (static_cast<std::size_t>(num(f[at])) != got.calls.size())
      problem = "call count " + hex(std::int64_t(got.calls.size())) + " / " + f[at];
    else if (f[at + 2] != "-" && f[at + 2] != text)
      problem = "stream differs:\n    got  " + text.substr(0, 600) + "\n    want " +
                f[at + 2].substr(0, 600);
    else if (f[at + 1] != digest)
      problem = "stream digest differs; got: " + text.substr(0, 600);
  } else {
    ++t.refusals;
  }
  if (problem.empty()) {
    const std::size_t first = at + 3;
    if (f.size() != first + got.state.size())
      problem = "field count";
    else
      for (std::size_t k = 0; k < got.state.size(); ++k)
        if (got.state[k] != f[first + k]) {
          problem = "state field " + std::to_string(k) + ": " + got.state[k] + " / " +
                    f[first + k];
          break;
        }
  }
  if (!problem.empty()) {
    ++t.failures;
    ++g_failures;
    if (g_examples++ < 12)
      std::fprintf(stderr, "MISMATCH line %zu (%s): %s\n", line, prop.c_str(), problem.c_str());
  }
}

template <class Fn> void run_guarded(Outcome &o, Fn fn) {
  try {
    o.calls = fn();
  } catch (const rx::Refusal &) {
    o.refused = true;
    o.calls.clear();
  }
}

std::string made_bits(const rx::MadeFlags *m) {
  if (!m)
    return "-";
  std::uint32_t bits = 0;
  for (std::size_t k = 0; k < m->made.size(); ++k)
    bits |= std::uint32_t(m->made[k]) << k;
  return hex(bits);
}

void replay_row(const std::vector<std::string> &f, std::size_t line) {
  const std::string &kind = f.at(0);
  Outcome o;
  if (kind == "fx_spawn") {
    const auto id = static_cast<std::int32_t>(num(f[1]));
    Lcg cos(static_cast<std::uint32_t>(unum(f[3]))), shr(static_cast<std::uint32_t>(unum(f[4])));
    std::unique_ptr<rx::MadeFlags> made;
    if (f[7] != "-") {
      made = std::make_unique<rx::MadeFlags>();
      const auto bits = unum(f[7]);
      for (std::size_t k = 0; k < 24; ++k)
        made->made[k] = (bits >> k) & 1;
    }
    const auto pool = static_cast<std::uint32_t>(num(f[8]));
    run_guarded(o, [&] {
      return rx::fx_spawn(id, vec(f[2]), cos, shr, f[5] == "1", static_cast<std::int32_t>(num(f[6])),
                          made.get(), [pool](std::uint32_t) { return pool; });
    });
    o.state = {hex(cos.state), hex(shr.state), made_bits(made.get())};
    compare("fx_spawn id " + std::to_string(id), f, 9, o, line);
  } else if (kind == "disintegrate") {
    TableModel model(f[1]);
    Lcg rng(static_cast<std::uint32_t>(unum(f[8])));
    run_guarded(o, [&] {
      return rx::disintegrate_model(model, vec(f[2]), static_cast<std::uint32_t>(num(f[3])),
                                    raw(f[4]), raw(f[5]), num(f[6]) != 0,
                                    static_cast<std::uint32_t>(num(f[7])), rng);
    });
    o.state = {hex(rng.state)};
    compare("disintegrate", f, 9, o, line);
  } else if (kind == "ice_shatter") {
    TableModel model(f[1]);
    Lcg shared(static_cast<std::uint32_t>(unum(f[8])));
    const auto pool = static_cast<std::uint32_t>(num(f[9]));
    run_guarded(o, [&] {
      return rx::shatter_to_loose_ice(model, vec(f[2]), static_cast<std::uint32_t>(num(f[3])),
                                      raw(f[4]), raw(f[5]), raw(f[6]),
                                      static_cast<std::uint32_t>(num(f[7])), shared,
                                      [pool](std::uint32_t) { return pool; });
    });
    o.state = {hex(shared.state)};
    compare("ice shatter", f, 10, o, line);
  } else if (kind == "lose_part") {
    const auto cls = static_cast<rx::PartClass>(num(f[1]));
    rx::Creature c;
    c.variant = static_cast<std::uint32_t>(num(f[2]));
    const auto words = nums(f[3]);
    for (std::size_t k = 0; k < 8; ++k)
      c.cls[k] = static_cast<std::uint32_t>(words.at(k));
    c.pos = vec(f[4]);
    c.yaw = static_cast<std::uint32_t>(num(f[5]));
    c.anim = static_cast<std::uint32_t>(num(f[6]));
    c.colour = static_cast<std::int32_t>(num(f[7]));
    c.scale = raw(f[8]);
    c.kind = static_cast<std::int32_t>(num(f[9]));
    const auto part = static_cast<std::uint32_t>(num(f[10]));
    const auto pool = static_cast<std::uint32_t>(num(f[11]));
    c.show = unum(f[12]);
    run_guarded(o, [&] {
      return rx::lose_body_part(cls, c, part, hash_show_mask,
                                [pool](std::uint32_t) { return pool; });
    });
    char show[24];
    std::snprintf(show, sizeof show, "%llx", static_cast<unsigned long long>(c.show));
    o.state = {hex(c.variant), show};
    compare(std::string("lose part ") + rx::part_class_name(cls), f, 13, o, line);
  } else if (kind == "emit_verts") {
    TableModel model(f[1]);
    Lcg rng(static_cast<std::uint32_t>(unum(f[10])));
    Vec3 direction{}, offset{};
    const bool has_dir = f[7] != "-", has_off = f[8] != "-";
    if (has_dir)
      direction = vec(f[7]);
    if (has_off)
      offset = vec(f[8]);
    run_guarded(o, [&] {
      return rx::emit_from_model_verts(model, vec(f[2]), static_cast<std::uint32_t>(num(f[3])),
                                       static_cast<std::uint32_t>(num(f[4])),
                                       static_cast<std::int32_t>(num(f[5])),
                                       static_cast<std::uint32_t>(num(f[6])),
                                       has_dir ? &direction : nullptr,
                                       has_off ? &offset : nullptr, raw(f[9]), rng);
    });
    o.state = {hex(rng.state)};
    compare("emit from model verts", f, 11, o, line);
  } else if (kind == "sparkle") {
    std::unique_ptr<TableModel> model;
    if (f[1] != "-")
      model = std::make_unique<TableModel>(f[1]);
    Lcg rng(static_cast<std::uint32_t>(unum(f[7])));
    run_guarded(o, [&] {
      return rx::model_sparkle(model.get(), vec(f[2]), static_cast<std::uint32_t>(num(f[3])),
                               static_cast<std::uint32_t>(num(f[4])), raw(f[5]), raw(f[6]), rng,
                               f[8] == "1");
    });
    o.state = {hex(rng.state)};
    compare("model sparkle", f, 9, o, line);
  } else if (kind == "arcs") {
    std::unique_ptr<TableModel> model;
    if (f[1] != "-")
      model = std::make_unique<TableModel>(f[1]);
    rx::ArcState state;
    const auto a = nums(f[8]);
    for (std::size_t k = 0; k < 3; ++k)
      state.arcs[k] = {static_cast<std::int32_t>(a.at(3 * k)),
                       static_cast<std::int32_t>(a.at(3 * k + 1)),
                       static_cast<std::uint32_t>(a.at(3 * k + 2))};
    const auto answer = static_cast<std::uint32_t>(num(f[10]));
    Lcg rng(static_cast<std::uint32_t>(unum(f[11])));
    run_guarded(o, [&] {
      return rx::model_arcs(model.get(), vec(f[2]), static_cast<std::uint32_t>(num(f[3])),
                            static_cast<std::uint32_t>(num(f[4])), raw(f[5]), raw(f[6]),
                            raw(f[7]), state, static_cast<std::uint32_t>(num(f[9])), rng,
                            f[12] == "1",
                            [answer](const Vec3 &, const Vec3 &) { return answer; });
    });
    std::vector<std::int64_t> arcs;
    for (const auto &arc : state.arcs)
      arcs.insert(arcs.end(), {arc.a, arc.b, std::int64_t(arc.beam)});
    o.state = {hex(rng.state), hexl(arcs)};
    compare("model arcs", f, 13, o, line);
  } else if (kind == "glow") {
    std::unique_ptr<TableModel> model;
    if (f[1] != "-")
      model = std::make_unique<TableModel>(f[1]);
    std::uint32_t flags = 0;
    const bool has_obj = f[2] != "-";
    if (has_obj)
      flags = static_cast<std::uint32_t>(num(f[2]));
    rx::GlowRamp ramp;
    const auto ra = nums(f[3]), rb = nums(f[4]), ta = nums(f[11]), tb = nums(f[12]);
    std::array<std::uint32_t, 4> target_a{}, target_b{};
    for (std::size_t k = 0; k < 4; ++k) {
      ramp.a[k] = Raw(ra.at(k));
      ramp.b[k] = Raw(rb.at(k));
      target_a[k] = static_cast<std::uint32_t>(ta.at(k));
      target_b[k] = static_cast<std::uint32_t>(tb.at(k));
    }
    run_guarded(o, [&] {
      return rx::model_glow(model.get(), has_obj ? &flags : nullptr, ramp, vec(f[5]),
                            static_cast<std::uint32_t>(num(f[6])),
                            static_cast<std::uint32_t>(num(f[7])), raw(f[8]), raw(f[9]),
                            raw(f[10]), target_a, target_b, static_cast<std::int32_t>(num(f[13])),
                            static_cast<std::int32_t>(num(f[14])),
                            static_cast<std::uint32_t>(num(f[15])), f[16] == "1");
    });
    o.state = {hexl(ramp.a), hexl(ramp.b)};
    compare("model glow", f, 17, o, line);
  } else if (kind == "weapon") {
    rx::WeaponItem item;
    const auto glow = nums(f[3]), colour2 = nums(f[4]), entries = nums(f[5]);
    for (std::size_t k = 0; k < 4; ++k) {
      item.glow[k] = static_cast<std::int32_t>(glow.at(k));
      item.colour2[k] = static_cast<std::int32_t>(colour2.at(k));
      item.entries[k] = {static_cast<std::uint32_t>(entries.at(2 * k)),
                         static_cast<std::uint32_t>(entries.at(2 * k + 1))};
    }
    std::unique_ptr<TableWeapon> weapon;
    if (f[6] != "-")
      weapon = std::make_unique<TableWeapon>(f[6]);
    rx::WeaponFxState state;
    state.matrix = mat(f[7]);
    state.frame = static_cast<std::uint32_t>(num(f[8]));
    state.beam = static_cast<std::uint32_t>(num(f[9]));
    state.trail = static_cast<std::uint32_t>(num(f[10]));
    state.counter = static_cast<std::int32_t>(num(f[11]));
    state.spark_kind = static_cast<std::int32_t>(num(f[12]));
    state.sparks = static_cast<std::int32_t>(num(f[13]));
    std::unique_ptr<TableHolder> holder;
    if (f[16] != "-")
      holder = std::make_unique<TableHolder>(unum(f[16]));
    Mat34 override_matrix{};
    const bool has_override = f[20] != "-";
    if (has_override)
      override_matrix = mat(f[20]);
    Lcg rng(static_cast<std::uint32_t>(unum(f[21])));
    rx::WeaponContext ctx;
    ctx.level_flags = static_cast<std::uint32_t>(num(f[22]));
    ctx.gore = static_cast<std::int32_t>(num(f[23]));
    ctx.sim_frame = static_cast<std::uint32_t>(num(f[24]));
    ctx.dialog_open = f[25] == "1";
    const auto answers = nums(f[26]);
    ctx.answers = {static_cast<std::uint32_t>(answers.at(0)),
                   static_cast<std::uint32_t>(answers.at(1)),
                   static_cast<std::uint32_t>(answers.at(2))};
    run_guarded(o, [&] {
      return rx::model_status_effects(raw(f[1]), raw(f[2]), item, weapon.get(), state, vec(f[14]),
                                      static_cast<std::uint32_t>(num(f[15])), holder.get(),
                                      static_cast<std::uint32_t>(num(f[17])), f[18] == "1",
                                      static_cast<std::int32_t>(num(f[19])),
                                      has_override ? &override_matrix : nullptr, rng, ctx);
    });
    o.state = {hex(rng.state),      hexl(state.matrix),    hex(state.frame),
               hex(state.beam),     hex(state.trail),      hex(state.counter),
               hex(state.spark_kind), hex(state.sparks),   hexl(item.glow),
               hexl(item.colour2)};
    compare("weapon status", f, 27, o, line);
  } else {
    throw std::runtime_error("unknown property " + kind);
  }
}

bool replay(const std::string &dir) {
  const std::string path = dir + "/fx_recipes.tsv";
  std::ifstream in(path);
  if (!in) {
    std::fprintf(stderr, "cannot open %s\n", path.c_str());
    return false;
  }
  std::string text;
  std::size_t line = 0;
  while (std::getline(in, text)) {
    ++line;
    if (text.empty())
      continue;
    try {
      replay_row(split(text, '\t'), line);
    } catch (const std::exception &e) {
      ++g_failures;
      std::fprintf(stderr, "line %zu: malformed row (%s)\n", line, e.what());
    }
  }
  std::size_t rows = 0;
  for (const auto &kv : g_tally) {
    rows += kv.second.rows;
    std::printf("%s  %-28s %6zu cases %8zu calls %4zu refusals %zu mismatches\n",
                kv.second.failures ? "FAIL" : "PASS", kv.first.c_str(), kv.second.rows,
                kv.second.calls, kv.second.refusals, kv.second.failures);
  }
  std::printf("%zu vector rows\n", rows);
  return rows > 0;
}
} // namespace

int main(int argc, char **argv) {
  test_fx_spawn();
  test_deaths();
  test_lose_part();
  test_model_recipes();
  test_weapon_status();
  test_refusals();
  std::printf("unit checks: %d, failures %d\n", g_checks, g_failures);
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s <vectors directory>\n", argv[0]);
    return 2;
  }
  const bool ok = replay(argv[1]);
  std::printf("%s (%d failures)\n", ok && !g_failures ? "OK" : "FAILED", g_failures);
  return ok && !g_failures ? 0 : 1;
}
