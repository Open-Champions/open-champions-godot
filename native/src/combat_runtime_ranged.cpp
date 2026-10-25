#include "combat_runtime_internal.hpp"
// Round-2 projectiles (research ref_combat_ranged.RangedModel and
// RangedRuntime, executed by difftest_combat_ranged): obj_moveAndCollide
// 0x1509f8, Projectile_ctor 0x150e48, MissileWeapon ctor/run/onTouch/slot 17/
// dtor, playerProjectile ctor/run/dtor. A projectile destroys itself (slot 1
// with 3) in its own run.
namespace champions::simulation::rta::combat_runtime {
namespace {
constexpr Raw F_DT = 0x3c888888, F_TENTH = 0x3dcccccc, F_TWO = 0x40000000,
              F_TWELVE = 0x41400000, F_GRAVITY = 0x410fffff,
              F_INV65536 = 0x37800000, F_FIVE = 0x40a00000,
              F_EIGHTEEN = 0x41900000;
constexpr Address S_FX_LUMP = 0x532478, S_FX_LUMP_THROWN = 0x535ef0,
                  S_STONE_HIT = 0x532538, S_STONE_HIT_THROWN = 0x537b70;
const unsigned PROJECTILE_HEADS[] = {0x4c, 0x5c, 0x74, 0x8c, 0xa4,
                                     0xbc, 0x124, 0x128};
// fx_spawn(id, position) as the projectile bodies call it (ref_combat_ranged):
// recorded with its arguments so the effect half answers it (review finding 7).
std::vector<std::uint64_t> fx_args(Raw id, const Vec3 &at) { return {id, at[0], at[1], at[2]}; }
} // namespace
Raw World::State::obj_update_active(Address obj, Raw was, Raw radius) {
  // obj_updateActive 0x14db08 (ref_creature.Model.obj_update_active).
  Raw r = (u(obj + 12) & 0x80000) ? 0xabe0 : radius;
  auto r2 = from_int(s32(Raw(std::uint64_t(r) * r)));
  auto p = vec(obj + 16);
  auto near = [&](Address base) {
    auto dx = sub(p[0], u(base)), dy = sub(p[1], u(base + 4)),
         dz = sub(p[2], u(base + 8));
    return lt(add(add(mul(dx, dx), mul(dy, dy)), mul(dz, dz)), r2);
  };
  Raw now = 0;
  if (s8m(0x4039c1))
    now = near(0x44f048) ? 1 : 0;
  else
    for (int i = 0; i < std::max(s8m(0x403a08), 0); ++i)
      if (near(0x4039c8 + 12 * Raw(i))) {
        now = 1;
        break;
      }
  if (now == was)
    return now;
  if (auto node = u(obj + 4))
    remove(node);
  if (now)
    handle_link(0x5db3a8, obj + 4, obj);
  else {
    auto cursor = s32m(0x4039fc) + 1;
    w(0x4039fc, Raw(cursor));
    if (cursor >= 60)
      w(0x4039fc, 0);
    handle_link(Raw(0x5db3f0 + 4 * Raw(s32m(0x4039fc))), obj + 4, obj);
  }
  return now;
}
Raw World::State::move_and_collide(Address obj, const Vec3 &vel) {
  Vec3 step{mul(vel[0], F_DT), mul(vel[1], F_DT), mul(vel[2], F_DT)};
  Vec3 pos;
  for (unsigned i = 0; i < 3; ++i)
    pos[i] = add(u(obj + 16 + 4 * i), step[i]);
  for (unsigned i = 0; i < 3; ++i)
    w(obj + 16 + 4 * i, pos[i]);
  grid(obj);
  Raw result = 0;
  if (!services.height_below)
    throw Unsupported("height service missing");
  auto height = services.height_below;
  auto hgt = height(vec(obj + 16), u(obj + 0x1c));
  if (lt(u(obj + 0x18), hgt)) {
    result = 1;
    Raw k = F_TENTH;
    Vec3 cand;
    while (true) {
      Vec3 back{mul(step[0], k), mul(step[1], k), mul(step[2], k)};
      cand = {sub(u(obj + 16), back[0]), sub(u(obj + 20), back[1]),
              sub(u(obj + 24), back[2])};
      auto h2 = height(cand, u(obj + 0x1c));
      if (le(h2, cand[2]))
        break;
      k = add(k, F_TENTH);
      if (!lt(k, F_TWO))
        break;
    }
    wvec(obj + 16, cand);
    if (s8m(obj + 0x50) == 0) {
      if (!services.water_test)
        throw Unsupported("water query service missing");
      if (services.water_test(vec(obj + 16), F_TWELVE))
        throw Unsupported("a projectile entering water");
    }
  }
  auto r = u(obj + 0x1c), ht = u(obj + 0x20);
  auto p = vec(obj + 16);
  auto owner = u(obj + 0x4c) ? u(u(obj + 0x4c) + 12) : 0;
  auto found = box({sub(p[0], r), sub(p[1], r), sub(p[2], ht)},
                   {add(p[0], r), add(p[1], r), add(p[2], ht)}, 0x100);
  for (auto o : found) {
    auto flags = u(o + 12);
    if ((flags & 0x201) || (flags & 0x20000000) || o == obj || o == owner)
      continue;
    auto oz = u(o + 0x18), oh = u(o + 0x20), mz = u(obj + 0x18),
         mh = u(obj + 0x20);
    if (lt(add(oz, oh), sub(mz, mh)))
      continue;
    if (lt(add(mz, mh), oz))
      continue;
    auto dx = sub(u(obj + 16), u(o + 16)), dy = sub(u(obj + 20), u(o + 20));
    auto d2 = add(mul(dx, dx), mul(dy, dy));
    auto rr = add(u(obj + 0x1c), u(o + 0x1c));
    if (!lt(d2, mul(rr, rr)))
      continue;
    result |= 2;
    vcall(o, 6, {iv(obj)});
    vcall(obj, 6, {iv(o)});
  }
  return result;
}
void World::State::projectile_ctor(Address obj, const Vec3 &pos,
                                   Address owner) {
  construct(obj, pos,
            kind_hint == Kind::Thrown ? Kind::Thrown : Kind::Missile);
  for (auto off : PROJECTILE_HEADS)
    head(obj + off);
  if (kind_hint == Kind::Thrown)
    head(obj + 0x1c8);
  auto flags = u(obj + 12) | 0x4081;
  clear(obj + 0x4c);
  w(obj + 0x1c, F_TWELVE);
  w(obj + 0x48, VT_PROJECTILE);
  w(obj + 12, flags);
  w(obj + 0x50, 1, 0);
  if (owner)
    handle_link(obj + 0x4c, owner, owner);
}
void World::State::missile_ctor(Address obj, const Missile &m, Address owner) {
  auto kind = s32(m.kind);
  projectile_ctor(obj, m.position, owner);
  w(obj + 0x48, VT_MISSILE);
  for (unsigned k = 0; k < 5; ++k)
    clear(obj + 0x5c + 0x18 * k);
  auto L = obj + 0x174;
  clear(obj + 0x124);
  clear(obj + 0x128);
  w(L + 0x9, 1, 0);
  wvec(obj + 0x1a4, m.position);
  w(L + 0x18, 2, Raw(-0x20));
  w(L + 0x10, 2, Raw(-0x20));
  w(L + 0x24, 0x43280000);
  w(L + 0x28, 0x38149b92);
  w(L + 0x1c, 0x43280000);
  w(L + 0x20, 0x38149b92);
  w(L + 0x2c, 0);
  w(L + 0xa, 2, 0xff);
  w(L + 0xc, 2, 0x9b);
  w(L + 0xe, 2, 0);
  w(L + 0x12, 2, 0xff);
  w(L + 0x14, 2, 0x9b);
  w(L + 0x16, 2, 0);
  w(L + 0x8, 1, 0);
  note("presentation:light_link");
  w(obj + 0x28, 1, 0x48);
  w(obj + 0xfe, 1, 0);
  w(obj + 0xfc, 2, 0);
  w(obj + 0xfa, 2, 0);
  w(obj + 0xd0, Raw(kind));
  if (Raw(kind - 5) < 2) {
    w(obj + 0x180, 2, 0x9b);
    w(obj + 0x182, 2, 0xff);
    w(obj + 0x186, 2, 0);
    w(obj + 0x17e, 2, 0);
    w(obj + 0x188, 2, 0x9b);
    w(obj + 0x18a, 2, 0xff);
  }
  kind = s32m(obj + 0xd0);
  if (kind == 1 || kind == 2 || kind == 5 || kind == 6) {
    static const std::map<int, Address> casts = {
        {1, 0x532498}, {2, 0x532480}, {5, 0x5324b0}, {6, 0x5324c8}};
    resource_text(cstr(S_FX_LUMP), cstr(casts.at(kind)));
    note("presentation:sound_playOnObject");
    kind = s32m(obj + 0xd0);
  }
  if (kind == 0 || kind == 3)
    note("presentation:light_unlink");
  bool elemental = false;
  auto item = m.item;
  if (item && u(item)) {
    if (s8m(u(item) + 0x1e) == 6) {
      w(obj + 0x1c, 0x41c00000);
      w(obj + 0x20, 0x42c80000);
    }
    for (int wanted : {0x18, 0x16, 0x15, 0x17, 0x26}) {
      std::int32_t value = 0;
      for (unsigned k = 0; k < 4; ++k)
        if (s16m(item + 0x24 + 8 * k) == wanted) {
          value = s16m(item + 0x26 + 8 * k);
          break;
        }
      if (value) {
        w(obj + 0x170, 0);
        elemental = true;
        break;
      }
    }
  }
  if (!elemental && u(obj + 0xd0) == 0 && !(u(G_LEVEL_FLAGS) & 0x100)) {
    note("presentation:trail_create");
    w(obj + 0x170, 0);
    note("presentation:trail_addPoint");
  }
  w(obj + 0xd4, m.minimum);
  w(obj + 0xd8, m.maximum);
  w(obj + 0xdc, m.rating);
  w(obj + 0xe0, m.item_flags);
  w(obj + 0xe4, m.attack);
  w(obj + 0xe8, m.damage_flags);
  wvec(obj + 0x1b0, m.velocity);
  w(obj + 0x38, 2, m.facing);
  auto flags = u(obj + 12) | 0x4001;
  w(obj + 0x1c0, m.models[0]);
  w(obj + 0x1c4, m.models[1]);
  w(obj + 0x12c, item);
  w(obj + 12, flags);
  w(obj + 0xf4, 0);
  w(obj + 0xcc, 1, 0);
  w(obj + 0xf8, 1, 1);
  w(obj + 0xec, u(FRAME) + 0x12c);
  schedule(obj, 1);
  w(obj + 0xcd, 1, 1);
  if (m.launch)
    note("presentation:sound_playSample");
  w(obj + 0xf0, 0);
  w(obj + 0x1bc, m.impact);
}
void World::State::thrown_ctor(Address obj, const Thrown &t, Address owner) {
  auto kind = byte(t.kind);
  Missile m;
  m.position = t.position;
  m.velocity = t.velocity;
  m.facing = t.facing;
  m.minimum = t.minimum;
  m.maximum = t.maximum;
  m.attack = t.attack;
  m.damage_flags = 4;
  m.item = t.item;
  m.kind = 0;
  m.models = t.models;
  missile_ctor(obj, m, owner);
  clear(obj + 0x1c8);
  w(obj + 0x1cc, 2, 0x7d0);
  w(obj + 0x48, VT_THROWN);
  w(obj + 0x1ce, 1, Raw(kind));
  if (kind == 0)
    w(obj + 0x1cc, 2, Raw(-0x7d0));
  if (owner)
    handle_link(obj + 0x1c8, owner, owner);
}
void World::State::missile_run(Address self) {
  if (u(self + 0x128)) {
    auto active = obj_update_active(self, Raw(s8m(self + 0xcd)), 0x1e0);
    w(self + 0xcd, 1, active);
    auto host = u(u(self + 0x128) + 12);
    if ((u(host + 12) & 0x20) && (q(host + 0xa8) & (UINT64_C(1) << 41))) {
      erase_later(self);
      return;
    }
    if (model_of(host))
      throw Unsupported("model_getSamplePoint (a stuck arrow follows a model)");
    grid(self);
    return;
  }
  if (s8m(self + 0xcc)) {
    vcall(self, 1, {iv(3)});
    return;
  }
  auto kind = s32m(self + 0xd0);
  if (kind == 1 || kind == 2 || kind == 5 || kind == 6) {
    if (!round3)
      throw Unsupported("MissileWeapon_run of kind " + std::to_string(kind) +
                        " (impact effects)");
    missile_arrow_run(self, kind);
    return;
  }
  if (kind != 0 && kind != 3)
    throw Unsupported("MissileWeapon_run of kind " + std::to_string(kind) +
                      " (impact effects)");
  auto spin = h(self + 0xfa);
  if (s16(spin))
    w(self + 0xfc, 2, h(self + 0xfc) + spin);
  auto r = move_and_collide(self, vec(self + 0x1b0));
  auto res = r & ~2u;
  if (s8m(self + 0xcc))
    res |= 2;
  if (res) {
    w(self + 0xec, 0);
    if (res == 2) {
      auto cc = s8m(self + 0xcc);
      if (cc == 2)
        note("presentation:fx_spawn", fx_args((u(G_LEVEL_FLAGS) & 0x100) ? 0x11 : 2, vec(self + 0x10)));
      else if (cc == 3)
        w(self + 0xec, u(FRAME));
      else
        w(self + 0xec, u(FRAME) + 1);
    } else if (res == 1) {
      const Raw flags = u(G_LEVEL_FLAGS);
      note("presentation:fx_spawn",
           fx_args((flags & 0x40000) ? 8 : (flags & 0x100) ? 0x11 : 2, vec(self + 0x10)));
      resource_text(cstr(S_FX_LUMP), cstr(S_STONE_HIT));
      note("presentation:sound_playOnObject");
      if (u(self + 0x1bc))
        note("presentation:sound_playSample");
    }
  }
  kind = s32m(self + 0xd0);
  bool puff = false;
  if (kind == 0 && (u(G_LEVEL_FLAGS) & 0x100)) {
    if (own(self) % 3 == 0) {
      auto spot = vec(self + 0x10);
      const Raw d = own(self);
      spot[2] = add(spot[2], mul(mul(from_int(s32(d)), F_INV65536), F_EIGHTEEN));
      note("presentation:fx_spawn", fx_args(0x11, spot));
      puff = true;
    }
  }
  if (!puff && u(self + 0x170))
    note("presentation:trail_addPoint");
  if (!(s32m(FRAME) < s32m(self + 0xec)))
    vcall(self, 1, {iv(3)});
}
void World::State::thrown_run(Address self) {
  auto kind = b(self + 0x1ce);
  if (kind == 1 || kind == 2 || kind == 3) {
    if (!round4)
      throw Unsupported("playerProjectile_run of a thrown potion");
    potion_run(self);
    return;
  }
  auto spin = s16m(self + 0x1cc);
  if (kind == 0) {
    if (spin < 0x1388)
      w(self + 0x1cc, 2, Raw(spin + 0xc8));
  } else if (((kind - 4) & 0xff) < 3)
    w(self + 0x1cc, 2, Raw(spin + 0xbb8));
  else if (!(spin < -0x1387))
    w(self + 0x1cc, 2, Raw(spin - 0xc8));
  auto name = cstr(S_STONE_HIT_THROWN);
  w(self + 0x1b8, sub(u(self + 0x1b8), F_GRAVITY));
  auto r = move_and_collide(self, vec(self + 0x1b0));
  auto res = r & ~2u;
  if (s8m(self + 0xcc))
    res |= 2;
  if (res) {
    w(self + 0xec, 0);
    auto k = s8m(self + 0x1ce);
    bool fx = k == 0 || k == 4 || k == 5 || k == 6;
    if (res == 2) {
      if (!s8m(self + 0xcc)) {
        if (fx)
          note("presentation:fx_spawn", fx_args(2, vec(self + 0x10)));
        resource_text(cstr(S_FX_LUMP_THROWN), name);
        note("presentation:sound_playOnObject");
      }
    } else if (res == 1) {
      if (!services.height_below)
        throw Unsupported("height service missing");
      auto spot = vec(self + 0x10);
      spot[2] = add(services.height_below(vec(self + 16), u(self + 0x1c)), F_FIVE);
      if (fx)
        note("presentation:fx_spawn", fx_args(2, spot));
      resource_text(cstr(S_FX_LUMP_THROWN), name);
      note("presentation:sound_playOnObject");
    }
  }
  if (s32m(FRAME) < s32m(self + 0xec))
    return;
  vcall(self, 1, {iv(3)});
}
void World::State::missile_on_touch(Address self, Address other) {
  if (s8m(self + 0xcc))
    return;
  auto owner = u(self + 0x4c) ? u(u(self + 0x4c) + 12) : 0;
  if (other == 0)
    return;
  if (!(u(other + 12) & 0x20)) {
    if (same_type(type_of(other), static_type("AutoContainer_typeinfo")))
      vcall(other, 2, {iv(0), iv(0), iv(0)});
    else if (same_type(type_of(other), static_type("Container_typeinfo")))
      vcall(other, 2, {iv(0), iv(0), iv(0)});
    w(self + 0xcc, 1, 2);
    return;
  }
  if (owner && (u(owner + 12) & 0x20))
    if (!number(vcall(owner, 20, {iv(other)})))
      return;
  for (auto node = u(self + 0x124); node; node = u(node))
    if (u(node + 12) == other)
      return;
  bool pierce = false, unblockable = false;
  auto kind = s32m(self + 0xd0);
  if (owner && (u(owner + 12) & 0x40) && (kind == 0 || kind == 3)) {
    auto rank = skill_rank(u(owner + 0x3c8), 0x5d);
    if (rank)
      throw Unsupported("Penetrating Arrows statistics at a nonzero rank");
    Raw uf = 0, pf = 0;
    if (le(mul(from_int(draw()), F_INV65536), uf))
      unblockable = true;
    if (le(mul(from_int(draw()), F_INV65536), pf))
      pierce = true;
    else
      clear(self + 0x124);
  }
  auto oflags = u(other + 12);
  if ((oflags & 0x20) && owner && (u(owner + 12) & 0x40)) {
    if ((oflags & 0x40) && u(G_PVP_A) == 0)
      return;
    if ((oflags & 0x1000000) && !(oflags & 0x40))
      return;
    if (!pierce && u(self + 0xd0) == 0) {
      handle_link(self + 0x128, other, other);
      auto index = number(vcall(self, 17, {iv(other)}));
      w(self + 0xf4, index);
      if (index == 0)
        clear(self + 0x128);
      else if (same_type(type_of(other), static_type("Skeleton_typeinfo")))
        clear(self + 0x128);
      else if (model_of(other))
        throw Unsupported("model_getSamplePoint (an arrow sticks to a model)");
      w(self + 12, u(self + 12) | 1);
    }
    kind = s32m(self + 0xd0);
    if (kind == 1 || kind == 5 || kind == 6) {
      w(self + 0xe8, kind == 1 ? 0x20 : kind == 5 ? 0x400 : 0x80);
      w(self + 0xe4, 0x2710);
    }
  }
  std::int32_t chance = 0;
  oflags = u(other + 12);
  if (oflags & 0x40) {
    auto slot = u(other + 0x3c8);
    if (skill_rank(slot, 2) || skill_rank(u(other + 0x3c8), 0x53))
      throw Unsupported("Dodge statistics at a nonzero rank");
  } else if (oflags & 0x20) {
    if (number(vcall(other, 36, {})))
      throw Unsupported("Dodge statistic of a boss creature (Skill.v11)");
  }
  auto roll = std::int32_t(draw() % 100 + 1);
  oflags = u(other + 12);
  if (roll < chance)
    throw Unsupported("a deflected missile");
  if (oflags & 0x20) {
    auto flags = u(self + 0xe8) | (unblockable ? 0x40u : 0u);
    auto attacker = owner ? owner : self;
    auto result = damage_full(other, attacker, u(self + 0xe4), u(self + 0xdc),
                              u(self + 0xe0), flags, u(self + 0x12c),
                              from_int(s32m(self + 0xd4)),
                              from_int(s32m(self + 0xd8)));
    if (result == 1)
      w(self + 0xcc, 1, (u(other + 12) & 0x40) ? 3 : 1);
    else
      w(self + 0xcc, 1, 2);
  } else if (same_type(type_of(other), static_type("PushPhysicsProp_typeinfo")))
    w(self + 0xcc, 1, 2);
  if (pierce && s8m(self + 0xcc)) {
    handle_link(self + 0x124, other, other);
    w(self + 0xcc, 1, 0);
  }
}
Raw World::State::missile_nearest_sample(Address, Address other) {
  if (other == 0)
    return 0;
  auto model = model_of(other);
  Raw count = 0;
  if (model) {
    if (!services.sample_point_count)
      throw Unsupported("model sample point service missing");
    count = services.sample_point_count(model);
  }
  if (s32(count) == 0)
    return 0;
  throw Unsupported("model_getSamplePoint (nearest sample point)");
}
void World::State::projectile_destroy(Address a, Raw flags) {
  auto fn = u(u(a + 0x48) + 12);
  note("destroy", {a});
  if (fn == 0x3c6468) { // playerProjectile_dtor
    w(a + 0x48, VT_THROWN);
    clear(a + 0x1c8);
  }
  // MissileWeapon_dtor 0x151454 (this, 0 or flags).
  w(a + 0x48, VT_MISSILE);
  if (u(a + 0x170))
    note("presentation:trail_release");
  note("presentation:light_dtor");
  clear(a + 0x128);
  clear(a + 0x124);
  for (unsigned off : {0xbc, 0xa4, 0x8c, 0x74, 0x5c})
    clear(a + off);
  w(a + 0x48, VT_PROJECTILE);
  clear(a + 0x4c);
  gameobject_dtor_call(a, flags);
}
} // namespace champions::simulation::rta::combat_runtime
