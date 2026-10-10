#pragma once
#include "champions/items.hpp"
#include <functional>

namespace champions::simulation::rta::items {
std::int32_t weapon_group(const Definition &);
std::int32_t armor_group(const Definition &);
std::optional<Item> generate_preset(const Tables &, const Game &, Rng &,
                                    std::int32_t budget, std::int32_t category,
                                    std::int32_t level);
std::optional<Item> generate_with_affixes(const Tables &, const Game &, Rng &,
                                          std::int32_t budget,
                                          std::int32_t category,
                                          std::int32_t want_suffix,
                                          std::int32_t level);
std::optional<Item>
generate_reagent(const Tables &, const Game &, Rng &, std::int32_t level,
                 const std::optional<std::string> &picture = std::nullopt);
std::optional<Item> generate_figurine(const Tables &, const Game &, Rng &,
                                      std::int32_t level);
Item generate_consumable(const Tables &, const Game &, Rng &);
std::optional<Item> shop_append_unique(const Tables &, const Game &, Rng &,
                                       std::vector<Item> &stock,
                                       std::int32_t budget,
                                       std::int32_t category);
// Events own item snapshots; Definition identity is shared, as in Item itself.
// Hooks intentionally do not simulate original constructors, terrain or
// allocation.
struct Drop {
  std::string kind;
  std::optional<Item> item;
  std::int32_t value = 0;
};
class World {
public:
  std::vector<Drop> drops;
  virtual ~World() = default;
  virtual void spawn_gold(std::int32_t amount, Rng &);
  virtual bool spawn_item(const std::string &kind, Item &, Rng &);
  virtual void throw_item(const Item &, std::int32_t angle);
};
Item spawn_named_item(const Tables &, Rng &, World &, const std::string &text);
using GroundHeight = std::function<Raw(Raw, Raw, Raw)>;
std::array<Raw, 3> gold_scatter(Rng &, const GroundHeight &, Raw x, Raw y,
                                Raw z);
std::int32_t half_level(const Game &);
std::string loot_generate_drops(
    const Tables &, const Game &, Rng &, World &, Raw gold_scale,
    Raw gold_chance, Raw quality_row, Raw item_chance, std::int32_t angle = 0,
    const std::optional<std::string> &reagent_filter = std::nullopt,
    bool force_preset = false);
} // namespace champions::simulation::rta::items
