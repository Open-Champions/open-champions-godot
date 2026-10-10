#include "champions/combat.hpp"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <type_traits>

namespace c = champions::simulation::rta::combat;
namespace r = champions::simulation::rta;
using U = std::uint64_t;
using I = std::int64_t;
void require(bool ok, const char *text) {
  if (!ok)
    throw std::runtime_error(text);
}
std::string number(U n) {
  std::ostringstream out;
  out << std::hex << std::setfill('0') << std::setw(16) << n;
  return out.str();
}
std::vector<std::string> split(const std::string &s, char sep) {
  std::vector<std::string> out;
  std::size_t p = 0;
  for (;;) {
    auto q = s.find(sep, p);
    out.push_back(s.substr(p, q - p));
    if (q == s.npos)
      break;
    p = q + 1;
  }
  return out;
}
I signed64(U x) {
  return x <= INT64_MAX ? static_cast<I>(x) : -1 - static_cast<I>(~x);
}
std::vector<U> numbers(const std::string &s) {
  std::vector<U> out;
  if (s == "-")
    return out;
  for (auto &x : split(s, ',')) {
    require(x.size() == 16, "numeric width");
    std::size_t end = 0;
    auto n = std::stoull(x, &end, 16);
    require(end == 16, "invalid number");
    out.push_back(n);
  }
  return out;
}
std::string nums(const std::vector<U> &v) {
  std::string out;
  for (auto x : v) {
    if (!out.empty())
      out += ',';
    out += number(x);
  }
  return out.empty() ? "-" : out;
}
template <class T> std::string nums(const std::vector<T> &v) {
  std::vector<U> a;
  for (auto x : v)
    a.push_back(static_cast<U>(x));
  return nums(a);
}
struct Refs {
  c::Obj a, b;
  c::Spell s, t;
  c::ItemDef d;
  c::Item i, j;
};
std::string enc(const c::Text &t, const Refs &) {
  std::ostringstream out;
  out << 'S' << std::hex << std::setfill('0');
  bool first = true;
  for (auto x : t) {
    if (!first)
      out << ',';
    first = false;
    out << std::setw(8) << static_cast<std::uint32_t>(x);
  }
  return out.str();
}
std::string enc(bool x, const Refs &) { return x ? "T" : "F"; }
template <class T, std::enable_if_t<std::is_integral<T>::value &&
                                        !std::is_same<T, bool>::value,
                                    int> = 0>
std::string enc(T x, const Refs &) {
  return number(static_cast<U>(x));
}
std::string enc(c::Obj *x, const Refs &r) {
  return !x ? "N" : x == &r.a ? "@1" : x == &r.b ? "@2" : "BAD";
}
std::string enc(c::Spell *x, const Refs &r) {
  return !x ? "N" : x == &r.s ? "@3" : x == &r.t ? "@4" : "BAD";
}
std::string enc(c::ItemDef *x, const Refs &r) {
  return !x ? "N" : x == &r.d ? "@5" : "BAD";
}
std::string enc(c::Item *x, const Refs &r) {
  return !x ? "N" : x == &r.i ? "@6" : x == &r.j ? "@7" : "BAD";
}
template <class A, class B>
std::string enc(const std::pair<A, B> &x, const Refs &r) {
  return '[' + enc(x.first, r) + ';' + enc(x.second, r) + ']';
}
template <class T> std::string enc(const std::vector<T> &x, const Refs &r) {
  std::string out = "[";
  for (std::size_t i = 0; i < x.size(); ++i) {
    if (i)
      out += ';';
    out += enc(x[i], r);
  }
  return out + ']';
}
std::string enc(const c::EventValue &x, const Refs &refs) {
  return std::visit(
      [&](const auto &v) -> std::string {
        using V = std::decay_t<decltype(v)>;
        if constexpr (std::is_same<V, std::monostate>::value)
          return "N";
        else
          return enc(v, refs);
      },
      x.value);
}
// Event vectors call this after the EventValue overload is visible.
std::string events(const std::vector<c::Event> &list, const Refs &refs) {
  std::string out = "[";
  for (std::size_t i = 0; i < list.size(); ++i) {
    if (i)
      out += ';';
    out += '[';
    for (std::size_t j = 0; j < list[i].size(); ++j) {
      if (j)
        out += ';';
      out += enc(list[i][j], refs);
    }
    out += ']';
  }
  return out + ']';
}
#define F(field) f(#field, ++k, o.field)
template <class Fn> void fields(c::Spell &o, Fn f) {
  unsigned k = 0;
  F(kind);
  F(skill);
  F(level);
  F(owner);
  F(f18);
  F(f1c);
  F(f20);
  F(f2c);
}
template <class Fn> void fields(c::Obj &o, Fn f) {
  unsigned k = 0;
  F(tag);
  F(flags);
  F(pos);
  F(facing);
  F(owner_link);
  F(owner);
  F(cls);
  F(innoruuk_immune);
  F(res);
  F(flash);
  F(cflags);
  F(kind);
  F(dot);
  F(field_dc);
  F(armor);
  F(stamp_f0);
  F(hp);
  F(max_hp);
  F(status);
  F(poison_a);
  F(poison_b);
  F(status_source);
  F(spells);
  F(counter2);
  F(counter4);
  F(counter11);
  F(counter12);
  F(slot);
  F(state);
  F(attack_start);
  F(attack_chain);
  F(bonus);
  F(model_points);
  F(boss_rank);
  F(exp_value);
  F(own_seed);
  F(last_attacker);
}
template <class Fn> void fields(c::ItemDef &o, Fn f) {
  unsigned k = 0;
  F(name);
  F(dmg_min);
  F(dmg_max);
  F(armor);
  F(attack);
  F(category);
  F(subtype);
  F(dmg_type);
  F(flags);
}
template <class Fn> void fields(c::Item &o, Fn f) {
  unsigned k = 0;
  F(defn);
  F(count);
  F(flags);
  F(mods);
}
template <class Fn> void fields(c::PlayerRec &o, Fn f) {
  unsigned k = 0;
  F(cls);
  F(level58);
  F(level59);
  F(exp);
  F(base);
  F(eff);
  F(hp);
  F(max_hp);
  F(mana);
  F(max_mana);
  F(attack);
  F(dmg_min);
  F(dmg_max);
  F(stagger);
  F(skill_points);
  F(stat_points);
  F(passives);
  F(load_limit);
  F(bag2);
  F(obj);
  F(equip);
  F(weapon_sets);
  F(second_weapon);
  F(active_set);
  F(skills);
}
#undef F
template <class T> struct IsVector : std::false_type {};
template <class T> struct IsVector<std::vector<T>> : std::true_type {};
template <class T> std::string snapshot(T &obj, const Refs &refs) {
  std::string out = "[";
  fields(obj, [&](const char *, unsigned k, const auto &v) {
    if (k > 1)
      out += ';';
    out += enc(v, refs);
  });
  return out + ']';
}
template <class T> void fill(T &obj, std::uint32_t seed, Refs &refs) {
  auto n = [&](unsigned k) { return r::s32(seed + k * 0x9e3779b9u); };
  fields(obj, [&](const char *, unsigned k, auto &v) {
    using V = std::decay_t<decltype(v)>;
    if constexpr (std::is_same<V, c::Text>::value)
      v = U"Test" + c::Text(1, static_cast<char32_t>(0x3a3 + k)) +
          c::Text(1, static_cast<char32_t>(0xff + k));
    else if constexpr (std::is_same<V, bool>::value)
      v = bool(seed & 2);
    else if constexpr (std::is_same<V, c::Obj *>::value)
      v = (seed & 1) ? nullptr : (k % 2 ? &refs.b : &refs.a);
    else if constexpr (std::is_same<V, c::ItemDef *>::value)
      v = (seed & 1) ? nullptr : &refs.d;
    else if constexpr (std::is_same<V, c::Item *>::value)
      v = (seed & 1) ? nullptr : &refs.i;
    else if constexpr (std::is_same<V, std::vector<c::Spell *>>::value)
      v = {&refs.s, &refs.t, &refs.s};
    else if constexpr (std::is_same<V, std::vector<c::Item *>>::value)
      v = {&refs.i, nullptr, &refs.j};
    else if constexpr (std::is_same<V, c::Descriptor>::value)
      v = {{n(k), n(k + 1), n(k + 2), n(k + 3)}, {-1, 0, 0, 0}};
    else if constexpr (std::is_same<V, std::vector<std::pair<I, I>>>::value)
      v = {{n(k), n(k + 1)}, {-1, 5}, {2, 8}};
    else if constexpr (IsVector<V>::value) {
      v.clear();
      for (unsigned i = 0; i < (static_cast<U>(seed) + k) % 6; ++i)
        v.push_back(static_cast<typename V::value_type>(n(k + i)));
    } else if constexpr (std::is_same<V, U>::value)
      v = (static_cast<U>(seed) << 32) | static_cast<std::uint32_t>(n(k));
    else
      v = static_cast<V>(n(k));
  });
}
template <class T>
std::string record(bool copying, std::uint32_t seed, std::string &state) {
  Refs refs;
  T obj;
  if (!copying)
    return snapshot(obj, refs);
  fill(obj, seed, refs);
  auto copy = obj.copy();
  auto before = snapshot(copy, refs);
  fields(copy, [&](const char *, unsigned, auto &v) {
    if constexpr (IsVector<std::decay_t<decltype(v)>>::value)
      v.clear();
  });
  state = snapshot(obj, refs) + '|' + snapshot(copy, refs);
  return before;
}
std::string invoke(const std::string &op, const std::string &args,
                   std::string &state) {
  auto parts = split(args, '|');
  auto a = numbers(parts[0]);
  require(!a.empty() && a[0] < 16, "arithmetic configuration");
  const unsigned config = static_cast<unsigned>(a[0]);
  c::set_fpu(static_cast<c::Fpu::Model>(config / 4), bool(config & 2),
             bool(config & 1));
  auto x = [&](std::size_t i) {
    require(i < a.size(), "missing argument");
    return static_cast<std::uint32_t>(a[i]);
  };
  auto si = [&](std::size_t i) {
    require(i < a.size(), "missing argument");
    return signed64(a[i]);
  };
  auto out = [](auto n) { return number(static_cast<U>(n)); };
  if (op == "s8")
    return out(c::s8(x(1)));
  if (op == "s16" || op == "signed16")
    return out(c::s16(x(1)));
  if (op == "s32")
    return out(c::s32(x(1)));
  if (op == "itof")
    return out(c::itof(si(1)));
  if (op == "ftoi")
    return out(c::ftoi(x(1)));
  if (op == "fsqrt")
    return out(c::fsqrt(x(1)));
  if (op == "sqrtf")
    return out(c::sqrtf(x(1)));
  if (op == "round_half_away")
    return out(c::round_half_away(x(1)));
  if (op == "cos16")
    return out(c::cos16(x(1)));
  if (op == "sin16")
    return out(c::sin16(x(1)));
  if (op == "fadd")
    return out(c::fadd(x(1), x(2)));
  if (op == "fsub")
    return out(c::fsub(x(1), x(2)));
  if (op == "fmul")
    return out(c::fmul(x(1), x(2)));
  if (op == "fdiv")
    return out(c::fdiv(x(1), x(2)));
  if (op == "flt")
    return out(c::flt(x(1), x(2)));
  if (op == "fle")
    return out(c::fle(x(1), x(2)));
  if (op == "feq")
    return out(c::feq(x(1), x(2)));
  if (op == "fmin")
    return out(c::fmin(x(1), x(2)));
  if (op == "fmax")
    return out(c::fmax(x(1), x(2)));
  if (op == "powf")
    return out(c::powf(x(1), x(2)));
  if (op == "scalbnf")
    return out(c::scalbnf(x(1), r::s32(x(2))));
  if (op == "angles")
    return nums(std::vector<U>{c::cos16(x(1)), c::sin16(x(1))});
  if (op == "c_rem")
    return out(c::c_rem(r::s32(x(1)), r::s32(x(2))));
  if (op == "dbl_from_int")
    return out(c::dbl_from_int(si(1)));
  if (op == "dbl_from_unsigned")
    return out(c::dbl_from_unsigned(x(1)));
  if (op == "dbl_from_single")
    return out(c::dbl_from_single(x(1)));
  if (op == "dbl_add")
    return out(c::dbl_add(a.at(1), a.at(2)));
  if (op == "dbl_mul")
    return out(c::dbl_mul(a.at(1), a.at(2)));
  if (op == "dbl_fmod")
    return out(c::dbl_fmod(a.at(1), a.at(2)));
  if (op == "dbl_to_single")
    return out(c::dbl_to_single(a.at(1)));
  if (op == "index")
    return out(c::index(static_cast<std::size_t>(a.at(1)), si(2)));
  if (op == "seed" || op == "sphere") {
    c::Seed seed(x(1));
    std::vector<U> result;
    if (op == "seed")
      for (unsigned i = 0; i < 32; ++i)
        result.push_back(seed.draw());
    else
      for (unsigned i = 0; i < 4; ++i)
        for (auto v : seed.in_unit_sphere())
          result.push_back(v);
    std::vector<U> trace{seed.state};
    trace.insert(trace.end(), seed.draws.begin(), seed.draws.end());
    state = nums(trace);
    return nums(result);
  }
  if (op == "descriptor") {
    c::Descriptor desc;
    for (std::size_t i = 1; i < parts.size(); ++i) {
      std::vector<I> row;
      for (auto value : numbers(parts[i]))
        row.push_back(signed64(value));
      desc.push_back(row);
    }
    return out(c::desc_get(desc, si(1), si(2)));
  }
  if (op == "rank") {
    c::PlayerRec p;
    p.skills = x(1) ? std::vector<std::pair<I, I>>{{1, 2},
                                                   {0x10001, 0x1ffff},
                                                   {-1, 55},
                                                   {2, 99}}
                    : std::vector<std::pair<I, I>>{{2, 3}, {1, 0xffff}};
    return out(p.rank(si(2)));
  }
  if (op == "defaults" || op == "copy") {
    const bool cp = op == "copy";
    const auto seed = cp ? x(2) : 0;
    switch (x(1)) {
    case 0:
      return record<c::Spell>(cp, seed, state);
    case 1:
      return record<c::Obj>(cp, seed, state);
    case 2:
      return record<c::ItemDef>(cp, seed, state);
    case 3:
      return record<c::Item>(cp, seed, state);
    case 4:
      return record<c::PlayerRec>(cp, seed, state);
    default:
      throw std::runtime_error("record kind");
    }
  }
  if (op == "spell") {
    Refs refs;
    refs.s.kind = U"A";
    refs.t.kind = U"B";
    c::Spell last;
    last.kind = U"A";
    c::Obj o;
    if (si(1) > 0)
      o.spells.push_back(&refs.s);
    if (si(1) > 1)
      o.spells.push_back(&refs.t);
    if (si(1) > 2)
      o.spells.push_back(&last);
    c::Text wanted;
    for (char v : parts.at(1))
      wanted.push_back(static_cast<unsigned char>(v));
    auto *p = o.spell(wanted);
    return out(!p ? 0 : p == &refs.s ? 1 : p == &refs.t ? 2 : 3);
  }
  if (op == "world") {
    const auto i = x(1);
    c::World w(i, ~i);
    Refs refs;
    refs.a.tag = U"victim";
    refs.b.tag = U"attacker";
    w.stat_fn = [](I a, I b, I c) { return INT64_C(0x100000000) + a - b + c; };
    w.stat_for_fn = [&](I a, I b, I c, c::Obj *o, I d) {
      return INT64_C(0xffffffff) + a + b + c + d + (o == &refs.b ? 7 : 0);
    };
    w.adjust_fn = [](I a, I b, I c) { return INT64_C(0x80000000) + a + b + c; };
    w.pool_fn = [](I size) { return size % 2 == 0; };
    std::vector<U> result{static_cast<U>(w.stat(i, 2, 3)),
                          static_cast<U>(w.stat_for(1, 2, 3, &refs.b, 4)),
                          static_cast<U>(w.adjust(i, 3, 4)),
                          static_cast<U>(w.pool(i))};
    w.take_damage_fn = [&](c::Obj &v, r::Raw amount, c::Obj *a,
                           std::uint32_t flags) {
      v.hp = amount;
      v.last_attacker = a;
      w.log("callback", v.tag, flags);
      w.seed.draw();
    };
    w.take_damage(refs.a, 0x3f800000u + i, (i & 1) ? nullptr : &refs.b,
                  0xf0000000u + i);
    std::vector<U> trace{refs.a.hp, refs.a.last_attacker ? 2u : 0u,
                         w.seed.state};
    trace.insert(trace.end(), w.seed.draws.begin(), w.seed.draws.end());
    state = events(w.events, refs) + '|' + nums(trace);
    return nums(result);
  }
  throw std::runtime_error("unknown operation: " + op);
}
void anchors() {
  c::set_fpu();
  require(c::c_rem(INT32_MIN, -1) == 0, "signed division overflow case");
  require(c::c_rem(-7, 0) == -7, "zero remainder rule");
  require(c::dbl_from_single(0x80000000) == 0 &&
              c::dbl_to_single(r::D_SIGN) == 0,
          "Fraction canonical zero");
  require(c::dbl_to_single(c::NAN_VALUE) == 0x7f800000u,
          "Fraction NAN conversion");
  require(c::dbl_from_unsigned(0xffffffffu) == UINT64_C(0x41efffffffe00000),
          "unsigned direct draw conversion");
  require(
      c::dbl_add(UINT64_C(0x3bf8f82720000000), UINT64_C(0xc26cf51fa0000000)) ==
          UINT64_C(0xc26cf51f9fffffff),
      "Fraction addition retains opposite-sign tiny borrow");
  require(c::sqrtf(0x40800000) == 0x40000000 &&
              c::powf(0x40800000, r::HALF) == 0x40000000,
          "combat permits half-power");
  for (auto x : {UINT64_C(1), UINT64_C(0x7ff0000000000000)}) {
    bool fail = false;
    try {
      c::dbl_to_single(x);
    } catch (const c::Unsupported &) {
      fail = true;
    }
    require(fail, "unsupported double domain");
  }
  bool overflow = false;
  try {
    c::dbl_mul(UINT64_C(0x7fefffffffffffff), UINT64_C(0x4000000000000000));
  } catch (const c::Unsupported &) {
    overflow = true;
  }
  require(overflow, "double overflow explicit");
  c::World world;
  world.players.emplace_back();
  auto *p = &world.players.front();
  world.players.emplace_back();
  require(p == &world.players.front(), "appending player invalidated identity");
  c::Spell spell;
  spell.kind = U"active";
  auto *owned = world.add_owned_spell(spell);
  c::Obj obj;
  obj.spells = {owned};
  require(obj.spell(U"active") == owned, "owned spell identity");
  auto copy = obj.copy();
  copy.spells.clear();
  require(obj.spells.size() == 1, "copied list ownership");
  owned->f18 = 7;
  require(obj.spell(U"active")->f18 == 7, "linked spell mutation");
  obj.spells = {nullptr};
  bool null = false;
  try {
    obj.spell(U"x");
  } catch (const c::Error &) {
    null = true;
  }
  require(null, "null spell reference");
  c::Item item;
  item.mods = {{0x10001, 0xffff}};
  require(item.mod(1) == -1, "modifier s16 wrap");
  require(
      world.stat(1, 2, 3) == 0 && world.stat_for(1, 2, 3, nullptr, 4) == 0 &&
          world.adjust(1, 7, 3) == 7 && world.enemy_fn(nullptr, nullptr) == 1,
      "callback defaults");
  require(world.seed.draws.empty() && world.cosmetic.draws.empty(),
          "world constructor drew RNG");
  world.stat_fn = {};
  bool callback = false;
  try {
    world.stat(0, 0, 0);
  } catch (const c::Error &) {
    callback = true;
  }
  require(callback, "missing callback guard");
  c::World throwing;
  c::Obj target;
  throwing.take_damage_fn = [](c::Obj &, r::Raw, c::Obj *, std::uint32_t) {
    throw c::Error("authored callback");
  };
  bool threw = false;
  try {
    throwing.take_damage(target, r::ONE, nullptr, 7);
  } catch (const c::Error &) {
    threw = true;
  }
  require(threw && throwing.events.size() == 1,
          "callback throw preserves earlier modeled event");
  c::World growing;
  growing.players.emplace_back();
  auto *active = &growing.players.front();
  growing.take_damage_fn = [&](c::Obj &, r::Raw, c::Obj *, std::uint32_t) {
    for (unsigned i = 0; i < 1000; ++i)
      growing.players.emplace_back();
    active->hp = r::ONE;
  };
  growing.take_damage(target, 0, nullptr, 0);
  require(growing.players.front().hp == r::ONE,
          "callback append preserves player state identity");
  c::Tables tables;
  require(tables.exp_table.size() == 81 && tables.class_formulas.size() == 7 &&
              tables.passive_masks.size() == 7 &&
              tables.passive_masks[0].size() == 48,
          "table dimensions");
  require(c::at(tables.exp_table, -1) == 0, "negative list index");
  bool bounds = false;
  try {
    c::index(2, INT64_MIN);
  } catch (const c::Error &) {
    bounds = true;
  }
  require(bounds, "negative extreme index");
}
int main(int argc, char **argv) {
  try {
    require(argc == 2 || argc == 3,
            "usage: combat-common-tests vectors.tsv [trace.tsv]");
    anchors();
    std::ifstream input(argv[1]);
    require(bool(input), "vector open");
    std::ofstream trace;
    if (argc == 3) {
      trace.open(argv[2], std::ios::binary);
      require(bool(trace), "trace open");
    }
    std::string line;
    std::size_t rows = 0, errors = 0;
    std::map<std::string, std::size_t> counts;
    std::vector<bool> angles(65536), signed16(65536);
    U digest = 1469598103934665603ull;
    while (std::getline(input, line)) {
      if (!line.empty() && line.back() == '\r')
        line.pop_back();
      if (line.empty() || line[0] == '#')
        continue;
      auto f = split(line, '\t');
      require(f.size() == 5 && f[0] == "combat-common", "vector schema");
      std::string result, state = "-";
      try {
        result = invoke(f[1], f[2], state);
      } catch (const c::Unsupported &) {
        result = "unsupported";
        ++errors;
      } catch (const c::Error &) {
        result = "error";
        ++errors;
      }
      if (result != f[3] || state != f[4]) {
        std::cerr << "Mismatch row " << rows + 1 << " " << f[1] << " args "
                  << f[2] << "\nexpected " << f[3] << " state " << f[4]
                  << "\nactual " << result << " state " << state << '\n';
        return 1;
      }
      const auto record = f[0] + "\t" + f[1] + "\t" + f[2] + "\t" + result +
                          "\t" + state + "\n";
      for (unsigned char b : record) {
        digest ^= b;
        digest *= 1099511628211ull;
      }
      if (trace.is_open())
        trace << record;
      ++rows;
      ++counts[f[1]];
      if (f[1] == "angles" || f[1] == "signed16") {
        auto n = numbers(f[2]);
        require(n.size() == 2 && n[1] < 65536, "exhaustive case range");
        auto &seen = f[1] == "angles" ? angles : signed16;
        require(!seen[n[1]], "duplicate exhaustive input");
        seen[n[1]] = true;
      }
    }
    require(input.eof(), "vector read failure");
    std::map<std::string, std::size_t> expected_counts{
        {"angles", 65536},
        {"c_rem", 2053},
        {"copy", 640},
        {"cos16", 4336},
        {"dbl_add", 11684},
        {"dbl_fmod", 2052},
        {"dbl_from_int", 2048},
        {"dbl_from_single", 2048},
        {"dbl_from_unsigned", 2048},
        {"dbl_mul", 2052},
        {"dbl_to_single", 2055},
        {"defaults", 5},
        {"descriptor", 512},
        {"fadd", 7696},
        {"fdiv", 7696},
        {"feq", 7696},
        {"fle", 7696},
        {"flt", 7696},
        {"fmax", 7696},
        {"fmin", 7696},
        {"fmul", 7696},
        {"fsqrt", 4336},
        {"fsub", 7696},
        {"ftoi", 4336},
        {"index", 70},
        {"itof", 4336},
        {"powf", 7696},
        {"rank", 512},
        {"round_half_away", 4336},
        {"s16", 4336},
        {"s32", 4336},
        {"s8", 4336},
        {"scalbnf", 4096},
        {"seed", 128},
        {"signed16", 65536},
        {"sin16", 4336},
        {"spell", 15},
        {"sphere", 512},
        {"sqrtf", 4336},
        {"world", 128}};
    require(rows == 284050 && counts == expected_counts && errors == 147,
            "incomplete common corpus");
    require(
        std::all_of(angles.begin(), angles.end(), [](bool x) { return x; }) &&
            std::all_of(signed16.begin(), signed16.end(),
                        [](bool x) { return x; }),
        "missing exhaustive input");
    if (trace.is_open()) {
      trace.flush();
      require(bool(trace), "trace write failure");
    }
    std::cout << "combat common PASS: " << rows << " vectors, " << counts.size()
              << " operations, " << errors
              << " expected errors; fnv1a64=" << std::hex << std::setw(16)
              << std::setfill('0') << digest << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "combat common FAIL: " << e.what() << '\n';
    return 1;
  }
}
