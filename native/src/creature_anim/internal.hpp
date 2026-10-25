#pragma once
#include "champions/creature_anim/runtime.hpp"
namespace champions::simulation::rta::creature_anim {
inline std::int32_t quaternion_yaw(const Quat &q) {
  const auto side = add(mul(q[0], q[1]), mul(q[2], q[3]));
  return atan2_16(
      add(side, side),
      add(sub(sub(mul(q[0], q[0]), mul(q[1], q[1])), mul(q[2], q[2])),
          mul(q[3], q[3])));
}
inline Quat strip_yaw(const Quat &q) {
  return quat_mul(quat_from_axis_angle(ZERO, ZERO, ONE,
                                       static_cast<Raw>(-quaternion_yaw(q))),
                  q);
}
struct Runtime::Storage {
  Limits limits;
  animation_decoder::Context decoder;
  std::vector<std::unique_ptr<AnimState>> states;
  // 1-based tokens; fallback=count+1, default=count+2. Zero is null.
  std::vector<Pose> poses;
  std::vector<std::size_t> pose_order;
  std::uint32_t draw_frame = 0;
  std::array<std::array<Raw, 4>, 256> vu_mem{};
  Storage(std::size_t slots, Limits supplied, std::size_t count)
      : limits(supplied), decoder(slots, supplied.decoder), poses(count + 2) {
    for (auto i = count; i > 0; --i)
      pose_order.push_back(i);
    auto &d = poses.back();
    d.tag = 0x6dc18000;
    d.joint_count = 64;
    for (auto &m : d.mats)
      for (unsigned r = 0; r < 3; ++r)
        m[r][r] = 4096;
  }
};
} // namespace champions::simulation::rta::creature_anim
