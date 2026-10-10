#include "champions/core_arithmetic.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>

namespace champions::simulation::rta {
namespace {
// Ten base-2^32 limbs cover the largest exact single sum (278 bits).
// The same bounded integer representation handles the 122-bit double product.
struct Wide {
  std::array<std::uint32_t, 10> words{};
  explicit Wide(std::uint64_t value = 0) {
    words[0] = static_cast<Raw>(value);
    words[1] = static_cast<Raw>(value >> 32);
  }
  bool bit(int n) const {
    return n >= 0 && n < 320 &&
           ((words[static_cast<unsigned>(n) / 32] >> (n % 32)) & 1u);
  }
  int bits() const {
    for (int i = 9; i >= 0; --i)
      if (words[static_cast<unsigned>(i)]) {
        auto w = words[static_cast<unsigned>(i)];
        int n = 0;
        while (w) {
          ++n;
          w >>= 1;
        }
        return i * 32 + n;
      }
    return 0;
  }
  bool any_below(int n) const {
    for (int i = 0; i < n; ++i)
      if (bit(i))
        return true;
    return false;
  }
  Wide shifted(int amount) const {
    Wide out;
    if (amount >= 320 || amount <= -320)
      return out;
    const unsigned shift = static_cast<unsigned>(amount < 0 ? -amount : amount);
    const unsigned whole = shift / 32, part = shift % 32;
    for (unsigned i = 0; i < 10; ++i) {
      if (amount >= 0 && i + whole < 10) {
        out.words[i + whole] |= words[i] << part;
        if (part && i + whole + 1 < 10)
          out.words[i + whole + 1] |= words[i] >> (32 - part);
      } else if (amount < 0 && i >= whole) {
        out.words[i - whole] |= words[i] >> part;
        if (part && i > whole)
          out.words[i - whole - 1] |= words[i] << (32 - part);
      }
    }
    return out;
  }
  std::uint64_t low() const {
    return words[0] | (std::uint64_t{words[1]} << 32);
  }
};
int wide_compare(const Wide &a, const Wide &b) {
  for (int i = 9; i >= 0; --i) {
    const auto n = static_cast<unsigned>(i);
    if (a.words[n] != b.words[n])
      return a.words[n] > b.words[n] ? 1 : -1;
  }
  return 0;
}
Wide plus(const Wide &a, const Wide &b) {
  Wide out;
  std::uint64_t carry = 0;
  for (unsigned i = 0; i < 10; ++i) {
    const auto sum = std::uint64_t{a.words[i]} + b.words[i] + carry;
    out.words[i] = static_cast<Raw>(sum);
    carry = sum >> 32;
  }
  return out;
}
Wide minus(const Wide &a, const Wide &b) { // a >= b
  Wide out;
  std::uint64_t borrow = 0;
  for (unsigned i = 0; i < 10; ++i) {
    const auto subtrahend = std::uint64_t{b.words[i]} + borrow;
    out.words[i] = static_cast<Raw>(std::uint64_t{a.words[i]} - subtrahend);
    borrow = std::uint64_t{a.words[i]} < subtrahend;
  }
  return out;
}
Wide product(std::uint64_t a, std::uint64_t b) {
  Wide out;
  const std::array<Raw, 2> x{static_cast<Raw>(a), static_cast<Raw>(a >> 32)};
  const std::array<Raw, 2> y{static_cast<Raw>(b), static_cast<Raw>(b >> 32)};
  for (unsigned i = 0; i < 2; ++i) {
    std::uint64_t carry = 0;
    for (unsigned j = 0; j < 2; ++j) {
      const auto term = std::uint64_t{x[i]} * y[j] + out.words[i + j] + carry;
      out.words[i + j] = static_cast<Raw>(term);
      carry = term >> 32;
    }
    out.words[i + 2] = static_cast<Raw>(carry);
  }
  return out;
}
std::uint64_t quotient(const Wide &numerator, std::uint64_t denominator,
                       std::uint64_t &rest) {
  // Callers guarantee a <=62-bit divisor and a <=62-bit quotient.
  rest = 0;
  std::uint64_t result = 0;
  for (int i = numerator.bits() - 1; i >= 0; --i) {
    rest = (rest << 1) | (numerator.bit(i) ? 1u : 0u);
    const bool take = rest >= denominator;
    if (take)
      rest -= denominator;
    result = (result << 1) | (take ? 1u : 0u);
  }
  return result;
}
struct Single {
  unsigned sign;
  std::uint64_t mant;
  int exp;
};
Single split(Raw a) {
  const int e = static_cast<int>((a >> 23) & 255u);
  return {a >> 31, e == 0 ? 0 : std::uint64_t{(a & 0x7fffffu) | 0x800000u}, e};
}
Raw rounded(unsigned sign, const Wide &magnitude, int exponent, bool nearest) {
  if (!magnitude.bits())
    return sign << 31;
  int drop = magnitude.bits() - 24;
  auto kept = magnitude.shifted(-drop).low();
  if (drop > 0 && nearest && magnitude.bit(drop - 1) &&
      (magnitude.any_below(drop - 1) || (kept & 1u))) {
    ++kept;
    if (kept == (UINT64_C(1) << 24)) {
      kept >>= 1;
      ++drop;
    }
  }
  const int e = exponent + drop + 150;
  if (e > 255)
    return (sign << 31) | FMAX;
  if (e < 1)
    return sign << 31;
  return (sign << 31) | (static_cast<Raw>(e) << 23) |
         (static_cast<Raw>(kept) & 0x7fffffu);
}
thread_local Fpu context;
constexpr std::uint64_t Done = UINT64_C(1) << 60;
enum DClass { Snan, Qnan, Zero, Number, Inf };
struct Parts {
  DClass cls;
  unsigned sign;
  int exp;
  std::uint64_t frac;
};
Parts unpack(DoubleRaw a) {
  const unsigned s = static_cast<unsigned>(a >> 63);
  const int e = static_cast<int>((a >> 52) & 2047u);
  const auto f = a & UINT64_C(0xfffffffffffff);
  if (e == 0)
    return {Zero, s, 0, 0};
  if (e == 2047)
    return {f == 0 ? Inf : ((f & (UINT64_C(1) << 51)) ? Qnan : Snan), s, 1023,
            f == 0 ? Done : f};
  return {Number, s, e - 1023, (f << 8) | Done};
}
DoubleRaw pack(Parts a) {
  std::uint64_t field = 0, f = 0;
  if (a.cls < Zero) {
    field = 2047;
    f = a.frac | (UINT64_C(1) << 51);
  } else if (a.cls == Inf)
    field = 2047;
  else if (a.cls == Zero || a.frac == 0 || a.exp < -1022) {
  } else if (a.exp >= 1024)
    field = 2047;
  else {
    field = static_cast<std::uint64_t>(a.exp + 1023);
    f = a.frac >> 8;
  }
  return (std::uint64_t{a.sign & 1u} << 63) | (field << 52) |
         (f & UINT64_C(0xfffffffffffff));
}
Parts add_parts(Parts a, Parts b) {
  if (a.cls < Zero)
    return a;
  if (b.cls < Zero)
    return b;
  if (a.cls == Inf)
    return b.cls == Inf && a.sign != b.sign ? Parts{Snan, 0, 0, 0} : a;
  if (b.cls == Inf)
    return b;
  if (b.cls == Zero) {
    if (a.cls == Zero)
      a.sign &= b.sign;
    return a;
  }
  if (a.cls == Zero)
    return b;
  const int gap = a.exp - b.exp;
  const int count = gap < 0 ? -gap : gap;
  auto &smaller = gap > 0 ? b.frac : a.frac;
  if (count >= 64)
    smaller = 0;
  else
    for (int i = 0; i < count; ++i)
      smaller = (smaller >> 1) | (smaller & 1u);
  Parts out{Number, a.sign, std::max(a.exp, b.exp), 0};
  if (a.sign != b.sign) {
    if (a.frac >= b.frac) {
      out.frac = a.frac - b.frac;
      out.sign = a.sign;
    } else {
      out.frac = b.frac - a.frac;
      out.sign = b.sign;
    }
    if (out.frac == 0)
      out.sign = 0;
    while (out.frac && out.frac < Done) {
      out.frac <<= 1;
      --out.exp;
    }
  } else
    out.frac = a.frac + b.frac;
  if (out.frac >= (Done << 1)) {
    out.frac = (out.frac >> 1) | (out.frac & 1u);
    ++out.exp;
  }
  return out;
}
} // namespace
std::int32_t s16(std::uint32_t a) noexcept {
  const auto x = a & 0xffffu;
  return x < 0x8000 ? static_cast<std::int32_t>(x)
                    : static_cast<std::int32_t>(x) - 65536;
}
std::int32_t s32(std::uint32_t a) noexcept {
  return a <= 0x7fffffffu ? static_cast<std::int32_t>(a)
                          : -1 - static_cast<std::int32_t>(~a);
}
Fpu::Fpu(Model m, bool bias, bool nearest)
    : model(m), mul_bias(bias), div_nearest(nearest) {
  if (m != Model::Ps2 && m != Model::Ps2g1 && m != Model::Chop &&
      m != Model::Nearest)
    throw std::invalid_argument("invalid RTA FPU model");
}
Raw Fpu::add(Raw a, Raw b) const {
  auto x = split(a), y = split(b);
  if (!x.mant && !y.mant)
    return (x.sign & y.sign) << 31;
  if (!x.mant)
    return b;
  if (!y.mant)
    return a;
  if (model == Model::Ps2) {
    if (x.exp < y.exp) {
      std::swap(x, y);
      std::swap(a, b);
    }
    const int gap = x.exp - y.exp;
    if (gap >= 25)
      return a;
    const auto small = y.mant << 6;
    const auto floor_mag =
        (small >> gap) +
        ((y.sign && (small & ((UINT64_C(1) << gap) - 1))) ? 1u : 0u);
    const auto big = static_cast<std::int64_t>(x.mant << 6);
    const auto little = static_cast<std::int64_t>(floor_mag);
    const auto total = (x.sign ? -big : big) + (y.sign ? -little : little);
    return rounded(total < 0,
                   Wide(static_cast<std::uint64_t>(total < 0 ? -total : total)),
                   x.exp - 156, false);
  }
  const int gap = x.exp - y.exp;
  if (model == Model::Ps2g1) {
    if (gap >= 25)
      return a;
    if (gap <= -25)
      return b;
    if (gap > 1)
      y.mant = (y.mant >> (gap - 1)) << (gap - 1);
    if (gap < -1)
      x.mant = (x.mant >> (-gap - 1)) << (-gap - 1);
  }
  const int low = std::min(x.exp, y.exp);
  const Wide left = Wide(x.mant).shifted(x.exp - low),
             right = Wide(y.mant).shifted(y.exp - low);
  if (x.sign == y.sign)
    return rounded(x.sign, plus(left, right), low - 150,
                   model == Model::Nearest);
  const int order = wide_compare(left, right);
  if (order == 0)
    return 0;
  return rounded(order > 0 ? x.sign : y.sign,
                 order > 0 ? minus(left, right) : minus(right, left), low - 150,
                 model == Model::Nearest);
}
Raw Fpu::sub(Raw a, Raw b) const { return add(a, b ^ SIGN); }
Raw Fpu::mul(Raw a, Raw b) const {
  const auto x = split(a), y = split(b);
  const unsigned sign = x.sign ^ y.sign;
  if (!x.mant || !y.mant)
    return sign << 31;
  const auto p = x.mant * y.mant;
  const Wide wide(p);
  auto raw = rounded(sign, wide, x.exp + y.exp - 300, model == Model::Nearest);
  const int drop = wide.bits() - 24, exp = x.exp + y.exp - 150 + drop;
  if (mul_bias && drop > 0 && wide.any_below(drop) && exp >= 1 && exp <= 255 &&
      (raw & FMAX) > 0x800000u)
    --raw;
  return raw;
}
Raw Fpu::div(Raw a, Raw b) const {
  const auto x = split(a), y = split(b);
  const unsigned sign = x.sign ^ y.sign;
  if (!y.mant)
    return (sign << 31) | FMAX;
  if (!x.mant)
    return sign << 31;
  std::uint64_t rest = 0;
  const auto q = quotient(Wide(x.mant).shifted(40), y.mant, rest);
  return rounded(sign, Wide((q << 1) | (rest != 0)), x.exp - y.exp - 41,
                 div_nearest || model == Model::Nearest);
}
Raw Fpu::sqrt(Raw a) const {
  auto x = split(a);
  if (!x.mant)
    return x.sign << 31;
  int e = x.exp - 150;
  if (e % 2 != 0) {
    x.mant <<= 1;
    --e;
  }
  const Wide wide = Wide(x.mant).shifted(48);
  std::uint64_t root = 0;
  for (int i = 36; i >= 0; --i) {
    const auto candidate = root | (UINT64_C(1) << i);
    if (wide_compare(product(candidate, candidate), wide) <= 0)
      root = candidate;
  }
  const bool inexact = wide_compare(product(root, root), wide) != 0;
  return rounded(0, Wide((root << 1) | inexact), e / 2 - 25,
                 div_nearest || model == Model::Nearest);
}
Raw Fpu::rsqrt(Raw a, Raw b) const { return div(a, sqrt(b)); }
Raw Fpu::from_int(std::uint32_t value) const {
  const auto v = static_cast<std::int64_t>(s32(value));
  return rounded(v < 0, Wide(static_cast<std::uint64_t>(v < 0 ? -v : v)), 0,
                 model == Model::Nearest);
}
std::int32_t Fpu::to_int(Raw a) noexcept {
  const auto x = split(a);
  if (!x.mant)
    return 0;
  if (x.exp >= 158)
    return x.sign ? INT32_MIN : INT32_MAX;
  const auto mag = x.exp >= 150
                       ? x.mant << (x.exp - 150)
                       : (150 - x.exp >= 64 ? 0 : x.mant >> (150 - x.exp));
  const auto value = static_cast<std::int32_t>(mag);
  return x.sign ? -value : value;
}
int Fpu::compare(Raw a, Raw b) noexcept {
  const std::int64_t x =
      (a & SIGN) ? -static_cast<std::int64_t>(a & FMAX) : a & FMAX;
  const std::int64_t y =
      (b & SIGN) ? -static_cast<std::int64_t>(b & FMAX) : b & FMAX;
  return (x > y) - (x < y);
}
Fpu &set_fpu(Fpu::Model model, bool bias, bool nearest) {
  context = Fpu(model, bias, nearest);
  return context;
}
ScopedFpu::ScopedFpu(const Fpu &desired) : previous_(context) {
  context = Fpu(desired.model, desired.mul_bias, desired.div_nearest);
}
ScopedFpu::~ScopedFpu() noexcept { context = previous_; }
Raw add(Raw a, Raw b) { return context.add(a, b); }
Raw sub(Raw a, Raw b) { return context.sub(a, b); }
Raw mul(Raw a, Raw b) { return context.mul(a, b); }
Raw div(Raw a, Raw b) { return context.div(a, b); }
Raw sqrt(Raw a) { return context.sqrt(a); }
Raw rsqrt(Raw a, Raw b) { return context.rsqrt(a, b); }
Raw neg(Raw a) noexcept { return a ^ SIGN; }
Raw fabs(Raw a) noexcept { return a & FMAX; }
Raw fmax(Raw a, Raw b) noexcept { return Fpu::compare(a, b) >= 0 ? a : b; }
Raw fmin(Raw a, Raw b) noexcept { return Fpu::compare(a, b) <= 0 ? a : b; }
Raw from_int(std::uint32_t a) { return context.from_int(a); }
std::int32_t to_int(Raw a) noexcept { return Fpu::to_int(a); }
bool lt(Raw a, Raw b) noexcept { return Fpu::compare(a, b) < 0; }
bool le(Raw a, Raw b) noexcept { return Fpu::compare(a, b) <= 0; }
bool eq(Raw a, Raw b) noexcept { return Fpu::compare(a, b) == 0; }
std::uint32_t to_uint(Raw a) {
  return le(0x4f000000u, a)
             ? static_cast<Raw>(to_int(sub(a, 0x4f000000u))) | SIGN
             : static_cast<Raw>(to_int(a));
}
std::int32_t round_half_away(Raw a) {
  return to_int(le(ZERO, a) ? add(a, HALF) : sub(a, HALF));
}
DoubleRaw dadd(DoubleRaw a, DoubleRaw b) {
  return pack(add_parts(unpack(a), unpack(b)));
}
DoubleRaw dsub(DoubleRaw a, DoubleRaw b) {
  auto parts = unpack(b);
  parts.sign ^= 1;
  return pack(add_parts(unpack(a), parts));
}
DoubleRaw dmul(DoubleRaw a, DoubleRaw b) {
  auto x = unpack(a), y = unpack(b);
  const unsigned sign = x.sign ^ y.sign;
  if (x.cls < Zero) {
    x.sign = sign;
    return pack(x);
  }
  if (y.cls < Zero) {
    y.sign = sign;
    return pack(y);
  }
  if (x.cls == Inf || y.cls == Inf)
    return x.cls == Zero || y.cls == Zero ? D_NAN : pack({Inf, sign, 0, 0});
  if (x.cls == Zero || y.cls == Zero)
    return std::uint64_t{sign} << 63;
  auto frac = product(x.frac, y.frac).shifted(-60).low();
  int exp = x.exp + y.exp;
  if (frac >= (Done << 1)) {
    frac >>= 1;
    ++exp;
  }
  return pack({Number, sign, exp, frac});
}
DoubleRaw ddiv(DoubleRaw a, DoubleRaw b) {
  auto x = unpack(a), y = unpack(b);
  if (x.cls < Zero)
    return pack(x);
  if (y.cls < Zero)
    return pack(y);
  const unsigned sign = x.sign ^ y.sign;
  if (x.cls == Inf || x.cls == Zero) {
    if (x.cls == y.cls)
      return D_NAN;
    x.sign = sign;
    return pack(x);
  }
  if (y.cls == Inf)
    return std::uint64_t{sign} << 63;
  if (y.cls == Zero)
    return pack({Inf, sign, 0, 0});
  int exp = x.exp - y.exp;
  if (x.frac < y.frac) {
    x.frac <<= 1;
    --exp;
  }
  std::uint64_t rest = 0;
  return pack(
      {Number, sign, exp, quotient(Wide(x.frac).shifted(60), y.frac, rest)});
}
int dcmp(DoubleRaw a, DoubleRaw b) {
  const auto x = unpack(a), y = unpack(b);
  if (x.cls < Zero || y.cls < Zero)
    return 1;
  if (x.cls == Inf && y.cls == Inf)
    return static_cast<int>(y.sign) - static_cast<int>(x.sign);
  if (x.cls == Inf)
    return x.sign ? -1 : 1;
  if (y.cls == Inf)
    return y.sign ? 1 : -1;
  if (x.cls == Zero && y.cls == Zero)
    return 0;
  if (x.cls == Zero)
    return y.sign ? 1 : -1;
  if (y.cls == Zero)
    return x.sign ? -1 : 1;
  if (x.sign != y.sign)
    return x.sign ? -1 : 1;
  const int order = x.exp != y.exp ? (x.exp > y.exp ? 1 : -1)
                                   : ((x.frac > y.frac) - (x.frac < y.frac));
  return x.sign ? -order : order;
}
DoubleRaw i2d(std::uint32_t value) {
  const auto v = static_cast<std::int64_t>(s32(value));
  if (!v)
    return 0;
  std::uint64_t frac = static_cast<std::uint64_t>(v < 0 ? -v : v);
  int exp = 60;
  while (frac < Done) {
    frac <<= 1;
    --exp;
  }
  return pack({Number, v < 0, exp, frac});
}
std::int32_t d2i(DoubleRaw value) {
  const auto x = unpack(value);
  if (x.cls <= Zero)
    return 0;
  if (x.cls == Inf || x.exp >= 31)
    return x.sign ? INT32_MIN : INT32_MAX;
  if (x.exp < 0)
    return 0;
  const auto mag = static_cast<std::int32_t>(x.frac >> (60 - x.exp));
  return x.sign ? -mag : mag;
}
DoubleRaw f2d(Raw value) {
  const auto x = split(value);
  return x.mant ? pack({Number, x.sign, x.exp - 127, x.mant << 37})
                : std::uint64_t{x.sign} << 63;
}
Raw d2f(DoubleRaw value) {
  const auto x = unpack(value);
  if (x.cls == Zero || x.exp < -126)
    return x.sign << 31;
  if (x.exp >= 129)
    return (x.sign << 31) | 0x7f800000u;
  return (x.sign << 31) | (static_cast<Raw>(x.exp + 127) << 23) |
         (static_cast<Raw>(x.frac >> 37) & 0x7fffffu);
}
DoubleRaw fmod(DoubleRaw x, DoubleRaw y) {
  const auto ax = x & ~D_SIGN, ay = y & ~D_SIGN, sign = x & D_SIGN;
  if (ay == 0 || ax >= UINT64_C(0x7ff0000000000000) ||
      ay > UINT64_C(0x7ff0000000000000)) {
    const auto p = dmul(x, y);
    return ddiv(p, p);
  }
  if (ax < ay)
    return x;
  if (ax == ay)
    return sign;
  auto normalize = [](DoubleRaw value, int &exponent) {
    const int field = static_cast<int>(value >> 52);
    auto mant = value & UINT64_C(0xfffffffffffff);
    if (field) {
      exponent = field - 1023;
      return mant | (UINT64_C(1) << 52);
    }
    exponent = -1022;
    while (mant < (UINT64_C(1) << 52)) {
      mant <<= 1;
      --exponent;
    }
    return mant;
  };
  int ex = 0, ey = 0;
  auto mx = normalize(ax, ex);
  const auto my = normalize(ay, ey);
  for (int i = ex - ey; i > 0; --i) {
    if (mx >= my)
      mx -= my;
    if (!mx)
      return sign;
    mx <<= 1;
  }
  if (mx >= my)
    mx -= my;
  if (!mx)
    return sign;
  while (mx < (UINT64_C(1) << 52)) {
    mx <<= 1;
    --ey;
  }
  if (ey >= -1022)
    return sign | (static_cast<DoubleRaw>(ey + 1023) << 52) |
           (mx - (UINT64_C(1) << 52));
  return sign | (mx >> (-1022 - ey));
}
} // namespace champions::simulation::rta
