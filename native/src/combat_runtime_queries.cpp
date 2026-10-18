#include "combat_runtime_internal.hpp"
#include <cstdlib>
namespace champions::simulation::rta::combat_runtime {
Raw World::State::ray(const Vec3 &o, const Vec3 &d, Raw mask, Raw f) {
  if (!services.raycast)
    throw Unsupported("terrain ray service missing");
  auto fn = services.raycast;
  auto t = fn(o, d, mask, f);
  note("ray", {mask, f, t, o[0], o[1], o[2], d[0], d[1], d[2]});
  return t;
}
int World::State::can_see(Address c, Address o, Raw face, Raw arc, Raw mode,
                          Raw near, Raw far, Raw lead) {
  actor(c);
  if (!o)
    return 0;
  actor(o);
  auto p = vec(o + 16);
  if (!eq(lead, 0) && (u(o + 12) & 0x20)) {
    p[0] = add(p[0], mul(u(o + 0x308), lead));
    p[1] = add(p[1], mul(u(o + 0x30c), lead));
  }
  auto cp = vec(c + 16);
  Vec3 v;
  for (unsigned k = 0; k < 3; ++k)
    v[k] = sub(p[k], cp[k]);
  if (std::abs(s16(Raw(atan2_16(v[1], v[0])) - face)) >= s16(arc))
    return 0;
  auto x = sub(cp[0], p[0]), y = sub(cp[1], p[1]);
  auto d = sqrt(add(mul(x, x), mul(y, y))), orad = u(o + 0x1c),
       crad = u(c + 0x1c);
  if (!lt(sub(sub(d, orad), crad), far) || !le(near, sub(add(d, orad), crad)))
    return 0;
  cp[2] = add(cp[2], mul(u(c + 0x20), HALF));
  auto t = ray(cp, v, mode ? 0 : 3, 0x3f8ccccc);
  return lt(t, 0) || lt(ONE, t);
}
int World::State::hit_arc(Address c, const Arc &a, bool all) {
  actor_domain(c);
  if (!all) {
    auto t = target(c + TARGET);
    if (!t || !can_see(c, t, a.facing, a.angle, 0, a.near, a.far, 0))
      return 0;
    return damage(target(c + TARGET), c, from_int(a.minimum),
                  from_int(a.maximum), a.attack, a.flags);
  }
  auto p = vec(c + 16);
  auto h = mul(a.far, 0x3fb33333);
  Vec3 lo, hi;
  for (unsigned k = 0; k < 3; ++k) {
    lo[k] = sub(p[k], h);
    hi[k] = add(p[k], h);
  }
  auto list = box(lo, hi, 0x80);
  int any = 0;
  for (auto o : list)
    if (enemy(c, o) && can_see(c, o, a.facing, a.angle, 0, a.near, a.far, 0))
      if (damage(o, c, from_int(a.minimum), from_int(a.maximum), a.attack,
                 a.flags))
        any = 1;
  return any;
}
Address World::State::cone(Address c, Raw facing, Raw arc, Raw near, Raw far) {
  actor(c);
  auto best_d = mul(far, far);
  auto h = combat::dbl_to_single(combat::dbl_mul(combat::dbl_from_single(far),
                                                 UINT64_C(0x3ff6666666666666)));
  auto p = vec(c + 16);
  Vec3 lo, hi;
  for (unsigned k = 0; k < 3; ++k) {
    lo[k] = sub(p[k], h);
    hi[k] = add(p[k], h);
  }
  auto found = box(lo, hi, 0x80);
  Address best = 0;
  int best_score = 0;
  auto near2 = mul(near, near);
  for (auto o : found) {
    auto score = enemy(c, o);
    if (!score)
      continue;
    auto cp = vec(c + 16), op = vec(o + 16);
    Vec3 d;
    for (unsigned k = 0; k < 3; ++k)
      d[k] = sub(cp[k], op[k]);
    auto d2 = add(add(mul(d[0], d[0]), mul(d[1], d[1])), mul(d[2], d[2]));
    if (lt(d2, near2))
      continue;
    Vec3 v{sub(op[0], cp[0]), sub(op[1], cp[1]),
           sub(add(op[2], mul(u(o + 0x20), HALF)),
               add(cp[2], mul(u(c + 0x20), HALF)))};
    auto angle = s16(Raw(s16(0x4000 - Raw(atan2_16(v[0], v[1])))) - facing);
    if (s16(arc) < std::abs(angle))
      continue;
    cp[2] = add(cp[2], u(c + 0x20));
    auto t = ray(cp, v, 3, 0x3f8ccccc);
    if (le(0, t) && le(t, ONE))
      continue;
    if (lt(d2, best_d))
      ++score;
    if (target(c + LAST_ATTACKER) == o)
      ++score;
    if (target(c + TARGET) == o)
      ++score;
    if (score > best_score || (score == best_score && lt(d2, best_d))) {
      best = o;
      best_d = d2;
      best_score = score;
    }
  }
  return best;
}
int World::State::enemy(Address c, Address o) {
  actor(c);
  auto fn = u(u(c + 0x48) + 8 * 20 + 4);
  if (fn == 0x1b5ea8) {
    if (!o || c == o)
      return 0;
    actor(o);
    auto f = u(o + 12);
    if (!(f & 0x20) || (f & (8 | 0x1000000)) || (q(o + 0xa8) & 0x4000800))
      return 0;
    auto pvp = u(0x4f711c);
    if (pvp && !full())
      throw Unsupported("PvP enemy test");
    if (f & 0x40) {
      if (!pvp)
        return 0;
      return s8m(rec_of(o) + 0x4c24) != s8m(rec_of(c) + 0x4c24) ? 1 : 0;
    }
    if (s16(h(o + 0x312)) == 0)
      return 1;
    if (!pvp)
      return 0;
    auto team = s8m(o + 0x310);
    if (team < 0)
      return 0;
    return team != s8m(rec_of(c) + 0x4c24) ? 1 : 0;
  }
  if (fn != 0x119328)
    throw Unsupported("enemy virtual body");
  return s32(number(model.vcall(c, 20, {iv(o)})));
}
Raw World::State::move(Address c, const Vec3 &v) {
  if (full())
    actor(c);
  else
    actor_domain(c);
  return movement.move_by(c, v);
}
void World::State::knockback(Address c, Raw scale) {
  actor(c);
  auto flags = u(c + 12);
  if ((flags & 0x40) && (b(0x402791) || u(0x4f7120))) {
    w(c + 0x2e8, 0);
    return;
  }
  if (!lt(0, scale) || eq(u(c + 0x2e8), 0) || (flags & 2))
    return;
  auto speed = u(c + 0x2e8);
  Vec3 v{mul(mul(cos16(h(c + 0x2e0)), speed), scale),
         mul(mul(sin16(h(c + 0x2e0)), speed), scale), 0};
  auto root = q(c + 0xbc);
  w(c + 0xbc, 8, 0);
  auto yaw = h(c + 0xc4);
  w(c + 0xc4, 2, 0);
  auto bits = move(c, v);
  w(c + 0xbc, 8, root);
  w(c + 0xc4, 2, yaw);
  speed = u(c + 0x2e8);
  if (lt(0, speed)) {
    speed = sub(speed, TWO);
    w(c + 0x2e8, speed);
  }
  if (lt(speed, 0))
    w(c + 0x2e8, 0);
  if ((q(c + 0xa8) & UINT64_C(0x2000000000100000)) || (bits & 3))
    return;
  auto rem = [](Raw d, int n) {
    if (!n)
      throw Error("remainder by zero: dust radius below one");
    return s32(d) % n;
  };
  for (unsigned puff = 0; puff < 2; ++puff) {
    Vec3 spot, ball;
    auto r = u(c + 0x1c), d = own(c);
    spot[0] =
        add(sub(u(c + 16), mul(r, HALF)), from_int(Raw(rem(d, to_int(r)))));
    d = own(c);
    r = u(c + 0x1c);
    spot[1] =
        add(sub(u(c + 20), mul(r, HALF)), from_int(Raw(rem(d, to_int(r)))));
    d = own(c);
    spot[2] = add(u(c + 24), from_int(d % 10));
    unsigned attempts = 0;
    do {
      if (++attempts > 65536)
        throw Error("dust rejection budget");
      for (auto &x : ball) {
        auto unit = mul(from_int(own(c)), TWO_M16);
        x = sub(add(unit, unit), ONE);
      }
    } while (lt(ONE, add(add(mul(ball[0], ball[0]), mul(ball[1], ball[1])),
                         mul(ball[2], ball[2]))));
    for (auto &x : ball)
      x = mul(x, 0x41273333);
    Vec3 at;
    for (unsigned k = 0; k < 3; ++k)
      at[k] = add(from_int(own(c) % 12 - 6), spot[k]);
    note("dust", {at[0], at[1], at[2], ball[0], ball[1], ball[2]});
  }
}
int World::enemy(Address a, Address b) { return s_->enemy(a, b); }
void World::pick_target(Address a, bool force) {
  s_->actor(a);
  s_->model.vcall(a, 23, {iv(force)});
}
int World::can_see(Address a, Address b, Raw f, Raw arc, Raw m, Raw n, Raw far,
                   Raw l) {
  return s_->can_see(a, b, f, arc, m, n, far, l);
}
int World::hit_target_in_arc(Address a, const Arc &v) {
  return s_->hit_arc(a, v, false);
}
int World::hit_all_in_arc(Address a, const Arc &v) {
  return s_->hit_arc(a, v, true);
}
Address World::find_enemy_in_cone(Address a, Raw f, Raw ar, Raw n, Raw far) {
  return s_->cone(a, f, ar, n, far);
}
Raw World::move_creature(Address a, const Vec3 &v) { return s_->move(a, v); }
void World::apply_knockback(Address a, Raw f) { s_->knockback(a, f); }
void World::dead_update(Address a, Raw e, Raw h, Raw z, const Vec3 &off) {
  s_->dead_update(a, e, h, z, off);
}
} // namespace champions::simulation::rta::combat_runtime
