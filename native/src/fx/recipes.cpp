// One-shot, death, body-part and status effect recipes (Return to Arms v2.00).
// Handwritten port of the research contract tools/ref_fx_recipes.py (commit 19f80a6),
// operation for operation, on raw float patterns. docs/effects.md sections 10, 14 and 17
// are the prose version. No particle-definition values appear here: definitions are
// named by address (the key into the user's own fx_particle_defs.py --json).
#include "champions/fx_recipes.hpp"

#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <functional>

namespace champions::rta::fx::recipes {
namespace {
namespace sim = champions::simulation::rta;
using sim::add;
using sim::cos16;
using sim::div;
using sim::eq;
using sim::from_int;
using sim::le;
using sim::lt;
using sim::mul;
using sim::rsqrt;
using sim::s16;
using sim::s32;
using sim::sin16;
using sim::sub;
using sim::to_int;

constexpr Raw ZERO = sim::ZERO, ONE = sim::ONE, HALF = sim::HALF, TWO_M16 = sim::TWO_M16;
constexpr std::uint32_t M32 = 0xFFFFFFFFu;

// ---- float constants of fx_spawn, as the compiler stored them (lui/ori pairs) ----------
constexpr Raw K_MINUS_HALF = 0xBF000000, K_QUARTER = 0x3E800000, K_0_07 = 0x3D8F5C28,
              K_0_75 = 0x3F400000, K_0_77 = 0x3F451EB8, K_1_28 = 0x3FA3D70A,
              K_1_633 = 0x3FD10624, K_4 = 0x40800000, K_4_75 = 0x40980000,
              K_5 = 0x40A00000, K_5_45 = 0x40AE6666, K_6 = 0x40C00000, K_6_05 = 0x40C19999,
              K_7_86 = 0x40FB851E, K_10_16 = 0x41228F5C, K_10_45 = 0x41273333,
              K_12 = 0x41400000, K_15 = 0x41700000, K_15_75 = 0x417C0000,
              K_16 = 0x41800000, K_18 = 0x41900000, K_20 = 0x41A00000, K_21_6 = 0x41ACCCCC,
              K_24 = 0x41C00000, K_24_72 = 0x41C5C28F, K_34 = 0x42080000,
              K_36 = 0x42100000, K_40 = 0x42200000;
constexpr sim::DoubleRaw D_HALF = UINT64_C(0x3FE0000000000000);

const char *const BLOOD_SOUND_LUMP = "fx.lmp";

// ---- deaths ------------------------------------------------------------------------------
constexpr Raw K_1E_5 = 0x3727C5AC, K_35_35 = 0x420D6666, K_8_09E10 = 0x5196B020,
              K_3_14 = 0x4048F5C2, K_0_4 = 0x3ECCCCCC;
constexpr std::uint32_t DEF_DISINTEGRATE = 0x870638, DEF_DISINTEGRATE_ALT = 0x870688;
const char *const DISINTEGRATE_SOUND = "spel_dsrptn_afteraffct.anm";
const char *const ICE_CHUNK_MODEL = "icechunk.vif";
const char *const ICE_CHUNK_TEXTURE = "icechunk.tex";
constexpr std::uint32_t ICE_CHUNK_LAUNCH_FLAGS = 0x30A, VT_LOOSE_ICE_CHUNK = 0x50F328;

// ---- model helpers -------------------------------------------------------------------------
constexpr Raw K_9 = 0x41100000, K_10 = 0x41200000, K_0_3 = 0x3E999999,
              K_16_59 = 0x4184BFAB, K_3_76 = 0x4070B335, K_1_00597 = 0x3F80C39F,
              K_1E_4 = 0x38D1B717, K_1_145E8 = 0x4CDA69C5, K_94 = 0x42BC0000,
              K_250 = 0x437A0000, K_2013 = 0x44FBA000, K_10000 = 0x461C4000,
              K_60 = 0x42700000, K_GLOW_BIAS = 0xC14DB22D;
constexpr std::uint32_t GLOW_TEXTURE_HALO2 = 0x4F848C; // fx.lmp : halo2.tex (main 0x324028)
constexpr Raw K_7_78 = 0x40F8F5C2, K_4_47 = 0x408F0A3D;
constexpr std::uint32_t DEF_ARC_SPARK = 0x8708B8, DEF_ARC_SPARK_SLOW = 0x870908;

// ---- fx_modelStatusEffects -----------------------------------------------------------------
constexpr Raw K_0_05 = 0x3D4CCCCC, K_0_2 = 0x3E4CCCCC, K_0_68 = 0x3F2E147A, K_0_7 = 0x3F333333,
              K_0_00191 = 0x3AFA58F5, K_0_00291 = 0x3B3EB5B1, K_1_24TH = 0x3D2AAAAA,
              K_0_074 = 0x3D978D4F, K_0_156 = 0x3E1FBDAD, K_0_206 = 0x3E52EB5F,
              K_0_229 = 0x3E6A7EF9, K_0_556 = 0x3F0E55F3, K_0_578 = 0x3F13F7AD,
              K_0_667 = 0x3F2AC072, K_0_785 = 0x3F48F590, K_0_97 = 0x3F7851EB,
              K_2_47 = 0x401E147A, K_2_83 = 0x40351EB8, K_3 = 0x40400000,
              K_3_13 = 0x404851EB, K_3_5 = 0x40600000, K_3_974 = 0x407E5597,
              K_6_388 = 0x40CC6A46, K_6_589 = 0x40D2D916, K_13_13 = 0x4152147A,
              K_19_74 = 0x419DEB85, K_30 = 0x41F00000;
constexpr std::uint32_t ARC_RGBA = 0x5E808080;

// ---- small integer helpers with the reference's (Python) semantics ---------------------------
// float(int) of any integer: cvt.s.w reads the low 32 bits as signed.
Raw fi(std::int64_t value) { return from_int(static_cast<std::uint32_t>(value)); }
// a % n with the sign of n (Python); n != 0.
std::int64_t py_mod(std::int64_t a, std::int64_t n) {
  std::int64_t r = a % n;
  if (r != 0 && ((r < 0) != (n < 0)))
    r += n;
  return r;
}
std::int32_t s8(std::int64_t x) {
  const auto b = static_cast<std::int32_t>(static_cast<std::uint64_t>(x) & 0xFF);
  return b & 0x80 ? b - 0x100 : b;
}

// ---- call builders ---------------------------------------------------------------------------
Call make(Kind kind) {
  Call c;
  c.kind = kind;
  return c;
}
Call near_call(const Vec3 &pos) {
  Call c = make(Kind::Near);
  c.v[0] = pos;
  return c;
}
Call spawn_call(Raw backdate, std::uint32_t definition, const Vec3 &pos, const Vec3 &vel) {
  Call c = make(Kind::Spawn);
  c.f[0] = backdate;
  c.i[0] = definition;
  c.v[0] = pos;
  c.v[1] = vel;
  return c;
}
Call int_call(Kind kind, std::int64_t value) {
  Call c = make(kind);
  c.i[0] = value;
  return c;
}
Call res_find_call(const char *lump, const char *name = nullptr) {
  Call c = make(Kind::ResFind);
  c.s[0] = lump;
  c.s[1] = name;
  return c;
}
Call sound_call(Raw volume, const Vec3 &pos) {
  Call c = make(Kind::Sound);
  c.f[0] = volume;
  c.v[0] = pos;
  return c;
}
Call sample_call(Raw scale, std::uint32_t anim, const Vec3 &pos, std::int32_t yaw,
                 std::int64_t index) {
  Call c = make(Kind::Sample);
  c.f[0] = scale;
  c.i[0] = anim;
  c.v[0] = pos;
  c.i[1] = yaw;
  c.i[2] = index;
  return c;
}
Call beam_create_call(Raw jitter, Raw scale, const Vec3 &a, const Vec3 &b, std::uint32_t width,
                      std::uint32_t depth, std::uint32_t rgba) {
  Call c = make(Kind::BeamCreate);
  c.f = {jitter, scale};
  c.v = {a, b};
  c.i[0] = width;
  c.i[1] = depth;
  c.i[2] = rgba;
  return c;
}
Call beam_update_call(std::uint32_t handle, const Vec3 &a, const Vec3 &b, std::int32_t alpha,
                      std::uint32_t width) {
  Call c = make(Kind::BeamUpdate);
  c.i[0] = handle;
  c.v = {a, b};
  c.i[1] = alpha;
  c.i[2] = width;
  return c;
}
Call glow_call(Raw bias, Raw size, const Vec3 &pos, const std::array<std::uint32_t, 4> &rgba,
               std::uint32_t texture, std::int32_t angle, std::uint32_t flags) {
  Call c = make(Kind::Glow);
  c.f = {bias, size};
  c.v[0] = pos;
  c.rgba = rgba;
  c.i[0] = texture;
  c.i[1] = angle;
  c.i[2] = flags;
  return c;
}
Call transform_sample_call(const Mat34 &m, std::int64_t index) {
  Call c = make(Kind::TransformSample);
  c.matrix = m;
  c.i[0] = index;
  return c;
}
Call trail_pair_call(Raw w, std::uint32_t handle, const Vec3 &tip, const Vec3 &base) {
  Call c = make(Kind::TrailPair);
  c.f[0] = w;
  c.i[0] = handle;
  c.v = {tip, base};
  return c;
}

// ---- vector helpers ----------------------------------------------------------------------
Vec3 scaled(const Vec3 &u, Raw k) { return {mul(u[0], k), mul(u[1], k), mul(u[2], k)}; }
Raw dot_self(const Vec3 &v) {
  return add(add(mul(v[0], v[0]), mul(v[1], v[1])), mul(v[2], v[2]));
}
Vec3 vsub(const Vec3 &a, const Vec3 &b) {
  return {sub(a[0], b[0]), sub(a[1], b[1]), sub(a[2], b[2])};
}
Vec3 vadd(const Vec3 &a, const Vec3 &b) {
  return {add(a[0], b[0]), add(a[1], b[1]), add(a[2], b[2])};
}
Raw length(const Vec3 &v) { return sim::sqrt(dot_self(v)); }
Vec3 norm_scaled(const Vec3 &v, Raw scale) {
  const Raw k = rsqrt(ONE, dot_self(v));
  return {mul(mul(v[0], k), scale), mul(mul(v[1], k), scale), mul(mul(v[2], k), scale)};
}
Vec3 row(const Mat34 &m, int first) { return {m[first], m[first + 1], m[first + 2]}; }

// p + (cos(yaw), sin(yaw), 0) * dist; z gets + 0.0 (so -0 becomes +0).
Vec3 offset_by_yaw(const Vec3 &p, std::int32_t yaw, Raw dist) {
  const auto angle = static_cast<std::uint32_t>(yaw);
  return {add(p[0], mul(cos16(angle), dist)), add(p[1], mul(sin16(angle), dist)),
          add(p[2], ZERO)};
}

// ---- fx_spawn helpers ------------------------------------------------------------------------
// centre = (pos - size/2) + float(draw % size) per axis, x then y then z; the z axis may use
// another size (z_size) or base (z_base) and a lift added after.
struct Jitter {
  std::uint32_t z_size = 0; // 0: size
  bool has_z_base = false;
  Raw z_base = 0;
  bool has_z_lift = false;
  Raw z_lift = 0;
};
Vec3 jitter_centre(Lcg &rng, const Vec3 &pos, std::uint32_t size, const Jitter &j = {}) {
  const Raw half = fi(size / 2);
  const Raw cx = add(sub(pos[0], half), fi(rng.draw() % size));
  const Raw cy = add(sub(pos[1], half), fi(rng.draw() % size));
  const std::uint32_t zs = j.z_size ? j.z_size : size;
  const Raw zb = j.has_z_base ? j.z_base : half;
  Raw cz = add(sub(pos[2], zb), fi(rng.draw() % zs));
  if (j.has_z_lift)
    cz = add(cz, j.z_lift);
  return {cx, cy, cz};
}
Jitter z_lift(Raw lift) {
  Jitter j;
  j.has_z_lift = true;
  j.z_lift = lift;
  return j;
}

// p = float(draw % size - size/2) + centre per axis (x, y, z).
Vec3 offset_draw(Lcg &rng, const Vec3 &centre, std::uint32_t size) {
  Vec3 p{};
  for (int k = 0; k < 3; ++k)
    p[k] = add(fi(std::int64_t(rng.draw() % size) - std::int64_t(size / 2)), centre[k]);
  return p;
}

// ids 0xd-0x10, 0x12: a random angle (the draw's top half, signed), a random radius
// r * 15 in x y, and (r' - 0.5) * 15 in z.
Vec3 point_within_15(Lcg &rng, const Vec3 &pos) {
  const auto angle = static_cast<std::uint32_t>(s16(rng.draw()));
  const Raw r = rng.unit();
  const Raw x = add(pos[0], mul(mul(cos16(angle), r), K_15));
  const Raw y = add(pos[1], mul(mul(sin16(angle), r), K_15));
  const Raw z = add(pos[2], mul(add(rng.unit(), K_MINUS_HALF), K_15));
  return {x, y, z};
}

// abs((int)a) as an unsigned magnitude: the integer abs() applied to a float (id 0x15).
std::uint32_t abs_int(Raw a) {
  const std::int32_t t = to_int(a);
  return t < 0 ? 0u - static_cast<std::uint32_t>(t) : static_cast<std::uint32_t>(t);
}
Raw abs_trunc(Raw a) { return from_int(abs_int(a)); }

std::uint32_t ask_pool(const PoolNew &pool_new, std::uint32_t size) {
  return pool_new ? pool_new(size) : 1;
}

// ---- text form -------------------------------------------------------------------------------
void put_int(std::string &out, std::int64_t value) {
  char buffer[24];
  if (value < 0)
    std::snprintf(buffer, sizeof buffer, " -%" PRIx64, static_cast<std::uint64_t>(-value));
  else
    std::snprintf(buffer, sizeof buffer, " %" PRIx64, static_cast<std::uint64_t>(value));
  out += buffer;
}
void put_u64(std::string &out, std::uint64_t value) {
  char buffer[24];
  std::snprintf(buffer, sizeof buffer, " %" PRIx64, value);
  out += buffer;
}
template <std::size_t N> void put_list(std::string &out, const std::array<std::uint32_t, N> &v) {
  out += " [";
  for (std::size_t k = 0; k < N; ++k) {
    char buffer[16];
    std::snprintf(buffer, sizeof buffer, "%s%" PRIx32, k ? "," : "", v[k]);
    out += buffer;
  }
  out += ']';
}
void put_name(std::string &out, const char *name) {
  out += ' ';
  out += name;
}

} // namespace

std::string Call::text() const {
  std::string out;
  switch (kind) {
  case Kind::Ctor:
    out = "ctor";
    put_int(out, i[0]);
    break;
  case Kind::Near:
    out = "near";
    put_list(out, v[0]);
    break;
  case Kind::Spawn:
    out = "spawn";
    put_int(out, f[0]);
    put_int(out, i[0]);
    put_list(out, v[0]);
    put_list(out, v[1]);
    break;
  case Kind::SoundName:
    out = "sound_name";
    put_int(out, i[0]);
    break;
  case Kind::ResFind:
    out = "res_find";
    put_name(out, s[0]);
    if (s[1])
      put_name(out, s[1]);
    break;
  case Kind::Sound:
    out = "sound";
    put_int(out, f[0]);
    put_list(out, v[0]);
    break;
  case Kind::PoolNew:
    out = "pool_new";
    put_int(out, i[0]);
    break;
  case Kind::Fire:
    out = "fire";
    put_list(out, v[0]);
    break;
  case Kind::Hide:
    out = "hide";
    break;
  case Kind::SampleCount:
    out = "sample_count";
    break;
  case Kind::Sample:
    out = "sample";
    put_int(out, f[0]);
    put_int(out, i[0]);
    put_list(out, v[0]);
    put_int(out, i[1]);
    put_int(out, i[2]);
    break;
  case Kind::PhProp:
    out = "phprop";
    put_int(out, f[0]);
    put_list(out, v[0]);
    put_name(out, s[0]);
    put_name(out, s[1]);
    put_name(out, s[2]);
    put_int(out, i[0]);
    put_int(out, i[1]);
    put_int(out, i[2]);
    break;
  case Kind::VTable:
    out = "vtable";
    put_int(out, i[0]);
    break;
  case Kind::Mask:
    out = "mask";
    put_int(out, i[0]);
    put_int(out, i[1]);
    break;
  case Kind::Attach:
    out = "attach";
    put_int(out, f[0]);
    put_int(out, i[0]);
    put_list(out, v[0]);
    put_int(out, i[1]);
    put_int(out, i[2]);
    put_int(out, i[3]);
    break;
  case Kind::Gut:
    out = "gut";
    put_int(out, f[0]);
    put_list(out, v[0]);
    for (int k = 0; k < 6; ++k)
      put_int(out, i[k]);
    put_u64(out, mask64);
    put_int(out, i[6]);
    put_int(out, i[7]);
    break;
  case Kind::Lift:
    out = "lift";
    break;
  case Kind::GetModel:
    out = "get_model";
    break;
  case Kind::BeamCreate:
    out = "beam_create";
    put_int(out, f[0]);
    put_int(out, f[1]);
    put_list(out, v[0]);
    put_list(out, v[1]);
    put_int(out, i[0]);
    put_int(out, i[1]);
    put_int(out, i[2]);
    break;
  case Kind::BeamUpdate:
    out = "beam_update";
    put_int(out, i[0]);
    put_list(out, v[0]);
    put_list(out, v[1]);
    put_int(out, i[1]);
    put_int(out, i[2]);
    break;
  case Kind::Glow:
    out = "glow";
    put_int(out, f[0]);
    put_int(out, f[1]);
    put_list(out, v[0]);
    put_list(out, rgba);
    put_int(out, i[0]);
    put_int(out, i[1]);
    put_int(out, i[2]);
    break;
  case Kind::DialogOpen:
    out = "dialog_open";
    break;
  case Kind::AttachMatrix:
    out = "attach_matrix";
    put_int(out, f[0]);
    put_int(out, i[0]);
    put_int(out, i[1]);
    break;
  case Kind::EffectSegment:
    out = "effect_segment";
    break;
  case Kind::TrailCreate:
    out = "trail_create";
    put_int(out, f[0]);
    for (int k = 0; k < 7; ++k)
      put_int(out, i[k]);
    break;
  case Kind::TrailPair:
    out = "trail_pair";
    put_int(out, f[0]);
    put_int(out, i[0]);
    put_list(out, v[0]);
    put_list(out, v[1]);
    break;
  case Kind::FxSpawn:
    out = "fx_spawn";
    put_int(out, i[0]);
    put_list(out, v[0]);
    break;
  case Kind::TransformSample:
    out = "transform_sample";
    put_list(out, matrix);
    put_int(out, i[0]);
    break;
  case Kind::BeamAlive:
    out = "beam_alive";
    put_int(out, i[0]);
    break;
  }
  return out;
}

std::string stream_text(const Calls &calls) {
  std::string out;
  for (std::size_t k = 0; k < calls.size(); ++k) {
    if (k)
      out += ';';
    out += calls[k].text();
  }
  return out;
}

// =======================================================================================
// fx_spawn 0x1ef270 (id, pos*): one-shot burst at pos
// =======================================================================================
Calls fx_spawn(std::int32_t id, const Vec3 &pos, Lcg &rng, Lcg &shared, bool near,
               std::int32_t gore, MadeFlags *made, const PoolNew &pool_new) {
  Calls calls;
  // 1. every id but 5 asks isNearCameraFocus480 0x1ebd90 first
  if (id != 5) {
    calls.push_back(near_call(pos));
    if (!near)
      return calls;
  }
  // 2. the 24 definitions made on first use (flags 0x4f87bc + 4 i)
  if (made)
    for (std::size_t k = 0; k < FX_SPAWN_DEFS.size(); ++k)
      if (!made->made[k]) {
        made->made[k] = true;
        calls.push_back(int_call(Kind::Ctor, FX_SPAWN_DEFS[k]));
      }
  // 3. ids outside 0 ... 0x16 do nothing
  if (id < 0 || id > FX_SPAWN_LAST_ID)
    return calls;
  const Vec3 zero{ZERO, ZERO, ZERO};
  auto spawn = [&calls](std::uint32_t d, const Vec3 &p, const Vec3 &v) {
    calls.push_back(spawn_call(ZERO, d, p, v));
  };

  switch (id) {
  case 0:
  case 1:
  case 9: {
    // 4. red blood (ids 0, 1) returns at gore 0
    if ((id == 0 || id == 1) && gore == 0)
      return calls;
    const Vec3 centre = jitter_centre(rng, pos, 12);
    const Vec3 u = rng.in_unit_sphere();
    const Raw k = rsqrt(ONE, dot_self(u));
    Vec3 d = scaled(scaled(u, k), K_34);
    d[2] = add(d[2], K_4);
    calls.push_back(int_call(Kind::SoundName, (rng.draw() & 3) + 1));
    calls.push_back(res_find_call(BLOOD_SOUND_LUMP));
    calls.push_back(sound_call(ONE, pos));
    const std::uint32_t defn = id == 0 ? DEF_BLOOD_RED : id == 1 ? DEF_BLOOD_RED_90 : 0x86FC38;
    for (int n = 0; n < 300; ++n) {
      const Vec3 b = rng.in_unit_sphere();
      const Raw r = rng.unit();
      const Vec3 t = scaled(d, r);
      Vec3 vel{};
      for (int a = 0; a < 3; ++a)
        vel[a] = add(mul(b[a], K_7_86), t[a]);
      rng.in_unit_sphere(); // drawn, then not used
      spawn(defn, centre, vel);
    }
    break;
  }
  case 2:
  case 8: {
    Jitter j;
    j.z_size = 36;
    j.has_z_base = true;
    j.z_base = K_18;
    const Vec3 centre = jitter_centre(rng, pos, 24, j);
    const int count = id == 2 ? 150 : 300;
    const std::uint32_t defn = id == 2 ? DEF_DUST : 0x86FE18;
    const std::uint32_t half = id == 2 ? 12 : 24;
    for (int n = 0; n < count; ++n) {
      const Vec3 vel = scaled(rng.in_unit_sphere(), K_10_45);
      spawn(defn, offset_draw(rng, centre, half), vel);
    }
    break;
  }
  case 3:
  case 6: {
    const Vec3 centre = jitter_centre(rng, pos, 12, z_lift(K_18));
    const Raw k = id == 6 ? K_16 : K_10_16;
    for (int n = 0; n < 150; ++n)
      spawn(0x86FCD8, centre, scaled(rng.in_unit_sphere(), k));
    break;
  }
  case 4: {
    const Vec3 centre = jitter_centre(rng, pos, 12, z_lift(K_18));
    for (int n = 0; n < 150; ++n) {
      const Vec3 vel = scaled(rng.in_unit_sphere(), K_16);
      spawn(0x86FD28, offset_draw(rng, centre, 24), vel);
    }
    break;
  }
  case 5: {
    // SHARED seed (g_randSeed 0x404464): 3 or 4 Fire objects
    const std::uint32_t n = 3 + (shared.draw() & 1);
    for (std::uint32_t k = 0; k < n; ++k) {
      const Raw x = add(sub(pos[0], K_6), fi(shared.draw() % 12));
      const Raw y = add(sub(pos[1], K_6), fi(shared.draw() % 12));
      const Raw z = add(pos[2], fi(shared.draw() % 18));
      calls.push_back(int_call(Kind::PoolNew, 0x50));
      if (ask_pool(pool_new, 0x50)) {
        Call c = make(Kind::Fire);
        c.v[0] = {x, y, z};
        calls.push_back(c);
      }
    }
    break;
  }
  case 7:
  case 0x14: {
    const std::uint32_t defn = id == 7 ? 0x86FD78 : 0x86FDC8;
    for (int n = 0; n < 3; ++n) {
      const Vec3 a = rng.in_unit_sphere();
      const Vec3 v = rng.in_unit_sphere();
      const Vec3 vel{mul(v[0], K_4_75), mul(v[1], K_4_75),
                     add(mul(mul(v[2], K_4_75), K_0_77), K_6_05)};
      const Vec3 p{add(mul(a[0], K_15_75), pos[0]), add(mul(a[1], K_15_75), pos[1]),
                   add(add(mul(mul(a[2], K_15_75), K_0_75), K_0_07), pos[2])};
      spawn(defn, p, vel);
    }
    break;
  }
  case 10: {
    Vec3 centre = jitter_centre(rng, pos, 10);
    for (int n = 0; n < 25; ++n) {
      const Vec3 vel = scaled(rng.in_unit_sphere(), K_5_45);
      spawn(0x86FE68, offset_draw(rng, centre, 24), vel);
    }
    centre = jitter_centre(rng, pos, 12, z_lift(K_18));
    for (int n = 0; n < 80; ++n) {
      const Vec3 vel = scaled(rng.in_unit_sphere(), K_20);
      spawn(0x86FEB8, offset_draw(rng, centre, 24), vel);
    }
    break;
  }
  case 0xB:
  case 0xC: {
    Vec3 centre = jitter_centre(rng, pos, 10);
    for (int n = 0; n < 15; ++n) {
      const Vec3 vel = scaled(rng.in_unit_sphere(), K_5_45);
      spawn(0x86FF08, offset_draw(rng, centre, 24), vel);
    }
    centre = jitter_centre(rng, pos, 12, z_lift(K_18));
    for (int n = 0; n < 20; ++n) {
      const Vec3 vel = scaled(rng.in_unit_sphere(), K_40);
      spawn(0x86FF58, offset_draw(rng, centre, 24), vel);
    }
    if (id == 0xB)
      for (int n = 0; n < 5; ++n) {
        const Vec3 u = rng.in_unit_sphere();
        const Vec3 p{add(mul(u[0], K_5), centre[0]), add(mul(u[1], K_5), centre[1]),
                     add(mul(u[2], K_5), centre[2])};
        const Vec3 v = rng.in_unit_sphere();
        const Vec3 vel{mul(v[0], K_21_6), mul(v[1], K_21_6), add(mul(v[2], K_21_6), K_1_633)};
        spawn(0x86FFA8, p, vel);
      }
    break;
  }
  case 0xD:
  case 0xE:
  case 0xF:
  case 0x10:
  case 0x12: {
    const std::uint32_t defn = id == 0xD    ? 0x86FFF8
                               : id == 0xE  ? 0x870048
                               : id == 0xF  ? 0x870098
                               : id == 0x10 ? 0x8700E8
                                            : 0x870138;
    spawn(defn, point_within_15(rng, pos), zero);
    break;
  }
  case 0x11:
    spawn(0x870188, pos, zero);
    break;
  case 0x13: {
    const Vec3 u = rng.in_unit_sphere();
    const Vec3 vel{mul(u[0], K_21_6), mul(u[1], K_21_6), add(mul(u[2], K_21_6), K_1_633)};
    spawn(0x8701D8, pos, vel);
    if (rng.draw() % 10 == 0)
      spawn(0x870228, pos, scaled(vel, K_QUARTER));
    break;
  }
  case 0x15: {
    // the vertical terms use the integer abs() of a float: 0 unless exactly -1
    for (int n = 0; n < 5; ++n) {
      const Vec3 u = rng.in_unit_sphere();
      const Vec3 p{add(pos[0], mul(u[0], K_36)), add(pos[1], mul(u[1], K_36)),
                   add(pos[2], mul(abs_trunc(u[2]), K_36))};
      const Vec3 v = rng.in_unit_sphere();
      const Vec3 vel{mul(v[0], K_12), mul(v[1], K_12), mul(abs_trunc(v[2]), K_12)};
      spawn(0x870278, p, vel);
    }
    for (int n = 0; n < 5; ++n) {
      const Vec3 u = rng.in_unit_sphere();
      const Vec3 p{add(pos[0], mul(u[0], K_6)), add(pos[1], mul(u[1], K_6)),
                   add(pos[2], mul(abs_trunc(u[2]), K_6))};
      const Vec3 v = rng.in_unit_sphere();
      const Raw vz = sim::d2f(sim::dmul(sim::i2d(abs_int(v[2])), D_HALF));
      const Vec3 vel{mul(v[0], K_24), mul(v[1], K_24), mul(vz, K_24)};
      spawn(0x8702C8, p, vel);
    }
    break;
  }
  case 0x16: {
    const struct {
      std::uint32_t defn;
      Raw k, kz;
    } rows[2] = {{0x870318, K_5, K_5}, {0x870368, K_1_28, K_24_72}};
    for (const auto &r : rows) {
      const Vec3 u = rng.in_unit_sphere();
      const Raw uz = lt(u[2], ZERO) ? sim::fabs(u[2]) : u[2];
      spawn(r.defn, pos, {mul(u[0], r.k), mul(u[1], r.k), mul(mul(uz, r.k), r.kz)});
    }
    break;
  }
  default:
    break;
  }
  return calls;
}

// =======================================================================================
// Deaths: disintegrate, ice shatter
// =======================================================================================

// fx_disintegrateModel 0x1f4340 (a0 obj, a1 model, a2 anim, a3 pos*, t0 yaw16, t1 alternate,
// f12 scale, f13 offset): hide the object, the sound, then one particle per 8.09e10 of the
// running total of size^3 over the sample points (the remainder carries over).
Calls disintegrate_model(SampleModel &model, const Vec3 &pos, std::uint32_t yaw16, Raw scale,
                         Raw offset, bool alternate, std::uint32_t anim, Lcg &cosmetic) {
  const std::int32_t yaw = s16(yaw16);
  Calls calls;
  calls.push_back(make(Kind::Hide));
  calls.push_back(res_find_call(BLOOD_SOUND_LUMP, DISINTEGRATE_SOUND));
  calls.push_back(sound_call(ONE, pos));
  const std::int32_t n = model.sample_count();
  calls.push_back(make(Kind::SampleCount));
  Raw total = ZERO;
  for (std::int32_t i = 0; i < n; ++i) {
    calls.push_back(sample_call(scale, anim, pos, yaw, i));
    const SamplePoint sp = model.sample_point(scale, anim, pos, yaw, i);
    const Raw size = sp.size;
    total = add(total, mul(mul(size, size), size));
    while (lt(ZERO, total)) {
      total = sub(total, K_8_09E10);
      const Raw f = mul(mul(size, K_1E_5), K_35_35);
      Vec3 p{};
      for (int k = 0; k < 3; ++k)
        p[k] = add(mul(add(cosmetic.unit(), K_MINUS_HALF), f), sp.point[k]);
      Vec3 vel{};
      for (int k = 0; k < 3; ++k)
        vel[k] = mul(add(cosmetic.unit(), K_MINUS_HALF), K_3_14);
      if (!eq(offset, ZERO))
        p = offset_by_yaw(p, yaw, offset);
      const Raw back = mul(cosmetic.unit(), K_0_4);
      calls.push_back(
          spawn_call(back, alternate ? DEF_DISINTEGRATE_ALT : DEF_DISINTEGRATE, p, vel));
    }
  }
  return calls;
}

// fx_shatterToLooseIce 0x1fb240 (f12 count, f13 scale, f14 offset, a0 model, a1 anim,
// a2 pos*, a3 yaw16): `count` LooseIceChunk props; SHARED seed, 1 + n draws.
Calls shatter_to_loose_ice(SampleModel &model, const Vec3 &pos, std::uint32_t yaw16, Raw count_f,
                           Raw scale, Raw offset, std::uint32_t anim, Lcg &shared_io,
                           const PoolNew &pool_new) {
  const std::int32_t yaw = s16(yaw16);
  std::int64_t count = to_int(count_f);
  Calls calls;
  calls.push_back(make(Kind::SampleCount));
  std::int64_t n = model.sample_count();
  if (count == 0)
    throw Refusal("ice shatter: a count of 0 makes the original divide by zero "
                  "(docs/effects.md 14.4)");
  Lcg shared = shared_io;
  const std::int64_t step = n / count; // toward zero
  calls.push_back(int_call(Kind::SoundName, shared.draw() % 3 + 1));
  calls.push_back(res_find_call(BLOOD_SOUND_LUMP));
  calls.push_back(sound_call(ONE, pos));
  if (step == 0 || count <= 0) {
    shared_io = shared;
    return calls;
  }
  std::int64_t index = 0;
  while (count > 0) {
    calls.push_back(make(Kind::SampleCount));
    n = model.sample_count();
    if (n <= 0)
      throw Refusal("ice shatter: the sample count changed to 0 between queries; the "
                    "modulo by it is not documented (docs/effects.md 14.4)");
    index = py_mod(index + step, n);
    calls.push_back(sample_call(scale, anim, pos, yaw, index));
    Vec3 point =
        model.sample_point(scale, anim, pos, yaw, static_cast<std::int32_t>(index)).point;
    const std::uint32_t k = shared.draw() % 10;
    const std::uint32_t part_mask = k == 0 ? 2 : (k < 3 ? 3 : 4);
    if (!eq(offset, ZERO))
      point = offset_by_yaw(point, yaw, offset);
    calls.push_back(int_call(Kind::PoolNew, 0x248));
    if (ask_pool(pool_new, 0x248)) {
      Call c = make(Kind::PhProp); // PhProp_ctor 0x155a0c
      c.f[0] = ONE;
      c.v[0] = point;
      c.s = {BLOOD_SOUND_LUMP, ICE_CHUNK_MODEL, ICE_CHUNK_TEXTURE};
      c.i[0] = part_mask;
      c.i[1] = ICE_CHUNK_LAUNCH_FLAGS;
      c.i[2] = 0;
      calls.push_back(c);
      calls.push_back(int_call(Kind::VTable, VT_LOOSE_ICE_CHUNK));
    }
    --count;
  }
  shared_io = shared;
  return calls;
}

// =======================================================================================
// Losing a body part: the eight callers of GutChunk_ctorFromProp 0x201ad0
// =======================================================================================
namespace {
enum class Order { MaskFirst, ChunkFirst };
struct PartRow {
  std::uint32_t address;
  const char *name;
  std::uint32_t launch, blood;
  bool lift;
  Order order;
};
const PartRow &part_row(PartClass cls) {
  static const PartRow rows[8] = {
      {0x236B0C, "Ant", 0xE, 2, true, Order::MaskFirst},
      {0x238360, "AntQueen?", 0xE, 0, true, Order::MaskFirst},
      {0x247130, "FireBeetle", 7, 2, true, Order::ChunkFirst},
      {0x2672F4, "Ghoul?", 7, 0, true, Order::ChunkFirst},
      {0x2982CC, "Mosquito", 0xE, 2, true, Order::MaskFirst},
      {0x2A3DE0, "Orc", 7, 0, true, Order::ChunkFirst},
      {0x2A59F0, "Scorpion", 0xE, 2, true, Order::MaskFirst},
      {0x2ACFD8, "BlackWidow", 0xE, 2, false, Order::MaskFirst},
  };
  return rows[static_cast<int>(cls)];
}

// part -> model-variant bit (the class's own table; 0 for a part it has no bit for)
std::uint32_t part_bit(PartClass cls, std::uint32_t part) {
  switch (cls) {
  case PartClass::Ant:
    return part < 8 ? 2u << part : 0;
  case PartClass::AntQueen:
    return 0;
  case PartClass::FireBeetle:
    return part >= 2 && part <= 7 ? 1u << (part - 1) : 0;
  case PartClass::Ghoul: {
    static const std::uint32_t bits[5] = {2, 4, 8, 0x10, 1};
    return part < 5 ? bits[part] : 0;
  }
  case PartClass::Mosquito: {
    static const std::uint32_t bits[8] = {0, 0x10, 0x10, 0x40, 2, 1, 4, 8};
    return part < 8 ? bits[part] : 0;
  }
  case PartClass::Orc:
    return part >= 1 && part <= 5 ? 0x10000000u >> (part - 1) : 0;
  case PartClass::Scorpion:
    return 1u << (part & 31);
  case PartClass::BlackWidow:
    return part < 12 ? 1u << part : 0;
  }
  return 0;
}

// part -> constant mesh mask of the chunk-first classes (0 for any other part)
std::uint64_t part_constant_mask(PartClass cls, std::uint32_t part) {
  switch (cls) {
  case PartClass::FireBeetle: {
    static const std::uint64_t masks[8] = {0, 0, 0x4, 0x6, 0x40, 0xC00, 0x80, 0x300};
    return part < 8 ? masks[part] : 0;
  }
  case PartClass::Ghoul: {
    static const std::uint64_t masks[5] = {0x8, 0x10, 0x20, 0x40, 0x2};
    return part < 5 ? masks[part] : 0;
  }
  case PartClass::Orc: {
    static const std::uint64_t masks[6] = {0, 0x20406, 0x800, 0x10, 0x1000, 0x200};
    return part < 6 ? masks[part] : 0;
  }
  default:
    return 0;
  }
}

// Orc: the attach point index per part (asked and not used)
std::int64_t orc_attach_index(std::uint32_t part) {
  static const std::int64_t index[6] = {-1, 8, 6, 7, 4, 5};
  return part >= 1 && part <= 5 ? index[part] : -1;
}
} // namespace

std::uint32_t part_class_address(PartClass cls) { return part_row(cls).address; }
const char *part_class_name(PartClass cls) { return part_row(cls).name; }

// Orc_severPart part 1 (0x2a3e30-0x2a3f28): the head chunk's mesh mask follows the head
// variants already set in +0xe8 (bits 0-6 and 8-11).
std::uint64_t orc_head_mask(std::uint32_t variant) {
  std::uint64_t m = variant & 1 ? 0x60406 : 0x20406;
  if (variant & 2)
    m = (m ^ 2) | 0x80000;
  static const std::uint32_t heads[5][2] = {
      {4, 0x300000}, {8, 0xC00000}, {0x10, 0x3000000}, {0x20, 0xC000000}, {0x40, 0x30000000}};
  for (const auto &h : heads)
    if (variant & h[0])
      m = (m ^ 6) | h[1];
  for (unsigned k = 0; k < 4; ++k)
    if (variant & (0x100u << k))
      m |= UINT64_C(0x80000000) << k; // bits 31-34 of the 64-bit mesh mask
  return m;
}

Calls lose_body_part(PartClass cls, Creature &c, std::uint32_t part, const ShowMask &show_mask,
                     const PoolNew &pool_new) {
  const PartRow &r = part_row(cls);
  const std::uint32_t model = c.cls[3]; // class data +0xc
  Calls calls;
  if (cls == PartClass::BlackWidow && c.kind == 1)
    return calls; // nothing while +0x414 is 1
  if (cls == PartClass::Orc && (part == 6 || part == 7))
    throw Refusal("Orc part 6/7 (drop a held item as a PhProp) is not modelled by the "
                  "reference and never passed by its caller (docs/effects.md 14.5)");
  const std::uint32_t bit = part_bit(cls, part);
  bool chunk = true;
  if (cls == PartClass::Orc)
    chunk = part >= 1 && part <= 5;
  else if (cls == PartClass::BlackWidow)
    chunk = !(part >= 8 && part <= 11);
  auto mask_call = [&calls, model](std::uint32_t variant) {
    Call m = make(Kind::Mask);
    m.i[0] = model;
    m.i[1] = variant;
    calls.push_back(m);
  };
  std::uint64_t mask64 = 0;
  if (r.order == Order::MaskFirst) {
    // the meshes the new variant bit just hid: old show mask AND NOT new show mask
    mask_call(c.variant);
    const std::uint64_t old = show_mask(model, c.variant);
    c.variant |= bit;
    mask_call(c.variant);
    c.show = show_mask(model, c.variant);
    mask64 = old & ~c.show;
  } else {
    mask64 = cls == PartClass::Orc && part == 1 ? orc_head_mask(c.variant)
                                                : part_constant_mask(cls, part);
    c.variant |= bit;
    const std::int64_t attach = cls == PartClass::Orc ? orc_attach_index(part) : -1;
    if (attach >= 0) { // model_getAttachPoint 0x1754a0 at yaw - 0x4001, result unused
      Call a = make(Kind::Attach);
      a.f[0] = ONE;
      a.i[0] = model;
      a.v[0] = c.pos;
      a.i[1] = s16(c.yaw - 0x4001);
      a.i[2] = c.anim;
      a.i[3] = attach;
      calls.push_back(a);
    }
  }
  if (chunk) {
    calls.push_back(int_call(Kind::PoolNew, 0x268));
    if (ask_pool(pool_new, 0x268)) {
      const std::uint32_t tex =
          cls == PartClass::Ant && c.colour == 1 ? c.cls[6] : c.cls[4]; // +0x18 / +0x10
      Call g = make(Kind::Gut);
      g.f[0] = cls == PartClass::BlackWidow ? c.scale : ONE;
      g.v[0] = c.pos;
      g.i[0] = s16(c.yaw);
      g.i[1] = model;
      g.i[2] = c.anim;
      g.i[3] = c.cls[0];
      g.i[4] = c.cls[2];
      g.i[5] = tex;
      g.mask64 = mask64;
      g.i[6] = r.launch;
      g.i[7] = r.blood;
      calls.push_back(g);
      if (r.lift)
        calls.push_back(make(Kind::Lift)); // chunk +0x1c4 += 2.0
    }
  }
  if (r.order == Order::ChunkFirst) {
    mask_call(c.variant);
    c.show = show_mask(model, c.variant);
  }
  return calls;
}

// =======================================================================================
// Particles from model sample points, model arcs, model glow
// =======================================================================================

// fx_emitFromModelVerts 0x1eb1e8 (f12 speed, a0 obj, a1 count, a2 def, a3 dir* or 0,
// t0 offset* or 0).
Calls emit_from_model_verts(SampleModel &model, const Vec3 &pos, std::uint32_t yaw16,
                            std::uint32_t anim, std::int32_t count, std::uint32_t definition,
                            const Vec3 *direction, const Vec3 *offset, Raw speed,
                            Lcg &cosmetic) {
  Calls calls;
  calls.push_back(make(Kind::GetModel));
  calls.push_back(make(Kind::SampleCount));
  const std::int64_t n = model.sample_count();
  const std::int32_t yaw = s16(yaw16);
  if (count > 0 && n == 0)
    throw Refusal("emit from model verts: a model without sample points makes the original "
                  "divide by zero (docs/effects.md 14.7)");
  for (; count > 0; --count) {
    const Vec3 u = direction ? *direction : cosmetic.in_unit_sphere();
    const Vec3 vel = scaled(u, speed);
    const std::int64_t index = py_mod(cosmetic.draw(), n);
    calls.push_back(sample_call(ONE, anim, pos, yaw, index));
    Vec3 p = model.sample_point(ONE, anim, pos, yaw, static_cast<std::int32_t>(index)).point;
    if (offset)
      p = vadd(p, *offset);
    calls.push_back(spawn_call(ZERO, definition, p, vel));
  }
  return calls;
}

// fx_burnModel 0x1f5210: two passes over random sample points, camera gated.
Calls model_sparkle(SampleModel *model, const Vec3 &pos, std::uint32_t yaw16, std::uint32_t anim,
                    Raw amount, Raw offset, Lcg &rng, bool near) {
  Calls calls{near_call(pos)};
  if (!near || !model)
    return calls;
  calls.push_back(make(Kind::SampleCount));
  const std::int64_t n = model->sample_count();
  if (n == 0)
    return calls;
  const std::int32_t yaw = s16(yaw16);
  const auto angle = static_cast<std::uint32_t>(yaw);
  const struct {
    Raw k_count;
    std::uint32_t defn;
    Raw speed;
  } passes[2] = {{K_16_59, 0x870728, K_10}, {K_3_76, 0x870778, K_9}};
  for (const auto &pass : passes) {
    std::int32_t count = sim::round_half_away(mul(amount, pass.k_count));
    for (; count > 0; --count) {
      const std::int64_t index = py_mod(rng.draw(), n);
      calls.push_back(sample_call(amount, anim, pos, yaw, index));
      const SamplePoint sp =
          model->sample_point(amount, anim, pos, yaw, static_cast<std::int32_t>(index));
      const Raw ox = mul(cos16(angle), offset), oy = mul(sin16(angle), offset);
      Vec3 p{add(sp.point[0], ox), add(sp.point[1], oy), add(sp.point[2], ZERO)};
      Raw volume = mul(sp.size, sp.size);
      while (lt(ZERO, volume)) {
        // a random walk: p is NOT reset between particles
        const Raw f = mul(mul(sp.size, K_1_00597), K_1E_4);
        Vec3 d{};
        for (int k = 0; k < 3; ++k)
          d[k] = mul(sub(rng.unit(), HALF), f);
        p = vadd(p, d);
        const Vec3 vel = scaled(rng.in_unit_sphere(), pass.speed);
        const Raw back = mul(rng.unit(), K_0_3);
        calls.push_back(spawn_call(back, pass.defn, p, vel));
        volume = sub(volume, K_1_145E8);
      }
    }
  }
  return calls;
}

// fx_shockModel 0x1f61c0: three lightning arcs between pairs of sample points.
Calls model_arcs(SampleModel *model, const Vec3 &pos, std::uint32_t yaw16, std::uint32_t anim,
                 Raw alpha, Raw scale, Raw offset, ArcState &state, std::uint32_t rgba, Lcg &rng,
                 bool near, const ArcBeamCreate &beam_create) {
  Calls calls{near_call(pos)};
  if (!near || !model)
    return calls;
  calls.push_back(make(Kind::SampleCount));
  const std::int64_t n = model->sample_count();
  if (n == 0)
    return calls;
  if (lt(ONE, alpha))
    alpha = ONE;
  const std::int32_t yaw = s16(yaw16);
  const auto angle = static_cast<std::uint32_t>(yaw);
  auto ends = [&](std::int32_t ia, std::int32_t ib) {
    std::array<Vec3, 2> out{};
    const std::int32_t indices[2] = {ia, ib};
    for (int e = 0; e < 2; ++e) {
      calls.push_back(sample_call(scale, anim, pos, yaw, indices[e]));
      out[e] = model->sample_point(scale, anim, pos, yaw, indices[e]).point;
    }
    if (!eq(offset, ZERO)) {
      const Raw ox = mul(cos16(angle), offset), oy = mul(sin16(angle), offset);
      for (auto &p : out)
        p = {add(p[0], ox), add(p[1], oy), add(p[2], ZERO)};
    }
    return out;
  };
  for (Arc &arc : state.arcs) {
    const bool jump = arc.beam == 0 || rng.draw() < 20000;
    if (jump) {
      arc.beam = 0;
      arc.a = s16(static_cast<std::uint32_t>(py_mod(rng.draw(), n)));
      arc.b = s16(static_cast<std::uint32_t>(py_mod(rng.draw(), n)));
      const auto e = ends(arc.a, arc.b);
      calls.push_back(beam_create_call(K_7_78, K_4_47, e[0], e[1], 0xCF, 4, rgba));
      arc.beam = beam_create ? beam_create(e[0], e[1]) : 0;
    } else {
      if (n < arc.a)
        arc.a = 0;
      if (n < arc.b)
        arc.b = 0;
      const auto e = ends(arc.a, arc.b);
      calls.push_back(beam_update_call(arc.beam, e[0], e[1], to_int(mul(alpha, K_94)), M32));
      for (const Vec3 &p : e) { // a spark at each end: the fx_spawn id 0x13 recipe
        const Vec3 u = rng.in_unit_sphere();
        const Vec3 vel{mul(u[0], K_21_6), mul(u[1], K_21_6), add(mul(u[2], K_21_6), K_1_633)};
        calls.push_back(spawn_call(ZERO, DEF_ARC_SPARK, p, vel));
        if (rng.draw() % 10 == 0)
          calls.push_back(spawn_call(ZERO, DEF_ARC_SPARK_SLOW, p, scaled(vel, K_QUARTER)));
      }
    }
  }
  return calls;
}

// fx_drawModelGlowNearCamera 0x1f5890: per rendered frame, one glow per sample point.
Calls model_glow(SampleModel *model, const std::uint32_t *obj_flags, GlowRamp &ramp,
                 const Vec3 &pos, std::uint32_t yaw16, std::uint32_t anim, Raw scale, Raw offset,
                 Raw fade, const std::array<std::uint32_t, 4> &target_a,
                 const std::array<std::uint32_t, 4> &target_b, std::int32_t mode,
                 std::int32_t size_add, std::uint32_t sim_frame, bool near) {
  Calls calls{near_call(pos)};
  if (!near || !model)
    return calls;
  calls.push_back(make(Kind::SampleCount));
  const std::int32_t n = model->sample_count();
  if (n == 0)
    return calls;
  const std::int32_t yaw = s16(yaw16);
  const auto angle = static_cast<std::uint32_t>(yaw);
  const Vec3 p{add(pos[0], mul(cos16(angle), offset)), add(pos[1], mul(sin16(angle), offset)),
               pos[2]};
  if (!obj_flags || !(*obj_flags & 0x20))
    return calls;
  std::array<Raw, 4> fa{}, fb{};
  for (int k = 0; k < 4; ++k) {
    fa[k] = fi(s16(target_a[k]));
    fb[k] = fi(s16(target_b[k]));
  }
  if (mode == 0) { // ramp up toward the targets
    for (int k = 0; k < 4; ++k) {
      const Raw step = mul(fade, K_60);
      ramp.a[k] = add(ramp.a[k], div(fa[k], step));
      ramp.b[k] = add(ramp.b[k], div(fb[k], step));
      if (lt(fa[k], ramp.a[k]))
        ramp.a[k] = fa[k];
      if (lt(fb[k], ramp.b[k]))
        ramp.b[k] = fb[k];
    }
  } else if (mode == 2) { // ramp down toward 0
    for (int k = 0; k < 4; ++k) {
      const Raw step = mul(fade, K_60);
      ramp.a[k] = sub(ramp.a[k], div(fa[k], step));
      ramp.b[k] = sub(ramp.b[k], div(fb[k], step));
      if (lt(ramp.a[k], ZERO))
        ramp.a[k] = ZERO;
      if (lt(ramp.b[k], ZERO))
        ramp.b[k] = ZERO;
    }
  }
  for (std::int32_t i = 0; i < n; ++i) {
    calls.push_back(sample_call(scale, anim, p, yaw, i));
    const Vec3 point = model->sample_point(scale, anim, p, yaw, i).point;
    const Raw size = add(sub(add(mul(sin16((sim_frame << 10) & 0xFFFF), K_250), K_10000), K_2013),
                         fi(size_add));
    std::array<std::uint32_t, 4> rgba{};
    for (int k = 0; k < 4; ++k)
      rgba[k] = static_cast<std::uint32_t>(to_int(ramp.b[k]));
    calls.push_back(
        glow_call(K_GLOW_BIAS, size, point, rgba, GLOW_TEXTURE_HALO2, s16(sim_frame << 6), 3));
  }
  return calls;
}

// =======================================================================================
// Weapon and status visuals: fx_modelStatusEffects 0x1f6ac8
// =======================================================================================

std::int32_t WeaponItem::value(std::int32_t code) const {
  for (const auto &e : entries)
    if (s16(e[0]) == code)
      return s16(e[1]);
  return 0;
}

namespace {
constexpr std::uint32_t spark_def(std::int32_t kind) {
  return kind == 1 ? 0x870BD8 : kind == 2 ? 0x870C28 : kind == 3 ? 0x870C78 : 0;
}

// One fx_modelStatusEffects call: the working copies of the caller's objects.
struct StatusRun {
  Calls &calls;
  WeaponModel &model;
  Lcg &rng;
  std::int64_t n_samples = 0;

  void sample_count() {
    calls.push_back(make(Kind::SampleCount));
    n_samples = model.sample_count();
  }
  std::int64_t random_index() {
    const std::uint32_t d = rng.draw();
    if (n_samples == 0)
      throw Refusal("weapon status: a weapon model without sample points makes the original "
                    "divide by zero (docs/effects.md 14.6)");
    return py_mod(d, n_samples);
  }

  // Particles at random sample points of the weapon, the matrix walking from last frame's
  // toward this frame's by `step` per particle; emit(p, back) adds the particle calls.
  template <class Emit>
  std::pair<Mat34, Raw> walk(Mat34 mi, const Mat34 &step, std::int64_t index_count, Raw back,
                             Raw back_step, Emit emit) {
    for (std::int64_t k = 0; k < index_count; ++k) {
      const std::int64_t index = random_index();
      calls.push_back(transform_sample_call(mi, index));
      const SamplePoint sp = model.transform_sample_point(mi, static_cast<std::int32_t>(index));
      const Raw f = mul(mul(sp.size, K_1E_5), K_35_35);
      Vec3 p = sp.point;
      for (int i = 0; i < 3; ++i)
        p[i] = add(p[i], mul(sub(rng.unit(), HALF), f));
      emit(p, back);
      for (int i = 0; i < 12; ++i)
        mi[i] = add(mi[i], step[i]);
      back = add(back, back_step);
    }
    return {mi, back};
  }

  // The ribbon of particles along the blade between last frame and this one (cold, poison
  // and disease while swinging).
  void swing_trail(const Mat34 &m, const Mat34 &s, Raw clamp_e, Raw k_count,
                   std::uint32_t defn, Raw k_speed, bool doubled) {
    Vec3 t = row(m, 9), r = row(m, 3);
    const Vec3 tp = row(s, 9), rp = row(s, 3);
    Vec3 a{}, b{};
    for (int i = 0; i < 3; ++i) {
      a[i] = add(t[i], mul(r[i], K_36));
      b[i] = add(tp[i], mul(rp[i], K_36));
    }
    Raw d = length(vsub(a, b));
    if (lt(K_30, d))
      d = K_30;
    const std::int64_t whole = to_int(mul(d, k_count));
    const std::int64_t n = doubled ? whole * 2 : whole;
    const Raw e = mul(d, clamp_e);
    const Raw k = div(ONE, fi(whole));
    Vec3 st{}, sr{};
    for (int i = 0; i < 3; ++i) {
      st[i] = mul(sub(tp[i], t[i]), k);
      sr[i] = mul(sub(rp[i], r[i]), k);
    }
    for (std::int64_t q = 0; q < n; ++q) {
      const Raw rr = add(mul(mul(from_int(rng.draw()), TWO_M16), K_30), K_6);
      const Raw f = mul(mul(e, from_int(rng.draw())), TWO_M16);
      Vec3 p{};
      for (int i = 0; i < 3; ++i)
        p[i] = add(t[i], mul(r[i], rr));
      const Vec3 u = rng.in_unit_sphere();
      Vec3 vel{};
      for (int i = 0; i < 3; ++i)
        vel[i] = add(mul(r[i], f), mul(u[i], k_speed));
      calls.push_back(spawn_call(ZERO, defn, p, vel));
      t = vadd(t, st);
      r = vadd(r, sr);
    }
  }
};

// |M (0, 0, 36) - S (0, 0, 36)|, clamped
Raw motion(const Mat34 &m, const Mat34 &s, Raw clamp) {
  const Vec3 up{ZERO, ZERO, K_36};
  Raw d = length(vsub(sim::mat34_transform_point(m, up), sim::mat34_transform_point(s, up)));
  if (lt(clamp, d))
    d = clamp;
  return d;
}

Mat34 matrix_step(const Mat34 &m, const Mat34 &s, Raw k) {
  Mat34 step{};
  for (int i = 0; i < 12; ++i)
    step[i] = mul(sub(m[i], s[i]), k);
  return step;
}

bool any_nonzero(const std::array<std::int32_t, 4> &v) {
  return v[0] || v[1] || v[2] || v[3];
}

// Glow, the colour fade and the five elements; updates state. Returns nothing: every path
// of the original goes through here.
void status_finish(StatusRun &run, Raw amount, WeaponItem &item, WeaponFxState &state,
                   const Mat34 &m, const Mat34 &s, bool swinging, const WeaponContext &ctx) {
  Calls &calls = run.calls;
  Lcg &rng = run.rng;
  const std::uint32_t sim_frame = ctx.sim_frame;
  // glow: one glow_add per weapon sample point (through M)
  if (any_nonzero(item.glow)) {
    calls.push_back(make(Kind::SampleCount));
    const std::int32_t n = run.model.sample_count();
    for (std::int32_t i = 0; i < n; ++i) {
      calls.push_back(transform_sample_call(m, i));
      const Vec3 p = run.model.transform_sample_point(m, i).point;
      const Raw size = sub(add(mul(sin16((sim_frame << 10) & 0xFFFF), K_250), K_10000), K_2013);
      std::array<std::uint32_t, 4> rgba{};
      for (int k = 0; k < 4; ++k)
        rgba[k] = static_cast<std::uint32_t>(item.glow[k]);
      calls.push_back(
          glow_call(K_GLOW_BIAS, size, p, rgba, GLOW_TEXTURE_HALO2, s16(sim_frame << 6), 3));
    }
  }
  // both item colours (+0x18 then +0x10) move 2 toward 0
  for (auto *arr : {&item.colour2, &item.glow})
    for (auto &v : *arr)
      v = v > 0 ? std::max(v - 2, 0) : (v < 0 ? std::min(v + 2, 0) : 0);

  // lightning 0x18
  if (item.value(CODE_LIGHTNING) == 0 || le(amount, ZERO)) {
    state.beam = 0;
  } else {
    const Raw d = motion(m, s, K_15);
    const Raw back = mul(d, K_0_00291);
    std::int64_t count =
        to_int(mul(fi(std::int64_t(to_int(mul(d, K_13_13))) + 1), amount));
    if (count < 1)
      count = 1;
    const Raw k = div(ONE, fi(count));
    const Mat34 step = matrix_step(m, s, k);
    run.sample_count();
    const std::int64_t loops = std::max<std::int64_t>(count / 4, 8);
    const Raw back_step = mul(k, K_1_24TH);
    run.walk(s, step, loops, back, back_step, [&](const Vec3 &p, Raw bk) {
      const Vec3 u = rng.in_unit_sphere();
      const Vec3 vel{add(ZERO, mul(u[0], K_3)), add(ZERO, mul(u[1], K_3)),
                     add(K_19_74, mul(u[2], K_3))};
      calls.push_back(spawn_call(bk, 0x870958, p, vel));
    });
    std::array<Vec3, 2> ends{};
    for (auto &e : ends) {
      const std::int64_t index = run.random_index();
      calls.push_back(transform_sample_call(m, index));
      e = run.model.transform_sample_point(m, static_cast<std::int32_t>(index)).point;
    }
    bool alive = false;
    if (state.beam != 0) {
      calls.push_back(int_call(Kind::BeamAlive, state.beam));
      alive = ctx.answers.beam_alive != 0;
    }
    if (state.beam == 0 || !alive) {
      calls.push_back(beam_create_call(K_7_78, K_2_47, ends[0], ends[1], 0x6B, 4, ARC_RGBA));
      state.beam = ctx.answers.beam_create;
    } else {
      calls.push_back(
          beam_update_call(state.beam, ends[0], ends[1], to_int(mul(amount, K_94)), M32));
    }
  }
  // cold 0x16
  if (item.value(CODE_COLD) != 0) {
    const Raw d = motion(m, s, K_20);
    const Raw back = mul(d, K_0_00191);
    const std::int64_t count =
        to_int(mul(fi(std::int64_t(to_int(mul(d, K_3_13))) + 0x1A), amount));
    const Raw k = div(ONE, fi(count));
    const Mat34 step = matrix_step(m, s, k);
    run.sample_count();
    run.walk(s, step, count, back, mul(k, K_1_24TH), [&](const Vec3 &p, Raw bk) {
      calls.push_back(spawn_call(bk, 0x870AE8, p, {ZERO, ZERO, ZERO}));
    });
    if (swinging) {
      run.swing_trail(m, s, K_0_97, K_0_229, 0x870B38, K_2_83, true);
      run.swing_trail(m, s, K_0_97, K_0_229, 0x870B88, K_2_83, false);
    }
  }
  // fire 0x15
  if (item.value(CODE_FIRE) != 0) {
    const Raw d = motion(m, s, K_20);
    const Raw back = mul(d, K_0_00191);
    const std::int64_t count =
        to_int(mul(fi(std::int64_t(to_int(mul(d, K_3_13))) + 0x10), amount));
    const Raw k = div(ONE, fi(count));
    const Mat34 step = matrix_step(m, s, k);
    run.sample_count();
    std::uint32_t counter = 0;
    run.walk(s, step, count, back, mul(k, K_1_24TH), [&](const Vec3 &p, Raw bk) {
      const Vec3 u = rng.in_unit_sphere();
      const Vec3 vel{add(mul(u[0], K_6_589), ZERO), add(mul(u[1], K_6_589), ZERO),
                     add(mul(u[2], K_6_589), K_19_74)};
      calls.push_back(spawn_call(bk, 0x870A48, p, vel));
      if ((counter & 0xF) == 0) { // every 16th particle
        calls.push_back(spawn_call(bk, 0x8709F8, p, vel));
        const Vec3 p2{p[0], p[1], add(p[2], K_6)};
        const Vec3 w = rng.in_unit_sphere();
        Vec3 vel2{};
        for (int i = 0; i < 3; ++i)
          vel2[i] = add(vel[i], mul(w[i], K_3_5));
        calls.push_back(spawn_call(bk, 0x870A98, p2, vel2));
      }
      ++counter;
    });
  }
  // poison 0x17: the motion term compares this frame's matrix with itself (0)
  if (item.value(CODE_POISON) != 0) {
    const Raw d = motion(m, m, K_30);
    const Raw back = mul(d, K_0_00291);
    const std::int64_t count = to_int(
        mul(mul(fi(std::int64_t(to_int(mul(d, K_13_13))) + 0x1A), amount), K_0_156));
    const Raw k = div(ONE, fi(count));
    const Mat34 step = matrix_step(m, s, k);
    run.sample_count();
    run.walk(s, step, count, back, mul(k, K_1_24TH), [&](const Vec3 &p, Raw bk) {
      calls.push_back(spawn_call(bk, 0x870CC8, p, {ZERO, ZERO, ZERO}));
    });
    if (swinging)
      run.swing_trail(m, s, K_0_667, K_0_206, 0x870D18, K_6_388, true);
  }
  // disease 0x26 (the same motion rule as poison)
  if (item.value(CODE_DISEASE) != 0) {
    const Raw d = motion(m, m, K_30);
    const Raw back0 = mul(d, K_0_00291);
    const std::int64_t count = to_int(
        mul(mul(fi(std::int64_t(to_int(mul(d, K_13_13))) + 0x1A), amount), K_0_578));
    const Raw fcount = fi(count);
    const Raw k = div(ONE, fcount);
    const Mat34 step = matrix_step(m, s, k);
    run.sample_count();
    const Raw back_step = mul(k, K_1_24TH);
    const auto walked = run.walk(s, step, count, back0, back_step, [&](const Vec3 &p, Raw bk) {
      calls.push_back(spawn_call(bk, 0x870D68, p, {ZERO, ZERO, ZERO}));
    });
    // the back-date restarts, the matrix continues
    run.walk(walked.first, step, to_int(mul(fcount, K_0_074)), back0, back_step,
             [&](const Vec3 &p, Raw bk) {
               const Vec3 u = rng.in_unit_sphere();
               calls.push_back(spawn_call(bk, 0x870E08, p, scaled(u, K_3_974)));
             });
    if (!swinging) {
      state.matrix = m;
      return;
    }
    run.swing_trail(m, s, K_0_556, K_0_785, 0x870DB8, K_3_974, true);
  }
  if (swinging)
    state.frame = sim_frame;
  state.matrix = m;
}
} // namespace

Calls model_status_effects(Raw amount, Raw scale, WeaponItem &item_io, WeaponModel *model,
                           WeaponFxState &state_io, const Vec3 &pos, std::uint32_t yaw16,
                           HolderModel *holder, std::uint32_t anim, bool swinging,
                           std::int32_t joint_add, const Mat34 *override_matrix, Lcg &rng_io,
                           const WeaponContext &ctx) {
  Calls calls;
  // 1. level flag 0x10000000, no weapon model, or an open dialog: nothing
  if ((ctx.level_flags & LEVEL_FLAG_NO_STATUS_FX) || !model)
    return calls;
  calls.push_back(make(Kind::DialogOpen));
  if (ctx.dialog_open)
    return calls;
  // Work on copies; a refusal leaves the caller's objects as they were.
  WeaponItem item = item_io;
  WeaponFxState state = state_io;
  Lcg rng = rng_io;

  if (!swinging)
    state.counter = 0;
  else if (state.counter < 100)
    state.counter += 1;
  // 2. the weapon matrix M
  const bool gated = swinging || item.value(CODE_LIGHTNING) || item.value(CODE_COLD) ||
                     item.value(CODE_FIRE) || item.value(CODE_POISON) ||
                     item.value(CODE_DISEASE) || any_nonzero(item.glow);
  Mat34 m;
  if (!gated) {
    m = sim::mat34_set_identity();
  } else {
    if (holder) {
      const std::int32_t index =
          s32(static_cast<std::uint32_t>(std::int64_t(s8(model->attach_base())) + joint_add));
      Call a = make(Kind::AttachMatrix);
      a.f[0] = scale;
      a.i[0] = anim;
      a.i[1] = index;
      calls.push_back(a);
      m = holder->attach_matrix(scale, anim, index);
    } else {
      m = sim::mat34_set_identity();
    }
    m = sim::mat34_mul(m, sim::mat34_set_pos_rot_z(
                              pos, static_cast<std::uint32_t>(s16(yaw16 + 0x4000))));
  }
  if (override_matrix)
    m = *override_matrix;
  // 3. a stored matrix with 1.0 on its diagonal is replaced first (O 0x1f6e70-0x1f6f40)
  Mat34 s = state.matrix;
  if (eq(s[0], ONE) && eq(s[4], ONE) && eq(s[8], ONE)) {
    const bool copy = eq(s[9], s[10]) ? !eq(s[11], ONE) : !eq(s[11], ZERO);
    if (copy) {
      state.matrix = m;
      s = m;
    }
  }
  const Vec3 cur_r1 = row(m, 3), cur_t = row(m, 9);
  const Vec3 prev_r1 = row(s, 3), prev_t = row(s, 9);
  StatusRun run{calls, *model, rng};

  if (!(ctx.level_flags & LEVEL_FLAG_NO_SWING_TRAIL)) {
    // 4. the swing trail and hit sparks
    if (!swinging || override_matrix) {
      state.trail = 0;
      state.spark_kind = state.sparks = 0;
    } else {
      const Vec3 cd = norm_scaled(cur_r1, scale);
      const Vec3 pd = norm_scaled(prev_r1, scale);
      calls.push_back(make(Kind::EffectSegment));
      const auto seg = model->effect_segment();
      const Raw seg_a = seg[0][1], seg_b = seg[1][1];
      Vec3 base{}, tip{};
      for (int i = 0; i < 3; ++i) {
        base[i] = add(cur_t[i], mul(cd[i], seg_a));
        tip[i] = add(cur_t[i], mul(cd[i], seg_b));
      }
      Raw w = mul(length(vsub(cd, pd)), K_0_68);
      w = sim::fmax(w, ZERO);
      w = sim::fmin(w, ONE);
      w = mul(w, K_0_3);
      w = sim::fmin(w, mul(fi(s8(state.counter)), K_0_05));
      if (state.trail == 0 || state.frame != ctx.sim_frame - 1u) {
        Call t = make(Kind::TrailCreate);
        t.f[0] = K_0_2;
        const std::int64_t args[7] = {3, 1000, 2, 0x80, 0x80, 0x80, 0};
        for (int k = 0; k < 7; ++k)
          t.i[k] = args[k];
        calls.push_back(t);
        state.trail = ctx.answers.trail_create;
      } else {
        // an in-between pair from the averaged matrix
        Vec3 mt{}, md{};
        for (int i = 0; i < 3; ++i) {
          mt[i] = mul(add(cur_t[i], prev_t[i]), HALF);
          md[i] = mul(add(cd[i], pd[i]), HALF);
        }
        md = norm_scaled(md, scale);
        Vec3 mbase{}, mtip{};
        for (int i = 0; i < 3; ++i) {
          mbase[i] = add(mt[i], mul(md[i], seg_a));
          mtip[i] = add(mt[i], mul(md[i], seg_b));
        }
        calls.push_back(trail_pair_call(w, state.trail, mtip, mbase));
      }
      calls.push_back(trail_pair_call(w, state.trail, tip, base));
      const std::int32_t c = s8(state.sparks);
      if (c != 0) {
        if (ctx.gore > 0) {
          Vec3 d = vsub(tip, base);
          const Raw k = rsqrt(ONE, dot_self(d));
          Vec3 dn2{}, prev_tip{};
          for (int i = 0; i < 3; ++i) {
            dn2[i] = add(mul(d[i], k), mul(d[i], k));
            prev_tip[i] = add(prev_t[i], mul(pd[i], seg_b));
          }
          for (std::int32_t i = 0; i < std::max(c, 0); ++i) {
            d = vsub(tip, base);
            Vec3 q{}, start{}, step{};
            for (int j = 0; j < 3; ++j)
              q[j] = sub(tip[j], mul(d[j], K_0_7));
            const Raw f = fi(c - i);
            for (int j = 0; j < 3; ++j)
              start[j] = sub(q[j], mul(dn2[j], f));
            const std::int32_t n2 = 2 * i;
            const Raw fn = fi(n2);
            for (int j = 0; j < 3; ++j)
              step[j] = div(sub(prev_tip[j], tip[j]), fn);
            for (std::int32_t jj = 0; jj < n2; ++jj) {
              const Raw fj = fi(jj);
              Vec3 p{}, vel{};
              for (int t = 0; t < 3; ++t) {
                p[t] = add(start[t], mul(step[t], fj));
                vel[t] = mul(dn2[t], K_9);
              }
              const std::uint32_t defn = spark_def(s8(state.spark_kind));
              if (defn)
                calls.push_back(spawn_call(ZERO, defn, p, vel));
            }
          }
        }
        state.sparks = (c - 1) & 0xFF;
      }
    }
  } else {
    // level flag 0x100: sparks of fx_spawn 0x11 instead of the trail
    if (!swinging) {
      state.trail = 0;
      state.spark_kind = state.sparks = 0;
    } else {
      const Vec3 cd = norm_scaled(cur_r1, scale);
      const Vec3 pd = norm_scaled(prev_r1, scale);
      calls.push_back(make(Kind::EffectSegment));
      const Raw seg_b = model->effect_segment()[1][1];
      Vec3 tip{};
      for (int i = 0; i < 3; ++i)
        tip[i] = add(cur_t[i], mul(cd[i], seg_b));
      Raw v = length(vsub(cd, pd));
      v = add(v, v);
      if (lt(ONE, v))
        v = ONE;
      if (lt(v, ZERO))
        v = ZERO;
      const std::uint32_t r = rng.draw();
      // v is clamped to [0, 1], so the divisor is 1 ... 6
      const std::int64_t divisor = 6 - std::int64_t(to_int(mul(v, K_5)));
      if (std::int64_t(r) % divisor == 0) {
        Call fx = make(Kind::FxSpawn);
        fx.i[0] = 0x11;
        fx.v[0] = tip;
        calls.push_back(fx);
      }
    }
  }
  status_finish(run, amount, item, state, m, s, swinging, ctx);
  item_io = item;
  state_io = state;
  rng_io = rng;
  return calls;
}

} // namespace champions::rta::fx::recipes
