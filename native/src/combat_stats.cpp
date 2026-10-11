#include "champions/combat_stats.hpp"
#include "champions/combat_unicode_data.hpp"
#include <algorithm>
#include <limits>
namespace champions::simulation::rta::combat {
namespace {
template <class C> decltype(auto) ix(C &c, std::int64_t i) {
  if (i < 0)
    i += static_cast<std::int64_t>(c.size());
  if (i < 0 || static_cast<std::uint64_t>(i) >= c.size())
    throw Error("index out of range");
  return c[static_cast<std::size_t>(i)];
}
const ItemDef &def(const Item &item) {
  if (!item.defn)
    throw Error("missing item definition");
  return *item.defn;
}
Raw fi(std::int64_t v) { return itof(static_cast<std::uint32_t>(v)); }
std::int32_t wrap(std::int64_t v) { return s32(static_cast<std::uint32_t>(v)); }
int context(char32_t cp) {
  auto p = std::lower_bound(
      std::begin(unicode_data::context), std::end(unicode_data::context), cp,
      [](const auto &r, char32_t c) { return r.last < c; });
  return p != std::end(unicode_data::context) && p->first <= cp ? p->kind : 0;
}
} // namespace
std::u32string unicode_lower(const std::u32string &value) {
  std::u32string out;
  for (std::size_t i = 0; i < value.size(); ++i) {
    char32_t c = value[i];
    if (c > 0x10ffff)
      throw Error("invalid Unicode code point");
    if (c == 0x3a3) {
      bool before = false, after = false;
      for (std::size_t j = i; j > 0;) {
        int k = context(value[--j]);
        if (k == 1)
          continue;
        before = k == 2;
        break;
      }
      for (std::size_t j = i + 1; j < value.size(); ++j) {
        int k = context(value[j]);
        if (k == 1)
          continue;
        after = k == 2;
        break;
      }
      out.push_back(before && !after ? 0x3c2 : 0x3c3);
      continue;
    }
    auto p = std::lower_bound(
        std::begin(unicode_data::lower), std::end(unicode_data::lower), c,
        [](const auto &r, char32_t x) { return r.from < x; });
    if (p != std::end(unicode_data::lower) && p->from == c) {
      out.push_back(p->a);
      if (p->b)
        out.push_back(p->b);
    } else
      out.push_back(c);
  }
  return out;
}
std::int32_t passive_level(const Tables &t, const PlayerRec &r,
                           std::int64_t slot, std::uint64_t mask) {
  if (slot < 0 || slot > 4)
    return 0;
  const auto &rows = ix(t.passive_masks, r.cls);
  for (std::size_t i = 0; i < rows.size(); ++i)
    if (rows[i] == mask)
      return std::max(0, s8(ix(r.passives, i)));
  return 0;
}
std::int32_t boss_rank(const Tables &t, bool creature,
                       const std::optional<std::u32string> &type,
                       const std::u32string &prop, std::uint64_t flags) {
  if (!creature)
    return 0;
  if (!type)
    return flags & 1;
  auto name = unicode_lower(*type);
  for (const auto &n : t.boss_names)
    if (name == unicode_lower(n))
      return 2;
  if (name == unicode_lower(t.boss_gate))
    return 2;
  for (const auto &p : t.boss_pairs)
    if (name == unicode_lower(p.first) &&
        unicode_lower(prop) == unicode_lower(p.second))
      return 2;
  if (name == unicode_lower(ix(t.boss_last, 0)) ||
      name == unicode_lower(ix(t.boss_last, 1)))
    return 2;
  return flags & 1;
}
Raw level_scale(const Tables &t, std::int64_t byte, std::int64_t diff, bool pvp,
                const std::u32string &name) {
  Raw base = fi(s8(byte) + 2), v = pvp ? 0x40400000 : base;
  constexpr Raw offsets[] = {0x42080000, 0x42740000, 0x42b00000, 0x42e60000};
  if (diff >= 2 && diff <= 5)
    v = fadd(fmul(base, 0x3f400000), offsets[diff - 2]);
  if (name == ix(t.fixed_levels, 0))
    v = ONE;
  else if (name == ix(t.fixed_levels, 1))
    v = TWO;
  return v;
}
MonsterStats monster_stats(const Tables &t, const World &w,
                           const std::array<Raw, 5> &pct, std::int64_t rank,
                           const std::optional<std::u32string> &type, bool roll,
                           std::uint32_t draw, bool dead) {
  Raw hp = pct[0], armor = pct[1], damage = fmul(pct[2], 0x3fc51eb8),
      attack = pct[3], xp = pct[4], level = w.monster_level;
  if (rank == 1) {
    hp = fmul(hp, 0x41199999);
    damage = fmul(damage, 0x3fa66666);
    attack = fmul(attack, 0x40400000);
    xp = fmul(xp, 0x40800000);
  }
  auto n = s32(w.num_players);
  if (n >= 2 && !w.medal_mission) {
    Raw extra = fi(n - 1);
    xp = fmul(xp, fadd(extra, ONE));
    hp = fmul(hp, fadd(fmul(extra, 0x3f199999), ONE));
    if (rank != 2 && type &&
        unicode_lower(*type) != unicode_lower(t.no_scale_name))
      damage = fmul(damage, fadd(fmul(extra, 0x3e199999), ONE));
  }
  MonsterStats out;
  if (roll) {
    Raw power = powf(fmul(hp, level), 0x3f599999);
    Raw v = dbl_to_single(
        dbl_mul(dbl_add(dbl_from_int(draw % 5), UINT64_C(0x402f000000000000)),
                dbl_from_single(power)));
    bool fixed = false;
    for (const auto &name : t.fixed_levels)
      if (unicode_lower(w.level_name) == unicode_lower(name))
        fixed = true;
    if (!fixed) {
      if (w.difficulty == 2)
        v = fmul(v, 0x3fc00000);
      else if (w.difficulty == 3)
        v = fadd(v, v);
      else if (w.difficulty == 4)
        v = fmul(v, 0x40200000);
      else if (w.difficulty == 5)
        v = fmul(v, 0x40400000);
    }
    out.hp = v;
    out.max_hp = v;
  }
  out.armor = s16(ftoi(fmul(fmul(armor, 0x41600000), level)));
  out.dmg_low = s16(ftoi(fmul(damage, powf(level, ONE))));
  out.dmg_high = s16(ftoi(fmul(fadd(damage, damage), powf(level, ONE))));
  if (out.dmg_high < out.dmg_low + 1)
    out.dmg_high = s16(out.dmg_low + 1);
  out.attack = s16(ftoi(fmul(fmul(level, 0x41600000), attack)));
  out.exp_value = fi(ftoi(fmul(powf(fmul(xp, level), 0x3fa9374b), 0x41000000)));
  if (dead)
    out.hp = ZERO;
  return out;
}
std::int64_t exp_cap(const Tables &t, std::int64_t level) {
  level = s8(level);
  return level < 80 ? s32(std::uint32_t(ix(t.exp_table, level + 1)) -
                          std::uint32_t(ix(t.exp_table, level)))
                    : ix(t.exp_table, level);
}
std::int64_t item_armor_value(const Item &item) {
  std::int64_t base = s16(def(item).armor);
  auto p = item.mod(0xd);
  if (p)
    base += ftoi(fmul(fi(base), fmul(fi(p), K_001)));
  return base;
}
std::int32_t item_attack_rating(const Item &item) {
  Raw base = fi(s16(def(item).attack));
  auto p = item.mod(0x23);
  if (p)
    base = fadd(fmul(fmul(fi(p), K_001), base), base);
  return ftoi(base);
}
std::pair<std::int32_t, std::int32_t> item_damage_range(const Item &item,
                                                        bool elements) {
  Raw high = fi(s16(def(item).dmg_max)), low = fi(s16(def(item).dmg_min));
  auto p = item.mod(0x23);
  if (p) {
    Raw f = fadd(fmul(fi(p), K_001), ONE);
    high = fmul(high, f);
    low = fmul(low, f);
  }
  if (elements)
    for (int code : {0x15, 0x16, 0x18, 0x17, 0x26})
      if (item.mod(code)) {
        low = fadd(low, fi(item.mod(code, 1)));
        high = fadd(high, fi(item.mod(code, 2)));
      }
  return {ftoi(fadd(fmul(low, ONE), HALF)), ftoi(fadd(fmul(high, ONE), HALF))};
}
std::int32_t recalc_armor(const Tables &t, World &w, std::int64_t slot) {
  auto &r = ix(w.players, slot);
  auto *obj = r.obj;
  if (!obj)
    return 0;
  obj->res.assign(5, ZERO);
  std::int64_t armor = 0;
  auto *weapon = ix(r.weapon_sets, s8(r.active_set));
  for (std::size_t j = 0; j < std::min<std::size_t>(64, r.bag2.size()); ++j) {
    auto *item = r.bag2[j];
    if (!item || !item->defn || !(item->defn->flags & (UINT64_C(1) << 45)))
      continue;
    const auto &name = item->defn->name;
    int stat = -1, tier = -1;
    for (std::size_t i = 0; i < t.figurine_stats.size(); ++i)
      if (name.find(t.figurine_stats[i]) != std::u32string::npos) {
        stat = static_cast<int>(i);
        break;
      }
    if (stat < 0)
      continue;
    for (std::size_t i = 0; i < t.figurine_tiers.size(); ++i)
      if (name.find(t.figurine_tiers[i]) != std::u32string::npos) {
        tier = static_cast<int>(i);
        break;
      }
    constexpr std::array<int, 5> amounts{5, 10, 20, 25, 15};
    auto amount = ix(amounts, tier); // Python -1 selects the fallback.
    ix(r.eff, stat) = s32(std::uint32_t(ix(r.eff, stat)) +
                          std::uint32_t(s16(item->count) * amount));
  }
  for (int index = 0; index < 17; ++index) {
    if (weapon && index == 3 && s8(def(*weapon).subtype) == 4)
      continue;
    auto *item = ix(r.equip, index);
    if (!item)
      continue;
    if (s8(def(*item).category) == 1)
      armor += item_armor_value(*item);
    for (int code = 0; code < 5; ++code)
      obj->res[code] = fadd(obj->res[code], fmul(fi(item->mod(code)), K_001));
    Raw every = fmul(fi(item->mod(0x2a)), K_001);
    for (auto &res : obj->res)
      res = fadd(res, every);
    if (s8(def(*item).category) != 1)
      armor += item->mod(0xd);
    r.max_hp = fadd(r.max_hp, fi(item->mod(5)));
    r.max_mana = fadd(r.max_mana, fi(item->mod(6)));
  }
  Raw bonus = fmul(fi(w.stat(0x3f, 0, w.adjust(0x3f, r.rank(0x3f), 1))), K_001);
  for (auto &res : obj->res) {
    res = fadd(res, bonus);
    if (flt(ONE, res))
      res = ONE;
  }
  return wrap(armor + s32(ix(r.eff, 2)) / 2);
}
void recalc_stats(const Tables &t, World &w) {
  const auto player_count = s32(w.num_players);
  for (std::int64_t slot = 0; slot < player_count; ++slot) {
    auto &r = ix(w.players, slot);
    r.eff = r.base;
    for (auto &v : r.eff)
      v = s32(v);
    auto *obj = r.obj;
    if (!obj)
      continue;
    r.stagger = ZERO;
    for (auto *item : r.equip) {
      if (!item)
        continue;
      for (int i = 0; i < 4; ++i)
        ix(r.eff, i) =
            s32(std::uint32_t(ix(r.eff, i)) + std::uint32_t(item->mod(7 + i)));
      r.stagger = fadd(r.stagger, fmul(fi(item->mod(0x1e)), K_001));
    }
    for (int i = 0; i < 4; ++i) {
      auto v =
          (std::uint32_t(ix(r.eff, i)) - (i == 0 || i == 2 ? 30u : 5u)) & 255u;
      ix(obj->bonus, i) = s8(v) < 0 ? 0 : v;
    }
    auto formula = (r.cls >= 0 &&
                    static_cast<std::size_t>(r.cls) < t.class_formulas.size())
                       ? t.class_formulas[r.cls]
                       : t.class_default;
    obj->armor = s16(recalc_armor(t, w, slot));
    Raw level = fi(s8(r.level59));
    r.load_limit = fi(std::uint32_t(ix(r.eff, 0)) + 125u);
    r.max_mana = fadd(fmul(fsub(fi(ix(r.eff, 1)), formula[0]), formula[1]),
                      fmul(level, formula[2]));
    r.max_hp =
        fsub(fadd(fmul(level, formula[3]), fmul(fi(ix(r.eff, 3)), formula[4])),
             formula[5]);
    r.load_limit =
        fadd(r.load_limit,
             fmul(fi(passive_level(t, r, obj->slot, 0x40)), 0x41700000));
    if (w.load_cheat)
      r.load_limit = fadd(r.load_limit, 0x459c4000);
    if (flt(0x461c3c00, r.load_limit))
      r.load_limit = 0x461c3c00;
    if (flt(r.max_mana, r.mana))
      r.mana = r.max_mana;
    if (flt(r.max_hp, r.hp))
      r.hp = r.max_hp;
  }
}
std::int32_t give_exp(const Tables &t, World &w, std::int64_t slot,
                      std::int64_t amount,
                      const std::function<void(std::int64_t)> &callback) {
  auto &r = ix(w.players, slot);
  r.exp = s32(std::uint32_t(r.exp) + std::uint32_t(amount));
  auto ceiling = s32(std::uint32_t(ix(t.exp_table, 80)) - 1u);
  if (ceiling < r.exp)
    r.exp = ceiling;
  std::int32_t gained = 0;
  while (ix(t.exp_table, s8(r.level59)) <= r.exp) {
    if (gained == MAX_LEVEL_TRANSITIONS)
      throw Unsupported("level transition budget exhausted");
    auto level = s8(r.level59);
    auto points = level < 6    ? 1
                  : level < 16 ? 2
                  : level < 26 ? 3
                  : level < 36 ? 4
                               : 5;
    if (r.skill_points > std::numeric_limits<std::int64_t>::max() - points)
      throw Unsupported("skill point sum exceeds int64");
    r.skill_points += points;
    r.stat_points = s8(std::uint32_t(r.stat_points) + 3u);
    r.level59 = s8(level + 1);
    recalc_stats(t, w);
    if (flt(ZERO, r.hp)) {
      r.hp = r.max_hp;
      r.mana = r.max_mana;
    }
    w.log("level_up", slot, r.level59);
    if (callback)
      callback(slot);
    ++gained;
  }
  return gained;
}
std::vector<std::pair<std::int32_t, std::int64_t>>
kill_experience(const Tables &t, World &w, const Obj &creature,
                std::int64_t killer, std::uint32_t draw) {
  auto n = s32(w.num_players);
  std::int64_t total = 0;
  for (int i = 0; i < n; ++i)
    total += s8(ix(w.players, i).level59);
  auto raw = ftoi(powf(fdiv(creature.exp_value, fi(total)), 0x3fb33333));
  if (killer != -1) {
    auto rank = ix(w.players, killer).rank(0x3b);
    Raw chance = fmul(fi(w.stat(0x3b, 0, w.adjust(0x3b, rank, 1))), K_001);
    auto low = s32(draw) % 65536;
    if (flt(fi(low), fmul(chance, K_65536)) && w.pool(0xb8)) {
      w.draw_shared();
      w.log("orb", creature.tag);
    }
  }
  std::int64_t xp = ftoi(fmul(fi(raw), 0x3f400000));
  std::vector<std::pair<std::int32_t, std::int64_t>> awards;
  auto mana = [&](PlayerRec &r) {
    r.mana = fadd(r.mana, fmul(creature.max_hp, K_1_30));
    if (flt(r.max_mana, r.mana))
      r.mana = r.max_mana;
  };
  if (n < 2) {
    for (int i = 0; i < n; ++i) {
      auto &r = ix(w.players, i);
      if (!flt(ZERO, r.hp))
        continue;
      xp = std::min(xp, exp_cap(t, r.level59));
      awards.emplace_back(i, xp);
      give_exp(t, w, i, xp);
      if (r.cls == 0)
        mana(r);
    }
    return awards;
  }
  Raw whole = fi(xp);
  for (int i = 0; i < n; ++i) {
    auto &r = ix(w.players, i);
    Raw share = killer == -1 ? HALF : killer == i ? 0x3f199999 : 0x3ecccccc;
    std::int64_t amount = ftoi(fmul(whole, share));
    if (fle(r.hp, ZERO))
      amount = ftoi(fmul(fi(amount), HALF));
    amount = std::min(amount, exp_cap(t, r.level59));
    awards.emplace_back(i, amount);
    give_exp(t, w, i, amount);
    if (r.cls == 0 && killer == i)
      mana(r);
  }
  return awards;
}
std::int32_t level_for_exp(const Tables &t, std::int64_t exp) {
  int level = 1;
  while (level < 80 && ix(t.exp_table, level) <= exp)
    ++level;
  return level;
}
std::array<std::int32_t, 3> recalc_damage(const Tables &t, World &w,
                                          Obj &player, const Item *weapon,
                                          bool elements) {
  auto &r = ix(w.players, player.slot);
  auto strength = s32(ix(r.eff, 0)), dex = s32(ix(r.eff, 2));
  std::int64_t bs = strength / 4, bd = dex / 4;
  std::int64_t attack = s8(r.level58) * 20, low, high;
  if (!weapon) {
    low = bs + 1;
    high = bs + 2;
    attack += bs;
  } else {
    const auto &d = def(*weapon);
    auto range = item_damage_range(*weapon, elements);
    low = range.first;
    high = range.second;
    attack += item_attack_rating(*weapon);
    auto sub = s8(d.subtype);
    auto bonus = sub == 4 || sub == 6 ? bd : bs;
    low += bonus;
    high += bonus;
    attack += bonus;
    Raw f1 = ONE, f8 = ONE;
    if (r.rank(0x3c))
      f1 = fadd(fmul(fi(r.rank(0x3c)), 0x3d4ccccc), 0x3f933333);
    if (r.rank(0x3d))
      f8 = fadd(fmul(fi(r.rank(0x3d)), 0x3d4ccccc), 0x3f933333);
    if (s8(d.subtype) == 4) {
      Raw f =
          fmul(fi(w.stat(0x30, 0, w.adjust(0x30, r.rank(0x30), 20))), K_001);
      low = wrap(low + round_half_away(fmul(fi(low), f)));
      high = wrap(high + round_half_away(fmul(fi(high), f)));
    }
    if (s8(d.subtype) == 6) {
      Raw f = fmul(fi(w.stat(0x4f, 0, w.adjust(0x4f, r.rank(0x4f), 2))), K_001);
      low = wrap(low + round_half_away(fmul(fi(low), f)));
      high = wrap(high + round_half_away(fmul(fi(low), f)));
    }
    if (r.second_weapon && s8(d.subtype) != 6 &&
        (d.dmg_type == 1 || d.dmg_type == 8)) {
      Raw dual = fmul(
          fadd(fmul(fi(passive_level(t, r, player.slot, 0x40000)), 0x40200000),
               0x42c80000),
          K_001);
      if (d.dmg_type == 1)
        f1 = fmul(f1, dual);
      else
        f8 = fmul(f8, dual);
    }
    if (d.dmg_type == 1 || d.dmg_type == 8) {
      Raw f = d.dmg_type == 1 ? f1 : f8;
      low = ftoi(fadd(fmul(fi(low), f), HALF));
      high = ftoi(fadd(fmul(fi(high), f), HALF));
    }
    if (r.cls == 4) {
      bool wanted =
          (s8(d.subtype) == 0 && !(weapon->flags & (UINT64_C(1) << 36)) &&
           (d.dmg_type == 8 || d.dmg_type == 4)) ||
          (weapon->flags & (UINT64_C(1) << 43));
      if (wanted) {
        Raw f =
            fmul(fi(w.stat(0x5c, 0, w.adjust(0x5c, r.rank(0x5c), 1))), K_001);
        low = round_half_away(fmul(fi(low), f));
        high = round_half_away(fmul(fi(high), f));
      }
    }
  }
  Raw best = ZERO;
  for (std::size_t alive_index = 0; alive_index < w.alive.size();
       ++alive_index) {
    auto slot = w.alive[alive_index];
    auto *spell = required(ix(w.players, slot).obj).spell(U"AncestralCall");
    if (!spell)
      continue;
    auto level = s8(spell->level);
    auto reach = w.stat(spell->skill, 3, level);
    Raw factor = fmul(fi(w.stat(spell->skill, 0, level)), K_001);
    auto *owner = spell->owner;
    if (&player != owner) {
      if (w.pvp_a && !w.enemy_fn(owner, &player))
        continue;
      auto &o = required(owner);
      Raw dx = fsub(ix(player.pos, 0), ix(o.pos, 0)),
          dy = fsub(ix(player.pos, 1), ix(o.pos, 1));
      std::uint32_t reach_square =
          std::uint32_t(reach) * 12u * std::uint32_t(reach) * 12u;
      if (flt(fi(s32(reach_square)), fadd(fmul(dx, dx), fmul(dy, dy))))
        continue;
    }
    if (flt(best, factor))
      best = factor;
  }
  if (!feq(best, ZERO)) {
    low = round_half_away(fmul(fi(low), best));
    high = round_half_away(fmul(fi(high), best));
    attack = round_half_away(fmul(fi(attack), best));
  }
  Raw factor = ONE;
  if (r.cls == 5 && player.state == 1) {
    if (s32(std::uint32_t(w.frame) - std::uint32_t(player.attack_chain)) < 27) {
      auto rank = r.rank(0x47);
      if (rank > 0) {
        auto level = w.adjust(0x47, rank, 20);
        Raw rage = ONE;
        if (fle(fdiv(r.hp, r.max_hp), HALF)) {
          Raw elapsed = fmul(fi(s32(std::uint32_t(w.frame) -
                                    std::uint32_t(player.attack_start))),
                             0x3c888888);
          if (flt(ONE, elapsed))
            elapsed = ONE;
          rage =
              fadd(fmul(fmul(fi(w.stat(0x47, 1, level)), K_001), elapsed), ONE);
        }
        if (flt(factor, rage))
          factor = rage;
      }
    }
    auto *spell = player.spell(U"BloodLust");
    if (spell) {
      Raw lust = ONE;
      if (spell->f2c)
        lust = fadd(fmul(fi(w.stat(spell->skill, 1, s8(spell->level))), K_001),
                    ONE);
      if (flt(factor, lust))
        factor = lust;
    }
  }
  if (r.cls == 6 && player.state == 1) {
    auto *spell = player.spell(U"GiantGrowth");
    if (spell && flt(factor, required(spell->owner).field_dc))
      factor = spell->owner->field_dc;
  }
  low = ftoi(fmul(fi(low), factor));
  high = ftoi(fmul(fi(high), factor));
  attack = ftoi(fmul(fi(attack), factor));
  if (player.counter2) {
    low = ftoi(fmul(fi(low), 0x3fc00000));
    high = ftoi(fmul(fi(high), 0x3fc00000));
    attack = ftoi(fmul(fi(attack), 0x3fc00000));
  }
  if (player.counter4) {
    low = ftoi(fmul(fi(low), 0x3f999999));
    high = ftoi(fmul(fi(high), 0x3f999999));
    attack = ftoi(fmul(fi(attack), 0x3f999999));
  }
  r.attack = attack;
  r.dmg_min = low;
  r.dmg_max = high;
  return {static_cast<std::int32_t>(attack), static_cast<std::int32_t>(low),
          static_cast<std::int32_t>(high)};
}
} // namespace champions::simulation::rta::combat
