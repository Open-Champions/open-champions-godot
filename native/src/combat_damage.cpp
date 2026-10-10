#include "champions/combat_damage.hpp"
#include <algorithm>

namespace champions::simulation::rta::combat {
namespace {
EventValue tag(const Obj *object) {
  return object ? EventValue(object->tag) : EventValue(nullptr);
}
thread_local unsigned damage_depth = 0;
struct DamageDepth {
  DamageDepth() {
    if (damage_depth == 128)
      throw Unsupported("damage recursion safety exhausted");
    ++damage_depth;
  }
  ~DamageDepth() { --damage_depth; }
};
} // namespace
Raw base_roll(Raw minimum, Raw maximum, std::uint32_t draw) {
  const auto span = dbl_from_single(fadd(fsub(maximum, minimum), ONE));
  return dbl_to_single(dbl_add(dbl_from_single(minimum),
                               dbl_fmod(dbl_from_unsigned(draw), span)));
}
void set_status(World &w, Obj &obj, std::int64_t which, Raw value,
                Obj *source) {
  at(obj.status, which) = value;
  if (flt(ZERO, value)) {
    obj.status_source = source;
    w.log("status_source", obj.tag, tag(source));
  }
}
Raw proc_amount(World &w, std::int32_t low, std::int32_t high) {
  const auto divisor = s32(std::uint32_t(high) - std::uint32_t(low) + 1u);
  const auto draw = w.seed.draw();
  return itof(std::int64_t(low) +
              c_rem(static_cast<std::int32_t>(draw), divisor));
}
void fx(World &w, std::uint32_t name, const Obj &target) {
  w.log("fx", name, target.tag);
}
Raw bloodlust_armor_factor(World &w, Spell &spell) {
  if (!spell.f2c)
    return ONE;
  return fsub(ONE, fmul(itof(w.stat(spell.skill, 2, s8(spell.level))), K_001));
}
bool spirit_shield_absorbs(World &w, Spell &spell) {
  const auto limit =
      w.stat_for(spell.skill, 0, s8(spell.level), spell.owner, 10);
  const auto chance =
      w.stat_for(spell.skill, 1, s8(spell.level), spell.owner, 10);
  if (flt(itof(limit), spell.f1c))
    return false;
  if (!(std::int64_t(w.seed.draw() % 100) < chance))
    return false;
  if (!spell.f18) {
    spell.f18 = 1;
    spell.f20 = fadd(spell.f1c, 0x3e449b62);
  }
  w.log("fx_at", FX_SPIRIT_SHIELD);
  return true;
}
Raw frost_armor_on_hit(World &w, Spell &spell, Raw damage, Obj *attacker) {
  const auto level = s8(spell.level);
  Raw factor =
      fsub(ONE, fmul(itof(w.stat_for(spell.skill, 1, level, spell.owner, 0)),
                     K_001));
  if (flt(factor, ZERO))
    factor = ZERO;
  if (!attacker)
    return factor;
  const auto roll = w.seed.draw() % 100;
  if (!(std::int64_t(roll) < w.stat_for(spell.skill, 2, level, spell.owner, 0)))
    return factor;
  const auto amount = s16(ftoi(fsub(damage, factor)));
  Descriptor desc = EMPTY_DESC;
  desc[0] = {0x16, amount, amount, 0};
  damage_apply(w, *attacker, spell.owner, ZERO, ZERO, 100000, 0x540, &desc, 0);
  w.log("fx_at", FX_FROST_ARMOR);
  w.log("fx_spawn", 10);
  return factor;
}
void thorns(World &w, std::int64_t skill, Obj &victim, Obj &attacker,
            std::int64_t level) {
  if (!level)
    return;
  const auto damage = w.stat_for(skill, 0, level, &victim, 10);
  const auto chance = w.stat_for(skill, 1, level, &victim, 10);
  if (!(ftoi(fmul(fmul(itof(w.seed.draw()), K_1_65536), 0x42c80000)) < chance))
    return;
  victim.flash = {0x82, 0x82, 0x83, 0x55, 0x82, 0x82, 0x83, 0x55};
  if (!victim.model_points)
    return;
  w.log("thorns", skill, victim.tag);
  for (unsigned i = 0; i < 200; ++i) {
    w.cosmetic.draw();
    w.cosmetic.in_unit_sphere();
  }
  const auto low = itof(damage);
  const auto high = fadd(
      low, fmul(fmul(low, fmul(itof(w.seed.draw()), K_1_65536)), 0x3e4ccccc));
  damage_apply(w, attacker, &victim, low, high, 100000, 0x40, nullptr, 1);
}
Raw crit_bonus(World &w, std::int64_t skill, Obj &attacker, std::int64_t rank) {
  std::int64_t use = rank;
  if (!rank || !attacker.spell(U"CriticalHit")) {
    use = 0;
    // The list itself is not copied: callbacks can mutate later entries.
    for (std::size_t i = 0; i < w.alive.size(); ++i) {
      const auto slot = w.alive[i];
      if (slot == attacker.slot)
        continue;
      auto &other = required(at(w.players, slot).obj);
      const auto dx = fsub(at(other.pos, 0), at(attacker.pos, 0));
      const auto dy = fsub(at(other.pos, 1), at(attacker.pos, 1));
      const auto dz = fsub(at(other.pos, 2), at(attacker.pos, 2));
      if (!flt(fadd(fadd(fmul(dx, dx), fmul(dy, dy)), fmul(dz, dz)),
               0x49742400))
        continue;
      if (w.pvp_a && !w.enemy_fn(&other, &attacker))
        continue;
      if (!other.spell(U"CriticalHit"))
        continue;
      auto &rec = at(w.players, slot);
      auto best = rec.rank(SKILL_CRIT_B);
      best = std::max(best, rec.rank(SKILL_CRIT_C));
      best = std::max(best, rec.rank(SKILL_CRIT_A));
      if (!best)
        continue;
      const auto statistic = w.stat(skill, 1, best);
      const auto reach = itof(s32(std::uint32_t(statistic) * 12u));
      const auto x = fsub(at(other.pos, 0), at(attacker.pos, 0)),
                 y = fsub(at(other.pos, 1), at(attacker.pos, 1));
      if (flt(fadd(fmul(x, x), fmul(y, y)), fmul(reach, reach))) {
        if (best > 0)
          use = best;
        break;
      }
    }
  }
  const auto level = w.adjust(skill, use, 0x14);
  return !level ? ZERO : fmul(itof(w.stat(skill, 0, level)), K_001);
}
std::int32_t stun_chance(World &w, Obj &victim, std::int64_t rank) {
  if (!(victim.flags & 0x20) || w.pvp_a)
    return 0;
  const auto chance = w.stat(SKILL_STUN, 0, rank);
  if (victim.boss_rank >= 2)
    return 0;
  return victim.boss_rank <= 0 ? chance : std::min(chance, 5);
}
void stun_effect(World &w, Obj &victim) {
  if (!(victim.flags & 0x20))
    return;
  w.log("fx_at", FX_STUN);
  if (w.pool(0xbc)) {
    w.seed.draw();
    w.log("light");
  }
  const auto frames =
      ftoi(fmul(fadd(fmul(itof(w.seed.draw()), K_1_65536), 0x3fc00000), SIXTY));
  w.log("stun", victim.tag, frames);
}
Raw retribution(World &w, Spell &spell, Raw damage) {
  auto &owner = required(spell.owner);
  owner.flash = {0x46, 0x80, 0x4b, 0x3f, at(owner.flash, 4), 0x80, 0x4b, 0x3f};
  const auto fraction =
      fmul(itof(w.stat(spell.skill, 1, s8(spell.level))), K_001);
  if (owner.flags & 0x40) {
    auto &rec = at(w.players, owner.slot);
    if (!flt(ZERO, rec.hp))
      return ZERO;
    rec.hp = fadd(rec.hp, fmul(damage, fraction));
    if (flt(rec.max_hp, rec.hp))
      rec.hp = rec.max_hp;
  }
  return fraction;
}
int colour_class(const Obj &victim, std::uint32_t flags) {
  bool immune = false, resisted = false;
  const auto grade = [&](Raw value) {
    if (fle(ONE, value))
      immune = true;
    else if (flt(ZERO, value))
      resisted = true;
  };
  const auto cf = victim.cflags;
  if (flags & 0x100)
    grade(at(victim.res, 4));
  if (flags & 4) {
    resisted |= bool(cf & 2);
    immune |= bool(cf & 4);
  }
  if (flags & 8) {
    immune |= bool(cf & 0x10);
    resisted |= bool(cf & 8);
  }
  if (flags & 1) {
    immune |= bool(cf & 0x40);
    resisted |= bool(cf & 0x20);
  }
  grade(at(victim.res, 1));
  grade(at(victim.res, 0));
  resisted |= bool(cf & 0x80);
  immune |= bool(cf & 0x100);
  grade(at(victim.res, 4));
  grade(at(victim.res, 2));
  grade(at(victim.res, 3));
  return immune ? 2 : int(resisted);
}
bool facing_front(const Obj &victim, const Obj &attacker) {
  const auto c = cos16(victim.facing), s = sin16(victim.facing);
  const auto dx = fsub(at(attacker.pos, 0), at(victim.pos, 0)),
             dy = fsub(at(attacker.pos, 1), at(victim.pos, 1));
  return flt(ZERO, fadd(fmul(c, dx), fmul(s, dy)));
}
int damage_apply(World &w, Obj &victim, Obj *attacker, Raw dmin, Raw dmax,
                 std::int64_t attack, std::uint32_t flags,
                 const Descriptor *descriptor, std::int64_t no_crit) {
  DamageDepth depth;
  if (attacker && (attacker->flags & 0x20000))
    return 0;
  const Descriptor &desc = descriptor ? *descriptor : EMPTY_DESC;
  if (!w.cutscene) {
    const auto dy = fsub(at(w.focus, 1), at(victim.pos, 1)),
               dx = fsub(at(w.focus, 0), at(victim.pos, 0));
    if (flt(fsqrt(fadd(fmul(dx, dx), fmul(dy, dy))), 0x43aa0000)) {
      auto &entry = at(w.music, w.music_index);
      entry = (std::uint64_t(entry) + 1) & 255;
    }
  }
  if (victim.cls == U"Innoruuk" && victim.innoruuk_immune)
    return 0;
  if (!(victim.flags & 0x20)) {
    const auto amount = base_roll(dmin, dmax, w.seed.draw());
    w.take_damage(victim, amount, attacker, flags);
    return 1;
  }
  const auto cf = victim.cflags;
  if (cf & CF_INVULNERABLE)
    return 0;
  if ((cf & CF_ABSOLUTE) && (cf & 0x50) && (flags & 9))
    return 0;
  if ((flags & 0x8000000) && s8(victim.kind) != 0x10)
    return 0;
  if (attacker && (attacker->flags & 0x80) && !(victim.flags & 0x40) &&
      attacker->owner_link && victim.cls == required(attacker->owner).cls)
    return 1;
  if (attacker && !(victim.flags & 0x40) && !(attacker->flags & 0x40))
    flags |= 0x1000000;
  auto armor = s16(victim.armor);
  if (auto *spell = victim.spell(U"BloodLust")) {
    const auto factor = bloodlust_armor_factor(w, *spell);
    armor = ftoi(fmul(itof(armor), factor));
    if (armor <= 0)
      armor = 1;
  }
  const bool attacker_is_player = attacker && (attacker->flags & 0x40);
  const int sound_a0 = attacker_is_player ? 0 : 1;
  const auto sound_slot = attacker_is_player ? attacker->slot : 0;
  if ((victim.flags & 0x40) || w.pvp_a) {
    const auto cut = fmul(itof(s16(victim.armor)), K_1_30);
    auto low = fsub(dmin, cut), high = fsub(dmax, cut);
    if (flt(low, fmul(dmin, HALF)))
      low = fmul(dmin, HALF);
    if (flt(high, fmul(dmax, HALF)))
      high = fmul(dmax, HALF);
    if (flt(low, ZERO))
      low = ZERO;
    if (flt(high, ZERO))
      high = ZERO;
    dmin = low;
    dmax = high;
  }
  Raw dmg = dmin;
  if (!fle(dmax, dmin))
    dmg = base_roll(dmin, dmax, w.seed.draw());
  if (!(flags & 0x40)) {
    if ((victim.cflags & CF_BLOCKING) && attacker &&
        facing_front(victim, *attacker)) {
      fx(w, FX_BLOCK, victim);
      for (unsigned i = 0; i < 50; ++i)
        w.cosmetic.in_unit_sphere();
      w.log("sparks", 50);
      if (!(flags & 0x20000))
        return 0;
      dmg = fmul(dmg, HALF);
    }
    auto *spell = victim.spell(U"SpiritShield");
    if (spell && spirit_shield_absorbs(w, *spell)) {
      if (!(flags & 0x20000))
        return 0;
      dmg = fmul(dmg, HALF);
    }
  }
  const auto p = armor > 0 ? fsqrt(fdiv(itof(attack), itof(armor))) : ONE;
  auto crit_chance = fmul(itof(desc_get(desc, 0x1d, 1)), K_001);
  const auto threshold = fmul(p, K_65536);
  const bool hit = flt(itof(w.seed.draw()), threshold);
  if (s32(victim.counter12) > 0 && hit && attacker &&
      (attacker->flags & 0x20) && flt(ZERO, dmg) && attacker != &victim &&
      s8(attacker->kind) == 0x10) {
    damage_apply(w, *attacker, &victim, dmg, dmg, 100000000, 0x100, &desc, 0);
    attacker->counter11 = 0x11;
    attacker->stamp_f0 = w.frame;
  }
  if (auto *spell = victim.spell(U"FrostArmor")) {
    const auto factor = frost_armor_on_hit(w, *spell, dmg, attacker);
    dmg = fmul(dmg, factor);
  }
  if (armor > 0)
    dmg = fmul(dmg, fmin(fdiv(itof(attack), fmul(itof(armor), HALF)), ONE));
  if (!(flags & 0x40000000) && attacker_is_player && !no_crit) {
    auto &rec = at(w.players, attacker->slot);
    const auto b1 =
        crit_bonus(w, SKILL_CRIT_A, *attacker, rec.rank(SKILL_CRIT_A));
    const auto b2 =
        crit_bonus(w, SKILL_CRIT_B, *attacker, rec.rank(SKILL_CRIT_B));
    const auto b3 =
        crit_bonus(w, SKILL_CRIT_C, *attacker, rec.rank(SKILL_CRIT_C));
    crit_chance = fadd(crit_chance, fmax(b3, fmax(b1, b2)));
  }
  int crit = 0;
  if (flt(ZERO, crit_chance) && hit && !no_crit &&
      flt(itof(w.seed.draw()), fmul(crit_chance, K_65536))) {
    crit = 1;
    if (attacker_is_player)
      w.log("voice", attacker->slot, 10);
    dmg = fmul(dmg, itof(3));
  }
  Obj *source = attacker && (attacker->flags & 0x20) ? attacker : nullptr;
  const auto chance_roll = [&](int kind) {
    const auto draw = w.seed.draw();
    return flt(itof(draw),
               fmul(fmul(itof(desc_get(desc, kind, 1)), K_001), K_65536));
  };
  const auto reduced = [](Raw amount, Raw resistance) {
    return fsub(amount, fmul(amount, resistance));
  };
  const auto cold_time = [&](Raw amount) {
    auto t = at(victim.status, 1);
    t = feq(victim.hp, ZERO) ? fadd(t, SIXTY)
                             : fadd(t, fdiv(fmul(amount, SIXTY), victim.hp));
    return flt(SIXTY, t) ? SIXTY : t;
  };
  if (hit) {
    if (source) {
      const auto chance = desc_get(desc, 0x29, 2);
      if (chance &&
          std::int64_t(w.seed.draw()) < s32(std::uint32_t(chance) * 0x28fu))
        w.log("cast", desc_get(desc, 0x29, 1), desc_get(desc, 0x29, 3),
              source->tag, s16(source->facing));
    }
    if (chance_roll(0x1f)) {
      const auto before = at(victim.status, 0);
      set_status(w, victim, 0, fsub(ONE, at(victim.res, 0)), source);
      if (fle(before, ZERO) && flt(ZERO, at(victim.status, 0)))
        fx(w, FX_FIRE, victim);
      const auto amount =
          proc_amount(w, desc_get(desc, 0x1f, 2), desc_get(desc, 0x1f, 3));
      dmg = fadd(dmg, reduced(amount, at(victim.res, 0)));
    }
    if (chance_roll(0x20)) {
      fx(w, FX_COLD_PROC, victim);
      const auto amount =
          proc_amount(w, desc_get(desc, 0x20, 2), desc_get(desc, 0x20, 3));
      dmg = fadd(dmg, reduced(amount, at(victim.res, 1)));
      set_status(w, victim, 1, cold_time(dmg), source);
    }
    if (chance_roll(0x21)) {
      fx(w, FX_POISON, victim);
      const auto strength = desc_get(desc, 0x21, 2);
      if (w.pool(0x70)) {
        w.seed.draw();
        w.log("gas", strength * 30 + 30, strength * 25 + 25, tag(attacker));
      }
    }
    if (chance_roll(0x22)) {
      set_status(w, victim, 2, TWO, source);
      fx(w, FX_LIGHTNING, victim);
      const auto amount =
          proc_amount(w, desc_get(desc, 0x22, 2), desc_get(desc, 0x22, 3));
      dmg = fadd(dmg, reduced(amount, at(victim.res, 4)));
    }
  }
  const auto percent = desc_get(desc, 0x1b, 1);
  if (percent && s8(victim.kind) == 0x10)
    dmg = fadd(fmul(fmul(itof(percent), K_001), dmg), dmg);
  if (desc_get(desc, 0x15, 1)) {
    const auto resistance = at(victim.res, 0), before = at(victim.status, 0);
    const auto amount =
        proc_amount(w, desc_get(desc, 0x15, 1), desc_get(desc, 0x15, 2));
    dmg = fadd(dmg, reduced(amount, resistance));
    if (!(flags & 0x10000000))
      set_status(w, victim, 0, fsub(TWO, fadd(resistance, resistance)), source);
    if (fle(before, ZERO) && flt(ZERO, at(victim.status, 0)))
      fx(w, FX_FIRE, victim);
  }
  if (desc_get(desc, 0x16, 1)) {
    auto amount =
        proc_amount(w, desc_get(desc, 0x16, 1), desc_get(desc, 0x16, 2));
    amount = reduced(amount, at(victim.res, 1));
    w.log("fx_at", FX_COLD);
    dmg = fadd(dmg, amount);
    set_status(w, victim, 1, cold_time(amount), source);
  }
  if (desc_get(desc, 0x17, 1)) {
    fx(w, FX_POISON, victim);
    set_status(w, victim, 3, fsub(FIVE, fmul(at(victim.res, 2), FIVE)), source);
    victim.poison_a = desc_get(desc, 0x17, 1);
    victim.poison_b = desc_get(desc, 0x17, 2);
  }
  if (desc_get(desc, 0x26, 1)) {
    const bool late = flags & 0xc000;
    if (!late)
      fx(w, FX_DISEASE, victim);
    if (!victim.spell(U"DiseaseEffect")) {
      if (late)
        fx(w, FX_DISEASE, victim);
      const auto made = w.pool(0x1b0);
      if (made) {
        w.seed.draw();
        const auto strength = ftoi(fsub(FIVE, fmul(at(victim.res, 3), FIVE)));
        Spell spell;
        spell.kind = U"DiseaseEffect";
        spell.skill = SKILL_DISEASE;
        spell.level = 0;
        spell.owner = &victim;
        victim.spells.push_back(w.add_owned_spell(std::move(spell)));
        w.log("disease", strength, victim.tag, tag(attacker));
      }
      w.log("add_spell", victim.tag, made);
    }
  }
  if (desc_get(desc, 0x18, 1)) {
    const auto amount =
        proc_amount(w, desc_get(desc, 0x18, 1), desc_get(desc, 0x18, 2));
    dmg = fadd(dmg, reduced(amount, at(victim.res, 4)));
    fx(w, FX_LIGHTNING, victim);
    set_status(w, victim, 2,
               fsub(TWO, fadd(at(victim.res, 4), at(victim.res, 4))), source);
  }
  if (desc_get(desc, 0x19, 1) && s8(victim.kind) == 0x12) {
    set_status(w, victim, 2, TWO, source);
    fx(w, FX_LIGHTNING, victim);
    const auto amount =
        proc_amount(w, desc_get(desc, 0x19, 1), desc_get(desc, 0x19, 2));
    dmg = fadd(dmg, reduced(amount, at(victim.res, 4)));
  }
  if (desc_get(desc, 0x1a, 1) && s8(victim.kind) == 0x12 &&
      !(desc_get(desc, 0x1a, 1) < std::int64_t(w.seed.draw() % 100))) {
    flags |= 0x800;
    fx(w, FX_LIGHTNING, victim);
    dmg = fadd(dmg, victim.hp);
  }
  if (desc_get(desc, 0x1c, 1) && s8(victim.kind) == 0x10 &&
      !(desc_get(desc, 0x1c, 1) < std::int64_t(w.seed.draw() % 100))) {
    flags |= 0x2000000;
    fx(w, FX_UNDEAD_KILL, victim);
    dmg = fadd(dmg, victim.hp);
  }
  if (!crit && !flt(itof(w.seed.draw()), threshold)) {
    if (!(flags & 0x8000))
      w.log("hit_sound", dmg, sound_a0, sound_slot, flags, 0);
    w.take_damage(victim, ZERO, attacker,
                  victim.flags & 0x40 ? 0x2000 : 0x42000);
    return 0;
  }
  const auto colour = colour_class(victim, flags);
  if (attacker && (flags & 0xd)) {
    if ((victim.flags & 0x20) && (attacker->flags & 0x20)) {
      if (victim.flags & 0x40) {
        auto &rec = at(w.players, victim.slot);
        if (rec.rank(SKILL_RIPOSTE))
          thorns(w, SKILL_RIPOSTE, victim, *attacker, rec.rank(SKILL_RIPOSTE));
        if (rec.rank(SKILL_RIPOSTE_B))
          thorns(w, SKILL_RIPOSTE_B, victim, *attacker,
                 rec.rank(SKILL_RIPOSTE_B));
      } else if (victim.cflags & CF_THORNS)
        thorns(w, SKILL_RIPOSTE, victim, *attacker, ftoi(w.monster_level));
    }
    if ((victim.flags & 0x40) && (attacker->flags & 0x20) &&
        s8(attacker->kind) == 0x10) {
      auto *first = victim.spell(U"UndeadShield");
      auto *second = victim.spell(U"SecondaryUndeadShield");
      if (first)
        w.log("undead_shield", 1, victim.tag, attacker->tag, ftoi(dmg));
      else if (second)
        w.log("undead_shield", 2, victim.tag, attacker->tag, ftoi(dmg));
    }
  }
  const int victim_is_player = (victim.flags & 0x40) ? 1 : 0;
  if (w.pvp_a && victim_is_player)
    dmg = fmul(dmg, HALF);
  auto number = ftoi(fadd(dmg, HALF));
  if (flags & 0x800000) {
    const auto total = fadd(victim.dot, dmg);
    dmg = ZERO;
    victim.dot = total;
    if (!(w.frame & 31)) {
      if (fle(total, ONE))
        victim.dot = ONE;
      number = ftoi(fadd(victim.dot, HALF));
      w.take_damage(victim, itof(ftoi(victim.dot)), attacker, flags);
      victim.dot = ZERO;
    }
  }
  if (!(flags & 0x4000))
    w.log("hit_sound", dmg, victim_is_player, sound_slot, flags, 1);
  w.take_damage(victim, dmg, attacker, flags);
  Raw share = ZERO;
  if (attacker_is_player) {
    const auto rank = at(w.players, attacker->slot).rank(SKILL_STUN);
    const auto chance = stun_chance(w, victim, rank);
    if (std::int64_t(w.seed.draw() % 100) < chance)
      stun_effect(w, victim);
    if (auto *spell = attacker->spell(U"Retribution"))
      share = retribution(w, *spell, dmg);
  }
  const bool blocked_text = w.pvp_a && attacker && (victim.flags & 0x40) &&
                            (victim.state == 8 || victim.state == 0xb) &&
                            !(flags & 0x20040) &&
                            facing_front(victim, *attacker);
  if (victim_is_player &&
      !(w.pvp_a && attacker_is_player && attacker != &victim))
    return 1;
  if (!w.numbers_option && !w.network_mode)
    return 1;
  if (victim.flags & 8)
    return 1;
  if ((flags & 0x200) && (victim.cflags & 0x100))
    return 1;
  if (w.cutscene || blocked_text)
    return 1;
  if (flags & 0x800000) {
    if (w.frame & 31)
      return 1;
  } else if (flags & 0x1000000)
    return 1;
  if (w.pool(0x98)) {
    w.seed.draw();
    w.log("text", number, crit, victim_is_player, colour, 0);
  }
  if (feq(share, ZERO))
    return 1;
  number = ftoi(fadd(fmul(dmg, share), HALF));
  if (w.pool(0x98)) {
    w.seed.draw();
    w.log("text", number, crit, victim_is_player, colour, 2);
  }
  return 1;
}
} // namespace champions::simulation::rta::combat
