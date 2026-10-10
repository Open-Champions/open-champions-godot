#include "champions/combat.hpp"
#include "champions/core_math.hpp"
#include <limits>

// Contract: Open-Champions/champions-return-to-arms-re tools/ref_combat.py,
// commit 7212ecfecbeae8dd2015f6c82897226f49a94695. No executable data is
// loaded.
namespace champions::simulation::rta::combat {
namespace {
constexpr DoubleRaw D_MAG = UINT64_C(0x7fffffffffffffff);
constexpr DoubleRaw D_INF = UINT64_C(0x7ff0000000000000);
DoubleRaw checked_double(DoubleRaw value) {
  const auto mag = value & D_MAG;
  if (mag > D_INF)
    return NAN_VALUE;
  if (mag == D_INF)
    throw Unsupported("combat Fraction double does not model infinity");
  if (mag && !(mag >> 52))
    throw Unsupported("combat double input is below the normal binary64 range");
  return mag ? value : 0;
}
DoubleRaw checked_result(DoubleRaw value) { return checked_double(value); }
} // namespace
std::int32_t s8(std::uint32_t x) noexcept {
  return (x & 255u) < 128u ? static_cast<std::int32_t>(x & 255u)
                           : static_cast<std::int32_t>(x & 255u) - 256;
}
std::int64_t c_rem(std::int32_t a, std::int32_t b) noexcept {
  return b ? static_cast<std::int64_t>(a) % static_cast<std::int64_t>(b) : a;
}
Raw fadd(Raw a, Raw b) { return rta::add(a, b); }
Raw fsub(Raw a, Raw b) { return rta::sub(a, b); }
Raw fmul(Raw a, Raw b) { return rta::mul(a, b); }
Raw fdiv(Raw a, Raw b) { return rta::div(a, b); }
Raw fsqrt(Raw a) { return rta::sqrt(a); }
Raw itof(std::int64_t a) {
  return rta::from_int(static_cast<std::uint32_t>(a));
}
std::int32_t ftoi(Raw a) { return rta::to_int(a); }
bool flt(Raw a, Raw b) { return rta::lt(a, b); }
bool fle(Raw a, Raw b) { return rta::le(a, b); }
bool feq(Raw a, Raw b) { return rta::eq(a, b); }
DoubleRaw dbl_from_int(std::int64_t x) {
  return rta::i2d(static_cast<std::uint32_t>(x));
}
DoubleRaw dbl_from_unsigned(std::uint32_t x) {
  return rta::dadd(rta::i2d(x & 0x7fffffffu),
                   (x & SIGN) ? UINT64_C(0x41e0000000000000) : 0);
}
DoubleRaw dbl_from_single(Raw a) { return (a & 0x7f800000u) ? rta::f2d(a) : 0; }
DoubleRaw dbl_add(DoubleRaw a, DoubleRaw b) {
  a = checked_double(a);
  b = checked_double(b);
  if (a == NAN_VALUE || b == NAN_VALUE)
    return NAN_VALUE;
  // ref_combat uses exact Fraction addition before chopping to 53 bits.
  // The core library model instead drops an operand at a >=64 exponent gap.
  // Even an arbitrarily tiny opposite-sign addend borrows one output ULP when
  // chopping: preserve that combat-specific distinction here.
  if (a && b) {
    const auto ae = static_cast<int>((a >> 52) & 2047u),
               be = static_cast<int>((b >> 52) & 2047u);
    if (ae - be >= 64 || be - ae >= 64) {
      const auto large = ae > be ? a : b;
      return ((a ^ b) & rta::D_SIGN) ? checked_result(large - 1) : large;
    }
  }
  auto out = checked_result(rta::dadd(a, b));
  if (!out && a && b && a != (b ^ rta::D_SIGN))
    throw Unsupported("combat double sum underflowed its representation");
  return out;
}
DoubleRaw dbl_mul(DoubleRaw a, DoubleRaw b) {
  a = checked_double(a);
  b = checked_double(b);
  if (a == NAN_VALUE || b == NAN_VALUE)
    return NAN_VALUE;
  auto out = checked_result(rta::dmul(a, b));
  if (!out && a && b)
    throw Unsupported("combat double product underflowed its representation");
  return out;
}
DoubleRaw dbl_fmod(DoubleRaw a, DoubleRaw b) {
  a = checked_double(a);
  b = checked_double(b);
  if (a == NAN_VALUE || b == NAN_VALUE || !b)
    return NAN_VALUE;
  return checked_result(rta::fmod(a, b));
}
Raw dbl_to_single(DoubleRaw a) {
  a = checked_double(a);
  if (a == NAN_VALUE)
    return 0x7f800000u;
  return a ? rta::d2f(a) : 0;
}
Raw sqrtf(Raw a) { return rta::sqrtf(a); }
Raw scalbnf(Raw a, std::int32_t n) { return rta::scalbnf(a, n); }
Raw powf(Raw a, Raw b) { return rta::powf(a, b); }
Raw cos16(std::uint32_t a) { return rta::cos16(a); }
Raw sin16(std::uint32_t a) { return rta::sin16(a); }
std::size_t index(std::size_t size, std::int64_t p) {
  if (p >= 0) {
    if (static_cast<std::uint64_t>(p) >= size)
      throw Error("combat list index out of range");
    return static_cast<std::size_t>(p);
  }
  const auto distance = static_cast<std::uint64_t>(-(p + 1)) + 1;
  if (distance > size)
    throw Error("combat list index out of range");
  return size - static_cast<std::size_t>(distance);
}
std::uint32_t Seed::draw() {
  state = state * 0x19660du + 0x3c6ef35fu;
  auto value = state >> 16;
  draws.push_back(value);
  return value;
}
std::array<Raw, 3> Seed::in_unit_sphere() {
  for (;;) {
    std::array<Raw, 3> p{};
    for (auto &v : p) {
      auto u = fmul(itof(draw()), K_1_65536);
      v = fsub(fadd(u, u), ONE);
    }
    if (!flt(ONE,
             fadd(fadd(fmul(p[0], p[0]), fmul(p[1], p[1])), fmul(p[2], p[2]))))
      return p;
  }
}
Spell *Obj::spell(const Text &kind_) const {
  for (auto *value : spells)
    if (required(value).kind == kind_)
      return value;
  return nullptr;
}
std::int32_t desc_get(const Descriptor &desc, std::int64_t kind,
                      std::int64_t field) {
  for (const auto &slot : desc)
    if (s16(static_cast<std::uint32_t>(at(slot, 0))) == kind)
      return s16(static_cast<std::uint32_t>(at(slot, field)));
  return 0;
}
std::int32_t Item::mod(std::int64_t code, std::int64_t field) const {
  return desc_get(mods, code, field);
}
std::int32_t PlayerRec::rank(std::int64_t skill) const {
  for (auto pair : skills) {
    auto id = s16(static_cast<std::uint32_t>(pair.first));
    if (id == -1)
      break;
    if (id == skill)
      return s16(static_cast<std::uint32_t>(pair.second));
  }
  return 0;
}
EventValue::EventValue(const char *ascii) : value(Text{}) {
  if (!ascii)
    throw Error("null event text");
  auto &text = std::get<Text>(value);
  for (; *ascii; ++ascii) {
    const auto c = static_cast<unsigned char>(*ascii);
    if (c > 127)
      throw Error("event labels must be ASCII; use Text for Unicode");
    text.push_back(c);
  }
}
World::World(std::uint32_t s, std::uint32_t c) : seed(s), cosmetic(c) {
  stat_fn = [](std::int64_t, std::int64_t, std::int64_t) { return 0; };
  stat_for_fn = [](std::int64_t, std::int64_t, std::int64_t, Obj *,
                   std::int64_t) { return 0; };
  adjust_fn = [](std::int64_t, std::int64_t level, std::int64_t) {
    return level;
  };
  pool_fn = [](std::int64_t) { return true; };
  enemy_fn = [](Obj *, Obj *) { return 1; };
}
void World::log(std::initializer_list<EventValue> event) {
  events.emplace_back(event);
}
std::int32_t World::stat(std::int64_t skill, std::int64_t statistic,
                         std::int64_t level) {
  if (!stat_fn)
    throw Error("missing stat callback");
  return s32(static_cast<std::uint32_t>(stat_fn(skill, statistic, level)));
}
std::int32_t World::stat_for(std::int64_t skill, std::int64_t statistic,
                             std::int64_t level, Obj *caster,
                             std::int64_t cap) {
  if (!stat_for_fn)
    throw Error("missing stat-for callback");
  return s32(static_cast<std::uint32_t>(
      stat_for_fn(skill, statistic, level, caster, cap)));
}
std::int32_t World::adjust(std::int64_t skill, std::int64_t level,
                           std::int64_t n) {
  if (!adjust_fn)
    throw Error("missing adjust callback");
  return s32(static_cast<std::uint32_t>(adjust_fn(skill, level, n)));
}
bool World::pool(std::int64_t size) {
  if (!pool_fn)
    throw Error("missing pool callback");
  const bool ok = pool_fn(size);
  log("pool", size, ok);
  return ok;
}
void World::take_damage(Obj &victim, Raw amount, Obj *attacker,
                        std::uint32_t flags) {
  log("take_damage", victim.tag, amount,
      attacker ? EventValue(attacker->tag) : EventValue(nullptr), flags);
  if (take_damage_fn)
    take_damage_fn(victim, amount, attacker, flags);
}
Spell *World::add_owned_spell(Spell value) {
  auto owned = std::make_unique<Spell>(std::move(value));
  auto *p = owned.get();
  owned_spells.push_back(std::move(owned));
  return p;
}
} // namespace champions::simulation::rta::combat
