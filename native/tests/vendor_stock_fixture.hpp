#pragma once
#include "champions/vendor_stock.hpp"
#include <algorithm>

// Synthetic item tables for the vendor-stock contract. No retail names or bytes.
namespace champions::simulation::rta::items::vendor_fixture {
struct Options {
  bool presets = true;
  bool ordinary = true;
  bool bit40 = false;
  // Flags bit 40 on the ordinary weapon definitions named here only.
  std::vector<std::string> bit40_names;
};
inline Tables tables(const Options &options = {}) {
  Tables t;
  auto add = [&](std::vector<std::shared_ptr<Definition>> &rows, std::string name,
                 int category, int subtype, int slot, std::uint64_t flags,
                 int price, bool preset) {
    auto d = std::make_shared<Definition>();
    d->index = static_cast<std::int32_t>(rows.size());
    d->addr = (preset ? A_PRESETS : A_DEFS) + 64u * static_cast<std::uint32_t>(d->index);
    d->preset = preset;
    d->name = std::move(name);
    d->level = 1;
    d->weight = F_ONE;
    d->count = 1;
    d->price = price;
    d->slot = slot;
    d->category = category;
    d->subtype = subtype;
    d->damage_bits = subtype == 3 ? 8u : 1u;
    d->flags = flags;
    if (!options.ordinary && !preset)
      d->flags |= 0x10000u;
    if (options.bit40 && category == 0 && !preset)
      d->flags |= 1ull << 40;
    if (category == 0 && !preset &&
        std::find(options.bit40_names.begin(), options.bit40_names.end(),
                  d->name) != options.bit40_names.end())
      d->flags |= 1ull << 40;
    rows.push_back(d);
  };
  const int weapon_subtypes[3] = {6, 3, 0};
  for (int i = 0; i < 3; ++i) {
    add(t.defs, "w" + std::to_string(i), 0, weapon_subtypes[i], 0, 0, 100 + i * 10, false);
    // Above the storeLevel-0 first budget and inside the storeLevel-7 budget.
    add(t.defs, "wx" + std::to_string(i), 0, weapon_subtypes[i], 0, 0, 20000, false);
  }
  const int armour_slots[6] = {11, 4, 9, 12, 3, 0};
  for (int i = 0; i < 6; ++i) {
    add(t.defs, "a" + std::to_string(i), 1, 0, armour_slots[i], 0, 70 + i, false);
    add(t.defs, "ax" + std::to_string(i), 1, 0, armour_slots[i], 0, 20000, false);
    add(t.defs, "c" + std::to_string(i), 1, 0, armour_slots[i], 0x80000u, 80 + i, false);
    add(t.defs, "cx" + std::to_string(i), 1, 0, armour_slots[i], 0x80000u, 20000, false);
  }
  add(t.defs, "o0", 2, 0, 0, 0, 55, false);
  if (options.presets) {
    for (int i = 0; i < 3; ++i)
      add(t.presets, "p" + std::to_string(i), i, 0, 0, 0, 40 + i, true);
  }
  Prefix prefix;
  prefix.level = 1;
  prefix.mask = 0;
  prefix.texts.fill(std::optional<std::string>("+p"));
  t.prefixes.push_back(prefix);
  Suffix suffix;
  suffix.level = 1;
  suffix.texts.fill(std::optional<std::string>("+s"));
  t.suffixes.push_back(suffix);
  t.potion_stem = "potion_";
  t.thrown_stem = "thrown_";
  t.circlet_stem = "circlet";
  t.circlet_stem_equip = "circlet";
  t.weapon_multipliers.assign(8, F_ONE);
  return t;
}
inline VendorFixedText fixed_text() {
  VendorFixedText fixed;
  fixed.weapons = {"w0", "w1", "w2", "w0"};
  fixed.other = "o0";
  fixed.cell = [](std::uint32_t) { return std::string("o0"); };
  return fixed;
}
inline VendorStock ready(std::uint32_t shared, std::uint32_t cosmetic,
                         const Options &options = {}) {
  VendorStock stock(tables(options));
  stock.set_seeds(shared, cosmetic);
  stock.set_difficulty(1);
  stock.set_party(1, {0, 0, 0, 0});
  stock.set_fixed(fixed_text());
  return stock;
}
} // namespace champions::simulation::rta::items::vendor_fixture
