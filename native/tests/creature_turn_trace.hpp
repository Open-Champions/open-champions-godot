#pragma once
// Authored bounded integer/list transport. The explicit input describes state
// and commands; no corpus-specific recipe or expected value drives execution.
#include "champions/creature_movement.hpp"
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace champions::test::creature_turn_trace {
namespace M = simulation::rta::creature_movement;
namespace R = simulation::rta;
inline std::string sequence(const std::vector<std::string> &v) {
  std::string out = "[";
  for (const auto &x : v) {
    if (out.size() > 1)
      out += ',';
    out += x;
  }
  return out + ']';
}
template <class T> std::string n(T value) { return std::to_string(value); }
struct Value {
  bool list = false, negative = false;
  std::uint64_t magnitude = 0;
  std::vector<Value> items;
  const std::vector<Value> &array(std::size_t count) const {
    if (!list || items.size() != count)
      throw std::runtime_error("invalid DSL shape");
    return items;
  }
  const std::vector<Value> &bounded_array(std::size_t maximum) const {
    if (!list || items.size() > maximum)
      throw std::runtime_error("DSL list budget or shape");
    return items;
  }
  template <class T> T integer() const {
    static_assert(std::is_integral_v<T> && !std::is_same_v<T, bool>);
    if (list)
      throw std::runtime_error("expected DSL integer");
    if constexpr (std::is_signed_v<T>) {
      const auto max =
          static_cast<std::uint64_t>(std::numeric_limits<T>::max());
      if (magnitude > max + (negative ? 1ULL : 0ULL))
        throw std::runtime_error("DSL signed integer range");
      if (negative && magnitude == max + 1ULL)
        return std::numeric_limits<T>::min();
      const auto positive = static_cast<T>(magnitude);
      return negative ? static_cast<T>(-positive) : positive;
    } else {
      if (negative || magnitude > std::numeric_limits<T>::max())
        throw std::runtime_error("DSL unsigned integer range");
      return static_cast<T>(magnitude);
    }
  }
  std::string canonical() const {
    if (!list)
      return (negative ? "-" : "") + n(magnitude);
    std::vector<std::string> out;
    for (const auto &v : items)
      out.push_back(v.canonical());
    return sequence(out);
  }
};
class Parser {
  const std::string &text;
  std::size_t at = 0, nodes = 0;
  Value value(unsigned depth) {
    if (depth > 10 || ++nodes > 8192)
      throw std::runtime_error("DSL depth/node budget");
    if (at == text.size())
      throw std::runtime_error("truncated DSL");
    Value v;
    if (text[at] == '[') {
      v.list = true;
      ++at;
      if (at < text.size() && text[at] == ']') {
        ++at;
        return v;
      }
      for (;;) {
        if (v.items.size() == 2048)
          throw std::runtime_error("DSL list budget");
        v.items.push_back(value(depth + 1));
        if (at == text.size())
          throw std::runtime_error("truncated DSL list");
        if (text[at] == ']') {
          ++at;
          return v;
        }
        if (text[at++] != ',')
          throw std::runtime_error("invalid DSL separator");
      }
    }
    if (text[at] == '-') {
      v.negative = true;
      ++at;
    }
    const auto start = at;
    while (at < text.size() && text[at] >= '0' && text[at] <= '9') {
      const unsigned digit = static_cast<unsigned>(text[at++] - '0');
      if (v.magnitude >
          (std::numeric_limits<std::uint64_t>::max() - digit) / 10)
        throw std::runtime_error("DSL integer overflow");
      v.magnitude = v.magnitude * 10 + digit;
    }
    if (at == start || (at - start > 1 && text[start] == '0') ||
        (v.negative && v.magnitude == 0))
      throw std::runtime_error("noncanonical DSL integer");
    return v;
  }

public:
  explicit Parser(const std::string &source) : text(source) {}
  Value parse() {
    auto result = value(0);
    if (at != text.size())
      throw std::runtime_error("trailing DSL bytes");
    return result;
  }
};
inline std::string hex(const std::string &s) {
  constexpr char digits[] = "0123456789abcdef";
  std::string out;
  for (unsigned char c : s) {
    out += digits[c >> 4];
    out += digits[c & 15];
  }
  return out;
}
inline std::string text(const std::string &s) { return "s" + hex(s); }
template <class T> std::string integers(const T &values) {
  std::vector<std::string> out;
  for (auto v : values)
    out.push_back(n(v));
  return sequence(out);
}
inline std::string typed(const M::Value &v) {
  switch (v.kind) {
  case M::ValueKind::None:
    return "_";
  case M::ValueKind::Integer:
    return n(v.scalar);
  case M::ValueKind::Vector:
    return sequence({text("tuple"), integers(v.vec)});
  case M::ValueKind::Buffer:
    return sequence(
        {text("tuple"), sequence({text("buf"), "b" + hex(v.bytes)})});
  case M::ValueKind::List:
  case M::ValueKind::Tuple: {
    std::vector<std::string> items;
    for (const auto &x : v.items)
      items.push_back(typed(x));
    return sequence({text(v.kind == M::ValueKind::List ? "list" : "tuple"),
                     sequence(items)});
  }
  }
  throw std::runtime_error("unknown callback value kind");
}
inline std::string values(const std::vector<M::Value> &v) {
  std::vector<std::string> out;
  for (const auto &x : v)
    out.push_back(typed(x));
  return sequence(out);
}
inline std::string write(const M::Write &w) {
  return sequence({n(w.address), n(w.size), n(w.value)});
}
inline std::string snapshot(const M::Model &model) {
  std::vector<std::string> memory, stores, writes, calls, events, callbacks,
      external;
  for (const auto &[a, v] : model.mem.bytes())
    memory.push_back(sequence({n(a), n(v)}));
  for (const auto &w : model.mem.log())
    stores.push_back(write(w));
  for (const auto &w : model.writes)
    writes.push_back(write(w));
  for (const auto &c : model.calls)
    calls.push_back(sequence({text(c.name), values(c.args)}));
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
  for (const auto &entry : model.callbacks)
    callbacks.push_back(text(entry.first));
  for (const auto &name : model.external)
    external.push_back(text(name));
  return sequence({sequence(memory), sequence(stores), sequence(writes),
                   sequence(calls), sequence(events), n(model.draws),
                   sequence(callbacks), sequence(external),
                   n(static_cast<unsigned>(model.policy)),
                   n(model.borrows_memory()), n(bool(model.mem.overlay()))});
}
inline void memory_command(M::Memory &memory, const std::vector<Value> &row,
                           std::size_t start, bool store) {
  const auto address = row[start].integer<std::uint32_t>();
  const auto width = row[start + 1].integer<std::size_t>();
  const auto value = row[start + 2].integer<std::uint64_t>();
  if (width > 8 || (width && std::uint64_t(address) + width - 1 > UINT32_MAX))
    throw std::runtime_error("memory width/address outside DSL domain");
  if (store)
    memory.store(address, width, value);
  else
    memory.poke(address, width, value);
}
inline std::string execute(unsigned config, const std::string &source) {
  if (config > 15)
    throw std::runtime_error("invalid arithmetic configuration");
  R::ScopedFpu scope{R::Fpu{static_cast<R::Fpu::Model>(config / 4),
                            (config & 2) != 0, (config & 1) != 0}};
  const auto input = Parser(source).parse();
  const auto &parts = input.array(2);
  M::Memory memory;
  M::Model model(memory, M::CallbackPolicy::RequireHandlers);
  for (const auto &poke : parts[0].bounded_array(512))
    memory_command(memory, poke.array(3), 0, false);
  const auto initial = snapshot(model);
  std::vector<std::string> steps;
  for (const auto &command : parts[1].bounded_array(128)) {
    const auto &row = command.array(4);
    const auto opcode = row[0].integer<unsigned>();
    std::string result = "_";
    if (opcode == 0) {
      const auto address = row[1].integer<std::uint32_t>();
      const auto mode = row[2].integer<std::int32_t>();
      const auto &point = row[3].array(3);
      result = n(model.turn_toward(address, mode,
                                   {point[0].integer<R::Raw>(),
                                    point[1].integer<R::Raw>(),
                                    point[2].integer<R::Raw>()}));
    } else if (opcode == 1 || opcode == 2)
      memory_command(memory, row, 1, opcode == 2);
    else
      throw std::runtime_error("unknown turn opcode");
    steps.push_back(sequence({text("ok"), result, snapshot(model)}));
  }
  return sequence({input.canonical(), initial, sequence(steps)});
}
inline const std::vector<std::string> &operations() {
  static const std::vector<std::string> names{
      "modes",    "directions",    "facing_wrap", "velocity_wrap",
      "rates",    "slow",          "point_bits",  "position_bits",
      "repeated", "interleaved",   "addresses",   "sparse",
      "storage",  "two_addresses", "overlap"};
  return names;
}
} // namespace champions::test::creature_turn_trace
