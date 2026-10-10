#include "champions/item_generation.hpp"
#include <algorithm>

namespace champions::simulation::rta::items {
namespace {
bool contains(const std::string &text, const std::string &stem) {
  return text.find(stem) != std::string::npos;
}
void attach_level(const Tables &t, Item &item, const std::string &name,
                  std::int32_t value) {
  if (!contains(name, t.potion_stem))
    item.set_modifier(0x0e, required_level_for_value(value));
}
std::int32_t difficulty_floor(const Game &g) {
  switch (g.difficulty) {
  case 2:
    return 0x11;
  case 3:
    return 0x1b;
  case 4:
    return 0x20;
  case 5:
    return 0x26;
  default:
    return -1;
  }
}
std::optional<Item>
pick_plain(const Tables &t, Rng &rng,
           const std::vector<std::shared_ptr<Definition>> &rows) {
  if (rows.empty())
    return std::nullopt;
  const auto &row = rows[rng.next() % rows.size()];
  auto item = parse_descriptor(t, row->name, -1, false, &rng);
  randomize_variant(item, rng);
  return item;
}
} // namespace
std::int32_t weapon_group(const Definition &d) {
  if (d.category != 0)
    return -1;
  if (d.subtype == 6)
    return 0;
  return d.subtype == 3 && (d.damage_bits & 8) ? 1 : 2;
}
std::int32_t armor_group(const Definition &d) {
  if (d.category != 1)
    return -1;
  switch (d.slot) {
  case 11:
    return 0;
  case 4:
    return 1;
  case 9:
    return 2;
  case 12:
    return 3;
  case 3:
    return 4;
  default:
    return 5;
  }
}
std::optional<Item> generate_preset(const Tables &t, const Game &g, Rng &rng,
                                    std::int32_t budget, std::int32_t category,
                                    std::int32_t level) {
  bool exclude = false;
  if (category == 1) {
    const auto threshold = ftrunc(fmul(g.cloth_fraction(), F_2_POW_16));
    exclude = threshold < static_cast<std::int32_t>(rng.next() % 0xffffu);
  }
  std::vector<std::shared_ptr<Definition>> rows;
  for (const auto &d : t.presets) {
    if (!d)
      throw Error("null supplied preset definition");
    if ((d->flags & 0x10000u) || (category != -1 && d->category != category) ||
        level < d->level || (d->flags & UINT64_C(0x800000000)))
      continue;
    if (exclude && d->category == 1 && (d->flags & 0x80000u))
      continue;
    rows.push_back(d);
  }
  if (rows.empty())
    return std::nullopt;
  std::int32_t best = -1;
  std::optional<Item> result;
  for (unsigned n = 0; n < 10; ++n) {
    const auto pick = rng.next() % rows.size();
    const auto &row = rows[pick ? pick - 1 : 0];
    auto item = parse_descriptor(t, row->name, -1, true, &rng);
    const auto value = item_value(t, item);
    attach_level(t, item, row->name, value);
    if (best <= value && value <= budget) {
      best = value;
      result = std::move(item);
    }
  }
  if (best < 0)
    return std::nullopt;
  randomize_variant(*result, rng);
  return result;
}
std::optional<Item> generate_with_affixes(const Tables &t, const Game &g,
                                          Rng &rng, std::int32_t budget,
                                          std::int32_t category,
                                          std::int32_t want_suffix,
                                          std::int32_t level) {
  bool exclude = false, prefer = false;
  if (category == 1) {
    const auto fraction = g.cloth_fraction();
    if (ftrunc(fmul(fraction, F_2_POW_16)) <
        static_cast<std::int32_t>(rng.next() % 0xffffu))
      exclude = true;
    else if (flt(F_ZERO, fraction))
      prefer = rng.next() % 0xffffu <= 0xbfffu;
  }
  std::int32_t weapon = -1, armor = -1;
  if (category == 0) {
    const auto roll = fmul(fint(rng.next()), F_2_POW_M16);
    weapon = flt(roll, 0x3d8f5c28u) ? 0 : flt(roll, 0x3df5c28fu) ? 1 : 2;
    if (weapon == 0)
      budget /= 5;
  } else if (category == 1) {
    const auto roll = fmul(fint(rng.next()), F_2_POW_M16);
    constexpr std::array<Raw, 5> limits{0x3e23d70au, 0x3ea3d70au, 0x3ef5c28fu,
                                        0x3f23d70au, 0x3f4cccccu};
    armor = 5;
    for (std::size_t i = 0; i < limits.size(); ++i)
      if (flt(roll, limits[i])) {
        armor = static_cast<std::int32_t>(i);
        break;
      }
  }
  std::vector<std::shared_ptr<Definition>> rows;
  for (const auto &d : t.defs) {
    if (!d)
      throw Error("null supplied base definition");
    if ((d->flags & 0x10000u) || (category != -1 && d->category != category) ||
        level < d->level || (d->flags & UINT64_C(0x200800000000)))
      continue;
    if (prefer && d->category == 1 && !(d->flags & 0x80000u) &&
        !contains(d->name, t.circlet_stem))
      continue;
    if (exclude && d->category == 1 && (d->flags & 0x80000u))
      continue;
    if (d->category == 0 && weapon_group(*d) != weapon)
      continue;
    if (d->category == 1 && armor_group(*d) != armor)
      continue;
    rows.push_back(d);
  }
  if (rows.empty())
    return std::nullopt;
  if (want_suffix == -1) {
    if (category == 2)
      want_suffix = 1;
    else if (rng.next() < 0x1964u)
      want_suffix = 1;
  }
  const auto prefix_count = static_cast<std::size_t>(
      std::count_if(t.prefixes.begin(), t.prefixes.end(),
                    [&](const Prefix &p) { return p.level <= level; }));
  if (std::none_of(t.suffixes.begin(), t.suffixes.end(),
                   [&](const Suffix &s) { return s.level <= level; }))
    throw Error("no suffix fits the cap; reference redraw would not terminate");
  std::int32_t best = -1;
  std::optional<Item> result;
  auto consider = [&](Item item, const std::string &name) {
    const auto value = item_value(t, item);
    attach_level(t, item, name, value);
    if (best <= value && value <= budget) {
      best = value;
      result = std::move(item);
    }
  };
  for (unsigned attempt = 0; attempt < (category == 0 ? 15u : 10u); ++attempt) {
    const auto &d = rows[rng.next() % rows.size()];
    auto suffix = rng.next() % t.suffixes.size();
    while (level < t.suffixes[suffix].level)
      suffix = rng.next() % t.suffixes.size();
    const auto suffix_text = t.suffix_text(suffix, d->category);
    const bool suffix_fits = want_suffix > 0 && suffix_text.has_value() &&
                             !contains(d->name, t.potion_stem);
    if (category == 2) {
      const auto appended =
          suffix_fits ? suffix_text : t.suffix_text(0, d->category);
      if (!appended)
        throw Error("category-2 fallback suffix is null; reference cannot "
                    "concatenate it");
      auto item = parse_descriptor(t, d->name + *appended, -1, false, &rng);
      item.prefix = 0xff;
      item.suffix = static_cast<std::uint8_t>(suffix_fits ? suffix : 0);
      consider(std::move(item), d->name);
      continue;
    }
    for (std::size_t p = 0; p < prefix_count; ++p) {
      const auto &prefix = t.prefixes[p];
      if (level < prefix.level)
        continue;
      std::string text = d->name;
      bool allowed = false;
      std::optional<std::string> prefix_text;
      if (attempt) {
        prefix_text = t.prefix_text(p, d->category);
        allowed = (d->flags & prefix.mask) != 0 || prefix.mask == 0;
        if (allowed && prefix_text)
          text += *prefix_text;
        if (suffix_fits)
          text += *suffix_text;
      }
      auto item = parse_descriptor(t, text, -1, false, &rng);
      if (attempt) {
        if (allowed && prefix_text)
          item.prefix = static_cast<std::uint8_t>(p);
        if (prefix.damage_mask)
          item.prefix = (d->damage_bits & prefix.damage_mask)
                            ? static_cast<std::uint8_t>(p)
                            : 0xff;
        if (suffix_fits)
          item.suffix = static_cast<std::uint8_t>(suffix);
      }
      consider(std::move(item), d->name);
    }
  }
  if (best < 0)
    return std::nullopt;
  randomize_variant(*result, rng);
  return result;
}
std::optional<Item>
generate_reagent(const Tables &t, const Game &g, Rng &rng, std::int32_t level,
                 const std::optional<std::string> &picture) {
  level = items::s16(static_cast<std::uint32_t>(level));
  const auto floor = difficulty_floor(g);
  std::vector<std::shared_ptr<Definition>> rows;
  for (const auto &d : t.defs) {
    if (!d)
      throw Error("null supplied base definition");
    if (d->category != 2 || !(d->flags & UINT64_C(0x800000000)) ||
        d->level < floor || level < d->level)
      continue;
    if (!picture) {
      if (d->flags & 0x10000u)
        continue;
    } else {
      if (!d->picture)
        throw Error("eligible reagent has no picture name");
      if (*d->picture != *picture)
        continue;
    }
    rows.push_back(d);
  }
  return pick_plain(t, rng, rows);
}
std::optional<Item> generate_figurine(const Tables &t, const Game &g, Rng &rng,
                                      std::int32_t level) {
  level = items::s16(static_cast<std::uint32_t>(level));
  const auto floor = difficulty_floor(g);
  std::vector<std::shared_ptr<Definition>> rows;
  for (const auto &d : t.defs) {
    if (!d)
      throw Error("null supplied base definition");
    if (!(d->flags & 0x10000u) && d->category == 2 &&
        (d->flags & UINT64_C(0x200000000000)) && floor <= d->level &&
        d->level <= level)
      rows.push_back(d);
  }
  return pick_plain(t, rng, rows);
}
Item generate_consumable(const Tables &t, const Game &g, Rng &rng) {
  const auto tier = g.difficulty < 4 ? g.difficulty : 3;
  const std::uint32_t low = g.pvp_b ? 0x7fffu : 30000u,
                      high = g.pvp_b ? 0x10000u : 60000u;
  const auto roll = rng.next();
  if (roll >= high)
    return parse_descriptor(t, t.recall, -1, false, &rng);
  const auto &names = roll < low ? t.mana_potions : t.heal_potions;
  const auto size = fadd(fmul(fmul(fint(rng.next()), F_2_POW_M16), 0x3e4cccccu),
                         fmul(g.level, 0x3da3d70au));
  const int index = fle(size, F_ONE) ? 11 : fle(size, F_TWO) ? 10 : 9;
  const auto offset =
      static_cast<std::int64_t>(index) - 3 * static_cast<std::int64_t>(tier);
  if (offset < 0 || static_cast<std::uint64_t>(offset) >= names.size())
    throw Error("potion lookup is outside supplied table");
  return parse_descriptor(t, names[static_cast<std::size_t>(offset)], -1, false,
                          &rng);
}
std::optional<Item> shop_append_unique(const Tables &t, const Game &g, Rng &rng,
                                       std::vector<Item> &stock,
                                       std::int32_t budget,
                                       std::int32_t category) {
  std::optional<Item> current;
  for (unsigned attempt = 0; attempt < 2; ++attempt) {
    auto made = generate_with_affixes(t, g, rng, budget, category, -1, 1);
    if (made)
      current = std::move(made);
    if (!current)
      throw Unsupported("first shop attempt generated nothing; unwritten stack "
                        "contents are not modeled");
    const bool duplicate =
        std::any_of(stock.begin(), stock.end(), [&](const Item &item) {
          return item.definition == current->definition &&
                 item.flags == current->flags;
        });
    if (!duplicate) {
      Item entry(current->definition);
      entry.count = current->count;
      entry.flags = current->flags;
      stock.push_back(entry);
      return entry;
    }
  }
  return std::nullopt;
}
void World::spawn_gold(std::int32_t amount, Rng &) {
  drops.push_back({"gold", std::nullopt, amount});
}
bool World::spawn_item(const std::string &kind, Item &item, Rng &) {
  drops.push_back({kind, item, 0});
  return true;
}
void World::throw_item(const Item &, std::int32_t angle) {
  drops.push_back({"throw", std::nullopt, angle});
}
Item spawn_named_item(const Tables &t, Rng &rng, World &world,
                      const std::string &text) {
  auto item = parse_descriptor(t, text, -1, false, &rng);
  world.spawn_item("named", item, rng);
  return item;
}
std::array<Raw, 3> gold_scatter(Rng &rng, const GroundHeight &height, Raw x,
                                Raw y, Raw z) {
  for (unsigned attempt = 0; attempt < 10; ++attempt) {
    Raw vx = 0, vy = 0, vz = 0;
    for (;;) {
      const auto a = fmul(fint(rng.next()), F_2_POW_M16);
      const auto b = fmul(fint(rng.next()), F_2_POW_M16);
      const auto c = fmul(fint(rng.next()), F_2_POW_M16);
      vx = fsub(fadd(a, a), F_ONE);
      vy = fsub(fadd(b, b), F_ONE);
      vz = fsub(fadd(c, c), F_ONE);
      if (!flt(F_ONE, fadd(fadd(fmul(vx, vx), fmul(vy, vy)), fmul(vz, vz))))
        break;
    }
    const auto dx = fmul(fmul(vx, 0x41400000u), F_THREE),
               dy = fmul(fmul(vy, 0x41400000u), F_THREE);
    const auto h = height(fadd(dx, x), fadd(dy, y), z);
    if (flt(fsub(h, z) & 0x7fffffffu, 0x40c00000u))
      return {fadd(x, dx), fadd(y, dy), h};
  }
  return {x, y, z};
}
std::int32_t half_level(const Game &g) {
  return items::s16(static_cast<std::uint32_t>(ftrunc(g.level) / 2));
}
std::string loot_generate_drops(
    const Tables &t, const Game &g, Rng &rng, World &world, Raw gold_scale,
    Raw gold_chance, Raw quality_row, Raw item_chance, std::int32_t angle,
    const std::optional<std::string> &reagent_filter, bool force_preset) {
  if (g.num_players >= 2) {
    item_chance = fadd(item_chance, item_chance);
    gold_chance = float_times_double(gold_chance, UINT64_C(0x3ff8000000000000));
    gold_scale = float_times_double(gold_scale, t.gold_scale_multiplayer);
  }
  item_chance = fmul(item_chance, F_HALF);
  if (g.pvp_b)
    item_chance = 0x3e800000u;
  if (flt(quality_row, F_ZERO))
    quality_row = F_ZERO;
  if (flt(0x41100000u, quality_row))
    quality_row = 0x41100000u;
  const auto row = ftrunc(quality_row);
  const auto blend = fsub(quality_row, fint(static_cast<std::uint32_t>(row)));
  const auto here = t.quality_row(row), there = t.quality_row(row + 1);
  const auto keep = fsub(F_ONE, blend);
  auto roll = fmul(fint(rng.next()), F_2_POW_M16);
  const auto poor = fadd(fmul(here[0], keep), fmul(there[0], blend)),
             normal = fadd(fmul(here[1], keep), fmul(there[1], blend));
  Raw quality = F_ONE;
  if (flt(roll, poor))
    quality = F_HALF;
  if (flt(fadd(poor, normal), roll))
    quality = F_THREE;
  const auto gold_roll = rng.next();
  const unsigned gold_rolls = gold_roll <= 0xee47u   ? 1
                              : gold_roll <= 0xf9ffu ? 2
                                                     : 3;
  gold_scale = fmul(gold_scale, g.level_times_0_4());
  if (g.pvp_a)
    gold_chance = F_ZERO;
  for (unsigned n = 0; n < gold_rolls; ++n)
    if (flt(fint(rng.next()), fmul(gold_chance, F_2_POW_16))) {
      const auto amount = ftrunc(
          fmul(fmul(fint(rng.next() % 40 + 40), gold_scale), 0x3d0f5c28u));
      world.spawn_gold(amount, rng);
    }
  const auto extra = fint(static_cast<std::uint32_t>(g.num_players) - 1u);
  auto chance = g.pvp_b ? fadd(fmul(extra, 0x3d75c28fu), 0x3da3d70au)
                        : fadd(fmul(extra, 0x3d4cccccu), 0x3d999999u);
  if (flt(fint(rng.next()), fmul(chance, F_2_POW_16))) {
    auto item = rng.next() < 8000u
                    ? parse_descriptor(t, t.arrows, -1, false, &rng)
                    : generate_consumable(t, g, rng);
    world.spawn_item("consumable", item, rng);
  }
  const bool filter = reagent_filter && !reagent_filter->empty();
  chance = filter ? 0x3c03126eu : 0x3c4cccccu;
  if (flt(fint(rng.next()), fmul(chance, F_2_POW_16))) {
    auto item = generate_reagent(t, g, rng, half_level(g),
                                 filter ? reagent_filter : std::nullopt);
    if (!item)
      return "no reagent";
    world.spawn_item("reagent", *item, rng);
  }
  if (flt(fint(rng.next()), 0x43a3d70au)) {
    auto item = generate_figurine(t, g, rng, half_level(g));
    if (!item)
      return "no figurine";
    world.spawn_item("figurine", *item, rng);
  }
  if (!flt(fint(rng.next()), fmul(item_chance, F_2_POW_16)))
    return "no equipment roll";
  const auto spread =
      fadd(fmul(fmul(fint(rng.next()), F_2_POW_M16), 0x3eccccccu), 0x3f4cccccu);
  auto budget = fmul(fmul(quality, spread),
                     fmul(g.level_times_difficulty(), 0x43480000u));
  roll = fint(rng.next());
  if (flt(roll, 0x4423d666u))
    budget = fmul(budget, F_THREE);
  else if (flt(roll, 0x454ccbffu))
    budget = fadd(budget, budget);
  const auto whole = ftrunc(budget);
  std::int32_t category = 0;
  if (rng.next() >= 30000u)
    category = rng.next() > 0xea5fu ? 2 : 1;
  const auto level = ftrunc(g.level);
  const bool preset_roll = flt(fint(rng.next()), 0x44f5c28fu);
  std::optional<Item> item;
  if (preset_roll || force_preset) {
    const auto factor = flt(F_THREE, budget) ? 0x3fd99999u : 0x40e00000u;
    item = generate_preset(
        t, g, rng,
        ftrunc(fmul(fint(static_cast<std::uint32_t>(whole)), factor)), category,
        level);
  } else
    item = generate_with_affixes(t, g, rng, whole, category, -1, level);
  if (!item)
    return "no equipment";
  if (world.spawn_item("equipment", *item, rng)) {
    const auto thrown = items::s32(static_cast<std::uint32_t>(angle) +
                                   (rng.next() & 0x3fffu) - 0x2000u);
    world.throw_item(*item, thrown);
  }
  return "done";
}
} // namespace champions::simulation::rta::items
