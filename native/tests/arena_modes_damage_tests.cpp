// Versus against story damage through the real combat::damage_apply, in the
// shape of research t3/research-arena difftest_arena_modes.py test_pvp: the
// same hit twice with g_pvpModeA clear and set, attack rating 100000, no
// critical, no descriptor, flags 0, the cutscene flag set, a flat range.
// Expectations are arena_modes::armour_bound and pvp_half (docs/arena-modes.md
// section 7). The attacker is a synthetic non-player creature.
#include "champions/arena_modes.hpp"
#include "champions/combat_damage.hpp"

#include <cstdio>
#include <vector>

using namespace champions::simulation::rta;
namespace am = champions::simulation::rta::arena_modes;
using namespace champions::simulation::rta::combat;

namespace {
int g_fails = 0;
#define CHECK(cond)                                                                            \
  do {                                                                                         \
    if (!(cond)) {                                                                             \
      std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);                      \
      ++g_fails;                                                                               \
    }                                                                                          \
  } while (0)

struct Hit {
  std::vector<Raw> taken;
  bool stun = false;
};

Hit hit(bool pvp, bool player, std::int64_t armor, std::int32_t amount) {
  World w(20973, 1);
  w.pvp_a = pvp ? 1 : 0;
  w.cutscene = 1;
  w.stat_fn = [](std::int64_t, std::int64_t, std::int64_t) { return 0; };
  w.stat_for_fn = [](std::int64_t, std::int64_t, std::int64_t, Obj *, std::int64_t) { return 0; };
  w.adjust_fn = [](std::int64_t, std::int64_t, std::int64_t n) { return n; };
  w.pool_fn = [](std::int64_t) { return true; };
  w.enemy_fn = [](Obj *, Obj *) { return 1; };
  Hit out;
  w.take_damage_fn = [&](Obj &, Raw value, Obj *, std::uint32_t) { out.taken.push_back(value); };
  Obj victim;
  victim.tag = U"victim";
  victim.flags = 0x20 | (player ? 0x40 : 0);
  victim.armor = armor;
  victim.hp = from_int(1000);
  victim.max_hp = from_int(1000);
  Obj attacker;
  attacker.tag = U"attacker";
  attacker.flags = 0x20;
  if (player) {
    victim.slot = 0;
    w.players.emplace_back();
    w.players[0].obj = &victim;
    w.num_players = 1;
  }
  const auto flat = from_int(static_cast<std::uint32_t>(amount));
  damage_apply(w, victim, &attacker, flat, flat, 100000, 0, nullptr, 1);
  for (const auto &event : w.events)
    if (!event.empty() && std::holds_alternative<Text>(event[0].value) &&
        std::get<Text>(event[0].value) == U"stun")
      out.stun = true;
  return out;
}
} // namespace

int main() {
  const auto off = hit(false, true, 0, 40);
  const auto on = hit(true, true, 0, 40);
  CHECK(off.taken.size() == 1 && on.taken.size() == 1);
  if (off.taken.size() == 1 && on.taken.size() == 1) {
    CHECK(off.taken[0] != ZERO);
    CHECK(on.taken[0] == am::pvp_half(off.taken[0]));
  }
  CHECK(!on.stun);
  const auto mon_off = hit(false, false, 300, 100);
  const auto mon_on = hit(true, false, 300, 100);
  CHECK(mon_off.taken.size() == 1 && mon_on.taken.size() == 1);
  if (mon_off.taken.size() == 1 && mon_on.taken.size() == 1) {
    CHECK(mon_off.taken[0] == from_int(100));
    CHECK(mon_on.taken[0] == am::armour_bound(from_int(100), 300));
    CHECK(mon_on.taken[0] != am::pvp_half(mon_off.taken[0]));
  }
  if (g_fails) {
    std::fprintf(stderr, "arena damage: %d failed\n", g_fails);
    return 1;
  }
  std::puts("arena damage: ok");
  return 0;
}
