#include "champions/core_math.hpp"
#include <algorithm>

// Authored against champions-return-to-arms-re tools/ref_core.py (sections
// 2–8). Every arithmetic call is a distinct raw-pattern operation. No host
// float/libm.
namespace champions::simulation::rta {
namespace {
constexpr Raw Z = 0, H = 0x3f000000, O = 0x3f800000, T = 0x40000000;
constexpr Raw M = 0xbf800000, THREE = 0x40400000, SIX = 0x40c00000;
constexpr Raw SIGNBIT = 0x80000000, MAG = 0x7fffffff;
constexpr Raw K_HUGE = 0x7149f2c9, TINY = 0x0da2425f, PI = 0x40490fda,
              PI2 = 0x3fc90fda, PILO = 0x34222168;
Raw dot(Raw a, Raw b, Raw c, Raw d, Raw e, Raw f) {
  return add(add(mul(a, b), mul(c, d)), mul(e, f));
}
Raw dot(const Vec3 &a, const Vec3 &b) {
  return dot(a[0], b[0], a[1], b[1], a[2], b[2]);
}
Vec3 difference(const Vec3 &a, const Vec3 &b) {
  return {sub(a[0], b[0]), sub(a[1], b[1]), sub(a[2], b[2])};
}
Quat basis(Raw t) {
  const Raw t2 = mul(t, t), t3 = mul(t2, t), v = mul(t2, THREE);
  return {add(sub(add(t3, t3), v), O), add(mul(t3, 0xc0000000), v),
          add(sub(t3, add(t2, t2)), t), sub(t3, t2)};
}
Vec3 blend(const Quat &h, const Vec3 &a, const Vec3 &b, const Vec3 &c,
           const Vec3 &d) {
  Vec3 out{};
  for (unsigned i = 0; i < 3; ++i)
    out[i] = add(add(add(mul(a[i], h[0]), mul(b[i], h[1])), mul(c[i], h[2])),
                 mul(d[i], h[3]));
  return out;
}
Mat3 rotation(const Mat34 &m) {
  Mat3 a{};
  std::copy_n(m.begin(), 9, a.begin());
  return a;
}
Mat34 affine(const Mat3 &r, const Vec3 &t = {}) {
  Mat34 a{};
  std::copy(r.begin(), r.end(), a.begin());
  std::copy(t.begin(), t.end(), a.begin() + 9);
  return a;
}
Mat4 identity4() { return {O, 0, 0, 0, 0, O, 0, 0, 0, 0, O, 0, 0, 0, 0, O}; }
// Signed floor division by a power of two without implementation-defined
// shifts.
std::int32_t shift_signed(std::int32_t v, unsigned n) {
  const auto divisor = std::int64_t{1} << n;
  return static_cast<std::int32_t>(
      v >= 0 ? v / divisor : -((-std::int64_t(v) + divisor - 1) / divisor));
}
template <std::size_t N>
std::array<Raw, N> rotate(std::array<Raw, N> m, unsigned width, unsigned rows,
                          std::int32_t axis, std::uint32_t angle, bool post) {
  if (axis < 0 || axis > 2)
    return m;
  const unsigned first = axis == 0 ? 1 : 0, second = axis == 2 ? 1 : 2;
  const Raw c = cos16(angle), s = sin16(angle);
  for (unsigned k = 0; k < (post ? rows : width); ++k) {
    const unsigned a = post ? k * 3 + first : first * width + k,
                   b = post ? k * 3 + second : second * width + k;
    const Raw p = m[a], q = m[b];
    if ((axis == 1) == post) {
      m[a] = add(mul(c, p), mul(s, q));
      m[b] = add(mul(neg(s), p), mul(c, q));
    } else {
      m[a] = sub(mul(c, p), mul(s, q));
      m[b] = add(mul(s, p), mul(c, q));
    }
  }
  return m;
}
constexpr std::array<Raw, 4> AH{0x3eed6338, 0x3f490fda, 0x3f7b985d, 0x3fc90fda};
constexpr std::array<Raw, 4> AL{0x31ac3769, 0x33222167, 0x33140fb4, 0x33a22168};
constexpr std::array<Raw, 11> AT{0x3eaaaaab, 0xbe4ccccc, 0x3e124924, 0xbde38e38,
                                 0x3dba2e6e, 0xbd9d8795, 0x3d886b34, 0xbd6ef16a,
                                 0x3d4bda59, 0xbd15a221, 0x3c8569d7};
constexpr Raw BP[2]{O, 0x3fc00000}, DH[2]{0, 0x3f15c000}, DL[2]{0, 0x35d1cfdc};
constexpr Raw PL[7]{0,          0x3f199999, 0x3edb6db7, 0x3eaaaaab,
                    0x3e8ba304, 0x3e6c3254, 0x3e53f142};
constexpr Raw PP[6]{0,          0x3e2aaaaa, 0xbb360b60,
                    0x388ab354, 0xb5ddea0e, 0x3331bb4b};
constexpr Raw THIRD = 0x3eaaaaaa, IV = 0x3fb8aa3a, IVH = 0x3fb8aa00,
              IVL = 0x36eca56f;
constexpr Raw CP = 0x3f76384e, CPH = 0x3f763800, CPL = 0x369dc39f,
              LG = 0x3f317217, LGH = 0x3f317200, LGL = 0x35bfbe8c,
              OVT = 0x3338aa3b, HEAD = 0xfffff000;
} // namespace
Raw cos16(std::uint32_t angle) {
  std::int32_t a = s16(angle);
  Raw sign = O;
  if (((angle ^ (angle << 1)) & 0x8000) != 0) {
    sign = M;
    a = static_cast<std::int32_t>(angle & 0x7fff) -
        ((angle & 0x4000) ? 0x8000 : 0);
  }
  const Raw x = mul(from_int(static_cast<std::uint32_t>(a)), 0x38490fda),
            x2 = mul(x, x), x3 = mul(x, x2);
  Raw s = sub(x, mul(x3, 0x3e2aaa57));
  const Raw x5 = mul(x3, x2);
  s = add(s, mul(x5, 0x3c086b24));
  s = sub(s, mul(mul(x5, x2), 0x39458017));
  return mul(sign, sub(O, mul(add(s, s), s)));
}
Raw sin16(std::uint32_t a) { return cos16(a - 0x4000u); }
std::int32_t angle_from_radians(Raw x) {
  return s16(static_cast<std::uint32_t>(to_int(mul(x, 0x4622f983))));
}
std::int32_t angle_from_degrees(Raw x) {
  return s16(static_cast<std::uint32_t>(to_int(mul(x, 0x43360b60))));
}
std::int32_t atan2_16(Raw y, Raw x) { return angle_from_radians(atan2f(y, x)); }
std::pair<std::int32_t, std::uint32_t> angle_turn_dir(std::uint32_t a,
                                                      std::uint32_t b) {
  const auto d = s16(a - b);
  return {d > 0 ? -1 : 1, static_cast<std::uint32_t>(d < 0 ? -d : d)};
}
std::int32_t angle_approach(Raw rate, Raw lo, Raw hi, std::uint32_t cur,
                            std::uint32_t target) {
  const auto d = s16(cur - target);
  Raw step = mul(from_int(static_cast<std::uint32_t>(d < 0 ? -d : d)), rate);
  if (!eq(lo, M) && lt(step, lo))
    step = lo;
  if (!eq(hi, M) && lt(hi, step))
    step = hi;
  const auto amount = static_cast<std::uint32_t>(to_int(step));
  return s16(cur + (d > 0 ? 0u - amount : amount));
}
Raw hermite(Raw t, Raw p0, Raw p1, Raw m0, Raw m1) {
  const Raw t2 = mul(t, t), t3 = mul(t2, t), v = mul(t2, THREE);
  Raw out = add(mul(sub(add(t3, t3), v), p0), p0);
  out = add(out, mul(add(mul(t3, 0xc0000000), v), p1));
  out = add(out, mul(add(sub(t3, add(t2, t2)), t), m0));
  return add(out, mul(sub(t3, t2), m1));
}
std::uint32_t log2_floor(std::int64_t x) {
  std::uint32_t n = 0;
  while (x > 1) {
    x /= 2;
    ++n;
  }
  return n;
}
std::uint32_t color_scale_clamped(Raw f, std::uint32_t rgba) {
  std::uint32_t out = 0;
  for (unsigned i = 0; i < 32; i += 8) {
    const auto v = to_int(mul(from_int((rgba >> i) & 255), f));
    out |= static_cast<std::uint32_t>(std::clamp(v, 0, 255)) << i;
  }
  return out;
}
std::uint32_t color_from_hsv(Raw h, Raw s, Raw v) {
  const auto clamp = [](Raw x) { return lt(x, Z) ? Z : (lt(O, x) ? O : x); };
  const auto byte = [](Raw x) { return to_uint(mul(x, 0x437f0000)); };
  h = clamp(h);
  s = clamp(s);
  v = clamp(v);
  if (eq(s, Z)) {
    const Raw b = byte(v);
    return 0xff000000 | b | (b << 8) | (b << 16);
  }
  const Raw h6 = mul(h, SIX);
  const auto sector = to_int(h6);
  const Raw frac = sub(h6, from_int(static_cast<std::uint32_t>(sector))),
            p = mul(v, sub(O, s)), q = mul(v, sub(O, mul(s, frac))),
            t = mul(v, sub(O, mul(s, sub(O, frac))));
  const Vec3 table[6]{{v, t, p}, {q, v, p}, {p, v, t},
                      {p, q, v}, {t, p, v}, {v, p, q}};
  if (sector < 0 || sector > 5)
    return 0xff000000;
  return 0xff000000 | byte(table[sector][0]) | (byte(table[sector][1]) << 8) |
         (byte(table[sector][2]) << 16);
}
Vec3 vec3_hermite(Raw t, const Vec3 &a, const Vec3 &b, const Vec3 &c,
                  const Vec3 &d) {
  return blend(basis(t), a, b, c, d);
}
Vec3 vec3_hermite_derivative(Raw t, const Vec3 &a, const Vec3 &b, const Vec3 &c,
                             const Vec3 &d) {
  const Raw t2 = mul(t, t), s = mul(t, SIX), v = mul(t2, THREE);
  return blend({sub(mul(t2, SIX), s), add(mul(t2, 0xc0c00000), s),
                add(sub(v, mul(t, 0x40800000)), O), sub(v, add(t, t))},
               a, b, c, d);
}
Raw vec3_dot_delta(const Vec3 &a, const Vec3 &n, const Vec3 &b) {
  return dot(n, difference(b, a));
}
Raw vec2_dot_delta(const Vec2 &a, const Vec2 &n, const Vec2 &b) {
  return add(mul(n[0], sub(b[0], a[0])), mul(n[1], sub(b[1], a[1])));
}
Raw ray_plane_distance(const Vec3 &p, const Vec3 &d, const Vec3 &pp,
                       const Vec3 &n) {
  const Raw facing = dot(d, n), height = dot(n, difference(p, pp));
  if (le(Z, facing) || lt(height, Z))
    return M;
  const Vec3 scaled{mul(n[0], height), mul(n[1], height), mul(n[2], height)};
  const Raw den = neg(dot(scaled, d));
  return mul(height, lt(0x3a83126e, fabs(den)) ? div(height, den)
                                               : mul(height, 0x447a0001));
}
SegmentHit segment_hits_plane_near_edge(const Vec3 &a, const Vec3 &b,
                                        const Vec3 &n, const Vec3 &p1,
                                        const Vec3 &p0) {
  const Vec3 d = difference(p1, p0);
  if (le(Z, dot(d, n)))
    return {false, std::nullopt};
  const Raw num = dot(difference(a, p0), n);
  Raw den = dot(d, n);
  if (lt(Z, den) && lt(den, 0x3c23d70a))
    den = 0x3c23d70a;
  if (lt(den, Z) && lt(0xbc23d70a, den))
    den = 0xbc23d70a;
  const Raw t = div(num, den);
  if (lt(t, Z) || lt(O, t))
    return {false, std::nullopt};
  Vec3 point{};
  for (unsigned i = 0; i < 3; ++i)
    point[i] = add(p0[i], mul(d[i], t));
  const Vec3 edge = difference(a, b);
  const Raw length = sqrt(dot(edge, edge)), inv = div(O, length);
  const Vec3 unit{mul(edge[0], inv), mul(edge[1], inv), mul(edge[2], inv)};
  const Raw along = dot(unit, difference(point, b));
  return {!lt(along, 0xbe800000) && !lt(add(length, 0x3e800000), along), point};
}
BBox bbox_set_empty() {
  return {0x4b189680, 0x4b189680, 0x4b189680,
          0xcb189680, 0xcb189680, 0xcb189680};
}
BBox bbox_union(BBox a, const BBox &b) {
  for (unsigned i = 0; i < 3; ++i)
    if (lt(b[i], a[i]))
      a[i] = b[i];
  for (unsigned i = 3; i < 6; ++i)
    if (lt(a[i], b[i]))
      a[i] = b[i];
  return a;
}
BBox bbox_add_point(BBox a, const Vec3 &p) {
  for (unsigned i = 0; i < 3; ++i) {
    if (lt(p[i], a[i]))
      a[i] = p[i];
    if (lt(a[i + 3], p[i]))
      a[i + 3] = p[i];
  }
  return a;
}
bool bbox_contains_point(const BBox &b, const Vec3 &p) {
  for (unsigned i = 0; i < 3; ++i)
    if (!le(b[i], p[i]) || !le(p[i], b[i + 3]))
      return false;
  return true;
}
bool bbox_overlaps(const BBox &a, const BBox &b) {
  for (unsigned i = 0; i < 3; ++i)
    if (lt(a[i + 3], b[i]) || lt(b[i + 3], a[i]))
      return false;
  return true;
}
Mat3 mat3_set_identity() { return {O, 0, 0, 0, O, 0, 0, 0, O}; }
Mat34 mat34_set_identity() { return affine(mat3_set_identity()); }
Mat34 mat34_set_translation(const Vec3 &p) {
  return affine(mat3_set_identity(), p);
}
Mat3 mat3_mul(const Mat3 &a, const Mat3 &b) {
  Mat3 out{};
  for (unsigned r = 0; r < 3; ++r)
    for (unsigned c = 0; c < 3; ++c)
      out[r * 3 + c] =
          dot(a[r * 3], b[c], a[r * 3 + 1], b[c + 3], a[r * 3 + 2], b[c + 6]);
  return out;
}
Mat34 mat34_mul_mat3(const Mat3 &a, const Mat34 &b) {
  return affine(mat3_mul(a, rotation(b)), {b[9], b[10], b[11]});
}
Mat34 mat34_mul(const Mat34 &a, const Mat34 &b) {
  Vec3 t{};
  for (unsigned c = 0; c < 3; ++c)
    t[c] = add(dot(a[9], b[c], a[10], b[c + 3], a[11], b[c + 6]), b[c + 9]);
  return affine(mat3_mul(rotation(a), rotation(b)), t);
}
Mat4 mat4_mul_mat34(const Mat34 &a, const Mat4 &b) {
  Mat4 out{};
  for (unsigned r = 0; r < 4; ++r)
    for (unsigned c = 0; c < 4; ++c) {
      Raw v =
          dot(a[r * 3], b[c], a[r * 3 + 1], b[c + 4], a[r * 3 + 2], b[c + 8]);
      out[r * 4 + c] = r == 3 ? add(v, b[c + 12]) : v;
    }
  return out;
}
Mat4 mat4_mul(const Mat4 &a, const Mat4 &b) {
  Mat4 out{};
  for (unsigned r = 0; r < 4; ++r)
    for (unsigned c = 0; c < 4; ++c)
      out[r * 4 + c] = add(
          dot(a[r * 4], b[c], a[r * 4 + 1], b[c + 4], a[r * 4 + 2], b[c + 8]),
          mul(a[r * 4 + 3], b[c + 12]));
  return out;
}
Mat3 mat3_rotate_axis(Mat3 m, std::int32_t axis, std::uint32_t a) {
  return rotate(m, 3, 3, axis, a, false);
}
Mat34 mat34_rotate_axis(Mat34 m, std::int32_t axis, std::uint32_t a) {
  return rotate(m, 3, 3, axis, a, false);
}
Mat4 mat4_rotate_axis(Mat4 m, std::int32_t axis, std::uint32_t a) {
  return rotate(m, 4, 4, axis, a, false);
}
Mat3 mat3_rotate_axis_post(Mat3 m, std::int32_t axis, std::uint32_t a) {
  return rotate(m, 3, 3, axis, a, true);
}
Mat34 mat34_rotate_axis_post(Mat34 m, std::int32_t axis, std::uint32_t a) {
  return rotate(m, 3, 4, axis, a, true);
}
Mat3 mat3_set_rot_z(std::uint32_t a) {
  const Raw c = cos16(a), s = sin16(a);
  return {c, s, 0, neg(s), c, 0, 0, 0, O};
}
Mat34 mat34_set_pos_rot_z(const Vec3 &p, std::uint32_t a) {
  return affine(mat3_set_rot_z(a), p);
}
Mat34 mat34_set_pos_rot_z_scale(Raw scale, const Vec3 &p, std::uint32_t a) {
  const Raw c = mul(cos16(a), scale), s = mul(sin16(a), scale);
  return affine({c, s, 0, neg(s), c, 0, 0, 0, scale}, p);
}
Mat34 mat34_set_pos_euler_scale(Raw s, const Vec3 &p, std::uint32_t y,
                                std::uint32_t pitch, std::uint32_t roll) {
  auto m = mat34_set_translation(p);
  m = mat34_rotate_axis(m, 2, y + 0x4000);
  m = mat34_rotate_axis(m, 1, pitch);
  m = mat34_rotate_axis(m, 0, roll);
  for (unsigned i = 0; i < 9; ++i)
    m[i] = mul(m[i], s);
  return m;
}
Mat4 mat4_invert(const Mat4 &m) {
  auto w = m;
  auto out = identity4();
  for (unsigned c = 0; c < 4; ++c) {
    Raw pivot = w[5 * c], best = fabs(pivot);
    unsigned pick = c;
    for (unsigned j = c + 1; j < 4; ++j)
      if (lt(best, fabs(w[4 * c + j]))) {
        pivot = w[4 * c + j];
        best = fabs(pivot);
        pick = j;
      }
    if (pick != c)
      for (unsigned k = 0; k < 4; ++k) {
        std::swap(out[4 * k + c], out[4 * k + pick]);
        std::swap(w[4 * k + c], w[4 * k + pick]);
      }
    if (le(best, 0x38d1b717))
      return identity4();
    const Raw inverse = div(O, pivot);
    for (unsigned k = 0; k < 4; ++k) {
      w[4 * k + c] = mul(w[4 * k + c], inverse);
      out[4 * k + c] = mul(out[4 * k + c], inverse);
    }
    for (unsigned j = 0; j < 4; ++j)
      if (j != c) {
        const Raw factor = w[4 * c + j];
        for (unsigned k = 0; k < 4; ++k) {
          w[4 * k + j] = sub(w[4 * k + j], mul(w[4 * k + c], factor));
          out[4 * k + j] = sub(out[4 * k + j], mul(out[4 * k + c], factor));
        }
      }
  }
  return out;
}
Mat34 mat34_invert(const Mat34 &m) {
  Mat4 wide{};
  for (unsigned r = 0; r < 4; ++r)
    for (unsigned c = 0; c < 3; ++c)
      wide[4 * r + c] = m[3 * r + c];
  wide[15] = O;
  wide = mat4_invert(wide);
  Mat34 out{};
  for (unsigned r = 0; r < 4; ++r)
    for (unsigned c = 0; c < 3; ++c)
      out[3 * r + c] = wide[4 * r + c];
  return out;
}
Mat3 mat3_transpose(const Mat3 &m) {
  return {m[0], m[3], m[6], m[1], m[4], m[7], m[2], m[5], m[8]};
}
Mat3 mat3_scale(Raw s, Mat3 m) {
  for (auto &x : m)
    x = mul(x, s);
  return m;
}
Mat3 mat3_scale_xyz(Raw x, Raw y, Raw z, Mat3 m) {
  const Raw s[3]{x, y, z};
  for (unsigned i = 0; i < 9; ++i)
    m[i] = mul(m[i], s[i / 3]);
  return m;
}
Mat4 mat4_scale_rotation(Raw s, Mat4 m) {
  for (unsigned r = 0; r < 3; ++r)
    for (unsigned c = 0; c < 3; ++c)
      m[4 * r + c] = mul(m[4 * r + c], s);
  return m;
}
Mat34 mat34_translate_local(Raw x, Raw y, Raw z, Mat34 m) {
  for (unsigned c = 0; c < 3; ++c)
    m[9 + c] = add(m[9 + c], dot(m[c], x, m[3 + c], y, m[6 + c], z));
  return m;
}
Mat4 mat4_translate_local(Raw x, Raw y, Raw z, Mat4 m) {
  for (unsigned c = 0; c < 4; ++c)
    m[12 + c] = add(m[12 + c], dot(m[c], x, m[4 + c], y, m[8 + c], z));
  return m;
}
Vec3 mat3_mul_vec(const Mat3 &m, const Vec3 &v) {
  Vec3 out{};
  for (unsigned c = 0; c < 3; ++c)
    out[c] = dot(m[c], v[0], m[3 + c], v[1], m[6 + c], v[2]);
  return out;
}
Vec3 mat34_rotate_vec(const Mat34 &m, const Vec3 &v) {
  return mat3_mul_vec(rotation(m), v);
}
Vec3 mat3_mul_vec_transposed(const Mat3 &m, const Vec3 &v) {
  Vec3 out{};
  for (unsigned r = 0; r < 3; ++r)
    out[r] = dot(m[3 * r], v[0], m[3 * r + 1], v[1], m[3 * r + 2], v[2]);
  return out;
}
Vec3 mat34_transform_point(const Mat34 &m, const Vec3 &v) {
  auto out = mat34_rotate_vec(m, v);
  for (unsigned c = 0; c < 3; ++c)
    out[c] = add(out[c], m[9 + c]);
  return out;
}
TransformPoint mat4_transform_point(const Mat4 &m, const Vec3 &v) {
  Quat out{};
  for (unsigned c = 0; c < 4; ++c)
    out[c] = add(dot(m[c], v[0], m[4 + c], v[1], m[8 + c], v[2]), m[12 + c]);
  return {{out[0], out[1], out[2]}, out[3]};
}
TransformPoint mat4_transform_point_transposed(const Mat4 &m, const Vec3 &v) {
  Vec3 out{};
  for (unsigned r = 0; r < 3; ++r)
    out[r] = add(dot(m[4 * r], v[0], m[4 * r + 1], v[1], m[4 * r + 2], v[2]),
                 m[4 * r + 3]);
  return {out, add(dot(m[3], v[0], m[7], v[1], m[11], v[2]), m[15])};
}
Mat4 mat4_set_projection(Raw sx, Raw sy, Raw scale, Raw za, Raw zb, Raw near,
                         Raw far) {
  Mat4 out{};
  const Raw k = div(mul(sub(zb, za), far), sub(far, near));
  out[0] = mul(scale, sx);
  out[5] = mul(scale, sy);
  out[10] = sub(zb, k);
  out[11] = O;
  out[14] = div(mul(mul(sub(zb, za), far), near), sub(far, near));
  return out;
}
Mat4 mat4_set_projection_fixed(Raw sx, Raw sy, Raw scale, Raw za, Raw zb,
                               Raw near, Raw far) {
  auto out = mat4_set_projection(sx, sy, scale, za, zb, near, far);
  out[10] = mul(mul(out[10], 0x41800000), 0x56800000);
  out[14] = mul(mul(out[14], 0x41800000), 0x56800000);
  return out;
}
Mat4 mat4_from_mat3_pos(const Mat3 &m, const Vec3 &p) {
  return {m[0], m[3], m[6], 0, m[1], m[4], m[7], 0,
          m[2], m[5], m[8], 0, p[0], p[1], p[2], O};
}
Quat quat_from_axis_angle(Raw x, Raw y, Raw z, std::uint32_t a) {
  const auto half = static_cast<std::uint32_t>(shift_signed(s16(a), 1));
  const Raw inv = rsqrt(O, add(add(mul(x, x), mul(y, y)), mul(z, z))),
            s = mul(sin16(half), inv);
  return {mul(s, x), mul(s, y), mul(s, z), cos16(half)};
}
Quat quat_mul(const Quat &a, const Quat &b) {
  return {add(add(mul(a[3], b[0]), mul(b[3], a[0])),
              sub(mul(a[1], b[2]), mul(a[2], b[1]))),
          add(add(mul(a[3], b[1]), mul(b[3], a[1])),
              sub(mul(a[2], b[0]), mul(a[0], b[2]))),
          add(add(mul(a[3], b[2]), mul(b[3], a[2])),
              sub(mul(a[0], b[1]), mul(a[1], b[0]))),
          sub(mul(a[3], b[3]), dot(a[0], b[0], a[1], b[1], a[2], b[2]))};
}
Quat quat_normalize(const Quat &q) {
  const Raw n = add(add(add(mul(q[3], q[3]), mul(q[0], q[0])), mul(q[1], q[1])),
                    mul(q[2], q[2])),
            inv = rsqrt(O, n);
  return {mul(q[0], inv), mul(q[1], inv), mul(q[2], inv), mul(q[3], inv)};
}
Vec3 quat_rotate_vec(const Quat &q, const Vec3 &v) {
  const Raw x = q[0], y = q[1], z = q[2], w = q[3],
            tw = neg(dot(x, v[0], y, v[1], z, v[2])),
            tx = add(mul(w, v[0]), sub(mul(y, v[2]), mul(z, v[1]))),
            ty = add(mul(w, v[1]), sub(mul(z, v[0]), mul(x, v[2]))),
            tz = add(mul(w, v[2]), sub(mul(x, v[1]), mul(y, v[0]))),
            nx = neg(x), ny = neg(y), nz = neg(z);
  return {add(add(mul(tw, nx), mul(w, tx)), sub(mul(ty, nz), mul(tz, ny))),
          add(add(mul(tw, ny), mul(w, ty)), sub(mul(tz, nx), mul(tx, nz))),
          add(add(mul(tw, nz), mul(w, tz)), sub(mul(tx, ny), mul(ty, nx)))};
}
Quat quat_from_mat3(const Mat3 &m) {
  const Raw trace = add(add(m[0], m[4]), m[8]);
  if (lt(Z, trace)) {
    const Raw r = sqrt(add(trace, O)), s = div(H, r);
    return {mul(sub(m[5], m[7]), s), mul(sub(m[6], m[2]), s),
            mul(sub(m[1], m[3]), s), mul(r, H)};
  }
  unsigned i = lt(m[0], m[4]) ? 1 : 0;
  if (lt(m[4 * i], m[8]))
    i = 2;
  const unsigned j = (i + 1) % 3, k = (j + 1) % 3;
  const Raw r = sqrt(add(sub(m[4 * i], add(m[4 * j], m[4 * k])), O)),
            s = eq(r, Z) ? r : div(H, r);
  Quat q{};
  q[i] = mul(r, H);
  q[3] = mul(sub(m[3 * j + k], m[3 * k + j]), s);
  q[j] = mul(add(m[3 * i + j], m[3 * j + i]), s);
  q[k] = mul(add(m[3 * i + k], m[3 * k + i]), s);
  return q;
}
Mat3 mat3_from_quat(const Quat &q) {
  const Raw x = q[0], y = q[1], z = q[2], w = q[3],
            s = div(T,
                    add(add(add(mul(x, x), mul(y, y)), mul(z, z)), mul(w, w))),
            xs = mul(x, s), ys = mul(y, s), zs = mul(z, s), xx = mul(x, xs),
            yy = mul(y, ys), zz = mul(z, zs), xy = mul(x, ys), xz = mul(x, zs),
            yz = mul(y, zs), wx = mul(w, xs), wy = mul(w, ys), wz = mul(w, zs);
  return {sub(O, add(yy, zz)), add(xy, wz),         sub(xz, wy),
          sub(xy, wz),         sub(O, add(xx, zz)), add(yz, wx),
          add(xz, wy),         sub(yz, wx),         sub(O, add(xx, yy))};
}
Mat34 mat34_from_quat(const Quat &q) { return affine(mat3_from_quat(q)); }
Mat4 mat4_from_quat(const Quat &q, const Mat4 &old) {
  auto out = mat4_from_mat3_pos(mat3_from_quat(q), {});
  std::copy(old.begin() + 12, old.end(), out.begin() + 12);
  return out;
}
Raw noise(const NoiseTable &t, std::uint32_t i) {
  const Raw f = mul(from_int(i & 15), 0x3d800000);
  const auto low = t[(i >> 4) & 255], high = t[((i >> 4) + 1) & 255];
  return mul(add(mul(from_int(static_cast<std::uint32_t>(low)), sub(O, f)),
                 mul(from_int(static_cast<std::uint32_t>(high)), f)),
             0x38000000);
}
Vec3 noise3(const NoiseTable &t, std::uint32_t i, std::uint32_t step) {
  return {noise(t, i), noise(t, i + step), noise(t, i + 2 * step)};
}
NoiseBuildResult build_noise_table(std::uint32_t seed, const Vec3 &stale,
                                   const std::optional<Quat> &weights) {
  Quat w{};
  if (weights)
    w = *weights;
  else
    for (unsigned k = 0; k < 4; ++k)
      w[k] = powf(from_int(1u << (3 - k)), M);
  Quat acc{Z, stale[0], stale[1], stale[2]}, slope{};
  NoiseBuildResult out{};
  for (unsigned n = 0; n < 256; ++n) {
    for (unsigned k = 0; k < 4; ++k) {
      if ((n & ((1u << k) - 1)) == 0) {
        seed = seed * 0x19660du + 0x3c6ef35fu;
        Raw target = sub(mul(from_int(seed >> 16), 0x37800000), H);
        if (n >= 0xf1)
          target = Z;
        slope[k] = mul(sub(target, acc[k]), div(O, from_int(1u << k)));
      }
      acc[k] = add(acc[k], slope[k]);
    }
    Raw total = Z;
    for (unsigned k = 0; k < 4; ++k)
      total = add(total, mul(acc[k], w[k]));
    out.table[n] = static_cast<std::int16_t>(
        s16(static_cast<std::uint32_t>(to_int(mul(total, 0x471c4000)))));
  }
  out.state = seed;
  return out;
}

Raw atanf(Raw x) {
  const auto hx = s32(x);
  const Raw ix = x & MAG;
  int index = -1;
  if (ix > 0x507fffff)
    return hx > 0 ? add(AH[3], AL[3]) : sub(neg(AH[3]), AL[3]);
  if (ix <= 0x3edfffff) {
    if (ix <= 0x30ffffff && lt(O, add(x, K_HUGE)))
      return x;
  } else {
    x = fabs(x);
    if (ix <= 0x3f97ffff) {
      if (ix <= 0x3f2fffff) {
        index = 0;
        x = div(sub(add(x, x), O), add(x, T));
      } else {
        index = 1;
        x = div(sub(x, O), add(x, O));
      }
    } else if (ix <= 0x401bffff) {
      index = 2;
      x = div(sub(x, 0x3fc00000), add(mul(x, 0x3fc00000), O));
    } else {
      index = 3;
      x = div(M, x);
    }
  }
  const Raw z = mul(x, x), w = mul(z, z);
  Raw even = mul(w, AT[10]), odd = mul(w, AT[9]);
  for (int n = 8; n >= 2; n -= 2) {
    even = mul(w, add(AT[n], even));
    odd = mul(w, add(AT[n - 1], odd));
  }
  const Raw s1 = mul(z, add(AT[0], even));
  if (index < 0)
    return sub(x, mul(x, add(s1, odd)));
  const Raw out = sub(AH[index], sub(sub(mul(x, add(s1, odd)), AL[index]), x));
  return hx < 0 ? neg(out) : out;
}
Raw atan2f(Raw y, Raw x) {
  const auto hx = s32(x), hy = s32(y);
  const Raw ix = x & MAG, iy = y & MAG;
  if (x == O)
    return atanf(y);
  const unsigned quadrant = ((y >> 31) & 1) | ((x >> 30) & 2);
  if (iy <= 0x007fffff) {
    if (quadrant == 2)
      return PI;
    if (quadrant == 3)
      return neg(PI);
    return y;
  }
  if (ix <= 0x007fffff)
    return hy < 0 ? neg(PI2) : PI2;
  const auto k =
      shift_signed(static_cast<std::int32_t>(std::int64_t(iy) - ix), 23);
  const Raw z = k > 60 ? PI2 : (hx < 0 && k < -60 ? Z : atanf(fabs(div(y, x))));
  if (quadrant == 0)
    return z;
  if (quadrant == 1)
    return neg(z);
  return quadrant == 2 ? sub(PI, sub(z, PILO)) : sub(sub(z, PILO), PI);
}
Raw floorf(Raw x) {
  const Raw ix = x & MAG;
  const int exponent = static_cast<int>(ix >> 23) - 127;
  if (exponent >= 23)
    return x;
  if (exponent < 0) {
    if (lt(Z, add(x, K_HUGE))) {
      if (!(x & SIGNBIT))
        return Z;
      if (ix > 0x007fffff)
        return M;
    }
    return x;
  }
  const Raw mask = 0x007fffffu >> exponent;
  if ((x & mask) == 0)
    return x;
  if (lt(Z, add(x, K_HUGE))) {
    if (x & SIGNBIT)
      x += 0x00800000u >> exponent;
    x &= ~mask;
  }
  return x;
}
Raw ceilf(Raw x) {
  const Raw ix = x & MAG;
  const int exponent = static_cast<int>(ix >> 23) - 127;
  if (exponent >= 23)
    return x;
  if (exponent < 0) {
    if (lt(Z, add(x, K_HUGE))) {
      if (x & SIGNBIT)
        return SIGNBIT;
      if (ix > 0x007fffff)
        return O;
    }
    return x;
  }
  const Raw mask = 0x007fffffu >> exponent;
  if ((x & mask) == 0)
    return x;
  if (lt(Z, add(x, K_HUGE))) {
    if (s32(x) > 0)
      x += 0x00800000u >> exponent;
    x &= ~mask;
  }
  return x;
}
Raw copysignf(Raw x, Raw y) { return (x & MAG) | (y & SIGNBIT); }
Raw scalbnf(Raw x, std::int32_t n) {
  const Raw hx = x & MAG;
  if (hx <= 0x007fffff)
    return x;
  const std::int64_t k = std::int64_t(hx >> 23) + n;
  if (k >= 256)
    return mul(copysignf(K_HUGE, x), K_HUGE);
  if (k > 0)
    return (x & 0x807fffff) | (static_cast<Raw>(k) << 23);
  return n > 50000 ? mul(copysignf(K_HUGE, x), K_HUGE)
                   : mul(copysignf(TINY, x), TINY);
}
Raw sqrtf(Raw x) {
  if ((x & MAG) <= 0x007fffff)
    return x;
  if (x & SIGNBIT) {
    const Raw zero = sub(x, x);
    return div(zero, zero);
  }
  int exponent = static_cast<int>(x >> 23) - 127;
  std::uint32_t remainder = (x & 0x007fffff) | 0x00800000;
  if (exponent % 2 != 0)
    remainder <<= 1;
  exponent = shift_signed(exponent, 1);
  remainder <<= 1;
  std::uint32_t q = 0, s = 0;
  for (std::uint32_t r = 0x01000000; r; r >>= 1) {
    const auto t = s + r;
    if (t <= remainder) {
      s = t + r;
      remainder -= t;
      q += r;
    }
    remainder <<= 1;
  }
  if (remainder)
    q += q & 1;
  return 0x3f000000u + (q >> 1) +
         static_cast<std::uint32_t>(exponent) * 0x00800000u;
}
Raw powf(Raw x, Raw y) {
  const auto hx = s32(x), hy = s32(y);
  Raw ix = x & MAG;
  const Raw iy = y & MAG;
  if (iy <= 0x007fffff)
    return O;
  unsigned yisint = 0;
  if (hx < 0) {
    if (iy > 0x4b7fffff)
      yisint = 2;
    else if (iy > 0x3f7fffff) {
      const unsigned shift = 0x96 - (iy >> 23), j = iy >> shift;
      if ((j << shift) == iy)
        yisint = 2 - (j & 1);
    }
  }
  if (iy == O)
    return hy >= 0 ? x : div(O, x);
  if (y == T)
    return mul(x, x);
  if (y == H && hx >= 0)
    return sqrtf(x);
  Raw ax = fabs(x);
  if (ix <= 0x007fffff || ix == O) {
    Raw z = ax;
    if (hy < 0)
      z = div(O, z);
    if (hx < 0) {
      if (ix == O && yisint == 0) {
        const Raw zero = sub(z, z);
        z = div(zero, zero);
      } else if (yisint == 1)
        z = neg(z);
    }
    return z;
  }
  const bool negative = (x & SIGNBIT) != 0;
  if (negative && yisint == 0) {
    const Raw zero = sub(x, x);
    return div(zero, zero);
  }
  Raw t1, t2;
  if (iy > 0x4d000000) {
    if (ix <= 0x3f7ffff7)
      return hy < 0 ? 0x7f7fffff : Z;
    if (ix > 0x3f800007)
      return hy > 0 ? 0x7f7fffff : Z;
    const Raw t = sub(x, O),
              w = mul(mul(t, t),
                      sub(H, mul(t, sub(THIRD, mul(t, 0x3e800000))))),
              u = mul(t, IVH), v = sub(mul(t, IVL), mul(w, IV));
    t1 = add(u, v) & HEAD;
    t2 = sub(v, sub(t1, u));
  } else {
    const Raw j = ix & 0x007fffff;
    int n = static_cast<int>(ix >> 23) - 127;
    ix = j | O;
    unsigned k;
    if (j <= 0x1cc471)
      k = 0;
    else if (j <= 0x5db3d6)
      k = 1;
    else {
      k = 0;
      ++n;
      ix -= 0x00800000;
    }
    ax = ix;
    Raw u = sub(ax, BP[k]), v = div(O, add(ax, BP[k]));
    const Raw s = mul(u, v), sh = s & HEAD;
    Raw th = ((ix >> 1) | 0x20000000) + (k << 21) + 0x00040000;
    Raw tl = sub(ax, sub(th, BP[k]));
    const Raw sl = mul(v, sub(sub(u, mul(sh, th)), mul(sh, tl)));
    Raw s2 = mul(s, s), r = mul(s2, PL[6]);
    for (int index = 5; index >= 1; --index) {
      r = add(r, PL[index]);
      if (index > 1)
        r = mul(s2, r);
    }
    r = mul(mul(s2, s2), r);
    r = add(r, mul(sl, add(sh, s)));
    s2 = mul(sh, sh);
    th = add(add(s2, THREE), r) & HEAD;
    tl = sub(r, sub(sub(th, THREE), s2));
    u = mul(sh, th);
    v = add(mul(sl, th), mul(tl, s));
    const Raw ph = add(u, v) & HEAD, pl = sub(v, sub(ph, u)), zh = mul(ph, CPH),
              zl = add(add(mul(ph, CPL), mul(pl, CP)), DL[k]),
              t = from_int(static_cast<Raw>(n));
    t1 = add(add(add(zh, zl), DH[k]), t) & HEAD;
    t2 = sub(zl, sub(sub(sub(t1, t), DH[k]), zh));
  }
  const Raw sign = negative && yisint == 1 ? M : O, y1 = y & HEAD;
  const Raw pl = add(mul(sub(y, y1), t1), mul(y, t2));
  Raw ph = mul(y1, t1), z = add(pl, ph);
  const auto j = s32(z);
  const Raw i = z & MAG;
  if (j > 0) {
    if (i > 0x43010000 || (i == 0x43010000 && lt(sub(z, ph), add(pl, OVT))))
      return mul(mul(sign, K_HUGE), K_HUGE);
  } else if (i > 0x42fc0000 || (i == 0x42fc0000 && le(pl, sub(z, ph))))
    return mul(mul(sign, TINY), TINY);
  int n = 0;
  Raw bits = z;
  if (i > 0x3f000000) {
    const Raw rounded = z + (0x00800000u >> ((i >> 23) - 0x7e));
    const unsigned k = ((rounded >> 23) & 255) - 127;
    const Raw t = rounded & ~(0x007fffffu >> k);
    n = static_cast<int>(((rounded & 0x007fffff) | 0x00800000) >> (23 - k));
    if (j < 0)
      n = -n;
    ph = sub(ph, t);
    bits = add(pl, ph);
  }
  const Raw t = bits & HEAD, u = mul(t, LGH),
            v = add(mul(sub(pl, sub(t, ph)), LG), mul(t, LGL));
  z = add(u, v);
  const Raw w = sub(v, sub(z, u)), square = mul(z, z);
  Raw poly = mul(square, PP[5]);
  for (int index = 4; index >= 1; --index)
    poly = mul(square, add(poly, PP[index]));
  t1 = sub(z, poly);
  const Raw r = sub(div(mul(z, t1), sub(t1, T)), add(w, mul(z, w)));
  z = sub(O, sub(r, z));
  const Raw scaled = z + static_cast<Raw>(n) * 0x00800000u;
  z = s32(scaled) < 0x00800000 ? scalbnf(z, n) : scaled;
  return mul(sign, z);
}
Raw fmodf(Raw x, Raw y) {
  Raw hx = x & MAG, hy = y & MAG;
  const Raw sign = x & SIGNBIT;
  if (hy <= 0x007fffff) {
    const Raw product = mul(x, y);
    return div(product, product);
  }
  if (hx < hy)
    return x;
  if (hx == hy)
    return sign;
  const int ix = static_cast<int>(hx >> 23) - 127;
  int iy = static_cast<int>(hy >> 23) - 127;
  hx = 0x00800000 | (hx & 0x007fffff);
  hy = 0x00800000 | (hy & 0x007fffff);
  for (int k = 0; k < ix - iy; ++k) {
    if (hx < hy)
      hx += hx;
    else if (hx == hy)
      return sign;
    else
      hx = (hx - hy) * 2;
  }
  if (hx >= hy)
    hx -= hy;
  if (hx == 0)
    return sign;
  while (hx < 0x00800000) {
    hx <<= 1;
    --iy;
  }
  if (iy >= -126)
    return (hx - 0x00800000) | (static_cast<Raw>(iy + 127) << 23) | sign;
  const unsigned shift = static_cast<unsigned>(-126 - iy);
  return (shift >= 32 ? 0u : hx >> shift) | sign;
}
std::array<Raw, 46> libm_coefficients() {
  // Same order as the published decimal list, including its huge/tiny
  // immediates.
  return {AH[0], AH[1], AH[2],  AH[3], AL[0], AL[1],      AL[2],  AL[3],
          AT[0], AT[1], AT[2],  AT[3], AT[4], AT[5],      AT[6],  AT[7],
          AT[8], AT[9], AT[10], DH[1], DL[1], 0x7f7fffff, K_HUGE, TINY,
          THIRD, IV,    IVH,    IVL,   PL[1], PL[2],      PL[3],  PL[4],
          PL[5], PL[6], PP[1],  PP[2], PP[3], PP[4],      PP[5],  CP,
          CPH,   CPL,   LG,     LGH,   LGL,   OVT};
}
} // namespace champions::simulation::rta
