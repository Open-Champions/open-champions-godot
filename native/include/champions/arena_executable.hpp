#pragma once

#include <cstdint>
#include <string>
#include <vector>

// The owner supplies the RTA US v2.00 executable at run time. The two arena
// name tables are documented in research-arena 17810bb, docs/arena-modes.md;
// no retail names or bytes are built into this client.
namespace champions::simulation::rta::arena_executable {

inline constexpr std::uint32_t NAMES_A_TABLE = 0x5037b8;
inline constexpr std::uint32_t NAMES_B_TABLE = 0x503798;

class Executable {
public:
  bool load(std::vector<std::uint8_t> bytes);
  std::vector<std::string> names(std::uint32_t address) const;

private:
  struct Segment {
    std::uint32_t address = 0, offset = 0, size = 0;
  };
  std::size_t file_offset(std::uint32_t address, std::size_t length) const;
  std::uint32_t word(std::uint32_t address) const;
  std::string string(std::uint32_t address) const;
  std::vector<std::uint8_t> bytes_;
  std::vector<Segment> segments_;
};

// Refuses a truncated or wrong-size table, as the wave-24 reader did.
std::vector<std::string> read_mode_a_names(const Executable &executable);
int mode_a_index(const std::vector<std::string> &names,
                 const std::string &level);

} // namespace champions::simulation::rta::arena_executable
