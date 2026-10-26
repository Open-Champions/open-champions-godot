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
  auto hit = [&](Address victim) {
    auto low = from_int(a.minimum), high = from_int(a.maximum);
    if (hit_descriptor)
      return damage_with_descriptor(*this, victim, c, a.attack, a.flags,
                                    *hit_descriptor, low, high);
    return damage(victim, c, low, high, a.attack, a.flags);
  };
  if (!all) {
    auto t = target(c + TARGET);
    if (!t || !can_see(c, t, a.facing, a.angle, 0, a.near, a.far, 0))
      return 0;
    return hit(t);
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
      if (hit(o))
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
// world_findFreeSpot 0x14acf8 (research t3/research-levels 13fafa4:
// ref_objects.World.find_free_spot; docs/level-flow.md 17.2 and 18.3).
// A try draws a shared-seed point in the unit ball and offsets the centre by
// its x and y times the spread. It fails on terrain_testFlag1, on a floor not
// below centre z + 6 (unless anyHeight), or on a box hit by an object
// without flag 0x1 (dx*dx + dy*dy <= radius*radius + r*r). After 50 failures
// ok is 0 and the point is the last one that passed the terrain tests, or
// the centre.
std::pair<Vec3, int> World::State::find_free_spot(const Vec3 &centre,
                                                  Raw spread, Raw radius,
                                                  Raw any_height) {
  if (!services.terrain_blocked || !services.terrain_height)
    throw Unsupported("world_findFreeSpot terrain service missing");
  constexpr Raw F_6 = 0x40c00000, F_100000 = 0x47c35000;
  const auto fs = from_int(spread), fr = from_int(radius);
  const auto radius_sq =
      from_int(Raw(std::int64_t(s32(radius)) * std::int64_t(s32(radius))));
  auto blocked = services.terrain_blocked;
  auto height = services.terrain_height;
  auto result = centre;
  unsigned tries = 0;
  while (tries < 50) {
    auto ball = shared_ball_round4();
    auto ox = mul(ball[0], fs), oy = mul(ball[1], fs);
    Vec3 cand{add(ox, centre[0]), add(oy, centre[1]), centre[2]};
    if (!eq(blocked(cand, fr), 0)) {
      ++tries;
      continue;
    }
    Vec3 probe{add(ox, centre[0]), add(oy, centre[1]), add(0, centre[2])};
    auto h = height(probe, fr);
    if (!lt(sub(h, centre[2]), F_6) && !any_height) {
      ++tries;
      continue;
    }
    result = {add(centre[0], ox), add(centre[1], oy), h};
    Vec3 lo{sub(result[0], fr), sub(result[1], fr), sub(h, F_100000)},
        hi{add(result[0], fr), add(result[1], fr), add(h, F_100000)};
    bool hit = false;
    for (auto o : box(lo, hi, 0x80)) {
      if (u(o + 12) & 1)
        continue;
      auto dy = sub(u(o + 20), result[1]), dx = sub(u(o + 16), result[0]);
      auto reach = add(radius_sq, mul(u(o + 0x1c), u(o + 0x1c)));
      if (le(add(mul(dx, dx), mul(dy, dy)), reach)) {
        hit = true;
        break;
      }
    }
    if (!hit)
      break;
    ++tries;
  }
  const int ok = tries != 50 ? 1 : 0;
  note("findFreeSpot", {centre[0], centre[1], centre[2], spread, radius,
                        any_height, result[0], result[1], result[2], Raw(ok)});
  return {result, ok};
}
} // namespace champions::simulation::rta::combat_runtime
