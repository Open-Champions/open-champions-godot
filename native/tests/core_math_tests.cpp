#include "champions/core_math.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace r = champions::simulation::rta;
namespace {
using Words = std::vector<std::uint32_t>;
template <std::size_t N>
std::array<r::Raw, N> array(const Words &a, std::size_t at) {
  if (at > a.size() || N > a.size() - at)
    throw std::runtime_error("short argument array");
  std::array<r::Raw, N> out{};
  std::copy_n(a.begin() + static_cast<std::ptrdiff_t>(at), N, out.begin());
  return out;
}
template <std::size_t N> Words pack(const std::array<r::Raw, N> &a) {
  return {a.begin(), a.end()};
}
Words pack(std::uint32_t a) { return {a}; }
Words pack(std::int32_t a) { return {static_cast<std::uint32_t>(a)}; }
Words pack(bool a) { return {a ? 1u : 0u}; }
Words pack(const std::pair<std::int32_t, std::uint32_t> &a) {
  return {static_cast<std::uint32_t>(a.first), a.second};
}
Words pack(const r::TransformPoint &a) {
  return {a.point[0], a.point[1], a.point[2], a.w};
}
Words pack(const r::SegmentHit &a) {
  Words out{a.hit ? 1u : 0u, a.point ? 1u : 0u};
  for (unsigned i = 0; i < 3; ++i)
    out.push_back(a.point ? (*a.point)[i] : 0);
  return out;
}
Words pack(const r::NoiseBuildResult &a) {
  Words out;
  for (auto x : a.table)
    out.push_back(static_cast<std::uint32_t>(static_cast<std::int32_t>(x)));
  out.push_back(a.state);
  return out;
}
r::NoiseTable noise_table(const Words &a) {
  r::NoiseTable t{};
  if (a.size() < 256)
    throw std::runtime_error("short noise table");
  for (unsigned i = 0; i < 256; ++i) {
    const auto v = r::s32(a[i]);
    if (v < -32768 || v > 32767)
      throw std::runtime_error("noise input range");
    t[i] = static_cast<std::int16_t>(v);
  }
  return t;
}
std::int64_t signed64(std::uint32_t lo, std::uint32_t hi) {
  const std::uint64_t x = lo | (std::uint64_t(hi) << 32);
  return (hi & 0x80000000) ? -1 - static_cast<std::int64_t>(~x)
                           : static_cast<std::int64_t>(x);
}
Words execute(const std::string &name, const Words &a) {
  if (name == "cos16") {
    if (a.size() != 1)
      throw std::runtime_error("cos16 arity");
    return pack(r::cos16(a[0]));
  }
  if (name == "sin16") {
    if (a.size() != 1)
      throw std::runtime_error("sin16 arity");
    return pack(r::sin16(a[0]));
  }
  if (name == "angle_from_radians") {
    if (a.size() != 1)
      throw std::runtime_error("angle_from_radians arity");
    return pack(r::angle_from_radians(a[0]));
  }
  if (name == "angle_from_degrees") {
    if (a.size() != 1)
      throw std::runtime_error("angle_from_degrees arity");
    return pack(r::angle_from_degrees(a[0]));
  }
  if (name == "atan2_16") {
    if (a.size() != 2)
      throw std::runtime_error("atan2_16 arity");
    return pack(r::atan2_16(a[0], a[1]));
  }
  if (name == "angle_turn_dir") {
    if (a.size() != 2)
      throw std::runtime_error("angle_turn_dir arity");
    return pack(r::angle_turn_dir(a[0], a[1]));
  }
  if (name == "angle_approach") {
    if (a.size() != 5)
      throw std::runtime_error("angle_approach arity");
    return pack(r::angle_approach(a[0], a[1], a[2], a[3], a[4]));
  }
  if (name == "hermite") {
    if (a.size() != 5)
      throw std::runtime_error("hermite arity");
    return pack(r::hermite(a[0], a[1], a[2], a[3], a[4]));
  }
  if (name == "log2_floor") {
    if (a.size() != 2)
      throw std::runtime_error("log2_floor arity");
    return pack(r::log2_floor(signed64(a[0], a[1])));
  }
  if (name == "color_scale_clamped") {
    if (a.size() != 2)
      throw std::runtime_error("color_scale_clamped arity");
    return pack(r::color_scale_clamped(a[0], a[1]));
  }
  if (name == "color_from_hsv") {
    if (a.size() != 3)
      throw std::runtime_error("color_from_hsv arity");
    return pack(r::color_from_hsv(a[0], a[1], a[2]));
  }
  if (name == "vec3_hermite") {
    if (a.size() != 13)
      throw std::runtime_error("vec3_hermite arity");
    return pack(r::vec3_hermite(a[0], array<3>(a, 1), array<3>(a, 4),
                                array<3>(a, 7), array<3>(a, 10)));
  }
  if (name == "vec3_hermite_derivative") {
    if (a.size() != 13)
      throw std::runtime_error("vec3_hermite_derivative arity");
    return pack(r::vec3_hermite_derivative(a[0], array<3>(a, 1), array<3>(a, 4),
                                           array<3>(a, 7), array<3>(a, 10)));
  }
  if (name == "vec3_dot_delta") {
    if (a.size() != 9)
      throw std::runtime_error("vec3_dot_delta arity");
    return pack(
        r::vec3_dot_delta(array<3>(a, 0), array<3>(a, 3), array<3>(a, 6)));
  }
  if (name == "vec2_dot_delta") {
    if (a.size() != 6)
      throw std::runtime_error("vec2_dot_delta arity");
    return pack(
        r::vec2_dot_delta(array<2>(a, 0), array<2>(a, 2), array<2>(a, 4)));
  }
  if (name == "ray_plane_distance") {
    if (a.size() != 12)
      throw std::runtime_error("ray_plane_distance arity");
    return pack(r::ray_plane_distance(array<3>(a, 0), array<3>(a, 3),
                                      array<3>(a, 6), array<3>(a, 9)));
  }
  if (name == "segment_hits_plane_near_edge") {
    if (a.size() != 15)
      throw std::runtime_error("segment_hits_plane_near_edge arity");
    return pack(r::segment_hits_plane_near_edge(array<3>(a, 0), array<3>(a, 3),
                                                array<3>(a, 6), array<3>(a, 9),
                                                array<3>(a, 12)));
  }
  if (name == "bbox_set_empty") {
    if (a.size() != 0)
      throw std::runtime_error("bbox_set_empty arity");
    return pack(r::bbox_set_empty());
  }
  if (name == "bbox_union") {
    if (a.size() != 12)
      throw std::runtime_error("bbox_union arity");
    return pack(r::bbox_union(array<6>(a, 0), array<6>(a, 6)));
  }
  if (name == "bbox_add_point") {
    if (a.size() != 9)
      throw std::runtime_error("bbox_add_point arity");
    return pack(r::bbox_add_point(array<6>(a, 0), array<3>(a, 6)));
  }
  if (name == "bbox_contains_point") {
    if (a.size() != 9)
      throw std::runtime_error("bbox_contains_point arity");
    return pack(r::bbox_contains_point(array<6>(a, 0), array<3>(a, 6)));
  }
  if (name == "bbox_overlaps") {
    if (a.size() != 12)
      throw std::runtime_error("bbox_overlaps arity");
    return pack(r::bbox_overlaps(array<6>(a, 0), array<6>(a, 6)));
  }
  if (name == "mat3_set_identity") {
    if (a.size() != 0)
      throw std::runtime_error("mat3_set_identity arity");
    return pack(r::mat3_set_identity());
  }
  if (name == "mat34_set_identity") {
    if (a.size() != 0)
      throw std::runtime_error("mat34_set_identity arity");
    return pack(r::mat34_set_identity());
  }
  if (name == "mat34_set_translation") {
    if (a.size() != 3)
      throw std::runtime_error("mat34_set_translation arity");
    return pack(r::mat34_set_translation(array<3>(a, 0)));
  }
  if (name == "mat3_mul") {
    if (a.size() != 18)
      throw std::runtime_error("mat3_mul arity");
    return pack(r::mat3_mul(array<9>(a, 0), array<9>(a, 9)));
  }
  if (name == "mat34_mul_mat3") {
    if (a.size() != 21)
      throw std::runtime_error("mat34_mul_mat3 arity");
    return pack(r::mat34_mul_mat3(array<9>(a, 0), array<12>(a, 9)));
  }
  if (name == "mat34_mul") {
    if (a.size() != 24)
      throw std::runtime_error("mat34_mul arity");
    return pack(r::mat34_mul(array<12>(a, 0), array<12>(a, 12)));
  }
  if (name == "mat4_mul_mat34") {
    if (a.size() != 28)
      throw std::runtime_error("mat4_mul_mat34 arity");
    return pack(r::mat4_mul_mat34(array<12>(a, 0), array<16>(a, 12)));
  }
  if (name == "mat4_mul") {
    if (a.size() != 32)
      throw std::runtime_error("mat4_mul arity");
    return pack(r::mat4_mul(array<16>(a, 0), array<16>(a, 16)));
  }
  if (name == "mat3_rotate_axis") {
    if (a.size() != 11)
      throw std::runtime_error("mat3_rotate_axis arity");
    return pack(r::mat3_rotate_axis(array<9>(a, 0), r::s32(a[9]), a[10]));
  }
  if (name == "mat34_rotate_axis") {
    if (a.size() != 14)
      throw std::runtime_error("mat34_rotate_axis arity");
    return pack(r::mat34_rotate_axis(array<12>(a, 0), r::s32(a[12]), a[13]));
  }
  if (name == "mat4_rotate_axis") {
    if (a.size() != 18)
      throw std::runtime_error("mat4_rotate_axis arity");
    return pack(r::mat4_rotate_axis(array<16>(a, 0), r::s32(a[16]), a[17]));
  }
  if (name == "mat3_rotate_axis_post") {
    if (a.size() != 11)
      throw std::runtime_error("mat3_rotate_axis_post arity");
    return pack(r::mat3_rotate_axis_post(array<9>(a, 0), r::s32(a[9]), a[10]));
  }
  if (name == "mat34_rotate_axis_post") {
    if (a.size() != 14)
      throw std::runtime_error("mat34_rotate_axis_post arity");
    return pack(
        r::mat34_rotate_axis_post(array<12>(a, 0), r::s32(a[12]), a[13]));
  }
  if (name == "mat3_set_rot_z") {
    if (a.size() != 1)
      throw std::runtime_error("mat3_set_rot_z arity");
    return pack(r::mat3_set_rot_z(a[0]));
  }
  if (name == "mat34_set_pos_rot_z") {
    if (a.size() != 4)
      throw std::runtime_error("mat34_set_pos_rot_z arity");
    return pack(r::mat34_set_pos_rot_z(array<3>(a, 0), a[3]));
  }
  if (name == "mat34_set_pos_rot_z_scale") {
    if (a.size() != 5)
      throw std::runtime_error("mat34_set_pos_rot_z_scale arity");
    return pack(r::mat34_set_pos_rot_z_scale(a[0], array<3>(a, 1), a[4]));
  }
  if (name == "mat34_set_pos_euler_scale") {
    if (a.size() != 7)
      throw std::runtime_error("mat34_set_pos_euler_scale arity");
    return pack(
        r::mat34_set_pos_euler_scale(a[0], array<3>(a, 1), a[4], a[5], a[6]));
  }
  if (name == "mat4_invert") {
    if (a.size() != 16)
      throw std::runtime_error("mat4_invert arity");
    return pack(r::mat4_invert(array<16>(a, 0)));
  }
  if (name == "mat34_invert") {
    if (a.size() != 12)
      throw std::runtime_error("mat34_invert arity");
    return pack(r::mat34_invert(array<12>(a, 0)));
  }
  if (name == "mat3_transpose") {
    if (a.size() != 9)
      throw std::runtime_error("mat3_transpose arity");
    return pack(r::mat3_transpose(array<9>(a, 0)));
  }
  if (name == "mat3_scale") {
    if (a.size() != 10)
      throw std::runtime_error("mat3_scale arity");
    return pack(r::mat3_scale(a[0], array<9>(a, 1)));
  }
  if (name == "mat3_scale_xyz") {
    if (a.size() != 12)
      throw std::runtime_error("mat3_scale_xyz arity");
    return pack(r::mat3_scale_xyz(a[0], a[1], a[2], array<9>(a, 3)));
  }
  if (name == "mat4_scale_rotation") {
    if (a.size() != 17)
      throw std::runtime_error("mat4_scale_rotation arity");
    return pack(r::mat4_scale_rotation(a[0], array<16>(a, 1)));
  }
  if (name == "mat34_translate_local") {
    if (a.size() != 15)
      throw std::runtime_error("mat34_translate_local arity");
    return pack(r::mat34_translate_local(a[0], a[1], a[2], array<12>(a, 3)));
  }
  if (name == "mat4_translate_local") {
    if (a.size() != 19)
      throw std::runtime_error("mat4_translate_local arity");
    return pack(r::mat4_translate_local(a[0], a[1], a[2], array<16>(a, 3)));
  }
  if (name == "mat3_mul_vec") {
    if (a.size() != 12)
      throw std::runtime_error("mat3_mul_vec arity");
    return pack(r::mat3_mul_vec(array<9>(a, 0), array<3>(a, 9)));
  }
  if (name == "mat34_rotate_vec") {
    if (a.size() != 15)
      throw std::runtime_error("mat34_rotate_vec arity");
    return pack(r::mat34_rotate_vec(array<12>(a, 0), array<3>(a, 12)));
  }
  if (name == "mat3_mul_vec_transposed") {
    if (a.size() != 12)
      throw std::runtime_error("mat3_mul_vec_transposed arity");
    return pack(r::mat3_mul_vec_transposed(array<9>(a, 0), array<3>(a, 9)));
  }
  if (name == "mat34_transform_point") {
    if (a.size() != 15)
      throw std::runtime_error("mat34_transform_point arity");
    return pack(r::mat34_transform_point(array<12>(a, 0), array<3>(a, 12)));
  }
  if (name == "mat4_transform_point") {
    if (a.size() != 19)
      throw std::runtime_error("mat4_transform_point arity");
    return pack(r::mat4_transform_point(array<16>(a, 0), array<3>(a, 16)));
  }
  if (name == "mat4_transform_point_transposed") {
    if (a.size() != 19)
      throw std::runtime_error("mat4_transform_point_transposed arity");
    return pack(
        r::mat4_transform_point_transposed(array<16>(a, 0), array<3>(a, 16)));
  }
  if (name == "mat4_set_projection") {
    if (a.size() != 7)
      throw std::runtime_error("mat4_set_projection arity");
    return pack(
        r::mat4_set_projection(a[0], a[1], a[2], a[3], a[4], a[5], a[6]));
  }
  if (name == "mat4_set_projection_fixed") {
    if (a.size() != 7)
      throw std::runtime_error("mat4_set_projection_fixed arity");
    return pack(
        r::mat4_set_projection_fixed(a[0], a[1], a[2], a[3], a[4], a[5], a[6]));
  }
  if (name == "mat4_from_mat3_pos") {
    if (a.size() != 12)
      throw std::runtime_error("mat4_from_mat3_pos arity");
    return pack(r::mat4_from_mat3_pos(array<9>(a, 0), array<3>(a, 9)));
  }
  if (name == "quat_from_axis_angle") {
    if (a.size() != 4)
      throw std::runtime_error("quat_from_axis_angle arity");
    return pack(r::quat_from_axis_angle(a[0], a[1], a[2], a[3]));
  }
  if (name == "quat_mul") {
    if (a.size() != 8)
      throw std::runtime_error("quat_mul arity");
    return pack(r::quat_mul(array<4>(a, 0), array<4>(a, 4)));
  }
  if (name == "quat_normalize") {
    if (a.size() != 4)
      throw std::runtime_error("quat_normalize arity");
    return pack(r::quat_normalize(array<4>(a, 0)));
  }
  if (name == "quat_rotate_vec") {
    if (a.size() != 7)
      throw std::runtime_error("quat_rotate_vec arity");
    return pack(r::quat_rotate_vec(array<4>(a, 0), array<3>(a, 4)));
  }
  if (name == "quat_from_mat3") {
    if (a.size() != 9)
      throw std::runtime_error("quat_from_mat3 arity");
    return pack(r::quat_from_mat3(array<9>(a, 0)));
  }
  if (name == "mat3_from_quat") {
    if (a.size() != 4)
      throw std::runtime_error("mat3_from_quat arity");
    return pack(r::mat3_from_quat(array<4>(a, 0)));
  }
  if (name == "mat34_from_quat") {
    if (a.size() != 4)
      throw std::runtime_error("mat34_from_quat arity");
    return pack(r::mat34_from_quat(array<4>(a, 0)));
  }
  if (name == "mat4_from_quat") {
    if (a.size() != 20)
      throw std::runtime_error("mat4_from_quat arity");
    return pack(r::mat4_from_quat(array<4>(a, 0), array<16>(a, 4)));
  }
  if (name == "noise") {
    if (a.size() != 257)
      throw std::runtime_error("noise arity");
    return pack(r::noise(noise_table(a), a[256]));
  }
  if (name == "noise3") {
    if (a.size() != 258)
      throw std::runtime_error("noise3 arity");
    return pack(r::noise3(noise_table(a), a[256], a[257]));
  }
  if (name == "build_noise_table") {
    if (a.size() != 9)
      throw std::runtime_error("build_noise_table arity");
    return pack(r::build_noise_table(
        a[0], array<3>(a, 1),
        a[4] ? std::optional<r::Quat>(array<4>(a, 5)) : std::nullopt));
  }
  if (name == "atanf") {
    if (a.size() != 1)
      throw std::runtime_error("atanf arity");
    return pack(r::atanf(a[0]));
  }
  if (name == "atan2f") {
    if (a.size() != 2)
      throw std::runtime_error("atan2f arity");
    return pack(r::atan2f(a[0], a[1]));
  }
  if (name == "floorf") {
    if (a.size() != 1)
      throw std::runtime_error("floorf arity");
    return pack(r::floorf(a[0]));
  }
  if (name == "ceilf") {
    if (a.size() != 1)
      throw std::runtime_error("ceilf arity");
    return pack(r::ceilf(a[0]));
  }
  if (name == "copysignf") {
    if (a.size() != 2)
      throw std::runtime_error("copysignf arity");
    return pack(r::copysignf(a[0], a[1]));
  }
  if (name == "scalbnf") {
    if (a.size() != 2)
      throw std::runtime_error("scalbnf arity");
    return pack(r::scalbnf(a[0], r::s32(a[1])));
  }
  if (name == "sqrtf") {
    if (a.size() != 1)
      throw std::runtime_error("sqrtf arity");
    return pack(r::sqrtf(a[0]));
  }
  if (name == "powf") {
    if (a.size() != 2)
      throw std::runtime_error("powf arity");
    return pack(r::powf(a[0], a[1]));
  }
  if (name == "fmodf") {
    if (a.size() != 2)
      throw std::runtime_error("fmodf arity");
    return pack(r::fmodf(a[0], a[1]));
  }
  if (name == "libm_coefficients") {
    if (!a.empty())
      throw std::runtime_error("coefficients arity");
    return pack(r::libm_coefficients());
  }
  throw std::runtime_error("unknown math operation: " + name);
}
std::vector<std::string> split(const std::string &s, char separator) {
  std::vector<std::string> out;
  std::size_t start = 0;
  while (true) {
    const auto end = s.find(separator, start);
    out.push_back(s.substr(start, end - start));
    if (end == std::string::npos)
      return out;
    start = end + 1;
  }
}
Words words(const std::string &s) {
  if (s == "-")
    return {};
  Words out;
  for (const auto &t : split(s, ',')) {
    if (t.size() != 8 ||
        t.find_first_not_of("0123456789abcdef") != std::string::npos)
      throw std::runtime_error("invalid hex word");
    out.push_back(static_cast<std::uint32_t>(std::stoul(t, nullptr, 16)));
  }
  return out;
}
std::string hexwords(const Words &w) {
  std::ostringstream s;
  s << std::hex << std::setfill('0');
  for (std::size_t i = 0; i < w.size(); ++i) {
    if (i)
      s << ',';
    s << std::setw(8) << w[i];
  }
  return s.str();
}
void require(bool yes, const char *message) {
  if (!yes)
    throw std::runtime_error(message);
}
void anchors() {
  r::set_fpu();
  require(r::cos16(0) == 0x3f800000 && r::sin16(0) == 0x34800000,
          "angle nonzero residual anchor");
  require(r::angle_turn_dir(0, 0x8000) == std::make_pair(1, 32768u),
          "half-turn signed anchor");
  require(r::color_from_hsv(r::ONE, r::ONE, r::ONE) == 0xff000000,
          "hue endpoint anchor");
  require(r::floorf(0xbf000000) == r::MINUS_ONE &&
              r::ceilf(0xbf000000) == 0x80000000,
          "signed floor/ceil anchor");
  const r::Mat3 matrix{r::ONE, r::TWO, 0, 0, r::ONE, 0, 0, 0, r::ONE};
  require(r::mat3_mul_vec(matrix, {r::ONE, r::TWO, 0}) ==
              r::Vec3{r::ONE, 0x40800000, 0},
          "row-vector matrix anchor");
  require(r::mat3_mul_vec_transposed(matrix, {r::ONE, r::TWO, 0}) ==
              r::Vec3{0x40a00000, r::TWO, 0},
          "transposed row anchor");
  const auto missed_edge = r::segment_hits_plane_near_edge(
      {0, 0, 0}, {r::TWO, 0, 0}, {0, 0, r::ONE}, {0x40800000, 0, r::MINUS_ONE},
      {0x40800000, 0, r::ONE});
  require(!missed_edge.hit && missed_edge.point &&
              (*missed_edge.point)[0] == 0x40800000,
          "missed edge still writes intersection");
  r::NoiseTable table{};
  table[0] = -32768;
  table[1] = 32767;
  require(r::noise(table, 0) == r::MINUS_ONE &&
              r::noise(table, 4096) == r::MINUS_ONE,
          "noise signed endpoint and period");
  require(r::mat4_invert({}) == r::Mat4{r::ONE, 0, 0, 0, 0, r::ONE, 0, 0, 0, 0,
                                        r::ONE, 0, 0, 0, 0, r::ONE},
          "singular identity fallback anchor");
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc < 2 || argc > 3)
      throw std::runtime_error(
          "usage: core-math-tests VECTORS.tsv [TRACE.tsv]");
    anchors();
    std::ifstream file(argv[1]);
    if (!file)
      throw std::runtime_error("cannot open vectors");
    std::ofstream trace;
    if (argc == 3) {
      trace.open(argv[2], std::ios::binary);
      if (!trace)
        throw std::runtime_error("cannot open trace");
      trace << "# rta-core-math-trace-v1\n";
    }
    std::map<std::string, std::size_t> counts;
    std::map<std::string, std::size_t> config_counts;
    std::set<std::string> configs;
    std::array<bool, 65536> cosine{}, sine{};
    std::size_t rows = 0, components = 0, line = 0;
    std::uint64_t digest = 14695981039346656037ull;
    auto hash = [&](const std::string &s) {
      for (unsigned char c : s) {
        digest ^= c;
        digest *= 1099511628211ull;
      }
    };
    std::string text;
    bool schema = false;
    while (std::getline(file, text)) {
      ++line;
      if (line == 1) {
        require(text == "# rta-core-math-v1", "wrong schema");
        schema = true;
        continue;
      }
      if (text.empty())
        throw std::runtime_error("empty vector record");
      if (text[0] == '#')
        continue;
      const auto cells = split(text, '\t');
      require(cells.size() == 6, "vector field count");
      r::Fpu::Model model;
      if (cells[1] == "ps2")
        model = r::Fpu::Model::Ps2;
      else if (cells[1] == "ps2g1")
        model = r::Fpu::Model::Ps2g1;
      else if (cells[1] == "chop")
        model = r::Fpu::Model::Chop;
      else if (cells[1] == "nearest")
        model = r::Fpu::Model::Nearest;
      else
        throw std::runtime_error("unknown arithmetic model");
      require((cells[2] == "0" || cells[2] == "1") &&
                  (cells[3] == "0" || cells[3] == "1"),
              "invalid arithmetic switches");
      r::set_fpu(model, cells[2] == "1", cells[3] == "1");
      const auto inputs = words(cells[4]), expected = words(cells[5]);
      require(!expected.empty(), "empty expected output");
      const auto actual = execute(cells[0], inputs);
      if (actual != expected) {
        std::cerr << "Mismatch line " << line << " " << cells[0] << " "
                  << cells[1] << " flags=" << cells[2] << cells[3]
                  << "\nexpected " << hexwords(expected) << "\nactual   "
                  << hexwords(actual) << '\n';
        return 1;
      }
      const std::string key = cells[1] + ":" + cells[2] + ":" + cells[3];
      configs.insert(key);
      ++counts[cells[0]];
      ++config_counts[cells[0] + "/" + key];
      ++rows;
      components += actual.size();
      if (key == "ps2:0:0" && inputs.size() == 1 && inputs[0] < 65536) {
        if (cells[0] == "cos16")
          cosine[inputs[0]] = true;
        if (cells[0] == "sin16")
          sine[inputs[0]] = true;
      }
      const std::string record = cells[0] + "\t" + key + "\t" + cells[4] +
                                 "\t" + hexwords(actual) + "\n";
      hash(record);
      if (trace.is_open())
        trace << record;
    }
    require(schema && file.eof(), "truncated vector read");
    require(configs.size() == 16, "missing arithmetic configurations");
    require(counts.size() == 77, "missing function coverage");
    require(
        std::all_of(cosine.begin(), cosine.end(), [](bool b) { return b; }) &&
            std::all_of(sine.begin(), sine.end(), [](bool b) { return b; }),
        "missing exhaustive angle coverage");
    // Require every modeled API under every setting, not a shared global count.
    require(config_counts.size() == 77 * 16, "missing API/configuration pair");
    for (const auto &entry : config_counts) {
      const auto slash = entry.first.find('/');
      const auto n = entry.first.substr(0, slash);
      const auto config = entry.first.substr(slash + 1);
      std::size_t expected_count = 64;
      if (n == "atanf" || n == "atan2f" || n == "floorf" || n == "ceilf" ||
          n == "copysignf" || n == "scalbnf" || n == "sqrtf" || n == "powf" ||
          n == "fmodf")
        expected_count = 1024;
      if ((n == "cos16" || n == "sin16") && config == "ps2:0:0")
        expected_count += 65536;
      if (n == "bbox_set_empty" || n == "mat3_set_identity" ||
          n == "mat34_set_identity" || n == "libm_coefficients")
        expected_count = 1;
      if (n == "build_noise_table")
        expected_count = 12;
      if (n == "segment_hits_plane_near_edge")
        expected_count += 3;
      if (n == "mat4_invert" || n == "quat_from_mat3" || n == "color_from_hsv")
        ++expected_count;
      require(entry.second == expected_count,
              "incorrect per-API/configuration coverage");
    }
    if (trace.is_open()) {
      trace.flush();
      require(bool(trace), "trace write failure");
    }
    std::cout << "core math PASS: " << rows << " vectors, " << components
              << " raw words, 77 APIs, 16 configurations; fnv1a64=" << std::hex
              << std::setw(16) << std::setfill('0') << digest << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "core math FAIL: " << e.what() << '\n';
    return 1;
  }
}
