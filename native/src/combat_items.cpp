#include "champions/combat_items.hpp"

namespace champions::simulation::rta::combat {
void ItemArena::update(Entry &entry) {
  const auto &source = *entry.source;
  auto &target = *entry.target;
  target.name.clear();
  for (unsigned char byte : source.name)
    target.name.push_back(byte);
  target.dmg_min = source.damage_min;
  target.dmg_max = source.damage_max;
  target.armor = source.armor;
  target.attack = source.attack;
  target.category = source.category;
  target.subtype = source.subtype;
  target.dmg_type = source.damage_bits;
  target.flags = source.flags;
}
Item ItemArena::convert(const items::Item &source) {
  Item out;
  if (source.definition) {
    const auto identity = source.definition.get();
    auto found = definitions_.find(identity);
    if (found == definitions_.end()) {
      Entry entry{source.definition, std::make_unique<ItemDef>()};
      found = definitions_.emplace(identity, std::move(entry)).first;
    }
    update(found->second);
    out.defn = found->second.target.get();
  }
  out.count = source.count;
  out.flags = source.flags;
  for (std::size_t i = 0; i < source.mods.size(); ++i) {
    if (source.mods[i]) {
      const auto &m = *source.mods[i];
      out.mods[i] = {m.code, m.values[0], m.values[1], m.values[2]};
    }
  }
  return out;
}
void ItemArena::refresh_definitions() {
  for (auto &pair : definitions_)
    update(pair.second);
}
} // namespace champions::simulation::rta::combat
