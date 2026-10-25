#pragma once
// LMP reading for the counts-only level_spawn_measure tool. The level-flow
// CTest covers it with authored lumps.
#include "champions/archive.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace champions::level_spawn {

struct LmpEntry {
  std::string name;
  std::vector<std::uint8_t> bytes;
};

// champions::readLmp: u32 count, then 12-byte records (research FORMATS.md
// "LMP"; the same reading as NET-3 on codex/netplay, b06c4e5). Returns false,
// and leaves out empty, when readLmp refuses the lump.
inline bool lmp_entries(const std::vector<std::uint8_t> &blob,
                        std::vector<LmpEntry> &out) {
  out.clear();
  const champions::Bytes lump{blob.data(), blob.size()};
  std::vector<LmpEntry> entries;
  try {
    for (const auto &entry : champions::readLmp(lump)) {
      const auto bytes = champions::payload(lump, entry);
      entries.push_back({entry.name, std::vector<std::uint8_t>(
                                         bytes.data, bytes.data + bytes.size)});
    }
  } catch (const champions::FormatError &) {
    return false;
  }
  out = std::move(entries);
  return true;
}

} // namespace champions::level_spawn
