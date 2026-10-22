#include "combat_runtime_internal.hpp"
#include "champions/player_wading.hpp"
#include <cstdlib>
// Round-2 movement (research ref_combat_move.MoveModel, executed by
// difftest_combat_move): obj_moveWithCollision 0x14d4b8, Player_move 0x1b4020,
// Player_animStep 0x1b9520 and Player_clampToScreen 0x1b3d90.
namespace champions::simulation::rta::combat_runtime {
namespace {
constexpr Raw DT = 0x3c888888, EPS = 0x3a83126e, SIXTY = 0x42700000,
              K001 = 0x3c23d70a, SPELL_CHARGE = 0x3cce9c,
              SPELL_DROWSY = 0x3d1234;
constexpr Address G_PVP_WORD_C = 0x4f7124, G_TURN_WORD = 0x4d8a28,
                  S_WATMEDAL = 0x537678, G_SCREEN_NORMALS = 0x4f8350,
                  G_SCREEN_POINTS = 0x4f8380, G_MOVE_VELOCITY = 0x8657a0;
constexpr std::uint64_t WATER_BIT = UINT64_C(1) << 61,
                        D_045 = UINT64_C(0x3fdccccccccccccc),
                        D_07 = UINT64_C(0x3fe6666666666666),
                        D_3 = UINT64_C(0x4008000000000000);
bool feq(Raw a, Raw b) { return eq(a, b); }
} // namespace
Raw dbl_track_weight(Raw w, Raw s, std::uint64_t knee) {
  // float((double(w) x (double(s) - knee)) x 3.0) through the soft doubles;
  // a zero product keeps the sign of the operands (0x80000000 for -0.0).
  auto diff = combat::dbl_add(combat::dbl_from_single(s), knee ^ D_SIGN);
  auto q = combat::dbl_mul(
      combat::dbl_mul(combat::dbl_from_single(w), diff), D_3);
  if (!q) {
    bool negative_diff = (diff >> 63) && diff;
    return ((w >> 31) ^ Raw(negative_diff)) ? SIGN : 0;
  }
  return combat::dbl_to_single(q);
}
std::optional<Raw> World::State::slow_factor(Address c) const {
  auto n = b(c + 0x2fc);
  if (n < 1 || n > 60)
    return std::nullopt;
  return mul(from_int(60 - s8m(c + 0x2fc)), DT);
}
bool World::State::dialog_open() {
  if (!services.dialog_is_open)
    throw Unsupported("dialog state service missing");
  auto fn = services.dialog_is_open;
  return fn();
}
Raw World::State::camera_yaw(Raw slot) {
  if (!services.camera_yaw)
    throw Unsupported("camera yaw service missing");
  auto fn = services.camera_yaw;
  return fn(slot);
}
Raw World::State::camera_flag(Raw slot) {
  if (!full())
    return 0;
  if (!services.camera_flag)
    throw Unsupported("camera flag service missing");
  auto fn = services.camera_flag;
  return fn(slot);
}
Raw World::State::move_with_collision(Address obj, const Vec3 &vel,
                                      Address ignore, Raw skip) {
  Vec3 d{mul(vel[0], DT), mul(vel[1], DT), mul(vel[2], DT)};
  Raw bits = 0;
  if (lt(EPS, add(add(mul(d[0], d[0]), mul(d[1], d[1])), mul(d[2], d[2])))) {
    auto flags = u(obj + 12);
    Raw mask = flags & 0x20 ? 3 : 1;
    if (flags & 0x40)
      mask = (mask | 4) & 5;
    if (!services.slide)
      throw Unsupported("slide service missing");
    auto fn = services.slide;
    auto out = fn(vec(obj + 16), d, mask, u(obj + 0x1c));
    vec(obj + 16, out.position);
    bits = out.hit ? 1 : 0;
    grid(obj);
  }
  auto p = vec(obj + 16);
  auto r = u(obj + 0x1c);
  auto found = box({sub(p[0], r), sub(p[1], r), sub(p[2], 0x447a0000)},
                   {add(p[0], r), add(p[1], r), add(p[2], 0x447a0000)}, 0x40);
  auto top = add(u(obj + 0x18), u(obj + 0x20));
  Raw stand = 0xc7c35000;
  r = u(obj + 0x1c);
  for (auto o : found) {
    auto oflags = u(o + 12);
    if ((oflags & 1) || o == obj || o == ignore || (skip && (oflags & 0x20)))
      continue;
    auto dx = sub(u(obj + 16), u(o + 16)), dy = sub(u(obj + 20), u(o + 20));
    auto dist = sqrt(add(mul(dx, dx), mul(dy, dy)));
    auto orad = u(o + 0x1c);
    if (!lt(add(dist, EPS), add(r, orad)))
      continue;
    auto mf = u(obj + 12);
    bool push = true;
    if ((oflags | mf) & 0x10) {
      auto oz = u(o + 0x18);
      if (lt(top, add(oz, 0x40400000)))
        continue;
      auto otop = add(oz, u(o + 0x20));
      auto z0 = u(obj + 0x18);
      if (lt(otop, z0))
        continue;
      if (lt(sub(otop, z0), 0x40a00000)) {
        if (!((oflags & 0x200) || (mf & 0x400))) {
          if (lt(stand, otop))
            stand = otop;
          continue;
        }
        push = false;
      }
    }
    if (push && !((oflags & 0x200) || (mf & 0x400)) && lt(EPS, fabs(dist))) {
      auto k = div(sub(add(r, orad), dist), dist);
      auto fn = services.slide;
      auto out = fn(vec(obj + 16), {mul(dx, k), mul(dy, k), 0}, 1, r);
      vec(obj + 16, out.position);
      grid(obj);
      bits |= 2;
      mf = u(obj + 12);
    }
    if (!(mf & 0x20000))
      vcall(o, 6, {iv(obj)});
    vcall(obj, 6, {iv(o)});
    r = u(obj + 0x1c);
  }
  if (!services.height_below)
    throw Unsupported("height service missing");
  auto hb = services.height_below;
  auto ground = hb(vec(obj + 16), r);
  if (lt(ground, stand))
    ground = stand;
  auto vz = sub(u(obj + 0x30), 0x410fffff);
  w(obj + 0x30, vz);
  auto nz = add(u(obj + 0x18), mul(vz, DT));
  w(obj + 0x18, nz);
  if (le(sub(nz, K001), ground)) {
    w(obj + 0x18, ground);
    if (lt(vz, 0)) {
      w(obj + 0x30, 0);
      bits |= 4;
    }
  }
  return bits;
}
Raw World::State::anim_advance_root(Address actor, Raw speed, Address root) {
  auto ev = advance_anim(actor, speed);
  if (root && full()) {
    if (!services.animation_root)
      throw Unsupported("animation root-motion service missing");
    auto fn = services.animation_root;
    auto r = fn(actor);
    poke(root, 4, r[0]);
    poke(root + 4, 4, r[1]);
  }
  return ev;
}
Raw World::State::player_anim_step(Address p, Raw speed) {
  if (s16m(p + 0x31a) > 0)
    if (auto spell = find_spell_by_type(p, SPELL_DROWSY))
      (void)spell, throw Unsupported("SpellDrowsy statistic");
  auto n = b(p + 0x2fc);
  if (n >= 1 && n <= 60)
    speed = mul(mul(speed, from_int(60 - s8m(p + 0x2fc))), DT);
  auto ev = anim_advance_root(p, speed, p + 0xbc);
  auto f = s16m(p + 0x38);
  auto c = cos16(Raw(f - 0x4000)), sn = sin16(Raw(f - 0x4000));
  auto rx = neg(u(p + 0xbc)), ry = u(p + 0xc0);
  auto ny = sub(mul(rx, sn), mul(ry, c));
  auto nx = add(mul(rx, c), mul(ry, sn));
  w(p + 0xc0, ny);
  w(p + 0xbc, nx);
  return ev;
}
void World::State::screen_planes(Address pos) {
  if (!services.screen_planes)
    throw Unsupported("the screen clamp needs the camera frustum (per machine)");
  auto fn = services.screen_planes;
  auto planes = fn(vec(pos));
  for (unsigned i = 0; i < 4; ++i)
    for (unsigned j = 0; j < 3; ++j) {
      poke(G_SCREEN_POINTS + 12 * i + 4 * j, 4, planes[i].point[j]);
      poke(G_SCREEN_NORMALS + 12 * i + 4 * j, 4, planes[i].normal[j]);
    }
  note("camera_planes");
}
World::State::Clamp World::State::clamp_on_image(Address p, const Vec3 &v,
                                                 Address maskptr) {
  Vec3 step{mul(v[0], DT), mul(v[1], DT), mul(v[2], DT)};
  screen_planes(p + 16);
  auto dot_delta = [&](Address point, Address normal) {
    auto pt = vec(point), n = vec(normal), q = vec(p + 16);
    return add(add(mul(n[0], sub(q[0], pt[0])), mul(n[1], sub(q[1], pt[1]))),
               mul(n[2], sub(q[2], pt[2])));
  };
  bool outside = false;
  for (unsigned i = 0; i < 4 && !outside; ++i)
    outside = lt(dot_delta(G_SCREEN_POINTS + 12 * i, G_SCREEN_NORMALS + 12 * i),
                 0);
  if (!outside)
    return {0, v, 0};
  bool used = false;
  Raw mask = 0;
  for (unsigned i = 0; i < 4; ++i) {
    auto nrm = G_SCREEN_NORMALS + 12 * i;
    if (!lt(dot_delta(G_SCREEN_POINTS + 12 * i, nrm), 0))
      continue;
    mask |= 1u << i;
    if (maskptr)
      w(maskptr, mask);
    auto n = vec(nrm);
    auto dot =
        add(add(mul(step[0], n[0]), mul(step[1], n[1])), mul(step[2], n[2]));
    if (!lt(dot, 0))
      continue;
    if (used) {
      step = {0, 0, 0};
      break;
    }
    used = true;
    for (unsigned j = 0; j < 3; ++j)
      step[j] = sub(step[j], mul(n[j], dot));
  }
  if (used)
    return {1, {mul(step[0], SIXTY), mul(step[1], SIXTY), mul(step[2], SIXTY)},
            mask};
  return {0, v, mask};
}
Raw World::State::clamp_to_screen(Address p, Address vptr, Address maskptr) {
  w(maskptr, 0);
  auto out = clamp_on_image(p, vec(vptr), maskptr);
  if (out.result)
    wvec(vptr, out.velocity);
  return out.result;
}
Raw World::State::speed_scale(Address p) {
  Raw base = 0x3f5ca86d;
  if (s8m(G_NETWORK) && (u(G_PVP_B) == 0 || u(G_PVP_WORD_C) != 0))
    base = 0x3f400000;
  if (cstr(G_LEVEL_NAME, 256) == cstr(S_WATMEDAL, 256))
    base = 0x3f5ca86d;
  auto slot = s32m(p + 0x3c8);
  if (slot != -1)
    base = mul(base, u(Raw(PLAYERS + 0x4c20 + Raw(slot) * RECORD_SIZE)));
  auto s = mul(base, u(G_RUN_SPEED));
  if (lt(0x3f999999, s))
    s = 0x3f999999;
  if (s16m(p + 0x31a) > 0 && find_spell_by_type(p, SPELL_DROWSY))
    throw Unsupported("SpellDrowsy statistic");
  return s;
}
void World::State::player_water(Address p, Raw s) {
  // Player_move step (f): champions/player_wading.hpp (t3/research-wading).
  // The depth band is the two services' own query boxes; nothing here changes
  // the player's speed, facing, blend or velocity.
  namespace PW = player_wading;
  PW::Host host;
  host.water_test = [this](const PW::Vec3 &at, Raw radius) {
    if (!services.water_test)
      throw Unsupported("water query service missing");
    PW::TestResult r;
    r.hit = services.water_test(at, radius) != 0;
    if (r.hit && services.water_height) {
      r.has_height = true;
      r.height = services.water_height(at, radius);
    }
    return r;
  };
  host.water_disturb = [this](const PW::Vec3 &at, Raw strength, Raw radius) {
    if (!services.water_disturb)
      throw Unsupported("water_disturb service missing");
    note("presentation:water_disturb");
    return services.water_disturb(at, strength, radius);
  };
  host.hurt = [this, p] {
    damage_full(p, p, PW::HURT_AMOUNT, 0, 0, PW::HURT_FLAGS, 0, PW::HURT_RANGE,
                PW::HURT_RANGE);
  };
  host.draw = [this] { draw(); };
  host.splash = [this](Raw volume) {
    note("presentation:sound_playOnObject", {volume});
  };
  host.burst = [this](const PW::Vec3 &at, const PW::TestResult &t) {
    // The height is the chunk's minimum z from water_test (research
    // player-wading.md section 3); with no height service it is not recorded.
    if (t.has_height)
      note("presentation:fx_burst_1fbdf0", {at[0], at[1], t.height});
    else
      note("presentation:fx_burst_1fbdf0", {at[0], at[1]});
  };
  PW::Input in;
  in.frame = u(FRAME);
  in.level_flags = u(G_LEVEL_FLAGS);
  in.speed = s;
  in.position = vec(p + 16);
  in.in_water = (q(p + 0xa8) & WATER_BIT) != 0;
  in.airborne = b(p + 0x47c) != 0;
  const bool wet = PW::step(in, host);
  if (wet != in.in_water)
    w(p + 0xa8, 8, wet ? (q(p + 0xa8) | WATER_BIT) : (q(p + 0xa8) & ~WATER_BIT));
}
Raw World::State::locomotion(Address p, Raw s) {
  Raw rate;
  if (lt(s, 0x3edc28f5))
    rate = mul(s, 0x4014d653);
  else if (lt(s, 0x3f333333))
    rate = add(mul(mul(sub(s, 0x3edc28f5), 0x406d097a), 0x3eb33330), ONE);
  else
    rate = add(mul(mul(sub(s, 0x3f333333), 0x40555554), 0x3e1999a0),
               0x3faccccc);
  auto wt = u(p + 0x46c);
  if (lt(K001, rate)) {
    wt = add(wt, 0x3dcccccc);
    w(p + 0x46c, wt);
    if (lt(ONE, wt)) {
      w(p + 0x46c, ONE);
      w(p + 0x134, 0);
    }
  } else {
    wt = sub(wt, 0x3dcccccc);
    w(p + 0x46c, wt);
    if (lt(wt, 0))
      w(p + 0x46c, 0);
  }
  Raw ev;
  if (lt(rate, K001) && lt(u(p + 0x46c), 0x3d4ccccc)) {
    player_anim_step(p, ONE);
    ev = 0;
  } else {
    ev = Raw(byte(player_anim_step(p, rate) & 0xff));
    ev &= le(0x3f199999, s) ? 4 : 1;
  }
  auto prev = u(p + 0x468);
  if (lt(sub(prev, s), 0xbd4ccccc))
    s = add(prev, 0x3d4ccccc);
  wt = u(p + 0x46c);
  w(p + 0x14c, wt);
  w(p + 0x144, wt);
  auto t2 = dbl_track_weight(wt, s, D_045);
  w(p + 0x164, t2);
  w(p + 0x16c, t2);
  auto t3 = dbl_track_weight(wt, s, D_07);
  w(p + 0x468, s);
  w(p + 0x184, t3);
  w(p + 0x18c, t3);
  return ev;
}
Raw World::State::player_move(Address p, Raw mode_raw, Address pad,
                              const Vec3 &ext) {
  auto mode = s32(mode_raw);
  auto slot = u(p + 0x3c8);
  auto old = vec(p + 16);
  Vec3 v{0, 0, 0};
  Raw ev = 0;
  // (a) stick suppression
  if (auto n = b(p + 0x481)) {
    w(p + 0x481, 1, n - 1);
    w(pad + 0x14, 0);
    w(pad + 0x18, 0);
  }
  if (dialog_open()) {
    w(pad + 0x18, 0);
    w(pad + 0x14, 0);
  }
  // (b), (c)
  auto S = speed_scale(p);
  auto lx = u(pad + 0x14), ly = u(pad + 0x18);
  auto m = sqrt(add(mul(lx, lx), mul(ly, ly)));
  auto aim = atan2_16(lx, ly);
  auto s = mul(S, m);
  auto yaw = s32(camera_yaw(slot));
  auto target = s16(Raw(aim - yaw - 0x41c2));
  if (lt(S, s))
    s = S;
  // (d)
  Raw cs = 0, cc = 0;
  if (mode != 5 && lt(K001, s)) {
    if (mode != 3) {
      auto f = s16m(p + 0x38);
      Raw rate;
      if (mode == 1) {
        if (s32m(p + 0x58c) == 8) {
          auto a = mul(from_int(std::abs(s32(Raw(target - f)))), 0x393ea672);
          if (lt(ONE, a))
            a = ONE;
          w(p + 0x1d4, add(u(p + 0x1d4), mul(a, 0x3f7fffff)));
        }
        rate = 0x3e4ccccc;
      } else if (mode == 4)
        rate = 0x3d5266e5;
      else if (mode == 6)
        rate = mul(s, 0x3da3d70a);
      else if (u(G_TURN_WORD))
        rate = mul(s, 0x3d0f5c28);
      else {
        auto spell = find_spell_by_type(p, SPELL_CHARGE);
        if (mode == 2 && spell)
          throw Unsupported("SpellCharge turn rate statistic");
        rate = 0x3ecccccc;
      }
      w(p + 0x38, 2,
        Raw(angle_approach(rate, MINUS_ONE, MINUS_ONE, Raw(f), Raw(target))));
    }
    auto yaw2 = s32(camera_yaw(slot));
    cs = mul(sin16(Raw(s16m(p + 0x38) + yaw2 + 0x4000)), s);
    auto yaw3 = s32(camera_yaw(slot));
    cc = mul(cos16(Raw(s16m(p + 0x38) + yaw3 + 0x4000)), s);
  }
  // (e)
  if (mode == 2 || s32m(p + 0x58c) == 1)
    s = ONE;
  // (f)
  player_water(p, s);
  // (g), (h)
  if (mode == 0) {
    ev = locomotion(p, s);
    auto yc = s32(camera_yaw(slot));
    auto a1 = Raw(-yc - 0x4000), a2 = Raw(-yc - 0x8000);
    Vec3 A{neg(cos16(a1)), neg(sin16(a1)), 0}, B{neg(cos16(a2)),
                                                 neg(sin16(a2)), 0};
    auto pp = mul(cc, 0xc3340000), qq = mul(cs, 0x43340000);
    for (unsigned i = 0; i < 3; ++i)
      v[i] = add(mul(A[i], pp), mul(B[i], qq));
    if (b(p + 0x47e) && b(p + 0x47c))
      for (auto &x : v)
        x = mul(x, 0x3fa00000);
    w(p + 0x4bc, v[0]);
    w(p + 0x4c0, v[1]);
  } else if (mode == 1 || mode == 4 || mode == 6) {
    for (unsigned off : {0x46c, 0x184, 0x14c, 0x144, 0x16c, 0x164, 0x18c})
      w(p + off, 0);
  } else if (mode == 2) {
    auto f = Raw(s16m(p + 0x38));
    v = {mul(cos16(f), 0x43960000), mul(sin16(f), 0x43960000), 0};
  } else if (mode == 5)
    v = ext;
  // (i) the swing window of action state 1
  bool heavy = s32m(p + 0x44c) == 2;
  if (!heavy)
    w(p + 0x490, 1, 0);
  if (s32m(p + 0x58c) == 1) {
    auto cls = s8m(Raw(PLAYERS + 0x4a86 + Raw(s32(slot)) * RECORD_SIZE));
    auto t = from_int(s32(u(FRAME) - u(p + 0x448)));
    Raw lo, hi;
    if (cls == 2) {
      lo = mul(heavy ? 0x3dcccccc : 0x3e199999, SIXTY);
      hi = 0x41b40000;
    } else if (cls == 1) {
      lo = 0x4220147a;
      hi = 0x42b40000;
    } else if (cls == 0) {
      lo = 0x410fffff;
      hi = heavy ? 0x4206a3d6 : 0x41b40000;
    } else if (cls == 3) {
      lo = mul(heavy ? 0x3dcccccc : 0x3eddb22d, SIXTY);
      hi = 0x42c3f5c1;
    } else if (heavy) {
      lo = 0x4130a3d6;
      hi = 0x4218e147;
    } else {
      lo = 0x40c5c28e;
      hi = 0x41bbae13;
    }
    if (le(lo, t) && le(t, hi))
      v[0] = v[1] = 0;
    if (heavy && (b(p + 0x490) & 3))
      v[0] = v[1] = 0;
  }
  // (j)
  if (auto k = slow_factor(p))
    for (auto &x : v)
      x = mul(x, *k);
  // (k)
  Raw mask = 0;
  bool clamp = s32m(G_ALIVE_COUNT) >= 2 && !s8m(G_NETWORK);
  if (clamp) {
    auto out = clamp_on_image(p, v, 0);
    v = out.velocity;
    mask = out.mask;
  }
  // (l)
  auto pos = vec(p + 16);
  Vec3 goal{add(pos[0], mul(v[0], DT)), add(pos[1], mul(v[1], DT)), pos[2]};
  Vec3 wv{add(v[0], mul(u(p + 0xbc), SIXTY)), add(v[1], mul(u(p + 0xc0), SIXTY)),
          v[2]};
  wvec(G_MOVE_VELOCITY, wv);
  auto bits = move_with_collision(p, wv, 0, 0);
  w(p + 0xbc, 0);
  w(p + 0xc0, 0);
  // (m)
  if (bits & 3)
    w(p + 0x490, 1, bits & 0xff);
  if ((bits & 2) && !(u(p + 12) & 0x20000))
    w(G_GHOST_REBASE, 1, 1);
  if (!(bits & 3)) {
    auto vx = u(p + 0x4bc), vy = u(p + 0x4c0);
    w(p + 0x308, vx);
    w(p + 0x30c, vy);
    if (auto k = slow_factor(p)) {
      w(p + 0x308, mul(vx, *k));
      w(p + 0x30c, mul(vy, *k));
    }
  } else {
    w(p + 0x308, 0);
    w(p + 0x30c, 0);
  }
  if (bits & 4) {
    w(p + 0x482, 1, 15);
    if (b(p + 0x47d)) {
      note("presentation:Player_footstep");
      w(p + 0x58c, 2);
      w(Raw(p + 0x110 + 32 * Raw(s8m(p + 0x118))), 0xbd6eeeee);
      w(p + 0x47c, 1, 0);
      w(p + 0x47d, 1, 0);
    }
    if (!(u(p + 12) & 0x20000) && mode == 2 && (bits & 3))
      throw Unsupported("SpellCharge_onContact");
  } else {
    auto c = (b(p + 0x482) - 1) & 0xff;
    w(p + 0x482, 1, c);
    if (byte(c) < 0)
      w(p + 0x482, 1, 0);
  }
  // (n)
  if (!camera_flag(slot) && clamp) {
    auto now = vec(p + 16);
    if (!(feq(goal[0], now[0]) && feq(goal[1], now[1]))) {
      Vec3 d{sub(now[0], old[0]), sub(now[1], old[1]), sub(now[2], old[2])};
      for (unsigned i = 0; i < 4; ++i) {
        if (!((mask >> i) & 1))
          continue;
        auto n = vec(G_SCREEN_NORMALS + 12 * i);
        auto dot = add(add(mul(d[0], n[0]), mul(d[1], n[1])), mul(d[2], n[2]));
        if (lt(dot, 0)) {
          w(p + 20, old[1]);
          w(p + 16, old[0]);
          break;
        }
      }
    }
  }
  return ev;
}
} // namespace champions::simulation::rta::combat_runtime
