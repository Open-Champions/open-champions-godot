#pragma once
#include <cstdint>

namespace champions::simulation::rta {
using Raw = std::uint32_t;
using DoubleRaw = std::uint64_t;
inline constexpr Raw SIGN = 0x80000000u, FMAX = 0x7fffffffu, M32 = 0xffffffffu;
inline constexpr Raw ZERO = 0, HALF = 0x3f000000u, ONE = 0x3f800000u,
                     TWO = 0x40000000u, MINUS_ONE = 0xbf800000u,
                     TWO_M16 = 0x37800000u;
inline constexpr DoubleRaw M64 = UINT64_MAX,
                           D_SIGN = UINT64_C(0x8000000000000000),
                           D_NAN = UINT64_C(0x7ff8000000000000);
std::int32_t s16(std::uint32_t value) noexcept;
std::int32_t s32(std::uint32_t value) noexcept;

// RTA reference arithmetic, not a claim of measured console FPU behavior.
// No operation interprets a pattern with the host floating-point hardware.
class Fpu {
public:
  enum class Model { Ps2, Ps2g1, Chop, Nearest };
  explicit Fpu(Model model = Model::Ps2, bool mul_bias = false,
               bool div_nearest = false);
  Model model;
  bool mul_bias;
  bool div_nearest;
  Raw add(Raw a, Raw b) const;
  Raw sub(Raw a, Raw b) const;
  Raw mul(Raw a, Raw b) const;
  Raw div(Raw a, Raw b) const;
  Raw sqrt(Raw a) const;
  Raw rsqrt(Raw a, Raw b) const;
  Raw from_int(std::uint32_t value) const;
  static std::int32_t to_int(Raw a) noexcept;
  static int compare(Raw a, Raw b) noexcept;
};
// Each thread owns its context; independent clients may instead hold explicit
// Fpu objects.
Fpu &set_fpu(Fpu::Model model = Fpu::Model::Ps2, bool mul_bias = false,
             bool div_nearest = false);
// Temporarily select an explicit arithmetic contract for compound operations.
// Nested calls and exceptions restore the caller's thread-local settings.
class ScopedFpu {
  Fpu previous_;

public:
  explicit ScopedFpu(const Fpu &desired);
  ~ScopedFpu() noexcept;
  ScopedFpu(const ScopedFpu &) = delete;
  ScopedFpu &operator=(const ScopedFpu &) = delete;
};
Raw add(Raw a, Raw b);
Raw sub(Raw a, Raw b);
Raw mul(Raw a, Raw b);
Raw div(Raw a, Raw b);
Raw sqrt(Raw a);
Raw rsqrt(Raw a, Raw b);
Raw neg(Raw a) noexcept;
Raw fabs(Raw a) noexcept;
Raw fmax(Raw a, Raw b) noexcept;
Raw fmin(Raw a, Raw b) noexcept;
Raw from_int(std::uint32_t value);
std::int32_t to_int(Raw a) noexcept;
bool lt(Raw a, Raw b) noexcept;
bool le(Raw a, Raw b) noexcept;
bool eq(Raw a, Raw b) noexcept;
std::uint32_t to_uint(Raw a);
std::int32_t round_half_away(Raw a);
DoubleRaw dadd(DoubleRaw a, DoubleRaw b);
DoubleRaw dsub(DoubleRaw a, DoubleRaw b);
DoubleRaw dmul(DoubleRaw a, DoubleRaw b);
DoubleRaw ddiv(DoubleRaw a, DoubleRaw b);
int dcmp(DoubleRaw a, DoubleRaw b);
DoubleRaw i2d(std::uint32_t value);
std::int32_t d2i(DoubleRaw value);
DoubleRaw f2d(Raw value);
Raw d2f(DoubleRaw value);
DoubleRaw fmod(DoubleRaw x, DoubleRaw y);
} // namespace champions::simulation::rta
