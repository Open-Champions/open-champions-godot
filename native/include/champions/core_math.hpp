#pragma once

#include "champions/core_arithmetic.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <utility>

// Authored port of tools/ref_core.py sections 2–8. All numeric lanes are raw
// console patterns; matrices use row vectors and quaternions are x,y,z,w.
namespace champions::simulation::rta {
using Vec2 = std::array<Raw, 2>;
using Vec3 = std::array<Raw, 3>;
using Quat = std::array<Raw, 4>;
using Mat3 = std::array<Raw, 9>;
using Mat34 = std::array<Raw, 12>;
using Mat4 = std::array<Raw, 16>;
using BBox = std::array<Raw, 6>;
using NoiseTable = std::array<std::int16_t, 256>;
struct SegmentHit {
  bool hit;
  std::optional<Vec3> point;
};
struct TransformPoint {
  Vec3 point;
  Raw w;
};
struct NoiseBuildResult {
  NoiseTable table;
  std::uint32_t state;
};
Raw cos16(std::uint32_t angle);
Raw sin16(std::uint32_t angle);
std::int32_t angle_from_radians(Raw);
std::int32_t angle_from_degrees(Raw);
std::int32_t atan2_16(Raw y, Raw x);
std::pair<std::int32_t, std::uint32_t> angle_turn_dir(std::uint32_t a,
                                                      std::uint32_t b);
std::int32_t angle_approach(Raw rate, Raw min_step, Raw max_step,
                            std::uint32_t current, std::uint32_t target);
Raw hermite(Raw t, Raw p0, Raw p1, Raw m0, Raw m1);
std::uint32_t log2_floor(std::int64_t);
std::uint32_t color_scale_clamped(Raw factor, std::uint32_t rgba);
std::uint32_t color_from_hsv(Raw h, Raw s, Raw v);
Vec3 vec3_hermite(Raw t, const Vec3 &, const Vec3 &, const Vec3 &,
                  const Vec3 &);
Vec3 vec3_hermite_derivative(Raw t, const Vec3 &, const Vec3 &, const Vec3 &,
                             const Vec3 &);
Raw vec3_dot_delta(const Vec3 &a, const Vec3 &n, const Vec3 &b);
Raw vec2_dot_delta(const Vec2 &a, const Vec2 &n, const Vec2 &b);
Raw ray_plane_distance(const Vec3 &point, const Vec3 &direction,
                       const Vec3 &plane_point, const Vec3 &normal);
SegmentHit segment_hits_plane_near_edge(const Vec3 &a, const Vec3 &b,
                                        const Vec3 &normal, const Vec3 &p1,
                                        const Vec3 &p0);
BBox bbox_set_empty();
BBox bbox_union(BBox a, const BBox &b);
BBox bbox_add_point(BBox, const Vec3 &);
bool bbox_contains_point(const BBox &, const Vec3 &);
bool bbox_overlaps(const BBox &, const BBox &);
Mat3 mat3_set_identity();
Mat34 mat34_set_identity();
Mat34 mat34_set_translation(const Vec3 &);
Mat3 mat3_mul(const Mat3 &, const Mat3 &);
Mat34 mat34_mul_mat3(const Mat3 &, const Mat34 &);
Mat34 mat34_mul(const Mat34 &, const Mat34 &);
Mat4 mat4_mul_mat34(const Mat34 &, const Mat4 &);
Mat4 mat4_mul(const Mat4 &, const Mat4 &);
Mat3 mat3_rotate_axis(Mat3, std::int32_t axis, std::uint32_t angle);
Mat34 mat34_rotate_axis(Mat34, std::int32_t axis, std::uint32_t angle);
Mat4 mat4_rotate_axis(Mat4, std::int32_t axis, std::uint32_t angle);
Mat3 mat3_rotate_axis_post(Mat3, std::int32_t axis, std::uint32_t angle);
Mat34 mat34_rotate_axis_post(Mat34, std::int32_t axis, std::uint32_t angle);
Mat3 mat3_set_rot_z(std::uint32_t angle);
Mat34 mat34_set_pos_rot_z(const Vec3 &, std::uint32_t angle);
Mat34 mat34_set_pos_rot_z_scale(Raw scale, const Vec3 &, std::uint32_t angle);
Mat34 mat34_set_pos_euler_scale(Raw scale, const Vec3 &, std::uint32_t yaw,
                                std::uint32_t pitch, std::uint32_t roll);
Mat4 mat4_invert(const Mat4 &);
Mat34 mat34_invert(const Mat34 &);
Mat3 mat3_transpose(const Mat3 &);
Mat3 mat3_scale(Raw, Mat3);
Mat3 mat3_scale_xyz(Raw sx, Raw sy, Raw sz, Mat3);
Mat4 mat4_scale_rotation(Raw, Mat4);
Mat34 mat34_translate_local(Raw x, Raw y, Raw z, Mat34);
Mat4 mat4_translate_local(Raw x, Raw y, Raw z, Mat4);
Vec3 mat3_mul_vec(const Mat3 &, const Vec3 &);
Vec3 mat34_rotate_vec(const Mat34 &, const Vec3 &);
Vec3 mat3_mul_vec_transposed(const Mat3 &, const Vec3 &);
Vec3 mat34_transform_point(const Mat34 &, const Vec3 &);
TransformPoint mat4_transform_point(const Mat4 &, const Vec3 &);
TransformPoint mat4_transform_point_transposed(const Mat4 &, const Vec3 &);
Mat4 mat4_set_projection(Raw sx, Raw sy, Raw scale, Raw z_a, Raw z_b, Raw near,
                         Raw far);
Mat4 mat4_set_projection_fixed(Raw sx, Raw sy, Raw scale, Raw z_a, Raw z_b,
                               Raw near, Raw far);
Mat4 mat4_from_mat3_pos(const Mat3 &, const Vec3 &);
Quat quat_from_axis_angle(Raw x, Raw y, Raw z, std::uint32_t angle);
Quat quat_mul(const Quat &, const Quat &);
Quat quat_normalize(const Quat &);
Vec3 quat_rotate_vec(const Quat &, const Vec3 &);
Quat quat_from_mat3(const Mat3 &);
Mat3 mat3_from_quat(const Quat &);
Mat34 mat34_from_quat(const Quat &);
Mat4 mat4_from_quat(const Quat &, const Mat4 &old);
Raw noise(const NoiseTable &, std::uint32_t index);
Vec3 noise3(const NoiseTable &, std::uint32_t index, std::uint32_t step);
// Cosmetic boot inputs must be supplied; never silently seed shared gameplay
// state.
NoiseBuildResult
build_noise_table(std::uint32_t seed, const Vec3 &stale = {},
                  const std::optional<Quat> &weights = std::nullopt);
Raw atanf(Raw);
Raw atan2f(Raw y, Raw x);
Raw floorf(Raw);
Raw ceilf(Raw);
Raw copysignf(Raw x, Raw y);
Raw scalbnf(Raw x, std::int32_t n);
Raw sqrtf(Raw);
Raw powf(Raw x, Raw y);
Raw fmodf(Raw x, Raw y);
// Published decimal coefficients after compiler truncation, exposed for exact
// audit.
std::array<Raw, 46> libm_coefficients();
} // namespace champions::simulation::rta
