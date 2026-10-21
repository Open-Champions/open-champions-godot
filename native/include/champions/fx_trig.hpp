#pragma once
// math_cos16 (0x166630) and math_sin16 (0x1666e0).
//
// A degree-7 odd polynomial on the half angle, then cos 2x = 1 - 2 sin^2 x.
// The four coefficients are immediates. Neither routine loads a table. The sine
// is a second copy of the body after subtracting 0x4000, which is cos16 of that
// angle. Each mul, add and subtract truncates toward zero with the default ps2
// adder (the smaller operand keeps six extra bits). cvt.s.w of the folded angle
// is exact. No host libm and no fused multiply-add, so the bits are the same on
// arm64 and x86_64.
//
// The contract is docs/spec/trig16.md in the FX research (t3/research-fx fe137cc)
// and tools/ref_trig.py, which uses tools/ref_core.py's FPU. The fx-exact "trig"
// rows check all 65,536 angles against that reference.
//
// The only circle built from these results is the 64 haze pairs. particle_init
// writes them at run time; they are not in the executable. haze_circle recomputes
// one pair. Nothing here is a committed retail table.

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace champions::rta::fx::trig {
namespace {

struct Parts {
    unsigned sign;
    std::uint32_t mant;
    int exp;
};

inline Parts split(std::uint32_t raw) {
    const int exp = static_cast<int>((raw >> 23) & 0xffu);
    return {raw >> 31, exp == 0 ? 0u : ((raw & 0x7fffffu) | 0x800000u), exp};
}

// Truncate toward zero. mag is a positive integer; the result is mag * 2^exponent.
inline std::uint32_t trunc_pack(unsigned sign, std::uint64_t mag, int exponent) {
    if (mag == 0)
        return sign << 31;
    int bits = 0;
    for (std::uint64_t m = mag; m != 0; m >>= 1)
        ++bits;
    const int drop = bits - 24;
    const std::uint32_t kept = drop >= 0 ? static_cast<std::uint32_t>(mag >> drop)
                                         : static_cast<std::uint32_t>(mag << -drop);
    const int e = exponent + drop + 150;
    if (e > 255)
        return (sign << 31) | 0x7fffffffu;
    if (e < 1)
        return sign << 31;
    return (sign << 31) | (static_cast<std::uint32_t>(e) << 23) | (kept & 0x7fffffu);
}

inline std::int32_t s32(std::uint32_t a) noexcept {
    return a <= 0x7fffffffu ? static_cast<std::int32_t>(a)
                            : -1 - static_cast<std::int32_t>(~a);
}

// ps2 adder: align the smaller significand with six extra bits and floor the rest.
// Ignored when the exponents differ by 25 or more. Matches ref_core Fpu model 'ps2'.
inline std::uint32_t add(std::uint32_t a, std::uint32_t b) {
    auto x = split(a), y = split(b);
    if (x.mant == 0 && y.mant == 0)
        return (x.sign & y.sign) << 31;
    if (x.mant == 0)
        return b;
    if (y.mant == 0)
        return a;
    if (x.exp < y.exp) {
        const auto tx = x;
        x = y;
        y = tx;
        const auto ta = a;
        a = b;
        b = ta;
    }
    const int gap = x.exp - y.exp;
    if (gap >= 25)
        return a;
    const std::uint64_t small = std::uint64_t{y.mant} << 6;
    const std::uint64_t mask = gap == 0 ? 0ull : ((std::uint64_t{1} << gap) - 1ull);
    const std::uint64_t floor_mag = (small >> gap) + ((y.sign && (small & mask)) ? 1ull : 0ull);
    const auto big = static_cast<std::int64_t>(std::uint64_t{x.mant} << 6);
    const auto little = static_cast<std::int64_t>(floor_mag);
    const auto total = (x.sign ? -big : big) + (y.sign ? -little : little);
    if (total == 0)
        return 0;
    const auto mag = static_cast<std::uint64_t>(total < 0 ? -total : total);
    return trunc_pack(total < 0 ? 1u : 0u, mag, x.exp - 156);
}

inline std::uint32_t sub(std::uint32_t a, std::uint32_t b) { return add(a, b ^ 0x80000000u); }

inline std::uint32_t mul(std::uint32_t a, std::uint32_t b) {
    const auto x = split(a), y = split(b);
    const unsigned sign = x.sign ^ y.sign;
    if (x.mant == 0 || y.mant == 0)
        return sign << 31;
    return trunc_pack(sign, std::uint64_t{x.mant} * y.mant, x.exp + y.exp - 300);
}

inline std::uint32_t from_int(std::uint32_t value) {
    const auto v = static_cast<std::int64_t>(s32(value));
    if (v == 0)
        return 0;
    const auto mag = static_cast<std::uint64_t>(v < 0 ? -v : v);
    return trunc_pack(v < 0 ? 1u : 0u, mag, 0);
}

inline std::int32_t to_int(std::uint32_t a) {
    const auto x = split(a);
    if (x.mant == 0)
        return 0;
    if (x.exp >= 158)
        return x.sign ? INT32_MIN : INT32_MAX;
    std::uint32_t mag;
    if (x.exp >= 150)
        mag = x.mant << (x.exp - 150);
    else if (150 - x.exp >= 32)
        mag = 0;
    else
        mag = x.mant >> (150 - x.exp);
    const auto value = static_cast<std::int32_t>(mag);
    return x.sign ? -value : value;
}

inline std::int32_t s16(std::uint32_t angle) noexcept {
    const auto x = angle & 0xffffu;
    return x < 0x8000u ? static_cast<std::int32_t>(x) : static_cast<std::int32_t>(x) - 65536;
}

inline constexpr std::uint32_t K_HALF_ANGLE = 0x38490fdau;
inline constexpr std::uint32_t K_S3 = 0x3e2aaa57u;
inline constexpr std::uint32_t K_S5 = 0x3c086b24u;
inline constexpr std::uint32_t K_S7 = 0x39458017u;
inline constexpr std::uint32_t ONE = 0x3f800000u;
inline constexpr std::uint32_t MINUS_ONE = 0xbf800000u;
inline constexpr std::uint32_t K_FIVE = 0x40a00000u;

}  // namespace

// math_cos16. The low 16 bits are the angle. Bits above bit 15 do not change the result.
inline std::uint32_t cos16(std::uint32_t angle) {
    std::int32_t a = s16(angle);
    std::uint32_t sign = ONE;
    if (((angle ^ (angle << 1)) & 0x8000u) != 0) {
        sign = MINUS_ONE;
        a = static_cast<std::int32_t>(angle & 0x7fffu) - ((angle & 0x4000u) ? 0x8000 : 0);
    }
    const std::uint32_t x = mul(from_int(static_cast<std::uint32_t>(a)), K_HALF_ANGLE);
    const std::uint32_t x2 = mul(x, x);
    const std::uint32_t x3 = mul(x, x2);
    std::uint32_t s = sub(x, mul(x3, K_S3));
    const std::uint32_t x5 = mul(x3, x2);
    s = add(s, mul(x5, K_S5));
    const std::uint32_t x7 = mul(x5, x2);
    s = sub(s, mul(x7, K_S7));
    const std::uint32_t two_s = add(s, s);
    return mul(sign, sub(ONE, mul(two_s, s)));
}

// math_sin16: the same body after addiu v1, a0, -0x4000.
inline std::uint32_t sin16(std::uint32_t angle) { return cos16(angle - 0x4000u); }

// The float32 result, promoted to binary64. Every finite float32 is exact in binary64.
inline double as_double(std::uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, sizeof value);
    return static_cast<double>(value);
}

inline std::pair<double, double> cos_sin(std::uint32_t angle) {
    return {as_double(cos16(angle)), as_double(sin16(angle))};
}

// One pair particle_init stores: trunc(sin16(i<<10) * 5), trunc(cos16(i<<10) * 5).
// Index bits above the low six wrap. The reader uses this one pair, with no blend.
struct HazePair {
    std::int32_t sine = 0;
    std::int32_t cosine = 0;
};

inline HazePair haze_circle(std::uint32_t index) {
    const std::uint32_t angle = (index & 63u) << 10;
    HazePair pair;
    pair.sine = to_int(mul(sin16(angle), K_FIVE));
    pair.cosine = to_int(mul(cos16(angle), K_FIVE));
    return pair;
}

// particle_drawHaze's index. clock and birth are the halfwords; start_alpha and
// alpha_rate are the float patterns kind 3 stores at slot +0x2c and +0x3c.
inline std::uint32_t haze_index(std::uint32_t clock, std::uint32_t birth, std::uint32_t start_alpha,
                                std::uint32_t alpha_rate) {
    const auto age_i = static_cast<std::int32_t>(s16(clock) - s16(birth));
    const auto age = from_int(static_cast<std::uint32_t>(age_i));
    return static_cast<std::uint32_t>(to_int(add(start_alpha, mul(alpha_rate, age)))) & 63u;
}

// Creature_processDamage 0x110d30. The register is what math_cos16 and math_sin16
// receive; each consumes the low 16 bits. Combat round 5 (research 2980af1).
// Hit sparks add 0x7fff to the sign-extended hit halfword +0x2e0.
inline std::uint32_t process_damage_hit_angle(std::uint32_t hit_half) {
    return static_cast<std::uint32_t>(s16(hit_half) + 0x7fff);
}
// Metal sparks and Player_blockSparks silent 0 pass the sign-extended facing +0x38.
inline std::uint32_t process_damage_facing_angle(std::uint32_t facing_half) {
    return static_cast<std::uint32_t>(s16(facing_half));
}
// One gib heading. `draw` is the high 16 bits of one own-generator step. The
// bias-plus-remainder sum is sign-extended from 16 bits and added to the hit.
inline std::uint32_t process_damage_gib_angle(std::uint32_t hit_half, std::uint32_t draw,
                                             std::uint32_t span, std::uint32_t bias) {
    const auto rem = span == 0 ? 0u : draw % span;
    return static_cast<std::uint32_t>(s16(hit_half) + s16(bias + rem));
}

// Kind 3 and the screen axis are still open. Callers refuse instead of guessing.
[[noreturn]] inline void refuse_haze_kind3() {
    throw std::domain_error("whether particle_drawHaze runs only for kind 3 is not established");
}
[[noreturn]] inline void refuse_haze_screen_axis() {
    throw std::domain_error("which screen axis receives the haze sine word is not established");
}

}  // namespace champions::rta::fx::trig
