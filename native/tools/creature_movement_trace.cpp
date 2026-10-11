// Authored, asset-free creature movement replay. The native runtime computes
// every value; immutable Python-model fixtures supply independent expectations.
#include "champions/creature_movement.hpp"
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
namespace M = R::creature_movement;
using V = M::Value;
using Vec = M::Vec3;
template <class T> std::string n(T value) { return std::to_string(value); }
std::string value(const V &v) {
  switch (v.kind) {
  case M::ValueKind::None:
    return "_";
  case M::ValueKind::Integer:
    return n(v.scalar);
  case M::ValueKind::Vector:
    return sequence({text("tuple"), integers(v.vec)});
  case M::ValueKind::Buffer:
    return sequence({text("tuple"), sequence({text("buf"), bytes(v.bytes)})});
  case M::ValueKind::List:
  case M::ValueKind::Tuple: {
    std::vector<std::string> out;
    for (const auto &item : v.items)
      out.push_back(value(item));
    return sequence(
        {text(v.kind == M::ValueKind::List ? "list" : "tuple"), sequence(out)});
  }
  }
  throw std::runtime_error("unknown callback value");
}
void value_anchors() {
  const auto tuple = V::tuple({V::integer(1), V::integer(2), V::integer(3)});
  const auto list = V::list(tuple.items);
  if (value(tuple) != value(V::vector({1, 2, 3})) ||
      value(tuple) == value(list) ||
      value(V::list({})) == value(V::tuple({})) ||
      value(V::buffer(std::string("\0\xff", 2))) !=
          sequence({text("tuple"), sequence({text("buf"), "b00ff"})}))
    throw std::runtime_error("callback shape observation anchor failed");
}
std::string values(const std::vector<V> &vs) {
  std::vector<std::string> out;
  for (const auto &v : vs)
    out.push_back(value(v));
  return sequence(out);
}
std::string write(const M::Write &w) {
  return sequence({n(w.address), n(w.size), n(w.value)});
}
std::string call(const M::Call &c) {
  return sequence({text(c.name), values(c.args)});
}
const std::array<std::string, 46> names{
    "basic",           "hidden",
    "root_motion",     "root_timer",
    "slide",           "slide_no_hit",
    "slide_mutate",    "slide_throw",
    "grid_throw",      "contacts_order",
    "contacts_filter", "contacts_coincident",
    "contacts_nopush", "standing",
    "standing_skip",   "standing_passthrough",
    "standing_ignore", "touch_mutate",
    "touch_throw",     "vtable_adjust",
    "base_v6",         "settle",
    "settle_target",   "settle_throw",
    "water_enter",     "water_skip",
    "water_stay",      "water_exit",
    "water_disable",   "water_mutate",
    "water_throw",     "ground_mutate",
    "ground_throw",    "rising_snap",
    "repeat",          "raw_bits",
    "callback_store",  "virtual_throw",
    "radius_reload",   "missing_handlers",
    "water_false",     "water_cadence",
    "buffer_token",    "base_empty",
    "external_base",   "replace_handler"};
constexpr std::uint32_t C = 0x10000, A = 0x20000, B = 0x21000, T = 0x30000,
                        TA = 0x31000, TB = 0x32000, NODE = 0x40000;
constexpr std::uint32_t FRAME = 0x44fdcc, WORLD = 0x44fdc8, SHARED = 0x404464;
struct CallbackFailure : std::runtime_error {
  CallbackFailure() : runtime_error("authored callback failure") {}
};
std::pair<std::string, std::string> scenario(const std::string &name,
                                             std::int64_t config,
                                             const std::string &arguments) {
  auto args = split(arguments, ';');
  if (args.size() != 2 || config < 0 || config > 15)
    throw std::runtime_error("invalid configuration/arguments");
  const auto id = integer(args[0]), variant = integer(args[1]);
  if (id < 0 || id >= 1472 || variant < 0 || variant > 1 || id / 92 != config ||
      (id / 46) % 2 != variant || names[std::size_t(id % 46)] != name)
    throw std::runtime_error("invalid scenario identity");
  R::set_fpu(static_cast<R::Fpu::Model>(config / 4), bool(config & 2),
             bool(config & 1));
  M::Model model(name == "missing_handlers"
                     ? M::CallbackPolicy::ReferenceZero
                     : M::CallbackPolicy::RequireHandlers);
  auto &mem = model.mem;
  auto p = [&](std::uint32_t a, std::size_t z, std::uint64_t v) {
    mem.poke(a, z, v);
  };
  auto vi = [](int x, int y, int z) -> Vec {
    return {R::from_int(std::uint32_t(x)), R::from_int(std::uint32_t(y)),
            R::from_int(std::uint32_t(z))};
  };
  auto pv = [&](std::uint32_t a, Vec v) {
    for (std::size_t i = 0; i < 3; ++i)
      p(a + std::uint32_t(4 * i), 4, v[i]);
  };
  const std::array<std::array<std::int64_t, 3>, 3> objects{
      {{{C, T, 0}}, {{A, TA, 3}}, {{B, TB, -3}}}};
  for (const auto &o : objects) {
    auto address = std::uint32_t(o[0]), table = std::uint32_t(o[1]);
    p(address + 0xc, 4, 0x20);
    pv(address + 0x10, vi(int(o[2]), 0, 0));
    p(address + 0x1c, 4, R::from_int(4));
    p(address + 0x20, 4, R::from_int(8));
    p(address + 0x30, 4, 0);
    p(address + 0x44, 4, variant ? 0xffffffff : 1);
    p(address + 0x48, 4, table);
    p(address + 0xa8, 8, 0x100000);
    for (auto slot : {6u, 10u, 11u}) {
      p(table + 8 * slot, 2, 0);
      p(table + 8 * slot + 4, 4, 0x900000 + slot);
    }
  }
  p(C + 0x38, 2, variant ? 0x7fff : 17);
  p(C + 0xb8, 4, 0);
  pv(C + 0xbc, {0, 0, 0});
  p(C + 0x230, 1, variant);
  p(C + 0x231, 1, 0);
  p(C + 0x234, 4, 0);
  p(C + 0x314, 2, 0);
  p(WORLD, 4, 0x50000);
  p(FRAME, 4, variant);
  p(SHARED, 4, 0x12345678);
  Vec velocity{}, normal{0x3f800000, 0, 0};
  std::vector<std::uint32_t> found;
  R::Raw ground = 0;
  int slide_hit = 1;
  auto is = [&](std::initializer_list<const char *> ns) {
    return std::any_of(ns.begin(), ns.end(), [&](auto s) { return name == s; });
  };
  auto starts = [&](const std::string &s) {
    return name.compare(0, s.size(), s) == 0;
  };
  if (name == "hidden")
    p(C + 0x231, 1, 8 | variant);
  if (is({"root_motion", "root_timer"})) {
    pv(C + 0xbc, {0x3f000001, 0xbf800000, variant ? 0xffffu : 0x4000u});
    velocity = vi(1, 2, 3);
    if (name == "root_timer")
      p(C + 0x314, 2, variant ? 0xffff : 1);
  }
  if (is({"slide", "slide_no_hit", "slide_mutate", "slide_throw", "grid_throw",
          "callback_store", "virtual_throw", "base_empty"}))
    velocity = vi(60, -30, 7);
  if (name == "slide_no_hit")
    slide_hit = 0;
  if (is({"contacts_order", "contacts_filter", "contacts_coincident",
          "contacts_nopush", "standing", "standing_skip",
          "standing_passthrough", "standing_ignore", "touch_mutate",
          "touch_throw", "vtable_adjust", "base_v6", "base_empty",
          "external_base", "replace_handler", "settle", "settle_target",
          "settle_throw", "radius_reload"}))
    found = {A, B};
  if (name == "contacts_order")
    found = variant ? std::vector<std::uint32_t>{B, A, B}
                    : std::vector<std::uint32_t>{A, B, A};
  if (name == "contacts_filter") {
    found = {C, A, B};
    p(A + 0xc, 4, 1);
    p(B + 0x10, 4, R::from_int(100));
  }
  if (name == "contacts_coincident") {
    pv(A + 0x10, vi(0, 0, 0));
    found = {A};
  }
  if (name == "contacts_nopush")
    p(C + 0xc, 4, variant ? 0x401 : 0x400);
  if (starts("standing")) {
    p(A + 0xc, 4, 0x10);
    p(B + 0xc, 4, 0x10);
    p(A + 0x20, 4, R::from_int(3));
    p(B + 0x20, 4, R::from_int(4));
    if (name == "standing_skip") {
      p(A + 0x18, 4, R::from_int(20));
      p(B + 0x18, 4, R::from_int(std::uint32_t(-20)));
    }
    if (name == "standing_passthrough") {
      p(A + 0xc, 4, 0x210);
      p(B + 0xc, 4, 0x210);
    }
    if (name == "standing_ignore")
      p(C + 0xc, 4, 0x400);
  }
  if (name == "vtable_adjust") {
    p(TA + 48, 2, variant ? 0xfff0 : 16);
    p(T + 88, 2, variant ? 0xffe0 : 32);
  }
  if (is({"base_v6", "external_base"})) {
    p(C + 0xc, 4, variant ? 0x40 : 0x20);
    p(TA + 52, 4, 0x11a418);
    p(TB + 52, 4, 0x11a418);
    p(TA + 48, 2, variant ? 16 : 0);
  }
  if (name == "external_base")
    model.external.insert("Creature.v6");
  if (name == "base_empty") {
    p(T + 84, 4, 0x11a3f0);
    p(T + 92, 4, 0x11a448);
  }
  if (name == "settle_target") {
    p(C + 0x234, 4, NODE);
    p(NODE + 0xc, 4, variant ? A : B);
  }
  if (starts("water_") || name == "buffer_token") {
    p(C + 0xa8, 8, 0);
    p(FRAME, 4, variant);
    p(C + 0x230, 1, variant);
    if (name == "water_skip")
      p(FRAME, 4, 3 + variant);
    if (is({"water_stay", "water_exit", "water_cadence"})) {
      p(C + 0xa8, 8, 0x2000000000000000ULL);
      p(C + 0xb8, 4, variant ? 0 : 0x3f800000);
    }
    if (name == "water_cadence") {
      p(C + 0xb8, 4, 0);
      p(FRAME, 4, variant ? 16 : 1);
    }
    if (name == "water_disable")
      p(C + 0xa8, 8, 0x2000000000100000ULL);
  }
  if (name == "rising_snap") {
    p(C + 0x30, 4, R::from_int(10));
    ground = R::from_int(2);
  }
  if (name == "raw_bits") {
    velocity = {variant ? 0x7fffffffu : 1u, 0x80000000,
                variant ? 0xffffffffu : 0x00800000u};
    p(C + 0x30, 4, variant ? 0xff7fffff : 0x80000000);
    ground = 0xc7c35000;
  }
  std::vector<std::string> pokes, callback_results;
  auto poke = [&](std::uint32_t a, std::size_t z, std::uint64_t v) {
    if (z < 8)
      v &= (std::uint64_t(1) << (z * 8)) - 1;
    pokes.push_back(sequence({n(model.calls.size()), n(a), n(z), n(v)}));
    mem.poke(a, z, v);
  };
  auto pokevec = [&](std::uint32_t a, Vec v) {
    for (std::size_t i = 0; i < 3; ++i)
      poke(a + std::uint32_t(4 * i), 4, v[i]);
  };
  auto bind = [&](const std::string &label, M::Model::Callback callback) {
    model.callbacks[label] = [&, callback](M::Model &m,
                                           const std::vector<V> &av) {
      auto result = callback(m, av);
      callback_results.push_back(sequence({n(m.calls.size()), value(result)}));
      return result;
    };
  };
  int touches = 0;
  bind("world_slideMove", [&](auto &, const std::vector<V> &av) {
    const auto address = std::uint32_t(av.at(0).scalar);
    const auto delta = av.at(1).vec;
    Vec result{};
    for (std::size_t i = 0; i < 3; ++i)
      result[i] = R::add(R::Raw(mem.load(address + 4 * i, 4)), delta[i]);
    pokevec(address, result);
    if (name == "slide_mutate") {
      poke(C + 0x1c, 4, R::from_int(std::uint32_t(2 + variant)));
      poke(C + 0x30, 4, R::from_int(15));
    }
    if (name == "callback_store")
      mem.store(C + 0x30, 4, R::from_int(std::uint32_t(12 + variant)));
    if (name == "slide_throw")
      throw CallbackFailure();
    return V::tuple(
        {V::integer(slide_hit),
         name == "slide_no_hit" && variant == 1 ? V{} : V::vector(normal)});
  });
  bind("obj_updateGridCell", [&](auto &, const auto &) {
    if (name == "grid_throw") {
      poke(C + 0x38, 2, 0x1234);
      throw CallbackFailure();
    }
    return V{};
  });
  bind("world_collectObjectsInBox", [&](auto &, const auto &) {
    std::vector<V> v;
    for (auto a : found)
      v.push_back(V::integer(a));
    return V::list(v);
  });
  bind("Creature.v6", [&](auto &, const auto &) {
    ++touches;
    if (name == "replace_handler")
      bind("Creature.v6", [&](auto &, const auto &) {
        poke(C + 0x38, 2, 0x9000 + variant);
        return V{};
      });
    if (is({"touch_mutate", "radius_reload"})) {
      poke(C + 0x1c, 4, R::from_int(touches == 1 ? 1 : 6));
      if (name == "touch_mutate") {
        poke(B + 0xc, 4, 1);
        poke(C + 0x30, 4, R::from_int(20));
      }
    }
    if (is({"touch_throw", "settle_throw"})) {
      poke(C + 0x18, 4, R::from_int(9));
      throw CallbackFailure();
    }
    return V{};
  });
  bind("terrain_heightBelow", [&](auto &, const auto &) {
    if (is({"ground_mutate", "ground_throw"})) {
      poke(C + 0x18, 4, R::from_int(std::uint32_t(5 + variant)));
      poke(C + 0x30, 4, R::from_int(10));
    }
    if (name == "ground_throw")
      throw CallbackFailure();
    return V::integer(ground);
  });
  bind("water_test", [&](auto &, const auto &) {
    if (name == "water_mutate")
      poke(C + 0xa8, 8, 0x8000000000000001ULL);
    return V::integer(name == "water_false" ? 0 : 1);
  });
  bind("water_disturb", [&](auto &, const auto &) {
    if (name == "water_throw") {
      poke(C + 0xb8, 4, 0x3f800000);
      throw CallbackFailure();
    }
    return V::integer(name == "water_exit" ? 0 : 1);
  });
  auto virt = [&](M::Model &, const std::vector<V> &) {
    if (name == "virtual_throw") {
      poke(C + 0x38, 2, 0x55aa);
      throw CallbackFailure();
    }
    return V{};
  };
  bind("Creature.v10", virt);
  bind("Creature.v11", virt);
  bind("libc_sprintf", [&](auto &, const auto &) {
    return name == "buffer_token"
               ? V::buffer(std::string("splash\0\xff", 8) + char(variant))
               : V::integer(0x60000);
  });
  bind("res_find", [](auto &, const auto &) { return V::integer(0x60010); });
  bind("sound_playOnObject", [](auto &, const auto &) { return V{}; });
  if (name == "missing_handlers")
    model.callbacks.clear();
  std::vector<std::string> results;
  for (int i = 0; i < (name == "repeat" ? 3 : 1); ++i) {
    try {
      if (starts("settle")) {
        model.settle(C);
        results.push_back("_");
      } else
        results.push_back(n(model.move_by(C, velocity)));
    } catch (const CallbackFailure &) {
      results.push_back(text("CallbackFailure"));
    }
  }
  std::vector<std::string> memory, writes, stores, calls, events;
  for (auto [a, v] : mem.bytes())
    memory.push_back(sequence({n(a), n(v)}));
  for (const auto &w : model.writes)
    writes.push_back(write(w));
  for (const auto &w : mem.log())
    stores.push_back(write(w));
  for (const auto &c : model.calls)
    calls.push_back(call(c));
  for (const auto &e : model.events) {
    if (e.kind == M::EventKind::Write)
      events.push_back(sequence({text("write"), n(e.write.address),
                                 n(e.write.size), n(e.write.value)}));
    else if (e.kind == M::EventKind::Call)
      events.push_back(
          sequence({text("call"), text(e.call.name), values(e.call.args)}));
    else
      throw std::runtime_error("unknown event kind");
  }
  return {"ok", sequence({sequence(results), sequence(memory), sequence(writes),
                          sequence(stores), sequence(calls), sequence(pokes),
                          n(model.draws), sequence(events),
                          sequence(callback_results)})};
}
// Pinned independently generated corpus coverage and full computed trace
// digest.
constexpr std::uint64_t expected_digest = 0xd72ad8eb3b7c2e63ULL;
constexpr std::size_t expected_rows = 1472;
int run(const std::vector<std::filesystem::path> &args) {
  if (args.size() < 1 || args.size() > 2)
    throw std::runtime_error(
        "usage: creature-movement-trace VECTORS.tsv [TRACE.tsv]");
  constexpr std::uintmax_t max_fixture = 32ULL * 1024 * 1024;
  constexpr std::size_t max_line = 64 * 1024;
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
  if (line() != "# creature_movement_vectors_v1")
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
      std::cerr << "creature movement mismatch at row " << rows + 1 << " ("
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
    throw std::runtime_error("incomplete or changed creature movement corpus");
  if (args.size() == 2) {
    output.flush();
    if (!output)
      throw std::runtime_error("trace write failed");
  }
  std::cout << rows
            << " exact creature movement vectors passed; FNV1a64=" << std::hex
            << digest << '\n';
  return 0;
}
int entry(const std::vector<std::filesystem::path> &args) {
  try {
    value_anchors();
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
