#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>

// The single-precision rounding helpers of the Return to Arms effect references
// (research tools/ref_particles.py rz() and f32(), commit 19f80a6). The references
// compute with binary64 values and cut each result to single precision; this header
// does exactly the same binary64 operation followed by the same cut. Header-only so the
// round-1 champions-native-fx library (lighting.cpp, fx.cpp) can use it without linking
// champions-fx-exact. Builds must not contract a*b+c into a fused multiply-add
// (-ffp-contract=off / /fp:strict are set by the targets).
namespace champions::rta::fx::rounding {

inline constexpr double FLT_MAX_D = 3.4028234663852886e38;

inline std::uint64_t double_bits(double x) noexcept {
    std::uint64_t b;
    std::memcpy(&b, &x, sizeof b);
    return b;
}
inline double bits_double(std::uint64_t b) noexcept {
    double x;
    std::memcpy(&x, &b, sizeof x);
    return x;
}
inline std::uint32_t float_bits(float x) noexcept {
    std::uint32_t b;
    std::memcpy(&b, &x, sizeof b);
    return b;
}
inline float bits_float(std::uint32_t b) noexcept {
    float x;
    std::memcpy(&x, &b, sizeof x);
    return x;
}

// f32(): round a binary64 to the nearest IEEE single (ties to even), as Python's
// struct.pack('<f', x) does, and widen it back. Python raises OverflowError for a finite
// value that rounds to infinity; this returns the infinity instead (no reference input
// reaches it).
inline double f32(double x) noexcept {
    if (std::isfinite(x) && std::fabs(x) >= 3.4028235677973366e38)  // FLT_MAX + half an ulp
        return std::copysign(HUGE_VAL, x);
    return static_cast<double>(static_cast<float>(x));
}

// rz(): a binary64 cut to single precision toward zero, as the console FPU rounds. Zero
// and NaN give +0.0; a magnitude below the smallest normal single gives +0.0; a result
// beyond FLT_MAX (including infinity) gives +-FLT_MAX.
inline double rz(double x) noexcept {
    if (x == 0.0 || x != x)
        return 0.0;
    std::uint64_t b = double_bits(x);
    if (((b >> 52) & 0x7ffu) < 1023u - 126u)
        return 0.0;
    b &= ~((std::uint64_t{1} << 29) - 1u);
    const double v = bits_double(b);
    if (std::fabs(v) <= FLT_MAX_D)
        return static_cast<double>(static_cast<float>(v));  // exact: 24 significant bits
    return std::copysign(FLT_MAX_D, v);
}

// The IEEE single pattern of a value that rz()/f32() produced.
inline std::uint32_t pattern(double single_value) noexcept {
    return float_bits(static_cast<float>(single_value));
}

}  // namespace champions::rta::fx::rounding
