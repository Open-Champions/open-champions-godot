#pragma once

#include "champions/arena_executable.hpp"
#include "champions/quests.hpp"
#include <stdexcept>

namespace champions::simulation::rta::quests {
// US v2.00 table, research-story-route 76d4447. Read owner data at runtime;
// no executable payload or mission text belongs in the portable client.
inline std::vector<MissionRow> read_mission_rows(const arena_modes::Executable &exe) {
  constexpr std::uint32_t table = 0x4fe540u, stride = 0x34u;
  std::vector<MissionRow> rows;
  for (std::uint32_t i = 0; i < 13; ++i) {
    const auto base = table + i * stride;
    auto name = [&](std::uint32_t offset) {
      const auto ptr = exe.word(base + offset);
      return ptr ? exe.string(ptr) : std::string();
    };
    rows.push_back({name(0x08), name(0x0c), name(0x10), name(0x14),
                    std::int32_t(exe.word(base + 0x20)), std::int32_t(exe.word(base + 0x24)),
                    std::int32_t(exe.word(base + 0x28))});
  }
  if (rows[0].story != "blood1" || rows[1].story != "innovat1")
    throw std::runtime_error("The executable does not establish the documented story menu entries.");
  return rows;
}
} // namespace champions::simulation::rta::quests
