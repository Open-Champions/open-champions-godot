#pragma once
// Correctly rounded binary64 cos/sin of the emitter angles, without the C library.
//
// The reference (research ref_particles._cs) models the original's math_cos16 with
// Python's math.cos / math.sin of a = s16(angle) * (2 pi / 65536): host libm calls.
// Their last bit is the platform's choice: macOS arm64 and x86_64 libm disagree on
// 18,327 of the 65,536 arguments and neither is correctly rounded everywhere (review
// fixes 2026-10-20, work/x64trig). The port and the vector generator therefore both
// use the correctly rounded value, the one result every IEEE platform can reproduce:
// double-double argument reduction and Taylor series in + - * / only, then one
// rounding. The whole argument domain (65,536 angles) is checked exhaustively against
// tools/binary64_trig.py (exact decimal arithmetic) by the fx-exact "trig" rows.
//
// Like the rest of the exact code this needs IEEE binary64 round-to-nearest and no FMA
// contraction (-ffp-contract=off / MSVC /fp:strict; the clang pragma below as well).
#include <cstdint>
#include <utility>

// Block-scoped in clang: never changes the includer's own contraction setting.
#if defined(__clang__)
#define CHAMPIONS_FX_TRIG_NO_CONTRACT _Pragma("clang fp contract(off)")
#else
#define CHAMPIONS_FX_TRIG_NO_CONTRACT
#endif
namespace champions::rta::fx::trig {
struct DD {
    double hi, lo;
};
inline DD quick_two_sum(double a, double b) {
    CHAMPIONS_FX_TRIG_NO_CONTRACT
    const double s = a + b;
    return {s, b - (s - a)};
}
inline DD two_sum(double a, double b) {
    CHAMPIONS_FX_TRIG_NO_CONTRACT
    const double s = a + b;
    const double bb = s - a;
    return {s, (a - (s - bb)) + (b - bb)};
}
inline DD split(double a) {
    CHAMPIONS_FX_TRIG_NO_CONTRACT
    const double c = 134217729.0 * a;   // 2^27 + 1 (Dekker)
    const double hi = c - (c - a);
    return {hi, a - hi};
}
inline DD two_prod(double a, double b) {
    CHAMPIONS_FX_TRIG_NO_CONTRACT
    const double p = a * b;
    const DD x = split(a), y = split(b);
    const double e = ((x.hi * y.hi - p) + x.hi * y.lo + x.lo * y.hi) + x.lo * y.lo;
    return {p, e};
}
inline DD add(DD a, DD b) {
    CHAMPIONS_FX_TRIG_NO_CONTRACT
    DD s = two_sum(a.hi, b.hi);
    const DD t = two_sum(a.lo, b.lo);
    s.lo += t.hi;
    s = quick_two_sum(s.hi, s.lo);
    s.lo += t.lo;
    return quick_two_sum(s.hi, s.lo);
}
inline DD mul(DD a, DD b) {
    CHAMPIONS_FX_TRIG_NO_CONTRACT
    DD p = two_prod(a.hi, b.hi);
    p.lo += a.hi * b.lo + a.lo * b.hi;
    return quick_two_sum(p.hi, p.lo);
}
inline DD div(DD a, double b) {
    CHAMPIONS_FX_TRIG_NO_CONTRACT
    const double q = a.hi / b;
    const DD p = two_prod(q, b);
    const double r = ((a.hi - p.hi) - p.lo + a.lo) / b;
    return quick_two_sum(q, r);
}
inline DD neg(DD a) { return {-a.hi, -a.lo}; }

// pi / 2 as three doubles (patterns 0x3FF921FB54442D18, 0x3C91A62633145C07,
// 0xB91F1976B7ED8FBC); their sum is within 6e-50 of pi / 2.
inline constexpr double PIO2_1 = 0x1.921fb54442d18p+0;
inline constexpr double PIO2_2 = 0x1.1a62633145c07p-54;
inline constexpr double PIO2_3 = -0x1.f1976b7ed8fbcp-110;

// cos and sin of x (|x| <= 4), each correctly rounded on the checked domain.
inline std::pair<double, double> cos_sin(double x) {
    CHAMPIONS_FX_TRIG_NO_CONTRACT
    // Quadrant: n = nearest integer to x / (pi / 2); the choice only needs |r| < 1.
    const double q = x * (1.0 / PIO2_1);
    const int n = q >= 0 ? int(q + 0.5) : -int(-q + 0.5);
    // r = x - n pi/2 in double-double; x - n * PIO2_1 is exact (Sterbenz, n * PIO2_1 exact).
    const double r1 = x - double(n) * PIO2_1;
    DD r = add({r1, 0.0}, {-double(n) * PIO2_2, 0.0});
    r = add(r, {-double(n) * PIO2_3, 0.0});
    const DD r2 = mul(r, r);
    // Taylor series, a fixed 18 terms: |r| < 0.8 makes the last term below 2^-120.
    DD s = r, ts = r, c = {1.0, 0.0}, tc = {1.0, 0.0};
    for (int k = 1; k <= 18; ++k) {
        ts = neg(div(mul(ts, r2), double((2 * k) * (2 * k + 1))));
        tc = neg(div(mul(tc, r2), double((2 * k - 1) * (2 * k))));
        s = add(s, ts);
        c = add(c, tc);
    }
    DD cx, sx;
    switch (((n % 4) + 4) % 4) {
    case 0: cx = c; sx = s; break;
    case 1: cx = neg(s); sx = c; break;
    case 2: cx = neg(c); sx = neg(s); break;
    default: cx = s; sx = neg(c); break;
    }
    return {cx.hi + cx.lo, sx.hi + sx.lo};
}
} // namespace champions::rta::fx::trig
#undef CHAMPIONS_FX_TRIG_NO_CONTRACT
