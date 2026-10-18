// Authored from ref_anim.py at 2502e2b. All simulation lanes remain raw32.
#include "internal.hpp"
#include <algorithm>
namespace champions::simulation::rta::creature_anim {
namespace {
std::int32_t ftoi(Raw raw, int shift) {
  const int exp = (raw >> 23) & 255;
  if (!exp)
    return 0;
  const auto mant = (raw & 0x7fffff) | 0x800000;
  const int e = exp - 150 + shift;
  if (e >= 8)
    return raw >> 31 ? INT32_MIN : INT32_MAX;
  const auto magnitude = e >= 0 ? mant << e : -e >= 32 ? 0u : mant >> -e;
  return raw >> 31 ? -static_cast<std::int32_t>(magnitude)
                   : static_cast<std::int32_t>(magnitude);
}
} // namespace
std::uint32_t Runtime::draw_frame() const noexcept {
  return storage_->draw_frame;
}
void Runtime::set_draw_frame(std::uint32_t f) noexcept {
  storage_->draw_frame = f;
}
void Runtime::detach_pose(AnimState &s) {
  if (s.pose) {
    storage_->poses.at(s.pose - 1).owner = 0;
    s.pose = 0;
  }
}
void Runtime::pose_to_tail(std::size_t token) {
  auto &o = storage_->pose_order;
  auto it = std::find(o.begin(), o.end(), token);
  if (it != o.end() && o.back() != token) {
    o.erase(it);
    o.push_back(token);
  }
}
void Runtime::reset_stack() {
  storage_->vu_mem[0] = {ONE, ZERO, ZERO, ZERO};
  storage_->vu_mem[1] = {ZERO, ONE, ZERO, ZERO};
  storage_->vu_mem[2] = {ZERO, ZERO, ONE, ZERO};
}
PoseRows Runtime::compose_joint(const Quat &q, const Vec3 &p,
                                const std::array<std::int16_t, 4> &bind,
                                std::int32_t depth) {
  const auto x = q[0], y = q[1], z = q[2], w = q[3];
  const auto x2 = add(x, x), y2 = add(y, y), z2 = add(z, z);
  const auto xx = mul(x, x2), wx = mul(w, x2), xy = mul(x, y2), yy = mul(y, y2),
             wy = mul(w, y2);
  const auto xz = mul(x, z2), yz = mul(y, z2), zz = mul(z, z2), wz = mul(w, z2);
  std::array<std::array<Raw, 4>, 3> local{
      {{sub(sub(ONE, yy), zz), sub(add(ZERO, xy), wz), add(add(ZERO, xz), wy),
        p[0]},
       {add(xy, wz), sub(sub(ONE, xx), zz), sub(add(ZERO, yz), wx), p[1]},
       {sub(xz, wy), add(yz, wx), sub(sub(ONE, xx), yy), p[2]}}};
  if (depth < 0 || depth > 83)
    throw HostFault("joint depth outside VU data memory");
  const auto base = static_cast<std::size_t>(depth) * 3;
  auto &mem = storage_->vu_mem;
  std::array<std::array<Raw, 4>, 3> world{};
  for (unsigned r = 0; r < 3; ++r)
    for (unsigned c = 0; c < 4; ++c) {
      auto acc = mul(local[0][c], mem[base + r][0]);
      acc = add(acc, mul(local[1][c], mem[base + r][1]));
      acc = add(acc, mul(local[2][c], mem[base + r][2]));
      world[r][c] = add(acc, mul(c == 3 ? ONE : ZERO, mem[base + r][3]));
    }
  for (unsigned r = 0; r < 3; ++r)
    mem[base + 3 + r] = world[r];
  Vec3 offset{};
  for (unsigned k = 0; k < 3; ++k)
    offset[k] = mul(from_int(static_cast<Raw>(bind[k] * 64)), 0x39800000);
  PoseRows out{};
  for (unsigned r = 0; r < 3; ++r) {
    auto acc = mul(offset[0], world[r][0]);
    acc = add(acc, mul(offset[1], world[r][1]));
    acc = add(acc, mul(offset[2], world[r][2]));
    auto t = mul(add(acc, mul(ONE, world[r][3])), 0x40800000);
    for (unsigned c = 0; c < 3; ++c)
      out[r][c] = static_cast<std::int16_t>(
          s16(static_cast<Raw>(ftoi(world[r][c], 12))));
    out[r][3] = static_cast<std::int16_t>(s16(static_cast<Raw>(ftoi(t, 4))));
  }
  return out;
}
const Pose *Runtime::build_pose(StateId id, bool for_draw) {
  auto &s = checked_state(id);
  auto &pool = storage_->poses;
  std::size_t token = 0;
  const auto limit = s32(draw_frame() - 3);
  for (auto n : storage_->pose_order) {
    if (n == storage_->pose_order.back())
      break;
    if (s32(pool[n - 1].stamp) < limit) {
      token = n;
      break;
    }
  }
  if (!token) {
    if (for_draw) {
      s.pose = 0;
      return nullptr;
    }
    token = pool.size() - 1;
  }
  s.pose = token;
  auto base = s.count - 1;
  std::uint64_t mask = UINT64_MAX;
  for (; base >= 0; --base) {
    const auto &t = track(s.tracks[base]);
    if (le(ONE, fmin(t.weight, t.life)) && le(ZERO, t.time)) {
      mask &= clip(t.clip).joint_mask;
      if (!mask)
        break;
    }
  }
  if (base < 0) {
    s.pose = 0;
    return nullptr;
  }
  auto &buf = pool[token - 1];
  if (buf.owner == id)
    throw HostFault("pose buffer already belongs to state");
  if (buf.owner)
    const_cast<AnimState &>(state(buf.owner)).pose = 0;
  buf.owner = id;
  pose_to_tail(token);
  auto &t = track(s.tracks[base]);
  const auto count = clip(t.clip).joint_count;
  auto sid = animtrack_decode(s.tracks[base]);
  std::array<Quat, 64> qs{};
  std::array<Vec3, 64> ps{};
  for (int j = 0; j < count; ++j) {
    qs[j] = joint_quat(sid, j, t.time);
    ps[j] = joint_pos(sid, j, t.time);
    if (!j && (t.flags & YAW))
      qs[j] = strip_yaw(qs[j]);
    if (!j && (t.flags & ROOT))
      ps[j][0] = ps[j][1] = ZERO;
  }
  for (auto i = base + 1; i < s.count; ++i) {
    auto &layer = track(s.tracks[i]);
    if (lt(layer.time, ZERO))
      continue;
    auto w = fmin(fmax(fmin(layer.weight, layer.life), ZERO), ONE);
    if (layer.flags & EASE)
      w = hermite(w, ZERO, ONE, ZERO, ZERO);
    if (lt(w, 0x3a83126e))
      continue;
    sid = animtrack_decode(s.tracks[i]);
    const auto rest = sub(ONE, w);
    for (int j = 0; j < count; ++j) {
      if ((clip(layer.clip).joint_mask >> j) & 1)
        continue;
      auto q2 = joint_quat(sid, j, layer.time);
      if (!j && (layer.flags & YAW))
        q2 = strip_yaw(q2);
      const auto q1 = qs[j];
      Raw total = 0, dist = 0;
      bool first = true;
      for (auto k : {3, 0, 1, 2}) {
        const auto plus = add(q1[k], q2[k]), minus = sub(q1[k], q2[k]);
        total = first ? mul(plus, plus) : add(total, mul(plus, plus));
        dist = first ? mul(minus, minus) : add(dist, mul(minus, minus));
        first = false;
      }
      if (lt(total, dist))
        for (auto &v : q2)
          v = neg(v);
      auto w2 = q2[3];
      unsigned guard = 0;
      if (lt(0x40490fda, sub(w2, q1[3])))
        do {
          w2 = sub(w2, 0x40c90fda);
          ++guard;
          if (!lt(0x40490fda, sub(w2, q1[3])))
            break;
          if (guard > 100000)
            throw HostFault("quaternion unwrap does not end");
        } while (true);
      if (lt(sub(q1[3], w2), 0xc0490fda))
        do {
          w2 = add(w2, 0x40c90fda);
          ++guard;
          if (!lt(sub(q1[3], w2), 0xc0490fda))
            break;
          if (guard > 100000)
            throw HostFault("quaternion unwrap does not end");
        } while (true);
      q2[3] = w2;
      for (unsigned k = 0; k < 4; ++k)
        qs[j][k] = add(mul(q1[k], rest), mul(q2[k], w));
      auto p2 = joint_pos(sid, j, layer.time);
      for (unsigned k = 0; k < 2; ++k)
        ps[j][k] = !j && (layer.flags & ROOT)
                       ? mul(ps[j][k], rest)
                       : add(mul(ps[j][k], rest), mul(p2[k], w));
      ps[j][2] = add(mul(ps[j][2], rest), mul(p2[2], w));
    }
  }
  buf.joint_count = count;
  buf.tag = ((count * 3 + 1) << 16) | 0x6d008000;
  const auto &skel = clip(track(s.tracks[s.count - 1]).clip);
  reset_stack();
  for (int j = 0; j < count; ++j) {
    if (j >= skel.joint_count)
      throw HostFault("top clip has no skeleton entry");
    buf.mats[j] = compose_joint(quat_normalize(qs[j]), ps[j], skel.bind.at(j),
                                skel.depth.at(j));
  }
  if (count < 64)
    buf.mats[count] = {};
  return &buf;
}
const Pose &Runtime::draw_pose(StateId id) {
  auto &s = checked_state(id);
  auto &pool = storage_->poses;
  if (s.pose == pool.size() - 1)
    detach_pose(s);
  if (!s.pose)
    build_pose(id, true);
  if (!s.pose)
    return pool.back();
  auto &p = pool[s.pose - 1];
  p.stamp = draw_frame();
  pose_to_tail(s.pose);
  return p;
}
Mat34 Runtime::joint_matrix(StateId id, std::int32_t joint) {
  auto &s = checked_state(id);
  if (!s.pose)
    build_pose(id);
  const auto &p = s.pose ? storage_->poses[s.pose - 1] : storage_->poses.back();
  const auto j = std::min(std::max(joint, 0), p.joint_count - 1);
  if (j < 0)
    throw HostFault("pose has no joints");
  Mat34 out{};
  for (unsigned r = 0; r < 3; ++r)
    for (unsigned k = 0; k < 3; ++k)
      out[r * 3 + k] =
          mul(from_int(static_cast<Raw>(p.mats[j][k][r])), 0x39800000);
  for (unsigned k = 0; k < 3; ++k)
    out[9 + k] = mul(from_int(static_cast<Raw>(p.mats[j][k][3])), 0x3c800000);
  return out;
}
void Runtime::rebase_root(TrackId id) {
  auto &t = track(id);
  auto &r = slot(animtrack_decode(id)).joints[0];
  auto dt = sub(t.time, from_int(static_cast<Raw>(r.pt)));
  const auto y = add(r.pos[1], mul(r.vel[1], dt)),
             x = add(r.pos[0], mul(r.vel[0], dt));
  r.pos[1] = sub(r.pos[1], y);
  r.pos[0] = sub(r.pos[0], x);
}
Motion Runtime::creature_step(Raw speed, StateId id, std::int32_t facing,
                              Raw scale, std::uint8_t slow, bool active,
                              Motion previous) {
  const bool slowed = slow >= 1 && slow <= 60;
  auto factor = from_int(60 - slow);
  auto m = previous;
  m.events = 0;
  if (active)
    m = advance(slowed ? mul(mul(speed, factor), 0x3c888888) : speed, id);
  const auto x = neg(mul(m.x, scale)), y = mul(m.y, scale);
  const auto angle = s16(static_cast<Raw>(facing)) - 0x4000;
  const auto c = cos16(angle), sn = sin16(angle);
  m.x = add(mul(x, c), mul(y, sn));
  m.y = sub(mul(x, sn), mul(y, c));
  if (slowed) {
    auto f = mul(factor, 0x3c888888);
    m.y = mul(m.y, f);
    m.x = mul(m.x, f);
    m.yaw = s16(m.yaw * to_int(f));
  }
  return m;
}
} // namespace champions::simulation::rta::creature_anim
