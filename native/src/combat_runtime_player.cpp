#include "combat_runtime_internal.hpp"
namespace champions::simulation::rta::combat_runtime {
namespace {
std::string str(const World::State &s, Address a, unsigned limit = 32) {
  std::string out;
  for (unsigned k = 0; k < limit && s.b(a + k); ++k)
    out += char(s.b(a + k));
  return out;
}
} // namespace
void World::State::player_find_target(Address p, Raw reach) {
  record(p);
  if (str(*this, 0x41b721, full() ? 64 : 32).find("stomedal") !=
      std::string::npos)
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
    process_damage_full(p);
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
      melee_hit_full(p, t, 0, 0, 0);
  }
}
void World::player_find_target(Address p, Raw r) {
  s_->player_find_target(p, r);
}
void World::player_melee_hit(Address p, Address t, Raw e, Raw z) {
  s_->player_melee_hit(p, t, e, z);
}
} // namespace champions::simulation::rta::combat_runtime
