#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace champions::simulation::rta::play_session {
inline constexpr const char *POLICY = "rta-play-session-v1";
inline constexpr std::size_t DEFAULT_WINDOW = 128, MAX_WINDOW = 128;
// Stop before the existing signed/raw frame domains can overflow. This is a
// numeric-domain guard, not a retained-history or elapsed-time budget.
inline constexpr std::uint64_t MAX_TICKS = 0x7ffffffd;
inline constexpr std::size_t STREAMS = 15;
using Counts = std::array<std::uint64_t, STREAMS>;
// Stream order: Session events/draws, net log/outbox, Memory stores,
// HP writes/calls/events, combat events/seed/cosmetic draws,
// movement writes/calls/events/results. Indices in suffix records are relative
// to this window. Add the corresponding retired count for an absolute index.
struct History {
  std::uint64_t through_ready = 0, retirements = 0;
  Counts retired{};
};
struct Retained {
  Counts records{}, capacity{};
};
} // namespace champions::simulation::rta::play_session
