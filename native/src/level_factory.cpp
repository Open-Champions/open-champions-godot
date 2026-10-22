#include "champions/level_factory.hpp"
#include <limits>
#include <stdexcept>

namespace champions::simulation::rta::level_factory {
namespace {
const std::vector<Row> kTests = {
#include "level_factory_table.inc"
};
// The Line row (0x15d3c0): paths_add, then a WaveMaker when the name property
// is strcasecmp-equal to "wave" (docs/script-session.md section 3).
constexpr std::uint32_t LINE = 0x5323c0, LINE_NAME_KEY = 0x5327b0, LINE_WAVE = 0x533900;
constexpr std::uint32_t WAVE_SIZE = 0x768, WAVE_CTOR = 0x15ecf8;
// Fall-backs in the original's order (0x15d684-0x15d998). light: the size and
// constructor depend on propList_hasAnyLight 0x23a2b4.
struct Fallback {
  bool str;
  std::uint32_t key;
  bool light;
  std::uint32_t size, ctor, lit_size, lit_ctor;
};
const Fallback kFallbacks[] = {
    {false, 0x533968, false, 0x3d0, 0x243624, 0, 0},
    {false, 0x533980, true, 0x148, 0x23d93c, 0x150, 0x24259c},
    {false, 0x533990, true, 0xf8, 0x23ad18, 0x100, 0x241468},
    {false, 0x5339a0, true, 0x198, 0x23c44c, 0x448, 0x2416c4},
    {false, 0x5339b0, false, 0x60, 0x240dc8, 0, 0},
    {false, 0x5339c0, false, 0x198, 0x23c44c, 0, 0},
    {false, 0x5339d0, false, 0x198, 0x23c44c, 0, 0},
    {false, 0x5339e0, false, 0x448, 0x2547fc, 0, 0},
    {true, 0x5339e8, false, 0x3e8, 0x2657a0, 0, 0},
    {true, 0x5339f8, false, 0x490, 0x31d180, 0, 0},
};
// propList_hasAnyLight: three keys, then three formats for 0..3.
constexpr std::uint32_t LIGHT_KEYS[] = {0x541ec8, 0x541ed8, 0x541ee8};
constexpr std::uint32_t LIGHT_FORMATS[] = {0x541e98, 0x541eb0, 0x541ec0};
constexpr int LIGHT_INDICES = 4;

int signed_char(unsigned char c) { return c >= 128 ? int(c) - 256 : int(c); }
// Bit 0 of the class byte (indexed by the signed character) lowers by 0x20.
int lower(unsigned char c, const Executable &exe) {
  const int s = signed_char(c);
  return (exe.byte(static_cast<std::uint32_t>(std::int64_t(CTYPE) + s)) & 1) ? s + 0x20 : s;
}
std::string lowered(const std::string &text, const Executable &exe) {
  std::string out = text;
  for (auto &c : out)
    c = static_cast<char>(static_cast<unsigned char>(lower(static_cast<unsigned char>(c), exe)));
  return out;
}
bool prop_int_nonzero(const std::vector<std::string> *props, const std::string &key,
                      const Executable &exe) {
  const auto text = prop_find(props, key, exe);
  return text && atoi(*text, exe) != 0;
}
bool has_any_light(const std::vector<std::string> *props, const Executable &exe) {
  for (const auto key : LIGHT_KEYS)
    if (prop_int_nonzero(props, exe.string(key), exe))
      return true;
  for (int k = 0; k < LIGHT_INDICES; ++k)
    for (const auto format : LIGHT_FORMATS) {
      auto key = exe.string(format);
      const auto at = key.find("%d");
      if (at == std::string::npos)
        throw std::runtime_error("light key format has no %d");
      key.replace(at, 2, std::to_string(k));
      if (prop_int_nonzero(props, key, exe))
        return true;
    }
  return false;
}
Outcome object(std::uint32_t size, std::vector<std::uint32_t> calls) {
  Outcome out;
  out.kind = Kind::Object;
  out.size = size;
  out.calls = std::move(calls);
  return out;
}
struct CreatureRow {
  std::uint32_t test;
  std::uint8_t kind;
};
const CreatureRow kCreatureRows[] = {
#include "creature_class_table.inc"
};
} // namespace

Creature creature_kind(const Outcome &outcome) {
  if (outcome.kind != Kind::Object || outcome.test == 0 || outcome.test == LINE)
    return Creature::Unknown;
  for (const auto &row : kCreatureRows)
    if (row.test == outcome.test)
      return row.kind == 1 ? Creature::Yes : row.kind == 0 ? Creature::No : Creature::Unknown;
  return Creature::Unknown;
}

const std::vector<Row> &tests() { return kTests; }

bool str_eq(const std::string &a, const std::string &b, const Executable &exe) {
  const std::size_t n = a.size() < b.size() ? a.size() : b.size();
  for (std::size_t i = 0; i <= n; ++i) {
    const unsigned char x = i < a.size() ? static_cast<unsigned char>(a[i]) : 0;
    const unsigned char y = i < b.size() ? static_cast<unsigned char>(b[i]) : 0;
    if (lower(x, exe) != lower(y, exe))
      return false;
    if (x == 0)
      return true;
  }
  return false;
}

bool str_prefix(const std::string &text, const std::string &prefix, const Executable &exe) {
  for (std::size_t i = 0; i < prefix.size(); ++i) {
    const unsigned char have = i < text.size() ? static_cast<unsigned char>(text[i]) : 0;
    if (lower(have, exe) != lower(static_cast<unsigned char>(prefix[i]), exe))
      return false;
  }
  return true;
}

std::optional<std::string> prop_find(const std::vector<std::string> *props,
                                     const std::string &key, const Executable &exe) {
  if (!props)
    return std::nullopt;
  const auto k = lowered(key, exe);
  for (const auto &entry : *props)
    if (entry.size() > k.size() && entry.compare(0, k.size(), k) == 0 && entry[k.size()] == '=')
      return entry.substr(k.size() + 1);
  return std::nullopt;
}

std::uint32_t atoi(const std::string &text, const Executable &exe) {
  std::size_t pos = 0;
  while (pos < text.size() &&
         (exe.byte(static_cast<std::uint32_t>(std::int64_t(CTYPE) +
                                              signed_char(static_cast<unsigned char>(text[pos])))) &
          0x08))
    ++pos;
  bool negative = false;
  if (pos < text.size() && (text[pos] == '+' || text[pos] == '-'))
    negative = text[pos++] == '-';
  // newlib _strtol_r with a 64-bit long: clamp on overflow.
  constexpr std::uint64_t LIMIT = std::uint64_t(std::numeric_limits<std::int64_t>::max());
  std::uint64_t value = 0;
  bool over = false;
  for (; pos < text.size() && text[pos] >= '0' && text[pos] <= '9'; ++pos) {
    const auto digit = std::uint64_t(text[pos] - '0');
    if (!over && value > (LIMIT + (negative ? 1 : 0) - digit) / 10)
      over = true;
    if (!over)
      value = value * 10 + digit;
  }
  std::uint64_t result;
  if (over)
    result = negative ? (LIMIT + 1) : LIMIT;
  else
    result = value;
  if (negative)
    result = ~result + 1;
  return static_cast<std::uint32_t>(result);
}

Outcome classify(const std::string &name, const std::vector<std::string> *props,
                 const Executable &exe) {
  for (const auto &row : kTests) {
    const auto test = exe.string(row.name);
    const bool hit = row.prefix ? str_prefix(name, test, exe) : str_eq(name, test, exe);
    if (!hit)
      continue;
    std::vector<std::uint32_t> calls(row.calls, row.calls + row.call_count);
    if (row.clear_color) {
      Outcome out;
      out.kind = Kind::ClearColor;
      out.test = row.name;
      return out;
    }
    if (row.name == LINE) {
      const auto value = prop_find(props, exe.string(LINE_NAME_KEY), exe);
      if (value && str_eq(*value, exe.string(LINE_WAVE), exe)) {
        calls.push_back(WAVE_CTOR);
        auto out = object(WAVE_SIZE, std::move(calls));
        out.test = row.name;
        return out;
      }
    }
    if (!row.size) {
      Outcome out;
      out.kind = Kind::Call;
      out.test = row.name;
      out.calls = std::move(calls);
      return out;
    }
    auto out = object(row.size, std::move(calls));
    out.test = row.name;
    return out;
  }
  for (const auto &f : kFallbacks) {
    const auto text = prop_find(props, exe.string(f.key), exe);
    const bool passed = f.str ? text.has_value() : (text && atoi(*text, exe) != 0);
    if (!passed)
      continue;
    if (f.light && has_any_light(props, exe))
      return object(f.lit_size, {f.lit_ctor});
    return object(f.size, {f.ctor});
  }
  return {};
}

} // namespace champions::simulation::rta::level_factory
