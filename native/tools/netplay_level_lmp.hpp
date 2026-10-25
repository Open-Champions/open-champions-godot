#pragma once
// LMP selection for the counts-only netplay_level_measure tool. The netplay
// CTest covers it with authored lumps.
#include "champions/archive.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace champions::netplay_level {

struct LmpRefusals {
  unsigned count = 0;
};

// champions::readLmp: u32 count, then 12-byte records (research FORMATS.md
// "LMP"). Copies the first .world payload and the first objects.ob. A lump
// readLmp refuses is counted and skipped.
inline bool lmp_world(const std::uint8_t *data, std::size_t size,
                      std::vector<std::uint8_t> &world,
                      std::vector<std::uint8_t> &objects,
                      LmpRefusals &refusals) {
  world.clear();
  objects.clear();
  const champions::Bytes lump{data, size};
  std::vector<champions::Entry> entries;
  try {
    entries = champions::readLmp(lump);
  } catch (const champions::FormatError &) {
    ++refusals.count;
    return false;
  }
  bool found = false;
  for (const auto &entry : entries) {
    const auto bytes = champions::payload(lump, entry);
    if (!found && champions::hasExtension(entry.name, ".world")) {
      world.assign(bytes.data, bytes.data + bytes.size);
      found = true;
    } else if (objects.empty() && entry.name.size() == 10 &&
               champions::hasExtension(entry.name, "objects.ob")) {
      objects.assign(bytes.data, bytes.data + bytes.size);
    }
  }
  return found;
}

} // namespace champions::netplay_level
