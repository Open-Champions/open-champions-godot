#pragma once

#include "champions/arena_executable.hpp"
#include "champions/quests.hpp"
#include "champions/level_flow.hpp"
#include <limits>
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
struct ActTables {
  std::vector<std::vector<std::string>> names;
  level_flow::Acts flow;
};
// research-quests 7efd3ce; same runtime table reader as quests_measure.cpp.
// 64/256 are its authored validation bounds, not claimed game capacities.
inline ActTables read_act_tables(const arena_modes::Executable &exe) {
  constexpr std::uint32_t lists = 0x4f8e48u, counts = 0x4f8e10u, total = 0x4f8e7cu;
  const auto count = exe.word(total);
  if (count > 64) throw std::runtime_error("Act table exceeds the reader bound.");
  ActTables out;
  for (std::uint32_t act = 0; act < count; ++act) {
    const auto list = exe.word(lists + 4 * act), rows = exe.word(counts + 4 * act);
    if (rows > 256) throw std::runtime_error("Act row table exceeds the reader bound.");
    std::vector<std::string> names;
    level_flow::Act flow;
    for (std::uint32_t i = 0; i < rows; ++i) {
      const auto at = std::uint64_t(list) + 8 * std::uint64_t(i);
      if (at + 4 > std::numeric_limits<std::uint32_t>::max())
        throw std::runtime_error("Act row address overflows.");
      const auto pointer = exe.word(std::uint32_t(at));
      const auto name = pointer ? exe.string(pointer) : std::string();
      names.push_back(name);
      flow.push_back({name, exe.word(std::uint32_t(at + 4))});
    }
    out.names.push_back(std::move(names));
    out.flow.push_back(std::move(flow));
  }
  return out;
}
} // namespace champions::simulation::rta::quests
