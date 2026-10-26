#pragma once
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>

namespace champions::simulation::rta::player {
// Bounded host implementation of Player_run's libc_sprintf name call-outs
// (player-tick.md states 0/1/2): a literal or one signed decimal substitution.
// The format itself comes from the selected executable, never a guessed name.
inline std::string format_name(const std::string &format,
                               std::optional<std::int32_t> value) {
  // Matches the composed World's 64-byte resource-name reader, a host bound.
  constexpr std::size_t limit = 64;
  if (format.empty() || format.size() >= limit || format.find('\0') != std::string::npos)
    throw std::invalid_argument("player name format: empty, unterminated or over host bound");
  const auto at = format.find('%');
  if (at == std::string::npos) return format;
  if (format.compare(at, 2, "%d") != 0 || !value ||
      format.find('%', at + 2) != std::string::npos)
    throw std::invalid_argument("player name format: only one documented %d substitution is supported");
  auto out = format.substr(0, at) + std::to_string(*value) + format.substr(at + 2);
  if (out.size() >= limit)
    throw std::invalid_argument("player name format: result exceeds host resource-name bound");
  return out;
}
} // namespace champions::simulation::rta::player
