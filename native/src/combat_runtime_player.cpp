#include "combat_runtime_internal.hpp"
namespace champions::simulation::rta::combat_runtime {
namespace {
std::string str(const World::State &s, Address a) {
  std::string out;
  for (unsigned k = 0; k < 32 && s.b(a + k); ++k)
    out += char(s.b(a + k));
  return out;
}
} // namespace
void World::State::player_take_damage(Address p, Address a, Raw flags,
                                      Raw amount) {
  auto r = record(p);
  if (byte(b(0x41b51d)) || s16(h(p + 0x4b4)) || u(p + 0x540) ||
      str(*this, 0x41b721) == "actintro")
    return;
  if (!services.jumpgate_travel_state)
    throw Unsupported("jumpgate state service missing");
  if (services.jumpgate_travel_state())
    return;
  if (u(0x41b4fc) || s16(h(0x4f694a)))
    return;
  auto slot = u(p + 0x3c8), flash = 0x41b930 + 4 * slot;
  if (b(0x41b69d))
    note("presentation:gfx_setColorOffset");
  if (byte(b(r + 0x4c27)) > 0) {
    w(flash, u(flash) | 1);
    return;
  }
  auto st = u(p + 0x58c);
  if (a && (st == 8 || st == 11) && !(flags & 0x40))
    throw Unsupported("blocking player");
  if (target(p + 0x364))
    throw Unsupported("Holy Shield/spells");
  w(p + 0xc8, add(u(p + 0xc8), amount));
  auto df = u(p + 0x2dc) | flags;
  w(p + 0x2dc, df);
  if (a) {
    actor(a);
    if (!(df & 0x800000))
      w(p + 0x2e0, 2,
        0x4000 - Raw(atan2_16(sub(u(p + 16), u(a + 16)),
                              sub(u(p + 20), u(a + 20)))));
    clear(p + LAST_ATTACKER);
    auto src = a;
    if (u(a + 12) & 0x80) {
      if (auto owner = target(a + 0x4c))
        src = owner;
    } else if (flags & 0x10000000) {
      if (auto source = target(p + 0x360))
        src = source;
    }
    if (src)
      link(p + LAST_ATTACKER, src, src);
  }
  if (lt(0, u(p + 0xc8)))
    w(flash, u(flash) | 1);
}
void World::State::state3(Address p) {
  w(p + 0x58c, 3);
  w(p + 0x448, u(FRAME));
  w(0x402797, 1, 1);
  note("presentation:player_voice", {u(p + 0x3c8), 12});
}
void World::State::reaction(Address p) {
  auto n = byte(b(p + 0x118));
  if (n >= 5) {
    w(p + 0x110 + 32 * Raw(n), 0xbdaaaaaa);
    auto a = p + 0x10c + 32 * Raw(n);
    if (lt(ONE, u(a)))
      w(a, ONE);
  }
  auto cls = byte(b(record(p) + 0x4a86));
  start_anim(p, u(u(p + 0x24) + Raw(0x68 * cls) + 0x7c), 10, 0x3dcccccc,
             0x3dcccccc);
}
void World::State::player_process_damage(Address p) {
  auto r = record(p);
  bool suppress = false;
  if (!(u(p + 0x2dc) & 0x20000000)) {
    if (b(0x44fcd0))
      suppress = true;
    else {
      if (!services.dialog_is_open)
        throw Unsupported("dialog state service missing");
      auto fn = services.dialog_is_open;
      suppress = fn();
    }
  }
  if (suppress) {
    w(p + 0x2dc, 0);
    w(p + 0xc8, 0);
    return;
  }
  auto df = u(p + 0x2dc);
  auto rel = s16(h(p + 0x2e0) - h(p + 0x38));
  int level = 0;
  bool resist = false;
  auto st = s32(u(p + 0x58c));
  if ((df & 0x3c0000) && st != 6 && st != 10) {
    if (!services.skill_stat)
      throw Unsupported("Strong Stance statistic service missing");
    auto rank = skill_rank(u(p + 0x3c8), 0x3e);
    auto fn = services.skill_stat;
    auto pct = s32(fn("SkillStrongStance_getStat0", u(0x41b268), rank));
    df = u(p + 0x2dc);
    resist = int(draw() % 100) < pct;
    constexpr Raw speeds[] = {0x42c80000, 0x43160000, 0x43480000, 0x437a0000};
    for (unsigned k = 0; k < 4; ++k)
      if (df & (0x40000u << k)) {
        w(p + 0x2e8, speeds[k]);
        level = k == 0 ? 1 : k == 1 ? 2 : 3;
        break;
      }
  }
  if (u(0x4f711c))
    throw Unsupported("PvP A");
  if (level > 0) {
    if (st == 8 && !(-0x5fff <= rel && rel < 0x6000))
      --level;
    bool small = level < 2;
    if (small || !resist)
      state3(p);
    if (!small)
      throw Unsupported("knock-down reaction");
    if (!(u(p + 0x2dc) & 0x400000))
      reaction(p);
  }
  if (!lt(0, u(p + 0xc8)) || s32(u(p + 0x58c)) == 4) {
    w(p + 0x2dc, 0);
    return;
  }
  df = u(p + 0x2dc);
  if (u(0x44f20c) && !(df & 0x20000000))
    w(p + 0xc8, 0);
  w(r + 0xac, sub(u(r + 0xac), u(p + 0xc8)));
  if ((s32(u(0x44fdc0)) > 0 || b(0x402791)) && (df & 13))
    throw Unsupported("player blood and gibs");
  if ((df & 0x10) && le(u(r + 0xac), 0))
    throw Unsupported("disintegration");
  w(p + 0xc8, 0);
  w(p + 12, u(p + 12) | 0x100);
  if (le(u(r + 0xac), 0)) {
    if (target(p + 0x364))
      throw Unsupported("Resurrect spell");
    w(r + 0xac, 0);
    w(p + 12, u(p + 12) | 8);
    if (target(p + 0x588))
      throw Unsupported("held spell destruction");
    auto model = s32(u(r + 0x54));
    bool group = model == 0 || model == 1 || model == 4 || model == 5 ||
                 model == 8 || model == 9 || model == 10 || model == 11;
    auto off = group && byte(b(r + 0x4a86)) == 2 ? 0x14 : 12;
    start_anim(p, u(u(p + 0x24) + Raw(off)), 0x206, HALF);
    w(p + 0x58c, 4);
    w(p + 0x448, u(FRAME));
    w(p + 12, u(p + 12) | 1);
    alive();
    note("player_dead", {p});
    note("presentation:net_closeLocalMenus", {1});
    w(0x402797, 1, 1);
    if (u(NUM_PLAYERS) >= 2) {
      auto other = u(p + 0x3c8) == 0 ? 1u : 0u;
      if (lt(0, u(PLAYERS + other * RECORD_SIZE + 0xac)))
        note("presentation:player_voice", {other, 3});
    }
    if (u(0x4f7120))
      throw Unsupported("PvP B");
    w(p + 0x2dc, 0);
    return;
  }
  df = u(p + 0x2dc);
  st = s32(u(p + 0x58c));
  if ((df & 0x400000) || st == 3 || st == 7 || s32(u(p + 0x44c)) == 2) {
    w(p + 0x2dc, 0);
    return;
  }
  auto chance = sub(0x3d23d70a, mul(u(r + 0xdc), 0x3d23d70a));
  if (lt(chance, 0))
    chance = 0;
  auto roll = draw();
  if (lt(from_int(roll), mul(chance, 0x47800000)) ||
      (u(p + 0x2dc) & 0x4000000)) {
    st = s32(u(p + 0x58c));
    if (st != 6 && st != 10) {
      state3(p);
      reaction(p);
    }
  }
  w(p + 0x2dc, 0);
}
void World::State::player_find_target(Address p, Raw reach) {
  record(p);
  if (str(*this, 0x41b721).find("stomedal") != std::string::npos)
    return;
  auto fx = cos16(h(p + 0x38)), fy = sin16(h(p + 0x38));
  auto pos = vec(p + 16);
  auto list = box(
      {sub(pos[0], reach), sub(pos[1], reach), sub(pos[2], 0x42900000)},
      {add(pos[0], reach), add(pos[1], reach), add(pos[2], 0x42900000)}, 0x100);
  clear(p + 0x4b8);
  Address best = 0;
  Raw score_best = 0x551184e7;
  for (auto o : list) {
    if (o == p)
      continue;
    auto f = u(o + 12);
    if ((f & 0x49) || !(f & 0x20) || s16(h(o + 0x312)))
      continue;
    pos = vec(p + 16);
    auto dx = sub(u(o + 16), pos[0]), dy = sub(u(o + 20), pos[1]);
    auto dist = sqrt(add(mul(dx, dx), mul(dy, dy))), inv = div(ONE, dist);
    auto dot = add(mul(mul(dx, inv), fx), mul(mul(dy, inv), fy));
    if (lt(dot, HALF))
      continue;
    auto edge = sub(dist, u(o + 0x1c));
    if (le(reach, edge))
      continue;
    auto score = sub(edge, mul(dot, 0x42000000));
    auto op = vec(o + 16);
    auto t = ray({add(pos[0], 0), pos[1], add(pos[2], 0x42100000)},
                 {sub(op[0], pos[0]), sub(op[1], pos[1]), sub(op[2], pos[2])},
                 0, ONE);
    if (le(0, t) || lt(score_best, score))
      continue;
    best = o;
    score_best = score;
  }
  relink(p + TARGET, best);
}
void World::State::player_melee_hit(Address p, Address t, Raw extra,
                                    Raw scale) {
  auto r = record(p);
  if (u(p + 12) & 0x20000)
    return;
  actor(t);
  if (!eq(scale, 0))
    throw Unsupported("scaled melee hit");
  auto pos = vec(p + 16), to = vec(t + 16);
  if (le(0, ray({pos[0], pos[1], add(pos[2], 0x42100000)},
                {sub(to[0], pos[0]), sub(to[1], pos[1]), sub(to[2], pos[2])}, 0,
                ONE)))
    throw Unsupported("blocked melee AutoProp exception");
  if (u(r + 0x43f0) && u(r + 0x43f4))
    throw Unsupported("dual wield");
  auto weapon = u(r + 0x4434 + 4 * Raw(byte(b(r + 0x4a7c))));
  if (weapon)
    throw Unsupported("armed player");
  recalc(p);
  Raw flags = 1;
  auto since = from_int(u(FRAME) - u(p + 0x448));
  auto cls = byte(b(r + 0x4a86));
  if (cls == 1 && le(0x42700000, since))
    flags |= 0x1000;
  flags |= cls == 3 && le(0x42700000, since) ? 0x80000 : 0x40000;
  if (u(p + 0x44c) == 2)
    throw Unsupported("heavy attack");
  if (target(p + 0x364))
    throw Unsupported("HarmTouch spell");
  auto landed = damage(t, p, from_int(u(r + 0xd0)), from_int(u(r + 0xd4)),
                       u(r + 0xcc), flags | extra);
  if (landed && (u(t + 12) & 0x20)) {
    auto cf = q(t + 0xa8);
    w(r + 0x4ac5, 1, cf & 0x2000 ? 3 : cf & 0x1000 ? 2 : cf & 0x40000 ? 1 : 0);
    w(r + 0x4ac6, 1, 5);
  }
}
void World::State::player_run(Address p) {
  auto r = record(p);
  model.creature_run(p, p + 0x544);
  knockback(p, ONE);
  w(p + 0x228, u(r + 0xac));
  if (!(u(p + 12) & 0x20000))
    w(r + 0x4c27, 1, Raw(std::max(byte(b(r + 0x4c27)) - 1, 0)));
  if (s16(h(p + 0x4b4))) {
    auto v = (h(p + 0x4b4) - 1) & 0xffff;
    w(p + 0x4b4, 2, v);
    if (s16(v) < 20)
      w(p + 12, u(p + 12) & ~2u);
  }
  if (b(p + 0x231))
    throw Unsupported("scripted player control");
  w(p + 0x4c0, 0);
  w(p + 0x4bc, 0);
  if (u(r + 0x4434 + 4 * Raw(byte(b(r + 0x4a7c)))))
    throw Unsupported("armed player tick");
  w(p + 0x48c, 0x3ea8f5c2);
  if (!(u(p + 12) & 8))
    player_process_damage(p);
  // Explicit combat-only adapter phase. Original Player_run's switch requires
  // the observed-only Player_move; it is deliberately not silently substituted.
  // External hit events may drive unarmed tests after pending damage is
  // handled.
  auto st = u(p + 0x58c);
  if (st == 4)
    return;
  if (st != 1 && st != 2 && st != 3)
    throw Unsupported("player action state outside combat phase");
  Raw ev = 0;
  for (auto it = queued_events.begin(); it != queued_events.end();) {
    auto a = target(it->first);
    if (!a || a == p) {
      if (a == p)
        ev |= it->second.second;
      clear(it->first);
      it = queued_events.erase(it);
    } else
      ++it;
  }
  if (st == 1 && (ev & 1)) {
    auto t = target(p + TARGET);
    if (t)
      player_melee_hit(p, t, 0, 0);
  }
}
void World::player_find_target(Address p, Raw r) {
  s_->player_find_target(p, r);
}
void World::player_melee_hit(Address p, Address t, Raw e, Raw z) {
  s_->player_melee_hit(p, t, e, z);
}
} // namespace champions::simulation::rta::combat_runtime
