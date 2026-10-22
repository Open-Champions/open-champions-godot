#include "champions/arena_executable.hpp"

#include <stdexcept>
#include <utility>

namespace champions::simulation::rta::arena_executable {
namespace {
std::uint32_t u32(const std::vector<std::uint8_t> &bytes, std::size_t at) {
  return std::uint32_t(bytes[at]) | std::uint32_t(bytes[at + 1]) << 8 |
         std::uint32_t(bytes[at + 2]) << 16 | std::uint32_t(bytes[at + 3]) << 24;
}
std::uint16_t u16(const std::vector<std::uint8_t> &bytes, std::size_t at) {
  return std::uint16_t(bytes[at] | bytes[at + 1] << 8);
}
std::string fold(std::string value) {
  for (char &letter : value)
    if (letter >= 'A' && letter <= 'Z')
      letter = char(letter - 'A' + 'a');
  return value;
}
} // namespace

bool Executable::load(std::vector<std::uint8_t> bytes) {
  bytes_ = std::move(bytes);
  segments_.clear();
  if (bytes_.size() < 0x34 || bytes_[0] != 0x7f || bytes_[1] != 'E' ||
      bytes_[2] != 'L' || bytes_[3] != 'F' || bytes_[4] != 1 || bytes_[5] != 1)
    return false;
  const auto phoff = u32(bytes_, 0x1c);
  const auto phentsize = u16(bytes_, 0x2a);
  const auto phnum = u16(bytes_, 0x2c);
  if (phentsize < 0x20)
    return false;
  for (unsigned i = 0; i < phnum; ++i) {
    const std::size_t at = std::size_t(phoff) + std::size_t(i) * phentsize;
    if (at > bytes_.size() || bytes_.size() - at < 0x20) {
      segments_.clear();
      return false;
    }
    if (u32(bytes_, at) != 1)
      continue;
    Segment segment{u32(bytes_, at + 8), u32(bytes_, at + 4),
                    u32(bytes_, at + 16)};
    if (segment.offset > bytes_.size() ||
        segment.size > bytes_.size() - segment.offset) {
      segments_.clear();
      return false;
    }
    segments_.push_back(segment);
  }
  return !segments_.empty();
}
std::size_t Executable::file_offset(std::uint32_t address,
                                    std::size_t length) const {
  for (const auto &segment : segments_)
    if (address >= segment.address &&
        std::uint64_t(address) + length <=
            std::uint64_t(segment.address) + segment.size)
      return segment.offset + (address - segment.address);
  throw std::runtime_error("Arena table address is not file-backed.");
}
std::uint32_t Executable::word(std::uint32_t address) const {
  return u32(bytes_, file_offset(address, 4));
}
std::string Executable::string(std::uint32_t address) const {
  std::string value;
  for (std::uint32_t at = address;; ++at) {
    const auto character = bytes_[file_offset(at, 1)];
    if (!character)
      return value;
    value.push_back(char(character));
  }
}
std::vector<std::string> Executable::names(std::uint32_t address) const {
  std::vector<std::string> result;
  for (unsigned i = 0; i < 32; ++i) {
    const auto pointer = word(address + 4u * i);
    if (!pointer)
      return result;
    result.push_back(string(pointer));
  }
  throw std::runtime_error("Arena name table has no terminator within 32 entries.");
}
std::vector<std::string> read_mode_a_names(const Executable &executable) {
  auto names_a = executable.names(NAMES_A_TABLE);
  const auto names_b = executable.names(NAMES_B_TABLE);
  if (names_a.size() != 13 || names_b.size() != 7)
    throw std::runtime_error("Arena name tables are not the documented 13 and 7 entries.");
  return names_a;
}
int mode_a_index(const std::vector<std::string> &names,
                 const std::string &level) {
  const auto want = fold(level);
  for (std::size_t i = 0; i < names.size(); ++i)
    if (!want.empty() && fold(names[i]) == want)
      return int(i);
  return -1;
}
} // namespace champions::simulation::rta::arena_executable
