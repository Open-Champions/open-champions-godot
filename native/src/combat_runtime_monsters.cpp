#include "combat_runtime_internal.hpp"
#include <cstdlib>
// Round-2 creature helpers, death and loot (research
// ref_combat_monsters.MonsterCombatModel, executed by difftest_combat_monsters).
// None of the hit, sight and cooldown helpers draws the shared seed.
namespace champions::simulation::rta::combat_runtime {
int damage_with_descriptor(World::State &, Address, Address, Raw, Raw,
                           const combat::Descriptor &, Raw, Raw);
namespace {
constexpr Raw F_60 = 0x42700000, F_RAY = 0x3f8ccccc, F_96 = 0x42c00000,
              F_QUARTER = 0x3e800000, F_HALF = 0x3f000000, F1 = 0x3f800000,
              F1_5 = 0x3fc00000, F2 = 0x40000000, F3 = 0x40400000,
              F4 = 0x40800000, F5 = 0x40a00000, F6 = 0x40c00000,
              F7 = 0x40e00000, F7_5 = 0x40f00000, F8 = 0x41000000,
              F8_5 = 0x41080000, F15 = 0x41700000, F20 = 0x41a00000;
constexpr Address S_NOLOOT = 0x52ffd8, S_GOBLIN_DEAD = 0x546998,
                  S_GOBLIN_NOLOOT = 0x5469a0, S_GOBLIN_ARROWS = 0x5469a8;
int seen(Raw t) { return lt(t, 0) || lt(ONE, t) ? 1 : 0; }
struct Extra {
  Raw gs, gc, q, ic;
};
struct LootClass {
  const char *name;
  int condition; // 0 none, 1 MiniBoss bit, 2 word +0x3d8
  bool ordinary;
  std::vector<Extra> extras;
  Raw coin; // 0 = always
  Raw q, ic;
};
const std::vector<Extra> STD = {{F4, F1, F6, F2}, {F5, F1, F7, F2}};
// Creature_dropLoot's class tests, in order (research LOOT_CLASSES).
const LootClass LOOT_CLASSES[] = {
    {"SpiderQueen_typeinfo", 0, true, STD, F_HALF, F8, F2},
    {"AntQueen_typeinfo", 0, true, STD, 0, F8, F2},
    {"UndeadKnight_typeinfo", 0, true, STD, F_HALF, F8, F2},
    {"VampireLord_typeinfo", 0, true, STD, F_HALF, F8, F2},
    {"Arenabeast_typeinfo", 0, true, STD, F_HALF, F8, F2},
    {"SeaMonster_typeinfo", 1, false, {{F4, F1, F5, F2}, {F5, F1, F6, F2}},
     F_HALF, F7_5, F2},
    {"CloudGiant_typeinfo", 2, true, STD, 0, F8, F2},
    {"Minion_typeinfo", 0, true, STD, 0, F8, F2},
    {"MummyKing_typeinfo", 0, true, STD, F_HALF, F8, F2},
    {"Innoruuk_typeinfo", 0, true, {{F15, F1, F6, F2}, {F20, F1, F7, F2}},
     F_HALF, F8, F2},
    {"Zek_typeinfo", 0, false,
     {{F4, F1, F4, F2}, {F4, F1, F5, F2}, {F6, F1, F7, F2}}, F_HALF, F8_5, F2}};
} // namespace
int World::State::can_see_point(Address c, const Vec3 &pos, Raw facing, Raw arc,
                                Raw near, Raw far) {
  auto f = s16(facing), a = s16(arc);
  auto cp = vec(c + 16);
  Vec3 v{sub(pos[0], cp[0]), sub(pos[1], cp[1]), sub(pos[2], cp[2])};
  auto delta = s16(Raw(s16(Raw(0x4000 - atan2_16(v[0], v[1]))) - f));
  if (!(std::abs(delta) < a))
    return 0;
  auto ex = sub(cp[0], pos[0]), ey = sub(cp[1], pos[1]);
  auto d = sqrt(add(mul(ex, ex), mul(ey, ey)));
  if (!lt(d, far) || !le(near, d))
    return 0;
  return seen(ray({cp[0], cp[1], add(cp[2], u(c + 0x20))}, v, 3, F_RAY));
}
int World::State::can_see_edge(Address c, Address o, Raw facing, Raw arc,
                               Raw mode, Raw near, Raw far, Raw lead) {
  if (!o)
    return 0;
  auto f = s16(facing), a = s16(arc);
  auto p = vec(o + 16);
  if (!eq(lead, 0) && (u(o + 12) & 0x20)) {
    p[0] = add(p[0], mul(u(o + 0x308), lead));
    p[1] = add(p[1], mul(u(o + 0x30c), lead));
  }
  auto cp = vec(c + 16);
  Vec3 v{sub(p[0], cp[0]), sub(p[1], cp[1]), sub(p[2], cp[2])};
  if (!(std::abs(s16(Raw(atan2_16(v[1], v[0]) - f))) < a))
    return 0;
  auto ex = sub(cp[0], p[0]), ey = sub(cp[1], p[1]);
  auto d = sqrt(add(mul(ex, ex), mul(ey, ey)));
  auto orad = u(o + 0x1c);
  if (!lt(d, add(far, orad)) || !le(sub(near, orad), d))
    return 0;
  auto t = ray({cp[0], cp[1], add(cp[2], mul(u(c + 0x20), HALF))}, v,
               mode ? 0 : 3, F_RAY);
  return seen(t);
}
Address World::State::find_player_in_cone(Address c, Raw facing, Raw arc,
                                          Raw near, Raw far) {
  if (s16m(c + 0x312))
    return cone(c, facing, arc, near, far);
  auto f = s16(facing), a = s16(arc);
  auto best = far;
  Address found = 0;
  for (int i = 0; i < std::max(s32m(NUM_PLAYERS), 0); ++i) {
    auto o = u(Raw(PLAYERS + Raw(i) * RECORD_SIZE + 0x43d8));
    if (!o || (u(o + 12) & 8))
      continue;
    auto cp = vec(c + 16), op = vec(o + 16);
    auto ctop = add(cp[2], mul(u(c + 0x20), HALF));
    Vec3 v{sub(op[0], cp[0]), sub(op[1], cp[1]),
           sub(add(op[2], mul(u(o + 0x20), HALF)), ctop)};
    auto delta = s16(Raw(s16(Raw(0x4000 - atan2_16(v[0], v[1]))) - f));
    if (!(std::abs(delta) < a))
      continue;
    auto ex = sub(cp[0], op[0]), ey = sub(cp[1], op[1]);
    auto d = sqrt(add(mul(ex, ex), mul(ey, ey)));
    if (!lt(d, best))
      continue;
    auto orad = u(o + 0x1c);
    if (!lt(d, add(far, orad)) || !le(sub(near, orad), d))
      continue;
    if (seen(ray({cp[0], cp[1], add(cp[2], u(c + 0x20))}, v, 3, F_RAY))) {
      found = o;
      best = d;
    }
  }
  return found;
}
int World::State::hit_target_at_reach(Address c, Raw lo, Raw hi, Raw attack,
                                      Raw flags, Raw, Raw reach) {
  auto node = u(c + TARGET);
  if (!node)
    return 0;
  auto victim = u(node + 12);
  auto f = Raw(s16m(c + 0x38));
  auto px = add(u(c + 16), mul(cos16(f), reach));
  auto py = add(u(c + 20), mul(sin16(f), reach));
  auto pz = u(c + 24);
  auto t = u(u(c + TARGET) + 12);
  auto dx = sub(u(t + 16), px), dy = sub(u(t + 20), py), dz = sub(u(t + 24), pz);
  auto d = sqrt(add(add(mul(dx, dx), mul(dy, dy)), mul(dz, dz)));
  if (!lt(d, F_60) || (u(t + 12) & 8))
    return 0;
  if (hit_descriptor)
    return damage_with_descriptor(*this, victim, c, attack, flags, *hit_descriptor,
                                  from_int(s32(lo)), from_int(s32(hi)));
  return damage_full(victim, c, attack, 0, 0, flags, 0, from_int(s32(lo)),
                     from_int(s32(hi)));
}
int World::State::hit_target_in_arc_edge(Address c, Raw facing, Raw arc,
                                         Raw lo, Raw hi, Raw attack, Raw flags,
                                         Raw, Raw near, Raw far) {
  auto node = u(c + TARGET);
  if (!node)
    return 0;
  if (!can_see_edge(c, u(node + 12), Raw(s16(facing)), Raw(s16(arc)), 0, near,
                    far, 0))
    return 0;
  return damage_full(u(u(c + TARGET) + 12), c, attack, 0, 0, flags, 0,
                     from_int(s32(lo)), from_int(s32(hi)));
}
int World::State::hit_target_or_player(Address c, Raw facing, Raw arc, Raw lo,
                                       Raw hi, Raw attack, Raw flags, Raw,
                                       Raw near, Raw far) {
  auto f = Raw(s16(facing)), a = Raw(s16(arc));
  Address victim = 0;
  auto node = u(c + TARGET);
  if (node && can_see_point(c, vec(u(node + 12) + 16), f, a, near, far))
    victim = u(u(c + TARGET) + 12);
  else
    victim = find_player_in_cone(c, f, a, near, far);
  if (!victim)
    return 0;
  if (hit_descriptor)
    return damage_with_descriptor(*this, victim, c, attack, flags, *hit_descriptor,
                                  from_int(s32(lo)), from_int(s32(hi)));
  return damage_full(victim, c, attack, 0, 0, flags, 0, from_int(s32(lo)),
                     from_int(s32(hi)));
}
void World::State::knockback_target(Address c, Raw speed) {
  auto node = u(c + TARGET);
  if (!node)
    return;
  auto t = u(node + 12);
  w(t + 0x2e8, speed);
  auto a = atan2_16(sub(u(t + 20), u(c + 20)), sub(u(t + 16), u(c + 16)));
  w(t + 0x2e0, 2, Raw(a));
}
void World::State::set_cooldown(Address c, Raw frames) {
  if (!u(c + TARGET))
    return;
  if (s16m(c + 0x232) != 0x14)
    vcall(c, 27, {iv(0x10)});
  w(c + 0x318, 2, frames);
}
void World::State::set_cooldown_on(Address c, Raw frames, Address obj) {
  if (q(c + 0xa8) & 1) // creature_bossRank of a creature without a type name
    return;
  if (!obj)
    return;
  clear(c + TARGET);
  handle_link(c + TARGET, obj, obj);
  if (s16m(c + 0x232) != 0x14)
    vcall(c, 27, {iv(0x10)});
  w(c + 0x318, 2, frames);
}
void World::State::notify_objects_near(Raw a, const Vec3 &pos, Raw b,
                                       Raw radius) {
  auto found =
      box({sub(pos[0], radius), sub(pos[1], radius), sub(pos[2], radius)},
          {add(pos[0], radius), add(pos[1], radius), add(pos[2], radius)}, 0x80);
  for (auto o : found)
    vcall(o, 12, {iv(a), pos, iv(b), iv(radius)});
}
void World::State::dead_update(Address c, Raw events, Raw hold, Raw size,
                               const Vec3 &offset) {
  actor(c);
  auto cf = q(c + 0xa8);
  if (cf & 0x1000000) {
    w(c + 12, u(c + 12) | 0x4000);
    if (!(h(c + 0x11a) & 0x80)) {
      if (!hold)
        w(c + 0x2f4, 0);
      return;
    }
    if (hold)
      return;
    auto sink = add(u(c + 0x2f4), 0x3eb33333);
    w(c + 0x2f4, sink);
    w(c + 0x3c, sub(u(c + 24), sink));
    if (lt(u(c + 0x20), u(c + 0x2f4))) {
      w(c + 12, u(c + 12) | 0x4002);
      erase_later(c);
    }
    return;
  }
  if ((events & 0x80) && !(u(c + 12) & 2) && lt(0, size) && !hold) {
    constexpr std::pair<std::uint64_t, Raw> kinds[] = {
        {0x1000, 6}, {0x2000, 3}, {0x4000, 0xa}, {0x40000, 0}};
    for (auto &k : kinds)
      if (cf & k.first) {
        if (!full())
          throw Unsupported("corpse blood decal");
        auto p = vec(c + 16);
        Vec3 at{add(p[0], offset[0]), add(p[1], offset[1]),
                add(p[2], offset[2])};
        auto angle = Raw(s16(own(c)));
        auto extra = own(c) % 10;
        note("decal_add", {at[0], at[1], at[2], angle, k.second, 1,
                           add(size, from_int(extra))});
        break;
      }
  }
  if ((q(c + 0x118) & 0x8000ff) == 0x800001 && hold)
    erase_later(c);
}
// Creature_dropLoot 0x10f2c8. Every drop is a loot_generateDrops call-out;
// the own draws are one per player slot in a coin loop. No shared draw.
void World::State::drop_loot(Address c, const Vec3 &pos, Raw angle, Raw filter,
                             Raw z, Raw thr, Raw gs, Raw gc, Raw qrow,
                             Raw ichance) {
  if (q(c + 0xa8) & UINT64_C(0x100000000))
    return;
  if (number(service("obj_getPropInt", {iv(c), iv(S_NOLOOT), iv(0)})) &&
      !u(G_PVP_B))
    return;
  if (!services.loot_generate || !services.is_class)
    throw Unsupported("loot generation and type identity services missing");
  auto gen = [&](Raw filt, Raw force, Raw g_s, Raw g_c, Raw q_r, Raw i_c) {
    LootDrop d{pos, angle, filt, force, z, thr, g_s, g_c, q_r, i_c};
    note("loot_generateDrops", {pos[0], pos[1], pos[2], angle, filt, force, z,
                                 thr, g_s, g_c, q_r, i_c});
    auto fn = services.loot_generate;
    fn(d);
  };
  auto per_player = [&](Raw coin, Raw q_e, Raw i_e) {
    for (int slot = 0; slot < std::max(s32m(NUM_PLAYERS), 0); ++slot) {
      Raw force = 1;
      if (coin)
        force = lt(mul(from_int(s32(own(c))), 0x37800000), coin) ? 1 : 0;
      gen(0, force, 0, 0, q_e, i_e);
    }
  };
  for (auto &k : LOOT_CLASSES) {
    auto test = services.is_class;
    if (!test(c, k.name))
      continue;
    if (k.condition == 1 && !(q(c + 0xa8) & 1))
      continue;
    if (k.condition == 2 && !u(c + 0x3d8))
      continue;
    if (k.ordinary)
      gen(filter, 0, gs, gc, qrow, ichance);
    for (auto &e : k.extras)
      gen(0, 0, e.gs, e.gc, e.q, e.ic);
    per_player(k.coin, k.q, k.ic);
    return;
  }
  auto rank = s32(Raw(q(c + 0xa8) & 1));
  if (rank == 2) {
    gen(filter, 0, gs, gc, qrow, ichance);
    for (auto &e : STD)
      gen(0, 0, e.gs, e.gc, e.q, e.ic);
    per_player(F_HALF, F8_5, F2);
  } else if (s32(Raw(q(c + 0xa8) & 1)) == 1) {
    for (auto &e : {Extra{F4, F1, F3, F1_5}, Extra{F5, F1, F5, F1_5}})
      gen(0, 0, e.gs, e.gc, e.q, e.ic);
    per_player(F_QUARTER, F7, F1_5);
  }
  if (s8m(c + 0xa0)) {
    gs = mul(gs, F3);
    qrow = mul(qrow, F4);
  }
  if (!(u(c + 0xa8) & 1))
    gen(filter, 0, gs, gc, qrow, ichance);
}
// Goblin_setState 0x26ccfc, the death branch (state 0x14) only.
void World::State::goblin_set_state(Address c, Raw requested) {
  actor(c);
  auto filtered = model.filter_state(c, requested);
  if (filtered.refused)
    return;
  auto state = filtered.state;
  w(c + 0xa8, 8, q(c + 0xa8) & ~UINT64_C(0x200));
  if (state < 0x18 && state != 0x14)
    throw Unsupported("Goblin_setState state outside the death branch");
  if (state == 0x14) {
    auto prop = [&](Address key) {
      return number(service("obj_getPropInt", {iv(c), iv(key), iv(0)}));
    };
    if (!prop(S_GOBLIN_DEAD)) {
      auto p = vec(c + 16);
      auto found = box({sub(p[0], F_96), sub(p[1], F_96), sub(p[2], F_96)},
                       {add(p[0], F_96), add(p[1], F_96), add(p[2], F_96)},
                       0x80);
      unsigned dead = 0;
      for (auto o : found)
        if ((u(o + 12) & 0x68) == 0x28)
          ++dead;
      if (dead >= 3)
        w(c + 0x488, 1);
    }
    if (!(u(c + 12) & 2)) {
      w(c + 12, u(c + 12) & ~1u);
      if (u(c + 0x50))
        throw Unsupported("a goblin weapon holder (Holder.v11)");
    }
    if (prop(S_GOBLIN_DEAD))
      w(c + 12, u(c + 12) | 1);
    start_anim(c, u(u(c + 0x24) + 0x44), 6, 0x3e999999);
    if (!(q(c + 0xa8) & UINT64_C(0x100000000))) {
      drop_loot(c, vec(c + 16), Raw(s16m(c + 0x38)), 0, 0x42100000, F2,
                0x3fe66666, 0x3f199999, F2, F_QUARTER);
      if (s32m(c + 0x3d4) == 4 && own(c) < 0x1770)
        if (!prop(S_GOBLIN_NOLOOT)) {
          if (!services.spawn_named_item)
            throw Unsupported("named item service missing");
          auto fn = services.spawn_named_item;
          auto at = vec(c + 16);
          note("loot_spawnNamedItem", {at[0], at[1], at[2], S_GOBLIN_ARROWS});
          fn(at, S_GOBLIN_ARROWS);
        }
    }
    vcall(c, 28, {iv(0)});
  }
  w(c + 0x232, 2, Raw(state));
}
} // namespace champions::simulation::rta::combat_runtime
