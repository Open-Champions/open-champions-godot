#pragma once
#include "champions/combat.hpp"
#include "champions/items.hpp"
#include <map>
#include <memory>

namespace champions::simulation::rta::combat {
// An explicit record bridge, without routing combat through fixed-Ps2 item
// math. This arena owns stable combat definitions and retains their source
// definitions. Converted Items borrow arena pointers: keep the arena alive
// through their use. Call refresh_definitions() after changing source
// definition fields.
class ItemArena {
  struct Entry {
    std::shared_ptr<items::Definition> source;
    std::unique_ptr<ItemDef> target;
  };
  std::map<const items::Definition *, Entry> definitions_;
  static void update(Entry &entry);

public:
  Item convert(const items::Item &source);
  void refresh_definitions();
  std::size_t definition_count() const noexcept { return definitions_.size(); }
};
} // namespace champions::simulation::rta::combat
