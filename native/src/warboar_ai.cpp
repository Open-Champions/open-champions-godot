#include "champions/campaign_actors.hpp"

// Authored from research-boar-ai 3f81c89, warboar-ai-handoff.md and its
// executed 530-case probe; inherited services retain creature-tick contracts.
namespace champions::simulation::rta::campaign_actors {
namespace {
template <class F> void need(const F &fn, const char *name) {
  if (!fn) throw Unsupported(std::string(name) + ": Unknown service");
}
constexpr Raw F10 = 0x41200000, F20 = 0x41a00000, F25 = 0x41c80000,
              F100 = 0x42c80000;
}

std::uint32_t Runtime::own_draw(Actor &a) {
  if (!shared_) return a.object->rand();
  const auto seed = std::uint32_t(a.hp.mem.load(a.object->addr + 0x44, 4)) *
                    0x19660du + 0x3c6ef35fu;
  a.hp.mem.store(a.object->addr + 0x44, 4, seed);
  return seed >> 16;
}

void Runtime::enter_boar_state(Actor &a, std::uint32_t state) {
  const auto at = a.object->addr;
  auto &mem = a.hp.mem;
  // The filter runs first, including root/stun/cooldown redirects. The class
  // then refuses a hit reaction during 6..15 without changing the old state.
  if (state == 18 && a.state >= 6 && a.state <= 15) return;
  need(services_.animation_start, "WarBoar_setState/animation");
  const auto start = [&](AnimationRequest request) { services_.animation_start(at, request); };
  const auto clear = [&] {
    need(services_.clear_path, "Creature_clearPath"); services_.clear_path(at);
  };
  switch (state) {
  case 0:
    start({0x84, 3, 0x3dcccccc});
    mem.store(at + 0xf4, 4, std::uint32_t(mem.load(at + 0x300, 4)) + 180u);
    break;
  case 1:
    clear();
    need(services_.pick_wander_dest, "Creature_pickWanderDest");
    services_.pick_wander_dest(at);
    start({0x94, 0x803, 0x3dcccccc, 0, true});
    break;
  case 3:
    clear(); start({0x4c, 0x803, 0x3e4ccccc, 0, true}); break;
  case 5:
    start({0x84, 3, 0x3e4ccccc, 0, true}); break;
  case 6: case 7: case 8: case 9: case 10: {
    constexpr unsigned offsets[]{0x24, 0x54, 0x2c, 0x3c, 0x34};
    constexpr Raw flags[]{8, 0x808, 0x4008, 0x4008, 0x808};
    mem.store(at + 0x3c8, 4, 0);
    mem.store(at + 0x3cc, 4, state);
    a.hit_frame = 0; a.last_attack = state;
    start({offsets[state - 6], flags[state - 6], 0x3e999999, 0x3e999999});
    break;
  }
  case 16:
    clear(); start({0x44, 0x803, 0x3dcccccc, 0, true}); break;
  case 18:
    start({0x6c, 0x10, 0x3dcccccc, 0x3dcccccc}); break;
  case 19:
    start({0x84, 3, 0x3dcccccc}); break;
  default:
    throw Unsupported("WarBoar_setState: unexecuted state entry");
  }
  a.state = state;
  mem.store(at + 0x232, 2, state);
}

void Runtime::tick_boar(Actor &a, Raw events) {
  const auto at = a.object->addr;
  auto &mem = a.hp.mem;
  const auto word = [&](unsigned off, unsigned size = 4) { return Raw(mem.load(at + off, size)); };
  const auto face = [&](int offset = 0) { return Raw(std::int32_t(std::int16_t(word(0x38, 2) + offset))); };
  const auto target = [&] { return word(0x234) != 0; };
  const auto see = [&](Raw facing, Raw arc, Raw near, Raw far) {
    need(services_.target_in_arc, "Creature_canSeeTargetIn");
    return services_.target_in_arc(at, facing, arc, near, far);
  };
  const auto enemy = [&](Raw facing, Raw arc = 3000u) {
    need(services_.enemy_in_cone, "creature_findEnemyInCone");
    return services_.enemy_in_cone(at, facing, arc, 0, F25);
  };
  const auto st = [&](Raw state) { set_state(a, state); };
  const auto turn = [&] {
    need(services_.turn_to_target, "Creature_turnToTarget"); services_.turn_to_target(at);
  };
  const auto close_choice = [&] {
    const auto draw = own_draw(a) % 5;
    st(draw == 0 ? 8 : draw == 1 ? 9 : 6);
  };
  switch (a.state) {
  case 0: case 1: {
    need(services_.can_see_target, "Creature_canSeeTarget");
    if (services_.can_see_target(at, 0)) { st(a.state == 0 ? 3 : 0); return; }
    if (a.state == 1) {
      need(services_.move_to_dest, "Creature_moveToDest"); services_.move_to_dest(at); return;
    }
    need(services_.away_from_home, "Creature_isAwayFromHome");
    if (services_.away_from_home(at) || std::int32_t(word(0xf4)) < std::int32_t(word(0x300))) st(1);
    return;
  }
  case 3:
    if (!target()) { st(0); return; }
    if (see(face(), 3000, 0, F25)) { close_choice(); return; }
    if (see(face(), 3000, F25, F100)) { st(10); return; }
    if (see(face(8000), 3000, 0, F25)) { st(8); return; }
    if (see(face(-8000), 3000, 0, F25)) { st(9); return; }
    if (enemy(face())) { close_choice(); return; }
    if (enemy(face(8000))) { st(8); return; }
    if (enemy(face(-8000))) { st(9); return; }
    if (see(face(), 0x7fff, 0, F20)) { turn(); return; }
    need(services_.move_to_target, "Creature_moveToTarget"); services_.move_to_target(at); return;
  case 5:
    if (target()) {
      if (see(face(), 3000, 0, F25)) {
        Raw next = 6;
        if (word(0x3cc) == 6 && own_draw(a) % 3 == 0) {
          constexpr Raw choices[]{10, 8, 9}; next = choices[own_draw(a) % 3];
        }
        st(next); return;
      }
      if (see(face(8000), 3000, 0, F25)) { st(8); return; }
      if (see(face(-8000), 3000, 0, F25)) { st(9); return; }
      if (enemy(face())) { st(6); return; }
      if (enemy(face(8000))) { st(8); return; }
      if (enemy(face(-8000))) { st(9); return; }
    }
    if (word(0x314, 2) == 0) st(3);
    return;
  case 6: case 7: case 8: case 9: case 10: {
    need(services_.move_zero, "Creature_moveBy"); services_.move_zero(at);
    if (events & 1) {
      Raw flags = 8;
      if (a.state == 7 || (a.state == 10 && !word(0x3d0, 1)))
        flags = own_draw(a) % 3 == 0 ? 0x80008 : 0x40008;
      need(services_.hit_all_in_arc, "Creature_hitAllEnemiesInArc");
      services_.hit_all_in_arc(at, flags);
      a.hit_frame = 1; mem.store(at + 0x3c8, 4, 1);
    }
    if (word(0x11a, 2) & 8) {
      if ((a.state == 7 || a.state == 10 || (a.state == 6 && !word(0x3c8))) &&
          see(face(), 3000, 0, F10)) turn();
    }
    if ((events & 2) && (see(face(), 8000, 0, F25) || enemy(face(), 8000))) st(5);
    else if (!(word(0x11a, 2) & 8)) st(3);
    return;
  }
  case 16:
    if (!target()) mem.store(at + 0x318, 2, 0);
    if (!word(0x318, 2)) st(0);
    else {
      need(services_.flee_from_target, "Creature_fleeFromTarget"); services_.flee_from_target(at, 0);
    }
    return;
  case 18:
    if (!(word(0x11a, 2) & 0x10)) st(3);
    return;
  case 19:
    if (!word(0x31c, 2)) st(3);
    return;
  case 20:
    if (events & 4) mem.store(at + 12, 4, word(12) | 1u);
    need(services_.dead_update, "Creature_deadUpdate"); services_.dead_update(at, events); return;
  case 2: case 4: case 11: case 12: case 13: case 14: case 15: case 17:
    return; // documented empty switch cases
  default: throw Unsupported("WarBoar_run: unexecuted state");
  }
}
} // namespace champions::simulation::rta::campaign_actors
