#pragma once
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace champions::presentation::rta {
// RTA SLUS_209.73: one simulated frame is 1/60 second. This host clock
// is deliberately absent from Session snapshots and gameplay arithmetic.
class FrameClock {
  std::uint64_t phase_ = 0;

public:
  static constexpr std::uint64_t HZ = 60, SECOND = 1000000;
  void reset() { phase_ = 0; }
  double alpha() const { return double(phase_) / double(SECOND); }
  // Integer microseconds retain the fractional 1/60 cadence without rounding
  // each tick to 16667us. No elapsed time is dropped during active catch-up.
  // The callback returns false on a pause, stop or failure, discarding debt.
  template <class Tick> void advance(std::int64_t usec, Tick tick) {
    if (usec < 0 ||
        std::uint64_t(usec) >
            (std::numeric_limits<std::uint64_t>::max() - phase_) / HZ)
      throw std::invalid_argument("Invalid presentation elapsed time.");
    phase_ += std::uint64_t(usec) * HZ;
    while (phase_ >= SECOND) {
      phase_ -= SECOND;
      if (!tick()) {
        reset();
        break;
      }
    }
  }
};
} // namespace champions::presentation::rta
