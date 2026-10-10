#include "champions/items.hpp"
#include "champions/core_math.hpp"
#include <algorithm>
#include <limits>

// Authored port of ref_items.py at 14f2f2dd7ca5875f9d33bff4e7d17b82ebc85512.
// Tables are caller supplied; no executable, original strings, or table data
// here.
namespace champions::simulation::rta::items {
namespace {
const Fpu fixed(Fpu::Model::Ps2, false, false);
const Definition &definition(const Item &i) {
  if (!i.definition)
    throw Error("item has no definition");
  return *i.definition;
}
bool contains(const std::string &s, const std::string &part) {
  return s.find(part) != std::string::npos;
}
bool whitespace(unsigned char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' ||
         c == '\r';
}
std::uint32_t bits(std::int32_t x) { return static_cast<std::uint32_t>(x); }
std::int32_t wrapped_add(std::int32_t a, std::int32_t b) {
  return items::s32(bits(a) + bits(b));
}
void put(std::array<std::uint8_t, ITEM_SIZE> &a, std::size_t at,
         std::uint64_t value, unsigned count) {
  for (unsigned i = 0; i < count; ++i)
    a[at + i] = static_cast<std::uint8_t>(value >> (8 * i));
}
std::uint64_t read_word(const std::vector<std::uint8_t> &a, std::size_t at,
                        unsigned count) {
  std::uint64_t out = 0;
  for (unsigned i = 0; i < count; ++i)
    out |= std::uint64_t(a[at + i]) << (8 * i);
  return out;
}
} // namespace
std::int32_t s8(std::uint32_t x) {
  x &= 255;
  return x & 128 ? static_cast<std::int32_t>(x) - 256
                 : static_cast<std::int32_t>(x);
}
std::int32_t s16(std::uint32_t x) { return rta::s16(x); }
std::int32_t s32(std::uint32_t x) { return rta::s32(x); }
Raw fadd(Raw a, Raw b) { return fixed.add(a, b); }
Raw fsub(Raw a, Raw b) { return fixed.sub(a, b); }
Raw fmul(Raw a, Raw b) { return fixed.mul(a, b); }
Raw fdiv(Raw a, Raw b) { return fixed.div(a, b); }
Raw fint(std::uint32_t a) { return fixed.from_int(a); }
std::int32_t ftrunc(Raw a) { return Fpu::to_int(a); }
bool flt(Raw a, Raw b) { return Fpu::compare(a, b) < 0; }
bool fle(Raw a, Raw b) { return Fpu::compare(a, b) <= 0; }
Raw fmax(Raw a, Raw b) { return Fpu::compare(a, b) >= 0 ? a : b; }
Raw float_times_double(Raw a, DoubleRaw d) {
  if (((d >> 52) & 2047) == 2047)
    throw Unsupported("float_times_double requires a finite constant");
  return d2f(dmul(f2d(a), d));
}
Raw powf(Raw x, Raw y) {
  if (y == F_HALF && !(x & SIGN))
    throw Unsupported("item powf positive half exponent is not modeled");
  const ScopedFpu context{Fpu{Fpu::Model::Ps2, false, false}};
  return rta::powf(x, y);
}
std::shared_ptr<Definition> Tables::by_addr(std::uint32_t address) const {
  for (const auto &d : defs)
    if (d && d->addr == address)
      return d;
  for (const auto &d : presets)
    if (d && d->addr == address)
      return d;
  return nullptr;
}
std::optional<std::string> Tables::prefix_text(std::size_t i,
                                               std::int32_t category) const {
  if (i >= prefixes.size() || category < 0 || category >= 3)
    throw Error("prefix text index outside supplied table");
  return prefixes[i].texts[static_cast<std::size_t>(category)];
}
std::optional<std::string> Tables::suffix_text(std::size_t i,
                                               std::int32_t category) const {
  if (i >= suffixes.size() || category < 0 || category >= 3)
    throw Error("suffix text index outside supplied table");
  return suffixes[i].texts[static_cast<std::size_t>(category)];
}
Raw Tables::weapon_mult(std::int32_t i) const {
  if (i < 0 || static_cast<std::size_t>(i) >= weapon_multipliers.size())
    throw Error("weapon multiplier index outside supplied table");
  return weapon_multipliers[static_cast<std::size_t>(i)];
}
std::array<Raw, 3> Tables::quality_row(std::int32_t i) const {
  if (i < 0 || static_cast<std::size_t>(i) >= quality_rows.size())
    throw Error("quality index outside supplied table");
  return quality_rows[static_cast<std::size_t>(i)];
}
std::uint32_t Rng::next() {
  seed = seed * 0x19660du + 0x3c6ef35fu;
  ++draws;
  return seed >> 16;
}
Raw Game::level_times_0_4() const {
  Raw value = level;
  if (pvp_b)
    value = items::fmax(F_ONE, fsub(value, 0x40800000));
  return fmul(value, 0x3ecccccc);
}
Raw Game::level_times_difficulty() const {
  const Raw factor = difficulty == 2   ? 0x41100000
                     : difficulty == 3 ? 0x41200000
                     : difficulty == 4 ? 0x41300000
                     : difficulty == 5 ? 0x41400000
                                       : 0x41000000;
  Raw value = level;
  if (pvp_b)
    value = items::fmax(F_ONE, fsub(value, 0x40800000));
  return fmul(value, factor);
}
Raw Game::cloth_fraction() const {
  if (num_players > 0 && static_cast<std::size_t>(num_players) > classes.size())
    throw Error("player count exceeds supplied classes");
  std::uint32_t count = 0;
  for (std::int32_t i = 0; i < num_players; ++i)
    if (classes[static_cast<std::size_t>(i)] == 4)
      ++count;
  return fdiv(fint(count), fint(bits(num_players)));
}
std::uint32_t Item::variant() const {
  return static_cast<std::uint32_t>((flags >> 11) & 31);
}
std::int32_t Item::get(std::int32_t code, std::size_t which) const {
  for (const auto &m : mods)
    if (m && m->code == code) {
      if (which >= 3)
        throw Error("modifier component out of range");
      return m->values[which];
    }
  return 0;
}
std::int32_t Item::set_modifier(std::int32_t code, std::int32_t v0,
                                std::int32_t v1, std::int32_t v2) {
  for (std::size_t i = 0; i < 4; ++i)
    if (!mods[i] || mods[i]->code == code) {
      mods[i] = Modifier{code,
                         {static_cast<std::int16_t>(items::s16(bits(v0))),
                          static_cast<std::int16_t>(items::s16(bits(v1))),
                          static_cast<std::int16_t>(items::s16(bits(v2)))}};
      return static_cast<std::int32_t>(i);
    }
  return -1;
}
std::vector<std::size_t> Item::undefined_offsets() const {
  std::vector<std::size_t> out{6, 7, 0x22, 0x23, 0x44, 0x45, 0x46, 0x47};
  for (std::size_t i = 0; i < 4; ++i)
    if (!mods[i])
      for (std::size_t p = 0x26 + 8 * i; p < 0x2c + 8 * i; ++p)
        out.push_back(p);
  return out;
}
std::array<std::uint8_t, ITEM_SIZE>
Item::pack(const std::array<std::uint8_t, ITEM_SIZE> &stale) const {
  auto out = stale;
  put(out, 0, definition ? definition->addr : 0, 4);
  put(out, 4, bits(count), 2);
  put(out, 8, flags, 8);
  for (unsigned i = 0; i < 4; ++i) {
    put(out, 0x10 + 2 * i, static_cast<std::uint16_t>(a[i]), 2);
    put(out, 0x18 + 2 * i, static_cast<std::uint16_t>(b[i]), 2);
  }
  out[0x20] = prefix;
  out[0x21] = suffix;
  for (unsigned i = 0; i < 4; ++i) {
    if (!mods[i])
      put(out, 0x24 + 8 * i, 0xffff, 2);
    else {
      const auto &m = *mods[i];
      if (m.code < -32768 || m.code > 32767)
        throw Error("modifier code does not fit stored signed16");
      put(out, 0x24 + 8 * i, bits(m.code), 2);
      for (unsigned k = 0; k < 3; ++k)
        put(out, 0x26 + 8 * i + 2 * k, static_cast<std::uint16_t>(m.values[k]),
            2);
    }
  }
  return out;
}
Item Item::unpack(const Tables &t, const std::vector<std::uint8_t> &data) {
  if (data.size() < 0x44)
    throw Error("short item record");
  Item out(t.by_addr(static_cast<std::uint32_t>(read_word(data, 0, 4))));
  out.count = items::s16(static_cast<std::uint32_t>(read_word(data, 4, 2)));
  out.flags = read_word(data, 8, 8);
  for (unsigned i = 0; i < 4; ++i) {
    out.a[i] = static_cast<std::int16_t>(items::s16(
        static_cast<std::uint32_t>(read_word(data, 0x10 + 2 * i, 2))));
    out.b[i] = static_cast<std::int16_t>(items::s16(
        static_cast<std::uint32_t>(read_word(data, 0x18 + 2 * i, 2))));
  }
  out.prefix = data[0x20];
  out.suffix = data[0x21];
  for (unsigned i = 0; i < 4; ++i) {
    const auto code = items::s16(
        static_cast<std::uint32_t>(read_word(data, 0x24 + 8 * i, 2)));
    if (code != -1) {
      Modifier m;
      m.code = code;
      for (unsigned k = 0; k < 3; ++k)
        m.values[k] =
            static_cast<std::int16_t>(items::s16(static_cast<std::uint32_t>(
                read_word(data, 0x26 + 8 * i + 2 * k, 2))));
      out.mods[i] = m;
    }
  }
  return out;
}
std::int32_t encode_definition_index(const Tables &, const Item &i) {
  const auto delta = std::int64_t(definition(i).addr) - A_DEFS;
  return static_cast<std::int32_t>(delta >= 0 ? delta / 64
                                              : -((-delta + 63) / 64));
}
std::shared_ptr<Definition> decode_definition_index(const Tables &t,
                                                    std::int32_t i) {
  return t.by_addr(A_DEFS + bits(i) * 64u);
}
bool same_identity(const Item &a, const Item &b) {
  return b.definition && a.definition == b.definition && a.flags == b.flags &&
         a.prefix == b.prefix && a.suffix == b.suffix;
}
void copy_mutable(Item &dst, const Item &src) {
  dst.count = src.count;
  dst.flags = src.flags;
  dst.prefix = src.prefix;
  dst.suffix = src.suffix;
  dst.a = {};
  dst.b = {};
  dst.mods = src.mods;
}
std::int32_t resource_index(const Item &i) {
  const auto &d = definition(i);
  return wrapped_add(d.model_row, (i.flags & 0x1000000000ull)
                                      ? static_cast<std::int32_t>(i.variant())
                                      : 0);
}
ScanResult scan(const std::string &fmt, const std::string &text) {
  ScanResult out;
  std::size_t p = 0, f = 0;
  const auto finish = [&](bool eof) {
    out.result = out.values.empty() && eof
                     ? -1
                     : static_cast<std::int32_t>(out.values.size());
    return out;
  };
  while (f < fmt.size()) {
    const auto c = static_cast<unsigned char>(fmt[f]);
    if (whitespace(c)) {
      ++f;
      while (p < text.size() && whitespace(static_cast<unsigned char>(text[p])))
        ++p;
      continue;
    }
    if (c != '%') {
      if (p >= text.size())
        return finish(true);
      if (text[p] != fmt[f])
        return finish(false);
      ++p;
      ++f;
      continue;
    }
    if (f + 1 >= fmt.size() || fmt[f + 1] != 'd')
      throw Error("scanner supports only decimal conversions");
    f += 2;
    if (p >= text.size())
      return finish(true);
    while (p < text.size() && whitespace(static_cast<unsigned char>(text[p])))
      ++p;
    if (p >= text.size())
      return finish(true);
    bool negative = false;
    if (text[p] == '+' || text[p] == '-') {
      negative = text[p] == '-';
      ++p;
    }
    const auto digits = p;
    std::uint64_t value = 0;
    const auto limit =
        negative ? (std::uint64_t{1} << 63) : std::uint64_t(INT64_MAX);
    while (p < text.size() && text[p] >= '0' && text[p] <= '9') {
      const auto digit = static_cast<unsigned>(text[p] - '0');
      value = value > (limit - digit) / 10 ? limit : value * 10 + digit;
      ++p;
    }
    if (p == digits)
      return finish(false);
    const auto low = static_cast<std::uint32_t>(value);
    out.values.push_back(items::s32(negative ? 0u - low : low));
  }
  return finish(false);
}
void randomize_variant(Item &i, Rng &rng) {
  const auto count = (definition(i).flags >> 11) & 31;
  const auto variant =
      count ? rng.next() % static_cast<std::uint32_t>(count) : 0;
  i.flags = (std::uint64_t(variant) << 11) | (i.flags & 0xffffffffffff07ffull);
}
std::shared_ptr<Definition>
find_definition(const Tables &t, const std::string &name, bool presets) {
  const auto &rows = presets ? t.presets : t.defs;
  if (rows.empty())
    throw Error("definition table is empty");
  for (const auto &d : rows) {
    if (!d)
      throw Error("null supplied definition");
    if (d->name == name)
      return d;
  }
  return rows[0];
}
Item parse_descriptor(const Tables &t, std::string text, std::int32_t variant,
                      bool use_presets, Rng *rng) {
  text.resize(std::min(text.size(), text.find('\0')));
  if (text.size() >= DESCRIPTOR_BUFFER)
    throw Error("descriptor exceeds modeled buffer");
  const auto plus = text.find('+');
  auto d = find_definition(t, use_presets ? text : text.substr(0, plus),
                           use_presets);
  Item item(d);
  item.count = d->count;
  item.flags = d->flags;
  std::size_t start = plus;
  while (start != std::string::npos) {
    const auto end = text.find('+', start + 1);
    const auto token = text.substr(
        start + 1, end == std::string::npos ? end : end - start - 1);
    for (const auto &format : t.formats) {
      auto parsed = scan(format.text, token);
      if (parsed.result == 0)
        continue;
      parsed.values.resize(3, 0);
      if (!format.code)
        variant = parsed.values[0];
      else {
        std::size_t wanted = 0, at = 0;
        while ((at = format.text.find("%d", at)) != std::string::npos) {
          ++wanted;
          at += 2;
        }
        item.set_modifier(*format.code, parsed.values[0],
                          wanted > 1 ? parsed.values[1] : 0,
                          wanted > 2 ? parsed.values[2] : 0);
      }
      break;
    }
    start = end;
  }
  if (variant == -1) {
    if (((d->flags >> 11) & 31) && !rng)
      throw Error("variant draw requires supplied RNG");
    if (rng)
      randomize_variant(item, *rng);
    else
      item.flags &= 0xffffffffffff07ffull;
  } else
    item.flags = static_cast<std::uint64_t>(static_cast<std::int64_t>(
                     items::s32(bits(variant) << 11))) |
                 (item.flags & 0xffffffffffff07ffull);
  return item;
}
Raw item_weight(const Item &i) {
  const auto &d = definition(i);
  Raw w = d.weight;
  const Raw limit = d.category == 1 ? 0x40a00000 : 0x41700000;
  if (flt(limit, w))
    w = fsub(w, fmul(fsub(w, limit), 0x3f44ec4f));
  w = fmul(w, fint(bits(i.count)));
  return fadd(fmul(fmul(fint(bits(i.get(0x24))), F_HUNDREDTH), w), w);
}
std::int32_t item_armor(const Item &i) {
  auto a = definition(i).armor;
  const auto p = i.get(0x0d);
  if (p)
    a = wrapped_add(
        a, ftrunc(fmul(fint(bits(a)), fmul(fint(bits(p)), F_HUNDREDTH))));
  return a;
}
std::int32_t item_attack_rating(const Item &i) {
  Raw a = fint(bits(definition(i).attack));
  const auto p = i.get(0x23);
  if (p)
    a = fadd(fmul(fmul(fint(bits(p)), F_HUNDREDTH), a), a);
  return ftrunc(a);
}
std::pair<std::int32_t, std::int32_t> item_damage_range(const Item &i,
                                                        bool elemental) {
  const auto &d = definition(i);
  Raw hi = fint(bits(d.damage_max)), lo = fint(bits(d.damage_min));
  const auto p = i.get(0x23);
  if (p) {
    const Raw factor = fadd(fmul(fint(bits(p)), F_HUNDREDTH), F_ONE);
    hi = fmul(hi, factor);
    lo = fmul(lo, factor);
  }
  if (elemental)
    for (auto code : {0x15, 0x16, 0x18, 0x17, 0x26})
      if (i.get(code)) {
        lo = fadd(lo, fint(bits(i.get(code))));
        hi = fadd(hi, fint(bits(i.get(code, 1))));
      }
  return {ftrunc(fint(bits(ftrunc(fadd(fmul(lo, F_ONE), F_HALF))))),
          ftrunc(fint(bits(ftrunc(fadd(fmul(hi, F_ONE), F_HALF)))))};
}
namespace {
Raw weapon_base(const Tables &t, const Item &i) {
  const auto range = item_damage_range(i, false);
  Raw lo = fmul(fint(bits(range.first)), 0x3f199913),
      hi = fmul(fint(bits(range.second)), 0x3f199913);
  for (const auto &m : i.mods) {
    const auto code = m ? m->code : -1;
    Raw flat = 0, burst = 0;
    switch (code) {
    case 0x15:
      flat = 0x3f028fb0;
      break;
    case 0x16:
      flat = 0x3f051ec9;
      break;
    case 0x17:
      flat = 0x3f028f4b;
      break;
    case 0x18:
      flat = 0x3effffde;
      break;
    case 0x26:
      flat = F_HALF;
      break;
    case 0x1f:
      burst = 0x3f70621b;
      break;
    case 0x20:
      burst = 0x3f743914;
      break;
    case 0x22:
      burst = 0x3f826e6d;
      break;
    default:
      break;
    }
    if (code == 0x13) {
      hi = fmul(hi, 0x3f666666);
      lo = fmul(lo, 0x3f666666);
    } else if (code == 0x14) {
      const Raw speed = fmul(fint(bits(i.get(code))), 0x3d3851eb);
      lo = fadd(fmul(speed, lo), lo);
      hi = fadd(fmul(speed, hi), hi);
    } else if (flat) {
      if (i.get(code, 2) <= 0) {
        lo = fadd(lo, fmul(fint(bits(i.get(code))), flat));
        hi = fadd(hi, fmul(fint(bits(i.get(code, 1))), flat));
      }
    } else if (code == 0x19) {
      lo = fadd(lo, fmul(fint(bits(i.get(code))), 0x3e0a3d70));
      hi = fadd(hi, fmul(fint(bits(i.get(code, 1))), 0x3e0a3d70));
    } else if (code == 0x1b) {
      lo = fadd(lo, fmul(fmul(fint(bits(i.get(code))), 0x3fa2d0aa),
                         fmul(lo, F_HUNDREDTH)));
      hi = fadd(hi, fmul(fmul(fint(bits(i.get(code, 1))), 0x3fa2d0aa),
                         fmul(hi, F_HUNDREDTH)));
    } else if (code == 0x1d) {
      Raw chance = fmul(fint(bits(i.get(code))), F_HUNDREDTH);
      lo = fadd(lo, fmul(fmul(fadd(chance, chance), lo), 0x3fd997bb));
      chance = fmul(fint(bits(i.get(code, 1))), F_HUNDREDTH);
      hi = fadd(hi, fmul(fmul(fadd(chance, chance), hi), 0x3fd997bb));
    } else if (burst) {
      const Raw chance = fmul(fint(bits(i.get(code))), F_HUNDREDTH);
      lo = fadd(lo, fmul(fmul(fint(bits(i.get(code, 1))), chance), burst));
      hi = fadd(hi, fmul(fmul(fint(bits(i.get(code, 2))), chance), burst));
    } else if (code == 0x21) {
      const Raw term = fmul(fmul(fint(bits(i.get(code, 1))),
                                 fmul(fint(bits(i.get(code))), F_HUNDREDTH)),
                            0x42540011);
      lo = fadd(lo, term);
      hi = fadd(hi, term);
    }
  }
  const Raw mult = t.weapon_mult(definition(i).subtype);
  hi = fmul(hi, mult);
  lo = fmul(lo, mult);
  return fsub(fmul(fadd(lo, hi), F_HALF), fmul(fsub(hi, lo), 0x3dcccccc));
}
Raw value_exponent(std::int32_t c) {
  switch (c) {
  case 0:
    return 0x3fc03e18;
  case 1:
    return 0x3fcccaab;
  case 2:
    return 0x3fb8d299;
  case 3:
    return 0x3fb3322f;
  case 4:
    return 0x3faf5b9a;
  case 5:
    return 0x3fd31105;
  case 6:
    return 0x3fd2d056;
  case 7:
    return 0x3ff998b7;
  case 8:
    return 0x3ff623d9;
  case 9:
    return 0x3ffccc57;
  case 10:
    return 0x40001027;
  case 11:
    return 0x3fdb8383;
  case 12:
    return 0x3fdba3d2;
  case 13:
    return 0x3f4105f2;
  case 26:
    return 0x3fff9d4d;
  case 28:
    return 0x4009ba3c;
  case 30:
    return 0x3ffdd263;
  case 42:
    return 0x402ccd29;
  default:
    return 0;
  }
}
} // namespace
std::int32_t item_value(const Tables &t, const Item &i) {
  const auto &d = definition(i);
  if (d.price >= 1)
    return items::s32(bits(d.price) * bits(i.count));
  Raw base = 0;
  if (d.category == 0) {
    if (d.subtype != 5)
      base = weapon_base(t, i);
  } else if (d.category == 1) {
    base = fmul(fmul(fint(bits(item_armor(i))), 0x3f199999), 0x3f8ccc5f);
    if (d.slot == 3)
      base = fmul(base, F_HALF);
  } else if (d.category == 2)
    base = 0x40a00000;
  Raw total = items::powf(base, 0x3ff99999);
  for (const auto &m : i.mods) {
    auto code = m ? m->code : -1;
    if (code == 0x0d && d.category == 1)
      code = 0x1a;
    const Raw exponent = value_exponent(code);
    if (exponent)
      total = fadd(total, items::powf(fint(bits(i.get(code))), exponent));
    else if (code == 0x25)
      total =
          fadd(fmul(fmul(fint(bits(i.get(code))), F_HUNDREDTH), total), total);
  }
  total = fmul(total, 0x42200000);
  if (contains(d.name, t.thrown_stem) || (d.flags & 0x1c0000000000ull))
    total = fmul(total, 0x3b83126e);
  if (d.category == 2 && (d.slot == 7 || d.slot == 5 || d.slot == 6 ||
                          d.slot == 14 || d.slot == 15))
    total = fmul(total, 0x41002899);
  return items::s32(bits(ftrunc(fadd(total, F_HALF))) * bits(i.count));
}
std::int32_t required_level_for_value(std::int32_t value) {
  auto excess = items::s32(bits(value) - 1200);
  if (excess < 0)
    excess = 0;
  const auto level = items::s32(
      bits(ftrunc(items::powf(fint(static_cast<std::uint32_t>(excess / 5000)),
                              0x3f28f5c2))) *
          2 +
      1);
  return level < 81 ? level : 80;
}
std::int32_t Player::skill_rank(std::int32_t id) const {
  const auto at = skills.find(id);
  return at == skills.end() ? 0 : at->second;
}
std::int32_t equip_restriction(const Tables &t, const Player &p,
                               const Item &i) {
  const auto &d = definition(i);
  const auto cls = p.class_id;
  std::int32_t reason = 0;
  if (d.category == 1) {
    if (i.flags & 0x80000)
      reason = cls != 4;
    else if (!contains(d.name, t.circlet_stem_equip) && cls == 4)
      reason = 1;
  } else if (cls == 1)
    reason = d.subtype == 3 || d.subtype == 4 || d.damage_bits == 8 ||
             d.damage_bits == 4;
  else if (cls == 3)
    reason = d.subtype == 1 || d.subtype == 2 || d.subtype == 3;
  else if (cls == 4) {
    reason = d.subtype == 1 || d.subtype == 2 || d.subtype == 4 ||
             d.damage_bits == 8 || d.damage_bits == 4;
    if (i.flags & 0x1000000000ull)
      reason = 0;
    if ((d.subtype == 0 || (i.flags & 0x80000000000ull)) &&
        p.skill_rank(0x5c) != 0)
      reason = 0;
  } else if (cls == 6)
    reason = d.subtype == 1 || d.subtype == 4;
  if (items::s8(bits(p.level)) < i.get(0x0e))
    reason = EQUIP_LEVEL;
  if (p.strength < i.get(0x0f))
    reason = EQUIP_STRENGTH;
  if (p.intelligence < i.get(0x10))
    reason = EQUIP_INTELLIGENCE;
  if (p.dexterity < i.get(0x11))
    reason = EQUIP_DEXTERITY;
  if (p.stamina < i.get(0x12))
    reason = EQUIP_STAMINA;
  if (items::s16(bits(p.skill_rank(0x3c))) < i.get(0x27))
    reason = EQUIP_BLUNT;
  if (items::s16(bits(p.skill_rank(0x3d))) < i.get(0x28))
    reason = EQUIP_SLASHING;
  if (i.flags & 0x20000000000ull)
    reason = EQUIP_FLAG_41;
  const auto wanted = i.get(0x13);
  if (wanted && cls != wanted - 1)
    reason = EQUIP_CLASS;
  return reason;
}
std::uint64_t load_resources(const Tables &t, Rng &rng) {
  const auto before = rng.draws;
  for (const auto &d : t.defs) {
    if (!d)
      throw Error("null supplied definition");
    parse_descriptor(t, d->name, -1, false, &rng);
  }
  return rng.draws - before;
}
} // namespace champions::simulation::rta::items
