#pragma once

#include "champions/arena_modes.hpp"

#include <cstdint>
#include <string>
#include <vector>

// The owner's executable, read at run time for the arena tables
// (research-arena 17810bb docs/arena-modes.md section 1). Nothing read here
// is copied into the tree: the bytes stay with the owner.
namespace champions::simulation::rta::arena_modes {

inline constexpr std::uint32_t NAMES_A_TABLE = 0x5037b8;
inline constexpr std::uint32_t NAMES_B_TABLE = 0x503798;
inline constexpr std::uint32_t RECIPE_TABLE = 0x503868;
inline constexpr std::uint32_t RECIPE_COUNT_HALF = 0x503886;

// ELF32 little-endian PT_LOAD mapping, file-backed bytes only. An address
// outside every file-backed segment refuses.
class Executable {
public:
  bool load(std::vector<std::uint8_t> bytes);
  std::uint32_t word(std::uint32_t va) const;
  std::uint16_t half(std::uint32_t va) const;
  std::uint8_t byte(std::uint32_t va) const;
  std::string string(std::uint32_t va) const;
  // A null-terminated pointer table. No null within 32 entries refuses.
  NameTable names(std::uint32_t va) const;

private:
  struct Seg {
    std::uint32_t va = 0, off = 0, size = 0;
  };
  std::size_t offset(std::uint32_t va, std::size_t n) const;
  std::vector<std::uint8_t> bytes_;
  std::vector<Seg> segs_;
};

struct ExeNames {
  NameTable names_a, names_b;
};
// Both name tables. A table that is not MODE_A_MAPS / MODE_B_MAPS long refuses.
ExeNames read_names(const Executable &);
// The mode-A table entry whose ASCII-lower-cased name equals the
// lower-cased level name, or -1.
int mode_a_index(const NameTable &, const std::string &level);

} // namespace champions::simulation::rta::arena_modes
