// Authored, asset-free creature HP replay. The native runtime computes every
// value; immutable Python-model fixtures supply independent expectations.
#include "champions/creature_hp.hpp"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {
std::vector<std::string> split(const std::string &s, char separator) {
  std::vector<std::string> result;
  std::size_t start = 0;
  for (;;) {
    const auto end = s.find(separator, start);
    result.push_back(s.substr(start, end == s.npos ? end : end - start));
    if (end == s.npos)
      return result;
    start = end + 1;
  }
}
std::int64_t integer(const std::string &s) {
  if (s.empty() || s == "-0" || s.front() == '+' ||
      (s.size() > 1 && s.front() == '0') ||
      (s.size() > 2 && s[0] == '-' && s[1] == '0'))
    throw std::runtime_error("noncanonical integer");
  for (std::size_t i = s.front() == '-' ? 1 : 0; i < s.size(); ++i)
    if (s[i] < '0' || s[i] > '9')
      throw std::runtime_error("invalid integer");
  std::size_t used = 0;
  const auto value = std::stoll(s, &used);
  if (used != s.size())
    throw std::runtime_error("invalid integer");
  return value;
}
std::string sequence(const std::vector<std::string> &values) {
  std::string out = "[";
  for (const auto &value : values) {
    if (out.size() > 1)
      out += ',';
    out += value;
  }
  return out + ']';
}
template <class C> std::string bytes(const C &values) {
  constexpr char hex[] = "0123456789abcdef";
  std::string out = "b";
  for (const auto value : values) {
    const auto ch = static_cast<unsigned char>(value);
    out += hex[ch >> 4];
    out += hex[ch & 15];
  }
  return out;
}
std::string text(const std::string &value) {
  auto out = bytes(value);
  out.front() = 's';
  return out;
}
template <class C> std::string integers(const C &values) {
  std::vector<std::string> out;
  for (const auto value : values)
    out.push_back(std::to_string(value));
  return sequence(out);
}

namespace R = champions::simulation::rta;
namespace H = R::creature_hp;
std::string n(std::uint64_t value) { return std::to_string(value); }
const std::array<std::string, 32> names{"basic",
                                        "inert",
                                        "ignore",
                                        "hp_zero",
                                        "hp_negative",
                                        "amount_negative",
                                        "amount_zero",
                                        "subnormal",
                                        "large_finite",
                                        "nan_like",
                                        "signed_zero",
                                        "angle_quadrants",
                                        "attacker_null",
                                        "self",
                                        "projectile",
                                        "projectile_null_node",
                                        "projectile_null_target",
                                        "effect",
                                        "effect_null_node",
                                        "effect_null_target",
                                        "precedence",
                                        "stale_target",
                                        "clear_mutate_hp",
                                        "clear_mutate_route",
                                        "clear_throw",
                                        "link_mutate_hp",
                                        "link_throw",
                                        "missing_handlers",
                                        "alias_flags",
                                        "callback_store",
                                        "repeat_hits",
                                        "random_bits"};
constexpr std::uint32_t C = 0x10000, A = 0x20000, OWNER = 0x30000,
                        EFFECT = 0x30100, LINK = 0x30200;
std::string write(const H::Write &w) {
  return sequence({n(w.address), n(w.size), n(w.value)});
}
std::string call(const H::Call &c) {
  return sequence({text(c.name), integers(c.args)});
}
struct CallbackFailure : std::runtime_error {
  using std::runtime_error::runtime_error;
};
std::pair<std::string, std::string> scenario(const std::string &name,
                                             std::int64_t config,
                                             const std::string &arguments) {
  const auto args = split(arguments, ';');
  if (args.size() != 2 || config < 0 || config > 15)
    throw std::runtime_error("invalid scenario/configuration");
  const auto id = integer(args[0]), variant = integer(args[1]);
  if (id < 0 || id >= 2048 || variant < 0 || variant > 3 ||
      id / 128 != config || (id / 32) % 4 != variant ||
      names[std::size_t(id % 32)] != name)
    throw std::runtime_error("inconsistent scenario identity");
  const auto v = std::size_t(variant);
  const R::Fpu selected(static_cast<R::Fpu::Model>(config / 4), config & 2,
                        config & 1);
  R::ScopedFpu context(selected);
  H::Model model(name == "missing_handlers"
                     ? H::CallbackPolicy::ReferenceZero
                     : H::CallbackPolicy::RequireHandlers);
  auto p = [&](H::Address address, std::size_t width, std::uint64_t value) {
    model.mem.poke(address, width, value);
  };
  auto raw = [](std::int64_t value) {
    return R::from_int(std::uint32_t(value));
  };
  p(C + 0xc, 4, 0x20);
  p(C + 0x10, 4, raw(1 + variant));
  p(C + 0x14, 4, raw(2 - variant));
  p(C + 0xa8, 8, 0x80000000ULL | v);
  p(C + 0xc8, 4, 0x3f000000);
  p(C + 0x227, 1, 0xa5);
  p(C + 0x228, 4, 0x41200000);
  p(C + 0x22c, 4, 0x41a00000);
  p(C + 0x230, 1, 0x5a);
  p(C + 0x231, 1, 0);
  p(C + 0x232, 2, 0x1234);
  p(C + 0x2dc, 4, 0x40000 | v);
  p(C + 0x2e0, 2, 0x5566);
  p(C + 0x2e2, 2, 0xcafe);
  p(C + 0x2e4, 4, LINK);
  p(C + 0x304, 4, 0x76543210);
  const std::array<unsigned, 5> timer_offsets{0x340, 0x344, 0x348, 0x34c,
                                              0x350};
  const std::array<std::uint32_t, 5> timer_values{0x41200000, 0x3f800000,
                                                  0xbf800000, 0, 0x7fc00000};
  for (unsigned i = 0; i < 5; ++i)
    p(C + timer_offsets[i], 4, timer_values[i]);
  p(C + 0x360, 4, EFFECT);
  p(C + 0x3af, 1, 0x69);
  p(C + 0x3b0, 2, 0x1234 + v);
  p(C + 0x3b2, 1, 0x96);
  p(A + 0xc, 4, 0x40);
  p(A + 0x10, 4, raw(-3 + variant));
  p(A + 0x14, 4, raw(7 - 2 * variant));
  p(A + 0x4c, 4, OWNER);
  p(OWNER + 0xc, 4, 0x40000 + v * 0x100);
  p(EFFECT + 0xc, 4, 0x50000 + v * 0x100);
  p(LINK + 0xc, 4, 0x60000 + v * 0x100);
  p(H::G_SIM_FRAME, 4,
    std::array<std::uint32_t, 4>{0, 1, 0x7fffffff, 0xffffffff}[v]);
  std::uint32_t attacker = A, flags = 0x80000 | (1u << v), amount = 0x40400000;
  const auto pick = [&](std::array<std::uint32_t, 4> values) {
    return values[v];
  };
  if (name == "inert")
    p(C + 0x231, 1, pick({1, 0x7f, 0x80, 0xff}));
  else if (name == "ignore")
    p(C + 0xa8, 8, UINT64_C(0x1000000000000) | (std::uint64_t(v) << 60));
  else if (name == "hp_zero")
    p(C + 0x228, 4, pick({0, 0x80000000, 0, 0x80000000}));
  else if (name == "hp_negative")
    p(C + 0x228, 4, pick({0xbf800000, 0xc1200000, 0xff7fffff, 0x80000001}));
  else if (name == "amount_negative")
    amount = pick({0xbf800000, 0xc1200000, 0xff7fffff, 0x80000001});
  else if (name == "amount_zero")
    amount = pick({0, 0x80000000, 0, 0x80000000});
  else if (name == "subnormal") {
    p(C + 0x228, 4, pick({1, 0x80000001, 0x007fffff, 0x807fffff}));
    p(C + 0xc8, 4, pick({0x007fffff, 1, 0x807fffff, 0x80000001}));
    amount = pick({0x80000001, 1, 0x007fffff, 0x807fffff});
  } else if (name == "large_finite") {
    p(C + 0x228, 4, pick({0x7f7fffff, 0x7f000000, 0xff7fffff, 0x00800000}));
    p(C + 0xc8, 4, pick({0x7f7fffff, 0xff7fffff, 0x00800000, 0x80800000}));
    amount = pick({0xff7fffff, 0x7f7fffff, 0x7f7fffff, 1});
  } else if (name == "nan_like") {
    p(C + 0x228, 4, pick({0x7fc00000, 0xffc00000, 0x7f800000, 0xff800000}));
    p(C + 0xc8, 4, 0x7fffffff);
    amount = pick({0x7f800000, 0x7fffffff, 0xffffffff, 0xffc00000});
  } else if (name == "signed_zero") {
    p(C + 0x228, 4, pick({0, 0x80000000, 0, 0x80000000}));
    p(C + 0xc8, 4, pick({0x80000000, 0, 0, 0x80000000}));
    amount = pick({0, 0, 0x80000000, 0x80000000});
  } else if (name == "angle_quadrants") {
    p(C + 0x10, 4, 0);
    p(C + 0x14, 4, 0);
    constexpr std::array<int, 4> x{1, -1, -1, 1}, y{1, 1, -1, -1};
    p(A + 0x10, 4, raw(x[v]));
    p(A + 0x14, 4, raw(y[v]));
  } else if (name == "attacker_null") {
    attacker = 0;
    flags |= 0x10000000;
  } else if (name == "self")
    attacker = C;
  else if (name.compare(0, 10, "projectile") == 0) {
    p(A + 0xc, 4, 0xc0);
    if (name == "projectile_null_node")
      p(A + 0x4c, 4, 0);
    else if (name == "projectile_null_target")
      p(OWNER + 0xc, 4, 0);
  } else if (name.compare(0, 6, "effect") == 0) {
    flags |= 0x10000000;
    if (name == "effect_null_node")
      p(C + 0x360, 4, 0);
    else if (name == "effect_null_target")
      p(EFFECT + 0xc, 4, 0);
  } else if (name == "precedence") {
    flags |= 0x10000000;
    p(A + 0xc, 4, 0xc0);
    if (v == 1)
      p(A + 0x4c, 4, 0);
    else if (v == 2)
      p(OWNER + 0xc, 4, 0);
    else if (v == 3)
      p(OWNER + 0xc, 4, 0xffffffff);
  } else if (name == "stale_target") {
    flags |= 0x10000000;
    p(EFFECT + 0xc, 4, pick({0xdeadbeef, 0xffffffff, C + 0x229, 1}));
  } else if (name == "clear_mutate_route")
    flags |= 0x10000000;
  else if (name == "alias_flags") {
    attacker = C + 0x2d8;
    p(C + 0x2e4, 4, LINK | 0x80);
    p(attacker + 0x10, 4, raw(variant + 1));
    p(attacker + 0x14, 4, raw(-variant - 1));
    p(attacker + 0x4c, 4, OWNER);
  } else if (name == "random_bits") {
    auto seed = pick({0, 1, 0xdeadbeef, 0xffffffff});
    std::array<std::uint32_t, 8> values{};
    for (auto &value : values) {
      seed = seed * 1664525u + 1013904223u;
      value = seed;
    }
    constexpr std::array<unsigned, 6> addresses{C + 0x228, C + 0xc8, C + 0x10,
                                                C + 0x14,  A + 0x10, A + 0x14};
    for (unsigned i = 0; i < 6; ++i)
      p(addresses[i], 4, values[i]);
    amount = values[6];
    flags = values[7];
  }
  std::vector<std::string> pokes, results;
  auto callback_poke = [&](H::Address address, std::size_t width,
                           std::uint64_t value) {
    if (width < 8)
      value &= (UINT64_C(1) << (8 * width)) - 1;
    pokes.push_back(
        sequence({n(model.calls.size()), n(address), n(width), n(value)}));
    p(address, width, value);
  };
  auto change_hp = [&]() {
    callback_poke(C + 0x228, 4,
                  pick({0xc1200000, 0x3f800000, 0x7fc00000, 0x80000000}));
    callback_poke(C + 0xc8, 4, pick({0x40000000, 0xbf800000, 0xff7fffff, 1}));
  };
  model.callbacks["handle_clear"] =
      [&](H::Model &, const std::vector<std::uint32_t> &values) {
        if (values.size() != 1)
          throw std::runtime_error("clear callback arity");
        const auto head = values[0];
        callback_poke(head, 4, 0);
        if (name == "clear_mutate_hp" || name == "clear_throw")
          change_hp();
        if (name == "clear_mutate_route") {
          callback_poke(A + 0xc, 4, pick({0x80, 0, 0, 0x80}));
          callback_poke(A + 0x4c, 4, v == 3 ? 0 : OWNER);
          callback_poke(OWNER + 0xc, 4, 0xfeed0000 + v);
          if (v == 2) {
            callback_poke(C + 0x231, 1, 0xff);
            callback_poke(C + 0xa8, 8, UINT64_C(0x1000000000000));
          }
        }
        if (name == "callback_store")
          model.mem.store(C + 0x228, 4, 0x3f800000 + v);
        if (name == "clear_throw")
          throw CallbackFailure("authored clear failure");
      };
  model.callbacks["handle_link"] =
      [&](H::Model &, const std::vector<std::uint32_t> &values) {
        if (values.size() != 3)
          throw std::runtime_error("link callback arity");
        const auto head = values[0], obj = values[2];
        callback_poke(head, 4, LINK);
        callback_poke(LINK + 0xc, 4, obj);
        if (name == "link_mutate_hp" || name == "link_throw")
          change_hp();
        if (name == "link_mutate_hp" && v == 3)
          callback_poke(head, 4, 0x12345678);
        if (name == "callback_store")
          model.mem.store(C + 0xc8, 4, 0xbf800000 + v);
        if (name == "link_throw")
          throw CallbackFailure("authored link failure");
      };
  if (name == "missing_handlers") {
    if (v == 0 || v == 3)
      model.callbacks.clear();
    else
      model.callbacks.erase(v == 1 ? "handle_clear" : "handle_link");
  }
  const auto amounts = name == "repeat_hits"
                           ? std::vector<std::uint32_t>{0x3f800000, 0x41200000,
                                                        0xc0a00000, 0x80000000}
                           : std::vector<std::uint32_t>{amount};
  for (std::size_t i = 0; i < amounts.size(); ++i) {
    try {
      model.take_damage(C, attacker, flags | (std::uint32_t(i) << 8),
                        amounts[i]);
      results.push_back("_");
    } catch (const CallbackFailure &) {
      results.push_back(text("CallbackFailure"));
    }
  }
  std::vector<std::string> memory, writes, stores, calls, events;
  for (const auto &entry : model.mem.bytes())
    memory.push_back(sequence({n(entry.first), n(entry.second)}));
  for (const auto &entry : model.writes)
    writes.push_back(write(entry));
  for (const auto &entry : model.mem.log())
    stores.push_back(write(entry));
  for (const auto &entry : model.calls)
    calls.push_back(call(entry));
  for (const auto &event : model.events) {
    if (event.kind == H::EventKind::Write)
      events.push_back(sequence({text("write"), n(event.write.address),
                                 n(event.write.size), n(event.write.value)}));
    else if (event.kind == H::EventKind::Call)
      events.push_back(sequence(
          {text("call"), text(event.call.name), integers(event.call.args)}));
    else
      throw std::runtime_error("unknown runtime event kind");
  }
  return {"ok", sequence({sequence(results), sequence(memory), sequence(writes),
                          sequence(stores), sequence(calls), sequence(pokes),
                          n(model.draws), sequence(events)})};
}
// Pinned independently generated corpus coverage and full computed trace
// digest.
constexpr std::uint64_t expected_digest = 0x56d402c64164cc7dULL;
constexpr std::size_t expected_rows = 2048;
int run(const std::vector<std::filesystem::path> &args) {
  if (args.size() < 1 || args.size() > 2)
    throw std::runtime_error(
        "usage: creature-hp-trace VECTORS.tsv [TRACE.tsv]");
  constexpr std::uintmax_t max_fixture = 1024ULL * 1024 * 1024;
  constexpr std::size_t max_line = 16 * 1024 * 1024;
  if (!std::filesystem::is_regular_file(args[0]) ||
      std::filesystem::file_size(args[0]) > max_fixture)
    throw std::runtime_error(
        "fixture is missing, not regular, or exceeds budget");
  if (args.size() == 2) {
    if (std::filesystem::weakly_canonical(args[0]) ==
            std::filesystem::weakly_canonical(args[1]) ||
        (std::filesystem::exists(args[1]) &&
         std::filesystem::equivalent(args[0], args[1])))
      throw std::runtime_error("trace must not overwrite fixture");
  }
  std::ifstream input(args[0], std::ios::binary);
  if (!input)
    throw std::runtime_error("cannot open fixture");
  std::ofstream output;
  if (args.size() == 2) {
    output.open(args[1], std::ios::binary);
    if (!output)
      throw std::runtime_error("cannot open trace");
  }
  auto line = [&]() {
    std::string value;
    for (char ch; input.get(ch);) {
      if (ch == '\n')
        return value;
      if (value.size() == max_line)
        throw std::runtime_error("fixture line exceeds budget");
      if (static_cast<unsigned char>(ch) > 127 || ch == '\r' || ch == '\0')
        throw std::runtime_error("fixture must be ASCII with LF separators");
      value += ch;
    }
    if (!value.empty())
      throw std::runtime_error("fixture ends without newline");
    return value;
  };
  if (line() != "# creature_hp_vectors_v1")
    throw std::runtime_error("fixture schema mismatch");
  std::set<std::string> seen;
  std::size_t rows = 0;
  std::uint64_t digest = 14695981039346656037ULL;
  while (input.peek() != std::char_traits<char>::eof()) {
    const auto columns = split(line(), '\t');
    if (columns.size() != 5)
      throw std::runtime_error("fixture requires five columns");
    const auto key = columns[0] + '\t' + columns[1] + '\t' + columns[2];
    if (!seen.insert(key).second)
      throw std::runtime_error("duplicate scenario");
    const auto actual = scenario(columns[0], integer(columns[1]), columns[2]);
    if (actual.first != columns[3] || actual.second != columns[4]) {
      std::cerr << "creature HP mismatch at row " << rows + 1 << " ("
                << columns[0] << ")\n";
      return 1;
    }
    const auto record = key + '\t' + actual.first + '\t' + actual.second + '\n';
    if (args.size() == 2) {
      output << record;
      if (!output)
        throw std::runtime_error("trace write failed");
    }
    for (unsigned char ch : record) {
      digest ^= ch;
      digest *= 1099511628211ULL;
    }
    if (++rows > 65536)
      throw std::runtime_error("fixture row budget exceeded");
  }
  if (input.bad())
    throw std::runtime_error("fixture read failed");
  if (rows != expected_rows || digest != expected_digest)
    throw std::runtime_error("incomplete or changed creature HP corpus");
  if (args.size() == 2) {
    output.flush();
    if (!output)
      throw std::runtime_error("trace write failed");
  }
  std::cout << rows << " exact creature HP vectors passed; FNV1a64=" << std::hex
            << digest << '\n';
  return 0;
}
int entry(const std::vector<std::filesystem::path> &args) {
  try {
    return run(args);
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
} // namespace
#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
#else
int main(int argc, char **argv) {
#endif
  std::vector<std::filesystem::path> args;
  for (int i = 1; i < argc; ++i)
    args.emplace_back(argv[i]);
  return entry(args);
}
