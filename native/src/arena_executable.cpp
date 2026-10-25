#include "champions/arena_executable.hpp"

#include <utility>

namespace champions::simulation::rta::arena_modes {
namespace {
std::uint32_t u32(const std::vector<std::uint8_t> &b, std::size_t at) {
  return std::uint32_t(b[at]) | std::uint32_t(b[at + 1]) << 8 | std::uint32_t(b[at + 2]) << 16 |
         std::uint32_t(b[at + 3]) << 24;
}
std::uint16_t u16(const std::vector<std::uint8_t> &b, std::size_t at) {
  return std::uint16_t(b[at] | (b[at + 1] << 8));
}
std::string fold(std::string s) {
  for (auto &c : s)
    if (c >= 'A' && c <= 'Z')
      c = char(c - 'A' + 'a');
  return s;
}
} // namespace

bool Executable::load(std::vector<std::uint8_t> bytes) {
  bytes_ = std::move(bytes);
  segs_.clear();
  if (bytes_.size() < 0x34 || bytes_[0] != 0x7f || bytes_[1] != 'E' || bytes_[2] != 'L' ||
      bytes_[3] != 'F' || bytes_[4] != 1 || bytes_[5] != 1)
    return false;
  const auto phoff = u32(bytes_, 0x1c);
  const auto phentsize = u16(bytes_, 0x2a);
  const auto phnum = u16(bytes_, 0x2c);
  for (unsigned i = 0; i < phnum; ++i) {
    const std::size_t at = phoff + std::size_t(i) * phentsize;
    if (at + 0x20 > bytes_.size()) {
      segs_.clear();
      return false;
    }
    if (u32(bytes_, at) != 1)
      continue;
    Seg seg{u32(bytes_, at + 8), u32(bytes_, at + 4), u32(bytes_, at + 16)};
    if (std::uint64_t(seg.off) + seg.size > bytes_.size()) {
      segs_.clear();
      return false;
    }
    segs_.push_back(seg);
  }
  return !segs_.empty();
}
std::size_t Executable::offset(std::uint32_t va, std::size_t n) const {
  for (const auto &seg : segs_)
    if (va >= seg.va && std::uint64_t(va) + n <= std::uint64_t(seg.va) + seg.size)
      return seg.off + (va - seg.va);
  throw Unsupported("address is not file-backed in the executable");
}
std::uint32_t Executable::word(std::uint32_t va) const { return u32(bytes_, offset(va, 4)); }
std::uint16_t Executable::half(std::uint32_t va) const { return u16(bytes_, offset(va, 2)); }
std::uint8_t Executable::byte(std::uint32_t va) const { return bytes_[offset(va, 1)]; }
std::string Executable::string(std::uint32_t va) const {
  auto at = offset(va, 1);
  auto end = at;
  while (end < bytes_.size() && bytes_[end])
    ++end;
  return std::string(reinterpret_cast<const char *>(bytes_.data() + at), end - at);
}
NameTable Executable::names(std::uint32_t va) const {
  NameTable out;
  for (int i = 0; i < 32; ++i) {
    const auto p = word(va + 4u * unsigned(i));
    if (!p)
      return out;
    out.push_back(string(p));
  }
  throw Unsupported("name table has no null pointer within 32 entries");
}
ExeNames read_names(const Executable &exe) {
  ExeNames out{exe.names(NAMES_A_TABLE), exe.names(NAMES_B_TABLE)};
  if (out.names_a.size() != std::size_t(MODE_A_MAPS) || out.names_b.size() != std::size_t(MODE_B_MAPS))
    throw Unsupported("arena name tables are not the documented 13 and 7 entries");
  return out;
}
int mode_a_index(const NameTable &names, const std::string &level) {
  const auto want = fold(level);
  for (std::size_t i = 0; i < names.size(); ++i)
    if (!want.empty() && fold(names[i]) == want)
      return int(i);
  return -1;
}
} // namespace champions::simulation::rta::arena_modes
