#include "combat_runtime_internal.hpp"
// Round-2 player offence (research ref_combat_melee.MeleeModel, executed by
// difftest_combat_melee): Player_pickAttackAnim 0x1bba88, Player_meleeHit
// 0x1b6a98 (every branch) and Player_hitSparks 0x1bcc20.
namespace champions::simulation::rta::combat_runtime {
int damage_with_descriptor(World::State &, Address, Address, Raw, Raw,
                           const combat::Descriptor &, Raw, Raw);
namespace {
constexpr Raw F36 = 0x42100000, F60 = 0x42700000, AUTOPROP_DESC = 0x9089f0;
constexpr Address G_CLASS_SKILLS = 0x4ef538;
constexpr std::uint64_t PASSIVE_DUAL_WIELD = 0x40000, PASSIVE_HEAVY = 0x20000;
Raw frame_hash(Raw frame) { return (frame * 0x19660dU + 0x3c6ef35fU) & 0xffff; }
struct Cut {
  Raw x;
  int v;
};
int pick(Raw h, int v0, std::initializer_list<Cut> cuts) {
  int value = v0;
  for (auto &c : cuts)
    if (h >= c.x)
      value = c.v;
  return value;
}
std::vector<int> pick_attack_variant(std::int32_t mi, std::int32_t cls,
                                     std::int32_t old, Raw h) {
  std::vector<int> out;
  if (mi == 9) {
    if (cls == 5) {
      if (old == 1)
        out.push_back(2);
      else if (old == 2)
        out.push_back(3);
      else if (old == 3)
        out.push_back(1);
    } else if (cls == 0) {
      if (old == 1)
        out.push_back(pick(h, 2, {{0x5555, 3}, {0xaaaa, 5}}));
      else if (old == 2)
        out.push_back(pick(h, 1, {{0x7fff, 3}}));
      else if (old == 3)
        out.push_back(4);
      else if (old == 4)
        out.push_back(pick(h, 2, {{0x7fff, 5}}));
      else if (old == 5) {
        out.push_back(4);
        if (h >= 0x5555)
          out.push_back(pick(h, 3, {{0xaaaa, 1}}));
      }
    } else if (cls == 1) {
      if (old == 1 || old == 3)
        out.push_back(2);
      else if (old == 2)
        out.push_back(pick(h, 1, {{0x7fff, 3}}));
    }
    if (cls == 0 || cls == 1 || cls == 5)
      return out;
  }
  if (cls == 0) {
    if (mi == 11) {
      switch (old) {
      case 1: out.push_back(pick(h, 2, {{0x7000, 4}, {0xe000, 5}})); break;
      case 2: out.push_back(pick(h, 3, {{0x6aa8, 1}, {0xd334, 5}})); break;
      case 3: out.push_back(pick(h, 4, {{0x7000, 2}, {0xe000, 5}})); break;
      case 4: out.push_back(pick(h, 5, {{0x53ed, 1}, {0xaaaa, 3}})); break;
      default: out.push_back(pick(h, 1, {{0x7fff, 3}})); break;
      }
    } else {
      switch (old) {
      case 1: out.push_back(pick(h, 2, {{0x7fff, 4}})); break;
      case 2: out.push_back(pick(h, 1, {{0x5555, 3}, {0xaaaa, 5}})); break;
      case 3: out.push_back(pick(h, 2, {{0x7fff, 4}})); break;
      case 4: out.push_back(pick(h, 1, {{0x5555, 3}, {0xaaaa, 5}})); break;
      default: out.push_back(pick(h, 2, {{0xbb80, 4}})); break;
      }
    }
  } else if (cls == 2 || cls == 3) {
    if (old == 1)
      out.push_back(pick(h, 2, {{0x7fff, 4}}));
    else if (old == 2)
      out.push_back(pick(h, 1, {{0x7fff, 3}}));
    else if (old == 3) {
      if (mi == 11 && cls == 2)
        out.push_back(pick(h, 2, {{0xbb80, 4}}));
      else
        out.push_back(pick(h, 2, {{0x7fff, 4}}));
    } else if (old == 4)
      out.push_back(pick(h, 1, {{0x7fff, 3}}));
  } else if (cls == 1 || cls == 5) {
    if (cls == 5 && mi == 11) {
      switch (old) {
      case 2: out.push_back(pick(h, 1, {{0x6d60, 3}, {0xdac0, 4}})); break;
      case 3: out.push_back(pick(h, 2, {{0xbb80, 4}})); break;
      case 4: out.push_back(pick(h, 2, {{0x7fff, 3}})); break;
      default: out.push_back(pick(h, 2, {{0x7fff, 4}})); break;
      }
    } else {
      if (old == 1 || old == 3)
        out.push_back(2);
      else if (old == 2)
        out.push_back(pick(h, 1, {{0x7fff, 3}}));
    }
  } else if (cls == 7)
    out.push_back(old == 1 ? 2 : 1);
  return out;
}
Raw skill_rank_scale(std::int32_t rank) {
  auto x = from_int(rank - 1);
  return mul(add(mul(x, 0x411fff98), 0x42c7ffc3), 0x3c23d70a);
}
std::int32_t round_half(Raw x) {
  return to_int(le(0, x) ? add(x, HALF) : sub(x, HALF));
}
// Type-info getters (virtual slot 0) and their descriptors: the union of the
// research's ranged and melee tables.
const std::map<Raw, Raw> TYPE_DESC = {
    {0x3c2608, 0x908130}, {0x3c9558, 0x9089e0}, {0x3c3308, 0x908140},
    {0x3cc62c, 0x908e20}, {0x3c7bc8, 0x908560}, {0x3c62c0, 0x908330},
    {0x3c3ab8, 0x9083d0}, {0x3cc6dc, 0x908e10}, {0x3c9508, 0x9089f0},
    // AntQueen_typeinfo and Soul_typeinfo (symbols/runtime.tsv descriptors);
    // their name words enter the image with the first such creature.
    {0x3c3c78, 0x908680}, {0x3cc7a0, 0x908e30},
    // LooseFire, Fire and poisonGasSpell (the potion endings).
    {0x3c67e8, 0x908200}, {0x3c50b8, 0x908690}, {0x3c78e8, 0x908700},
    // research-combat-ctors-3 symbols/runtime.tsv: LightEffect_typeinfo.
    {0x3c6908, 0x9085d0},
    // research-combat-ctors-3 symbols/runtime.tsv:500 pins getter 0x3c7318
    // to descriptor 0x9083c0. Descriptor-identity equality is a World adapter,
    // not an executed RTTI rule (see combat_runtime_services.cpp).
    {0x3c7318, 0x9083c0},
    // research-combat-ctors-3 symbols/runtime.tsv: HolyStrike_typeinfo.
    {0x3cf5bc, 0x909350}};
} // namespace
Raw World::State::type_of(Address obj) {
  auto fn = u(u(obj + 0x48) + 4);
  auto i = TYPE_DESC.find(fn);
  if (i == TYPE_DESC.end())
    throw Unsupported("the type-info getter " + std::to_string(fn) + " of " +
                      std::to_string(obj));
  return i->second;
}
bool World::State::same_type(Raw a, Raw b) const {
  if (a == b)
    return true;
  return cstr(u(a), 64) == cstr(u(b), 64);
}
Raw World::State::static_type(const std::string &name) {
  static const std::map<std::string, Raw> statics = {
      {"AutoProp_typeinfo", 0x9089f0},
      {"AutoContainer_typeinfo", 0x9089e0},
      {"Container_typeinfo", 0x908140},
      {"Skeleton_typeinfo", 0x908e20},
      {"PushPhysicsProp_typeinfo", 0x908560}};
  auto i = statics.find(name);
  if (i == statics.end())
    throw Unsupported("type-info getter " + name);
  return i->second;
}
std::int32_t World::State::passive_level(Raw slot, std::uint64_t mask) const {
  if (slot >= 5)
    return 0;
  auto r = PLAYERS + Raw(s32(slot)) * RECORD_SIZE;
  auto table = Raw(G_CLASS_SKILLS + Raw(s32m(r + 0x50)) * 0xa88);
  for (unsigned n = 0; n < 0x30; ++n)
    if (q(table + n * 0x38) == mask) {
      auto level = s8m(r + 0xef + n);
      return level < 0 ? 0 : level;
    }
  return 0;
}
void World::State::pick_attack_anim(Address p, Raw cls) {
  auto slot = u(p + 0x3c8);
  auto r = Raw(PLAYERS + Raw(s32(slot)) * RECORD_SIZE);
  draw();
  auto hash = frame_hash(u(FRAME));
  for (auto v : pick_attack_variant(s32m(r + 0x54), s32(cls), s8m(p + 0x461),
                                    hash))
    w(p + 0x461, 1, Raw(v));
}
Raw World::State::model_of(Address obj) {
  auto fn = u(u(obj + 0x48) + 8 * 16 + 4);
  note("virtual", {obj, 16});
  if (fn == 0x3c2738)
    return u(u(obj + 0x24) + 12);
  if (fn == 0x3c3b38)
    return u(obj + 0x544);
  if (fn == 0x3c8120)
    return 0;
  return number(full_vcall(obj, 16, {}));
}
void World::State::hit_sparks(Address p, Address target) {
  if (!services.sample_point_count)
    throw Unsupported("model sample point service missing");
  auto count = services.sample_point_count;
  auto n_a = s32(count(u(p + 0x544)));
  auto n_b = s32(count(number(full_vcall(target, 16, {}))));
  (void)n_b;
  if (n_a == 0)
    throw Unsupported(
        "Player_hitSparks: a model without sample points (division by zero)");
  draw();
  throw Unsupported("model_getSamplePoint (model geometry)");
}
void World::State::melee_hit_full(Address p, Address target, Raw, Raw extra,
                                  Raw scale) {
  if (u(p + 12) & 0x20000)
    return;
  auto slot = u(p + 0x3c8);
  auto r = Raw(PLAYERS + Raw(s32(slot)) * RECORD_SIZE);
  if (eq(scale, 0)) {
    auto pos = vec(p + 16), to = vec(target + 16);
    auto t = ray({add(pos[0], 0), pos[1], add(pos[2], F36)},
                 {sub(to[0], pos[0]), sub(to[1], pos[1]), sub(to[2], pos[2])},
                 0, ONE);
    if (le(0, t)) {
      if (!full())
        throw Unsupported("blocked melee AutoProp exception (type names are "
                          "a Full-composition fixture)");
      if (!target)
        throw Unsupported("typeid of a null target (cxx_throwBadTypeid)");
      typeid_target = target;
      has_typeid_target = true;
      auto mine = type_of(target);
      if (!same_type(mine, AUTOPROP_DESC))
        return;
    }
  }
  Raw a3 = 0, a4 = 0, flags = 1;
  unsigned hand = 0;
  bool chosen = false;
  Raw weapon = 0;
  if (u(r + 0x43f0) && u(r + 0x43f4))
    if (passive_level(slot, PASSIVE_DUAL_WIELD) > 0 && (b(p + 0x460) & 1)) {
      weapon = u(r + 0x443c);
      hand = 1;
      chosen = true;
    }
  if (!chosen)
    weapon = u(Raw(r + 0x4434 + 4 * Raw(s8m(r + 0x4a7c))));
  recalc_weapon(p, weapon, 0);
  if (weapon) {
    flags = u(u(weapon) + 0x20);
    a3 = item_rating(weapon);
    a4 = u(weapon + 8);
  }
  auto cls = s8m(r + 0x4a86);
  auto since = from_int(s32(u(FRAME) - u(p + 0x448)));
  if (cls == 1 && le(F60, since))
    flags |= 0x1000;
  Raw knock;
  if (!eq(scale, 0))
    knock = 0x80000;
  else
    knock = cls == 3 && le(F60, since) ? 0x80000 : 0x40000;
  flags |= knock;
  if (weapon && (cls == 4 || s8m(u(weapon) + 0x1e) == 6)) {
    recalc_weapon(p, 0, 0);
    flags |= 0x200000;
  }
  if (u(p + 0x44c) == 2) {
    flags |= 0x200000;
    auto s = skill_rank_scale(passive_level(slot, PASSIVE_HEAVY));
    auto lo = s32m(r + 0xd0);
    w(r + 0xd0, Raw(lo + round_half(mul(from_int(lo), s))));
    auto hi = s32m(r + 0xd4);
    w(r + 0xd4, Raw(hi + round_half(mul(from_int(hi), s))));
    if (u(target + 12) & 0x20)
      hit_sparks(p, target);
  }
  if (!eq(scale, 0)) {
    w(r + 0xd0, Raw(round_half(mul(from_int(s32m(r + 0xd0)), scale))));
    w(r + 0xd4, Raw(round_half(mul(from_int(s32m(r + 0xd4)), scale))));
  }
  auto dmin = s32m(r + 0xd0), dmax = s32m(r + 0xd4);
  if (find_spell_by_type(p, SPELL_HARM_TOUCH_GETTER))
    throw Unsupported("SpellHarmTouch_applyToHit");
  auto result = damage_full(target, p, u(r + 0xcc), a3, a4, flags | extra,
                            weapon, from_int(dmin), from_int(dmax));
  if (result && (u(target + 12) & 0x20)) {
    auto cf = q(target + 0xa8);
    w(r + 0x4ac5 + hand * 0x40, 1,
      cf & 0x2000 ? 3 : cf & 0x1000 ? 2 : cf & 0x40000 ? 1 : 0);
    w(r + 0x4ac6 + hand * 0x40, 1, 5);
  }
}
void World::State::player_melee_hit(Address p, Address t, Raw extra,
                                    Raw scale) {
  record(p);
  actor(t);
  melee_hit_full(p, t, 0, extra, scale);
}
} // namespace champions::simulation::rta::combat_runtime
