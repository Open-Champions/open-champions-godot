#include "combat_runtime_internal.hpp"
#include "champions/fx_trig.hpp"
// Round-2 player defence (research ref_combat_ptick.DefenceModel, executed by
// difftest_combat_ptick): Player_takeDamage 0x1b5330 with the block branch,
// Player_blockSparks 0x1b9658 (silent 0), Player_processDamage 0x1b2cc0 with
// knock-downs, blood, the Resurrect save and the held spell, Player_hasAnim
// 0x1bf230 and Player_interact 0x1b2640 (nothing in reach).
namespace champions::simulation::rta::combat_runtime {
namespace {
constexpr Raw F01 = 0x3dcccccc, F02 = 0x3e4ccccc, F5 = 0x40a00000,
              F10 = 0x41200000, F36 = 0x42100000, F2M16 = 0x37800000,
              F65536 = 0x47800000, F004 = 0x3d23d70a, HOLY_SHIELD = 0x3cf124,
              RESURRECT = 0x3d162c;
constexpr Address S_FX_LUMP = 0x535ef0, G_PROMPT_TIMER = 0x4d8ac0,
                  G_PROMPT_OWNER = 0x4d8ad0, G_PROMPT_TEXT = 0x864f50,
                  G_SKILL_STRONGSTANCE = 0x41b268, G_RECALL = 0x41b51d,
                  G_MENU_LOCKOUT = 0x41b4fc, G_EXIT_COUNTDOWN = 0x4f694a,
                  G_HUD_FLASH = 0x41b930, G_DEBUG_INVULNERABLE = 0x44f20c,
                  G_DECAL_DETAIL = 0x44fdc0, G_CUTSCENE = 0x44fcd0;
// File names the executable exposes, used as identifiers (research
// ref_combat_ptick NAME_BLOCK_SOUND, NAME_SPARK_SOUND, knockdown_name).
const char *const NAME_BLOCK_SOUND = "hit_mtlplate_bludg_med.anm";
const char *const NAME_SPARK_SOUND = "hit_mtlplate_slash_miss.anm";
std::string knockdown_name(Raw weapon_class, std::int32_t rel) {
  std::string out =
      ((weapon_class & 0xff) - 2) < 2 ? "knockdown2_" : "knockdown1_";
  if (rel < -0x5fff || rel >= 0x6000)
    out += "a.anm";
  else if (((rel + 0x6000) & 0xffff) < 0x4001)
    out += "c.anm";
  else if (((rel + 0x2000) & 0xffff) < 0x4001)
    out += "b.anm";
  else
    out += "d.anm";
  return out;
}
} // namespace
Raw World::State::rec_of(Address p) const {
  return Raw(PLAYERS + Raw(s32m(p + 0x3c8)) * RECORD_SIZE);
}
std::string World::State::cstr(Address a, unsigned limit) const {
  std::string out;
  for (unsigned k = 0; k < limit; ++k) {
    auto c = b(a + k);
    if (!c)
      break;
    out += char(c);
  }
  return out;
}
Raw World::State::resource(Raw lump, const std::string &entry) {
  if (!services.resource)
    throw Unsupported("resource lookup service missing");
  ResourceQuery query;
  query.lump = lump;
  query.entry = entry;
  auto fn = services.resource;
  return fn(query);
}
Raw World::State::resource_at(Raw lump, Address entry) {
  return resource(lump, cstr(entry, 64));
}
Raw World::State::resource_text(const std::string &lump,
                                const std::string &entry) {
  if (!services.resource)
    throw Unsupported("resource lookup service missing");
  ResourceQuery query;
  query.lump_is_text = true;
  query.lump_text = lump;
  query.entry = entry;
  auto fn = services.resource;
  return fn(query);
}
void World::State::take_damage_full(Address p, Address attacker, Raw flags,
                                    Raw amount) {
  if (s8m(G_RECALL) || s16m(p + 0x4b4) || u(p + 0x540))
    return;
  if (cstr(G_LEVEL_NAME, 32) == "actintro")
    return;
  if (!services.jumpgate_travel_state)
    throw Unsupported("jumpgate state service missing");
  if (services.jumpgate_travel_state())
    return;
  if (u(G_MENU_LOCKOUT) || s16m(G_EXIT_COUNTDOWN))
    return;
  auto slot = s32m(p + 0x3c8);
  auto r = rec_of(p);
  if (s8m(0x41b69d))
    note("presentation:gfx_setColorOffset");
  auto flash = Raw(G_HUD_FLASH + 4 * Raw(slot));
  if (s8m(r + 0x4c27) > 0) {
    w(flash, u(flash) | 1);
    return;
  }
  auto state = s32m(p + 0x58c);
  if (attacker && (state == 8 || state == 0xb) && !(flags & 0x40)) {
    auto row = u(p + 0x24) + Raw(0x68 * s8m(r + 0x4a86));
    if (s32m(p + 0x58c) != 0xb) {
      auto d = draw();
      if (0xafc7 < d)
        start_anim(p, u(row + 0x8c), 0x812, F01, F01);
      else
        start_anim(p, u(row + 0x94), 0x812, F02, F02);
    }
    auto fx = cos16(Raw(s16m(p + 0x38))), fy = sin16(Raw(s16m(p + 0x38)));
    auto dx = sub(u(attacker + 16), u(p + 16)),
         dy = sub(u(attacker + 20), u(p + 20));
    if (lt(0, add(mul(fx, dx), mul(fy, dy)))) {
      resource(S_FX_LUMP, NAME_BLOCK_SOUND);
      note("presentation:sound_playAtPos");
      block_sparks(p, 0);
      amount = (flags & 0x20000) ? mul(amount, HALF) : 0;
    }
  }
  if (auto spell = find_spell_by_type(p, HOLY_SHIELD)) {
    if (lt(0, u(r + 0xc0))) {
      // The call site is the executed take-damage body. The absorb function
      // itself is the round-4 body. The round-2 corpus still records the
      // refusal, so that replay does not enter it.
      if (!round4)
        throw Unsupported("SpellHolyShield_absorb");
      // A pose word at the caster's animation state selects the round-5 sample
      // body. Round-4 pins leave that word zero and keep the stubbed service.
      auto frac = u(p + 0x114) ? holy_shield_sample(spell) : holy_shield_absorb(spell);
      auto paid = mul(amount, frac);
      amount = sub(amount, paid);
      auto mana = sub(u(r + 0xc0), paid);
      w(r + 0xc0, mana);
      if (lt(amount, ZERO))
        amount = ZERO;
      if (lt(mana, ZERO))
        w(r + 0xc0, ZERO);
    }
  }
  w(p + 0xc8, add(u(p + 0xc8), amount));
  auto df = u(p + 0x2dc) | flags;
  w(p + 0x2dc, df);
  if (attacker) {
    actor(attacker);
    if (!(df & 0x800000))
      w(p + 0x2e0, 2,
        Raw(0x4000 - atan2_16(sub(u(p + 16), u(attacker + 16)),
                              sub(u(p + 20), u(attacker + 20)))));
    clear(p + LAST_ATTACKER);
    auto src = attacker;
    if (u(attacker + 12) & 0x80) {
      if (auto node = u(attacker + 0x4c))
        src = u(node + 12);
    } else if (flags & 0x10000000) {
      if (auto node = u(p + 0x360))
        src = u(node + 12);
    }
    if (src)
      handle_link(p + LAST_ATTACKER, src, src);
  }
  if (lt(0, u(p + 0xc8))) {
    flash = Raw(G_HUD_FLASH + 4 * Raw(s32m(p + 0x3c8)));
    w(flash, u(flash) | 1);
  }
}
void World::State::block_sparks(Address p, Raw silent) {
  if (silent) {
    if (!round4)
      throw Unsupported(
          "Player_blockSparks with silent != 0 (shared draws over sample points)");
    block_sparks_silent(p, silent);
    return;
  }
  auto r = rec_of(p);
  resource(S_FX_LUMP, NAME_SPARK_SOUND);
  note("presentation:sound_playOnObject", {u(r + 0x43d8)});
  if (!services.attach_point)
    throw Unsupported("attach point service missing");
  auto fn = services.attach_point;
  Vec3 hands[2];
  for (unsigned k = 0; k < 2; ++k)
    hands[k] = fn(p, u(p + 0x544), vec(p + 16), Raw(s16m(p + 0x38)), k);
  Vec3 mid;
  for (unsigned i = 0; i < 3; ++i)
    mid[i] = mul(add(hands[0][i], hands[1][i]), HALF);
  // Silent 0 passes the sign-extended facing at +0x38 (0x1b97dc / 0x1b9800).
  auto f = champions::rta::fx::trig::process_damage_facing_angle(h(p + 0x38));
  Vec3 vel{mul(champions::rta::fx::trig::cos16(f), F10),
           mul(champions::rta::fx::trig::sin16(f), F10), F10};
  for (unsigned spark = 0; spark < 50; ++spark) {
    Vec3 v;
    unsigned attempts = 0;
    do {
      if (++attempts > 65536)
        throw Error("spark rejection budget");
      for (auto &x : v) {
        auto t = mul(from_int(own(p)), F2M16);
        x = sub(add(t, t), ONE);
      }
    } while (lt(ONE, add(add(mul(v[0], v[0]), mul(v[1], v[1])),
                         mul(v[2], v[2]))));
    for (unsigned i = 0; i < 3; ++i)
      vel[i] = add(vel[i], mul(v[i], F5));
    note("presentation:particle_spawn",
         {mid[0], mid[1], mid[2], vel[0], vel[1], vel[2]});
  }
}
Raw World::State::has_anim(Address p, const std::string &name) {
  auto anm = resource(u(u(p + 0x24)), name);
  for (int i = 0; i < std::max(s8m(p + 0x118), 0); ++i)
    if (u(p + 0x11c + 0x20 * Raw(i)) == anm)
      return 1;
  return 0;
}
void World::State::interact(Address p, Address, Raw display_only) {
  if (display_only)
    throw Unsupported("Player_interact with displayOnly (the HUD caller)");
  if (u(p + 0x540))
    return;
  auto r = rec_of(p);
  if (le(u(r + 0xac), 0))
    return;
  auto o = u(r + 0x43d8);
  if (!o)
    return;
  auto op = vec(o + 16);
  auto found =
      box({sub(op[0], 0x42400000), sub(op[1], 0x42400000), op[2]},
          {add(op[0], 0x42400000), add(op[1], 0x42400000), add(op[2], 0x42700000)},
          0x100);
  int best = -1;
  Raw nearest = 0x4b189680;
  for (unsigned i = 0; i < found.size(); ++i) {
    auto obj = found[i];
    if (!(u(obj + 12) & 0x100000))
      continue;
    Vec3 d;
    for (unsigned k = 0; k < 3; ++k)
      d[k] = sub(u(obj + 16 + 4 * k), op[k]);
    auto dist = add(add(mul(d[0], d[0]), mul(d[1], d[1])), mul(d[2], d[2]));
    if (lt(dist, nearest)) {
      nearest = dist;
      best = int(i);
    }
  }
  if (best != -1)
    throw Unsupported(
        "Player_interact with an interactable object (flag 0x100000) in reach");
  auto slot = Raw(s32m(p + 0x3c8));
  auto timer = Raw(G_PROMPT_TIMER + 4 * slot);
  auto t = s32(u(timer) - 7);
  w(timer, Raw(t));
  if (t <= 0) {
    w(Raw(G_PROMPT_OWNER + 4 * slot), 0);
    w(timer, 0);
    auto text = Raw(G_PROMPT_TEXT + 0x200 * slot);
    w(text, 2, 0);
    w(text + 2, 2, 0);
  }
}
void World::State::state3(Address p) {
  w(p + 0x58c, 3);
  w(p + 0x448, u(FRAME));
  w(G_GHOST_REBASE, 1, 1);
  note("presentation:player_voice", {u(p + 0x3c8), 12});
}
void World::State::track_fixup(Address p) {
  auto n = s8m(p + 0x118);
  if (n >= 5) {
    w(p + 0x110 + 32 * Raw(n), 0xbdaaaaaa);
    auto a = p + 0x10c + 32 * Raw(n);
    if (lt(ONE, u(a)))
      w(a, ONE);
  }
}
void World::State::reaction(Address p) {
  auto cls = s8m(rec_of(p) + 0x4a86);
  start_anim(p, u(u(p + 0x24) + Raw(0x68 * cls) + 0x7c), 10, 0x3dcccccc,
             0x3dcccccc);
}
void World::State::process_damage_full(Address p) {
  if (!(u(p + 0x2dc) & 0x20000000) && (s8m(G_CUTSCENE) || dialog_open())) {
    w(p + 0x2dc, 0);
    w(p + 0xc8, 0);
    return;
  }
  auto r = rec_of(p);
  auto df = u(p + 0x2dc);
  auto rel = s16(h(p + 0x2e0) - h(p + 0x38));
  int level = 0;
  bool resist = false;
  auto state = s32m(p + 0x58c);
  if ((df & 0x3c0000) && state != 6 && state != 10) {
    auto rank = number(full_vcall(p, 39, {iv(0x3e)}));
    if (!services.skill_stat)
      throw Unsupported("Strong Stance statistic service missing");
    auto fn = services.skill_stat;
    auto pct = s32(fn("SkillStrongStance_getStat0", u(G_SKILL_STRONGSTANCE),
                      rank));
    df = u(p + 0x2dc);
    resist = std::int32_t(draw() % 100) < pct;
    constexpr Raw speeds[] = {0x42c80000, 0x43160000, 0x43480000, 0x437a0000};
    constexpr int levels[] = {1, 2, 3, 3};
    for (unsigned k = 0; k < 4; ++k)
      if (df & (0x40000u << k)) {
        w(p + 0x2e8, speeds[k]);
        level = levels[k];
        break;
      }
  }
  if (u(G_PVP_A) && level) {
    --level;
    w(p + 0x2e8, mul(u(p + 0x2e8), HALF));
  }
  if (level > 0) {
    if (s32m(p + 0x58c) == 8 && !(-0x5fff <= rel && rel < 0x6000))
      --level;
    bool small = level < 2;
    if (small || !resist)
      state3(p);
    if (small) {
      if (!(u(p + 0x2dc) & 0x400000)) {
        track_fixup(p);
        reaction(p);
      }
    } else {
      auto name = knockdown_name(b(r + 0x4a86), rel);
      if (resist) {
        if (!(u(p + 12) & 0x20000))
          throw Unsupported("SkillStrongStance_trigger (a resisted knock-down)");
        w(p + 0x2e8, 0);
      } else if (!has_anim(p, name)) {
        track_fixup(p);
        auto anm = resource(u(u(p + 0x24)), name);
        start_anim(p, anm, 0x80a, F01, F01);
      }
    }
  }
  if (!lt(0, u(p + 0xc8)) || s32m(p + 0x58c) == 4) {
    w(p + 0x2dc, 0);
    return;
  }
  if (u(G_DEBUG_INVULNERABLE) && !(u(p + 0x2dc) & 0x20000000))
    w(p + 0xc8, 0);
  r = rec_of(p);
  w(r + 0xac, sub(u(r + 0xac), u(p + 0xc8)));
  Vec3 point{u(p + 16), u(p + 20), add(u(p + 24), F36)};
  auto decal = s32m(G_DECAL_DETAIL);
  if ((decal > 0 || s8m(G_NETWORK)) && (u(p + 0x2dc) & 0xd)) {
    if (decal > 0)
      note("presentation:fx_spawn", {0, point[0], point[1], point[2]});
    if (!(u(p + 12) & 0x20000) && !(draw() & 7))
      if (pool_new(0x268))
        throw Unsupported("GutChunk_ctor (a blood chunk object)");
  }
  r = rec_of(p);
  if ((u(p + 0x2dc) & 0x10) && le(u(r + 0xac), 0))
    note("presentation:fx_disintegrateModel");
  w(p + 0xc8, 0);
  w(p + 12, u(p + 12) | 0x100);
  r = rec_of(p);
  if (le(u(r + 0xac), 0))
    player_death(p);
  else
    player_stagger(p);
}
void World::State::player_death(Address p) {
  if (find_spell_by_type(p, RESURRECT) && s32m(NUM_PLAYERS) == 1)
    throw Unsupported("SpellResurrect_onOwnerDeath");
  auto r = rec_of(p);
  if (!le(u(r + 0xac), 0)) {
    w(p + 0x2dc, 0);
    return;
  }
  w(r + 0xac, 0);
  w(p + 12, u(p + 12) | 8);
  if (auto node = u(p + 0x588))
    if (auto spell = u(node + 12))
      spell_virtual(spell, 1, {iv(3)});
  r = rec_of(p);
  auto model_index = u(r + 0x54);
  bool group = model_index == 0 || model_index == 1 || model_index == 4 ||
               model_index == 5 || model_index == 8 || model_index == 9 ||
               model_index == 10 || model_index == 11;
  Raw off = group && s8m(r + 0x4a86) == 2 ? 0x14 : 0xc;
  start_anim(p, u(u(p + 0x24) + off), 0x206, HALF);
  w(p + 0x58c, 4);
  w(p + 0x448, u(FRAME));
  w(p + 12, u(p + 12) | 1);
  alive();
  note("player_dead", {p});
  note("presentation:net_closeLocalMenus", {1});
  w(G_GHOST_REBASE, 1, 1);
  if (s32m(NUM_PLAYERS) >= 2) {
    auto other = u(p + 0x3c8) == 0 ? 1u : 0u;
    if (lt(0, u(PLAYERS + other * RECORD_SIZE + 0xac)) && !u(G_PVP_A))
      note("presentation:player_voice", {other, 3});
  }
  if (u(G_PVP_A))
    if (auto node = u(p + LAST_ATTACKER)) {
      auto killer = u(node + 12);
      if (!killer)
        throw Error("a last-attacker node without an object");
      if (u(killer + 12) & 0x40)
        throw Unsupported("PvP mode A kill credit (mana, a skill 0x3b orb)");
    }
  if (u(G_PVP_B))
    throw Unsupported("PvP mode B arena lives");
  w(p + 0x2dc, 0);
}
void World::State::player_stagger(Address p) {
  auto state = s32m(p + 0x58c);
  if ((u(p + 0x2dc) & 0x400000) || state == 3 || state == 7 ||
      s32m(p + 0x44c) == 2) {
    w(p + 0x2dc, 0);
    return;
  }
  auto r = rec_of(p);
  auto chance = sub(F004, mul(u(r + 0xdc), F004));
  if (lt(chance, 0))
    chance = 0;
  if (lt(from_int(draw()), mul(chance, F65536)) ||
      (u(p + 0x2dc) & 0x4000000)) {
    state = s32m(p + 0x58c);
    if (state != 6 && state != 10) {
      state3(p);
      track_fixup(p);
      reaction(p);
    }
  }
  w(p + 0x2dc, 0);
}
void World::State::player_take_damage(Address p, Address a, Raw f, Raw n) {
  take_damage_full(p, a, f, n);
}
void World::State::player_process_damage(Address p) { process_damage_full(p); }
} // namespace champions::simulation::rta::combat_runtime
